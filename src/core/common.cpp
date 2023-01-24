#include "common.hpp"

namespace {

std::uint32_t LeftRotate(std::uint32_t a, std::uint8_t k) {
  return (a << k) | (a >> (32 - k));
}

}  // namespace

namespace common {

void ToLower(std::string& s) {
  for (char& c : s) {
    c = tolower(c);
  }
}

std::string SHA1(std::string_view s) {
  constexpr std::uint32_t shaDigestLen = 20;
  char r[shaDigestLen];
  std::string payload{s};
  std::uint32_t h0 = 0x67452301;
  std::uint32_t h1 = 0xEFCDAB89;
  std::uint32_t h2 = 0x98BADCFE;
  std::uint32_t h3 = 0x10325476;
  std::uint32_t h4 = 0xC3D2E1F0;

  std::uint64_t ml = payload.size();
  payload += static_cast<std::uint8_t>(0x80);
  std::uint64_t padLen = (56 - ((ml + 1) % 64) + 64) % 64;
  for (std::uint64_t i = 0; i < padLen; i++) {
    payload += static_cast<std::uint8_t>(0);
  }
  ml <<= 3;
  payload += static_cast<std::uint8_t>((ml >> 56) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 48) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 40) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 32) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 24) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 16) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 8) & 0xff);
  payload += static_cast<std::uint8_t>((ml >> 0) & 0xff);

  std::uint64_t len = payload.size();
  std::uint8_t* p = reinterpret_cast<std::uint8_t*>(payload.data());
  for (std::uint64_t chunk = 0; chunk < len; chunk += 64) {
    std::uint32_t w[80] = {0};
    for (int i = 0; i < 16; i++) {
      w[i] |= p[chunk + i * 4] << 24;
      w[i] |= p[chunk + i * 4 + 1] << 16;
      w[i] |= p[chunk + i * 4 + 2] << 8;
      w[i] |= p[chunk + i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
      w[i] = LeftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = h0;
    std::uint32_t b = h1;
    std::uint32_t c = h2;
    std::uint32_t d = h3;
    std::uint32_t e = h4;

    for (int i = 0; i < 80; i++) {
      std::uint32_t f = 0, k = 0;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      std::uint32_t temp = LeftRotate(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = LeftRotate(b, 30);
      b = a;
      a = temp;
    }

    h0 = h0 + a;
    h1 = h1 + b;
    h2 = h2 + c;
    h3 = h3 + d;
    h4 = h4 + e;
  }

  r[0] = (h0 >> 24) & 0xff;
  r[1] = (h0 >> 16) & 0xff;
  r[2] = (h0 >> 8) & 0xff;
  r[3] = (h0 >> 0) & 0xff;

  r[4] = (h1 >> 24) & 0xff;
  r[5] = (h1 >> 16) & 0xff;
  r[6] = (h1 >> 8) & 0xff;
  r[7] = (h1 >> 0) & 0xff;

  r[8] = (h2 >> 24) & 0xff;
  r[9] = (h2 >> 16) & 0xff;
  r[10] = (h2 >> 8) & 0xff;
  r[11] = (h2 >> 0) & 0xff;

  r[12] = (h3 >> 24) & 0xff;
  r[13] = (h3 >> 16) & 0xff;
  r[14] = (h3 >> 8) & 0xff;
  r[15] = (h3 >> 0) & 0xff;

  r[16] = (h4 >> 24) & 0xff;
  r[17] = (h4 >> 16) & 0xff;
  r[18] = (h4 >> 8) & 0xff;
  r[19] = (h4 >> 0) & 0xff;

  return std::string{r, r + shaDigestLen};
}

std::string Base64(std::string_view payload) {
  const char* alpha =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  int len = payload.size();
  const unsigned char* p = reinterpret_cast<const unsigned char*>(payload.data());
  std::string result;
  while (len >= 3) {
    std::uint32_t d = p[0] << 16 | p[1] << 8 | p[2];
    result += alpha[d >> 18 & 0b111111];
    result += alpha[d >> 12 & 0b111111];
    result += alpha[d >> 6 & 0b111111];
    result += alpha[d & 0b111111];
    p += 3;
    len -= 3;
  }
  if (len > 1) {
    std::uint32_t d = p[0] << 16 | p[1] << 8;
    result += alpha[d >> 18 & 0b111111];
    result += alpha[d >> 12 & 0b111111];
    result += alpha[d >> 6 & 0b111111];
    result += '=';
    return result;
  }
  if (len > 0) {
    std::uint32_t d = p[0] << 16;
    result += alpha[d >> 18 & 0b111111];
    result += alpha[d >> 12 & 0b111111];
    result += "==";
    return result;
  }
  return result;
}

}  // namespace common
