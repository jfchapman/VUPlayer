#include "WndTrackbar.h"

#include "Utility.h"

// Trackbar control ID
UINT_PTR WndTrackbar::s_WndTrackbarID = 1600;

// Trackbar width
static const int s_TrackbarWidth = 127;

// Trackbar height
static const int s_TrackbarHeight = 26;

// Window procedure
static LRESULT CALLBACK WndTrackbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTrackbar* wndTrackbar = reinterpret_cast<WndTrackbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTrackbar ) {
		switch ( message ) {
			case WM_NOTIFY : {
				LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
				if( ( nullptr != nmhdr ) && ( nmhdr->code == TTN_GETDISPINFO ) ) {
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
				break;
			}
			case WM_COMMAND : {
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndTrackbar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndTrackbar::WndTrackbar( HINSTANCE instance, HWND parent, Output& output, const int minValue, const int maxValue ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL ),
	m_Output( output )
{
	const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_DOWNISLEFT | TBS_NOTICKS | TBS_TOOLTIPS | TBS_TRANSPARENTBKGND | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN;
	const int x = 0;
	const int y = 0;
	const float dpiScaling = GetDPIScaling();
	const int width = static_cast<int>( s_TrackbarWidth * dpiScaling );
	const int height = static_cast<int>( s_TrackbarHeight * dpiScaling );
	LPVOID param = NULL;

	m_hWnd = CreateWindowEx( 0, TRACKBAR_CLASS, 0, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndTrackbarID++ ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( WndTrackbarProc ) ) );
	
	SendMessage( m_hWnd, TBM_SETRANGEMIN, 0 /*redraw*/, static_cast<LPARAM>( minValue ) );
	SendMessage( m_hWnd, TBM_SETRANGEMAX, 0 /*redraw*/, static_cast<LPARAM>( maxValue ) );
	SendMessage( m_hWnd, TBM_SETPAGESIZE, 0 /*redraw*/, static_cast<LPARAM>( ( maxValue - minValue ) / 10 ) );
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
