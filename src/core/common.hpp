#pragma once
#include <string>

namespace common {

void ToLower(std::string&);

std::string SHA1(std::string_view);

std::string Base64(std::string_view);

}  // namespace common
