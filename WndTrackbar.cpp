#include "WndTrackbar.h"

#include "resource.h"
#include "Utility.h"

// Trackbar control ID
UINT_PTR WndTrackbar::s_WndTrackbarID = 1600;

// Trackbar width
static const int s_TrackbarWidth = 127;

// Trackbar height
static const int s_TrackbarHeight = 26;

LRESULT CALLBACK WndTrackbar::TrackbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTrackbar* wndTrackbar = reinterpret_cast<WndTrackbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTrackbar ) {
		switch ( message ) {
			case WM_NOTIFY : {
				LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != nmhdr ) && ( nmhdr->code == TTN_GETDISPINFO ) ) {
					LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
					info->hinst = 0;
					info->lpszText = const_cast<LPWSTR>( wndTrackbar->GetTooltipText().c_str() );
				}
				break;
			}
			case WM_HSCROLL : {
				const LPARAM change = LOWORD( wParam );
				const int position = static_cast<int>( SendMessage( hwnd, TBM_GETPOS, 0, 0 ) );		
				if ( TB_THUMBTRACK == change ) {
					wndTrackbar->OnDrag( position );
				} else if ( TB_ENDTRACK != change ) {
					wndTrackbar->OnPositionChanged( position );
				}
				const HWND hwndToolTip = reinterpret_cast<HWND>( SendMessage( hwnd, TBM_GETTOOLTIPS, 0, 0 ) );
				if ( nullptr != hwndToolTip ) {
					SendMessage( hwndToolTip, TTM_UPDATE, 0, 0 );
				}
				break;
			}
			case WM_CONTEXTMENU : {
				POINT pt = {};
				pt.x = LOWORD( lParam );
				pt.y = HIWORD( lParam );
				wndTrackbar->OnContextMenu( pt );
				break;
			}
			case WM_SIZE : {
				const HWND hwndToolTip = reinterpret_cast<HWND>( SendMessage( hwnd, TBM_GETTOOLTIPS, 0, 0 ) );
				if ( nullptr != hwndToolTip ) {
					const int width = static_cast<int>( LOWORD( lParam ) );
					const int height = static_cast<int>( HIWORD( lParam ) );
					TOOLINFO toolInfo = {};
					toolInfo.cbSize = sizeof( TOOLINFO );
					toolInfo.hwnd = hwnd;
					toolInfo.uId = GetWindowLongPtr( hwndToolTip, GWLP_USERDATA );
					if ( TRUE == SendMessage( hwndToolTip, TTM_GETTOOLINFO, 0, reinterpret_cast<LPARAM>( &toolInfo ) ) ) {
						toolInfo.rect.left = 0;
						toolInfo.rect.top = 0;
						toolInfo.rect.right = width;
						toolInfo.rect.bottom = height;
						SendMessage( hwndToolTip, TTM_NEWTOOLRECT, 0, reinterpret_cast<LPARAM>( &toolInfo ) );
					}
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndTrackbar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndTrackbar::WndTrackbar( HINSTANCE instance, HWND parent, Output& output, const int minValue, const int maxValue, const Type type ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL ),
	m_Output( output ),
	m_Type( type )
{
	DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_DOWNISLEFT | TBS_NOTICKS | TBS_TRANSPARENTBKGND | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN;
	const int x = 0;
	const int y = 0;
	const float dpiScaling = GetDPIScaling();
	const int width = static_cast<int>( s_TrackbarWidth * dpiScaling );
	const int height = static_cast<int>( s_TrackbarHeight * dpiScaling );
	LPVOID param = NULL;

	m_hWnd = CreateWindowEx( 0, TRACKBAR_CLASS, 0, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndTrackbarID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( TrackbarProc ) ) );
	
	SendMessage( m_hWnd, TBM_SETRANGEMIN, 0 /*redraw*/, static_cast<LPARAM>( minValue ) );
	SendMessage( m_hWnd, TBM_SETRANGEMAX, 0 /*redraw*/, static_cast<LPARAM>( maxValue ) );
	SendMessage( m_hWnd, TBM_SETPAGESIZE, 0 /*redraw*/, static_cast<LPARAM>( ( maxValue - minValue ) / 10 ) );

	HWND hwndToolTip = CreateWindowEx( WS_EX_TOPMOST, TOOLTIPS_CLASS, 0, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, m_hWnd, 0 /*menu*/, m_hInst, 0 /*param*/ );
	SetWindowLongPtr( hwndToolTip, GWLP_USERDATA, s_WndTrackbarID );
	TOOLINFO toolInfo = {};
	toolInfo.cbSize = sizeof( TOOLINFO );
	toolInfo.hwnd = m_hWnd;
	toolInfo.hinst = m_hInst;
	toolInfo.uFlags = TTF_SUBCLASS;
	toolInfo.uId = s_WndTrackbarID++;
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	GetClientRect( m_hWnd, &toolInfo.rect );
	SendMessage( hwndToolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>( &toolInfo ) );
	SendMessage( m_hWnd, TBM_SETTOOLTIPS, reinterpret_cast<LPARAM>( hwndToolTip ), 0 );
	SendMessage( m_hWnd, WM_UPDATEUISTATE, MAKEWPARAM( UIS_SET, UISF_HIDEFOCUS ), 0 /*lParam*/ );
}

WndTrackbar::~WndTrackbar()
{
	SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_DefaultWndProc ) );
}

WNDPROC WndTrackbar::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndTrackbar::GetWindowHandle()
{
	return m_hWnd;
}

int WndTrackbar::GetPosition() const
{
	const int pos = static_cast<int>( SendMessage( m_hWnd, TBM_GETPOS, 0, 0 ) );
	return pos;
}

void WndTrackbar::SetPosition( const int position )
{
	const int currentPos = GetPosition();
	if ( currentPos != position ) {
		SendMessage( m_hWnd, TBM_SETPOS, TRUE /*redraw*/, position );
		const HWND hwndToolTip = reinterpret_cast<HWND>( SendMessage( m_hWnd, TBM_GETTOOLTIPS, 0, 0 ) );
		if ( nullptr != hwndToolTip ) {
			SendMessage( hwndToolTip, TTM_UPDATE, 0, 0 );
		}
	}
}

int WndTrackbar::GetRange() const
{
	const int min = static_cast<int>( SendMessage( m_hWnd, TBM_GETRANGEMIN, 0, 0 ) );
	const int max = static_cast<int>( SendMessage( m_hWnd, TBM_GETRANGEMAX, 0, 0 ) );
	const int range = max - min;
	return range;
}

HINSTANCE WndTrackbar::GetInstanceHandle()
{
	return m_hInst;
}

Output& WndTrackbar::GetOutput()
{
	return m_Output;
}

const Output& WndTrackbar::GetOutput() const
{
	return m_Output;
}

bool WndTrackbar::GetEnabled() const
{
	const bool enabled = ( TRUE == IsWindowEnabled( m_hWnd ) );
	return enabled;
}

void WndTrackbar::SetEnabled( const bool enabled )
{
	const bool isEnabled = GetEnabled();
	if ( isEnabled != enabled ) {
		EnableWindow( m_hWnd, enabled ? TRUE : FALSE );
	}
}

void WndTrackbar::SetVisible( const bool visible )
{
	ShowWindow( m_hWnd, visible ? SW_SHOW : SW_HIDE );
}

WndTrackbar::Type WndTrackbar::GetType() const
{
	return m_Type;
}

void WndTrackbar::SetType( const Type type )
{
	m_Type = type;
}

void WndTrackbar::OnContextMenu( const POINT& /*position*/ )
{
	// Does nothing in the base class
}
