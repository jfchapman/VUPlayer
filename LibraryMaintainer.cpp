#include "LibraryMaintainer.h"

#include "VUPlayer.h"

DWORD WINAPI LibraryMaintainer::MaintainerThreadProc( LPVOID lpParam )
{
	LibraryMaintainer* maintainer = static_cast<LibraryMaintainer*>( lpParam );
	if ( nullptr != maintainer ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		maintainer->Handler();
		CoUninitialize();
	}
	return 0;
}

LibraryMaintainer::LibraryMaintainer( Library& library ) :
	m_Library( library ),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Thread( nullptr ),
	m_PendingCount( {} )
{
}

LibraryMaintainer::~LibraryMaintainer()
{
	Stop();
	CloseHandle( m_StopEvent );
}

void LibraryMaintainer::Start()
{
	Stop();
	m_Thread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, MaintainerThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	if ( nullptr != m_Thread ) {
		SetThreadPriority( m_Thread, THREAD_PRIORITY_LOWEST );
	}
}

void LibraryMaintainer::Stop()
{
	if ( nullptr != m_Thread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_Thread, INFINITE );
		CloseHandle( m_Thread );
		m_Thread = nullptr;
	}
	ResetEvent( m_StopEvent );
	m_PendingCount = 0;
}

void LibraryMaintainer::Handler()
{
	const MediaInfo::List allMedia = m_Library.GetAllMedia();
	int updatedCount = 0;
	m_PendingCount = static_cast<int>( allMedia.size() );
	auto mediaInfo = allMedia.begin();
	while ( ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) && ( allMedia.end() != mediaInfo ) ) {
		const MediaInfo previousInfo( *mediaInfo );
		MediaInfo currentInfo( *mediaInfo );
		if ( !m_Library.GetMediaInfo( currentInfo, true /*checkFileAttributes*/, true /*scanMedia*/, false /*sendNotification*/, true /*removeMissing*/ ) ) {
			currentInfo = MediaInfo();
		}
		if ( ( previousInfo.GetFiletime() != currentInfo.GetFiletime() ) ) {
			++updatedCount;
		}
		--m_PendingCount;
		++mediaInfo;
	}

	if ( ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) && ( updatedCount > 0 ) ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			vuplayer->OnLibraryRefreshed();
		}
	}
}

int LibraryMaintainer::GetPendingCount() const
{
	return m_PendingCount.load();
}
