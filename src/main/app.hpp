#pragma once
#include "network.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  explicit AppLayer(std::string_view);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  std::string MimeTypeOf(std::string_view path);
  std::string wwwRoot;
};

}  // namespace application
