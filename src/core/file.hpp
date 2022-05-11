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
  int Size() const;
  bool Ok() const;

private:
  int fd;
  int size;
};

}  // namespace os
