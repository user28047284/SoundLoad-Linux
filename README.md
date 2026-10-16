# SoundLoad
[![Stable release](https://img.shields.io/github/v/release/user28047284/SoundLoad-Linux?filter=v*&label=stable%20release)](https://github.com/user28047284/SoundLoad-Linux/releases)

`sl` downloads SoundCloud tracks on Linux and Windows with as much of their data as possible: the audio, the cover art
in its highest available resolution, and tags for title, artist, album, genre, year and a comment
with the upload date, original title, description and tags. The goal is to make saving songs, and
moving them to other platforms, painless.

Based on [SoundLoad](https://github.com/xref2000/SoundLoad) by [xref2000](https://github.com/xref2000),
who designed and wrote the original code. Please report issues on this repository.

Only download content you have the right to save.

SoundCloud only serves AAC now, so tracks are saved as `.m4a`. To play them in Spotify
and sync them to your phone, see [SPOTIFY.md](SPOTIFY.md).

## Installation

### Download
Get it from the [Releases](../../releases) page:
- **Nightly** (marked *Latest*): built automatically from every change, so it always has the newest
  fixes and features.
- **Numbered versions** (e.g. `v1.2.0`): tested stable versions. The newest one is linked at the
  top of this README.

| System  | File                     | Notes                                                       |
| ------- | ------------------------ | ----------------------------------------------------------- |
| Linux   | `sl-linux-x86_64.tar.gz` | Needs glibc 2.39+ (e.g. Arch, Fedora 40, Ubuntu 24.04, Debian 13) and libcurl |
| Windows | `sl-windows-x86_64.zip`  | One self-contained `sl.exe`                                 |

Extract it anywhere. To run `sl` from any folder, use `sl -pvars` once: it shows how to add its
folder to your `PATH` and offers to do it (on Linux in your shell profile, on Windows in your user
`PATH` setting, with a Yes/No dialog). Nothing is changed unless you agree. It also offers this on
the first run. On Windows you can also double-click `sl.exe`: the window stays open until you press Enter.

**Install `ffmpeg` too** (Linux: your package manager, Windows: `winget install Gyan.FFmpeg`).
SoundCloud streams AAC in fragments, and joined together they form a "fragmented MP4" whose header
says the track is 0 seconds long, so players like Spotify show no length. With `ffmpeg` installed,
`sl` repackages each download into a regular MP4 (no re-encoding, nothing is lost). Without it
downloads still work, with a warning.

### Building from source
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

**Windows:** install Visual Studio 2022 (with "Desktop development with C++") and
[vcpkg](https://github.com/microsoft/vcpkg). The dependencies in `vcpkg.json` are installed
automatically:
```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

## Usage
```
sl <track link> [options]    download a track
sl [options] -s              only change saved settings
sl -undo                     delete recent downloads
sl '-?'                      list all options
```
Quote links and values that contain spaces or shell characters (`? & * ; < > |`), including `-?`.

Settings marked as saveable are stored in `cfg.json` when you add `-s`, and used for every
download after that. Options given on the command line always win over saved ones. `cfg.json` is
kept next to `sl`; use `-xdg` to keep it in your user settings folder instead, e.g. if `sl` is
installed somewhere you can't write to. Once a `cfg.json` exists there, it is used automatically.

|                | Linux                                         | Windows                                 |
| -------------- | --------------------------------------------- | --------------------------------------- |
| `cfg.json`     | next to `sl`, or `~/.config/soundload/`       | next to `sl.exe`, or `%APPDATA%\soundload\` |
| `-undo` history | `~/.local/state/soundload/history.json`      | `%LOCALAPPDATA%\soundload\history.json` |

On Linux, `$XDG_CONFIG_HOME` and `$XDG_STATE_HOME` are used instead of `~/.config` and
`~/.local/state` when they are set.

## Options
Option names ignore case.

| Option                     | Short    | Description                                               | Saveable |
| -------------------------- | -------- | --------------------------------------------------------- | :------: |
| `-save`                    | `-s`     | Save the saveable options of this command to `cfg.json`   |          |
| `-audio-dst <dir>`         | `-adst`  | Folder for audio files                                    |    ✅    |
| `-img-dst <dir>`           | `-idst`  | Folder for cover art images                               |    ✅    |
| `-audio-name <name>`       | `-aname` | Audio file name (default: the track title)                |          |
| `-img-name <name>`         | `-iname` | Cover art file name (default: the track title)            |          |
| `-audio` / `-n-audio`      |          | Download the audio (default) / don't                      |    ✅    |
| `-art` / `-n-art`          |          | Also save the cover art as an image file / don't (default)|    ✅    |
| `-img-src <source>`        | `-isrc`  | Cover art to embed: image file, image link or track link  |          |
| `-title <title>`           | `-t`     | Title tag                                                 |          |
| `-artists <artists>`       | `-a`     | Artist tag (default: the track's artist, or its uploader) |          |
| `-a-artist <artist>`       | `-aa`    | Album artist tag                                          |          |
| `-album <album>`           | `-al`    | Album tag (default: the track's album, or its title)      |          |
| `-genre <genre>`           | `-g`     | Genre tag                                                 |          |
| `-num <number>`            | `-n`     | Track number tag                                          |          |
| `-year <year>`             | `-y`     | Year tag (default: the upload year)                       |          |
| `-comment <text>`          | `-c`     | Comment tag, SoundCloud's details are added after it      |          |
| `-parse`                   | `-p`     | Split "Artist - Title" track names into artist and title  |          |
| `-replace <from> <to>`     |          | Replace an artist name in the artist tag                  |    ✅    |
| `-n-replace <from>`        |          | Remove a saved `-replace`                                 |    ✅    |
| `-undo`                    |          | Delete any of your last 10 downloads                      |          |
| `-cid <id>`                |          | SoundCloud client ID (normally found automatically)       |    ✅    |
| `-xdg`                     |          | Keep `cfg.json` in your user settings folder (see above)  |          |
| `-pvars`                   |          | Show how to add `sl` to `PATH` (asks first)               |          |
| `-version`                 |          | Show the version                                          |          |
| `-?`                       |          | Show all options                                          |          |

## Examples

### Saving your folders
```
sl -adst ~/Music -idst ~/Pictures/covers -s
```

### Downloading a song with your own tags
Everything not given is read from SoundCloud.
```
sl https://soundcloud.com/fat-kid-915108395/exrta-stixxs-ft-pook-g-lul -aname "Extra Stixx" -a "Pook G, Lul Jody"
```

### Taking the artist from the track name
Many uploads put the artist in the track name. With `-p`, "ISLURWHENITALK X 2SHANEZ - KILLEMALL
(PROD. NIKO EAST)" is tagged with the artist "ISLURWHENITALK X 2SHANEZ" and the title "KILLEMALL
(PROD. NIKO EAST)". It splits at the first " - " (a hyphen, en dash or em dash with spaces around it),
so names like "Mid-Air" are left alone. `-title` and `-artists` win over the parsed values, and the
file name keeps the full track name unless you set `-aname`.
```
sl https://soundcloud.com/some-artist/some-track -p
```

### Replacing artist names
If an artist uploads under a name you don't want in your library, `-replace` swaps it in the artist
tag. Saved with `-s`, it applies to every download. Matching ignores case and only replaces whole
names, so `wifiskeleton x 4jay` becomes `skeleton x 4jay` but `-replace jay j` leaves `4jay` alone.
It runs after `-p`, so parsed artists are replaced too, but an artist given with `-artists` is kept.
```
sl -replace 'wifiskeleton' 'skeleton' -s     # add (repeat for more artists)
sl -n-replace 'wifiskeleton' -s              # remove
```

### Saving cover art
`-art` also saves the cover art as its own image, in the highest resolution SoundCloud has. Combined
with `-n-audio`, only the cover art is downloaded. Useful for archiving underground music, where cover
art is often changed or lost. Both can be saved with `-s`. Downloaded songs always get the cover art
embedded, whether or not `-art` is used.
```
sl https://soundcloud.com/sellasouls/bdayy-sexxx -n-audio -art -s
```

### Undoing downloads
`-undo` lists your last 10 downloads and deletes the ones you pick: a number, several (`1 3`) or a
range (`1-3`). It asks once more before deleting anything. A download's separate cover art (`-art`) is
deleted with it. If a download replaced an older copy of the song, the older copy can't be brought
back.
```
$ sl -undo
Last downloads (newest first):
   1  real eyes realize real lies.m4a
   2  (SIGILKORE) MARKZ GET REKT.m4a
Delete which? (e.g. 1, 1 3, 1-3, Enter to cancel): 1
Delete 1 download (1 file)? [y/N] y
deleted 1 download
```

## How downloads behave
- Tracks are saved as 160 kbps AAC, or 96 kbps if that is all a track has (you get a warning).
- Tracks without their own cover art get the uploader's profile picture, like on SoundCloud.
- If the file already exists, `sl` asks before downloading it again and replacing it. When it isn't
  run from a terminal (e.g. from a script), existing songs are skipped.
- Songs are written under a hidden temporary name and only appear in the folder once they are fully
  tagged, so music players that watch the folder (like Spotify) never pick up an unfinished file.
- Some tracks are only streamed encrypted (DRM). These can't be downloaded and `sl` says so.
- Only single tracks can be downloaded, not albums, playlists or profiles. Go+ tracks can't be
  downloaded either (they may work with a client ID from a Go+ account, not tested).
- The characters `< > : " / \ | ? *` are replaced with `_` in file names, and names Windows
  reserves (`CON`, `NUL`, `COM1`...) get a `_` added, so files can be copied between systems.
  Unicode is kept.
- Output folders must already exist.

## Client IDs
SoundCloud's API needs a client ID. `sl` finds one automatically and saves it, and fetches a new one
if the saved one stops working. If that fails, you can set one yourself:

1. Open your browser's developer tools (`Ctrl+Shift+I`) and go to the Network tab
2. Open [SoundCloud](https://soundcloud.com)
3. Filter the requests by `client_id=` and copy the ID from one of them
4. Run `sl -cid <id> -s`

Please also open an issue, since this usually means SoundCloud changed something.

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
link at the top of this README is updated to it:
```
git tag v1.2.0
git push --tags
```
Tags with a suffix, like `v1.2.0-rc1`, are published as pre-releases and don't change the README.

## License
MIT, see [`LICENSE.txt`](LICENSE.txt).
