#include "stream.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppStreamTranscoder::AppStreamTranscoder(std::unique_ptr<codec::Decoder> decoder, std::unique_ptr<codec::Filter> filter,
    std::unique_ptr<codec::Encoder> encoder, std::unique_ptr<codec::Transcoder> transcoder,
    std::unique_ptr<codec::Writer> writer_)
    : decoder{std::move(decoder)},
      filter{std::move(filter)},
      encoder{std::move(encoder)},
      transcoder{std::move(transcoder)},
      writer{std::move(writer_)} {
  writer->Begin();
}

AppStreamTranscoder::~AppStreamTranscoder() {
  transcoder->Flush(*this);
  writer->End();
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

std::unique_ptr<AppStreamTranscoder> AppStreamTranscoderFactory::Create(codec::WriterProcessor& processor) const {
  auto decoder = std::make_unique<codec::Decoder>(decoderOptions);
  auto filter = std::make_unique<codec::Filter>(filterOptions);
  auto encoder = std::make_unique<codec::Encoder>(encoderOptions);
  auto transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  auto writer = std::make_unique<codec::BufferWriter>(writerOptions, processor);
  return std::make_unique<AppStreamTranscoder>(
      std::move(decoder), std::move(filter), std::move(encoder), std::move(transcoder), std::move(writer));
}

std::unique_ptr<AppStreamTranscoder> AppStreamTranscoderFactory::Create(std::string_view filename) const {
  auto decoder = std::make_unique<codec::Decoder>(decoderOptions);
  auto filter = std::make_unique<codec::Filter>(filterOptions);
  auto encoder = std::make_unique<codec::Encoder>(encoderOptions);
  auto transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  auto writer = std::make_unique<codec::FileWriter>(writerOptions, filename);
  return std::make_unique<AppStreamTranscoder>(
      std::move(decoder), std::move(filter), std::move(encoder), std::move(transcoder), std::move(writer));
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
  if (recorderOptions.saveRecord) {
    recorderStartTime = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.", std::localtime(&recorderStartTime));
    std::string f{buf};
    f += recorderOptions.format;
    transcoder = transcoderFactory.Create(f);
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
    const video::CapturerOptions& capturerOptions, AppStreamDistributer& streamDistributer)
    : capturerOptions{capturerOptions}, streamDistributer{streamDistributer} {
}

void AppStreamCapturerRunner::Run() {
  capturerThread = std::thread([this] {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(capturerOptions);
    while (true) {
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamCapturerRunner::ProcessFrame(std::string_view frame) {
  streamDistributer.Process({frame.data(), frame.size()});
}

}  // namespace application
