#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "json/json.hpp"

#include <taglib/fileref.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/tdebuglistener.h>

/* nlohmann::json modifications
*
* [*] nlohmann::json::value
*
* > NEW OVERLOAD
*   - defaults ReturnType to std::string
*   - removes default_value argument, will instead return ReturnType{} by default
*   - catches nlohmann::json::type_error exception 302 and returns ReturnType{} (properly handles cases of existing null values)
*
* > MODIFIED OVERLOAD
*   - catches nlohmann::json::type_error exception 302 and returns default_value (properly handles cases of existing null values)
*/

using Json = nlohmann::json;

// COMPILE/RUNTIME FNV1A HASHING (case-insensitive)

consteval uint32_t hash(const char* input, const uint32_t val = 0x811C9DC5u) noexcept
{
	char ch = input[0];
	if (ch < 91 && ch > 64) ch += 32;
	return !ch ? val : hash(input + 1, (val ^ static_cast<uint8_t>(ch)) * 0x01000193u);
}

inline constexpr uint32_t hash_rt(const char* input, const uint32_t val = 0x811C9DC5u) noexcept
{
	char ch = input[0];
	if (ch < 91 && ch > 64) ch += 32;
	return !ch ? val : hash_rt(input + 1, (val ^ static_cast<uint8_t>(ch)) * 0x01000193u);
}
