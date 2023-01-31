#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <thread>
#include "app.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "server.hpp"
#include "stream.hpp"
#include "video.hpp"

int main() {
  spdlog::set_level(spdlog::level::off);
  codec::DisableCodecLogs();

  YAML::Node config = YAML::LoadFile("config.yaml");
  auto serverAddr = config["server"]["address"].as<std::string>();
  auto serverPort = config["server"]["port"].as<std::uint16_t>();
  auto capturerCodec = config["capturer"]["codec"].as<std::string>();
  auto capturerPixfmt = config["capturer"]["pixfmt"].as<std::string>();
  auto capturerWidth = config["capturer"]["width"].as<int>();
  auto capturerHeight = config["capturer"]["height"].as<int>();
  auto capturerFramerate = config["capturer"]["framerate"].as<int>();
  auto recorderCodec = config["recorder"]["codec"].as<std::string>();
  auto recorderPixfmt = config["recorder"]["pixfmt"].as<std::string>();
  auto recorderFormat = config["recorder"]["format"].as<std::string>();
  auto recorderWidth = config["recorder"]["width"].as<int>();
  auto recorderHeight = config["recorder"]["height"].as<int>();
  auto recorderBitrate = config["recorder"]["bitrate"].as<int>();
  auto maxRecordingTimeInSeconds = config["recorder"]["maxRecordingTimeInSeconds"].as<int>();
  auto encoderCodec = config["encoder"]["codec"].as<std::string>();
  auto encoderPixfmt = config["encoder"]["pixfmt"].as<std::string>();
  auto encoderFormat = config["encoder"]["format"].as<std::string>();
  auto encoderWidth = config["encoder"]["width"].as<int>();
  auto encoderHeight = config["encoder"]["height"].as<int>();
  auto encoderBitrate = config["encoder"]["bitrate"].as<int>();

  application::AppStreamRecorderOptions streamRecorderOptions;
  streamRecorderOptions.format = recorderFormat;
  streamRecorderOptions.maxRecordingTimeInSeconds = maxRecordingTimeInSeconds;
  streamRecorderOptions.saveRecord = false;

  video::CapturerOptions capturerOptions;
  capturerOptions.width = capturerWidth;
  capturerOptions.height = capturerHeight;
  capturerOptions.framerate = capturerFramerate;

  codec::DecoderOptions decoderOptions;
  decoderOptions.codec = capturerCodec;

  codec::FilterOptions recorderFilterOptions;
  recorderFilterOptions.width = recorderWidth;
  recorderFilterOptions.height = recorderHeight;
  recorderFilterOptions.framerate = capturerOptions.framerate;
  recorderFilterOptions.inFormat = capturerPixfmt;
  recorderFilterOptions.outFormat = recorderPixfmt;
  recorderFilterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions recorderEncoderOptions;
  recorderEncoderOptions.codec = recorderCodec;
  recorderEncoderOptions.pixfmt = recorderFilterOptions.outFormat;
  recorderEncoderOptions.width = recorderFilterOptions.width;
  recorderEncoderOptions.height = recorderFilterOptions.height;
  recorderEncoderOptions.framerate = recorderFilterOptions.framerate;
  recorderEncoderOptions.bitrate = recorderBitrate;

  codec::WriterOptions recorderWriterOptions;
  recorderWriterOptions.format = recorderFormat;
  recorderWriterOptions.codec = recorderEncoderOptions.codec;
  recorderWriterOptions.width = recorderEncoderOptions.width;
  recorderWriterOptions.height = recorderEncoderOptions.height;
  recorderWriterOptions.framerate = recorderEncoderOptions.framerate;
  recorderWriterOptions.bitrate = recorderEncoderOptions.bitrate;

  codec::FilterOptions encodedStreamFilterOptions;
  encodedStreamFilterOptions.width = encoderWidth;
  encodedStreamFilterOptions.height = encoderHeight;
  encodedStreamFilterOptions.framerate = capturerOptions.framerate;
  encodedStreamFilterOptions.inFormat = capturerPixfmt;
  encodedStreamFilterOptions.outFormat = encoderPixfmt;
  encodedStreamFilterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions encodedStreamEncoderOptions;
  encodedStreamEncoderOptions.codec = encoderCodec;
  encodedStreamEncoderOptions.pixfmt = encodedStreamFilterOptions.outFormat;
  encodedStreamEncoderOptions.width = encodedStreamFilterOptions.width;
  encodedStreamEncoderOptions.height = encodedStreamFilterOptions.height;
  encodedStreamEncoderOptions.framerate = encodedStreamFilterOptions.framerate;
  encodedStreamEncoderOptions.bitrate = encoderBitrate;

  codec::WriterOptions encodedStreamWriterOptions;
  encodedStreamWriterOptions.format = encoderFormat;
  encodedStreamWriterOptions.codec = encodedStreamEncoderOptions.codec;
  encodedStreamWriterOptions.width = encodedStreamEncoderOptions.width;
  encodedStreamWriterOptions.height = encodedStreamEncoderOptions.height;
  encodedStreamWriterOptions.framerate = encodedStreamEncoderOptions.framerate;
  encodedStreamWriterOptions.bitrate = encodedStreamEncoderOptions.bitrate;

  application::AppStreamDistributer mjpegDistributer;
  application::AppStreamCapturerRunner capturerRunner{capturerOptions, mjpegDistributer};
  capturerRunner.Run();

  common::ConcreteEventQueue<application::AppRecorderEvent> recorderEventQueue;
  application::AppStreamTranscoderFactory recorderTranscoderFactory{
      decoderOptions, recorderFilterOptions, recorderEncoderOptions, recorderWriterOptions};
  application::AppStreamRecorderRunner recorderRunner{
      recorderEventQueue, streamRecorderOptions, recorderTranscoderFactory};
  recorderRunner.Run();

  application::AppStreamSnapshotSaver snapshotSaver{mjpegDistributer};
  application::AppStreamRecorderController recorderController{mjpegDistributer, recorderEventQueue};
  application::AppStreamTranscoderFactory encodedStreamTranscoderFactory{
      decoderOptions, encodedStreamFilterOptions, encodedStreamEncoderOptions, encodedStreamWriterOptions};

  application::AppHttpLayer appHttpLayer{snapshotSaver, recorderController};

  std::vector<std::thread> workers;
  const int nWorkers = std::thread::hardware_concurrency() + 1;
  for (int i = 0; i < nWorkers; i++) {
    workers.emplace_back(
        [&serverAddr, serverPort, &appHttpLayer, &mjpegDistributer, &encodedStreamTranscoderFactory]() {
          network::Server server;
          server.Add(
              network::HttpMethod::GET, "/", [&appHttpLayer](network::HttpRequest&& req, network::HttpSender& sender) {
                appHttpLayer.GetIndex(std::move(req), sender);
              });
          server.Add(network::HttpMethod::GET, "/snapshot",
              [&appHttpLayer](network::HttpRequest&& req, network::HttpSender& sender) {
                appHttpLayer.GetSnapshot(std::move(req), sender);
              });
          server.Add(network::HttpMethod::GET, "/recording",
              [&appHttpLayer](network::HttpRequest&& req, network::HttpSender& sender) {
                appHttpLayer.GetRecording(std::move(req), sender);
              });
          server.Add(network::HttpMethod::POST, "/recording",
              [&appHttpLayer](network::HttpRequest&& req, network::HttpSender& sender) {
                appHttpLayer.SetRecording(std::move(req), sender);
              });

          auto mjpegSenderFactory = std::make_unique<application::AppMjpegSenderFactory>(mjpegDistributer, 1);
          server.Add(network::HttpMethod::GET, "/mjpeg", std::move(mjpegSenderFactory));
          auto mjpeg2SenderFactory = std::make_unique<application::AppMjpegSenderFactory>(mjpegDistributer);
          server.Add(network::HttpMethod::GET, "/mjpeg2", std::move(mjpeg2SenderFactory));
          auto encodedStreamSenderFactory = std::make_unique<application::AppEncodedStreamSenderFactory>(
              mjpegDistributer, encodedStreamTranscoderFactory);
          server.Add(network::HttpMethod::GET, "/stream", std::move(encodedStreamSenderFactory));

          server.Start(serverAddr, serverPort);
        });
  }
  for (auto& w : workers) {
    w.join();
  }

  return 0;
}
