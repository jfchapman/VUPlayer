#pragma once

#include <atomic>

#include "Library.h"

// Library maintainer.
class LibraryMaintainer
{
public:
	// 'library' - media library.
	LibraryMaintainer( Library& library );

	virtual ~LibraryMaintainer();

	// Starts library maintenance.
	void Start();

	// Stops library maintenance.
	void Stop();

	// Returns the number of remaining items to update.
	int GetPendingCount() const;

private:
	// Thread procedure.
	static DWORD WINAPI MaintainerThreadProc( LPVOID lpParam );

	// Maintenance thread handler.
	void Handler();

	// Media library.
	Library& m_Library;

	// Stop event handle.
	HANDLE m_StopEvent;

	// Thread handle.
	HANDLE m_Thread;

	// Number of items left to process.
	std::atomic<int> m_PendingCount;
};

