#pragma once
#include <string_view>

namespace os {

class File {
public:
  explicit File(std::string_view);
  ~File();
  File(const File&) = delete;
  File(File&&);
  File& operator=(const File&) = delete;
  File& operator=(File&&);

  int Fd() const;
  size_t Size() const;
  bool Ok() const;

private:
  int fd;
  size_t size;
};

}  // namespace os
