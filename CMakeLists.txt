cmake_minimum_required(VERSION 3.20)

project(SoundLoad VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# With vcpkg's static triplets (Windows), also link the C runtime statically: one self-contained sl.exe
if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Dependencies

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# TagLib 2.x installs a CMake package (TagLib::tag), pkg-config is the fallback
find_package(taglib 2.0 CONFIG QUIET)

if(TARGET TagLib::tag)
    set(SL_TAGLIB TagLib::tag)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=2.0)
    set(SL_TAGLIB PkgConfig::TAGLIB)
endif()

# Version shown by -version. CI sets SL_VERSION (the tag, or "nightly"); the commit is read at configure time.

set(SL_VERSION "${PROJECT_VERSION}" CACHE STRING "Version shown by sl -version")
set(SL_COMMIT "")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE SL_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

configure_file(src/core/version.hpp.in "${CMAKE_BINARY_DIR}/generated/version.hpp" @ONLY)

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

if(WIN32)
    target_sources(sl PRIVATE src/platform/platform_windows.cpp)
    target_compile_definitions(sl PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN) # windows.h (also via curl.h) must not define min/max
    target_link_libraries(sl PRIVATE shell32 advapi32 user32)
else()
    target_sources(sl PRIVATE src/platform/platform_linux.cpp)
endif()

target_include_directories(sl PRIVATE src/core "${CMAKE_BINARY_DIR}/generated")
target_include_directories(sl SYSTEM PRIVATE includes) # vendored (modified) nlohmann json

target_link_libraries(sl PRIVATE CURL::libcurl ${SL_TAGLIB} Threads::Threads)

target_precompile_headers(sl PRIVATE src/core/pch.hpp)

if(MSVC)
    target_compile_options(sl PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(sl PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS sl RUNTIME DESTINATION bin)
