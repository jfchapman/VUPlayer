#include "DiscManager.h"

#include "HandlerCDDA.h"
#include "VUPlayer.h"

#include <vector>

DiscManager::DiscManager( const HINSTANCE instance, const HWND hwnd, Library& library, Handlers& handlers, MusicBrainz& musicbrainz ) :
	m_Library( library ),
	m_MusicBrainz( musicbrainz ),
	m_UpdateThread( nullptr ),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_hInst( instance ),
	m_hWnd( hwnd )
{
	handlers.AddHandler( Handler::Ptr( new HandlerCDDA( m_hInst, *this ) ) );
	StartUpdateThread();
}

DiscManager::~DiscManager()
{
	StopUpdateThread();
	CloseHandle( m_StopEvent );
	CloseHandle( m_WakeEvent );
}

DiscManager::CDDAMediaMap DiscManager::GetCDDADrives() const
{
	std::lock_guard<std::mutex> lock( m_MediaMutex );
	return m_CDDAMedia;
}

DiscManager::DataMediaMap DiscManager::GetDataDrives() const
{
	std::lock_guard<std::mutex> lock( m_MediaMutex );
	return m_DataMedia;
}

void DiscManager::OnDeviceChange( const wchar_t drive, const ChangeType change )
{
	std::lock_guard<std::mutex> lock( m_MediaMutex );
	m_ChangeMap[ drive ] = change;
	SetEvent( m_WakeEvent );
}

void DiscManager::StartUpdateThread()
{
	StopUpdateThread();
	ResetEvent( m_StopEvent );
	SetEvent( m_WakeEvent );
	m_UpdateThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, UpdateThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
}

void DiscManager::StopUpdateThread()
{
	if ( nullptr != m_UpdateThread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_UpdateThread, INFINITE );
		m_UpdateThread = nullptr;
	}
}

DWORD WINAPI DiscManager::UpdateThreadProc( LPVOID lParam )
{
	if ( DiscManager* manager = static_cast<DiscManager*>( lParam ); nullptr != manager ) {
		manager->UpdateThreadHandler();
	}
	return 0;
}

void DiscManager::UpdateThreadHandler()
{
	// Startup scan.
	auto allDrives = GetCDROMDrives();
	for ( const wchar_t drive : allDrives ) {
		if ( const auto cddaMedia = GetCDDAMedia( drive ); cddaMedia ) {
			std::lock_guard<std::mutex> lock( m_MediaMutex );
			m_CDDAMedia.emplace( CDDAMediaMap::value_type( drive, *cddaMedia ) );
		}
		if ( const auto dataMedia = GetDataMedia( drive ); dataMedia ) {
			std::lock_guard<std::mutex> lock( m_MediaMutex );
			m_DataMedia.emplace( DataMediaMap::value_type( drive, *dataMedia ) );
		}

		if ( WaitForSingleObject( m_StopEvent, 0 ) == WAIT_OBJECT_0 ) {
			break;
		}
	}

	if ( !m_CDDAMedia.empty() || !m_DataMedia.empty() ) {
		PostMessage( m_hWnd, MSG_DISCREFRESHED, 0, 0 );
	}

	// Change handler.
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		std::optional<std::pair<wchar_t, ChangeType>> change;
		{
			std::lock_guard<std::mutex> lock( m_MediaMutex );
			if ( const auto entry = m_ChangeMap.begin(); m_ChangeMap.end() != entry ) {
				change = std::make_optional<std::pair<wchar_t, ChangeType>>( entry->first, entry->second );
				m_ChangeMap.erase( entry );
			} else {
				ResetEvent( m_WakeEvent );
			}
		}

		if ( change ) {
			bool updated = true;
			const auto& [ drive, changeType ] = *change;
			switch ( changeType ) {
				case ChangeType::Added : {
					{
						std::lock_guard<std::mutex> lock( m_MediaMutex );
						m_CDDAMedia.erase( drive );
						m_DataMedia.erase( drive );
					}
					if ( const auto cddaMedia = GetCDDAMedia( drive ); cddaMedia ) {
						std::lock_guard<std::mutex> lock( m_MediaMutex );
						m_CDDAMedia.emplace( CDDAMediaMap::value_type( drive, *cddaMedia ) );
					}
					if ( const auto dataMedia = GetDataMedia( drive ); dataMedia ) {
						std::lock_guard<std::mutex> lock( m_MediaMutex );
						m_DataMedia.emplace( DataMediaMap::value_type( drive, *dataMedia ) );
					}
					break;
				}
				case ChangeType::Removed : {
					std::lock_guard<std::mutex> lock( m_MediaMutex );
					m_CDDAMedia.erase( drive );
					m_DataMedia.erase( drive );
					break;
				}
				default : {
					updated = false;
					break;
				}
			}
			if ( updated ) {
				PostMessage( m_hWnd, MSG_DISCREFRESHED, 0, 0 );
			}
		}
	}
}

std::set<wchar_t> DiscManager::GetCDROMDrives()
{
	std::set<wchar_t> drives;
	const DWORD bufferLength = GetLogicalDriveStrings( 0 /*bufferLength*/, nullptr /*buffer*/ );
	if ( bufferLength > 0 ) {
		std::vector<WCHAR> buffer( static_cast<size_t>( bufferLength ) );
		if ( GetLogicalDriveStrings( bufferLength, buffer.data() ) <= bufferLength ) {
			LPWSTR driveString = buffer.data();
			size_t stringLength = wcslen( driveString );
			while ( stringLength > 0 ) {
				const UINT driveType = GetDriveType( driveString );
				if ( DRIVE_CDROM == driveType ) {
					drives.insert( driveString[ 0 ] );
				}
				driveString += stringLength + 1;
				stringLength = _tcslen( driveString );
			}
		}
	}
	return drives;
}

std::optional<CDDAMedia> DiscManager::GetCDDAMedia( const wchar_t drive ) const
{
	try {
		return std::make_optional<CDDAMedia>( drive, m_Library, m_MusicBrainz );
	} catch ( const std::runtime_error& ) {
		return std::nullopt;
	}
}

std::optional<std::wstring> DiscManager::GetDataMedia( const wchar_t drive ) const
{
	std::optional<std::wstring> name;
	const std::wstring driveString = std::wstring( 1, drive ) + L":\\";
	const DWORD attributes = GetFileAttributes( driveString.c_str() );
	if ( ( INVALID_FILE_ATTRIBUTES != attributes ) && CDDAMedia::ContainsData( drive ) ) {
		WCHAR nameBuffer[ MAX_PATH + 1 ] = {};
		if ( GetVolumeInformation( driveString.c_str(), nameBuffer, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0 ) && ( wcslen( nameBuffer ) > 0 ) ) {
			name = nameBuffer;
		}
	}
	return name;
}
