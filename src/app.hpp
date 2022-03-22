#pragma once

#include "network.hpp"

namespace application {
class AppLayer : public network::HttpProcessor {
public:
  virtual network::HttpResponse Process(const network::HttpRequest&) override;
};
}  // namespace application
