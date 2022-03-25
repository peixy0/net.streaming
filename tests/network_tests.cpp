#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "network.hpp"
#include "network_mocks.hpp"
#include "spdlog/common.h"

using namespace testing;

namespace network {

TEST(HttpLayerTestSuite, whenReceivedValidHttpRequest_itShouldRespondOk) {
  std::string responsePayload;
  HttpRequest httpRequest;
  httpRequest.method = "get";
  httpRequest.uri = "/";
  PlainHttpResponse httpResponse;
  httpResponse.status = HttpStatus::OK;
  auto parserMock = std::make_unique<StrictMock<network::HttpParserMock>>();
  EXPECT_CALL(*parserMock, Parse(_)).WillOnce(Return(httpRequest));
  StrictMock<HttpProcessorMock> processor;
  StrictMock<network::NetworkSenderMock> senderMock;
  EXPECT_CALL(senderMock, Send(_)).WillOnce(SaveArg<0>(&responsePayload));
  auto sut = std::make_unique<HttpLayer>(std::move(parserMock), processor, senderMock);
  EXPECT_CALL(processor, Process(_)).WillOnce(Return(httpResponse));
  std::string requestPayload("GET / HTTP/1.1\r\n\r\n");
  sut->Receive(requestPayload);
  EXPECT_EQ(responsePayload, "HTTP/1.1 200 OK\r\ncontent-length: 0\r\n\r\n");
}

TEST(HttpParserTestSuite, whenReceivedValidHttpRequest_itShouldParseTheRequest) {
  spdlog::set_level(spdlog::level::debug);
  auto sut = std::make_unique<ConcreteHttpParser>();
  std::string p{
      "GET / HTTP/1.1\r\n"
      "Host: 127.0.0.1:8080\r\n"
      "Accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/"
      "signed-exch"};
  std::string p2{
      "ange;v=b3;q=0.9\r\n"
      "Sec-Fetch-Site: none\r\n"
      "Sec-Fetch-Mode: navigate\r\n"
      "Sec-Fetch-User: ?1\r\n"
      "Sec-Fetch-Dest: document\r\n"
      "Accept-Encoding: gzip, deflate, br\r\n"
      "Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7\r\n\r\n"};
  sut->Parse(p);
  p += p2;
  auto req = sut->Parse(p);
  ASSERT_TRUE(req);
  ASSERT_EQ(req->method, "get");
  ASSERT_EQ(req->uri, "/");
  ASSERT_EQ(req->version, "HTTP/1.1");
}

}  // namespace network
