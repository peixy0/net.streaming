#include <gmock/gmock.h>
#include "http.hpp"
#include "network.hpp"

namespace network {

class HttpParserMock : public HttpParser {
public:
  MOCK_METHOD(std::optional<HttpRequest>, Parse, (std::string&), (override));
};

class HttpProcessorMock : public HttpProcessor {
public:
  MOCK_METHOD(HttpResponse, Process, (const HttpRequest&), (override));
};

class TcpSenderMock : public TcpSender {
public:
  MOCK_METHOD(void, Send, (std::string_view), (override));
  MOCK_METHOD(void, SendFile, (int, size_t), (override));
  MOCK_METHOD(void, Close, (), (override));
};

}  // namespace network
