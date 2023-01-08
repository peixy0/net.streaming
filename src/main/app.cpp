#include "app.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"

namespace application {

AppLayer::AppLayer(std::string_view rootPath) {
  const std::string s{rootPath};
  char resolvedPath[PATH_MAX];
  realpath(s.c_str(), resolvedPath);
  chdir(resolvedPath);
  wwwRoot = resolvedPath;
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  std::string uri = req.uri;
  if (uri.ends_with("/")) {
    uri += "index.html";
  }
  if (uri.starts_with("/")) {
    uri.erase(0, 1);
  }
  char localPathStr[PATH_MAX];
  realpath(uri.c_str(), localPathStr);
  std::string localPath{localPathStr};
  spdlog::debug("request local path is {}", localPath);
  if (not localPath.starts_with(wwwRoot)) {
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::NotFound;
    resp.headers.emplace("Content-Type", "text/plain");
    resp.body = "Not Found";
    return resp;
  }
  std::string mimeType = MimeTypeOf(localPath);
  network::FileHttpResponse resp;
  resp.path = std::move(localPath);
  resp.headers.emplace("Content-type", std::move(mimeType));
  return resp;
}

std::string AppLayer::MimeTypeOf(std::string_view path) {
  if (path.ends_with(".html")) {
    return "text/html";
  }
  return "text/plain";
}

}  // namespace application
