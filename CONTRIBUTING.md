# Contributing

## Building from source
**Linux:** install a C++20 compiler (GCC 13+ or Clang 17+), CMake, pkg-config, libcurl and TagLib 2.x:

- Arch: `sudo pacman -S --needed base-devel cmake pkgconf curl taglib`
- Debian/Ubuntu: `sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libtag-dev`
  (needs a release that packages TagLib 2.x, such as Debian 13; on older ones build TagLib from source)
- Fedora: `sudo dnf install gcc-c++ cmake pkgconf-pkg-config libcurl-devel taglib-devel`

```
cmake -S . -B build
cmake --build build -j
cmake --install build --prefix ~/.local    # installs ~/.local/bin/sl
```

**Windows:** install Visual Studio 2022 or newer (with "Desktop development with C++") and
[vcpkg](https://github.com/microsoft/vcpkg), and set `VCPKG_ROOT` to the vcpkg folder. The
dependencies in `vcpkg.json` are installed automatically. In Visual Studio, open the folder and pick
the "Windows x64 static (Release)" configuration. From a Developer Command Prompt:
```
cmake --preset windows-static-release
cmake --build --preset windows-static-release
```

## Adding metadata
To keep more of SoundCloud's data, look at the fields in
`https://api-v2.soundcloud.com/resolve?url=<track link>&client_id=<client id>` and add them to the
comment in `sc_upload::build_comment` (`src/site_api/site_api.cpp`), one line each:
```C++
add("Upload date: ",          "created_at");
add("Original title: ",       "title");
add("Original description: ", "description");
add("Original tags: ",        "tag_list");
```
Note that `nlohmann::json::value` is modified in this repository, see `src/core/pch.hpp`.

## Releases
GitHub Actions builds Linux and Windows binaries for every push and pull request
(`.github/workflows/build.yml`). Each push to `master` replaces the **Nightly** release with the new
builds, and it is always the repository's *Latest* release. To publish a numbered (stable) release,
tag the commit; the release notes are generated from the commits since the previous one, and the
link at the top of the README is updated to it:
```
git tag v1.2.0
git push --tags
```
Tags with a suffix, like `v1.2.0-rc1`, are published as pre-releases and don't change the README.
