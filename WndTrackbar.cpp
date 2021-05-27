#include "WndTrackbar.h"

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"
#include "windowsx.h"

// Trackbar control ID
UINT_PTR WndTrackbar::s_WndTrackbarID = 1600;

// Trackbar host window class name
static const wchar_t s_TrackbarHostClass[] = L"VUTrackbarHostClass";

// Trackbar height
static const int s_TrackbarHeight = 26;

LRESULT CALLBACK WndTrackbar::TrackbarHostProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTrackbar* wndTrackbar = reinterpret_cast<WndTrackbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTrackbar ) {
		switch ( message ) {
			case WM_PAINT : {
				PAINTSTRUCT ps = {};
				BeginPaint( hwnd, &ps );
				wndTrackbar->OnPaint( ps );
				EndPaint( hwnd, &ps );
				break;
			}
			case WM_NOTIFY : {
				if ( LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam ); nullptr != nmhdr ) {
					if ( NM_CUSTOMDRAW == nmhdr->code ) {
						LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>( lParam );
						if ( const auto result = wndTrackbar->OnCustomDraw( nmcd ); result.has_value() ) {
							return result.value();
						}
					}
				}
				break;
			}
			case WM_HSCROLL : {
				SendMessage( reinterpret_cast<HWND>( lParam ) /*trackbarWnd*/, message, wParam, lParam );
				break;
			}
			case WM_PRINTCLIENT : {
				wndTrackbar->OnEraseTrackbarBackground( HDC( wParam ) );
				return TRUE;
			}
			case WM_SIZE : {
				wndTrackbar->OnSize();
				break;
			}
			case WM_ERASEBKGND : {
				return TRUE;
			}
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, DWLP_USER, 0 );
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

LRESULT CALLBACK WndTrackbar::TrackbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTrackbar* wndTrackbar = reinterpret_cast<WndTrackbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTrackbar ) {
		switch ( message ) {
			case WM_NOTIFY : {
				if ( LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam ); nullptr != nmhdr ) {
					if ( nmhdr->code == TTN_GETDISPINFO ) {
						LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
						info->hinst = 0;
						info->lpszText = const_cast<LPWSTR>( wndTrackbar->GetTooltipText().c_str() );
					}
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
			case WM_ERASEBKGND : {
				return TRUE;
			}
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndTrackbar->GetDefaultWndProc() ) );
				SetWindowLongPtr( hwnd, DWLP_USER, 0 );
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndTrackbar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndTrackbar::WndTrackbar( HINSTANCE instance, HWND parent, Output& output, Settings& settings, const int minValue, const int maxValue, const Type type ) :
	m_hInst( instance ),
	m_hHostWnd( nullptr ),
	m_hTrackbarWnd( nullptr ),
	m_DefaultWndProc( nullptr ),
	m_Output( output ),
	m_Settings( settings ),
	m_Type( type ),
	m_ThumbColour( DEFAULT_ICONCOLOUR ),
	m_BackgroundColour( GetSysColor( COLOR_WINDOW ) ),
	m_IsHighContrast( IsHighContrastActive() ),
	m_IsClassicTheme( IsClassicThemeActive() ),
	m_IsWindows10( IsWindows10() )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hInstance = instance;
	wc.lpfnWndProc = TrackbarHostProc;
	wc.lpszClassName = s_TrackbarHostClass;
	RegisterClassEx( &wc );

	DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
	m_hHostWnd = CreateWindowEx( 0, s_TrackbarHostClass, NULL, style, 0, 0, 0, 0, parent, reinterpret_cast<HMENU>( s_WndTrackbarID++ ), instance, nullptr );
	SetWindowLongPtr( m_hHostWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

	settings.GetToolbarColours( m_ThumbColour, m_BackgroundColour );
	style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_DOWNISLEFT | TBS_NOTICKS | TBS_BOTH | TBS_TRANSPARENTBKGND;

	m_hTrackbarWnd = CreateWindowEx( 0, TRACKBAR_CLASS, 0, style, 0 /*x*/, 0 /*y*/, 0 /*width*/, 0 /*height*/, m_hHostWnd, reinterpret_cast<HMENU>( s_WndTrackbarID++ ), instance, nullptr );
	SetWindowLongPtr( m_hTrackbarWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hTrackbarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( TrackbarProc ) ) );
	
	SendMessage( m_hTrackbarWnd, TBM_SETRANGEMIN, 0 /*redraw*/, static_cast<LPARAM>( minValue ) );
	SendMessage( m_hTrackbarWnd, TBM_SETRANGEMAX, 0 /*redraw*/, static_cast<LPARAM>( maxValue ) );
	SendMessage( m_hTrackbarWnd, TBM_SETPAGESIZE, 0 /*redraw*/, static_cast<LPARAM>( ( maxValue - minValue ) / 10 ) );

	HWND hwndToolTip = CreateWindowEx( WS_EX_TOPMOST, TOOLTIPS_CLASS, 0, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, m_hTrackbarWnd, 0 /*menu*/, m_hInst, 0 /*param*/ );
	SetWindowLongPtr( hwndToolTip, GWLP_USERDATA, s_WndTrackbarID );
	TOOLINFO toolInfo = {};
	toolInfo.cbSize = sizeof( TOOLINFO );
	toolInfo.hwnd = m_hTrackbarWnd;
	toolInfo.hinst = m_hInst;
	toolInfo.uFlags = TTF_SUBCLASS;
	toolInfo.uId = s_WndTrackbarID++;
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	GetClientRect( m_hTrackbarWnd, &toolInfo.rect );
	SendMessage( hwndToolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>( &toolInfo ) );
	SendMessage( m_hTrackbarWnd, TBM_SETTOOLTIPS, reinterpret_cast<LPARAM>( hwndToolTip ), 0 );
	SendMessage( m_hTrackbarWnd, WM_UPDATEUISTATE, MAKEWPARAM( UIS_SET, UISF_HIDEFOCUS ), 0 /*lParam*/ );
}

WndTrackbar::~WndTrackbar()
{
}

WNDPROC WndTrackbar::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

int WndTrackbar::GetPosition() const
{
	const int pos = static_cast<int>( SendMessage( m_hTrackbarWnd, TBM_GETPOS, 0, 0 ) );
	return pos;
}

void WndTrackbar::SetPosition( const int position )
{
	const int currentPos = GetPosition();
	if ( currentPos != position ) {
		SendMessage( m_hTrackbarWnd, TBM_SETPOS, TRUE /*redraw*/, position );
		const HWND hwndToolTip = reinterpret_cast<HWND>( SendMessage( m_hTrackbarWnd, TBM_GETTOOLTIPS, 0, 0 ) );
		if ( nullptr != hwndToolTip ) {
			SendMessage( hwndToolTip, TTM_UPDATE, 0, 0 );
		}
	}
}

int WndTrackbar::GetRange() const
{
	const int min = static_cast<int>( SendMessage( m_hTrackbarWnd, TBM_GETRANGEMIN, 0, 0 ) );
	const int max = static_cast<int>( SendMessage( m_hTrackbarWnd, TBM_GETRANGEMAX, 0, 0 ) );
	const int range = max - min;
	return range;
}

HINSTANCE WndTrackbar::GetInstanceHandle() const
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

Settings& WndTrackbar::GetSettings()
{
	return m_Settings;
}

bool WndTrackbar::GetEnabled() const
{
	const bool enabled = ( TRUE == IsWindowEnabled( m_hTrackbarWnd ) );
	return enabled;
}

void WndTrackbar::SetEnabled( const bool enabled )
{
	const bool isEnabled = GetEnabled();
	if ( isEnabled != enabled ) {
		EnableWindow( m_hTrackbarWnd, enabled ? TRUE : FALSE );
	}
}

void WndTrackbar::SetVisible( const bool visible )
{
	ShowWindow( m_hTrackbarWnd, visible ? SW_SHOW : SW_HIDE );
}

WndTrackbar::Type WndTrackbar::GetType() const
{
	return m_Type;
}

void WndTrackbar::SetType( const Type type )
{
	m_Type = type;
}

int WndTrackbar::GetID() const
{
	return 0;
}

HWND WndTrackbar::GetWindowHandle() const
{
	return m_hHostWnd;
}

std::optional<int> WndTrackbar::GetWidth() const
{
	return std::nullopt;
}

std::optional<int> WndTrackbar::GetHeight() const
{
	return std::nullopt;
}

bool WndTrackbar::HasDivider() const
{
	return true;
}

bool WndTrackbar::CanHide() const
{
	return false;
}

void WndTrackbar::OnChangeRebarItemSettings( Settings& settings )
{
	settings.GetToolbarColours( m_ThumbColour, m_BackgroundColour );
}

std::optional<LRESULT> WndTrackbar::OnCustomDraw( LPNMCUSTOMDRAW nmcd )
{
	std::optional<LRESULT> result;
	if ( m_IsWindows10 && !m_IsHighContrast ) {
		if ( CDDS_PREPAINT == nmcd->dwDrawStage ) {
			result = CDRF_NOTIFYITEMDRAW;
		} else if ( CDDS_ITEMPREPAINT == nmcd->dwDrawStage ) {
			if ( TBCD_THUMB == nmcd->dwItemSpec ) {
				Gdiplus::Graphics graphics( nmcd->hdc );
				Gdiplus::Color colour;
				colour.SetFromCOLORREF( GetThumbColour( nmcd->rc ) );
				Gdiplus::SolidBrush brush( colour );
				const int width = nmcd->rc.right - nmcd->rc.left;
				const int height = nmcd->rc.bottom - nmcd->rc.top;
				const int x = nmcd->rc.left;
				const int y = nmcd->rc.top;
				graphics.FillRectangle( &brush, x, y, width - 1, height - 1 );
				result = CDRF_SKIPDEFAULT;
			}
		}
	}
	return result;
}

bool WndTrackbar::ShowContextMenu( const POINT& /*position*/ )
{
	return true;
}

void WndTrackbar::OnSize()
{
	// Reposition the trackbar control within the host window.
	RECT rect = {};
	GetClientRect( m_hHostWnd, &rect );
	const int width = rect.right - rect.left;
	const int height = int( s_TrackbarHeight * GetDPIScaling() );
	const int x = 0;
	int y = rect.bottom - rect.top - height;
	y -= y / 2;
	SetWindowPos( m_hTrackbarWnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER );
}

void WndTrackbar::OnPaint( const PAINTSTRUCT& ps )
{
	RECT rect = {};
	GetWindowRect( m_hTrackbarWnd, &rect );
	MapWindowRect( nullptr, m_hHostWnd, &rect );
	ExcludeClipRect( ps.hdc, rect.left, rect.top, rect.right, rect.bottom );
	Gdiplus::Color backgroundColour;
	backgroundColour.SetFromCOLORREF( GetBackgroundColour() );
	Gdiplus::Graphics graphics( ps.hdc );
	graphics.Clear( backgroundColour );
}

void WndTrackbar::OnEraseTrackbarBackground( const HDC dc )
{
	Gdiplus::Color backgroundColour;
	backgroundColour.SetFromCOLORREF( GetBackgroundColour() );
	Gdiplus::Graphics graphics( dc );
	graphics.Clear( backgroundColour );
}

void WndTrackbar::OnSysColorChange( const bool isHighContrast, const bool isClassicTheme )
{
	m_IsHighContrast = isHighContrast;
	m_IsClassicTheme = isClassicTheme;
}

COLORREF WndTrackbar::GetBackgroundColour() const
{
	return m_IsHighContrast ? GetSysColor( COLOR_WINDOW ) : ( m_IsClassicTheme ? GetSysColor( COLOR_3DFACE ) : m_BackgroundColour );
}

COLORREF WndTrackbar::GetThumbColour( const RECT& thumbRect ) const
{
	COLORREF colour = m_ThumbColour;
	if ( IsWindowEnabled( m_hTrackbarWnd ) ) {
		if ( m_hTrackbarWnd == GetCapture() ) {
			colour = GetSysColor( COLOR_SCROLLBAR );
		} else {
			if ( POINT pt = {}; GetCursorPos( &pt ) && MapWindowPoints( nullptr, m_hTrackbarWnd, &pt, 1 ) ) {
				if ( PtInRect( &thumbRect, pt ) ) {
					colour = 0;
				}
			}
		}
	} else {
		colour = GetSysColor( COLOR_SCROLLBAR );
	}
	return colour;
}
