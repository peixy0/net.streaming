#include "common.hpp"

namespace {

std::uint32_t LeftRotate(std::uint32_t a, std::uint8_t k) {
  return (a << k) | (a >> (32 - k));
}

}  // namespace

namespace common {

char ToChar(std::uint64_t n) {
  char c;
  reinterpret_cast<std::uint8_t&>(c) = static_cast<std::uint8_t>(n & 0xff);
  return c;
}

void ToLower(std::string& s) {
  for (char& c : s) {
    c = static_cast<char>(tolower(c));
  }
}

std::string SHA1(std::string_view s) {
  std::string payload{s};
  std::uint32_t h0 = 0x67452301;
  std::uint32_t h1 = 0xEFCDAB89;
  std::uint32_t h2 = 0x98BADCFE;
  std::uint32_t h3 = 0x10325476;
  std::uint32_t h4 = 0xC3D2E1F0;

  std::uint64_t ml = payload.size();
  std::uint64_t padLen = (56 - ((ml + 1) % 64) + 64) % 64;
  payload.reserve(ml + padLen + 8);
  payload += ToChar(0x80);
  for (std::uint64_t i = 0; i < padLen; i++) {
    payload += ToChar(0);
  }
  ml <<= 3;
  payload += ToChar(ml >> 56);
  payload += ToChar(ml >> 48);
  payload += ToChar(ml >> 40);
  payload += ToChar(ml >> 32);
  payload += ToChar(ml >> 24);
  payload += ToChar(ml >> 16);
  payload += ToChar(ml >> 8);
  payload += ToChar(ml >> 0);

  std::uint64_t len = payload.size();
  std::uint8_t* p = reinterpret_cast<std::uint8_t*>(payload.data());
  for (std::uint64_t chunk = 0; chunk < len; chunk += 64) {
    std::uint32_t w[80] = {0};
    for (int i = 0; i < 16; i++) {
      int x = i * 4;
      w[i] |= p[chunk + x] << 24;
      w[i] |= p[chunk + x + 1] << 16;
      w[i] |= p[chunk + x + 2] << 8;
      w[i] |= p[chunk + x + 3];
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

  constexpr std::uint32_t shaDigestLen = 20;
  char r[shaDigestLen];
  r[0] = ToChar(h0 >> 24);
  r[1] = ToChar(h0 >> 16);
  r[2] = ToChar(h0 >> 8);
  r[3] = ToChar(h0 >> 0);

  r[4] = ToChar(h1 >> 24);
  r[5] = ToChar(h1 >> 16);
  r[6] = ToChar(h1 >> 8);
  r[7] = ToChar(h1 >> 0);

  r[8] = ToChar(h2 >> 24);
  r[9] = ToChar(h2 >> 16);
  r[10] = ToChar(h2 >> 8);
  r[11] = ToChar(h2 >> 0);

  r[12] = ToChar(h3 >> 24);
  r[13] = ToChar(h3 >> 16);
  r[14] = ToChar(h3 >> 8);
  r[15] = ToChar(h3 >> 0);

  r[16] = ToChar(h4 >> 24);
  r[17] = ToChar(h4 >> 16);
  r[18] = ToChar(h4 >> 8);
  r[19] = ToChar(h4 >> 0);

  return std::string{r, r + shaDigestLen};
}

std::string Base64(std::string_view payload) {
  const char* alpha =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  size_t len = payload.size();
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
