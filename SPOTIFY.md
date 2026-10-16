# Using SoundLoad downloads in Spotify
`sl` saves tracks as AAC (`.m4a`). Spotify can play them as *Local Files* and sync them to its 
mobile app, but on Linux it needs some setup first.

## Playing the files on Linux
Spotify's *Local Files* can play `.m4a` files on Linux, but two things get in the way.

**1. Spotify cannot decode local files at all on a current system.**
Every local file is listed as dimmed with `-:--` for its length, and clicking it shows 
"Spotify cannot play this file currently". Spotify decodes local files with the system's FFmpeg 
libraries and only knows `libavcodec.so.54` to `.61` (FFmpeg 2.x to 7.x). Rolling distributions 
such as Arch currently ship FFmpeg 9 (`libavcodec.so.63`), so Spotify finds nothing it can use. 
This affects all local files, not just the ones `sl` makes.

The fix is to install an older FFmpeg next to the new one and point Spotify at it. 
On Arch the `ffmpeg4.4` package installs its libraries in `/usr/lib/ffmpeg4.4` without replacing 
your normal FFmpeg:

```
sudo pacman -S ffmpeg4.4
```

If you start Spotify with `spotify-launcher`, add this to `~/.config/spotify-launcher.conf`:

```
[spotify]
extra_env_vars = ["LD_LIBRARY_PATH=/usr/lib/ffmpeg4.4"]
```

If you start it another way, set the variable yourself, e.g. 
`LD_LIBRARY_PATH=/usr/lib/ffmpeg4.4 spotify`. On other distributions use whichever 
FFmpeg 4 to 7 package is available and point `LD_LIBRARY_PATH` at its library directory. 
Restart Spotify completely afterwards.

**2. The track length shows as `-:--`.**
`sl` fixes this automatically when `ffmpeg` is installed (see the [README](README.md#installation)). 
For `.m4a` files downloaded without it, repackage them (no re-encoding, nothing is lost):

```
ffmpeg -i "song.m4a" -map 0 -c copy -movflags +faststart "song-fixed.m4a"
```

Spotify's Local Files also merges tracks that have identical title, artist and album, so if a 
converted copy seems to be missing, check whether it is being treated as a duplicate of the original.

## Syncing to the Spotify mobile app
Spotify doesn't upload local files. Your phone copies them straight from the desktop app over 
your local network, so both have to be able to see each other:

1. In the desktop app, turn on **Settings → Your Library → Show Local Files** and add your 
   download folder (e.g. `~/Music`) as a source.
2. Connect your PC and phone to the **same Wi-Fi network**, and log in to the same Spotify account on both.
3. Make sure **no VPN is enabled** on either device. A VPN hides the devices from each other, 
   and the sync fails without telling you why.
4. Add the tracks to a playlist on the desktop. Local files can't be downloaded from the 
   *Local Files* list itself, only from a playlist.
5. On your phone, open that playlist and **download it**. Keep the desktop app open until every 
   track has a green download arrow.

If a track is greyed out with a red download icon and shows no artist or cover art, Spotify 
cached it before its tags were written. Quit Spotify completely on the desktop, delete 
`~/.config/spotify/Users/<your user>/local-files.bnk`, start it again and re-add the track to 
the playlist.
