#include <gmock/gmock.h>
#include "http.hpp"
#include "network.hpp"

namespace network {

class HttpProcessorMock : public HttpProcessor {
public:
  MOCK_METHOD(HttpResponse, Process, (const HttpRequest&), (override));
};

class NetworkSenderMock : public NetworkSender {
public:
  MOCK_METHOD(void, Send, (std::string_view), (override));
  MOCK_METHOD(void, Close, (), (override));
};

}  // namespace network
