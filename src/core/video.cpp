#include "video.hpp"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string>

namespace {

template <typename... T>
int xioctl(int fd, unsigned long req, T... args) {
  int r;
  do {
    r = ioctl(fd, req, args...);
  } while (r == -1 and EINTR == errno);
  return r;
}

}  // namespace

namespace video {

DeviceBuffer::DeviceBuffer(int fd, off_t offset, size_t length) : length{length} {
  ptr = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
  if (reinterpret_cast<long>(ptr) < 0) {
    spdlog::error("video mmap(): {}", strerror(errno));
    return;
  }
  spdlog::debug("video buffer mapped");
}

DeviceBuffer::DeviceBuffer(DeviceBuffer&& buffer) {
  ptr = buffer.ptr;
  buffer.ptr = reinterpret_cast<void*>(-1);
  buffer.length = 0;
}

DeviceBuffer::~DeviceBuffer() {
  if (reinterpret_cast<long>(ptr) < 0) {
    return;
  }
  munmap(ptr, length);
  spdlog::debug("video buffer unmapped");
}

Stream::Stream(int fd, const CapturerOptions& options) : fd{fd} {
  SetParameters(options);
  BindBuffers();
  StartStreaming();
  spdlog::debug("streaming started");
}

Stream::~Stream() {
  StopStreaming();
  spdlog::debug("streaming stopped");
}

void Stream::ProcessFrame(StreamProcessor& processor) const {
  while (true) {
    constexpr int maxEvents = 10;
    epoll_event events[maxEvents];
    int n = epoll_wait(epfd, events, maxEvents, -1);
    if (n == -1) {
      spdlog::error("video epoll_wait(): {}", strerror(errno));
      return;
    }
    for (int i = 0; i < n; i++) {
      if (events[i].data.fd != fd) {
        continue;
      }
      v4l2_buffer buf;
      memset(&buf, 0, sizeof buf);
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        if (EAGAIN == errno) {
          continue;
        }
        spdlog::error("video ioctl(VIDIOC_DQBUF): {}", strerror(errno));
        return;
      }
      const char* p = static_cast<const char*>(buffers[buf.index].Get());
      processor.ProcessFrame({p, p + buf.bytesused});
      if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        spdlog::error("video ioctl(VIDIOC_QBUF): {}", strerror(errno));
        return;
      }
      return;
    }
  }
}

void Stream::SetParameters(const CapturerOptions& options) const {
  v4l2_format fmt;
  memset(&fmt, 0, sizeof fmt);
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = options.width;
  fmt.fmt.pix.height = options.height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
    spdlog::error("video ioctl(VIDIOC_S_FMT): {}", strerror(errno));
  }
  v4l2_streamparm parm;
  memset(&parm, 0, sizeof parm);
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  parm.parm.capture.timeperframe.numerator = 1;
  parm.parm.capture.timeperframe.denominator = options.framerate;
  if (xioctl(fd, VIDIOC_S_PARM, &parm) == -1) {
    spdlog::error("video ioctl(VIDIOC_S_PARM): {}", strerror(errno));
  }
  v4l2_control ctrl;
  memset(&ctrl, 0, sizeof ctrl);
  ctrl.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
  ctrl.value = 0;
  if (xioctl(fd, VIDIOC_S_CTRL, &ctrl) == -1) {
    spdlog::error("video ioctl(VIDIOC_S_CTRL): {}", strerror(errno));
  }
}

void Stream::BindBuffers() {
  v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof reqbufs);
  reqbufs.count = 4;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd, VIDIOC_REQBUFS, &reqbufs) == -1) {
    spdlog::error("video ioctl(VIDIOC_REQBUFS): {}", strerror(errno));
    return;
  }
  if (reqbufs.count < 2) {
    spdlog::error("video REQBUFS.count({}) < 2", reqbufs.count);
    return;
  }
  buffers.reserve(reqbufs.count);
  for (std::uint32_t n = 0; n < reqbufs.count; n++) {
    v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n;
    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
      spdlog::error("video ioctl(VIDIOC_QUERYBUF): {}", strerror(errno));
      return;
    }
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      spdlog::error("video ioctl(VIDIOC_QBUF): {}", strerror(errno));
      return;
    }
    buffers.emplace_back(fd, buf.m.offset, buf.length);
  }
}

void Stream::StartStreaming() {
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
    spdlog::error("video ioctl(VIDIOC_STREAMON): {}", strerror(errno));
    return;
  }
  epfd = epoll_create1(0);
  if (epfd == -1) {
    spdlog::error("video epoll_create1(): {}", strerror(errno));
    return;
  }
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    spdlog::error("video epoll_ctl(): {}", strerror(errno));
    return;
  }
}

void Stream::StopStreaming() const {
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev) == -1) {
    spdlog::error("video epoll_ctl(): {}", strerror(errno));
  }
  close(epfd);
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
    spdlog::error("video ioctl(VIDIOC_STREAMOFF): {}", strerror(errno));
  }
}

Device::Device(std::string_view deviceName) {
  const std::string dn{deviceName};
  fd = open(dn.c_str(), O_RDWR | O_NONBLOCK, 0);
  if (fd < 0) {
    spdlog::error("video open(): {}", strerror(errno));
  }
}

Stream Device::GetStream(const CapturerOptions& options) const {
  return Stream{fd, options};
}

Device::~Device() {
  if (fd < 0) {
    return;
  }
  close(fd);
}

}  // namespace video
