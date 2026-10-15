# SoundLoad
The goal of this project is to help SoundCloud users save their favorite songs 
with as much data as possible preserved, and to simplify the process of porting 
songs to other platforms, which can be very tedious normally.

Based on [SoundLoad](https://github.com/xref2000/SoundLoad) by [xref2000](https://github.com/xref2000), 
who designed and wrote the original code. Please report issues on this repository.

Only download content you have the right to save.

SoundCloud only serves AAC now, so tracks are saved as `.m4a`. To play them in Spotify on Linux 
and sync them to your phone, see [SPOTIFY.md](SPOTIFY.md).

## Installation & usage
Install the dependencies (a C++20 compiler, CMake, pkg-config, libcurl and TagLib 2.x):

- Arch: `sudo pacman -S --needed base-devel cmake pkgconf curl taglib`
- Debian/Ubuntu: `sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libtag-dev`
- Fedora: `sudo dnf install gcc-c++ cmake pkgconf-pkg-config libcurl-devel taglib-devel`

`ffmpeg` is an optional (but recommended) runtime dependency. SoundCloud serves
AAC as HLS fragments, which are joined into a "fragmented MP4" whose header reports a duration of 0,
so players like Spotify show no track length. If `ffmpeg` is on your `PATH`, `sl` repackages the file
(stream copy, no re-encoding) so the real duration is stored. Without `ffmpeg` the download still
works and you get a warning.

Then build (and optionally install) the program:
```
cmake -S . -B build
cmake --build build -j
cmake --install build --prefix ~/.local    # optional, installs ~/.local/bin/sl
```

`cfg.json` is stored next to `sl`. Use `-xdg` to store it in `~/.config/soundload/` (or 
`$XDG_CONFIG_HOME/soundload/`) instead, which is useful if `sl` is installed somewhere you can't 
write to. Once a `cfg.json` exists there it is used automatically, e.g. after `sl -xdg -s`.

Run `sl '-?'` to see a list of all available features, or reference the table below.

## Arguments (case-insensitive)

| Argument          |   Short  | Description                                          | Saveable? |
| ----------------- | :------: | ---------------------------------------------------- | :-------: |
| `-cid`            |     —    | SoundCloud client ID                                 |     ✅    |
| `-xdg`            |     —    | Stores `cfg.json` in `~/.config/soundload`           |     ❌    |
| `-pvars`          |     —    | Adds program to PATH (asks first)                    |     ❌    |
| `-save`           |   `-s`   | Saves applicable arguments to `cfg.json`             |     ❌    |
| `-img-name`       | `-iname` | Cover art file name                                  |     ❌    |
| `-img-dst`        |  `-idst` | Cover art output directory                           |     ✅    |
| `-img-src`        |  `-isrc` | Cover art source (path, SoundCloud link, image link) |     ❌    |
| `-art`            |     —    | Independent cover art download                       |     ✅    |
| `-n-art`          |     —    | Disable independent cover art download               |     ✅    |
| `-audio-name`     | `-aname` | Audio file name                                      |     ❌    |
| `-audio-dst`      |  `-adst` | Audio output directory                               |     ✅    |
| `-audio`          |     —    | Enables audio downloads                              |     ✅    |
| `-n-audio`        |     —    | Disables audio downloads                             |     ✅    |
| `-title`          |   `-t`   | Audio tag title                                      |     ❌    |
| `-comment`        |   `-c`   | Audio tag comment                                    |     ❌    |
| `-artists`        |   `-a`   | Audio tag contributing artists                       |     ❌    |
| `-a-artist`       |   `-aa`  | Audio tag album artist                               |     ❌    |
| `-album`          |   `-al`  | Audio tag album                                      |     ❌    |
| `-genre`          |   `-g`   | Audio tag genre                                      |     ❌    |
| `-num`            |   `-n`   | Audio tag track number                               |     ❌    |
| `-year`           |   `-y`   | Audio tag year                                       |     ❌    |
| `-parse`          |   `-p`   | Split "Artist - Title" track names into artist + title |     ❌    |


## Examples

### Setting basic config info
By running this, the audio output dir and cover art output dir will be saved to cfg.json.
```
$ sl -adst ~/Music -idst ~/Pictures/covers -s
```

### Downloading a song
This will download the song, name the audio file "Extra Stixx", and set the contributing artists to "Pook G, Lul Jody".
Other metadata will be automatically scraped from the page, but can of course be set manually.
```
$ sl https://soundcloud.com/fat-kid-915108395/exrta-stixxs-ft-pook-g-lul -aname "Extra Stixx" -a "Pook G, Lul Jody"
```

### Parsing the artist from the track name
Many uploads put the artist in the track name. With `-p`, a title like
"ISLURWHENITALK X 2SHANEZ - KILLEMALL (PROD. NIKO EAST)" is tagged with the artist
"ISLURWHENITALK X 2SHANEZ" and the title "KILLEMALL (PROD. NIKO EAST)". The split happens at the first
" - " (a hyphen, en dash or em dash with a space on each side), so names like "Mid-Air" are left alone.
`-title` and `-artists` always win over the parsed values, and the file name still uses the full track
name unless you set `-aname`.
```
$ sl https://soundcloud.com/some-artist/some-track -p
```

### Downloading cover art
By using the `-art` arg, you tell the program to seperately download the cover art from the track. It 
should be noted that this option can be saved to cfg.json. This is useful for anyone archiving 
underground music, where cover art is often changed or lost to time. The `-n-audio` arg is also used 
and saved, preventing the track itself from being downloaded. Cover art will be embedded into the 
tracks metadata (if one is downloaded) in the highest available quality no matter what.
```
$ sl https://soundcloud.com/sellasouls/bdayy-sexxx -n-audio -art -s
```

## Extensibility
The source code is structured in such a way that makes adding new metadata fields extremely simple. 
First, go to `https://api-v2.soundcloud.com/resolve?url=put_link_here&client_id=your_cid` and take note 
of any fields you would like to archive. You then go to `site_api.cpp/sc_upload::create_comments` 
and use the `add_comment` lambda, which takes just one simple line of code:
```C++
add_comment("Upload date: ",          "created_at");
add_comment("Original title: ",       "title");
add_comment("Original description: ", "description");
add_comment("Original tags: ",        "tag_list");
```

## PATH
Adding the program's directory to your PATH lets your shell find `sl` no matter your working 
directory. When you run the program for the first time (or use the `-pvars` argument), it will 
show the line to add to your shell profile (`~/.bashrc`, `~/.zshrc` or `config.fish`) and ask 
whether to add it for you. Nothing is changed unless you answer `y`. Alternatively, install it 
to `~/.local/bin` as shown above.

## Client IDs
A client ID is a string that must be appended to certain requests in order for 
them to succeed. The program will automatically resolve and save a client ID, 
but in the case that it fails, you can provide a client ID manually like this:

- Open browser dev tools (`ctrl+shift+i`)
- Go to the network tab
- Go to [SoundCloud](https://soundcloud.com)
- Filter URLs with "client_id="
- Find a request with a client ID in it
- Pass that ID to `sl` as the value for the `-cid` argument

That being said, please open an issue on this repository if it's failing to resolve 
a client ID automatically, as failure is indicative of a SoundCloud update.

## Notes
- Tracks are downloaded as 160 kbps AAC. If a track only has a 96 kbps AAC stream, that is used.
- `nlohmann::json::value` has multiple modifications in this repo - check `pch.hpp` for more info.
- Unicode characters are supported in file names and metadata.
- The following characters will be replaced with an underscore in file names: `< > : " / \ | ? *`
- Quote `-?` (`sl '-?'`) and any links containing `?` or `&`, otherwise your shell will interpret them.
- Output directories must already exist.
- Album/playlist downloads are not implemented yet.
- Go+ songs cannot be downloaded (may work if you use a CID generated by a Go+ account, not tested).

## License
MIT, see [`LICENSE.txt`](LICENSE.txt).
