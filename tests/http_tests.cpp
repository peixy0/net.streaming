#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "http.hpp"
#include "network.hpp"

using namespace testing;

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

namespace network {

class HttpLayerTestFixture : public Test {
public:
  HttpLayerTestFixture() {
    sut = std::make_unique<HttpLayer>(processor, senderMock);
  }

protected:
  StrictMock<network::HttpProcessorMock> processor;
  StrictMock<network::NetworkSenderMock> senderMock;
  std::unique_ptr<HttpLayer> sut;
};

TEST_F(HttpLayerTestFixture, whenReceivedValidHttpRequest_itWillRespondOk) {
  std::string response;
  EXPECT_CALL(processor, Process(_));
  EXPECT_CALL(senderMock, Send(_)).WillOnce(SaveArg<0>(&response));
  EXPECT_CALL(senderMock, Close());
  std::string request("GET / HTTP/1.1\r\n\r\n");
  sut->Receive(request);
  EXPECT_TRUE(response.starts_with("HTTP/1.1 200 OK"));
}

}  // namespace network
