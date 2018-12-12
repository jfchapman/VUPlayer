#include "WndToolbar.h"

LRESULT CALLBACK WndToolbar::ToolbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndToolbar* wndToolbar = reinterpret_cast<WndToolbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndToolbar ) {
		switch ( message ) {
			case WM_NOTIFY: {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != hdr ) && ( TTN_GETDISPINFO == hdr->code ) ) {
					LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
					const UINT resourceID = wndToolbar->GetTooltip( static_cast<UINT>( info->hdr.idFrom ) );
					if ( resourceID > 0 ) {
						info->hinst = wndToolbar->GetInstanceHandle();
						info->lpszText = MAKEINTRESOURCE( resourceID );
					}
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndToolbar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndToolbar::WndToolbar( HINSTANCE instance, HWND parent, const long long id ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL )
{
	const DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TOOLTIPS;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 0;
	LPVOID param = NULL;

	m_hWnd = CreateWindowEx( 0, TOOLBARCLASSNAME, 0, style, x, y, width, height, parent, reinterpret_cast<HMENU>( id ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( ToolbarProc ) ) );
}

WndToolbar::~WndToolbar()
{
	SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_DefaultWndProc ) );
}

WNDPROC WndToolbar::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndToolbar::GetWindowHandle() const
{
	return m_hWnd;
}

HINSTANCE WndToolbar::GetInstanceHandle() const
{
	return m_hInst;
}

int WndToolbar::GetHeight() const
{
	RECT rect = {};
	GetWindowRect( m_hWnd, &rect );
	const int height = rect.bottom - rect.top;
	return height;
}

bool WndToolbar::IsButtonEnabled( const UINT commandID ) const
{
	const LRESULT state = SendMessage( m_hWnd, TB_GETSTATE, commandID, 0 );
	const bool enabled = ( 0 != ( state & TBSTATE_ENABLED ) );
	return enabled;
}

bool WndToolbar::IsButtonChecked( const UINT commandID ) const
{
	const LRESULT state = SendMessage( m_hWnd, TB_GETSTATE, commandID, 0 );
	const bool checked = ( state & TBSTATE_CHECKED );
	return checked;
}

void WndToolbar::SetButtonEnabled( const UINT commandID, const bool enabled )
{
	if ( IsButtonEnabled( commandID ) != enabled ) {
		LRESULT state = SendMessage( m_hWnd, TB_GETSTATE, commandID, 0 );
		if ( enabled ) {
			state |= TBSTATE_ENABLED;
		} else {
			state &= ~TBSTATE_ENABLED;
		}
		SendMessage( m_hWnd, TB_SETSTATE, commandID, state );
	}
}

void WndToolbar::SetButtonChecked( const UINT commandID, const bool checked )
{
	if ( IsButtonChecked( commandID ) != checked ) {
		SendMessage( m_hWnd, TB_CHECKBUTTON, commandID, checked ? TRUE : FALSE );
	}
}

int WndToolbar::GetID() const
{
	return static_cast<int>( GetDlgCtrlID( m_hWnd ) );
}