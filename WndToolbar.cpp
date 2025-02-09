#include "WndToolbar.h"

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

LRESULT CALLBACK WndToolbar::ToolbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndToolbar* wndToolbar = reinterpret_cast<WndToolbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndToolbar ) {
		switch ( message ) {
			case WM_NOTIFY: {
				if ( LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam ); ( nullptr != hdr ) && ( TTN_GETDISPINFO == hdr->code ) ) {
					LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
					const UINT resourceID = wndToolbar->GetTooltip( static_cast<UINT>( info->hdr.idFrom ) );
					if ( resourceID > 0 ) {
						info->hinst = wndToolbar->GetInstanceHandle();
						info->lpszText = MAKEINTRESOURCE( resourceID );
					}
				}
				break;
			}
			case WM_DESTROY: {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndToolbar->GetDefaultWndProc() ) );
				SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
				break;
			}
			default: {
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
	m_ButtonColour( DEFAULT_ICONCOLOUR ),
	m_BackgroundColour( GetSysColor( COLOR_WINDOW ) ),
	m_ImageList( nullptr ),
	m_IsHighContrast( IsHighContrastActive() ),
	m_IsClassicTheme( IsClassicThemeActive() )
{
	m_Settings.GetToolbarColours( m_ButtonColour, m_BackgroundColour );
	CreateImageList();

	const DWORD style = WS_CHILD | WS_VISIBLE | CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TOOLTIPS | TBSTYLE_CUSTOMERASE | TBSTYLE_FLAT;
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

HINSTANCE WndToolbar::GetInstanceHandle() const
{
	return m_hInst;
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

void WndToolbar::CreateImageList()
{
	const int buttonSize = GetButtonSize();
	const int imageCount = static_cast<int>( m_IconList.size() );
	HIMAGELIST imageList = ImageList_Create( buttonSize, buttonSize, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ );
	const COLORREF colour = m_IsHighContrast ? GetSysColor( COLOR_HIGHLIGHT ) : m_ButtonColour;
	for ( const auto& iconID : m_IconList ) {
		if ( const HBITMAP bitmap = CreateColourBitmap( m_hInst, iconID, buttonSize, colour ); nullptr != bitmap ) {
			ImageList_Add( imageList, bitmap, nullptr );
		}
	}

	SendMessage( m_hWnd, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( imageList ) );
	if ( nullptr != m_ImageList ) {
		ImageList_Destroy( m_ImageList );
	}
	m_ImageList = imageList;

	const DWORD previousSize = static_cast<DWORD>( SendMessage( m_hWnd, TB_GETBUTTONSIZE, 0, 0 ) );
	const int previousWidth = LOWORD( previousSize );
	const int previousHeight = HIWORD( previousSize );
	if ( ( buttonSize != previousWidth ) || ( buttonSize != previousHeight ) ) {
		SendMessage( m_hWnd, TB_SETBUTTONSIZE, 0, MAKELPARAM( buttonSize, buttonSize ) );
		SendMessage( m_hWnd, TB_AUTOSIZE, 0, 0 );
	}
}

HIMAGELIST WndToolbar::GetImageList() const
{
	return m_ImageList;
}

HWND WndToolbar::GetWindowHandle() const
{
	return m_hWnd;
}

std::optional<int> WndToolbar::GetWidth() const
{
	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	return buttonCount * ( rect.right - rect.left );
}

std::optional<int> WndToolbar::GetHeight() const
{
	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	return rect.bottom - rect.top;
}

bool WndToolbar::HasDivider() const
{
	return true;
}

bool WndToolbar::CanHide() const
{
	return true;
}

void WndToolbar::OnChangeRebarItemSettings( Settings& settings )
{
	settings.GetToolbarColours( m_ButtonColour, m_BackgroundColour );
	CreateImageList();
}

std::optional<LRESULT> WndToolbar::OnCustomDraw( LPNMCUSTOMDRAW nmcd )
{
	std::optional<LRESULT> result;
	if ( !m_IsHighContrast && !m_IsClassicTheme ) {
		if ( CDDS_PREERASE == nmcd->dwDrawStage ) {
			Gdiplus::Graphics graphics( nmcd->hdc );
			Gdiplus::Color colour;
			colour.SetFromCOLORREF( m_BackgroundColour );
			graphics.Clear( colour );
			result = CDRF_SKIPDEFAULT;
		}
	}
	return result;
}

bool WndToolbar::ShowContextMenu( const POINT& /*position*/ )
{
	return false;
}

void WndToolbar::OnSysColorChange( const bool isHighContrast, const bool isClassicTheme )
{
	m_IsHighContrast = isHighContrast;
	m_IsClassicTheme = isClassicTheme;
	CreateImageList();
}
