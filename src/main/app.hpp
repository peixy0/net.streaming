#pragma once
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include "codec.hpp"
#include "network.hpp"
#include "video.hpp"

namespace application {

class AppStreamRecorder : public codec::EncodedDataProcessor {
public:
  explicit AppStreamRecorder(codec::Transcoder&);
  ~AppStreamRecorder();
  void ProcessBuffer(std::string_view);
  void ProcessEncodedData(std::string_view) override;

private:
  FILE* fp;
  codec::Transcoder& transcoder;
};

class AppLiveStreamOverseer;

class AppStreamSubscriber : public network::RawStream {
public:
  AppStreamSubscriber(AppLiveStreamOverseer&, network::SenderNotifier&);
  ~AppStreamSubscriber() override;
  std::optional<std::string> GetBuffered() override;
  void ProcessFrame(std::string_view);

private:
  AppLiveStreamOverseer& overseer;
  network::SenderNotifier& notifier;
  std::deque<std::string> streamBuffer;
  std::mutex bufferMut;
};

class AppStreamSubscriberFactory : public network::RawStreamFactory {
public:
  explicit AppStreamSubscriberFactory(AppLiveStreamOverseer&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&) override;

private:
  AppLiveStreamOverseer& overseer;
};

class AppLiveStreamOverseer {
public:
  void ProcessBuffer(std::string_view buffer);
  void AddSubscriber(AppStreamSubscriber*);
  void RemoveSubscriber(AppStreamSubscriber*);
  std::string GetSnapshot() const;

private:
  void NotifySubscribers(std::string_view) const;
  void SaveSnapshot(std::string_view);

  std::set<AppStreamSubscriber*> subscribers;
  std::mutex mutable subscribersMut;

  std::string snapshot;
  std::mutex mutable snapshotMut;
};

class AppStreamProcessor : public video::StreamProcessor {
public:
  AppStreamProcessor(video::StreamOptions&&, AppLiveStreamOverseer&, codec::DecoderOptions&&, codec::FilterOptions&&,
                     codec::EncoderOptions&&);
  void ProcessFrame(std::string_view) override;
  void StartLiveStream();
  void StopLiveStream();
  void StartRecording();
  void StopRecording();

private:
  void StartStreaming(video::StreamOptions&&);

  std::thread streamThread;
  std::atomic<bool> streamRunning;

  AppLiveStreamOverseer& liveStreamOverseer;
  std::atomic<bool> liveStreamRunning;

  codec::DecoderOptions decoderOptions;
  std::unique_ptr<codec::Decoder> decoder;
  codec::FilterOptions filterOptions;
  std::unique_ptr<codec::Filter> filter;
  codec::EncoderOptions encoderOptions;
  std::unique_ptr<codec::Encoder> encoder;
  std::unique_ptr<codec::Transcoder> transcoder;
  std::unique_ptr<AppStreamRecorder> recorder;
  std::mutex recorderMut;
};

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(AppStreamProcessor&, AppLiveStreamOverseer&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppStreamProcessor& streamProcessor;
  AppLiveStreamOverseer& liveStreamOverseer;
};

}  // namespace application
