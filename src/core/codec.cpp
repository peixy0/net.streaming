#include "codec.hpp"
#include <spdlog/spdlog.h>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

namespace {

AVPixelFormat convert(codec::PixelFormat fmt) {
  switch (fmt) {
    case codec::PixelFormat::YUVJ422:
      return AV_PIX_FMT_YUVJ422P;
    case codec::PixelFormat::YUV422:
      return AV_PIX_FMT_YUV422P;
    case codec::PixelFormat::YUV420:
      return AV_PIX_FMT_YUV420P;
  }
  return AV_PIX_FMT_YUV420P;
}

}  // namespace

namespace codec {

void DisableCodecLogs() {
  av_log_set_level(AV_LOG_QUIET);
}

class PacketRefGuard {
public:
  PacketRefGuard(AVPacket* packet) : packet{packet} {
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
  FrameRefGuard(AVFrame* frame) : frame{frame} {
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
    spdlog::error("error finding decoder");
    return;
  }
  context = avcodec_alloc_context3(codec);
  if (context == nullptr) {
    spdlog::error("error allocating decoder context");
    return;
  }
  context->framerate.num = 0;
  context->framerate.den = 1;
  if (avcodec_open2(context, codec, nullptr) < 0) {
    spdlog::error("error initializng decoder context");
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("error allocating frame");
    return;
  }
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("error allocating packet");
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
      spdlog::error("error receiving frame");
      return;
    }
    FrameRefGuard frameRef{frame};
    processor.ProcessDecodedData(frame);
  }
}

void Decoder::Decode(std::string_view buf, DecodedDataProcessor& processor) const {
  std::string bufCopy{buf};
  packet->data = reinterpret_cast<std::uint8_t*>(bufCopy.data());
  packet->size = bufCopy.size();
  if (avcodec_send_packet(context, packet) < 0) {
    spdlog::error("error sending packet");
    return;
  }
  GetDecodedFrame(processor);
}

void Decoder::Flush(DecodedDataProcessor& processor) const {
  if (avcodec_send_packet(context, nullptr) < 0) {
    spdlog::error("error sending packet");
    return;
  }
  GetDecodedFrame(processor);
}

Filter::Filter(const FilterOptions& options) {
  const AVFilter* bufferIn = avfilter_get_by_name("buffer");
  const AVFilter* bufferOut = avfilter_get_by_name("buffersink");
  filterIn = avfilter_inout_alloc();
  filterOut = avfilter_inout_alloc();
  graph = avfilter_graph_alloc();
  char args[512];
  std::snprintf(args, sizeof args, "video_size=%dx%d:pix_fmt=%d:time_base=1/%d:pixel_aspect=1/1", options.width,
                options.height, convert(options.inFormat), options.framerate);
  if (avfilter_graph_create_filter(&contextIn, bufferIn, "in", args, nullptr, graph) < 0) {
    spdlog::error("error creating in filter");
    return;
  }
  if (avfilter_graph_create_filter(&contextOut, bufferOut, "out", nullptr, nullptr, graph) < 0) {
    spdlog::error("error creating out filter");
    return;
  }
  const auto outFmt = convert(options.outFormat);
  if (av_opt_set_bin(contextOut, "pix_fmts", reinterpret_cast<const std::uint8_t*>(&outFmt), sizeof outFmt,
                     AV_OPT_SEARCH_CHILDREN) < 0) {
    spdlog::error("error setting out format");
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

  if (not filterOut->name or not filterIn->name) {
    spdlog::error("error getting filter names");
    return;
  }
  if (avfilter_graph_parse_ptr(graph, options.description.c_str(), &filterIn, &filterOut, nullptr) < 0) {
    spdlog::error("error adding graph");
    return;
  }
  if (avfilter_graph_config(graph, nullptr) < 0) {
    spdlog::error("error initializing graph config");
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("error allocating filter frame");
    return;
  }
}

Filter::~Filter() {
  avfilter_graph_free(&graph);
  avfilter_inout_free(&filterIn);
  avfilter_inout_free(&filterOut);
}

void Filter::Process(AVFrame* in, FilteredDataProcessor& processor) {
  if (av_buffersrc_add_frame(contextIn, in) < 0) {
    spdlog::error("error adding frame to filter");
    return;
  }
  while (true) {
    int r = av_buffersink_get_frame(contextOut, frame);
    if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
      return;
    }
    if (r < 0) {
      spdlog::error("error receiving frame");
      return;
    }
    FrameRefGuard frameRef{frame};
    processor.ProcessFilteredData(frame);
  }
}

Encoder::Encoder(const EncoderOptions& options) {
  const auto* codec = avcodec_find_encoder_by_name(options.codec.c_str());
  if (codec == nullptr) {
    spdlog::error("error finding encoder");
    return;
  }
  context = avcodec_alloc_context3(codec);
  if (context == nullptr) {
    spdlog::error("error allocating encoder context");
    return;
  }
  context->bit_rate = options.bitrate;
  context->bit_rate_tolerance = options.bitrate / 2;
  context->width = options.width;
  context->height = options.height;
  context->time_base.num = 1;
  context->time_base.den = options.framerate;
  context->framerate.num = options.framerate;
  context->framerate.den = 1;
  context->gop_size = 12;
  context->pix_fmt = convert(options.format);
  context->color_range = AVCOL_RANGE_JPEG;
  av_opt_set(context->priv_data, "preset", "fast", 0);
  if (avcodec_open2(context, codec, nullptr) < 0) {
    spdlog::error("error initializng encoder context");
    return;
  }
  frame = av_frame_alloc();
  if (frame == nullptr) {
    spdlog::error("error allocating frame");
    return;
  }
  frame->format = context->pix_fmt;
  frame->width = context->width;
  frame->height = context->height;
  if (av_frame_get_buffer(frame, 0) < 0) {
    spdlog::error("error allocating frame buffer");
    return;
  }
  if (av_frame_make_writable(frame) < 0) {
    spdlog::error("error making frame writable");
    return;
  }
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("error allocating packet");
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
      spdlog::error("error receiving packet");
      return;
    }
    const PacketRefGuard packetRef{packet};
    processor.ProcessEncodedData(packet);
  }
}

void Encoder::Encode(AVFrame* frame, EncodedDataProcessor& processor) {
  frame->pts = pts++;
  if (avcodec_send_frame(context, frame) < 0) {
    spdlog::error("error sending frame");
    return;
  }
  GetEncodedPacket(processor);
}

void Encoder::Flush(EncodedDataProcessor& processor) const {
  if (avcodec_send_frame(context, nullptr) < 0) {
    spdlog::error("error sending frame");
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

Writer::Writer(std::string_view filename, const WriterOptions& options_) : options{options_} {
  const std::string s{filename};
  if (avformat_alloc_output_context2(&formatContext, nullptr, nullptr, s.c_str()) < 0) {
    spdlog::error("error allocating output context");
    return;
  }
  stream = avformat_new_stream(formatContext, nullptr);
  if (stream == nullptr) {
    spdlog::error("error adding new stream");
    return;
  }
  const auto* codec = avcodec_find_encoder_by_name(options.codec.c_str());
  if (codec == nullptr) {
    spdlog::error("error finding encoder");
    return;
  }
  stream->codecpar->codec_id = codec->id;
  stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  stream->codecpar->bit_rate = options.bitrate;
  stream->codecpar->width = options.width;
  stream->codecpar->height = options.height;
  stream->time_base.num = 1;
  stream->time_base.den = options.framerate;
  if (avio_open(&formatContext->pb, s.c_str(), AVIO_FLAG_WRITE) < 0) {
    spdlog::error("error opening file");
    return;
  }
  if (avformat_write_header(formatContext, nullptr) < 0) {
    spdlog::error("error writing header");
    return;
  }
  packet = av_packet_alloc();
  if (packet == nullptr) {
    spdlog::error("error allocating packet");
    return;
  }
}

Writer::~Writer() {
  if (formatContext and formatContext->pb) {
    av_write_trailer(formatContext);
    avio_close(formatContext->pb);
  }
  av_packet_free(&packet);
  avformat_free_context(formatContext);
  formatContext = nullptr;
}

void Writer::Process(AVPacket* packet) {
  av_packet_rescale_ts(packet, {1, options.framerate}, stream->time_base);
  if (av_interleaved_write_frame(formatContext, packet) < 0) {
    spdlog::error("error writing frame");
    return;
  }
}

}  // namespace codec
