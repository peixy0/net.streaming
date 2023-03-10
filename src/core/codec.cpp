#include "codec.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

namespace {

AVPixelFormat ConvertPixFormat(std::string_view fmt) {
  if (fmt == "YUVJ422") {
    return AV_PIX_FMT_YUVJ422P;
  }
  if (fmt == "YUV422") {
    return AV_PIX_FMT_YUV422P;
  }
  if (fmt == "YUV420") {
    return AV_PIX_FMT_YUV420P;
  }
  if (fmt == "NV12") {
    return AV_PIX_FMT_NV12;
  }
  throw std::invalid_argument("PIX_FMT not supported");
}

int WriterCallbackHelper(void* writer_, std::uint8_t* buffer, int size) {
  codec::BufferWriter* writer = reinterpret_cast<codec::BufferWriter*>(writer_);
  const char* p = reinterpret_cast<char*>(buffer);
  writer->WriterCallback({p, p + size});
  return size;
}

}  // namespace

namespace codec {

void DisableCodecLogs() {
  av_log_set_level(AV_LOG_QUIET);
}

class PacketRefGuard {
public:
  explicit PacketRefGuard(AVPacket* packet) : packet{packet} {
  }

  PacketRefGuard(const PacketRefGuard&) = delete;

  ~PacketRefGuard() {
    av_packet_unref(packet);
  }

private:
  AVPacket* packet;
};

class FrameRefGuard {
public:
  explicit FrameRefGuard(AVFrame* frame) : frame{frame} {
  }

  FrameRefGuard(const FrameRefGuard&) = delete;

  ~FrameRefGuard() {
    av_frame_unref(frame);
  }

private:
  AVFrame* frame;
};

Decoder::Decoder(const DecoderOptions& options) {
  const auto* codec = avcodec_find_decoder_by_name(options.codec.c_str());
  if (codec == nullptr) {
    spdlog::error("codec avcodec_find_decoder_by_name({})", options.codec);
    return;
  }
  context = avcodec_alloc_context3(codec);
  if (context == nullptr) {
    spdlog::error("codec avcodec_alloc_context3");
    return;
  }

  int r;
  context->framerate.num = 0;
  context->framerate.den = 1;
  if ((r = avcodec_open2(context, codec, nullptr)) < 0) {
    spdlog::error("codec avcodec_open2(): {}", r);
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("codec av_frame_alloc()");
    return;
  }
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("codec av_packet_alloc()");
    return;
  }
}

Decoder::~Decoder() {
  av_packet_free(&packet);
  av_frame_free(&frame);
  avcodec_free_context(&context);
}

void Decoder::GetDecodedFrame(DecodedDataProcessor& processor) const {
  while (true) {
    const int r = avcodec_receive_frame(context, frame);
    if (r == AVERROR(EAGAIN) or r == AVERROR_EOF) {
      return;
    }
    if (r < 0) {
      spdlog::error("codec avcodec_receive_frame(): {}", r);
      return;
    }
    FrameRefGuard frameRef{frame};
    processor.ProcessDecodedData(frame);
  }
}

void Decoder::Decode(std::string_view buf, DecodedDataProcessor& processor) const {
  int r;
  std::string bufCopy{buf};
  packet->data = reinterpret_cast<std::uint8_t*>(bufCopy.data());
  packet->size = static_cast<int>(bufCopy.size());
  if ((r = avcodec_send_packet(context, packet)) < 0) {
    spdlog::error("codec avcodec_send_packet(): {}", r);
    return;
  }
  GetDecodedFrame(processor);
}

void Decoder::Flush(DecodedDataProcessor& processor) const {
  int r;
  if ((r = avcodec_send_packet(context, nullptr)) < 0) {
    spdlog::error("codec avcodec_send_packet(): {}", r);
    return;
  }
  GetDecodedFrame(processor);
}

Filter::Filter(const FilterOptions& options) {
  int r;
  const AVFilter* bufferIn = avfilter_get_by_name("buffer");
  const AVFilter* bufferOut = avfilter_get_by_name("buffersink");
  filterIn = avfilter_inout_alloc();
  filterOut = avfilter_inout_alloc();
  graph = avfilter_graph_alloc();
  char args[512];
  std::snprintf(args, sizeof args, "video_size=%dx%d:pix_fmt=%d:time_base=1/%d:pixel_aspect=1/1", options.width,
      options.height, ConvertPixFormat(options.inFormat), options.framerate);
  if ((r = avfilter_graph_create_filter(&contextIn, bufferIn, "in", args, nullptr, graph)) < 0) {
    spdlog::error("codec avfilter_graph_create_filter(in): {}", r);
    return;
  }
  if ((r = avfilter_graph_create_filter(&contextOut, bufferOut, "out", nullptr, nullptr, graph)) < 0) {
    spdlog::error("codec avfilter_graph_create_filter(out): {}", r);
    return;
  }
  const auto outFmt = ConvertPixFormat(options.outFormat);
  if ((r = av_opt_set_bin(contextOut, "pix_fmts", reinterpret_cast<const std::uint8_t*>(&outFmt), sizeof outFmt,
           AV_OPT_SEARCH_CHILDREN)) < 0) {
    spdlog::error("codec av_opt_set_bin(): {}", r);
    return;
  }

  filterOut->name = av_strdup("in");
  filterOut->filter_ctx = contextIn;
  filterOut->pad_idx = 0;
  filterOut->next = nullptr;

  filterIn->name = av_strdup("out");
  filterIn->filter_ctx = contextOut;
  filterIn->pad_idx = 0;
  filterIn->next = nullptr;

  if ((r = avfilter_graph_parse_ptr(graph, options.description.c_str(), &filterIn, &filterOut, nullptr)) < 0) {
    spdlog::error("codec avfilter_graph_parse_ptr(): {}", r);
    return;
  }
  if ((r = avfilter_graph_config(graph, nullptr)) < 0) {
    spdlog::error("codec avfilter_graph_config(): {}", r);
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("codec av_frame_alloc()");
    return;
  }
}

Filter::~Filter() {
  avfilter_graph_free(&graph);
  avfilter_inout_free(&filterIn);
  avfilter_inout_free(&filterOut);
}

void Filter::Process(AVFrame* in, FilteredDataProcessor& processor) {
  int r;
  if ((r = av_buffersrc_add_frame(contextIn, in)) < 0) {
    spdlog::error("codec av_buffersrc_add_frame(): {}", r);
    return;
  }
  while (true) {
    int r = av_buffersink_get_frame(contextOut, frame);
    if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
      return;
    }
    if (r < 0) {
      spdlog::error("codec av_buffersink_get_frame(): {}", r);
      return;
    }
    FrameRefGuard frameRef{frame};
    processor.ProcessFilteredData(frame);
  }
}

Encoder::Encoder(const EncoderOptions& options) {
  int r;
  const auto* codec = avcodec_find_encoder_by_name(options.codec.c_str());
  if (codec == nullptr) {
    spdlog::error("codec avcodec_find_encoder_by_name({})", options.codec);
    return;
  }
  context = avcodec_alloc_context3(codec);
  if (context == nullptr) {
    spdlog::error("codec avcodec_alloc_context3()");
    return;
  }
  context->width = options.width;
  context->height = options.height;
  context->time_base.num = 1;
  context->time_base.den = options.framerate;
  context->framerate.num = options.framerate;
  context->framerate.den = 1;
  context->gop_size = 12;
  context->pix_fmt = ConvertPixFormat(options.pixfmt);
  context->color_range = AVCOL_RANGE_JPEG;
  context->bit_rate = options.bitrate;
  context->bit_rate_tolerance = options.bitrate / 2;
  av_opt_set(context->priv_data, "preset", "fast", 0);
  if ((r = avcodec_open2(context, codec, nullptr)) < 0) {
    spdlog::error("codec avcodec_open2(): {}", r);
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("codec av_frame_alloc()");
    return;
  }
  frame->format = context->pix_fmt;
  frame->width = context->width;
  frame->height = context->height;
  if ((r = av_frame_get_buffer(frame, 0)) < 0) {
    spdlog::error("codec av_frame_get_buffer(): {}", r);
    return;
  }
  if ((r = av_frame_make_writable(frame)) < 0) {
    spdlog::error("codec av_frame_make_writable(): {}", r);
    return;
  }
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("codec av_packet_alloc()");
    return;
  }
}

Encoder::~Encoder() {
  av_packet_free(&packet);
  av_frame_free(&frame);
  avcodec_free_context(&context);
}

void Encoder::GetEncodedPacket(EncodedDataProcessor& processor) const {
  while (true) {
    const int r = avcodec_receive_packet(context, packet);
    if (r == AVERROR(EAGAIN) or r == AVERROR_EOF) {
      return;
    }
    if (r < 0) {
      spdlog::error("codec avcodec_receive_packet(): {}", r);
      return;
    }
    const PacketRefGuard packetRef{packet};
    processor.ProcessEncodedData(packet);
  }
}

void Encoder::Encode(AVFrame* frame, EncodedDataProcessor& processor) {
  int r;
  frame->pts = pts++;
  if ((r = avcodec_send_frame(context, frame)) < 0) {
    spdlog::error("codec avcodec_send_frame(): {}", r);
    return;
  }
  GetEncodedPacket(processor);
}

void Encoder::Flush(EncodedDataProcessor& processor) const {
  int r;
  if ((r = avcodec_send_frame(context, nullptr)) < 0) {
    spdlog::error("codec avcodec_send_frame(): {}", r);
    return;
  }
  GetEncodedPacket(processor);
}

class TranscoderHelper : public DecodedDataProcessor, public FilteredDataProcessor {
public:
  TranscoderHelper(Filter& filter, Encoder& encoder, EncodedDataProcessor& processor)
      : filter{filter}, encoder{encoder}, processor{processor} {
  }

  void ProcessDecodedData(AVFrame* frame) override {
    filter.Process(frame, *this);
  }

  void ProcessFilteredData(AVFrame* frame) override {
    encoder.Encode(frame, processor);
  }

private:
  Filter& filter;
  Encoder& encoder;
  EncodedDataProcessor& processor;
};

Transcoder::Transcoder(Decoder& decoder, Filter& filter, Encoder& encoder)
    : decoder{decoder}, filter{filter}, encoder{encoder} {
}

void Transcoder::Process(std::string_view buf, EncodedDataProcessor& processor) {
  TranscoderHelper helper{filter, encoder, processor};
  decoder.Decode(buf, helper);
}

void Transcoder::Flush(EncodedDataProcessor& processor) {
  TranscoderHelper helper{filter, encoder, processor};
  decoder.Flush(helper);
  encoder.Flush(processor);
}

Writer::Writer(const WriterOptions& options) : options{options} {
  int r;
  if ((r = avformat_alloc_output_context2(&formatContext, nullptr, options.format.c_str(), nullptr)) < 0) {
    spdlog::error("codec avformat_alloc_output_context2(): {}", r);
    return;
  }
  formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
  stream = avformat_new_stream(formatContext, nullptr);
  if (stream == nullptr) {
    spdlog::error("codec avformat_new_stream()");
    return;
  }
  const auto* codec = avcodec_find_encoder_by_name(options.codec.c_str());
  if (codec == nullptr) {
    spdlog::error("codec avcodec_find_encoder_by_name({})", options.codec);
    return;
  }
  stream->codecpar->codec_id = codec->id;
  stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  stream->codecpar->width = options.width;
  stream->codecpar->height = options.height;
  stream->codecpar->bit_rate = options.bitrate;
  stream->time_base.num = 1;
  stream->time_base.den = options.framerate;
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("codec av_packet_alloc()");
    return;
  }
}

Writer::~Writer() {
  av_packet_free(&packet);
  avformat_free_context(formatContext);
  formatContext = nullptr;
}

void Writer::Process(AVPacket* packet) {
  int r;
  av_packet_rescale_ts(packet, {1, options.framerate}, stream->time_base);
  if ((r = av_interleaved_write_frame(formatContext, packet)) < 0) {
    spdlog::error("codec av_interleaved_write_frame(): {}", r);
    return;
  }
}

BufferWriter::BufferWriter(const WriterOptions& options, WriterProcessor& processor)
    : Writer{options}, processor{processor} {
}

void BufferWriter::Begin() {
  constexpr int writable = 1;
  buffer = static_cast<std::uint8_t*>(av_malloc(bufferSize));
  formatContext->pb = avio_alloc_context(buffer, bufferSize, writable, this, nullptr, WriterCallbackHelper, nullptr);
  if (formatContext->pb == nullptr) {
    spdlog::error("codec avio_alloc_context()");
    return;
  }
  formatContext->flags = AVFMT_FLAG_CUSTOM_IO;
  formatContext->pb->seekable = 0;
  int r;
  if ((r = avformat_write_header(formatContext, nullptr)) < 0) {
    spdlog::error("codec avformat_write_header(): {}", r);
    return;
  }
}

void BufferWriter::End() {
  if (formatContext and formatContext->pb) {
    av_write_trailer(formatContext);
    avio_context_free(&formatContext->pb);
  }
  av_free(buffer);
}

void BufferWriter::WriterCallback(std::string_view buffer) {
  processor.WriteData(buffer);
}

FileWriter::FileWriter(const WriterOptions& options, std::string_view filename) : Writer{options}, filename{filename} {
}

void FileWriter::Begin() {
  int r;
  if ((r = avio_open2(&formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr)) < 0) {
    spdlog::error("codec avio_open2()");
    return;
  }
  if (formatContext->pb == nullptr) {
    spdlog::error("codec avio_open2()");
    return;
  }
  if ((r = avformat_write_header(formatContext, nullptr)) < 0) {
    spdlog::error("codec avformat_write_header(): {}", r);
    return;
  }
}

void FileWriter::End() {
  if (formatContext and formatContext->pb) {
    av_write_trailer(formatContext);
    avio_close(formatContext->pb);
  }
}

}  // namespace codec
