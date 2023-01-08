#include <gmock/gmock.h>
#include "http.hpp"
#include "network.hpp"

namespace network {

class HttpProcessorMock : public HttpProcessor {
public:
  MOCK_METHOD(HttpResponse, Process, (const HttpRequest&), (override));
};

class TcpSenderMock : public TcpSender {
public:
  MOCK_METHOD(void, Send, (std::string_view), (override));
  MOCK_METHOD(void, Send, (os::File), (override));
  MOCK_METHOD(void, Send, (std::unique_ptr<RawStream>), (override));
  MOCK_METHOD(void, SendBuffered, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, MarkPending, (), (override));
  MOCK_METHOD(void, UnmarkPending, (), (override));
};

}  // namespace network
