cmake_minimum_required(VERSION 3.20)

project(SoundLoad VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Dependencies

find_package(CURL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=2.0)

find_package(Threads REQUIRED)

# Executable

add_executable(sl
    src/config/config.cpp
    src/core/main.cpp
    src/core/util.cpp
    src/history/history.cpp
    src/media/m4a.cpp
    src/net/http.cpp
    src/site_api/site_api.cpp
)

target_include_directories(sl PRIVATE src/core)
target_include_directories(sl SYSTEM PRIVATE includes) # vendored (modified) nlohmann json

target_link_libraries(sl PRIVATE CURL::libcurl PkgConfig::TAGLIB Threads::Threads)

target_precompile_headers(sl PRIVATE src/core/pch.hpp)

target_compile_options(sl PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS sl RUNTIME DESTINATION bin)
