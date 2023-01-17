#include <gtest/gtest.h>
#include "http.hpp"
#include "spdlog/common.h"

using namespace testing;

namespace network {

TEST(HttpParserTestSuite, whenReceivedValidHttpRequest_itShouldParseTheRequest) {
  auto sut = std::make_unique<ConcreteHttpParser>();
  std::string p1{
      "GET /request?key=value&key2=value2 HTTP/1.1\r\n"
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
  sut->Append(p1);
  const auto req1 = sut->Parse();
  ASSERT_FALSE(req1);
  sut->Append(p2);
  const auto req2 = sut->Parse();
  ASSERT_TRUE(req2);
  ASSERT_EQ(req2->method, "get");
  ASSERT_EQ(req2->uri, "/request");
  ASSERT_EQ(req2->version, "HTTP/1.1");
  ASSERT_EQ(req2->headers.at("accept-encoding"), "gzip, deflate, br");
  ASSERT_EQ(req2->headers.at("accept-language"), "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7");
  ASSERT_EQ(req2->query.at("key"), "value");
  ASSERT_EQ(req2->query.at("key2"), "value2");
}

}  // namespace network
