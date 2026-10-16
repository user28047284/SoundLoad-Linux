#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "json/json.hpp"

#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
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
