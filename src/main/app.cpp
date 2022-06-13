#include "app.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"

namespace application {

AppLayer::AppLayer(std::string_view rootPath) {
  char resolvedPath[PATH_MAX];
  realpath(rootPath.data(), resolvedPath);
  chdir(resolvedPath);
  wwwRoot = resolvedPath;
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  std::string uri = req.uri;
  if (uri.ends_with("/")) {
    uri.append("index.html");
  }
  if (uri.starts_with("/")) {
    uri.erase(0, 1);
  }
  char localPathStr[PATH_MAX];
  realpath(uri.data(), localPathStr);
  std::string localPath{localPathStr};
  spdlog::debug("request local path is {}", localPath);
  if (not localPath.starts_with(wwwRoot)) {
    return network::PlainTextHttpResponse{network::HttpStatus::NotFound, ""};
  }
  std::string mimeType = MimeTypeOf(localPath);
  return network::FileHttpResponse{std::move(localPath), std::move(mimeType)};
}

std::string AppLayer::MimeTypeOf(std::string_view path) {
  if (path.ends_with(".html")) {
    return "text/html";
  }
  return "text/plain";
}

}  // namespace application
