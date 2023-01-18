#include "stream.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppStreamRecordWriter::AppStreamRecordWriter(std::string_view filename_) : filename{filename_} {
  fp = fopen(filename.c_str(), "wb");
}

AppStreamRecordWriter::~AppStreamRecordWriter() {
  fclose(fp);
}

void AppStreamRecordWriter::WriteData(std::string_view buffer) {
  fwrite(buffer.data(), 1, buffer.size(), fp);
}

AppStreamTranscoder::AppStreamTranscoder(const codec::DecoderOptions& decoderOptions,
    const codec::FilterOptions& filterOptions, const codec::EncoderOptions& encoderOptions,
    const codec::WriterOptions& writerOptions, codec::WriterProcessor& processor) {
  decoder = std::make_unique<codec::Decoder>(decoderOptions);
  filter = std::make_unique<codec::Filter>(filterOptions);
  encoder = std::make_unique<codec::Encoder>(encoderOptions);
  transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  writer = std::make_unique<codec::Writer>(writerOptions, processor);
}

AppStreamTranscoder::~AppStreamTranscoder() {
  if (transcoder) {
    transcoder->Flush(*this);
  }
  transcoder.reset();
  writer.reset();
  decoder.reset();
  filter.reset();
  encoder.reset();
}

void AppStreamTranscoder::Process(std::string_view buffer) {
  transcoder->Process(buffer, *this);
}

void AppStreamTranscoder::ProcessEncodedData(AVPacket* encoded) {
  writer->Process(encoded);
}

AppStreamTranscoderFactory::AppStreamTranscoderFactory(const codec::DecoderOptions& decoderOptions,
    const codec::FilterOptions& filterOptions, const codec::EncoderOptions& encoderOptions,
    const codec::WriterOptions& writerOptions)
    : decoderOptions{decoderOptions},
      filterOptions{filterOptions},
      encoderOptions{encoderOptions},
      writerOptions{writerOptions} {
}

std::unique_ptr<AppStreamTranscoder> AppStreamTranscoderFactory::Create(codec::WriterProcessor& processor) {
  return std::make_unique<AppStreamTranscoder>(decoderOptions, filterOptions, encoderOptions, writerOptions, processor);
}

AppStreamRecorderRunner::AppStreamRecorderRunner(common::EventQueue<AppRecorderEvent>& eventQueue,
    const AppStreamRecorderOptions& recorderOptions, AppStreamTranscoderFactory& transcoderFactory)
    : eventQueue{eventQueue}, recorderOptions{recorderOptions}, transcoderFactory{transcoderFactory} {
  Reset();
}

void AppStreamRecorderRunner::Run() {
  processorThread = std::thread([this] {
    while (true) {
      auto event = eventQueue.Pop();
      std::visit(*this, event);
    }
  });
}

void AppStreamRecorderRunner::Process(std::string_view buffer) {
  if (not recorderOptions.saveRecord) {
    return;
  }
  if (recorderOptions.maxRecordingTimeInSeconds > 0) {
    const auto now = std::time(nullptr);
    const auto diff = std::difftime(now, recorderStartTime);
    if (diff >= recorderOptions.maxRecordingTimeInSeconds) {
      Reset();
    }
  }
  transcoder->Process(buffer);
}

void AppStreamRecorderRunner::Reset() {
  transcoder.reset();
  recordWriter.reset();
  if (recorderOptions.saveRecord) {
    recorderStartTime = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.ts", std::localtime(&recorderStartTime));
    recordWriter = std::make_unique<AppStreamRecordWriter>(buf);
    transcoder = transcoderFactory.Create(*recordWriter);
  }
}

void AppStreamRecorderRunner::operator()(const StartRecording&) {
  if (not recorderOptions.saveRecord) {
    recorderOptions.saveRecord = true;
    Reset();
  }
}

void AppStreamRecorderRunner::operator()(const StopRecording&) {
  recorderOptions.saveRecord = false;
  Reset();
}

void AppStreamRecorderRunner::operator()(const RecordData& data) {
  Process(data.buffer);
}

void AppStreamDistributer::Process(std::string_view buffer) {
  std::lock_guard lock{receiversMut};
  for (auto* s : receivers) {
    s->Notify(buffer);
  }
}

void AppStreamDistributer::AddSubscriber(AppStreamReceiver* subscriber) {
  std::lock_guard lock{receiversMut};
  receivers.emplace(subscriber);
}

void AppStreamDistributer::RemoveSubscriber(AppStreamReceiver* subscriber) {
  std::lock_guard lock{receiversMut};
  receivers.erase(subscriber);
}

AppStreamCapturerRunner::AppStreamCapturerRunner(
    const video::StreamOptions& streamOptions, AppStreamDistributer& streamDistributer)
    : streamOptions{streamOptions}, streamDistributer{streamDistributer} {
}

void AppStreamCapturerRunner::Run() {
  streamThread = std::thread([this] {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(streamOptions);
    while (true) {
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamCapturerRunner::ProcessFrame(std::string_view frame) {
  streamDistributer.Process({frame.data(), frame.size()});
}

}  // namespace application
