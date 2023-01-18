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

  application::AppStreamRecorderOptions streamRecorderOptions;
  streamRecorderOptions.maxRecordingTimeInSeconds = 10 * 60;
  streamRecorderOptions.saveRecord = false;

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
  filterOptions.inFormat = codec::PixelFormat::NV12;
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

  codec::WriterOptions recorderWriterOptions;
  recorderWriterOptions.format = "mp4";
  recorderWriterOptions.codec = encoderOptions.codec;
  recorderWriterOptions.width = encoderOptions.width;
  recorderWriterOptions.height = encoderOptions.height;
  recorderWriterOptions.framerate = encoderOptions.framerate;
  recorderWriterOptions.bitrate = encoderOptions.bitrate;

  codec::WriterOptions liveWriterOptions;
  liveWriterOptions.format = "mpegts";
  liveWriterOptions.codec = encoderOptions.codec;
  liveWriterOptions.width = encoderOptions.width;
  liveWriterOptions.height = encoderOptions.height;
  liveWriterOptions.framerate = encoderOptions.framerate;
  liveWriterOptions.bitrate = encoderOptions.bitrate;

  application::AppStreamDistributer mjpegDistributer;
  application::AppStreamCapturerRunner capturerRunner{streamOptions, mjpegDistributer};
  capturerRunner.Run();

  common::ConcreteEventQueue<application::AppRecorderEvent> streamProcessorEventQueue;
  application::AppStreamTranscoderFactory recorderTranscoderFactory{
      decoderOptions, filterOptions, encoderOptions, recorderWriterOptions};
  application::AppStreamRecorderRunner recorderRunner{
      streamProcessorEventQueue, streamRecorderOptions, recorderTranscoderFactory};
  recorderRunner.Run();

  application::AppStreamSnapshotSaver snapshotSaver{mjpegDistributer};
  application::AppStreamRecorderController streamRecorderController{mjpegDistributer, streamProcessorEventQueue};
  application::AppStreamTranscoderFactory liveTranscoderFactory{
      decoderOptions, filterOptions, encoderOptions, liveWriterOptions};
  application::AppLayerFactory appFactory{
      mjpegDistributer, snapshotSaver, streamRecorderController, liveTranscoderFactory};

  network::HttpOptions httpOptions;
  httpOptions.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory httpLayerFactory{httpOptions, appFactory};

  network::TcpOptions tcpOptions;
  tcpOptions.maxBufferedSize = 0;
  network::Tcp4Layer tcp{host, port, tcpOptions, httpLayerFactory};
  tcp.Start();

  return 0;
}
