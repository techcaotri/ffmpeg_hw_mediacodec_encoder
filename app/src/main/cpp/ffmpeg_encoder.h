#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <iostream>
#include <string>
#include <vector>

#define USE_RAW 1

// #define SUPPORT_HW_ENCODER

class FFmpegEncoder {
 public:
  enum class EncoderType { VAAPI, NVENC, MEDIACODEC, LIBX264 };
  
  FFmpegEncoder(EncoderType pEncoderType, int pWidth, int pHeight, int pQuality=4, int pFps=30);
  ~FFmpegEncoder();
  bool Initialize(const std::string& output_file);
  bool EncodeFrame(const std::string& img);

 private:
  EncoderType      encoder_type_;
  AVFormatContext* format_context_;
  AVCodecContext*  codec_context_;
  AVStream*        video_stream_;
#ifdef SUPPORT_HW_ENCODER
  AVBufferRef*     hw_device_ctx;
#endif
  int64_t          next_pts;
  int64_t          pts_increment;
  int              quality;
  int              fps;
  int              width;
  int              height;
  bool             support_multiple_ref_frames_;

  bool OpenVideoFile(const std::string& output_file);
  bool SetupEncoder(const std::string& output_file);
#ifdef SUPPORT_HW_ENCODER
  bool InitializeHWContext();
#endif
  bool WriteTrailer();
  void Cleanup();
};

#endif /* FFMPEGENCODER_H */
