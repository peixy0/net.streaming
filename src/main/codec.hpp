#pragma once

#include <chrono>
#include <string>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

namespace codec {

void DisableCodecLogs();

enum class PixelFormat { YUVJ422, YUV422, YUV420 };

class FilteredDataProcessor {
public:
  virtual ~FilteredDataProcessor() = default;
  virtual void ProcessFilteredData(AVFrame*) = 0;
};

struct FilterOptions {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t framerate;
  PixelFormat inFormat;
  PixelFormat outFormat;
};

class Filter {
public:
  Filter(FilterOptions&&);
  ~Filter();
  void Process(AVFrame*, FilteredDataProcessor&);

private:
  AVFilterContext* contextIn{nullptr};
  AVFilterContext* contextOut{nullptr};
  AVFilterInOut* filterIn{nullptr};
  AVFilterInOut* filterOut{nullptr};
  AVFilterGraph* graph{nullptr};
  AVFrame* frame{nullptr};
};

class EncodedDataProcessor {
public:
  virtual ~EncodedDataProcessor() = default;
  virtual void ProcessEncodedData(std::string_view) = 0;
};

struct EncoderOptions {
  std::string codec;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t framerate;
  std::uint32_t bitrate;
};

class Encoder {
public:
  explicit Encoder(EncoderOptions&&);
  Encoder(const Encoder&) = delete;
  ~Encoder();
  void Encode(AVFrame*, EncodedDataProcessor&);
  void Flush(EncodedDataProcessor&) const;

private:
  void GetEncodedPacket(EncodedDataProcessor&) const;

  AVCodecContext* context{nullptr};
  AVFrame* frame{nullptr};
  AVPacket* packet{nullptr};
  int pts{0};
};

class DecodedDataProcessor {
public:
  virtual ~DecodedDataProcessor() = default;
  virtual void ProcessDecodedData(AVFrame*) = 0;
};

struct DecoderOptions {
  std::string codec;
};

class Decoder {
public:
  explicit Decoder(DecoderOptions&&);
  Decoder(const Decoder&) = delete;
  ~Decoder();
  void Decode(std::string_view, DecodedDataProcessor&) const;
  void Flush(DecodedDataProcessor&) const;

private:
  void GetDecodedFrame(DecodedDataProcessor&) const;

  AVCodecContext* context{nullptr};
  AVFrame* frame{nullptr};
  AVPacket* packet{nullptr};
};

class Transcoder {
public:
  Transcoder(Decoder&, Filter&, Encoder&);
  void Process(std::string_view, EncodedDataProcessor&);
  void Flush(EncodedDataProcessor&);

private:
  Decoder& decoder;
  Filter& filter;
  Encoder& encoder;
};

}  // namespace codec
