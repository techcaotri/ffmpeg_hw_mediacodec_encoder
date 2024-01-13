#include "ffmpeg_encoder.h"

#include <string>
#include <fstream>

#ifndef ANDROID
#include "nvenc_utils.h"
#endif

#define USE_MRS_CODE 1

#if USE_RAW
#define FPS 10
#else
#define FPS 30
#endif

#include "my_log.h"

constexpr int kBitrateQualityScale = 200000;
const char *kEncoderTypeNames[] = {"VAAPI", "NVENC", "MEDIACODEC", "LIBX264"};

static void dump_avframe_info(AVFrame* in_frame) {
    if (!in_frame) {
        ILOGD("Invalid AVFrame pointer");
        return;
    }

    // Print general frame info
    ILOGD("Frame width: %d, height: %d", in_frame->width, in_frame->height);
    ILOGD("Pixel format: %s", av_get_pix_fmt_name(static_cast<AVPixelFormat>(in_frame->format)));

    // Get the number of planes
    int num_planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(in_frame->format));
    ILOGD("Number of planes: %d", num_planes);

    // Iterate over each plane
    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        ILOGD("Plane %d data pointer: %p", i, static_cast<void*>(in_frame->data[i]));
        ILOGD("Plane %d linesize: %d", i, in_frame->linesize[i]);
    }
}

FFmpegEncoder::FFmpegEncoder(EncoderType pEncoderType, int pWidth, int pHeight, int pQuality,
                             int pFps)
        : format_context_(nullptr), codec_context_(nullptr), video_stream_(nullptr),
#ifdef SUPPORT_HW_ENCODER
        hw_device_ctx(nullptr),
#endif
          next_pts(0), pts_increment((AV_TIME_BASE + FPS / 2) / FPS), encoder_type_(pEncoderType),
          width(pWidth), height(pHeight), fps(pFps), quality(pQuality) {
    // Constructor initialization
    // av_register_all();
    // avcodec_register_all();
    av_log_set_level(AV_LOG_TRACE);

#ifdef SUPPORT_HW_ENCODER
    InitializeHWContext();
#endif
}

FFmpegEncoder::~FFmpegEncoder() {
    Cleanup();  // Cleanup resources
}

bool FFmpegEncoder::Initialize(const std::string &output_file) {
    return OpenVideoFile(output_file) && SetupEncoder(output_file);
}

#ifdef SUPPORT_HW_ENCODER
bool FFmpegEncoder::InitializeHWContext() {
  const char* device = nullptr;
  AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;

  if (encoder_type_ == EncoderType::VAAPI) {
    device = "/dev/dri/renderD128";  // This may need to be dynamic
    hw_type = AV_HWDEVICE_TYPE_VAAPI;
  } else if (encoder_type_ == EncoderType::NVENC) {
    // device = "cuda";  // Typically for Nvidia, but can be different based on setup
    device = nullptr;
    hw_type = AV_HWDEVICE_TYPE_CUDA;
  } else if (encoder_type_ == EncoderType::MEDIACODEC) {
    hw_type = AV_HWDEVICE_TYPE_MEDIACODEC;
  }

  if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, device, nullptr, 0) < 0) {
    ILOGE("Failed to create hardware context for %s", kEncoderTypeNames[(int)encoder_type_] );
    // Handle error
    return false;
  }

#ifndef ANDROID
  support_multiple_ref_frames_ = DetectMultipleRefFramesCap() > 0;
#endif

  return true;
}
#endif

void FFmpegEncoder::Cleanup() {
    WriteTrailer();

    // Release all allocated resources
    if (format_context_ && !(format_context_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&format_context_->pb);

    avcodec_free_context(&codec_context_);
    avformat_free_context(format_context_);
#ifdef SUPPORT_HW_ENCODER
    av_buffer_unref(&hw_device_ctx);
#endif
}

bool FFmpegEncoder::OpenVideoFile(const std::string &output_file) {
    // Allocate format context
    avformat_alloc_output_context2(&format_context_, nullptr, nullptr, output_file.c_str());
    if (!format_context_) {
        ILOGE("Could not allocate format context");
        return false;
    }

    return true;
}

bool FFmpegEncoder::SetupEncoder(const std::string &output_file) {
    // Find the encoder
    const AVCodec *codec = nullptr;

    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    if (encoder_type_ == EncoderType::VAAPI) {
        codec = avcodec_find_encoder_by_name("h264_vaapi");
        if (!codec) {
            ILOGE("VAAPI encoder not found");
            return false;
        }
        ILOGD("Found video codec for %s\n", "h264_vaapi");
        pix_fmt = AV_PIX_FMT_VAAPI;
    } else if (encoder_type_ == EncoderType::NVENC) {
        codec = avcodec_find_encoder_by_name("h264_nvenc");
        if (!codec) {
            ILOGE("NVENC encoder not found");
            return false;
        }
        pix_fmt = AV_PIX_FMT_CUDA;  // NV12 is typically supported by NVENC

        if (!support_multiple_ref_frames_) codec_context_->refs = 0;
    } else if (encoder_type_ == EncoderType::MEDIACODEC) {
        codec = avcodec_find_encoder_by_name("h264_mediacodec");
        if (!codec) {
            ILOGE("MediaCodec encoder not found");
            return false;
        }
        ILOGD("Found video codec for %s\n", "h264_mediacodec");

#ifdef SUPPORT_HW_ENCODER
        pix_fmt = AV_PIX_FMT_MEDIACODEC; // Use the format compatible with MediaCodec

        AVHWDeviceType type = av_hwdevice_find_type_by_name("mediacodec");
        enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
        for (int i = 0; ; ++i) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config) {
                ILOGD("Decoder: %s does not support device type: %s\n", codec->name,
                     av_hwdevice_get_type_name(type));
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
                // AV_PIX_FMT_MEDIACODEC(165)
                hw_pix_fmt = config->pix_fmt;
                ILOGD("Decoder: %s support device type: %d, type name: %s, hw_pix_fmt: %d, AV_PIX_FMT_MEDIACODEC: %d\n", codec->name,
                     type, av_hwdevice_get_type_name(type), hw_pix_fmt, AV_PIX_FMT_MEDIACODEC);
                break;
            }
        }
#else
        pix_fmt = AV_PIX_FMT_NV12;
#endif
    } else if (encoder_type_ == EncoderType::LIBX264) {
        // codec = avcodec_find_encoder_by_name("libx264");

        ILOGD("Found video codec for libx264");
        codec = avcodec_find_encoder_by_name("libx264");
        if (!codec) {
            ILOGE("H264 encoder not found");
            return false;
        }
        pix_fmt = AV_PIX_FMT_NV12;
    }

    ILOGD("Setting pix fmt to %d %s\n", pix_fmt, av_get_pix_fmt_name(pix_fmt));

    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        ILOGE("Could not allocate video codec context");
        return false;
    }

    // Set codec parameters
    codec_context_->width = width;    // Replace with actual width
    codec_context_->height = height;  // Replace with actual height
    // codec_context_->time_base = (AVRational){1, fps};  // Example time base
    codec_context_->framerate = (AVRational) {fps, 1};  // Example frame rate
    // codec_context_->bit_rate = quality * kBitrateQualityScale;

    // These options are optional
    codec_context_->time_base = AV_TIME_BASE_Q;
    codec_context_->bit_rate = 2000000;
    codec_context_->level = 32;
    codec_context_->codec_id = AV_CODEC_ID_H264;
    codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context_->gop_size = 12;
#ifndef ANDROID
    codec_context_->max_b_frames = 1;
#endif
    codec_context_->pix_fmt = pix_fmt;

#ifdef SUPPORT_HW_ENCODER
    codec_context_->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    // Create a hardware frames context for the encoder
    AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx);
    if (!hw_frames_ref) {
      ILOGE("Failed to create VAAPI hardware frame context." );
      return false;
    }

    AVHWFramesContext* frames_ctx = reinterpret_cast<AVHWFramesContext*>(hw_frames_ref->data);
    switch (encoder_type_) {
      case EncoderType::VAAPI:
        frames_ctx->format = AV_PIX_FMT_VAAPI;  // VAAPI format
        frames_ctx->sw_format = AV_PIX_FMT_NV12;  // Typically used format for VAAPI
        break;
      case EncoderType::NVENC:
        frames_ctx->format = AV_PIX_FMT_CUDA;  // CUDA format, or another compatible one
        frames_ctx->sw_format = AV_PIX_FMT_NV12;  // Typically used format for VAAPI
        break;
      case EncoderType::MEDIACODEC:
        frames_ctx->format = AV_PIX_FMT_YUV420P;
        frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
        break;
      default:
        frames_ctx->format = AV_PIX_FMT_VAAPI;  // VAAPI format
    }
    ILOGD("Setting frame format to %d %s\n", frames_ctx->format, av_get_pix_fmt_name(frames_ctx->format));

    ILOGD("Setting frame sw_format to %d %s\n", frames_ctx->sw_format, av_get_pix_fmt_name(frames_ctx->sw_format));

    frames_ctx->width = codec_context_->width;
    frames_ctx->height = codec_context_->height;
    // frames_ctx->initial_pool_size = 20;

    if (av_hwframe_ctx_init(hw_frames_ref) < 0) {
      ILOGE("Failed to initialize " << kEncoderTypeNames[(int)encoder_type_] << " hardware frame context." );
      av_buffer_unref(&hw_frames_ref);
      return false;
    }

    codec_context_->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!codec_context_->hw_frames_ctx) {
      ILOGE("Failed to set hardware frame context for the codec context." );
      av_buffer_unref(&hw_frames_ref);
      return false;
    }

    av_buffer_unref(&hw_frames_ref);
    av_buffer_unref(&hw_device_ctx);
#endif

    AVDictionary *opts = nullptr;
    std::string options = "";
    auto ret = av_dict_parse_string(&opts, options.c_str(), "=", ",#\n", 0);
    if (ret < 0) {
        ILOGD("Could not parse ffmpeg encoder options list '%s'\n", options.c_str());
    } else {
        const AVDictionaryEntry *entry = av_dict_get(opts, "reorder_queue_size", nullptr,
                                                     AV_DICT_MATCH_CASE);
        if (entry) {
            ILOGD("reorder_queue_size \n");
            // remove it to prevent complaining later.
            av_dict_set(&opts, "reorder_queue_size", nullptr, AV_DICT_MATCH_CASE);
        }
    }

    if (avcodec_open2(codec_context_, codec, &opts) < 0) {
        ILOGE("Could not open codec");
        return false;
    }

    // Create new video stream
    video_stream_ = avformat_new_stream(format_context_, nullptr);
    if (!video_stream_) {
        ILOGE("Could not create video stream");
        return false;
    }

    video_stream_->codecpar->codec_id = codec->id;
    video_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_stream_->codecpar->width = codec_context_->width;
    video_stream_->codecpar->height = codec_context_->height;
    video_stream_->codecpar->format = codec_context_->pix_fmt;
    video_stream_->time_base = (AVRational) {1, AV_TIME_BASE};  // More granular time base

    avcodec_parameters_from_context(video_stream_->codecpar, codec_context_);

    if (!(format_context_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_context_->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
            ILOGE("Could not open output file");
            return false;
        }
    }

    if (avformat_write_header(format_context_, nullptr) < 0) {
        ILOGE("Error occurred when writing header");
        return false;
    }

    return true;
}

static size_t GetBufferSize(AVPixelFormat pf, unsigned int width, unsigned int height) {
    return av_image_get_buffer_size(pf, width, height, 1);
}

bool FFmpegEncoder::EncodeFrame(const std::string &img) {
    ILOGD("FFmpegEncoder::EncodeFrame - img= %s", img.c_str());
    // Load the image using FFmpeg
    AVPixelFormat in_pf = AV_PIX_FMT_BGR24;
    AVPixelFormat out_pf = AV_PIX_FMT_NV12;
#if USE_RAW
/* Warn if the input or output pixelformat is not supported */
    if (!sws_isSupportedInput(in_pf)) {
        ILOGE("FFmpegEncoder::EncodeFrame - swscale does not support the input format: %c%c%c%c",
              (in_pf) & 0xff, ((in_pf) & 0xff), ((in_pf >> 16) & 0xff), ((in_pf >> 24) & 0xff));
    }
    if (!sws_isSupportedOutput(out_pf)) {
        ILOGE("FFmpegEncoder::EncodeFrame - swscale does not support the output format: %c%c%c%c",
              (out_pf) & 0xff, ((out_pf >> 8) & 0xff), ((out_pf >> 16) & 0xff),
              ((out_pf >> 24) & 0xff));
    }

    int alignment = width % 32 ? 1 : 32;
    /* Check the buffer sizes */
    size_t needed_insize = GetBufferSize(in_pf, width, height);
    ILOGD("FFmpegEncoder::EncodeFrame - needed_insize=%ld", needed_insize);

    size_t needed_outsize = GetBufferSize(out_pf, width, height);
    ILOGD("FFmpegEncoder::EncodeFrame - needed_outsize=%ld", needed_outsize);

    std::ifstream inFile(img, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        ILOGE("Could not open the image file: %s", img.c_str());
        return false;
    }

    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!inFile.read(buffer.data(), size)) {
        ILOGE("Error reading the image file: %s", img.c_str());
        return false;
    }

#if USE_MRS_CODE
    SwsContext* sws_ctx = nullptr;
    int new_width = width;
    int new_height = height;
    uint8_t* in_buffer = reinterpret_cast<uint8_t *>(buffer.data());

    AVFrame *input_avframe = av_frame_alloc();
    AVFrame *output_avframe = av_frame_alloc();
    AVFrame* sw_frame = output_avframe;
    AVFrame *imgFrame = input_avframe;

    uint codec_imgsize = av_image_get_buffer_size(
        out_pf, width, height, alignment);
    ILOGD("FFmpegEncoder::EncodeFrame - buffer size %u from %s(%d) %dx%d, alignment=%d",
          codec_imgsize, av_get_pix_fmt_name(out_pf), out_pf, width, height, alignment);
    uint8_t *out_buffer = (uint8_t *)av_malloc(codec_imgsize);
    ILOGD("FFmpegEncoder::EncodeFrame - sws_getCachedContext(swscale=%p, width=%d, height=%d, in_pf=%d, new_width=%d, new_height=%d, out_pf=%d, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr)"
            , sws_ctx, width, height, in_pf, new_width, new_height, out_pf);
    /* Get the context */
    sws_ctx = sws_getCachedContext(sws_ctx,
                                       width, height, in_pf,
                                       new_width, new_height, out_pf,
                                       SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (sws_ctx == nullptr) {
        ILOGE("FFmpegEncoder::EncodeFrame - Failed getting swscale context");
        return -6;
    }

    /* Fill in the buffers */
    ILOGD("FFmpegEncoder::EncodeFrame - av_image_fill_arrays(input_avframe->data=%p, input_avframe->linesize=%d, in_buffer=%p, in_pf=%d, width=%d, height=%d, alignment=%d)"
            , input_avframe->data, input_avframe->linesize, (uint8_t*)in_buffer, in_pf, width, height, alignment);
    if (av_image_fill_arrays(input_avframe->data, input_avframe->linesize,
                             (uint8_t*)in_buffer, in_pf, width, height, alignment) <= 0) {
        ILOGE("FFmpegEncoder::EncodeFrame - Failed filling input frame with input buffer");
        return -7;
    }
    ILOGD("FFmpegEncoder::EncodeFrame - After calling av_image_fill_arrays, input_avframe:");
    dump_avframe_info(input_avframe);
    ILOGD("FFmpegEncoder::EncodeFrame - av_image_fill_arrays(output_avframe->data=%p, output_avframe->linesize=%d, out_buffer=%p, out_pf=%d, width=%d, height=%d, alignment=%d)"
            , output_avframe->data, output_avframe->linesize, out_buffer, out_pf, new_width, new_height, alignment);
    if (av_image_fill_arrays(output_avframe->data, output_avframe->linesize,
                             out_buffer, out_pf, new_width, new_height, alignment) <= 0) {
        ILOGE("FFmpegEncoder::EncodeFrame - Failed filling output frame with output buffer");
        return -8;
    }
    ILOGD("FFmpegEncoder::EncodeFrame - After calling av_image_fill_arrays, output_avframe:");
    dump_avframe_info(output_avframe);

    /* Do the conversion */
    ILOGD("FFmpegEncoder::EncodeFrame - sws_scale(sws_ctx=%p, input_avframe->data=%p, input_avframe->linesize=%d, 0, height=%d, output_avframe->data=%p, output_avframe->linesize=%d)"
            , sws_ctx, input_avframe->data, input_avframe->linesize, output_avframe->data, output_avframe->linesize);
    if (!sws_scale(sws_ctx,
                   input_avframe->data, input_avframe->linesize,
                   0, height,
                   output_avframe->data, output_avframe->linesize)) {
        ILOGE("FFmpegEncoder::EncodeFrame - swscale conversion failed");
        return -10;
    }
    sw_frame->format = out_pf;
    sw_frame->width = width;
    sw_frame->height = height;
#else
    // Allocate the input AVFrame
    AVFrame *imgFrame = av_frame_alloc();
    if (!imgFrame) {
        ILOGE("Could not allocate image frame");
        return false;
    }

    imgFrame->format = AV_PIX_FMT_BGR24;
    imgFrame->width = width;
    imgFrame->height = height;
    av_frame_get_buffer(imgFrame, 32);

    // Copy loaded data into imgFrame
    memcpy(imgFrame->data[0], buffer.data(), buffer.size());

    // Convert the image to NV12 format
    SwsContext *sws_ctx = sws_getContext(
            width, height, AV_PIX_FMT_BGR24,
            width, height, AV_PIX_FMT_NV12,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx) {
        ILOGE("Could not initialize the conversion context");
        av_frame_free(&imgFrame);
        return false;
    }

    AVFrame *sw_frame = av_frame_alloc();
    sw_frame->format = AV_PIX_FMT_NV12;
    sw_frame->width = width;
    sw_frame->height = height;
    av_frame_get_buffer(sw_frame, 32);

    ILOGD("FFmpegEncoder::EncodeFrame - imgFrame:");
    dump_avframe_info(imgFrame);
    ILOGD("FFmpegEncoder::EncodeFrame - sw_frame:");
    dump_avframe_info(sw_frame);

    sws_scale(sws_ctx, imgFrame->data, imgFrame->linesize, 0, height, sw_frame->data,
              sw_frame->linesize);
#endif

    // Load and encode image...
    AVPacket pkt;
    av_init_packet(&pkt);
#else
    AVFormatContext* imgFormatContext = nullptr;
    if (avformat_open_input(&imgFormatContext, img.c_str(), nullptr, nullptr) != 0) {
      ILOGE("Could not open the image file: %s", img.c_str() );
      return false;
    }

    if (avformat_find_stream_info(imgFormatContext, nullptr) < 0) {
      ILOGE("Could not find stream information in the image file" );
      avformat_close_input(&imgFormatContext);
      return false;
    }

    const AVCodec* imgCodec = avcodec_find_decoder(imgFormatContext->streams[0]->codecpar->codec_id);
    if (!imgCodec) {
      ILOGE("Unsupported codec for image" );
      avformat_close_input(&imgFormatContext);
      return false;
    }

    AVCodecContext* imgCodecContext = avcodec_alloc_context3(imgCodec);
    if (!imgCodecContext) {
      ILOGE("Could not allocate image codec context" );
      avformat_close_input(&imgFormatContext);
      return false;
    }

    if (avcodec_open2(imgCodecContext, imgCodec, nullptr) < 0) {
      ILOGE("Could not open image codec" );
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Load and encode image...
    AVPacket pkt;
    av_init_packet(&pkt);

    // Read the frame from the image file
    if (av_read_frame(imgFormatContext, &pkt) < 0) {
      ILOGE("Failed to read frame from image file" );
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Send the packet to the decoder
    if (avcodec_send_packet(imgCodecContext, &pkt) < 0) {
      ILOGE("Error sending a packet for decoding" );
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Allocate an AVFrame to hold the decoded image
    AVFrame* imgFrame = av_frame_alloc();
    if (!imgFrame) {
      ILOGE("Could not allocate image frame" );
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Receive the frame from the decoder
    if (avcodec_receive_frame(imgCodecContext, imgFrame) < 0) {
      ILOGE("Error during decoding" );
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Convert the image to the correct format
    SwsContext* sws_ctx = sws_getContext(
        imgCodecContext->width, imgCodecContext->height, imgCodecContext->pix_fmt,
        codec_context_->width, codec_context_->height, AV_PIX_FMT_NV12,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
      ILOGE("Could not initialize the conversion context" );
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Convert and encode the frame
    AVFrame* sw_frame = av_frame_alloc();
    sw_frame->format = AV_PIX_FMT_NV12;
    sw_frame->width = codec_context_->width;
    sw_frame->height = codec_context_->height;
    if (av_frame_get_buffer(sw_frame, 32) < 0) {
      ILOGE("Could not allocate frame buffer" );
      sws_freeContext(sws_ctx);
      av_frame_free(&sw_frame);
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    sws_scale(sws_ctx, imgFrame->data, imgFrame->linesize, 0, codec_context_->height, sw_frame->data, sw_frame->linesize);
#endif

    // Set PTS for the frame
    sw_frame->pts = next_pts;
    next_pts += pts_increment;

#ifdef SUPPORT_HW_ENCODER
    // Create a hardware frame for encoding
    AVFrame* hw_frame = av_frame_alloc();
    // hw_frame->format = AV_PIX_FMT_VAAPI;
    hw_frame->format = AV_PIX_FMT_YUV420P;
    hw_frame->width = codec_context_->width;
    hw_frame->height = codec_context_->height;
    hw_frame->pts = sw_frame->pts;

    if (av_hwframe_get_buffer(codec_context_->hw_frames_ctx, hw_frame, 0) < 0) {
      ILOGE("Failed to allocate VAAPI frame." );
      sws_freeContext(sws_ctx);
      av_frame_free(&hw_frame);
      av_frame_free(&sw_frame);
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Transfer the data from sw_frame to hw_frame
    if (av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0) {
      ILOGE("Error transferring frame data to VAAPI surface." );
      sws_freeContext(sws_ctx);
      av_frame_free(&hw_frame);
      av_frame_free(&sw_frame);
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }

    // Encode the frame
    if (avcodec_send_frame(codec_context_, hw_frame) < 0) {
      ILOGE("Error sending the frame to the hardware encoder" );
      sws_freeContext(sws_ctx);
      av_frame_free(&hw_frame);
      av_frame_free(&sw_frame);
      av_frame_free(&imgFrame);
      av_packet_unref(&pkt);
      avcodec_free_context(&imgCodecContext);
      avformat_close_input(&imgFormatContext);
      return false;
    }
#else

    ILOGD("FFmpegEncoder::EncodeFrame - Before sending to encoder, sw_frame:");
    dump_avframe_info(sw_frame);

    // Fallback to software encoding
    if (avcodec_send_frame(codec_context_, sw_frame) < 0) {
        ILOGE("Error sending the sw_frame to the encoder");
        sws_freeContext(sws_ctx);
        av_frame_free(&sw_frame);
        av_frame_free(&imgFrame);
        av_packet_unref(&pkt);
#if !(USE_RAW)
        avcodec_free_context(&imgCodecContext);
        avformat_close_input(&imgFormatContext);
#endif
        return false;
    }
#endif

    // Receive and write the encoded packet
    auto ret = 0;
    while (ret >= 0) {
        ILOGD("avcodec_receive_packet");
        ret = avcodec_receive_packet(codec_context_, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            ILOGE("Error during encoding");
            break;
        }

        // Write the encoded packet to the file
        av_interleaved_write_frame(format_context_, &pkt);
        av_packet_unref(&pkt);
    }

    // Cleanup
    sws_freeContext(sws_ctx);
#ifdef SUPPORT_HW_ENCODER
    av_frame_free(&hw_frame);
#endif
    av_frame_free(&sw_frame);
    av_frame_free(&imgFrame);
    av_packet_unref(&pkt);
#if !(USE_RAW)
    avcodec_free_context(&imgCodecContext);
    avformat_close_input(&imgFormatContext);
#endif

    return true;
}

bool FFmpegEncoder::WriteTrailer() {
    if (av_write_trailer(format_context_) < 0) {
        ILOGE("Error occurred when writing trailer");
        return false;
    }
    return true;
}
