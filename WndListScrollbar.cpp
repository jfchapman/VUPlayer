#include "WndListScrollbar.h"

#include "DarkMode.h"

#include <CommCtrl.h>

// Host window class name.
static const wchar_t s_HostClass[] = L"VUListScrollbarHostClass";

LRESULT CALLBACK WndListScrollbar::HostProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndListScrollbar* wndListScrollbar = reinterpret_cast<WndListScrollbar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndListScrollbar ) {
		switch ( message ) {
			case WM_SIZE: {
				if ( Overlay* overlay = wndListScrollbar->GetOverlay( hwnd ); nullptr != overlay ) {
					MoveWindow( overlay->Scrollbar, 0, 0, LOWORD( lParam ), HIWORD( lParam ), TRUE /*repaint*/ );
				}
				break;
			}
			case WM_VSCROLL:
			case WM_HSCROLL: {
				if ( Overlay* overlay = wndListScrollbar->GetOverlay( hwnd ); nullptr != overlay ) {
					wndListScrollbar->OnScroll( *overlay, LOWORD( wParam ) );
				}
				return 0;
			}
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL: {
				// Mouse wheel messages over the scrollbars are passed on to the list.
				SendMessage( wndListScrollbar->m_hList, message, wParam, lParam );
				return 0;
			}
			case WM_ERASEBKGND: {
				// The scrollbar controls cover the entire host windows, and the corner window is fully painted.
				return TRUE;
			}
			case WM_PAINT: {
				if ( hwnd == wndListScrollbar->m_hCorner ) {
					wndListScrollbar->OnPaintCorner();
					return 0;
				}
				break;
			}
			default: {
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

WndListScrollbar::WndListScrollbar( HINSTANCE instance, HWND list ) :
	m_hList( list )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wc.hInstance = instance;
	wc.lpfnWndProc = HostProc;
	wc.lpszClassName = s_HostClass;
	RegisterClassEx( &wc );

	// The windows are created hidden, and are only shown when the overlays are active.
	CreateOverlay( m_Vertical, instance, SB_VERT );
	CreateOverlay( m_Horizontal, instance, SB_HORZ );
	m_hCorner = CreateWindowEx( 0, s_HostClass, nullptr, WS_CHILD, 0, 0, 0, 0, GetParent( m_hList ), nullptr, instance, nullptr );
	SetWindowLongPtr( m_hCorner, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
}

WndListScrollbar::~WndListScrollbar()
{
	for ( const HWND hwnd : { m_Vertical.Host, m_Horizontal.Host, m_hCorner } ) {
		if ( IsWindow( hwnd ) ) {
			SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
			DestroyWindow( hwnd );
		}
	}
}

void WndListScrollbar::CreateOverlay( Overlay& overlay, const HINSTANCE instance, const int bar )
{
	overlay.Bar = bar;
	overlay.ObjectID = ( SB_VERT == bar ) ? OBJID_VSCROLL : OBJID_HSCROLL;
	overlay.Host = CreateWindowEx( 0, s_HostClass, nullptr, WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0, GetParent( m_hList ), nullptr, instance, nullptr );
	SetWindowLongPtr( overlay.Host, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	overlay.Scrollbar = CreateWindowEx( 0, WC_SCROLLBAR, nullptr, WS_CHILD | WS_VISIBLE | ( ( SB_VERT == bar ) ? SBS_VERT : SBS_HORZ ), 0, 0, 0, 0, overlay.Host, nullptr, instance, nullptr );
}

WndListScrollbar::Overlay* WndListScrollbar::GetOverlay( const HWND host )
{
	if ( host == m_Vertical.Host ) {
		return &m_Vertical;
	} else if ( host == m_Horizontal.Host ) {
		return &m_Horizontal;
	}
	return nullptr;
}

void WndListScrollbar::SetActive( const bool active )
{
	m_IsActive = active;
	for ( Overlay* overlay : { &m_Vertical, &m_Horizontal } ) {
		DarkMode::ApplyToControl( overlay->Scrollbar );
		// Force the scroll information to be refreshed.
		overlay->ScrollInfo = {};
	}
	// While the overlays are active, the list is clipped by them, so that it cannot draw its own scrollbars over the overlays.
	// (When the list is resized, it draws its own scrollbars at the new location before the window region can be updated).
	const LONG_PTR style = GetWindowLongPtr( m_hList, GWL_STYLE );
	if ( m_IsActive && !( WS_CLIPSIBLINGS & style ) ) {
		SetWindowLongPtr( m_hList, GWL_STYLE, style | WS_CLIPSIBLINGS );
		m_AddedClipSiblings = true;
	} else if ( !m_IsActive && m_AddedClipSiblings ) {
		SetWindowLongPtr( m_hList, GWL_STYLE, style & ~WS_CLIPSIBLINGS );
		m_AddedClipSiblings = false;
	}

	if ( m_IsActive ) {
		Update();
	} else {
		Hide();
	}
}

void WndListScrollbar::Update()
{
	if ( !m_IsActive || m_IsUpdating || !IsWindow( m_hList ) ) {
		return;
	}
	m_IsUpdating = true;

	// Determine which of the list's own scrollbars are visible.
	const bool listVisible = IsWindowVisible( m_hList );
	SCROLLBARINFO verticalInfo = {};
	verticalInfo.cbSize = sizeof( SCROLLBARINFO );
	SCROLLBARINFO horizontalInfo = {};
	horizontalInfo.cbSize = sizeof( SCROLLBARINFO );
	const auto isScrollbarVisible = [ this, listVisible ] ( const LONG objectID, SCROLLBARINFO& info ) {
		return listVisible && GetScrollBarInfo( m_hList, objectID, &info ) && !( ( STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN ) & info.rgstate[ 0 ] ) && !IsRectEmpty( &info.rcScrollBar );
	};
	const bool verticalVisible = isScrollbarVisible( OBJID_VSCROLL, verticalInfo );
	const bool horizontalVisible = isScrollbarVisible( OBJID_HSCROLL, horizontalInfo );

	// Screen rects of the list's own scrollbars, and the corner between them (empty if not visible).
	const RECT verticalRect = verticalVisible ? verticalInfo.rcScrollBar : RECT{};
	const RECT horizontalRect = horizontalVisible ? horizontalInfo.rcScrollBar : RECT{};
	const RECT cornerRect = ( verticalVisible && horizontalVisible ) ? RECT{ verticalRect.left, horizontalRect.top, verticalRect.right, horizontalRect.bottom } : RECT{};

	// Exclude the list's own scrollbars (and corner) from the list's window region, so that they are never drawn.
	RECT windowRect = {};
	GetWindowRect( m_hList, &windowRect );
	std::array<RECT, 3> regionRects = { verticalRect, horizontalRect, cornerRect };
	for ( auto& rect : regionRects ) {
		if ( !IsRectEmpty( &rect ) ) {
			OffsetRect( &rect, -windowRect.left, -windowRect.top );
		}
	}
	SetListRegion( regionRects );

	// Position the overlays over the list's own scrollbars.
	PositionWindow( m_Vertical.Host, verticalRect );
	PositionWindow( m_Horizontal.Host, horizontalRect );
	PositionWindow( m_hCorner, cornerRect );

	// Match the list's scroll information.
	if ( verticalVisible ) {
		UpdateScrollInfo( m_Vertical, verticalInfo );
	} else {
		m_Vertical.ScrollInfo = {};
	}
	if ( horizontalVisible ) {
		UpdateScrollInfo( m_Horizontal, horizontalInfo );
	} else {
		m_Horizontal.ScrollInfo = {};
	}

	m_IsUpdating = false;
}

void WndListScrollbar::PositionWindow( const HWND window, const RECT& screenRect )
{
	if ( IsRectEmpty( &screenRect ) ) {
		if ( IsWindowVisible( window ) ) {
			ShowWindow( window, SW_HIDE );
		}
	} else {
		const HWND parent = GetParent( window );
		RECT rect = screenRect;
		MapWindowPoints( nullptr, parent, reinterpret_cast<POINT*>( &rect ), 2 );
		RECT currentRect = {};
		GetWindowRect( window, &currentRect );
		MapWindowPoints( nullptr, parent, reinterpret_cast<POINT*>( &currentRect ), 2 );
		if ( !IsWindowVisible( window ) || !EqualRect( &rect, &currentRect ) ) {
			SetWindowPos( window, HWND_TOP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW );
		}
	}
}

void WndListScrollbar::UpdateScrollInfo( Overlay& overlay, const SCROLLBARINFO& scrollbarInfo )
{
	// The position is left alone while the overlay thumb is being dragged.
	SCROLLINFO scrollInfo = {};
	scrollInfo.cbSize = sizeof( SCROLLINFO );
	scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	if ( GetScrollInfo( m_hList, overlay.Bar, &scrollInfo ) ) {
		if ( overlay.IsTracking ) {
			scrollInfo.fMask &= ~SIF_POS;
			scrollInfo.nPos = overlay.ScrollInfo.nPos;
		}
		if ( ( scrollInfo.nMin != overlay.ScrollInfo.nMin ) || ( scrollInfo.nMax != overlay.ScrollInfo.nMax ) || ( scrollInfo.nPage != overlay.ScrollInfo.nPage ) || ( scrollInfo.nPos != overlay.ScrollInfo.nPos ) ) {
			SetScrollInfo( overlay.Scrollbar, SB_CTL, &scrollInfo, TRUE /*redraw*/ );
			overlay.ScrollInfo = scrollInfo;
		}
	}

	const bool enabled = !( STATE_SYSTEM_UNAVAILABLE & scrollbarInfo.rgstate[ 0 ] );
	if ( enabled != static_cast<bool>( IsWindowEnabled( overlay.Scrollbar ) ) ) {
		EnableWindow( overlay.Scrollbar, enabled );
	}
}

void WndListScrollbar::SetListRegion( const std::array<RECT, 3>& excludedRects )
{
	bool changed = false;
	bool empty = true;
	for ( size_t index = 0; index < excludedRects.size(); index++ ) {
		changed |= !EqualRect( &excludedRects[ index ], &m_RegionRects[ index ] );
		empty &= static_cast<bool>( IsRectEmpty( &excludedRects[ index ] ) );
	}
	if ( !changed || !IsWindow( m_hList ) ) {
		return;
	}

	if ( empty ) {
		SetWindowRgn( m_hList, nullptr, TRUE /*redraw*/ );
		m_RegionRects = {};
	} else {
		RECT windowRect = {};
		GetWindowRect( m_hList, &windowRect );
		if ( HRGN region = CreateRectRgn( 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top ); nullptr != region ) {
			for ( const auto& rect : excludedRects ) {
				if ( !IsRectEmpty( &rect ) ) {
					if ( HRGN excludedRegion = CreateRectRgnIndirect( &rect ); nullptr != excludedRegion ) {
						CombineRgn( region, region, excludedRegion, RGN_DIFF );
						DeleteObject( excludedRegion );
					}
				}
			}
			// Note that the system owns the region once it has been set.
			if ( SetWindowRgn( m_hList, region, TRUE /*redraw*/ ) ) {
				m_RegionRects = excludedRects;
			} else {
				DeleteObject( region );
			}
		}
	}
}

void WndListScrollbar::OnScroll( Overlay& overlay, const int request )
{
	const UINT scrollMessage = ( SB_VERT == overlay.Bar ) ? WM_VSCROLL : WM_HSCROLL;
	switch ( request ) {
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: {
			// The list cannot be sent thumb tracking requests (as it would use the tracking position of its own scrollbar), so scroll the list directly.
			overlay.IsTracking = ( SB_THUMBTRACK == request );
			SCROLLINFO trackInfo = {};
			trackInfo.cbSize = sizeof( SCROLLINFO );
			trackInfo.fMask = SIF_TRACKPOS;
			SCROLLINFO listInfo = {};
			listInfo.cbSize = sizeof( SCROLLINFO );
			listInfo.fMask = SIF_POS;
			if ( GetScrollInfo( overlay.Scrollbar, SB_CTL, &trackInfo ) && GetScrollInfo( m_hList, overlay.Bar, &listInfo ) ) {
				if ( const int delta = trackInfo.nTrackPos - listInfo.nPos; 0 != delta ) {
					if ( SB_VERT == overlay.Bar ) {
						// In report view, the list scrolls vertically in whole lines, with the scroll amount specified in pixels.
						if ( RECT itemRect = {}; ListView_GetItemRect( m_hList, 0, &itemRect, LVIR_BOUNDS ) ) {
							ListView_Scroll( m_hList, 0, delta * ( itemRect.bottom - itemRect.top ) );
						}
					} else {
						// In report view, the horizontal scroll position is in pixels.
						ListView_Scroll( m_hList, delta, 0 );
					}
				}
			}
			break;
		}
		case SB_ENDSCROLL: {
			overlay.IsTracking = false;
			SendMessage( m_hList, scrollMessage, MAKEWPARAM( SB_ENDSCROLL, 0 ), 0 );
			break;
		}
		default: {
			// Line, page, top & bottom requests are handled by the list itself.
			SendMessage( m_hList, scrollMessage, MAKEWPARAM( request, 0 ), 0 );
			break;
		}
	}
	Update();
}

void WndListScrollbar::Hide()
{
	for ( const HWND hwnd : { m_Vertical.Host, m_Horizontal.Host, m_hCorner } ) {
		if ( IsWindow( hwnd ) && IsWindowVisible( hwnd ) ) {
			ShowWindow( hwnd, SW_HIDE );
		}
	}
	SetListRegion( {} );
	m_Vertical.ScrollInfo = {};
	m_Horizontal.ScrollInfo = {};
}

void WndListScrollbar::OnPaintCorner()
{
	PAINTSTRUCT ps = {};
	BeginPaint( m_hCorner, &ps );
	RECT rect = {};
	GetClientRect( m_hCorner, &rect );
	DarkMode::DrawScrollbarCorner( ps.hdc, rect );
	EndPaint( m_hCorner, &ps );
}
