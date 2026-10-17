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

	// Joined HLS fragments form a fragmented MP4 with duration 0, so players show no length.
	// Repackages the file into a regular MP4 with ffmpeg (no re-encoding).
	repair_result repair_fragmented_m4a(const std::string& path);
}
