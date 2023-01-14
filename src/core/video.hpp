#pragma once

#include <unistd.h>
#include <string_view>
#include <vector>

namespace video {

class DeviceBuffer {
public:
  DeviceBuffer(int fd, off_t offset, size_t length);
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer(DeviceBuffer&& buffer);
  ~DeviceBuffer();

  const void* Get() const {
    return ptr;
  }

private:
  void* ptr;
  size_t length;
};

class StreamProcessor {
public:
  virtual void ProcessFrame(std::string_view) = 0;
  virtual ~StreamProcessor() = default;
};

enum class StreamFormat {
  MJPEG,
  YUV422,
};

struct StreamOptions {
  StreamFormat format;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t framerate;
};

class Stream {
public:
  Stream(int fd, const StreamOptions&);
  Stream(const Stream&) = delete;
  ~Stream();
  void ProcessFrame(StreamProcessor&) const;

private:
  void SetParameters(const StreamOptions&) const;
  void BindBuffers();
  void StartStreaming();
  void StopStreaming() const;

  int fd;
  int epfd;
  std::vector<DeviceBuffer> buffers;
};

class Device {
public:
  explicit Device(std::string_view deviceName);
  Device(const Device&) = delete;
  ~Device();
  Stream GetStream(const StreamOptions&) const;

private:
  int fd;
};

}  // namespace video
