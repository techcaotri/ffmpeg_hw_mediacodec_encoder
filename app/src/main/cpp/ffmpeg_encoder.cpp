#include "ffmpeg_encoder.h"
#ifndef ANDROID
#include "nvenc_utils.h"
#endif

#include "my_log.h"

constexpr int kBitrateQualityScale = 200000;
const char* kEncoderTypeNames[] = {"VAAPI", "NVENC", "MEDIACODEC", "LIBX264"};

FFmpegEncoder::FFmpegEncoder(EncoderType pEncoderType, int pWidth, int pHeight,int pQuality, int pFps)
    : format_context_(nullptr), codec_context_(nullptr), video_stream_(nullptr),
#ifdef SUPPORT_HW_ENCODER
  hw_device_ctx(nullptr),
#endif
  next_pts(0), pts_increment((AV_TIME_BASE + 30 / 2) / 30), encoder_type_(pEncoderType), width(pWidth), height(pHeight), fps(pFps), quality(pQuality) {
  // Constructor initialization
  // av_register_all();
  // avcodec_register_all();
  av_log_set_level(AV_LOG_DEBUG);

#ifdef SUPPORT_HW_ENCODER
  InitializeHWContext();
#endif
}

FFmpegEncoder::~FFmpegEncoder() {
  Cleanup();  // Cleanup resources
}

bool FFmpegEncoder::Initialize(const std::string& output_file) {
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

bool FFmpegEncoder::OpenVideoFile(const std::string& output_file) {
  // Allocate format context
  avformat_alloc_output_context2(&format_context_, nullptr, nullptr, output_file.c_str());
  if (!format_context_) {
    ILOGE("Could not allocate format context");
    return false;
  }

  return true;
}

bool FFmpegEncoder::SetupEncoder(const std::string& output_file) {
  // Find the encoder
  const AVCodec* codec = nullptr;

  AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

  if (encoder_type_ == EncoderType::VAAPI) {
    codec = avcodec_find_encoder_by_name("h264_vaapi");
    if (!codec) {
      ILOGE("VAAPI encoder not found" );
      return false;
    }
    ILOGD("Found video codec for %s\n", "h264_vaapi");
    pix_fmt = AV_PIX_FMT_VAAPI;
  } else if (encoder_type_ == EncoderType::NVENC) {
    codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
      ILOGE("NVENC encoder not found" );
      return false;
    }
    pix_fmt = AV_PIX_FMT_CUDA;  // NV12 is typically supported by NVENC

    if (!support_multiple_ref_frames_) codec_context_->refs = 0;
  } else if (encoder_type_ == EncoderType::MEDIACODEC) {
    codec = avcodec_find_encoder_by_name("h264_mediacodec");
    if (!codec) {
      ILOGE("MediaCodec encoder not found" );
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
  } else 
  if (encoder_type_ == EncoderType::LIBX264) {
    // codec = avcodec_find_encoder_by_name("libx264");

    ILOGD("Found video codec for libx264" );
    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
      ILOGE("H264 encoder not found" );
      return false;
    }
    pix_fmt = AV_PIX_FMT_NV12;
  }

  ILOGD("Setting pix fmt to %d %s\n", pix_fmt, av_get_pix_fmt_name(pix_fmt));

  codec_context_ = avcodec_alloc_context3(codec);
  if (!codec_context_) {
    ILOGE("Could not allocate video codec context" );
    return false;
  }

  // Set codec parameters
  codec_context_->width = width;    // Replace with actual width
  codec_context_->height = height;  // Replace with actual height
  // codec_context_->time_base = (AVRational){1, fps};  // Example time base
  codec_context_->framerate = (AVRational){fps, 1};  // Example frame rate
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

  AVDictionary* opts = nullptr;
  std::string options = "";
  auto ret = av_dict_parse_string(&opts, options.c_str(), "=", ",#\n", 0);
  if (ret < 0) {
    ILOGD("Could not parse ffmpeg encoder options list '%s'\n", options.c_str());
  } else {
    const AVDictionaryEntry* entry = av_dict_get(opts, "reorder_queue_size", nullptr, AV_DICT_MATCH_CASE);
    if (entry) {
      ILOGD("reorder_queue_size \n");
      // remove it to prevent complaining later.
      av_dict_set(&opts, "reorder_queue_size", nullptr, AV_DICT_MATCH_CASE);
    }
  }

  if (avcodec_open2(codec_context_, codec, &opts) < 0) {
    ILOGE("Could not open codec" );
    return false;
  }

  // Create new video stream
  video_stream_ = avformat_new_stream(format_context_, nullptr);
  if (!video_stream_) {
    ILOGE("Could not create video stream" );
    return false;
  }

  video_stream_->codecpar->codec_id = codec->id;
  video_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  video_stream_->codecpar->width = codec_context_->width;
  video_stream_->codecpar->height = codec_context_->height;
  video_stream_->codecpar->format = codec_context_->pix_fmt;
  video_stream_->time_base = (AVRational){1, AV_TIME_BASE};  // More granular time base

  avcodec_parameters_from_context(video_stream_->codecpar, codec_context_);

  if (!(format_context_->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&format_context_->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
      ILOGE("Could not open output file" );
      return false;
    }
  }

  if (avformat_write_header(format_context_, nullptr) < 0) {
    ILOGE("Error occurred when writing header" );
    return false;
  }

  return true;
}

bool FFmpegEncoder::EncodeFrame(const std::string& img) {
  ILOGD("FFmpegEncoder::EncodeFrame - img= %s", img.c_str() );
  // Load the image using FFmpeg
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
  // Fallback to software encoding
  if (avcodec_send_frame(codec_context_, sw_frame) < 0) {
    ILOGE("Error sending the frame to the software encoder" );
    sws_freeContext(sws_ctx);
    av_frame_free(&sw_frame);
    av_frame_free(&imgFrame);
    av_packet_unref(&pkt);
    avcodec_free_context(&imgCodecContext);
    avformat_close_input(&imgFormatContext);
    return false;
  }
#endif

  // Receive and write the encoded packet
  auto ret = 0;
  while (ret >= 0) {
    ILOGD("avcodec_receive_packet" );
    ret = avcodec_receive_packet(codec_context_, &pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      ILOGE("Error during encoding" );
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
  avcodec_free_context(&imgCodecContext);
  avformat_close_input(&imgFormatContext);

  return true;
}

bool FFmpegEncoder::WriteTrailer() {
  if (av_write_trailer(format_context_) < 0) {
    ILOGE("Error occurred when writing trailer" );
    return false;
  }
  return true;
}
