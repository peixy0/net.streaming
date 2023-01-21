#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <thread>
#include "app.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "http.hpp"
#include "stream.hpp"
#include "tcp.hpp"
#include "video.hpp"

int main() {
  spdlog::set_level(spdlog::level::off);
  codec::DisableCodecLogs();

  YAML::Node config = YAML::LoadFile("config.yaml");
  auto serverAddr = config["server"]["address"].as<std::string>();
  auto serverPort = config["server"]["port"].as<std::uint16_t>();
  auto streamCodec = config["stream"]["codec"].as<std::string>();
  auto streamPixfmt = config["stream"]["pixfmt"].as<std::string>();
  auto streamWidth = config["stream"]["width"].as<int>();
  auto streamHeight = config["stream"]["height"].as<int>();
  auto streamFramerate = config["stream"]["framerate"].as<int>();
  auto recorderCodec = config["recorder"]["codec"].as<std::string>();
  auto recorderPixfmt = config["recorder"]["pixfmt"].as<std::string>();
  auto recorderFormat = config["recorder"]["format"].as<std::string>();
  auto recorderWidth = config["recorder"]["width"].as<int>();
  auto recorderHeight = config["recorder"]["height"].as<int>();
  auto recorderBitrate = config["recorder"]["bitrate"].as<int>();
  auto maxRecordingTimeInSeconds = config["recorder"]["maxRecordingTimeInSeconds"].as<int>();
  auto encodedStreamCodec = config["encodedstream"]["codec"].as<std::string>();
  auto encodedStreamPixfmt = config["encodedstream"]["pixfmt"].as<std::string>();
  auto encodedStreamFormat = config["encodedstream"]["format"].as<std::string>();
  auto encodedStreamWidth = config["encodedstream"]["width"].as<int>();
  auto encodedStreamHeight = config["encodedstream"]["height"].as<int>();
  auto encodedStreamBitrate = config["encodedstream"]["bitrate"].as<int>();

  application::AppStreamRecorderOptions streamRecorderOptions;
  streamRecorderOptions.maxRecordingTimeInSeconds = maxRecordingTimeInSeconds;
  streamRecorderOptions.saveRecord = false;

  video::StreamOptions streamOptions;
  streamOptions.width = streamWidth;
  streamOptions.height = streamHeight;
  streamOptions.framerate = streamFramerate;

  codec::DecoderOptions decoderOptions;
  decoderOptions.codec = streamCodec;

  codec::FilterOptions recorderFilterOptions;
  recorderFilterOptions.width = recorderWidth;
  recorderFilterOptions.height = recorderHeight;
  recorderFilterOptions.framerate = streamOptions.framerate;
  recorderFilterOptions.inFormat = streamPixfmt;
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
  encodedStreamFilterOptions.width = encodedStreamWidth;
  encodedStreamFilterOptions.height = encodedStreamHeight;
  encodedStreamFilterOptions.framerate = streamOptions.framerate;
  encodedStreamFilterOptions.inFormat = streamPixfmt;
  encodedStreamFilterOptions.outFormat = encodedStreamPixfmt;
  encodedStreamFilterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions encodedStreamEncoderOptions;
  encodedStreamEncoderOptions.codec = encodedStreamCodec;
  encodedStreamEncoderOptions.pixfmt = encodedStreamFilterOptions.outFormat;
  encodedStreamEncoderOptions.width = encodedStreamFilterOptions.width;
  encodedStreamEncoderOptions.height = encodedStreamFilterOptions.height;
  encodedStreamEncoderOptions.framerate = encodedStreamFilterOptions.framerate;
  encodedStreamEncoderOptions.bitrate = encodedStreamBitrate;

  codec::WriterOptions encodedStreamWriterOptions;
  encodedStreamWriterOptions.format = encodedStreamFormat;
  encodedStreamWriterOptions.codec = encodedStreamEncoderOptions.codec;
  encodedStreamWriterOptions.width = encodedStreamEncoderOptions.width;
  encodedStreamWriterOptions.height = encodedStreamEncoderOptions.height;
  encodedStreamWriterOptions.framerate = encodedStreamEncoderOptions.framerate;
  encodedStreamWriterOptions.bitrate = encodedStreamEncoderOptions.bitrate;

  application::AppStreamDistributer mjpegDistributer;
  application::AppStreamCapturerRunner capturerRunner{streamOptions, mjpegDistributer};
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
  application::AppLayerFactory appFactory{
      mjpegDistributer, snapshotSaver, recorderController, encodedStreamTranscoderFactory};

  network::HttpOptions httpOptions;
  httpOptions.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory httpLayerFactory{httpOptions, appFactory};

  network::TcpOptions tcpOptions;
  tcpOptions.maxBufferedSize = 0;
  std::vector<std::thread> workers;
  const int nWorkers = std::thread::hardware_concurrency() + 1;
  for (int i = 0; i < nWorkers; i++) {
    workers.emplace_back([&serverAddr, &serverPort, &tcpOptions, &httpLayerFactory]() {
      network::Tcp4Layer tcp{serverAddr, serverPort, tcpOptions, httpLayerFactory};
      tcp.Start();
    });
  }
  for (int i = 0; i < nWorkers; i++) {
    workers[i].join();
  }

  return 0;
}
