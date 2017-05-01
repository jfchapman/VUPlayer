#pragma once

#include "Settings.h"

#include "replaygain_analysis.h"

#include <atomic>
#include <tuple>

class ReplayGain
{
public:
	// 'library' - media library.
	// 'handlers' - media handlers.
	ReplayGain( Library& library, const Handlers& handlers );

	virtual ~ReplayGain();

	// Calculates ReplayGain values for a 'mediaList'.
	void Calculate( const MediaInfo::List& mediaList );

	// Stops any pending ReplayGain calculations.
	void Stop();

	// Returns the number of ReplayGain calculations pending.
	int GetPendingCount() const;

	// Calculation thread handler.
	void Handler();

private:
	// ReplayGain album key.
	typedef std::tuple<long,long,std::wstring> AlbumKey;

	// Associates an album key with a list of media information.
	typedef std::map<AlbumKey,MediaInfo::List> AlbumMap;

	// Adds a 'mediaInfo' to the queue of pending tasks.
	void AddPending( const MediaInfo& mediaInfo );

	// Returns whether the calculation thread can continue.
	bool CanContinue() const;

	// Media library.
	Library& m_Library;

	// Media handlers.
	const Handlers& m_Handlers;

	// The task queue.
	AlbumMap m_AlbumQueue;

	// The mutex for the task queue.
	std::mutex m_Mutex;

	// Handle to stop the calculation thread.
	HANDLE m_StopEvent;

	// Handle to wake the calculation thread.
	HANDLE m_WakeEvent;

	// Calculation thread.
	HANDLE m_Thread;

	// Number of ReplayGain calculations pending.
	std::atomic<int> m_PendingCount;

	// ReplayGain analysis object.
	replaygain_analysis m_GainAnalysis;
};
