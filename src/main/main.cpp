#include <spdlog/spdlog.h>
#include <cstring>
#include "app.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "http.hpp"
#include "stream.hpp"
#include "tcp.hpp"
#include "video.hpp"

int main(int argc, char* argv[]) {
  if (argc < 3) {
    return -1;
  }
  auto* host = argv[1];
  std::uint16_t port = std::atoi(argv[2]);
  spdlog::set_level(spdlog::level::info);
  codec::DisableCodecLogs();

  application::AppStreamProcessorOptions streamProcessorOptions;
  streamProcessorOptions.maxRecordingTimeInSeconds = 10 * 60;
  streamProcessorOptions.distributeMjpeg = true;
  streamProcessorOptions.saveRecord = false;

  video::StreamOptions streamOptions;
  streamOptions.format = video::StreamFormat::MJPEG;
  streamOptions.width = 1280;
  streamOptions.height = 720;
  streamOptions.framerate = 30;

  codec::DecoderOptions decoderOptions;
  decoderOptions.codec = "mjpeg_qsv";

  codec::FilterOptions filterOptions;
  filterOptions.width = streamOptions.width;
  filterOptions.height = streamOptions.height;
  filterOptions.framerate = streamOptions.framerate;
  filterOptions.inFormat = codec::PixelFormat::YUVJ422;
  filterOptions.outFormat = codec::PixelFormat::NV12;
  filterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions encoderOptions;
  encoderOptions.codec = "hevc_qsv";
  encoderOptions.width = filterOptions.width;
  encoderOptions.height = filterOptions.height;
  encoderOptions.framerate = filterOptions.framerate;
  encoderOptions.bitrate = 2000000;
  encoderOptions.format = filterOptions.outFormat;

  codec::WriterOptions writerOptions;
  writerOptions.codec = encoderOptions.codec;
  writerOptions.width = encoderOptions.width;
  writerOptions.height = encoderOptions.height;
  writerOptions.framerate = encoderOptions.framerate;
  writerOptions.bitrate = encoderOptions.bitrate;

  common::ConcreteEventQueue<application::StreamProcessorEvent> streamProcessorEventQueue;

  application::AppStreamDistributer mjpegDistributer;
  application::AppStreamSnapshotSaver snapshotSaver{mjpegDistributer};

  application::AppStreamProcessorRunner processorRunner{streamProcessorEventQueue, mjpegDistributer,
      streamProcessorOptions, decoderOptions, filterOptions, encoderOptions, writerOptions};
  processorRunner.Run();

  application::AppStreamCapturerRunner capturerRunner{streamOptions, streamProcessorEventQueue};
  capturerRunner.Run();

  application::AppStreamProcessorController streamProcessorController{streamProcessorEventQueue};
  application::AppLayerFactory appFactory{mjpegDistributer, snapshotSaver, streamProcessorController};

  network::HttpOptions httpOptions;
  httpOptions.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory httpLayerFactory{httpOptions, appFactory};

  network::TcpOptions tcpOptions;
  tcpOptions.maxBufferedSize = 60;
  network::Tcp4Layer tcp{host, port, tcpOptions, httpLayerFactory};
  tcp.Start();

  return 0;
}
