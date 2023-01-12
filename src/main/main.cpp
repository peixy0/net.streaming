#include <spdlog/spdlog.h>
#include <cstring>
#include "app.hpp"
#include "codec.hpp"
#include "http.hpp"
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

  video::StreamOptions streamOptions;
  streamOptions.format = video::StreamFormat::MJPEG;
  streamOptions.width = 1280;
  streamOptions.height = 720;
  streamOptions.framerate = 30;

  application::AppLiveStreamOverseer liveStreamOverseer;

  codec::DecoderOptions decoderOptions;
  decoderOptions.codec = "mjpeg";

  codec::FilterOptions filterOptions;
  filterOptions.width = 1280;
  filterOptions.height = 720;
  filterOptions.framerate = 30;
  filterOptions.inFormat = codec::PixelFormat::YUVJ422;
  filterOptions.outFormat = codec::PixelFormat::YUV420;
  filterOptions.description =
      "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
      ":text='%{localtime}':fontcolor=yellow:x=10:y=10";

  codec::EncoderOptions encoderOptions;
  encoderOptions.codec = "libx264";
  encoderOptions.width = 1280;
  encoderOptions.height = 720;
  encoderOptions.framerate = 30;
  encoderOptions.bitrate = 2000000;

  codec::WriterOptions writerOptions;
  writerOptions.codec = encoderOptions.codec;
  writerOptions.width = encoderOptions.width;
  writerOptions.height = encoderOptions.height;
  writerOptions.framerate = encoderOptions.framerate;
  writerOptions.bitrate = encoderOptions.bitrate;

  application::AppStreamProcessor streamProcessor{std::move(streamOptions),  liveStreamOverseer,
                                                  std::move(decoderOptions), std::move(filterOptions),
                                                  std::move(encoderOptions), std::move(writerOptions)};
  application::AppLayer app{streamProcessor, liveStreamOverseer};

  network::HttpOptions httpOptions;
  httpOptions.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory factory{httpOptions, app};
  network::Tcp4Layer tcp{host, port, factory};
  tcp.Start();
  return 0;
}
