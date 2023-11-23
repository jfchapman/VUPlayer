#include "WndRebar.h"
#include <commctrl.h>
#include <windowsx.h>

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

// Rebar control ID.
static const UINT_PTR s_WndRebarID = 1200;

// Split window class name
static const wchar_t s_RebarClass[] = L"VURebarClass";

LRESULT CALLBACK WndRebar::RebarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndRebar* wndRebar = reinterpret_cast<WndRebar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndRebar ) {
		switch ( message ) {
			case WM_CONTEXTMENU : {
				POINT pt = {};
				if ( -1 == lParam ) {
					if ( const HWND rebarItemWnd = GetFocus(); nullptr != rebarItemWnd ) {
						if ( RECT rect = {}; GetWindowRect( rebarItemWnd, &rect ) ) {
							pt.x = rect.left;
							pt.y = rect.bottom;
						}
					}
				} else {
					pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				}
				wndRebar->OnContextMenu( reinterpret_cast<HWND>( wParam ), pt );
				break;
			}
			case WM_SIZE : {
				wndRebar->RearrangeItems();
				break;
			}
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
				break;
			}
			case WM_COMMAND : {
				SendMessage( GetParent( hwnd ), message, wParam, lParam );
				return TRUE;
			}
			case WM_NOTIFY : {
				if ( LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam ); nullptr != hdr ) {
					if ( NM_CUSTOMDRAW == hdr->code ) {
						LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>( lParam );
						if ( const auto result = wndRebar->OnCustomDraw( hdr->hwndFrom, nmcd ); result.has_value() ) {
							return result.value();
						}
					} else if ( TBN_HOTITEMCHANGE == hdr->code ) {
						return 0;
					}
				}
				break;
			}
			case WM_PAINT : {
				PAINTSTRUCT ps = {};
				BeginPaint( hwnd, &ps );
				FillRect(	ps.hdc, &ps.rcPaint, HBRUSH( COLOR_3DFACE + 1 ) );
				EndPaint( hwnd, &ps );
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

WndRebar::WndRebar( HINSTANCE instance, HWND parent, Settings& settings ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_Settings( settings ),
	m_Items(),
	m_ItemMap(),
	m_IsHighContrast( IsHighContrastActive() ),
	m_IsClassicTheme( IsClassicThemeActive() )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wc.hInstance = instance;
	wc.lpfnWndProc = RebarProc;
	wc.lpszClassName = s_RebarClass;
	RegisterClassEx( &wc );

	const DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 0;
	LPVOID param = NULL;

	m_hWnd = CreateWindowEx( 0, wc.lpszClassName, NULL, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndRebarID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
}

WndRebar::~WndRebar()
{
}

HWND WndRebar::GetWindowHandle()
{
	return m_hWnd;
}

void WndRebar::AddItem( WndRebarItem* item )
{
	if ( nullptr != item ) {
		m_Items.push_back( item );
		m_ItemMap.insert( { item->GetWindowHandle(), item } );
	}
}

void WndRebar::RearrangeItems()
{
	constexpr int borderSize = 2;
	constexpr int dividerSize = 2;
	constexpr int minimumItemSize = 160;

	if ( const HWND parent = GetParent( m_hWnd ); nullptr != parent ) {
		if ( RECT parentRect = {}; GetClientRect( parent, &parentRect ) ) {
			const int rebarWidth = parentRect.right - parentRect.left;

			int maxItemHeight = 0;
			for ( const auto& item : m_Items ) {
				if ( const auto height = item->GetHeight(); height.has_value() ) {
					if ( height.value() > maxItemHeight ) {
						maxItemHeight = height.value();
					}
				}
			}

			const int rebarHeight = maxItemHeight + borderSize;
			SetWindowPos( m_hWnd, nullptr /*insertAfter*/, 0 /*x*/, 0 /*y*/, rebarWidth, rebarHeight, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER );

			const int minimumItemWidth = static_cast<int>( minimumItemSize * GetDPIScaling() );
			std::map<const WndRebarItem*, int> itemWidths;
			int availableWidth = rebarWidth;
			for ( const auto& item : m_Items ) {
				if ( !item->CanHide() ) {
					const int width = item->GetWidth().has_value() ? item->GetWidth().value() : minimumItemWidth;
					if ( !itemWidths.empty() && item->HasDivider() ) {
						availableWidth -= dividerSize;
					}
					availableWidth -= width;
					itemWidths.insert( { item, width } );
				}
			}

			for ( auto iter = m_Items.rbegin(); iter != m_Items.rend(); ++iter ) {
				if ( const WndRebarItem* item = *iter; nullptr != item ) {
					const int width = item->GetWidth().has_value() ? item->GetWidth().value() : minimumItemWidth;
					if ( item->CanHide() ) {
						if ( const bool isShown = m_Settings.GetToolbarEnabled( item->GetID() ); isShown ) {
							if ( ( availableWidth - width ) >= 0 ) {
								itemWidths.insert( { item, width } );
								availableWidth -= width;
								if ( item->HasDivider() ) {
									availableWidth -= dividerSize;
								}
							} else {
								break;
							}
						}
					}
				}
			}

			int x = 0;
			for ( const auto& item : m_Items ) {
				if ( const auto found = itemWidths.find( item ); itemWidths.end() != found ) {
					int width = found->second;
					if ( !item->GetWidth().has_value() && ( availableWidth > 0 ) ) {
						width += availableWidth;
						availableWidth = 0;
					}

					const int itemHeight = item->GetHeight().has_value() ? item->GetHeight().value() : maxItemHeight;
					const int y = ( maxItemHeight - itemHeight ) / 2;
					SetWindowPos( item->GetWindowHandle(), nullptr /*insertAfter*/, x, y, width, itemHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW );
					x += width;
					if ( item->HasDivider() ) {
						x += dividerSize;
					}
				} else {
					ShowWindow( item->GetWindowHandle(), SW_HIDE );
				}
			}
		}
	}
}

void WndRebar::ToggleToolbar( const int toolbarID )
{
	m_Settings.SetToolbarEnabled( toolbarID, !m_Settings.GetToolbarEnabled( toolbarID ) );
	RearrangeItems();
}

void WndRebar::OnContextMenu( const HWND hwnd, const POINT& position )
{
	if ( const auto contextItem = m_ItemMap.find( hwnd ); ( m_ItemMap.end() == contextItem ) || !contextItem->second->ShowContextMenu( position ) ) {
		if ( HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_TOOLBAR ) ); nullptr != menu ) {
			if ( HMENU toolbarmenu = GetSubMenu( menu, 0 /*pos*/ ); nullptr != toolbarmenu ) {
				for ( const auto& item : m_Items ) {
					if ( const int itemID = item->GetID(); ( itemID > 0 ) && item->CanHide() ) {
						const UINT state = m_Settings.GetToolbarEnabled( itemID ) ? MF_CHECKED : MF_UNCHECKED;
						if ( ID_TOOLBAR_CONVERT == itemID ) {
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
								const Playlist::Ptr playlist = vuplayer->GetSelectedPlaylist();
								const int bufferSize = 64;
								WCHAR buffer[ bufferSize ] = {};
								const bool isCDDAPlaylist = playlist && ( Playlist::Type::CDDA == playlist->GetType() );
								LoadString( m_hInst, isCDDAPlaylist ? IDS_TOOLBAR_EXTRACT : IDS_TOOLBAR_CONVERT, buffer, bufferSize );
								std::wstring menuTitle = buffer;
								const size_t altDelimiter = menuTitle.find( '&' );
								if ( std::wstring::npos != altDelimiter ) {
									menuTitle.erase( altDelimiter /*offset*/, 1 /*count*/ );
								}
								ModifyMenu( menu, ID_TOOLBAR_CONVERT, MF_BYCOMMAND | MF_STRING, ID_TOOLBAR_CONVERT, menuTitle.c_str() );
							}
						}
						CheckMenuItem( toolbarmenu, itemID, state );
					}
				}

				const auto toolbarSize = m_Settings.GetToolbarSize();
				CheckMenuItem( menu, ID_TOOLBARSIZE_SMALL, ( Settings::ToolbarSize::Small == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );
				CheckMenuItem( menu, ID_TOOLBARSIZE_MEDIUM, ( Settings::ToolbarSize::Medium == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );
				CheckMenuItem( menu, ID_TOOLBARSIZE_LARGE, ( Settings::ToolbarSize::Large == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );

				UINT enableColourChoice = m_IsHighContrast ? MF_DISABLED : MF_ENABLED;
				EnableMenuItem( menu, ID_TOOLBAR_COLOUR_BUTTON, MF_BYCOMMAND | enableColourChoice );

				enableColourChoice = ( m_IsHighContrast || m_IsClassicTheme ) ? MF_DISABLED : MF_ENABLED;
				EnableMenuItem( menu, ID_TOOLBAR_COLOUR_BACKGROUND, MF_BYCOMMAND | enableColourChoice );

				TrackPopupMenu( toolbarmenu, TPM_RIGHTBUTTON, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
			}
			DestroyMenu( menu );
		}
	} 
}

void WndRebar::OnChangeSettings()
{
	for ( auto& item : m_Items ) {
		item->OnChangeRebarItemSettings( m_Settings );
	}
}

std::optional<LRESULT> WndRebar::OnCustomDraw( const HWND hwnd, LPNMCUSTOMDRAW nmcd )
{
	std::optional<LRESULT> result;
	if ( const auto item = m_ItemMap.find( hwnd ); m_ItemMap.end() != item ) {
		result = item->second->OnCustomDraw( nmcd );
	}
	return result;
}

void WndRebar::OnSelectColour( const UINT commandID )
{
	VUPlayer* vuplayer = VUPlayer::Get();
	COLORREF* customColours = ( nullptr != vuplayer ) ? vuplayer->GetCustomColours() : nullptr;
	COLORREF buttonColour = DEFAULT_ICONCOLOUR;
	COLORREF backgroundColour = GetSysColor( COLOR_WINDOW );
	m_Settings.GetToolbarColours( buttonColour, backgroundColour );
	if ( const auto colour = ChooseColour( m_hWnd, ( ( ID_TOOLBAR_COLOUR_BUTTON == commandID ) ? buttonColour : backgroundColour ), customColours ); colour.has_value() ) {
		if ( ID_TOOLBAR_COLOUR_BUTTON == commandID ) {
			buttonColour = colour.value();
		} else {
			backgroundColour = colour.value();
		}
		m_Settings.SetToolbarColours( buttonColour, backgroundColour );
		OnChangeSettings();
		RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
	}
}

void WndRebar::OnSysColorChange( const bool isHighContrast, const bool isClassicTheme )
{
	m_IsHighContrast = isHighContrast;
	m_IsClassicTheme = isClassicTheme;
	for ( const auto& item : m_ItemMap ) {
		item.second->OnSysColorChange( isHighContrast, isClassicTheme );
	}
	RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
}
