#include "WndToolbar.h"

#include "Utility.h"

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
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndToolbar->GetDefaultWndProc() ) );
				SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
				break;
			}
			case WM_SIZE : {
        UINT width = LOWORD( lParam );
        UINT height = HIWORD( lParam );
				height = width;
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndToolbar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndToolbar::WndToolbar( HINSTANCE instance, HWND parent, const long long id, Settings& settings, const IconList& iconList ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL ),
	m_Settings( settings ),
	m_IconList( iconList ),
	m_ImageList( CreateImageList() )
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
	ImageList_Destroy( m_ImageList );
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

int WndToolbar::GetButtonSize() const
{
	return Settings::GetToolbarButtonSize( m_Settings.GetToolbarSize() );
}

HIMAGELIST WndToolbar::CreateImageList() const
{
	const int buttonSize = GetButtonSize();
	const int cx = buttonSize;
	const int cy = buttonSize;

	const int imageCount = static_cast<int>( m_IconList.size() );
	HIMAGELIST imageList = ImageList_Create( cx, cy, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ );
	for ( const auto& iconID : m_IconList ) {
		HICON hIcon = static_cast<HICON>( LoadImage( GetInstanceHandle(), MAKEINTRESOURCE( iconID ), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR | LR_SHARED ) );
		if ( NULL != hIcon ) {
			ImageList_ReplaceIcon( imageList, -1, hIcon );
		}
	}
	return imageList;
}

HIMAGELIST WndToolbar::GetImageList() const
{
	return m_ImageList;
}

void WndToolbar::OnChangeToolbarSize()
{
	const HIMAGELIST imageList = CreateImageList();
	SendMessage( m_hWnd, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( imageList ) );

	const DWORD previousSize = static_cast<DWORD>( SendMessage( m_hWnd, TB_GETBUTTONSIZE, 0, 0 ) );
	const int previousWidth = LOWORD( previousSize );
	const int previousHeight = HIWORD( previousSize );

	SendMessage( m_hWnd, TB_SETBUTTONSIZE, 0, MAKELPARAM( GetButtonSize(), GetButtonSize() ) );
	ImageList_Destroy( m_ImageList );
	m_ImageList = imageList;
}