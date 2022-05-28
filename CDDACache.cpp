#include "CDDACache.h"

#include <Psapi.h>

bool CDDACache::GetData( const long sector, CDDAMedia::Data& data )
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	const auto iter = m_Cache.find( sector );
	const bool success = ( m_Cache.end() != iter );
	if ( success ) {
		data = iter->second;
	}
	return success;
}

void CDDACache::SetData( const long sector, const CDDAMedia::Data& data )
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	if ( m_Cache.emplace( std::map<long, CDDAMedia::Data>::value_type( sector, data ) ).second ) {
		m_CachedSectors.push( sector );
		if ( LimitMemoryUsage() ) {
			const long sectorToRemove = m_CachedSectors.front();
			m_CachedSectors.pop();
			m_Cache.erase( sectorToRemove );
		}
	}
}

bool CDDACache::LimitMemoryUsage()
{
	constexpr size_t kAbsoluteLimit = ( UINTPTR_MAX == UINT_MAX ) ? ( UINT_MAX / 4 ) : UINT_MAX;
	constexpr size_t kPercentOfAvailable = 25;

	PROCESS_MEMORY_COUNTERS memProcess = {};
	memProcess.cb = sizeof( PROCESS_MEMORY_COUNTERS );

	MEMORYSTATUSEX memSystem = {};
	memSystem.dwLength = sizeof( MEMORYSTATUSEX );

	bool limitMemoryUsage = true;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), &memProcess, memProcess.cb ) && GlobalMemoryStatusEx( &memSystem ) ) {
		limitMemoryUsage = ( memProcess.WorkingSetSize > kAbsoluteLimit ) || ( memProcess.WorkingSetSize > ( memSystem.ullAvailPhys * kPercentOfAvailable / 100 ) );
	}
	return limitMemoryUsage;
}
