#pragma once

namespace media
{
	enum class repair_result
	{
		repaired,
		ffmpeg_missing, // ffmpeg is not installed, the file was left as it is
		not_repaired,   // ffmpeg worked but the result could not replace the file, which was left as it is
		invalid         // ffmpeg could not read the file, so it is not valid audio
	};

	// SoundCloud's AAC streams are HLS fragments. Joining them gives a "fragmented MP4" whose
	// header says the duration is 0, so players such as Spotify show no track length. This
	// repackages the file into a regular MP4 with ffmpeg (no re-encoding, nothing is lost).
	repair_result repair_fragmented_m4a(const std::string& path);
}
