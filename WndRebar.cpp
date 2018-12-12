#include "WndRebar.h"
#include <commctrl.h>
#include <windowsx.h>

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

// Rebar control ID.
static const UINT_PTR s_WndRebarID = 1200;

// First rebar band ID.
static const UINT s_FirstBandID = 13;

// Next available band ID.
UINT WndRebar::s_BandID = s_FirstBandID;

// Rebar image size.
static const int s_ImageSize = 24;

LRESULT CALLBACK WndRebar::RebarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndRebar* wndRebar = reinterpret_cast<WndRebar*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndRebar ) {
		switch ( message ) {
			case WM_HSCROLL : {
				SendMessage( reinterpret_cast<HWND>( lParam ) /*trackbarWnd*/, message, wParam, lParam );
				break;
			}
			case WM_LBUTTONDOWN : 
			case WM_CONTEXTMENU : {
				POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				if ( WM_CONTEXTMENU == message ) {
					ScreenToClient( hwnd, &pt );
				}
				RBHITTESTINFO hitTestInfo = {};
				hitTestInfo.pt = pt;
				const int bandIndex = static_cast<int>( SendMessage( hwnd, RB_HITTEST, 0, reinterpret_cast<LPARAM>( &hitTestInfo ) ) );
				if ( ( bandIndex >= 0 ) && ( RBHT_CAPTION == hitTestInfo.flags ) ) {
					REBARBANDINFO bandInfo = {};
					bandInfo.cbSize = sizeof( REBARBANDINFO );
					bandInfo.fMask = RBBIM_ID;
					SendMessage( hwnd, RB_GETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &bandInfo ) );
					wndRebar->OnClickCaption( bandInfo.wID, ( WM_CONTEXTMENU == message ) );
				} else if ( WM_CONTEXTMENU == message ) {
					REBARBANDINFO bandInfo = {};
					bandInfo.cbSize = sizeof( REBARBANDINFO );
					bandInfo.fMask = RBBIM_ID;
					SendMessage( hwnd, RB_GETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &bandInfo ) );
					if ( wndRebar->CanBeHidden( bandInfo.wID ) ) {
						ClientToScreen( hwnd, &pt );
						wndRebar->OnContextMenu( pt );
					}
				}
				break;
			}
			case WM_SIZE : {
				wndRebar->RearrangeBands();
				break;
			}
		}
	}
	return CallWindowProc( wndRebar->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

WndRebar::WndRebar( HINSTANCE instance, HWND parent, Settings& settings ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_Settings( settings ),
	m_DefaultWndProc( NULL ),
	m_ImageList( NULL ),
	m_IconBands(),
	m_ImageListMap(),
	m_IconCallbackMap(),
	m_ClickCallbackMap(),
	m_CanBeHidden(),
	m_HiddenBands(),
	m_BandChildWindows(),
	m_PreventBandRearrangement( false ),
	m_PreviousRebarWidth( 0 )
{
	const DWORD style = WS_CHILD | WS_VISIBLE | CCS_NODIVIDER | RBS_FIXEDORDER | RBS_BANDBORDERS;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 30;
	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW, REBARCLASSNAME, 0, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndRebarID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( RebarProc ) ) );

	CreateImageList();
}

WndRebar::~WndRebar()
{
	SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_DefaultWndProc ) );
	ImageList_Destroy( m_ImageList );
}

WNDPROC WndRebar::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndRebar::GetWindowHandle()
{
	return m_hWnd;
}

void WndRebar::AddControl( HWND hwnd, const bool canHide )
{
	const UINT bandID = s_BandID++;
	if ( canHide ) {
		m_CanBeHidden.insert( bandID );
	}
	m_BandChildWindows.insert( BandChildMap::value_type( bandID, hwnd ) );

	InsertBand( bandID, hwnd );
}

void WndRebar::AddControl( HWND hwnd, const std::set<UINT>& icons, IconCallback iconCallback, ClickCallback clickCallback, const bool canHide )
{
	REBARBANDINFO info = {};
	info.cbSize = sizeof( REBARBANDINFO );
	info.fStyle = RBBS_CHILDEDGE | RBBS_FIXEDSIZE;
	info.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_STYLE | RBBIM_ID | RBBIM_IMAGE;
	info.wID = s_BandID++;

	if ( canHide ) {
		m_CanBeHidden.insert( info.wID );
	}
	m_BandChildWindows.insert( BandChildMap::value_type( info.wID, hwnd ) );

	const float dpiScale = GetDPIScaling();
	const int cx = static_cast<int>( s_ImageSize * dpiScale );
	const int cy = static_cast<int>( s_ImageSize * dpiScale );

	for ( const auto& iconID : icons ) {
		HICON hIcon = static_cast<HICON>( LoadImage( m_hInst, MAKEINTRESOURCE( iconID ), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR | LR_SHARED ) );
		if ( NULL != hIcon ) {
			const int imageListIndex = ImageList_ReplaceIcon( m_ImageList, -1, hIcon );
			m_ImageListMap.insert( ImageListMap::value_type( iconID, imageListIndex ) );
		}	
	}
	m_IconCallbackMap.insert( IconCallbackMap::value_type( info.wID, iconCallback ) );
	m_ClickCallbackMap.insert( ClickCallbackMap::value_type( info.wID, clickCallback ) );
	info.iImage = GetImageListIndex( info.wID );
	m_IconBands.insert( info.wID );

	RECT rect = {};
	GetWindowRect( hwnd, &rect );
	info.hwndChild = hwnd;
	info.cx = rect.right - rect.left;
	info.cxMinChild = info.cx;
	info.cyMinChild = rect.bottom - rect.top;
	SendMessage( m_hWnd, RB_INSERTBAND, static_cast<WPARAM>( -1 ), reinterpret_cast<LPARAM>( &info ) );
}

int WndRebar::GetPosition( const HWND hwnd ) const
{
	REBARBANDINFO info = {};
	info.cbSize = sizeof( REBARBANDINFO );
	info.fMask = RBBIM_CHILD;
	int position = -1;
	const int count = static_cast<int>( SendMessage( m_hWnd, RB_GETBANDCOUNT, 0, 0 ) );
	for ( int index = 0; index < count; index++ ) {
		SendMessage( m_hWnd, RB_GETBANDINFO, index, reinterpret_cast<LPARAM>( &info ) );
		if ( info.hwndChild == hwnd ) {
			position = static_cast<int>( index );
			break;
		}
	}
	return position;
}

void WndRebar::MoveToStart( const int currentPosition )
{
	SendMessage( m_hWnd, RB_MOVEBAND, currentPosition, 0 /*newPosition*/ );
}

void WndRebar::CreateImageList()
{
	const float dpiScale = GetDPIScaling();
	const int cx = static_cast<int>( s_ImageSize * dpiScale );
	const int cy = static_cast<int>( s_ImageSize * dpiScale );
	m_ImageList = ImageList_Create( cx, cy, ILC_COLOR32, 0 /*initial*/, 1 /*grow*/ );

	REBARINFO rebarInfo = {};
	rebarInfo.cbSize = sizeof( REBARINFO );
	rebarInfo.fMask = RBIM_IMAGELIST;
	rebarInfo.himl = m_ImageList;
	SendMessage( m_hWnd, RB_SETBARINFO, 0, reinterpret_cast<LPARAM>( &rebarInfo ) );
}

int WndRebar::GetImageListIndex( const UINT bandID ) const
{
	int imageListIndex = -1;
	const auto callbackIter = m_IconCallbackMap.find( bandID );
	if ( m_IconCallbackMap.end() != callbackIter ) {
		IconCallback callback = callbackIter->second;
		if ( nullptr != callback ) {
			const UINT iconID = callback();
			const auto imageListIter = m_ImageListMap.find( iconID );
			if ( m_ImageListMap.end() != imageListIter ) {
				imageListIndex = imageListIter->second;
			}
		}
	}
	return imageListIndex;
}

void WndRebar::Init()
{
	// Hide any disabled toolbars.
	for ( const auto& bandIter : m_BandChildWindows ) {
		const bool enabled = m_Settings.GetToolbarEnabled( GetDlgCtrlID( bandIter.second ) );
		if ( !enabled && ( m_HiddenBands.end() == m_HiddenBands.find( bandIter.first ) ) ) {
			m_HiddenBands.insert( bandIter.first );
			DeleteBand( bandIter.first );
			RearrangeBands( true /*force*/ );
		}
	}

	// Force an update.
	PostMessage( m_hWnd, WM_SIZE, 0, 0 );
}

void WndRebar::Update()
{
	// Update any icons.
	for ( const auto& bandID : m_IconBands ) {
		const int bandIndex = static_cast<int>( SendMessage( m_hWnd, RB_IDTOINDEX, bandID, 0 ) );
		if ( -1 != bandIndex ) {
			REBARBANDINFO info = {};
			info.cbSize = sizeof( REBARBANDINFO );
			info.fMask = RBBIM_IMAGE;
			SendMessage( m_hWnd, RB_GETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &info ) );
			const int imageListIndex = GetImageListIndex( bandID );
			if ( imageListIndex != info.iImage ) {
				info.iImage = imageListIndex;
				SendMessage( m_hWnd, RB_SETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &info ) );
			}
		}
	}
}

void WndRebar::OnClickCaption( const UINT bandID, const bool rightClick )
{
	const auto callbackIter = m_ClickCallbackMap.find( bandID );
	if ( m_ClickCallbackMap.end() != callbackIter ) {
		ClickCallback callback = callbackIter->second;
		if ( nullptr != callback ) {
			callback( rightClick );
		}
	}
}

void WndRebar::RearrangeBands( const bool force )
{
	RECT rect = {};
	GetWindowRect( m_hWnd, &rect );
	const int rebarWidth = ( rect.right - rect.left );

	if ( !m_PreventBandRearrangement ) {
		int rowCount = static_cast<int>( SendMessage( m_hWnd, RB_GETROWCOUNT, 0, 0 ) );
		if ( rowCount > 1 ) {
			// Hide the band with the highest ID.
			const int bandCount = static_cast<int>( SendMessage( m_hWnd, RB_GETBANDCOUNT, 0, 0 ) );

			UINT lowestBandID = UINT_MAX;
			for ( int bandIndex = 0; bandIndex < bandCount; bandIndex++ ) {
				REBARBANDINFO info = {};
				info.cbSize = sizeof( REBARBANDINFO );
				info.fMask = RBBIM_ID;
				SendMessage( m_hWnd, RB_GETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &info ) );
				if ( ( info.wID < lowestBandID ) && ( m_CanBeHidden.end() != m_CanBeHidden.find( info.wID ) ) ) {
					lowestBandID = info.wID;
				}
			}

			m_PreventBandRearrangement = true;
			DeleteBand( lowestBandID );
			m_PreventBandRearrangement = false;
			m_HiddenBands.insert( lowestBandID );

			rowCount = static_cast<int>( SendMessage( m_hWnd, RB_GETROWCOUNT, 0, 0 ) );
			if ( rowCount > 1 ) {
				RearrangeBands();
			} else {
				ReorderBandsByID();
			}
		} else if ( !m_HiddenBands.empty() && ( force || ( rebarWidth > m_PreviousRebarWidth ) ) ) {
			// Reshow any hidden bands, if space permits.
			HWND controlWnd = nullptr;
			UINT bandID = 0;
			for ( auto hiddenID = m_HiddenBands.rbegin(); ( nullptr == controlWnd ) && ( hiddenID != m_HiddenBands.rend() ); hiddenID++ ) {
				const auto bandIter = m_BandChildWindows.find( *hiddenID );
				if ( m_BandChildWindows.end() != bandIter ) {
					const HWND hwnd = bandIter->second;
					if ( m_Settings.GetToolbarEnabled( GetDlgCtrlID( hwnd ) ) ) {
						bandID = *hiddenID;
						controlWnd = hwnd;
					}
				}
			}

			if ( nullptr != controlWnd ) {
				// Try adding the band.
				m_PreventBandRearrangement = true;
				InsertBand( bandID, controlWnd );
				rowCount = static_cast<int>( SendMessage( m_hWnd, RB_GETROWCOUNT, 0, 0 ) );
				if ( rowCount > 1 ) {
					// Not enough room for the band, so remove it again.
					DeleteBand( bandID );
				} else {
					// Keep the band and reorder as necessary.
					m_HiddenBands.erase( bandID );
					ReorderBandsByID();
				}
				m_PreventBandRearrangement = false;
			}
		}
	}
	m_PreviousRebarWidth = rebarWidth;
}

void WndRebar::ReorderBandsByID()
{
	BandIDSet bandIDSet;
	const int bandCount = static_cast<int>( SendMessage( m_hWnd, RB_GETBANDCOUNT, 0, 0 ) );
	for ( int bandIndex = 0; bandIndex < bandCount; bandIndex++ ) {
		REBARBANDINFO info = {};
		info.cbSize = sizeof( REBARBANDINFO );
		info.fMask = RBBIM_ID;
		SendMessage( m_hWnd, RB_GETBANDINFO, bandIndex, reinterpret_cast<LPARAM>( &info ) );
		bandIDSet.insert( info.wID );
	}

	int targetIndex = 0;
	for ( const auto& bandID : bandIDSet ) {
		const int bandIndex = static_cast<int>( SendMessage( m_hWnd, RB_IDTOINDEX, bandID, 0 ) );
		if ( bandIndex != targetIndex ) {
			SendMessage( m_hWnd, RB_MOVEBAND, bandIndex, targetIndex );
		}
		++targetIndex;
	}
}

void WndRebar::InsertBand( const UINT bandID, const HWND controlWnd )
{
	REBARBANDINFO info = {};
	info.cbSize = sizeof( REBARBANDINFO );
	info.fStyle = RBBS_CHILDEDGE | RBBS_FIXEDSIZE;
	info.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_STYLE | RBBIM_ID | RBBIM_HEADERSIZE;
	info.wID = bandID;
	info.cxHeader = 1;

	RECT rect = {};
	GetWindowRect( controlWnd, &rect );
	info.hwndChild = controlWnd;
	info.cx = rect.right - rect.left;
	info.cxMinChild = info.cx;
	info.cyMinChild = rect.bottom - rect.top;

	SendMessage( m_hWnd, RB_INSERTBAND, static_cast<WPARAM>( -1 ), reinterpret_cast<LPARAM>( &info ) );
}

void WndRebar::DeleteBand( const UINT bandID )
{
	const int bandIndex = static_cast<int>( SendMessage( m_hWnd, RB_IDTOINDEX, bandID, 0 ) );
	if ( -1 != bandIndex ) {
		SendMessage( m_hWnd, RB_DELETEBAND, bandIndex, 0 );
	}
}

void WndRebar::ToggleToolbar( const int toolbarID )
{
	const bool enabled = !m_Settings.GetToolbarEnabled( toolbarID );
	m_Settings.SetToolbarEnabled( toolbarID, enabled );

	const auto bandIter = std::find_if( m_BandChildWindows.begin(), m_BandChildWindows.end(), [ toolbarID ] ( const BandChildMap::value_type& bandInfo )
	{
		return ( toolbarID == GetDlgCtrlID( bandInfo.second ) );			
	} );

	if ( m_BandChildWindows.end() != bandIter ) {
		if ( enabled ) {
			m_HiddenBands.erase( bandIter->first );
			InsertBand( bandIter->first, bandIter->second );
			ReorderBandsByID();
		} else if ( m_HiddenBands.end() == m_HiddenBands.find( bandIter->first ) ) {
			m_HiddenBands.insert( bandIter->first );
			DeleteBand( bandIter->first );
		}
	}

	RearrangeBands( true /*force*/ );

	// Force an update.
	PostMessage( m_hWnd, WM_SIZE, 0, 0 );
}

bool WndRebar::CanBeHidden( const UINT bandID ) const
{
	const bool canHide = ( m_CanBeHidden.end() != m_CanBeHidden.find( bandID ) );
	return canHide;
}

void WndRebar::OnContextMenu( const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_TOOLBAR ) );
	if ( NULL != menu ) {
		HMENU toolbarmenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != toolbarmenu ) {
			for ( const auto& bandIter : m_BandChildWindows ) {
				const int toolbarID = GetDlgCtrlID( bandIter.second );
				const UINT state = m_Settings.GetToolbarEnabled( toolbarID ) ? MF_CHECKED : MF_UNCHECKED;
				if ( ID_TOOLBAR_CONVERT == toolbarID ) {
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
				CheckMenuItem( toolbarmenu, toolbarID, state );
			}
			TrackPopupMenu( toolbarmenu, TPM_RIGHTBUTTON, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}
