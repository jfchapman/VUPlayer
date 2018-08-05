#include "CDDAManager.h"

#include "HandlerCDDA.h"
#include "VUPlayer.h"

#include <vector>

CDDAManager::CDDAManager( const HINSTANCE instance, const HWND hwnd, Library& library, Handlers& handlers ) :
	m_Library( library ),
	m_Media(),
	m_MediaMutex(),
	m_UpdateThread( nullptr ),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, FALSE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_hInst( instance ),
	m_hWnd( hwnd )
{
	handlers.AddHandler( Handler::Ptr( new HandlerCDDA( m_hInst, *this ) ) );
	StartUpdateThread();
}

CDDAManager::~CDDAManager()
{
	StopUpdateThread();
	CloseHandle( m_StopEvent );
	CloseHandle( m_WakeEvent );
}

CDDAManager::CDDAMediaMap CDDAManager::GetCDDADrives()
{
	std::lock_guard<std::mutex> lock( m_MediaMutex );
	return m_Media;
}

void CDDAManager::OnDeviceChange()
{
	SetEvent( m_WakeEvent );
}

void CDDAManager::StartUpdateThread()
{
	StopUpdateThread();
	ResetEvent( m_StopEvent );
	SetEvent( m_WakeEvent );
	m_UpdateThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, UpdateThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
}

void CDDAManager::StopUpdateThread()
{
	if ( nullptr != m_UpdateThread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_UpdateThread, INFINITE );
		m_UpdateThread = nullptr;
	}
}

DWORD WINAPI CDDAManager::UpdateThreadProc( LPVOID lParam )
{
	CDDAManager* manager = static_cast<CDDAManager*>( lParam );
	if ( nullptr != manager ) {
		manager->UpdateThreadHandler();
	}
	return 0;
}

void CDDAManager::UpdateThreadHandler()
{
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		bool updated = false;
		const std::set<wchar_t> drives = GetCDROMDrives();
		{
			// Deal with a device being removed.
			std::set<wchar_t> drivesToRemove;
			for ( const auto& media : m_Media ) {
				const wchar_t drive = media.first;
				if ( drives.end() == drives.find( drive ) ) {
					drivesToRemove.insert( drive );
				}
			}
			for ( const auto& drive : drivesToRemove ) {
				m_Media.erase( drive );
			}
		}

		// Deal with any devices or media being added/removed.
		for ( const wchar_t drive : drives ) {
			try {
				CDDAMedia cddaMedia( drive, m_Library );
				std::lock_guard<std::mutex> lock( m_MediaMutex );
				const auto mediaIter = m_Media.find( drive );
				if ( ( m_Media.end() != mediaIter ) && ( mediaIter->second.GetCDDB() != cddaMedia.GetCDDB() ) ) {
					m_Media.erase( mediaIter );
				}
				updated = m_Media.insert( CDDAMediaMap::value_type( drive, cddaMedia ) ).second;
			} catch ( const std::runtime_error& /*exception*/ ) {
				std::lock_guard<std::mutex> lock( m_MediaMutex );
				updated = ( m_Media.erase( drive ) > 0 );
			}
		}
		if ( updated ) {
			PostMessage( m_hWnd, MSG_CDDAREFRESHED, 0 /*wParam*/, 0 /*lParam*/ );
		}
	}
}

std::set<wchar_t> CDDAManager::GetCDROMDrives() const
{
	std::set<wchar_t> drives;
	const DWORD bufferLength = GetLogicalDriveStrings( 0 /*bufferLength*/, nullptr /*buffer*/ );
	if ( bufferLength > 0 ) {
		std::vector<WCHAR> buffer( static_cast<size_t>( bufferLength ) );
		if ( GetLogicalDriveStrings( bufferLength, &buffer[ 0 ] ) <= bufferLength ) {
			LPWSTR driveString = &buffer[ 0 ];
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
