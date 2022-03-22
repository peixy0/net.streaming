#include "app.hpp"

namespace application {
network::HttpResponse AppLayer::Process(const network::HttpRequest&) {
  return {"OK"};
}
}  // namespace application
