#pragma once

#include "Playlist.h"
#include "Settings.h"

#include <atomic>
#include <tuple>

class GainCalculator
{
public:
	// 'library' - media library.
	// 'handlers' - media handlers.
	GainCalculator( Library& library, const Handlers& handlers );

	virtual ~GainCalculator();

	// Calculates gain values for the playlist 'items'.
	void Calculate( const Playlist::ItemList& items );

	// Stops any pending gain calculations.
	void Stop();

	// Returns the number of gain calculations pending.
	int GetPendingCount() const;

private:
	// Gain album key.
	typedef std::tuple<long,long,std::wstring> AlbumKey;

	// Associates an album key with a list of items.
	typedef std::map<AlbumKey,Playlist::ItemList> AlbumMap;

	// Calculation thread procedure.
	static DWORD WINAPI CalcThreadProc( LPVOID lpParam );

	// Calculation thread handler.
	void Handler();

	// Adds an 'item' to the queue of pending tasks.
	void AddPending( const Playlist::Item& item );

	// Returns whether the calculation thread can continue.
	bool CanContinue() const;

	// Returns a decoder for the 'item', or nullptr if a decoder could not be opened.
	Decoder::Ptr OpenDecoder( const Playlist::Item& item ) const;

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

	// Number of gain calculations pending.
	std::atomic<int> m_PendingCount;
};
