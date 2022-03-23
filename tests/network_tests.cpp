#include <gtest/gtest.h>
#include "network_mocks.hpp"

using namespace testing;

namespace network {

TEST(HttpLayerTestSuite, whenReceivedValidHttpRequest_itShouldRespondOk) {
  std::string response;
  StrictMock<HttpProcessorMock> processor;
  auto senderMock = std::make_unique<StrictMock<network::NetworkSenderMock>>();
  EXPECT_CALL(*senderMock, Send(_)).WillOnce(SaveArg<0>(&response));
  EXPECT_CALL(*senderMock, Close());
  auto sut = std::make_unique<HttpLayer>(processor, std::move(senderMock));
  EXPECT_CALL(processor, Process(_));
  std::string request("GET / HTTP/1.1\r\n\r\n");
  sut->Receive(request);
  EXPECT_TRUE(response.starts_with("HTTP/1.1 200 OK"));
}

}  // namespace network
