#include <gtest/gtest.h>
#include "common.hpp"

using namespace testing;

namespace common {

TEST(CommonFunctionTest, whenEncodingWebsocketAcceptKey_itShouldProduceCorrectResult) {
  ASSERT_EQ(Base64(SHA1("x3JJHMbDL1EzLkh9GBhXDw=="
                        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")),
      "HSmrc0sMlYUkAGmm5OPpG2HaGWk=");
}

TEST(CommonFunctionTest, whenEncodingSHA1_itShouldProduceCorrectResult) {
  ASSERT_EQ(SHA1("abc"), "\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c\x9c\xd0\xd8\x9d");
}

}  // namespace common
