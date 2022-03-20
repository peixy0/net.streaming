#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "http.hpp"

using namespace testing;

namespace network {

class NetworkSenderMock : public NetworkSender {
public:
  MOCK_METHOD(void, Send, (std::string_view), (override));
  MOCK_METHOD(void, Close, (), (override));
};

}  // namespace network

namespace protocol {

class HttpLayerTestFixture : public Test {
public:
  HttpLayerTestFixture() : sut(std::make_unique<HttpLayer>(networkSenderMock)) {
  }

protected:
  StrictMock<network::NetworkSenderMock> networkSenderMock;
  std::unique_ptr<HttpLayer> sut;
};

TEST_F(HttpLayerTestFixture, whenReceivedPayloadExceedsLimit_itWillCloseConnection) {
  EXPECT_CALL(networkSenderMock, Close());
  std::string request(1 << 22, '.');
  sut->Receive(request);
}

TEST_F(HttpLayerTestFixture, whenReceivedValidHttpRequest_itWillRespondOk) {
  std::string response;
  EXPECT_CALL(networkSenderMock, Send(_)).WillOnce(SaveArg<0>(&response));
  EXPECT_CALL(networkSenderMock, Close());
  std::string requestPart1("GET / ");
  std::string requestPart2("HTTP/1.1\r\n\r\n");
  sut->Receive(requestPart1);
  sut->Receive(requestPart2);
  EXPECT_TRUE(response.starts_with("HTTP/1.1 200 OK"));
}

}  // namespace protocol
