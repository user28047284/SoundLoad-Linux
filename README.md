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
Get it from the [Releases](https://github.com/user28047284/SoundLoad-Linux/releases) page and extract it anywhere:
- **Nightly** (marked *Latest*): built automatically from every change, so it always has the newest
  fixes and features.
- **Numbered versions** (e.g. `v1.2.0`): tested stable versions. The newest one is linked at the
  top of this README.

| System  | File                     | Notes                                                       |
| ------- | ------------------------ | ----------------------------------------------------------- |
| Linux   | `sl-linux-x86_64.tar.gz` | Needs glibc 2.39+ (e.g. Arch, Fedora 40, Ubuntu 24.04, Debian 13) and libcurl |
| Windows | `sl-windows-x86_64.zip`  | One self-contained `sl.exe`                                 |

To run `sl` from any folder, use `sl -pvars` once: it offers to add its folder to your `PATH`
(nothing is changed unless you agree). It also offers this on the first run. On Windows you can
also double-click `sl.exe`: the window stays open until you press Enter.

**Install `ffmpeg` too** (Linux: your package manager, Windows: `winget install Gyan.FFmpeg`).
SoundCloud streams AAC in fragments, which players like Spotify show with no length. With `ffmpeg`
installed, `sl` repackages each download into a regular MP4 (no re-encoding, nothing is lost).
Without it downloads still work, with a warning.

To build it yourself, see [CONTRIBUTING.md](https://github.com/user28047284/SoundLoad-Linux/blob/master/CONTRIBUTING.md#building-from-source).

## Usage
```
sl <track link> [options]    download a track
sl [options] -s              only change saved settings
sl -undo                     delete recent downloads
sl '-?'                      list all options
```
Quote links and values that contain spaces or shell characters (`? & * ; < > |`), including `-?`.
On Windows `cmd.exe`, use double quotes (`"like this"`): single quotes are passed through literally
and end up in saved values. PowerShell and Linux shells accept either.

Settings marked as saveable are stored in `cfg.json` when you add `-s`, and used for every
download after that. Options given on the command line always win over saved ones. `cfg.json` is
kept next to `sl`; use `-xdg` to keep it in your user settings folder instead (`~/.config/soundload/`
or `%APPDATA%\soundload\`), e.g. if `sl` is installed somewhere you can't write to. Once a
`cfg.json` exists there, it is used automatically. The `-undo` history is in
`~/.local/state/soundload/history.json` or `%LOCALAPPDATA%\soundload\history.json`.

## Options
Option names ignore case. Run `sl '-?'` for the same list.

| Option                     | Short    | Description                                               | Saveable |
| -------------------------- | -------- | --------------------------------------------------------- | :------: |
| `-save`                    | `-s`     | Save the saveable options of this command to `cfg.json`   |          |
| `-audio-dst <dir>`         | `-adst`  | Folder for audio files                                    |    ✅    |
| `-img-dst <dir>`           | `-idst`  | Folder for cover art images                               |    ✅    |
| `-audio-name <name>`       | `-aname` | Audio file name (default: the track title)                |          |
| `-img-name <name>`         | `-iname` | Cover art file name (default: the track title)            |          |
| `-audio` / `-n-audio`      |          | Download the audio (default) / don't                      |    ✅    |
| `-art` / `-n-art`          |          | Also save the cover art as an image file / don't (default)|    ✅    |
| `-img-src <source>`        | `-isrc`  | Cover art to embed: image file or image link              |          |
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
| `-xdg`                     |          | Keep `cfg.json` in your user settings folder              |          |
| `-pvars`                   |          | Show how to add `sl` to `PATH` (asks first)               |          |
| `-version`                 |          | Show the version                                          |          |
| `-?`                       |          | Show all options                                          |          |

## Examples
```
sl -adst ~/Music -idst ~/Pictures/covers -s                  # save your folders
sl <track link> -aname "Extra Stixx" -a "Pook G, Lul Jody"   # your own file name and artists
sl <track link> -p                                           # take the artist from "Artist - Title"
sl <track link> -n-audio -art                                # only the cover art, in full resolution
sl -replace "wifiskeleton" "skeleton" -s                     # always tag this artist differently
sl -n-replace "wifiskeleton" -s                              # remove that rule again
sl -undo                                                     # pick recent downloads to delete
```
- Everything not given is read from SoundCloud. `-p` splits at the first " - " (a hyphen, en dash or
  em dash with spaces around it), so names like "Mid-Air" are left alone.
- `-replace` matches whole names and ignores case: `wifiskeleton x 4jay` becomes `skeleton x 4jay`,
  but `-replace jay j` leaves `4jay` alone. It runs after `-p`, but an artist given with `-artists` is kept.
- `-undo` also deletes a download's separate cover art. If a download replaced an older copy of the
  song, the older copy can't be brought back.

## How downloads behave
- Tracks are saved as 160 kbps AAC, or 96 kbps if that is all a track has (you get a warning).
- Tracks without their own cover art get the uploader's profile picture, like on SoundCloud.
- If the file already exists, `sl` asks before replacing it. When it isn't run from a terminal
  (e.g. from a script), existing songs are skipped.
- Songs are written under a hidden temporary name and only appear in the folder once they are fully
  tagged, so music players that watch the folder (like Spotify) never pick up an unfinished file.
- Some tracks are only streamed encrypted (DRM). These can't be downloaded and `sl` says so.
- Only single tracks can be downloaded, not albums, playlists or profiles. Go+ tracks can't be
  downloaded either (they may work with a client ID from a Go+ account, not tested).
- The characters `< > : " / \ | ? *` are replaced with `_` in file names, and names Windows
  reserves (`CON`, `NUL`, `COM1`...) get a `_` added. Unicode is kept.
- Output folders must already exist.

## Client IDs
SoundCloud's API needs a client ID. `sl` finds one automatically and saves it, and fetches a new one
if the saved one stops working. If that fails, find one yourself (browser developer tools, Network
tab on soundcloud.com, filter by `client_id=`) and run `sl -cid <id> -s`. Please also open an
issue, since this usually means SoundCloud changed something.

## More
- [CONTRIBUTING.md](https://github.com/user28047284/SoundLoad-Linux/blob/master/CONTRIBUTING.md): building from source, adding metadata, how releases are made.
- [SPOTIFY.md](SPOTIFY.md): playing the downloads in Spotify.

## License
MIT, see [`LICENSE.txt`](LICENSE.txt).
