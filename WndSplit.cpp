#include "WndSplit.h"

#include "Utility.h"

// Split window ID
UINT_PTR WndSplit::s_WndSplitID = 1800;

// Split window class name
static const wchar_t s_SplitClass[] = L"VUSplitClass";

// Minimum split width
static const int s_MinSplit = 200;

// Maximum split width
static const int s_MaxSplit = 500;

// Default split width
static const int s_DefaultSplit = 303;

// Window procedure
static LRESULT CALLBACK WndSplitProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndSplit* wndSplit = reinterpret_cast<WndSplit*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndSplit ) {
		switch ( message ) {
			case WM_PAINT : {
				PAINTSTRUCT ps;
				BeginPaint( hwnd, &ps );
				EndPaint( hwnd, &ps );
				break;
			}
			case WM_MOUSEMOVE : {
				if ( wndSplit->GetIsTracking() ) {
					if ( wndSplit->GetIsDragging() ) {
						POINT pt;
						pt.x = static_cast<short>( LOWORD( lParam ) );
						pt.y = static_cast<short>( HIWORD( lParam ) );
						wndSplit->OnDrag( pt );
					}
				} else {
					TRACKMOUSEEVENT tme = {};
					tme.cbSize = sizeof( TRACKMOUSEEVENT );
					tme.hwndTrack = hwnd;
					tme.dwFlags = TME_LEAVE;
					TrackMouseEvent( &tme );
					wndSplit->SetIsTracking( true );
				}
				break;
			}
			case WM_MOUSELEAVE : {
				wndSplit->SetIsTracking( false );
				break;
			}
			case WM_LBUTTONDOWN : {
				if ( GetCapture() != hwnd ) {
					SetCapture( hwnd );
					wndSplit->SetIsDragging( true );
				}
				break;
			}
			case WM_LBUTTONUP : {
				if ( GetCapture() == hwnd ) {
					ReleaseCapture();
				}
				break;
			}
			case WM_CAPTURECHANGED : {
				wndSplit->SetIsDragging( false );
				break;
			}
			case WM_SIZE : {
				if ( SIZE_MINIMIZED != wParam ) {
					wndSplit->Resize();
				}
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

WndSplit::WndSplit( HINSTANCE instance, HWND parent, HWND wndRebar, HWND wndStatus, HWND wndTree, HWND wndVisual, HWND wndList, Settings& settings ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_hParent( parent ),
	m_hRebar( wndRebar ),
	m_hStatus( wndStatus ),
	m_hTree( wndTree ),
	m_hVisual( wndVisual ),
	m_hList( wndList ),
	m_SplitPosition( 0 ),
	m_MinSplit( static_cast<int>( s_MinSplit * GetDPIScaling() ) ),
	m_MaxSplit( static_cast<int>( s_MaxSplit * GetDPIScaling() ) ),
	m_IsDragging( false ),
	m_IsTracking( false ),
	m_IsSizing( false ),
	m_Settings( settings )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hCursor = LoadCursor( 0, IDC_SIZEWE );
	wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_3DFACE + 1 );
	wc.hInstance = instance;
	wc.lpfnWndProc = WndSplitProc;
	wc.lpszClassName = s_SplitClass;
	RegisterClassEx( &wc );

	const DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 0;
	LPVOID param = NULL;

	m_hWnd = CreateWindowEx( 0, s_SplitClass, NULL, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndSplitID++ ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

	m_SplitPosition = m_Settings.GetSplitWidth();
	if ( ( m_SplitPosition < m_MinSplit ) || ( m_SplitPosition > m_MaxSplit ) ) {
		m_SplitPosition = s_DefaultSplit;
	}
	Resize();
}

WndSplit::~WndSplit()
{
	m_Settings.SetSplitWidth( m_SplitPosition );
}

HWND WndSplit::GetWindowHandle()
{
	return m_hWnd;
}

bool WndSplit::GetIsDragging() const
{
	return m_IsDragging;
}

void WndSplit::SetIsDragging( const bool dragging )
{
	m_IsDragging = dragging;
}

bool WndSplit::GetIsTracking() const
{
	return m_IsTracking;
}

void WndSplit::SetIsTracking( const bool tracking )
{
	m_IsTracking = tracking;
}

void WndSplit::OnDrag( const POINT& position )
{
	POINT pt = position;
	ClientToScreen( m_hWnd, &pt );
	ScreenToClient( m_hParent, &pt );

	int splitPosition = static_cast<int>( pt.x );

	if ( splitPosition < m_MinSplit ) {
		splitPosition = m_MinSplit;
	} else if ( splitPosition > m_MaxSplit ) {
		splitPosition = m_MaxSplit;
	}

	if ( splitPosition != m_SplitPosition ) {
		m_SplitPosition = splitPosition;
		Resize();
	}
}

void WndSplit::Resize()
{
	if ( !m_IsSizing ) {
		RECT clientRect = {};
		GetClientRect( m_hParent, &clientRect );

		if ( NULL != m_hRebar ) {
			RECT rect = {};
			GetWindowRect( m_hRebar, &rect );
			clientRect.top += ( rect.bottom - rect.top );
		}

		if ( NULL != m_hStatus ) {
			RECT rect = {};
			GetWindowRect( m_hStatus, &rect );
			clientRect.bottom -= ( rect.bottom - rect.top );
		}

		RECT splitRect = {};
		const int splitSize = GetSystemMetrics( SM_CXSIZEFRAME );
		splitRect.left = clientRect.left + m_SplitPosition;
		splitRect.right = splitRect.left + splitSize;
		splitRect.top = clientRect.top;
		splitRect.bottom = clientRect.bottom;

		RECT invalidParentRect = {};
		invalidParentRect.right = splitRect.left;

		if ( NULL != m_hWnd )	{
			const int x = static_cast<int>( splitRect.left );
			const int y = static_cast<int>( splitRect.top );
			const int width = static_cast<int>( splitRect.right - splitRect.left );
			const int height = static_cast<int>( splitRect.bottom - splitRect.top );
			m_IsSizing = true;
			MoveWindow( m_hWnd, x, y, width, height, FALSE /*repaint*/ );
			m_IsSizing = false;
		}

		if ( NULL != m_hVisual ) {
			RECT visualRect = {};
			visualRect.left = clientRect.left;
			visualRect.right = splitRect.left;
			visualRect.bottom = clientRect.bottom;
			visualRect.top = visualRect.bottom - ( visualRect.right - visualRect.left );
			const int x = static_cast<int>( visualRect.left );
			const int y = static_cast<int>( visualRect.top );
			const int width = static_cast<int>( visualRect.right - visualRect.left );
			const int height = static_cast<int>( visualRect.bottom - visualRect.top );
			MoveWindow( m_hVisual, x, y, width, height, FALSE /*repaint*/ );
		}

		if ( NULL != m_hTree ) {
			RECT treeRect = {};
			treeRect.left = clientRect.left;
			treeRect.right = splitRect.left;
			treeRect.top = clientRect.top;
			if ( NULL != m_hVisual ) {
				RECT visualRect = {};
				GetClientRect( m_hVisual, &visualRect );
				const int visualHeight = visualRect.bottom - visualRect.top;
				if ( 0 == visualHeight ) {
					treeRect.bottom = clientRect.bottom;
				} else {
					treeRect.bottom = clientRect.bottom - visualHeight - splitSize;
				}
			} else {
				treeRect.bottom = clientRect.bottom;
			}
			const int x = static_cast<int>( treeRect.left );
			const int y = static_cast<int>( treeRect.top );
			const int width = static_cast<int>( treeRect.right - treeRect.left );
			const int height = static_cast<int>( treeRect.bottom - treeRect.top );
			MoveWindow( m_hTree, x, y, width, height, FALSE /*repaint*/ );
			invalidParentRect.top = treeRect.bottom;
			invalidParentRect.bottom = invalidParentRect.top + splitSize;
		}

		if ( NULL != m_hList ) {
			RECT listRect = {};
			listRect.left = splitRect.right;
			listRect.top = clientRect.top;
			listRect.right = clientRect.right;
			listRect.bottom = clientRect.bottom;
			const int x = static_cast<int>( listRect.left );
			const int y = static_cast<int>( listRect.top );
			const int width = static_cast<int>( listRect.right - listRect.left );
			const int height = static_cast<int>( listRect.bottom - listRect.top );
			MoveWindow( m_hList, x, y, width, height, FALSE /*repaint*/ );
		}

		RedrawWindow( m_hWnd, NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN | RDW_UPDATENOW );
		RedrawWindow( m_hTree, NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN | RDW_UPDATENOW );
		RedrawWindow( m_hList, NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
		RedrawWindow( m_hVisual, NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN | RDW_UPDATENOW );

		InvalidateRect( m_hParent, &invalidParentRect, TRUE /*erase*/ );
	}
}
