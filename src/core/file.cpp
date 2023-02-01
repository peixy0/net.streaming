#include "file.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

namespace os {

File::File(std::string_view filename) {
  const std::string s{filename};
  fd = open(s.c_str(), O_RDONLY);
  if (Ok()) {
    struct stat statbuf;
    fstat(fd, &statbuf);
    size = statbuf.st_size;
  }
}

File::~File() {
  if (Ok()) {
    close(fd);
  }
}

File::File(File&& f) {
  *this = std::move(f);
}

File& File::operator=(File&& f) {
  fd = f.fd;
  size = f.size;
  f.fd = -1;
  f.size = 0;
  return *this;
}

int File::Fd() const {
  return fd;
}

size_t File::Size() const {
  return size;
}

bool File::Ok() const {
  return fd >= 0;
}

}  // namespace os
