#include "WndStatus.h"

#include "resource.h"

#include "Utility.h"

#include <sstream>
#include <vector>

// Status bar control ID
static const UINT_PTR s_WndStatusID = 1250;

// Number of status bar parts.
static const int s_PartCount = 4;

// Status bar part width.
static const int s_PartWidth = 120;

// File added message ID.
static const UINT MSG_UPDATESTATUS = WM_APP + 147;

LRESULT CALLBACK WndStatus::StatusProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndStatus* wndStatus = reinterpret_cast<WndStatus*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndStatus ) {
		switch ( message ) {
			case WM_SIZE: {
				if ( SIZE_MINIMIZED != wParam ) {
					wndStatus->Resize( static_cast<int>( LOWORD( lParam ) ) );
				}
				break;
			}
			case WM_DESTROY: {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndStatus->GetDefaultWndProc() ) );
				SetWindowLongPtr( hwnd, DWLP_USER, 0 );
				break;
			}
			case MSG_UPDATESTATUS: {
				wndStatus->Refresh();
				break;
			}
		}
	}
	return CallWindowProc( wndStatus->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndStatus::WndStatus( HINSTANCE instance, HWND parent ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL ),
	m_Playlist(),
	m_GainStatusCount( -1 ),
	m_MaintainerStatus(),
	m_MusicBrainzActive( false )
{
	const DWORD style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SBARS_SIZEGRIP;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 0;
	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( 0, STATUSCLASSNAME, 0, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndStatusID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( StatusProc ) ) );

	RECT rect;
	GetWindowRect( m_hWnd, &rect );
	const int initialWidth = rect.right - rect.left;
	Resize( initialWidth );
}

WndStatus::~WndStatus()
{
}

WNDPROC WndStatus::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndStatus::GetWindowHandle()
{
	return m_hWnd;
}

void WndStatus::Resize( const int width )
{
	RECT rect = {};
	GetClientRect( m_hWnd, &rect );
	const int gripWidth = static_cast<int>( rect.bottom - rect.top );
	const int rightmost = width - gripWidth;
	const int partWidth = static_cast<int>( GetDPIScaling() * s_PartWidth );
	int parts[ s_PartCount ] = { rightmost - 3 * partWidth, rightmost - 2 * partWidth, rightmost - partWidth, rightmost };
	SendMessage( m_hWnd, SB_SETPARTS, s_PartCount, reinterpret_cast<LPARAM>( parts ) );
}

void WndStatus::SetPlaylist( const Playlist::Ptr playlist )
{
	if ( m_Playlist != playlist ) {
		m_Playlist = playlist;
		Refresh();
	}
}

void WndStatus::Update( Playlist* playlist )
{
	if ( playlist == m_Playlist.get() ) {
		PostMessage( m_hWnd, MSG_UPDATESTATUS, 0, 0 );
	}
}

void WndStatus::Update( const GainCalculator& gainCalculator, const LibraryMaintainer& libraryMaintainer, const MusicBrainz& musicbrainz )
{
	const int pendingGain = gainCalculator.GetPendingCount();
	const std::wstring maintainerStatus = libraryMaintainer.GetStatus();
	const bool musicbrainzActive = musicbrainz.IsActive();
	if ( ( pendingGain != m_GainStatusCount ) || ( maintainerStatus != m_MaintainerStatus ) || ( musicbrainzActive != m_MusicBrainzActive ) ) {
		std::wstring statusText;
		if ( musicbrainzActive ) {
			const int bufSize = 64;
			WCHAR buf[ bufSize ];
			LoadString( m_hInst, IDS_MUSICBRAINZ_ACTIVE, buf, bufSize );
			statusText = buf;
		} else if ( !maintainerStatus.empty() ) {
			statusText = maintainerStatus;
		} else if ( 0 != pendingGain ) {
			const int bufSize = 64;
			WCHAR buf[ bufSize ];
			LoadString( m_hInst, IDS_STATUS_GAIN, buf, bufSize );
			statusText = buf;
			const size_t pos = statusText.find( '%' );
			if ( std::wstring::npos != pos ) {
				statusText.replace( pos, 1 /*len*/, std::to_wstring( pendingGain ) );
			}
		}
		SendMessage( m_hWnd, SB_SETTEXT, 0, reinterpret_cast<LPARAM>( statusText.c_str() ) );
		m_GainStatusCount = pendingGain;
		m_MaintainerStatus = maintainerStatus;
		m_MusicBrainzActive = musicbrainzActive;
	}
}

void WndStatus::Refresh()
{
	std::wstring part2;
	std::wstring part3;
	std::wstring part4;

	if ( m_Playlist ) {
		const long trackCount = m_Playlist->GetCount();
		if ( trackCount > 0 ) {
			std::wstringstream ss;
			const int bufSize = 16;
			WCHAR buf[ bufSize ] = {};
			LoadString( m_hInst, ( trackCount == 1 ) ? IDS_STATUS_TRACK : IDS_STATUS_TRACKS, buf, bufSize );
			ss << L"\t" << trackCount << L" " << buf;
			part2 = ss.str();
		}
	}

	if ( m_Playlist ) {
		const long long filesize = m_Playlist->GetFilesize();
		part3 = L"\t" + FilesizeToString( m_hInst, filesize );
	}

	if ( m_Playlist ) {
		const float duration = m_Playlist->GetDuration();
		if ( duration > 0 ) {
			part4 = L"\t" + DurationToString( m_hInst, duration, false /*colonDelimited*/ );
		}
	}

	std::wstring previous1;
	std::wstring previous2;
	std::wstring previous3;
	std::wstring previous4;
	const int part1Size = LOWORD( SendMessage( m_hWnd, SB_GETTEXTLENGTH, 0, 0 ) );
	if ( part1Size > 0 ) {
		std::vector<WCHAR> buf( 1 + part1Size, 0 );
		SendMessage( m_hWnd, SB_GETTEXT, 0, reinterpret_cast<LPARAM>( buf.data() ) );
		previous1 = buf.data();
	}
	const int part2Size = LOWORD( SendMessage( m_hWnd, SB_GETTEXTLENGTH, 1, 0 ) );
	if ( part2Size > 0 ) {
		std::vector<WCHAR> buf( 1 + part2Size, 0 );
		SendMessage( m_hWnd, SB_GETTEXT, 1, reinterpret_cast<LPARAM>( buf.data() ) );
		previous2 = buf.data();
	}
	const int part3Size = LOWORD( SendMessage( m_hWnd, SB_GETTEXTLENGTH, 2, 0 ) );
	if ( part3Size > 0 ) {
		std::vector<WCHAR> buf( 1 + part3Size, 0 );
		SendMessage( m_hWnd, SB_GETTEXT, 2, reinterpret_cast<LPARAM>( buf.data() ) );
		previous3 = buf.data();
	}
	const int part4Size = LOWORD( SendMessage( m_hWnd, SB_GETTEXTLENGTH, 3, 0 ) );
	if ( part4Size > 0 ) {
		std::vector<WCHAR> buf( 1 + part4Size, 0 );
		SendMessage( m_hWnd, SB_GETTEXT, 3, reinterpret_cast<LPARAM>( buf.data() ) );
		previous4 = buf.data();
	}

	if ( previous2 != part2 ) {
		SendMessage( m_hWnd, SB_SETTEXT, 1, reinterpret_cast<LPARAM>( part2.c_str() ) );
	}
	if ( previous3 != part3 ) {
		SendMessage( m_hWnd, SB_SETTEXT, 2, reinterpret_cast<LPARAM>( part3.c_str() ) );
	}
	if ( previous4 != part4 ) {
		SendMessage( m_hWnd, SB_SETTEXT, 3, reinterpret_cast<LPARAM>( part4.c_str() ) );
	}
}
