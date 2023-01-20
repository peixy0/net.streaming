#pragma once

#include <string>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
}

namespace codec {

void DisableCodecLogs();

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
  explicit Decoder(const DecoderOptions&);
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

class FilteredDataProcessor {
public:
  virtual ~FilteredDataProcessor() = default;
  virtual void ProcessFilteredData(AVFrame*) = 0;
};

struct FilterOptions {
  int width;
  int height;
  int framerate;
  std::string inFormat;
  std::string outFormat;
  std::string description;
};

class Filter {
public:
  Filter(const FilterOptions&);
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
  virtual void ProcessEncodedData(AVPacket*) = 0;
};

struct EncoderOptions {
  std::string codec;
  std::string pixfmt;
  int width;
  int height;
  int framerate;
  int bitrate;
};

class Encoder {
public:
  explicit Encoder(const EncoderOptions&);
  Encoder(const Encoder&) = delete;
  ~Encoder();
  void Encode(AVFrame*, EncodedDataProcessor&);
  void Flush(EncodedDataProcessor&) const;

private:
  void GetEncodedPacket(EncodedDataProcessor&) const;

  AVCodecContext* context{nullptr};
  AVFrame* frame{nullptr};
  AVPacket* packet{nullptr};
  std::int64_t pts{0};
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

class WriterProcessor {
public:
  virtual ~WriterProcessor() = default;
  virtual void WriteData(std::string_view) = 0;
};

struct WriterOptions {
  std::string format;
  std::string codec;
  int width;
  int height;
  int framerate;
  int bitrate;
};

class Writer {
public:
  explicit Writer(const WriterOptions&);
  virtual ~Writer();
  virtual void Begin() = 0;
  virtual void End() = 0;
  void Process(AVPacket*);

protected:
  WriterOptions options;
  AVFormatContext* formatContext{nullptr};

private:
  AVStream* stream;
  AVPacket* packet{nullptr};
};

class BufferWriter : public Writer {
public:
  BufferWriter(const WriterOptions&, WriterProcessor&);
  ~BufferWriter() override = default;
  void Begin() override;
  void End() override;
  void WriterCallback(std::string_view);

private:
  WriterProcessor& processor;
  std::uint8_t* buffer{nullptr};
  int bufferSize{1 << 16};
};

class FileWriter : public Writer {
public:
  FileWriter(const WriterOptions&, std::string_view);
  ~FileWriter() override = default;
  void Begin() override;
  void End() override;

private:
  std::string filename;
};

}  // namespace codec
