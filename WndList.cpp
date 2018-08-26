#include "WndList.h"

#include "resource.h"

#include <windowsx.h>

#include "Settings.h"
#include "Utility.h"
#include "VUPlayer.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

// Column information
WndList::ColumnFormats WndList::s_ColumnFormats = {
	{ Playlist::Column::Filename, ColumnFormat( {				ID_SHOWCOLUMNS_FILENAME,			ID_SORTPLAYLIST_FILENAME,				IDS_COLUMN_FILENAME,			LVCFMT_LEFT,		150 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::Filetime, ColumnFormat( {				ID_SHOWCOLUMNS_FILETIME,			ID_SORTPLAYLIST_FILETIME,				IDS_COLUMN_FILETIME,			LVCFMT_LEFT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::Filesize, ColumnFormat( {				ID_SHOWCOLUMNS_FILESIZE,			ID_SORTPLAYLIST_FILESIZE,				IDS_COLUMN_FILESIZE,			LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::Duration, ColumnFormat( {				ID_SHOWCOLUMNS_DURATION,			ID_SORTPLAYLIST_DURATION,				IDS_COLUMN_DURATION,			LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::SampleRate, ColumnFormat( {			ID_SHOWCOLUMNS_SAMPLERATE,		ID_SORTPLAYLIST_SAMPLERATE,			IDS_COLUMN_SAMPLERATE,		LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::BitsPerSample, ColumnFormat( {	ID_SHOWCOLUMNS_BITSPERSAMPLE,	ID_SORTPLAYLIST_BITSPERSAMPLE,	IDS_COLUMN_BITSPERSAMPLE,	LVCFMT_RIGHT,		50 /*width*/,		false /*canEdit*/ } ) },
	{ Playlist::Column::Channels, ColumnFormat( {				ID_SHOWCOLUMNS_CHANNELS,			ID_SORTPLAYLIST_CHANNELS,				IDS_COLUMN_CHANNELS,			LVCFMT_CENTER,	50 /*width*/,		false /*canEdit*/ } ) },
	{ Playlist::Column::Artist, ColumnFormat( {					ID_SHOWCOLUMNS_ARTIST,				ID_SORTPLAYLIST_ARTIST,					IDS_COLUMN_ARTIST,				LVCFMT_LEFT,		150 /*width*/,	true /*canEdit*/ } ) },
	{ Playlist::Column::Title, ColumnFormat( {					ID_SHOWCOLUMNS_TITLE,					ID_SORTPLAYLIST_TITLE,					IDS_COLUMN_TITLE,					LVCFMT_LEFT,		150 /*width*/,	true /*canEdit*/ } ) },
	{ Playlist::Column::Album, ColumnFormat( {					ID_SHOWCOLUMNS_ALBUM,					ID_SORTPLAYLIST_ALBUM,					IDS_COLUMN_ALBUM,					LVCFMT_LEFT,		100 /*width*/,	true /*canEdit*/ } ) },
	{ Playlist::Column::Genre, ColumnFormat( {					ID_SHOWCOLUMNS_GENRE,					ID_SORTPLAYLIST_GENRE,					IDS_COLUMN_GENRE,					LVCFMT_LEFT,		100 /*width*/,	true /*canEdit*/ } ) },
	{ Playlist::Column::Year, ColumnFormat( {						ID_SHOWCOLUMNS_YEAR,					ID_SORTPLAYLIST_YEAR,						IDS_COLUMN_YEAR,					LVCFMT_CENTER,	50 /*width*/,		true /*canEdit*/ } ) },
	{ Playlist::Column::Track, ColumnFormat( {					ID_SHOWCOLUMNS_TRACK,					ID_SORTPLAYLIST_TRACK,					IDS_COLUMN_TRACK,					LVCFMT_CENTER,	50 /*width*/,		true /*canEdit*/ } ) },
	{ Playlist::Column::Type, ColumnFormat( {						ID_SHOWCOLUMNS_TYPE,					ID_SORTPLAYLIST_TYPE,						IDS_COLUMN_TYPE,					LVCFMT_LEFT,		50 /*width*/,		false /*canEdit*/ } ) },
	{ Playlist::Column::Version, ColumnFormat( {				ID_SHOWCOLUMNS_VERSION,				ID_SORTPLAYLIST_VERSION,				IDS_COLUMN_VERSION,				LVCFMT_LEFT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::GainTrack, ColumnFormat( {			ID_SHOWCOLUMNS_TRACKGAIN,			ID_SORTPLAYLIST_TRACKGAIN,			IDS_COLUMN_GAINTRACK,			LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::GainAlbum, ColumnFormat( {			ID_SHOWCOLUMNS_ALBUMGAIN,			ID_SORTPLAYLIST_ALBUMGAIN,			IDS_COLUMN_GAINALBUM,			LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) },
	{ Playlist::Column::Bitrate, ColumnFormat( {				ID_SHOWCOLUMNS_BITRATE,				ID_SORTPLAYLIST_BITRATE,				IDS_COLUMN_BITRATE,				LVCFMT_RIGHT,		100 /*width*/,	false /*canEdit*/ } ) }
};

// List control ID
UINT_PTR WndList::s_WndListID = 1000;

// File added message ID.
static const UINT MSG_FILEADDED = WM_APP + 100;

// File removed message ID.
static const UINT MSG_FILEREMOVED = WM_APP + 101;

// Message ID for reordering the dummy column after a drag operation.
static const UINT MSG_REORDERDUMMY = WM_APP + 102;

// Drag timer ID.
static const UINT_PTR s_DragTimerID = 1010;

// Drag timer millisecond interval.
static const UINT s_DragTimerInterval = 20;

// Window procedure
static LRESULT CALLBACK WndListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndList* wndList = reinterpret_cast<WndList*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndList ) {
		switch( message )	{
			case WM_COMMAND : {
				const UINT commandID = LOWORD( wParam );
				wndList->OnCommand( commandID );
				break;
			}
			case WM_DROPFILES : {
				wndList->OnDropFiles( reinterpret_cast<HDROP>( wParam ) );
				break;
			}
			case MSG_FILEADDED : {
				wndList->AddFileHandler( wParam );
				break;
			}
			case MSG_FILEREMOVED : {
				wndList->RemoveFileHandler( wParam );
				break;
			}
			case MSG_REORDERDUMMY : {
				wndList->ReorderDummyColumn();
				break;
			}
			case WM_LBUTTONDBLCLK : {
				LVHITTESTINFO info = {};
				info.pt.x = LOWORD( lParam );
				info.pt.y = HIWORD( lParam );
				if ( ( ListView_SubItemHitTest( hwnd, &info ) >= 0 ) && ( info.iItem >= 0 ) ) {
					const long playlistItemID = wndList->GetPlaylistItemID( info.iItem );
					wndList->OnPlay( playlistItemID );
				}
				break;
			}
			case WM_CONTEXTMENU : {
				POINT pt = {};
				pt.x = LOWORD( lParam );
				pt.y = HIWORD( lParam );
				wndList->OnContextMenu( pt );
				break;
			}
			case WM_DESTROY : {
				wndList->SaveSettings();
				break;
			}
			case WM_KEYDOWN : {
				switch ( wParam ) {
					case VK_DELETE : {
						wndList->DeleteSelectedItems();
						return 0;
					}
					case VK_RETURN : {
						const Playlist::Item item = wndList->GetCurrentSelectedItem();
						if ( item.ID > 0 ) {
							wndList->OnPlay( item.ID );
						}
						return 0;					
					}
					default : {
						break;
					}
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
				wndList->OnEndDrag();
				break;
			}
			case WM_MOUSEMOVE : {
				if ( GetCapture() == hwnd ) {
					const POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
					wndList->OnDrag( pt );
				}
				break;
			}
			case WM_TIMER : {
				if ( ( GetCapture() == hwnd ) && ( s_DragTimerID == wParam ) ) {
					wndList->OnDragTimer();
					return 0;
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndList->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

// Edit control window procedure
LRESULT CALLBACK EditControlProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndList* wndList = reinterpret_cast<WndList*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndList ) {
		switch( message ) {
			case WM_WINDOWPOSCHANGING : {
				wndList->RepositionEditControl( reinterpret_cast<WINDOWPOS*>( lParam ) );
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndList->GetEditControlWndProc(), hwnd, message, wParam, lParam );
}

WndList::WndList( HINSTANCE instance, HWND parent, Settings& settings, Output& output ) :
	m_hInst( instance ),
	m_hWnd( NULL ),
	m_DefaultWndProc( NULL ),
	m_Playlist(),
	m_Settings( settings ),
	m_Output( output ),
	m_ColourHighlight( GetSysColor( COLOR_HIGHLIGHT ) ),
	m_ChosenFont( NULL ),
	m_EditControl( NULL ),
	m_EditItem( -1 ),
	m_EditSubItem( -1 ),
	m_EditControlWndProc( NULL ),
	m_IsDragging( false ),
	m_DragImage( NULL ),
	m_OldCursor( NULL ),
	m_ItemFilenames()
{
	const DWORD exStyle = WS_EX_ACCEPTFILES;
	LPCTSTR className = WC_LISTVIEW;
	LPCTSTR windowName = NULL;
	const DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS;
	const int x = 100;
	const int y = 100;
	const int width = 600;
	const int height = 400;
	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( exStyle, className, windowName, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndListID++ ), instance, param );
	ListView_SetExtendedListViewStyle( m_hWnd, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_DOUBLEBUFFER );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	SetWindowLongPtr( ListView_GetHeader( m_hWnd ), GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( WndListProc ) ) );

	// Insert a dummy column.
	LVCOLUMN lvc = {};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_FIXED_WIDTH;
	ListView_InsertColumn( m_hWnd, 0 /*iCol*/, &lvc );

	ApplySettings();
}

WndList::~WndList()
{
	SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_DefaultWndProc ) );
	if ( nullptr != m_ChosenFont ) {
		DeleteObject( m_ChosenFont );
	}
}

WNDPROC WndList::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndList::GetWindowHandle()
{
	return m_hWnd;
}

void WndList::ApplySettings()
{
	Settings::PlaylistColumns columns;
	LOGFONT logFont = GetFont();
	COLORREF fontColour = ListView_GetTextColor( m_hWnd );
	COLORREF backgroundColour = ListView_GetBkColor( m_hWnd );
	COLORREF highlightColour = GetHighlightColour();

	m_Settings.GetPlaylistSettings( columns, logFont, fontColour, backgroundColour, highlightColour );

	if ( columns.empty() ) {
		columns = {
			{ static_cast<int>( Playlist::Column::Artist ), 150 /*width*/ },
			{ static_cast<int>( Playlist::Column::Title ), 150 /*width*/ },
			{ static_cast<int>( Playlist::Column::Album ), 150 /*width*/ },
			{ static_cast<int>( Playlist::Column::Duration ), 100 /*width*/ }
		};
	}
	bool titleShown = false;
	for ( auto iter = columns.begin(); !titleShown && ( iter != columns.end() ); iter++ ) {
		titleShown = ( Playlist::Column::Title == static_cast<Playlist::Column>( iter->ID ) );
	}
	if ( !titleShown ) {
		columns.push_front( { static_cast<int>( Playlist::Column::Title ), 150 /*width*/ } );
	}
	for ( const auto& iter : columns ) {
		ShowColumn( static_cast<Playlist::Column>( iter.ID ), iter.Width, true /*visible*/ );
	}

	ListView_SetTextColor( m_hWnd, fontColour );
	ListView_SetTextBkColor( m_hWnd, backgroundColour );
	ListView_SetBkColor( m_hWnd, backgroundColour );
	m_ColourHighlight = highlightColour;
	SetFont( logFont );
}

void WndList::SaveSettings()
{
	Settings::PlaylistColumns columnSettings;
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( NULL != headerWnd ) {
		const int columnCount = Header_GetItemCount( headerWnd );
		std::vector<int> columnArray( columnCount );
		if ( FALSE != ListView_GetColumnOrderArray( m_hWnd, columnCount, &columnArray[ 0 ] ) ) {
			for ( const auto& iter : columnArray ) {
				LVCOLUMN lvc = {};
				lvc.mask = LVCF_SUBITEM | LVCF_WIDTH;
				if ( ( FALSE != ListView_GetColumn( m_hWnd, iter, &lvc ) ) && ( lvc.iSubItem > 0 ) ) {
					const Settings::PlaylistColumn column = { lvc.iSubItem, lvc.cx };
					columnSettings.push_back( column );
				}
			}
		}
	}

	LOGFONT logFont = GetFont();
	COLORREF fontColour = ListView_GetTextColor( m_hWnd );
	COLORREF backgroundColour = ListView_GetBkColor( m_hWnd );
	COLORREF highlightColour = GetHighlightColour();
	m_Settings.SetPlaylistSettings( columnSettings, logFont, fontColour, backgroundColour, highlightColour );
}

void WndList::ShowColumn( const Playlist::Column column, const int width, const bool show )
{
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( NULL != headerWnd ) {
		bool columnShown = false;
		const int columnCount = Header_GetItemCount( headerWnd );
		for ( int columnIndex = 1; columnIndex < columnCount; columnIndex++ ) {
			LVCOLUMN lvc = {};
			lvc.mask = LVCF_SUBITEM;
			if ( FALSE != ListView_GetColumn( m_hWnd, columnIndex, &lvc ) ) {
				const Playlist::Column columnID = static_cast<Playlist::Column>( lvc.iSubItem );
				if ( columnID == column ) {
					columnShown = true;
					if ( !show ) {
						ListView_DeleteColumn( m_hWnd, columnIndex );
					}
					break;
				}
			}
		}

		if ( show && !columnShown ) {
			const auto iter = s_ColumnFormats.find( column );
			if ( iter != s_ColumnFormats.end() ) {
				const ColumnFormat& columnFormat = iter->second;
				LVCOLUMN lvc = {};
				lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
				lvc.cx = ( width > 0 ) ? width : columnFormat.Width;
				lvc.fmt = columnFormat.Alignment;
				lvc.iSubItem = static_cast<int>( column );
				const int bufSize = 32;
				WCHAR buffer[ bufSize ] = {};
				LoadString( m_hInst, columnFormat.HeaderID, buffer, bufSize );
				lvc.pszText = buffer;
				ListView_InsertColumn( m_hWnd, columnCount /*iCol*/, &lvc );
				RefreshListViewItemText();
				UpdateSortIndicator();

				// Force an update to show the horizontal scrollbar if necessary.
				ListView_Scroll( m_hWnd, 0, 0 );
			}
		}
	}
}

void WndList::OnDropFiles( const HDROP hDrop )
{
	if ( nullptr != hDrop ) {
		if ( !m_Playlist ) {
			VUPlayer* vuplayer = VUPlayer::Get();
			if ( nullptr != vuplayer ) {
				m_Playlist = vuplayer->NewPlaylist();
			}
		}
		if ( m_Playlist ) {
			const Playlist::Type type = m_Playlist->GetType();
			if ( ( Playlist::Type::User == type ) || ( Playlist::Type::All == type ) || ( Playlist::Type::Favourites == type ) ) {
				const int bufSize = 512;
				WCHAR filename[ bufSize ];
				const UINT fileCount = DragQueryFile( hDrop, 0xffffffff, nullptr /*filename*/, 0 /*bufSize*/ );
				for( UINT fileIndex = 0; fileIndex < fileCount; fileIndex++ ) {
					if ( 0 != DragQueryFile( hDrop, fileIndex, filename, bufSize ) ) {
						const DWORD attributes = GetFileAttributes( filename );
						if ( INVALID_FILE_ATTRIBUTES != attributes ) {
							if( attributes & FILE_ATTRIBUTE_DIRECTORY ) {
								AddFolderToPlaylist( filename );
							} else {
								AddFileToPlaylist( filename );
							}
						}
					}
				}
			}
		}
	}
}

void WndList::AddFileToPlaylist( const std::wstring& filename )
{
	if ( m_Playlist ) {
		m_Playlist->AddPending( filename );
	}
}

void WndList::AddFolderToPlaylist( const std::wstring& folder )
{
	WIN32_FIND_DATA findData;
	std::wstring str = folder;
	if ( !str.empty() && ( '\\' != str.back() ) ) {
		str += '\\';
	}
	str += L"*.*";
	const HANDLE handle = FindFirstFile( str.c_str(), &findData );
	if ( INVALID_HANDLE_VALUE != handle ) {
		BOOL found = TRUE;
		while ( found ) {
			if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
				if ( findData.cFileName[ 0 ] != '.' ) {
					str = folder;
					if ( !str.empty() && ( '\\' != str.back() ) ) {
						str += '\\';
					}
					str += findData.cFileName;
					AddFolderToPlaylist( str );
				}
			}
			else {
				str = folder;
				if ( !str.empty() && ( '\\' != str.back() ) ) {
					str += '\\';
				}
				str += findData.cFileName;
				AddFileToPlaylist( str );
			}
			found = FindNextFile( handle, &findData );
		}
		FindClose( handle );
	}	
}

void WndList::InsertListViewItem( const Playlist::Item& playlistItem, const int position )
{
	LVITEM item = {};
	item.mask = LVIF_PARAM;
	item.iItem = ( position < 0 ) ? ListView_GetItemCount( m_hWnd ) : position;
	item.iSubItem = 0;
	item.lParam = static_cast<LPARAM>( playlistItem.ID );
	item.iItem = ListView_InsertItem( m_hWnd, &item );
	if ( item.iItem >= 0 ) {
		m_ItemFilenames.insert( ItemFilenames::value_type( playlistItem.ID, playlistItem.Info.GetFilename() ) );
		SetListViewItemText( item.iItem, playlistItem.Info );
	}
}

void WndList::DeleteListViewItem( const int itemIndex )
{
	m_ItemFilenames.erase( GetPlaylistItemID( itemIndex ) );
	ListView_DeleteItem( m_hWnd, itemIndex );
}

void WndList::SetListViewItemText( int itemIndex, const MediaInfo& mediaInfo )
{
	LVCOLUMN column = {};
	column.mask = LVCF_SUBITEM;
	int columnIndex = 0;
	while ( FALSE != ListView_GetColumn( m_hWnd, columnIndex, &column ) ) {
		const Playlist::Column columnID = static_cast<Playlist::Column>( column.iSubItem );
		switch( columnID ) {
			case Playlist::Column::Album : {
				const std::wstring str = mediaInfo.GetAlbum();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Artist : {
				const std::wstring str = mediaInfo.GetArtist();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Bitrate : {
				std::wstringstream ss;
				const long bitrate = mediaInfo.GetBitrate();
				if ( bitrate > 0 ) {
					const int bufSize = 16;
					WCHAR buf[ bufSize ] = {};
					if ( 0 != LoadString( m_hInst, IDS_UNITS_BITRATE, buf, bufSize ) ) {
						ss << std::to_wstring( bitrate ) << L" " << buf;
					}
				}
				const std::wstring str = ss.str();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::BitsPerSample : {
				const long bitsPerSample = mediaInfo.GetBitsPerSample();
				const std::wstring str = ( bitsPerSample > 0 ) ? std::to_wstring( bitsPerSample ) : std::wstring();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Channels : {
				const long channels = mediaInfo.GetChannels();
				const std::wstring str = ( channels > 0 ) ? std::to_wstring( channels ) : std::wstring();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Duration : {
				const float duration = mediaInfo.GetDuration();
				const std::wstring str = ( duration > 0 ) ? DurationToString( m_hInst, mediaInfo.GetDuration(), true /*colonDelimited*/ ) : std::wstring();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Filename : {
				const std::wstring str = mediaInfo.GetFilename();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Filesize : {
				const std::wstring str = FilesizeToString( m_hInst, mediaInfo.GetFilesize() );
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Filetime : {
				std::wstringstream ss;
				const long long filetime = mediaInfo.GetFiletime();
				if ( filetime > 0 ) {
					FILETIME ft;
					ft.dwHighDateTime = static_cast<DWORD>( filetime >> 32 );
					ft.dwLowDateTime = static_cast<DWORD>( filetime & 0xffffffff );
					SYSTEMTIME st;
					if ( 0 !=	FileTimeToSystemTime( &ft, &st ) ) {
						SYSTEMTIME lt;
						if ( 0 != SystemTimeToTzSpecificLocalTime( NULL /*timeZone*/, &st, &lt ) ) {
							const int bufSize = 128;
							WCHAR dateBuf[ bufSize ] = {};
							WCHAR timeBuf[ bufSize ] = {};
							if ( ( 0 != GetDateFormat( LOCALE_USER_DEFAULT, DATE_SHORTDATE, &lt, NULL /*format*/, dateBuf, bufSize ) ) &&
									( 0 != GetTimeFormat( LOCALE_USER_DEFAULT, TIME_NOSECONDS, &lt, NULL /*format*/, timeBuf, bufSize ) ) ) {
								ss << dateBuf << L" " << timeBuf;
							}
						}
					}
				}
				const std::wstring str = ss.str();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::GainAlbum :
			case Playlist::Column::GainTrack : {
				std::wstringstream ss;
				const float gain = ( Playlist::Column::GainAlbum == columnID ) ? mediaInfo.GetGainAlbum() : mediaInfo.GetGainTrack();
				if ( REPLAYGAIN_NOVALUE != gain ) {
					const int bufSize = 16;
					WCHAR buf[ bufSize ] = {};
					if ( 0 != LoadString( m_hInst, IDS_UNITS_DB, buf, bufSize ) ) {
						if ( gain > 0 ) {
							ss << L"+";
						}
						ss << std::fixed << std::setprecision( 2 ) << gain << L" " << buf;
					}
				}
				const std::wstring str = ss.str();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Genre : {
				const std::wstring str = mediaInfo.GetGenre();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::SampleRate : {
				std::wstringstream ss;
				const long rate = mediaInfo.GetSampleRate();
				if ( rate > 0 ) {
					const int bufSize = 16;
					WCHAR buf[ bufSize ] = {};
					if ( 0 != LoadString( m_hInst, IDS_UNITS_HZ, buf, bufSize ) ) {
						ss << std::to_wstring( rate ) << L" " << buf;
					}
				}
				const std::wstring str = ss.str();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;				
			}
			case Playlist::Column::Title : {
				const std::wstring str = mediaInfo.GetTitle( true /*filenameAsTitle*/ );
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Track : {
				const long track = mediaInfo.GetTrack();
				const std::wstring str = ( track > 0 ) ? std::to_wstring( track ) : std::wstring();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Type : {
				const std::wstring str = mediaInfo.GetType();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Version : {
				const std::wstring str = mediaInfo.GetVersion();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			case Playlist::Column::Year : {
				const long year = mediaInfo.GetYear();
				const std::wstring str = ( year > 0 ) ? std::to_wstring( year ) : std::wstring();
				ListView_SetItemText( m_hWnd, itemIndex, columnIndex, const_cast<LPWSTR>( str.c_str() ) );
				break;
			}
			default : {
				break;
			}
		}
		++columnIndex;
	}
	ListView_SetItemText( m_hWnd, itemIndex, 0 /*subItem*/, const_cast<LPWSTR>( mediaInfo.GetTitle( true /*filenameAsTitle*/ ).c_str() ) );
}

void WndList::OnPlay( const long itemID )
{
	m_Output.SetPlaylist( m_Playlist );
	m_Output.Play( itemID );
}

void WndList::OnContextMenu( const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_LIST ) );
	if ( NULL != menu ) {
		HMENU listmenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != listmenu ) {
			std::set<UINT> shownColumns;
			std::set<UINT> hiddenColumns;
			GetColumnVisibility( shownColumns, hiddenColumns );
			for ( const auto& hiddenColumn : hiddenColumns ) {
				CheckMenuItem( listmenu, hiddenColumn, MF_BYCOMMAND | MF_UNCHECKED );
			}
			for ( const auto& shownColumn : shownColumns ) {
				CheckMenuItem( listmenu, shownColumn, MF_BYCOMMAND | MF_CHECKED );
			}

			const bool hasItems = ( ListView_GetItemCount( m_hWnd ) > 0 );
			const bool hasSelectedItems = ( ListView_GetSelectedCount( m_hWnd ) > 0 );
			const bool allowPaste = ( m_Playlist && ( ( Playlist::Type::User == m_Playlist->GetType() ) || ( Playlist::Type::All == m_Playlist->GetType() ) || ( Playlist::Type::Favourites == m_Playlist->GetType() ) ) );
			const bool allowCut = ( m_Playlist && ( ( Playlist::Type::User == m_Playlist->GetType() ) || ( Playlist::Type::Favourites == m_Playlist->GetType() ) ) );
			const bool allowCopy = ( m_Playlist && ( Playlist::Type::CDDA != m_Playlist->GetType() ) );

			const UINT enablePaste = ( allowPaste && ( ( FALSE != IsClipboardFormatAvailable( CF_TEXT ) ) || ( FALSE != IsClipboardFormatAvailable( CF_UNICODETEXT ) ) ) ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_PASTE, MF_BYCOMMAND | enablePaste );

			const UINT enableCut = ( allowCut && hasSelectedItems ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_CUT, MF_BYCOMMAND | enableCut );

			const UINT enableCopy = ( allowCopy && hasSelectedItems ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_COPY, MF_BYCOMMAND | enableCopy );

			const UINT enableSelectAll = hasItems ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_SELECTALL, MF_BYCOMMAND | enableSelectAll );

			const UINT enableTrackInfo = hasSelectedItems ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_VIEW_TRACKINFORMATION, MF_BYCOMMAND | enableTrackInfo );

			const UINT enableAddFiles = allowPaste ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_PLAYLISTADDFOLDER, MF_BYCOMMAND | enableAddFiles );
			EnableMenuItem( listmenu, ID_FILE_PLAYLISTADDFILES, MF_BYCOMMAND | enableAddFiles );
			const UINT enableRemoveFiles = ( allowCut && hasSelectedItems ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_PLAYLISTREMOVEFILES, MF_BYCOMMAND | enableRemoveFiles );
			const UINT enableAddToFavourites = ( m_Playlist && ( Playlist::Type::Favourites != m_Playlist->GetType() ) && ( Playlist::Type::CDDA != m_Playlist->GetType() ) && hasSelectedItems ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_ADDTOFAVOURITES, MF_BYCOMMAND | enableAddToFavourites );

			const UINT enableExtract = ( m_Playlist && ( Playlist::Type::CDDA == m_Playlist->GetType() ) ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_CONVERT, MF_BYCOMMAND | enableExtract );

			const UINT enableReplayGain = hasSelectedItems ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_CALCULATEREPLAYGAIN, MF_BYCOMMAND | enableReplayGain );

			VUPlayer* vuplayer = VUPlayer::Get();

			const UINT gracenoteEnabled = ( m_Playlist && ( Playlist::Type::CDDA == m_Playlist->GetType() ) && ( nullptr != vuplayer ) && vuplayer->IsGracenoteEnabled() ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( listmenu, ID_FILE_GRACENOTE_QUERY, MF_BYCOMMAND | gracenoteEnabled );

			if ( nullptr != vuplayer ) {
				vuplayer->InsertAddToPlaylists( listmenu, false /*addPrefix*/ );
			}

			const UINT flags = TPM_RIGHTBUTTON;
			TrackPopupMenu( listmenu, flags, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}

void WndList::OnCommand( const UINT command )
{
	switch ( command ) {
		case ID_SHOWCOLUMNS_ARTIST :
		case ID_SHOWCOLUMNS_ALBUM :
		case ID_SHOWCOLUMNS_GENRE :
		case ID_SHOWCOLUMNS_YEAR :
		case ID_SHOWCOLUMNS_TRACK :
		case ID_SHOWCOLUMNS_TYPE :
		case ID_SHOWCOLUMNS_VERSION :
		case ID_SHOWCOLUMNS_SAMPLERATE :
		case ID_SHOWCOLUMNS_CHANNELS :
		case ID_SHOWCOLUMNS_BITRATE :
		case ID_SHOWCOLUMNS_BITSPERSAMPLE :
		case ID_SHOWCOLUMNS_DURATION :
		case ID_SHOWCOLUMNS_FILESIZE :
		case ID_SHOWCOLUMNS_FILENAME :
		case ID_SHOWCOLUMNS_FILETIME :
		case ID_SHOWCOLUMNS_TRACKGAIN :
		case ID_SHOWCOLUMNS_ALBUMGAIN : {
			OnShowColumn( command );
			break;
		}
		case ID_SORTPLAYLIST_ARTIST :
		case ID_SORTPLAYLIST_ALBUM :
		case ID_SORTPLAYLIST_GENRE :
		case ID_SORTPLAYLIST_YEAR :
		case ID_SORTPLAYLIST_TRACK :
		case ID_SORTPLAYLIST_TYPE :
		case ID_SORTPLAYLIST_VERSION :
		case ID_SORTPLAYLIST_SAMPLERATE :
		case ID_SORTPLAYLIST_CHANNELS :
		case ID_SORTPLAYLIST_BITRATE :
		case ID_SORTPLAYLIST_BITSPERSAMPLE :
		case ID_SORTPLAYLIST_DURATION :
		case ID_SORTPLAYLIST_FILESIZE :
		case ID_SORTPLAYLIST_FILENAME :
		case ID_SORTPLAYLIST_FILETIME :
		case ID_SORTPLAYLIST_TRACKGAIN :
		case ID_SORTPLAYLIST_ALBUMGAIN : {
			OnSortPlaylist( command );
			break;
		}
		case ID_LISTMENU_FONTSTYLE : {
			OnSelectFont();
			break;
		}
		case ID_LISTMENU_FONTCOLOUR : 
		case ID_LISTMENU_BACKGROUNDCOLOUR :
		case ID_LISTMENU_HIGHLIGHTCOLOUR : {
			OnSelectColour( command );
			break;
		}
		case ID_FILE_SELECTALL : {
			SelectAllItems();
			break;
		}
		case ID_FILE_PLAYLISTADDFOLDER : {
			OnCommandAddFolder();
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			OnCommandAddFiles();
			break;
		}
		case ID_FILE_PLAYLISTREMOVEFILES : {
			DeleteSelectedItems();
			break;
		}
		case ID_FILE_CUT : {
			OnCutCopy( true /*cut*/ );
			break;
		}
		case ID_FILE_COPY : {
			OnCutCopy( false /*cut*/ );
			break;
		}
		case ID_FILE_PASTE : {
			OnPaste();
			break;
		}
		default : {
			VUPlayer* vuplayer = VUPlayer::Get();
			if ( nullptr != vuplayer ) {
				vuplayer->OnCommand( command );
			}
			break;
		}
	}
}

bool WndList::IsColumnShown( const Playlist::Column& column ) const
{
	bool shown = false;
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( NULL != headerWnd ) {
		const int columnCount = Header_GetItemCount( headerWnd );
		for ( int columnIndex = 1; !shown && ( columnIndex < columnCount ); columnIndex++ ) {
			LVCOLUMN lvc = {};
			lvc.mask = LVCF_SUBITEM;
			if ( FALSE != ListView_GetColumn( m_hWnd, columnIndex, &lvc ) ) {
				shown = ( static_cast<Playlist::Column>( lvc.iSubItem ) == column );
			}
		}
	}
	return shown;
}

void WndList::RefreshListViewItemText()
{
	if ( m_Playlist ) {
		const int itemCount = ListView_GetItemCount( m_hWnd );
		for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
			Playlist::Item item( { GetPlaylistItemID( itemIndex ), MediaInfo() } );
			if ( m_Playlist->GetItem( item ) ) {
				SetListViewItemText( itemIndex, item.Info );
			}
		}
	}
}

void WndList::DeleteSelectedItems()
{
	if ( m_Playlist && ( ( m_Playlist->GetType() == Playlist::Type::User ) || ( m_Playlist->GetType() == Playlist::Type::Favourites ) ) ) {
		SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
		int itemIndex = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
		const int selectItem = itemIndex;
		while ( itemIndex != -1 ) {
			Playlist::Item playlistItem( { GetPlaylistItemID( itemIndex ), MediaInfo() } );
			m_Playlist->RemoveItem( playlistItem );
			DeleteListViewItem( itemIndex );
			itemIndex = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
		}
		ListView_SetItemState( m_hWnd, selectItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
	}
}

void WndList::SetPlaylist( const Playlist::Ptr playlist, const bool initSelection )
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	ListView_DeleteAllItems( m_hWnd );
	m_ItemFilenames.clear();
	if ( m_Playlist != playlist ) {
		m_Playlist = playlist;
	}
	if ( m_Playlist ) {
		const Playlist::ItemList& playlistItems = m_Playlist->GetItems();
		for ( const auto& iter : playlistItems ) {
			InsertListViewItem( iter );
		}
	}
	if ( initSelection ) {
		const int itemCount = ListView_GetItemCount( m_hWnd );
		if ( itemCount > 0 ) {
			int selectedIndex = 0;
			const long currentPlaying = m_Output.GetCurrentPlaying().PlaylistItem.ID;
			for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				const long playlistID = GetPlaylistItemID( itemIndex );
				if ( playlistID == currentPlaying ) {
					selectedIndex = itemIndex;
					break;
				}
			}
			ListView_SetItemState( m_hWnd, selectedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
			ListView_EnsureVisible( m_hWnd, selectedIndex, FALSE /*partialOK*/ );
		}
	}

	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
	RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );

	UpdateSortIndicator();
}

void WndList::UpdateSortIndicator()
{
	Playlist::Column column = Playlist::Column::_Undefined;
	bool sortAscending = false;
	if ( m_Playlist ) {
		m_Playlist->GetSort( column, sortAscending );
	}
	const int sortColumn = ( Playlist::Column::_Undefined == column ) ? 0 : static_cast<int>( column );
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( nullptr != headerWnd ) {
		const int itemCount = Header_GetItemCount( headerWnd );
		for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
			HDITEM headerItem = {};
			headerItem.mask = HDI_LPARAM | HDI_FORMAT;
			if ( ( TRUE == Header_GetItem( headerWnd, itemIndex, &headerItem ) ) && ( 0 != headerItem.lParam ) ) {
				const int previousFormat = headerItem.fmt;
				headerItem.fmt &= ~( HDF_SORTUP | HDF_SORTDOWN );
				if ( headerItem.lParam == sortColumn ) {
					headerItem.fmt |= sortAscending ? HDF_SORTUP : HDF_SORTDOWN;
				}
				if ( headerItem.fmt != previousFormat ) {
					headerItem.mask = HDI_FORMAT;
					Header_SetItem( headerWnd, itemIndex, &headerItem );
				}
			}
		}
	}
}

void WndList::OnFileAdded( Playlist* playlist, const Playlist::Item& item, const int position )
{
	if ( nullptr != playlist ) {
		AddedItem* addedItem = new AddedItem( { playlist, item, position } );
		PostMessage( m_hWnd, MSG_FILEADDED, reinterpret_cast<WPARAM>( addedItem ), 0 /*lParam*/ );
	}
}

void WndList::AddFileHandler( WPARAM wParam )
{
	AddedItem* addedItem = reinterpret_cast<AddedItem*>( wParam );
	if ( nullptr != addedItem ) {
		if ( addedItem->Playlist == m_Playlist.get() ) {
			InsertListViewItem( addedItem->Item, addedItem->Position );
		}
		delete addedItem;
	}
}

void WndList::OnFileRemoved( Playlist* playlist, const Playlist::Item& item )
{
	if ( nullptr != playlist ) {
		PostMessage( m_hWnd, MSG_FILEREMOVED, item.ID, 0 /*lParam*/ );
	}
}

void WndList::RemoveFileHandler( WPARAM wParam )
{
	const long removedItemID = static_cast<long>( wParam );
	const int itemCount = ListView_GetItemCount( m_hWnd );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		const long itemID = GetPlaylistItemID( itemIndex );
		if ( itemID == removedItemID ) {
			DeleteListViewItem( itemIndex );
			break;
		}
	}
}

void WndList::SortPlaylist( const Playlist::Column column )
{
	if ( m_Playlist ) {
		m_Playlist->Sort( column );
		SetPlaylist( m_Playlist );
	}
}

COLORREF WndList::GetHighlightColour() const
{
	return m_ColourHighlight;
}

void WndList::OnSelectFont()
{
	LOGFONT logFont = GetFont();

	CHOOSEFONT chooseFont = {};
	chooseFont.lStructSize = sizeof( CHOOSEFONT );
	chooseFont.hwndOwner = m_hWnd;
	chooseFont.Flags = CF_FORCEFONTEXIST | CF_NOVERTFONTS | CF_LIMITSIZE | CF_INITTOLOGFONTSTRUCT;
	chooseFont.nSizeMax = 32;
	chooseFont.lpLogFont = &logFont;

	if ( ( TRUE == ChooseFont( &chooseFont ) ) && ( nullptr != chooseFont.lpLogFont ) ) {
		SetFont( *chooseFont.lpLogFont );
	}
}

void WndList::OnSelectColour( const UINT commandID )
{
	CHOOSECOLOR chooseColor = {};
	chooseColor.lStructSize = sizeof( CHOOSECOLOR );
	chooseColor.hwndOwner = m_hWnd;
	VUPlayer* vuplayer = VUPlayer::Get();
	if ( nullptr != vuplayer ) {
		chooseColor.lpCustColors = vuplayer->GetCustomColours();
	}
	chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;

	switch ( commandID ) {
		case ID_LISTMENU_FONTCOLOUR : {
			chooseColor.rgbResult = ListView_GetTextColor( m_hWnd );
			break;
		}
		case ID_LISTMENU_BACKGROUNDCOLOUR : {
			chooseColor.rgbResult = ListView_GetTextBkColor( m_hWnd );
			break;
		}
		case ID_LISTMENU_HIGHLIGHTCOLOUR : {
			chooseColor.rgbResult = GetHighlightColour();
			break;
		}
		default : {
			break;
		}
	}

	if ( TRUE == ChooseColor( &chooseColor ) ) {
		switch ( commandID ) {
			case ID_LISTMENU_FONTCOLOUR : {
				ListView_SetTextColor( m_hWnd, chooseColor.rgbResult );
				break;
			}
			case ID_LISTMENU_BACKGROUNDCOLOUR : {
				ListView_SetTextBkColor( m_hWnd, chooseColor.rgbResult );
				ListView_SetBkColor( m_hWnd, chooseColor.rgbResult );
				break;
			}
			case ID_LISTMENU_HIGHLIGHTCOLOUR : {
				m_ColourHighlight = chooseColor.rgbResult;
				break;
			}
			default : {
				break;
			}
		}
		RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
	}
}

LOGFONT WndList::GetFont()
{
	LOGFONT logFont = {};
	const HFONT hFont = reinterpret_cast<HFONT>( SendMessage( m_hWnd, WM_GETFONT, 0, 0 ) );
	if ( nullptr != hFont ) {
		const int bufSize = GetObject( hFont, 0, NULL );
		if ( bufSize > 0 ) {
			unsigned char* buf = new unsigned char[ bufSize ];
			if ( bufSize == GetObject( hFont, bufSize, buf ) ) {
				LOGFONT* currentLogFont = reinterpret_cast<LOGFONT*>( buf );
				if ( nullptr != currentLogFont ) {
					logFont = *currentLogFont;
				}
			}
			delete [] buf;
		}
	}
	return logFont;
}

void WndList::SetFont( const LOGFONT& logFont )
{
	const HFONT hNewFont = CreateFontIndirect( &logFont );
	SendMessage( m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>( hNewFont ), FALSE /*redraw*/ );
	RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
	if ( nullptr != m_ChosenFont ) {
		DeleteObject( m_ChosenFont );
	}
	m_ChosenFont = hNewFont;
}

BOOL WndList::OnBeginLabelEdit( const LVITEM& item )
{
	BOOL denyEdit = TRUE;

	LVHITTESTINFO lvh = {};
	GetCursorPos( &lvh.pt );
	ScreenToClient( m_hWnd, &lvh.pt );
	if ( ( ListView_SubItemHitTest( m_hWnd, &lvh ) >= 0 ) && ( lvh.iItem == item.iItem ) ) {
		LVCOLUMN lvc = {};
		lvc.mask = LVCF_SUBITEM;
		if ( TRUE == ListView_GetColumn( m_hWnd, lvh.iSubItem, &lvc ) ) {
			const Playlist::Column columnID = static_cast<Playlist::Column>( lvc.iSubItem );
			const auto columnIter = s_ColumnFormats.find( columnID );
			const bool denyTrackNumberEdit = ( Playlist::Column::Track == columnID ) && m_Playlist && ( Playlist::Type::CDDA == m_Playlist->GetType() );
			if ( ( s_ColumnFormats.end() != columnIter ) && ( columnIter->second.CanEdit ) && !denyTrackNumberEdit ) {
				m_EditControl = ListView_GetEditControl( m_hWnd );
				if ( nullptr != m_EditControl ) {
					const int bufSize = 1024;
					WCHAR buf[ bufSize ] = {};
					ListView_GetItemText( m_hWnd, lvh.iItem, lvh.iSubItem, buf, bufSize );
					SetWindowText( m_EditControl, buf );
					m_EditItem = lvh.iItem;
					m_EditSubItem = lvh.iSubItem;
					SetWindowLongPtr( m_EditControl, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
					m_EditControlWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_EditControl, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( EditControlProc ) ) );
					denyEdit = FALSE;
				}
			}
		}		
	}
	return denyEdit;
}

void WndList::OnEndLabelEdit( const LVITEM& item )
{
	if ( nullptr != m_EditControl )	{
		if ( nullptr != item.pszText ) {
			if ( m_Playlist ) {
				Playlist::Item playlistItem = {	static_cast<long>( item.lParam ), MediaInfo() };
				if ( m_Playlist->GetItem( playlistItem ) ) {
					m_Playlist->GetLibrary().GetMediaInfo( playlistItem.Info, false /*checkFileAttributes*/, false /*scanMedia*/ );
					const MediaInfo previousMediaInfo( playlistItem.Info );
					LVCOLUMN lvc = {};
					lvc.mask = LVCF_SUBITEM;
					if ( FALSE != ListView_GetColumn( m_hWnd, m_EditSubItem, &lvc ) ) {
						Library::Column libraryColumnID = Library::Column::_Undefined;
						const Playlist::Column playlistColumnID = static_cast<Playlist::Column>( lvc.iSubItem );
						switch ( playlistColumnID ) {
							case Playlist::Column::Album : {
								libraryColumnID = Library::Column::Album;
								playlistItem.Info.SetAlbum( item.pszText );
								break;
							}
							case Playlist::Column::Artist : {
								libraryColumnID = Library::Column::Artist;
								playlistItem.Info.SetArtist( item.pszText );
								break;
							}
							case Playlist::Column::Genre : {
								libraryColumnID = Library::Column::Genre;
								playlistItem.Info.SetGenre( item.pszText );
								break;
							}
							case Playlist::Column::Title : {
								libraryColumnID = Library::Column::Title;
								playlistItem.Info.SetTitle( item.pszText );
								break;
							}
							case Playlist::Column::Track : {
								long tracknumber = 0;
								try {
									tracknumber = std::stol( item.pszText );
								} catch ( ... ) {
								}
								libraryColumnID = Library::Column::Track;
								playlistItem.Info.SetTrack( tracknumber );
								break;
							}
							case Playlist::Column::Year : {
								long year = 0;
								try {
									year = std::stol( item.pszText );
								} catch ( ... ) {
								}
								libraryColumnID = Library::Column::Year;
								playlistItem.Info.SetYear( year );
								break;
							}
							default : {
								break;
							}
						}
						if ( Library::Column::_Undefined != libraryColumnID ) {
							Library& library = m_Playlist->GetLibrary();
							library.UpdateMediaTags( previousMediaInfo, playlistItem.Info );
						}
					}
				}
			}
		}
		SetWindowLongPtr( m_EditControl, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_EditControlWndProc ) );
		m_EditControl = nullptr;
		m_EditControlWndProc = nullptr;
		m_EditItem = -1;
		m_EditSubItem = -1;
	}
}

void WndList::OnEndDragColumn()
{
	PostMessage( m_hWnd, MSG_REORDERDUMMY, 0 /*wParam*/, 0 /*lParam*/ );
}

void WndList::ReorderDummyColumn()
{
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( NULL != headerWnd ) {
		const int columnCount = Header_GetItemCount( headerWnd );
		if ( columnCount > 1 ) {
			std::vector<int> columnArray( columnCount );
			if ( ( FALSE != ListView_GetColumnOrderArray( m_hWnd, columnCount, &columnArray[ 0 ] ) ) && ( columnArray[ 1 ] == 0 ) ) {
				columnArray[ 1 ] = columnArray[ 0 ];
				columnArray[ 0 ] = 0;
				ListView_SetColumnOrderArray( m_hWnd, columnCount, &columnArray[ 0 ] );
			}
		}
	}
}

WNDPROC WndList::GetEditControlWndProc()
{
	return m_EditControlWndProc;
}

void WndList::RepositionEditControl( WINDOWPOS* position )
{
	RECT rect = {};
	if ( 0 != ListView_GetSubItemRect( m_hWnd, m_EditItem, m_EditSubItem, LVIR_BOUNDS, &rect ) ) {
		position->x = ( rect.left < 0 ) ? 0 : rect.left;
		position->y = rect.top;
		position->cy = rect.bottom - rect.top;
		position->cx = rect.right - rect.left;
		GetClientRect( m_hWnd, &rect );
		if( ( position->x + position->cx ) > rect.right ) {
			position->cx = rect.right - position->x;
		}
	}
}

void WndList::OnUpdatedMedia( const MediaInfo& mediaInfo )
{
	const int itemCount = ListView_GetItemCount( m_hWnd );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		const auto itemFilename = m_ItemFilenames.find( GetPlaylistItemID( itemIndex ) );
		if ( ( m_ItemFilenames.end() != itemFilename ) && ( itemFilename->second == mediaInfo.GetFilename() ) ) {
			SetListViewItemText( itemIndex, mediaInfo );
		}
	}
}

long WndList::GetPlaylistItemID( const int itemIndex )
{
	LVITEM item = {};
	item.iItem = itemIndex;
	item.mask = LVIF_PARAM;
	const long playlistItemID = ( TRUE == ListView_GetItem( m_hWnd, &item ) ) ? static_cast<long>( item.lParam ) : 0;
	return playlistItemID;
}

Playlist::Ptr WndList::GetPlaylist()
{
	return m_Playlist;
}

Playlist::Item WndList::GetCurrentSelectedItem()
{
	Playlist::Item item = {};
	const int itemIndex = GetCurrentSelectedIndex();
	if ( itemIndex >= 0 ) {
		item = { GetPlaylistItemID( itemIndex ), MediaInfo() };
		if ( m_Playlist ) { 
			m_Playlist->GetItem( item );
		}
	}
	return item;
}

int WndList::GetCurrentSelectedIndex()
{
	int itemIndex = -1;
	if ( ListView_GetSelectedCount( m_hWnd ) > 0 ) {
		itemIndex = static_cast<int>( ListView_GetNextItem( m_hWnd, -1 /*start*/, LVNI_ALL | LVNI_FOCUSED ) );
	}
	return itemIndex;
}

void WndList::OnBeginDrag( const int itemIndex )
{
	SetCapture( m_hWnd );
	m_OldCursor = SetCursor( LoadCursor( NULL /*inst*/, IDC_HAND ) );
	m_IsDragging = true;
	const POINT point = {};
	m_DragImage = ListView_CreateDragImage( m_hWnd, itemIndex, &point );
	SetTimer( m_hWnd, s_DragTimerID, s_DragTimerInterval, NULL /*timerFunc*/ );
}

void WndList::OnEndDrag()
{
	if ( m_IsDragging ) {
		m_IsDragging = false;
		KillTimer( m_hWnd, s_DragTimerID );
		if ( nullptr != m_DragImage ) {
			ImageList_Destroy( m_DragImage );
			m_DragImage = m_DragImage;
		}
		if ( nullptr != m_OldCursor ) {
			SetCursor( m_OldCursor );
			m_OldCursor = nullptr;
		}
		LVINSERTMARK lvim = {};
		lvim.cbSize = sizeof( LVINSERTMARK );
		ListView_GetInsertMark( m_hWnd, &lvim );
		int insertionIndex = lvim.iItem;
		if ( insertionIndex >= 0 ) {
			if ( LVIM_AFTER & lvim.dwFlags ) {
				++insertionIndex;
			}
			MoveSelectedItems( insertionIndex );
		}
		lvim.iItem = -1;
		ListView_SetInsertMark( m_hWnd, &lvim );
	}
}

void WndList::OnDrag( const POINT& point )
{
	if ( m_IsDragging ) {
		LVHITTESTINFO hittest = {};
		hittest.pt = point;
		const int itemIndex = ListView_HitTest( m_hWnd, &hittest );
		LVINSERTMARK lvim = {};
		lvim.cbSize = sizeof( LVINSERTMARK );
		lvim.iItem = -1;
		if ( -1 != itemIndex ) {
			lvim.iItem = hittest.iItem;
			const int itemCount = ListView_GetItemCount( m_hWnd );
			if ( 1 + lvim.iItem == itemCount ) {
				RECT itemRect = {};
				ListView_GetItemRect( m_hWnd, lvim.iItem, &itemRect, LVIR_BOUNDS );
				if ( static_cast<int>( hittest.pt.y ) > ( ( itemRect.top + itemRect.bottom ) / 2 ) ) {
					lvim.dwFlags = LVIM_AFTER;
				}
			}
		}
		ListView_SetInsertMark( m_hWnd, &lvim );
	}
}

void WndList::OnDragTimer()
{
	POINT pt = {};
	GetCursorPos( &pt );
	RECT listRect = {};
	GetWindowRect( m_hWnd, &listRect );
	if ( ( pt.y < listRect.top ) || ( pt.y > listRect.bottom ) ) {
		RECT itemRect = {};
		ListView_GetItemRect( m_hWnd, 0, &itemRect, LVIR_BOUNDS );
		const int itemHeight = itemRect.bottom - itemRect.top;
		const int dy = ( pt.y < listRect.top ) ? ( pt.y - listRect.top - itemHeight / 2 ) : ( pt.y - listRect.bottom + itemHeight / 2 );
		LVINSERTMARK lvim = {};
		lvim.cbSize = sizeof( LVINSERTMARK );
		lvim.iItem = -1;
		ListView_SetInsertMark( m_hWnd, &lvim );
		ListView_Scroll( m_hWnd, 0 /*dx*/, dy );	
	}
}

void WndList::MoveSelectedItems( const int insertionIndex )
{
	if ( m_Playlist ) {
		int itemIndex = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
		std::list<long> itemsToMove;
		while ( itemIndex >= 0 ) {
			const long itemID = GetPlaylistItemID( itemIndex );
			itemsToMove.push_back( itemID );
			itemIndex = ListView_GetNextItem( m_hWnd, itemIndex, LVNI_SELECTED );
		}
		if ( m_Playlist->MoveItems( insertionIndex, itemsToMove ) ) {
			RECT rect = {};
			ListView_GetItemRect( m_hWnd, 0, &rect, LVIR_BOUNDS );
			const int itemHeight = rect.bottom - rect.top;
			GetClientRect( m_hWnd, &rect );
			LVHITTESTINFO hittest = {};
			hittest.pt = { 0 /*x*/, rect.bottom - itemHeight /*y*/ };
			const int lastVisibleItem = ListView_HitTest( m_hWnd, &hittest );

			SetPlaylist( m_Playlist, false /*initSelection*/ );
			const int itemCount = ListView_GetItemCount( m_hWnd );
			for ( itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				const long itemID = GetPlaylistItemID( itemIndex );
				const bool movedItem = ( itemsToMove.end() != std::find( itemsToMove.begin(), itemsToMove.end(), itemID ) );
				if ( movedItem ) {
					ListView_SetItemState( m_hWnd, itemIndex, LVIS_SELECTED, LVIS_SELECTED );
				}
			}
			const int firstSelectedItem = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
			if ( firstSelectedItem >= 0 ) {
				ListView_SetItemState( m_hWnd, firstSelectedItem, LVIS_FOCUSED, LVIS_FOCUSED );
				ListView_EnsureVisible( m_hWnd, ( lastVisibleItem >= 0 ) ? lastVisibleItem : firstSelectedItem, FALSE /*partialOK*/ );
			}
		}
	}
}

void WndList::SelectPreviousItem()
{
	if ( m_Playlist ) {
		const Playlist::Item currentItem = GetCurrentSelectedItem();
		Playlist::Item previousItem = {};
		if ( m_Playlist->GetPreviousItem( currentItem, previousItem ) ) {
			// Deselect all items.
			int selectedItem = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
			while ( selectedItem >= 0 ) {
				ListView_SetItemState( m_hWnd, selectedItem, 0, LVIS_SELECTED | LVIS_FOCUSED );
				selectedItem = ListView_GetNextItem( m_hWnd, selectedItem, LVNI_SELECTED );
			}
			// Select previous item.
			const int itemCount = ListView_GetItemCount( m_hWnd );
			for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				const long itemID = GetPlaylistItemID( itemIndex );
				if ( itemID == previousItem.ID ) {
					ListView_SetItemState( m_hWnd, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
					ListView_EnsureVisible( m_hWnd, itemIndex, FALSE /*partialOK*/ );
					break;
				}
			}
		}
	}
}

void WndList::SelectNextItem()
{
	if ( m_Playlist ) {
		const Playlist::Item currentItem = GetCurrentSelectedItem();
		Playlist::Item nextItem = {};
		if ( m_Playlist->GetNextItem( currentItem, nextItem ) ) {
			// Deselect all items.
			int selectedItem = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
			while ( selectedItem >= 0 ) {
				ListView_SetItemState( m_hWnd, selectedItem, 0, LVIS_SELECTED | LVIS_FOCUSED );
				selectedItem = ListView_GetNextItem( m_hWnd, selectedItem, LVNI_SELECTED );
			}
			// Select next item.
			const int itemCount = ListView_GetItemCount( m_hWnd );
			for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				const long itemID = GetPlaylistItemID( itemIndex );
				if ( itemID == nextItem.ID ) {
					ListView_SetItemState( m_hWnd, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
					ListView_EnsureVisible( m_hWnd, itemIndex, FALSE /*partialOK*/ );
					break;
				}
			}
		}
	}
}

void WndList::EnsureVisible( const Playlist::Item& item, const bool select )
{
	const int itemCount = ListView_GetItemCount( m_hWnd );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		const long itemID = GetPlaylistItemID( itemIndex );
		if ( itemID == item.ID ) {
			ListView_EnsureVisible( m_hWnd, itemIndex, FALSE /*partialOK*/ );
			if ( select ) {
				int selectedItem = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
				while ( selectedItem >= 0 ) {
					ListView_SetItemState( m_hWnd, selectedItem, 0, LVIS_SELECTED | LVIS_FOCUSED );
					selectedItem = ListView_GetNextItem( m_hWnd, selectedItem, LVNI_SELECTED );
				}
				ListView_SetItemState( m_hWnd, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
			}
			break;
		}
	}
}

Playlist::ItemList WndList::GetSelectedPlaylistItems()
{
	Playlist::ItemList items;
	int itemIndex = ListView_GetNextItem( m_hWnd, -1, LVNI_SELECTED );
	while ( itemIndex >= 0 ) {
		const long itemID = GetPlaylistItemID( itemIndex );
		Playlist::Item item = { itemID, {} };
		if ( m_Playlist->GetItem( item ) ) {
			items.push_back( item );
		}
		itemIndex = ListView_GetNextItem( m_hWnd, itemIndex, LVNI_SELECTED );
	}
	return items;
}

int WndList::GetSelectedCount() const
{
	const int count = ListView_GetSelectedCount( m_hWnd );
	return count;
}

int WndList::GetCount() const
{
	const int count = ListView_GetItemCount( m_hWnd );
	return count;
}

void WndList::SelectAllItems()
{
	const int itemCount = ListView_GetItemCount( m_hWnd );
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		ListView_SetItemState( m_hWnd, itemIndex, LVIS_SELECTED, LVIS_SELECTED );
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndList::OnCommandAddFolder()
{
	WCHAR title[ MAX_PATH ] = {};
	LoadString( m_hInst, IDS_ADDFOLDERTOPLAYLIST_TITLE, title, MAX_PATH );

	WCHAR buffer[ MAX_PATH ] = {};
	BROWSEINFO bi = {};
	bi.hwndOwner = m_hWnd;
	bi.lpszTitle = title;
	bi.pszDisplayName = buffer;
	bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	LPITEMIDLIST idlist = SHBrowseForFolder( &bi );
	if ( nullptr != idlist ) {
		const DWORD pathSize = 1024;
		WCHAR path[ pathSize ] = {};
		if ( TRUE == SHGetPathFromIDListEx( idlist, path, pathSize, GPFIDL_DEFAULT ) ) {
			if ( !m_Playlist ) {
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					m_Playlist = vuplayer->NewPlaylist();
				}
			}
			AddFolderToPlaylist( path );
		}
		CoTaskMemFree( idlist );
	}
}

void WndList::OnCommandAddFiles()
{
	WCHAR title[ MAX_PATH ] = {};
	LoadString( m_hInst, IDS_ADDFILES_TITLE, title, MAX_PATH );

	WCHAR filter[ MAX_PATH ] = {};
	LoadString( m_hInst, IDS_ADDFILES_FILTERAUDIO, filter, MAX_PATH );
	const std::wstring filter1( filter );
	const std::set<std::wstring> audioTypes = m_Output.GetAllSupportedFileExtensions();
	std::wstring filter2;
	for ( const auto& iter : audioTypes ) {
		filter2 += L"*." + iter + L";";
	}
	if ( !filter2.empty() ) {
		filter2.pop_back();
	}
	LoadString( m_hInst, IDS_ADDFILES_FILTERALL, filter, MAX_PATH );
	const std::wstring filter3( filter );
	const std::wstring filter4( L"*.*" );
	std::vector<WCHAR> filterStr;
	filterStr.reserve( MAX_PATH );
	filterStr.insert( filterStr.end(), filter1.begin(), filter1.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter2.begin(), filter2.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter3.begin(), filter3.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter4.begin(), filter4.end() );
	filterStr.push_back( 0 );
	filterStr.push_back( 0 );

	const DWORD bufferSize = 32768;
	WCHAR buffer[ bufferSize ] = {};
	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrTitle = title;
	ofn.lpstrFilter = &filterStr[ 0 ];
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = bufferSize;
	if ( ( FALSE != GetOpenFileName( &ofn ) ) && ( ofn.nFileOffset > 0 ) ) {
		std::wstring foldername( ofn.lpstrFile, ofn.nFileOffset - 1 );
		foldername += L"\\";
		WCHAR* filenameOffset = ofn.lpstrFile + ofn.nFileOffset;
		while ( 0 != *filenameOffset ) {
			if ( !m_Playlist ) {
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					m_Playlist = vuplayer->NewPlaylist();
				}
			}
			const std::wstring filename = foldername + filenameOffset;
			AddFileToPlaylist( filename );
			filenameOffset += wcslen( filenameOffset ) + 1;
		}
	}
}

void WndList::OnCutCopy( const bool cut )
{
	const Playlist::ItemList items = GetSelectedPlaylistItems();
	if ( !items.empty() ) {
		if ( FALSE != OpenClipboard( m_hWnd ) ) {
			std::wstringstream stream;
			for ( const auto& item : items ) {
				stream << item.Info.GetFilename() << L"\r\n";
			}
			const std::wstring clipboardText = stream.str();
			const HGLOBAL mem = GlobalAlloc( GMEM_MOVEABLE, ( clipboardText.size() + 1 ) * sizeof( WCHAR ) );
			if ( nullptr != mem ) {
				LPWSTR destString = static_cast<LPWSTR>( GlobalLock( mem ) );
				if ( nullptr != destString ) {
					wcscpy_s( destString, clipboardText.size() + 1, clipboardText.c_str() );			
					GlobalUnlock( mem );
					EmptyClipboard();
					SetClipboardData( CF_UNICODETEXT, mem );
				} else {
					GlobalFree( mem );
				}
			}
			CloseClipboard();
		}
		if ( cut ) {
			DeleteSelectedItems();
		}
	}
}

void WndList::OnPaste()
{
	if ( FALSE != OpenClipboard( m_hWnd ) ) {
		std::wstring clipboardText;
		HANDLE handle = GetClipboardData( CF_UNICODETEXT );
		if ( nullptr != handle ) {
			LPCWSTR str = static_cast<LPCWSTR>( GlobalLock( handle ) );
			if ( nullptr != str ) {
				clipboardText = str;
				GlobalUnlock( handle );
			}	
		} else {
			handle = GetClipboardData( CF_TEXT );
			if ( nullptr != handle ) {
				LPCSTR str = static_cast<LPCSTR>( GlobalLock( handle ) );
				if ( nullptr != str ) {
					clipboardText = AnsiCodePageToWideString( str );
					GlobalUnlock( handle );
				}	
			}
		}
		CloseClipboard();

		try {
			std::wstringstream stream( clipboardText );
			do {
				std::wstring filename;
				std::getline( stream, filename );
				if ( !filename.empty() ) {
					if ( filename.back() == '\r' ) {
						filename.pop_back();
					}
					AddFileToPlaylist( filename );				
				}
			} while ( !stream.eof() );
		} catch ( ... ) {
		}
	}
}

void WndList::OnShowColumn( const UINT command )
{
	for ( const auto& iter : s_ColumnFormats ) {
		const ColumnFormat& columnFormat = iter.second;
		if ( columnFormat.ShowID == command ) {
			const Playlist::Column columnID = iter.first;
			ShowColumn( columnID, columnFormat.Width, !IsColumnShown( columnID ) );
			break;
		}
	}
}

void WndList::OnSortPlaylist( const UINT command )
{
	for ( const auto& iter : s_ColumnFormats ) {
		const ColumnFormat& columnFormat = iter.second;
		if ( columnFormat.SortID == command ) {
			const Playlist::Column columnID = iter.first;
			SortPlaylist( columnID );
			break;
		}
	}
}

void WndList::GetColumnVisibility( std::set<UINT>& shown, std::set<UINT>& hidden )
{
	shown.clear();
	hidden.clear();
	for ( const auto& columnIter : s_ColumnFormats ) {
		hidden.insert( columnIter.second.ShowID );
	}
	const HWND headerWnd = ListView_GetHeader( m_hWnd );
	if ( NULL != headerWnd ) {
		const int columnCount = Header_GetItemCount( headerWnd );
		for ( int columnIndex = 1; columnIndex < columnCount; columnIndex++ ) {
			LVCOLUMN lvc = {};
			lvc.mask = LVCF_SUBITEM;
			if ( FALSE != ListView_GetColumn( m_hWnd, columnIndex, &lvc ) ) {
				const Playlist::Column columnID = static_cast<Playlist::Column>( lvc.iSubItem );
				const auto columnFormatIter = s_ColumnFormats.find( columnID );
				if ( columnFormatIter != s_ColumnFormats.end() ) {
					const UINT cmdShowID = columnFormatIter->second.ShowID;
					hidden.erase( cmdShowID );
					shown.insert( cmdShowID );
				}		
			}
		}
	}
}
