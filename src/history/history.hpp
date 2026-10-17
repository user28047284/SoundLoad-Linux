#pragma once

// The last 10 downloads for -undo, newest first, in the platform's state folder

namespace history
{
	// Notes a file written by this run. replaced = an earlier copy existed and was overwritten.
	void add(const std::string& path, bool replaced);

	// Saves the files noted with add() as the newest download. Does nothing if there are none.
	void save(void);

	// -undo: lists the last downloads and deletes the ones the user picks (asks first)
	void undo(void);
}
