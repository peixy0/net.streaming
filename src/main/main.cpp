#include <spdlog/spdlog.h>
#include <cstring>
#include "app.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "http.hpp"
#include "recorder.hpp"
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

  common::EventQueueFactory eventQueueFactory;
  auto recorderEventQueue = eventQueueFactory.Create<application::RecorderEvent>();

  codec::DecoderOptions decoderOptions;
  decoderOptions.codec = "mjpeg";

  codec::FilterOptions filterOptions;
  filterOptions.width = 1280;
  filterOptions.height = 720;
  filterOptions.framerate = 30;
  filterOptions.inFormat = codec::PixelFormat::YUVJ422;
  filterOptions.outFormat = codec::PixelFormat::NV12;
  filterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions encoderOptions;
  encoderOptions.codec = "libx264";
  encoderOptions.width = 1280;
  encoderOptions.height = 720;
  encoderOptions.framerate = 30;
  encoderOptions.bitrate = 2000000;
  encoderOptions.format = codec::PixelFormat::NV12;

  codec::WriterOptions writerOptions;
  writerOptions.codec = encoderOptions.codec;
  writerOptions.width = encoderOptions.width;
  writerOptions.height = encoderOptions.height;
  writerOptions.framerate = encoderOptions.framerate;
  writerOptions.bitrate = encoderOptions.bitrate;

  video::StreamOptions streamOptions;
  streamOptions.format = video::StreamFormat::MJPEG;
  streamOptions.width = 1280;
  streamOptions.height = 720;
  streamOptions.framerate = 30;

  application::AppStreamRecorderRunner recorderRunner{decoderOptions, filterOptions, encoderOptions, writerOptions,
                                                      *recorderEventQueue};
  recorderRunner.Run();

  application::AppStreamDistributer streamDistributer;
  application::AppStreamCapturerRunner capturerRunner{streamOptions, streamDistributer, *recorderEventQueue};
  capturerRunner.Run();

  application::AppLayer app{streamDistributer, *recorderEventQueue};
  network::HttpOptions httpOptions;
  httpOptions.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory factory{httpOptions, app};
  network::Tcp4Layer tcp{host, port, factory};
  tcp.Start();
  return 0;
}
