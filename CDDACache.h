#pragma once

#include "stdafx.h"

#include "CDDAMedia.h"

#include <map>
#include <queue>

// CDDA cache, to facilitate quick access to previously read sectors (used for crossfading).
class CDDACache
{
public:
	// Gets CD audio 'data' for the 'sector' index, returning whether the sector data was retrieved.
	bool GetData( const long sector, CDDAMedia::Data& data );

	// Caches the CD audio 'data' for the 'sector' index.
	void SetData( const long sector, const CDDAMedia::Data& data );

private:
	// Returns whether to limit memory usage.
	bool LimitMemoryUsage();

	// CD audio data, mapped by sector.
	std::map<long, CDDAMedia::Data> m_Cache;

	// Cached sector indices.
	std::queue<long> m_CachedSectors;

	// Cache limiter.
	size_t m_MaxCachedSectors = 0;
			
	// Cache mutex.
	std::mutex m_Mutex;
};
