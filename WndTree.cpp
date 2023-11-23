#include "WndTree.h"

#include <windowsx.h>

#include <fstream>

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

// Tree control ID.
static const UINT_PTR s_WndTreeID = 1900;

// Icon size.
static const int s_IconSize = 16;

// User folders for the computer node.
static const std::list<KNOWNFOLDERID> s_UserFolders = { FOLDERID_Desktop, FOLDERID_Documents, FOLDERID_Downloads, FOLDERID_Music };

// Playlist ID for the scratch list.
static const std::string s_ScratchListID = "F641E764-3385-428A-9F39-88E928234E17";

// Unique ID for the streams list.
static const std::string s_StreamsListID = "F9E5A333-6043-43AF-B972-C633FC10650F";

// Initial folder setting for choosing playlists.
static const std::string s_PlaylistFolderSetting = "Playlist";

// Message ID for adding a folder to the computer node.
// 'wParam' : HTREEITEM - the parent item under which to add the folder.
// 'lParam' : std::wstring* - folder name, to be deleted by the message handler.
static const UINT MSG_FOLDERADD = WM_APP + 110;

// Message ID for deleting a folder from the computer node.
// 'wParam' : HTREEITEM - the item to delete.
// 'lParam' : unused.
static const UINT MSG_FOLDERDELETE = WM_APP + 111;

// Message ID for renaming a folder in the computer node.
// 'wParam' : std::wstring* - old folder path, to be deleted by the message handler.
// 'lParam' : std::wstring* - new folder path, to be deleted by the message handler.
static const UINT MSG_FOLDERRENAME = WM_APP + 112;

// Command ID of the first playlist entry on the Add to Playlist context sub menu.
static const UINT MSG_TREEMENU_ADDTOPLAYLIST_START = WM_APP + 0xE00;

// Command ID of the last playlist entry on the Add to Playlist context sub menu.
static const UINT MSG_TREEMENU_ADDTOPLAYLIST_END = MSG_TREEMENU_ADDTOPLAYLIST_START + 50;

// Root item ordering.
WndTree::OrderMap WndTree::s_RootOrder = {
	{ Playlist::Type::Folder,			1 },
	{ Playlist::Type::CDDA,				2 },
	{ Playlist::Type::User,				3 },
	{ Playlist::Type::Favourites,	4 },
	{ Playlist::Type::Streams,		5 },
	{ Playlist::Type::All,				6 },
	{ Playlist::Type::Artist,			7 },
	{ Playlist::Type::Album,			8 },
	{ Playlist::Type::Genre,			9 },
	{ Playlist::Type::Year,				10 }
};

LRESULT CALLBACK WndTree::TreeProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTree* wndTree = reinterpret_cast<WndTree*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTree ) {
		switch( message )	{
			case WM_COMMAND : {
				const UINT commandID = LOWORD( wParam );
				wndTree->OnCommand( commandID );
				break;
			}
			case WM_CONTEXTMENU : {
				POINT pt = {};
				if ( -1 == lParam ) {
					HTREEITEM item = TreeView_GetSelection( hwnd );
					if ( nullptr == item ) {
						item = TreeView_GetFirstVisible( hwnd );
					}
					if ( nullptr != item ) {
						RECT itemRect = {};
						TreeView_GetItemRect( hwnd, item, &itemRect, TRUE );
						RECT clientRect = {};
						GetClientRect( hwnd, &clientRect );
						RECT intersectRect = {};
						if ( FALSE == IntersectRect( &intersectRect, &itemRect, &clientRect ) ) {
							item = TreeView_GetFirstVisible( hwnd );
							TreeView_GetItemRect( hwnd, item, &itemRect, TRUE );
						}						
						pt.x = itemRect.left;
						pt.y = itemRect.bottom;
					}
					ClientToScreen( hwnd, &pt );
				} else {
					pt.x = LOWORD( lParam );
					pt.y = HIWORD( lParam );
				}
				wndTree->OnContextMenu( pt );
				break;
			}
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndTree->GetDefaultWndProc() ) );
				SetWindowLongPtr( hwnd, DWLP_USER, 0 );
				wndTree->OnDestroy();
				break;
			}
			case WM_RBUTTONDOWN : {
				TVHITTESTINFO hittest = {};
				hittest.pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				TreeView_HitTest( hwnd, &hittest );
				if ( ( NULL != hittest.hItem ) && ( TreeView_GetSelection( hwnd ) != hittest.hItem ) ) {
					TreeView_SelectItem( hwnd, hittest.hItem );
				}
				break;
			}
			case MSG_FOLDERADD : {
				const std::wstring* folder = reinterpret_cast<std::wstring*>( lParam );
				if ( nullptr != folder ) {
					wndTree->OnFolderAdd( reinterpret_cast<HTREEITEM>( wParam ), *folder );
					delete folder;
				}
				break;
			}
			case MSG_FOLDERDELETE : {
				wndTree->OnFolderDelete( reinterpret_cast<HTREEITEM>( wParam ) );
				break;
			}
			case MSG_FOLDERRENAME : {
				const std::wstring* oldFolderPath = reinterpret_cast<std::wstring*>( wParam );
				const std::wstring* newFolderPath = reinterpret_cast<std::wstring*>( lParam );
				if ( ( nullptr != oldFolderPath ) && ( nullptr != newFolderPath ) ) {
					wndTree->OnFolderRename( *oldFolderPath, *newFolderPath );
				}
				delete oldFolderPath;
				delete newFolderPath;
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndTree->GetDefaultWndProc(), hwnd, message, wParam, lParam );
}

LRESULT CALLBACK WndTree::EditControlProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndTree* wndTree = reinterpret_cast<WndTree*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndTree ) {
		switch( message ) {
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndTree->GetEditControlWndProc() ) );
				break;
			}
			case WM_GETDLGCODE : {
				return DLGC_WANTALLKEYS;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( wndTree->GetEditControlWndProc(), hwnd, message, wParam, lParam );
}

DWORD WINAPI WndTree::ScratchListUpdateProc( LPVOID lpParam )
{
	ScratchListUpdateInfo* info = static_cast<ScratchListUpdateInfo*>( lpParam );
	if ( nullptr != info ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		for ( auto& mediaInfo : info->MediaList ) {
			if ( WAIT_OBJECT_0 == WaitForSingleObject( info->StopEvent, 0 ) ) {
				break;
			} else {
				info->MediaLibrary.GetMediaInfo( mediaInfo );
			}
		}
		CoUninitialize();
	}
	delete info;
	return 0;
}

DWORD WINAPI WndTree::FileModifiedProc( LPVOID lpParam )
{
	WndTree* wndTree = static_cast<WndTree*>( lpParam );
	if ( nullptr != wndTree ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		wndTree->OnFileModifiedHandler();
		CoUninitialize();
	}
	return 0;
}

WndTree::WndTree( HINSTANCE instance, HWND parent, Library& library, Settings& settings, DiscManager& discManager, Output& output ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_DefaultWndProc( nullptr ),
	m_NodePlaylists( nullptr ),
	m_NodeArtists( nullptr ),
	m_NodeAlbums( nullptr ),
	m_NodeGenres( nullptr ),
	m_NodeYears( nullptr ),
	m_NodeAll( nullptr ),
	m_NodeFavourites( nullptr ),
	m_NodeStreams( nullptr ),
	m_NodeComputer( nullptr ),
	m_NodeCurrentOutput( nullptr ),
	m_Library( library ),
	m_Settings( settings ),
	m_DiscManager( discManager ),
	m_Output( output ),
	m_PlaylistMap(),
	m_ArtistMap(),
	m_AlbumMap(),
	m_GenreMap(),
	m_YearMap(),
	m_CDDAMap(),
	m_FolderPlaylistMap(),
	m_PlaylistAll( nullptr ),
	m_PlaylistFavourites( nullptr ),
	m_PlaylistStreams( nullptr ),
	m_ChosenFont( NULL ),
	m_ColourFont( GetSysColor( COLOR_WINDOWTEXT ) ),
	m_ColourBackground( GetSysColor( COLOR_WINDOW ) ),
	m_ColourHighlight( GetSysColor( COLOR_HIGHLIGHT ) ),
	m_ColourIcon( DEFAULT_ICONCOLOUR ),
	m_ImageList( nullptr ),
	m_IconMap(),
	m_IconIndexComputer( 0 ),
	m_IconIndexFolder( 0 ),
	m_IconIndexDrive( 0 ),
	m_IconIndexPlaying( 0 ),
	m_IconIndexPaused( 0 ),
	m_RootComputerFolders(),
	m_FolderNodesMap(),
	m_FolderNodesMapMutex(),
	m_FolderPlaylistMapMutex(),
	m_FolderMonitor( parent ),
	m_FileModifiedThread( nullptr ),
	m_FileModifiedStopEvent( CreateEvent( nullptr /*securityAttributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_FileModifiedWakeEvent( CreateEvent( nullptr /*securityAttributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_FilesModified(),
	m_FilesModifiedMutex(),
	m_ScratchListUpdateThread( nullptr ),
	m_ScratchListUpdateStopEvent( CreateEvent( nullptr /*securityAttributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_MergeDuplicates( settings.GetMergeDuplicates() ),
	m_EditControlWndProc( nullptr ),
	m_IsHighContrast( IsHighContrastActive() ),
	m_AddToPlaylistMenuMap(),
	m_AddedFolderTracks()
{
	const DWORD exStyle = 0;
	LPCTSTR className = WC_TREEVIEW;
	LPCTSTR windowName = NULL;
	const DWORD style = WS_CHILD | WS_TABSTOP | WS_VISIBLE | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS | TVS_EDITLABELS;
	const int x = 0;
	const int y = 0;
	const int width = 0;
	const int height = 0;
	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( exStyle, className, windowName, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndTreeID ), instance, param );
	TreeView_SetExtendedStyle( m_hWnd, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( TreeProc ) ) );
	SetWindowAccessibleName( instance, m_hWnd, IDS_ACCNAME_TREEVIEW );
	ApplySettings();
}

WndTree::~WndTree()
{
	if ( nullptr != m_ChosenFont ) {
		DeleteObject( m_ChosenFont );
	}

	ImageList_Destroy( m_ImageList );

	if ( nullptr != m_ScratchListUpdateStopEvent ) {
		CloseHandle( m_ScratchListUpdateStopEvent );
	}
	if ( nullptr != m_FileModifiedStopEvent ) {
		CloseHandle( m_FileModifiedStopEvent );
	}
	if ( nullptr != m_FileModifiedWakeEvent ) {
		CloseHandle( m_FileModifiedWakeEvent );
	}
}

WNDPROC WndTree::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

HWND WndTree::GetWindowHandle()
{
	return m_hWnd;
}

void WndTree::Initialise()
{
	Populate();
	const HTREEITEM selectedItem = GetStartupItem();
	if ( nullptr != selectedItem ) {
		TreeView_SelectItem( m_hWnd, selectedItem );
	} else if ( nullptr != m_NodeComputer ) {
		TreeView_SelectItem( m_hWnd, m_NodeComputer );
		TreeView_Expand( m_hWnd, m_NodeComputer, TVE_EXPAND );
	}
	StartFileModifiedThread();
}

HTREEITEM WndTree::GetStartupItem()
{
	HTREEITEM selectedItem = nullptr;
	const std::wstring startupPlaylist = m_Settings.GetStartupPlaylist();
	if ( m_PlaylistStreams && ( UTF8ToWideString( m_PlaylistStreams->GetID() ) == startupPlaylist ) ) {
		SelectPlaylist( m_PlaylistStreams );
		selectedItem = m_NodeStreams;
	} else {
		for ( const auto& playlistIter : m_PlaylistMap ) {
			const Playlist::Ptr playlist = playlistIter.second;
			if ( playlist && ( UTF8ToWideString( playlist->GetID() ) == startupPlaylist ) ) {
				selectedItem = playlistIter.first;
				break;
			}
		}

		if ( nullptr == selectedItem ) {
			if ( const UINT driveType = GetDriveType( startupPlaylist.c_str() ); ( DRIVE_CDROM == driveType ) ) {
				const auto& [ startupFilename, cueStart, cueEnd ] = m_Settings.GetStartupFile();
				for ( const auto& cddaDrive : m_CDDAMap ) {
					if ( cddaDrive.second ) {
						const auto playlistItems = cddaDrive.second->GetItems();
						const auto foundItem = std::find_if( playlistItems.begin(), playlistItems.end(), [ startupFilename ] ( const Playlist::Item& item )
						{
							return startupFilename == item.Info.GetFilename();
						} );
						if ( playlistItems.end() != foundItem ) {
							selectedItem = cddaDrive.first;
							TreeView_SelectItem( m_hWnd, selectedItem );							
							break;
						}
					}
				}
			} else {
				selectedItem = GetComputerFolderNode( startupPlaylist );
				if ( nullptr == selectedItem ) {
					const std::list<std::wstring> parts = WideStringSplit( startupPlaylist, '\t' );
					auto currentPart = parts.begin();
					HTREEITEM currentItem = TreeView_GetRoot( m_hWnd );
					while ( ( nullptr != currentItem ) && ( parts.end() != currentPart ) ) {
						const std::wstring itemLabel = GetItemLabel( currentItem );
						if ( itemLabel == *currentPart ) {
							++currentPart;
							if ( parts.end() == currentPart ) {
								selectedItem = currentItem;
								break;
							} else {
								currentItem = TreeView_GetChild( m_hWnd, currentItem );
							}
						} else {
							currentItem = TreeView_GetNextSibling( m_hWnd, currentItem );
						}
					}
				}
			}
		}
	}
	return selectedItem;
}

void WndTree::SaveStartupPlaylist( const Playlist::Ptr playlist )
{
	std::wstring startupPlaylist;
	if ( playlist ) {
		const Playlist::Type type = playlist->GetType();
		switch ( type ) {
			case Playlist::Type::User : {
				startupPlaylist = UTF8ToWideString( playlist->GetID() );
				break;
			}
			case Playlist::Type::All :
			case Playlist::Type::Favourites : {
				startupPlaylist = playlist->GetName();
				break;
			}
			case Playlist::Type::Streams : {
				startupPlaylist = UTF8ToWideString( s_StreamsListID );
				break;
			}
			case Playlist::Type::Artist : {
				for ( const auto& artist : m_ArtistMap ) {
					const Playlist::Ptr artistPlaylist = artist.second;
					if ( artistPlaylist && ( artistPlaylist->GetID() == playlist->GetID() ) ) {
						const std::list<std::wstring> parts( { GetItemLabel( m_NodeArtists ), playlist->GetName() } );
						startupPlaylist = WideStringJoin( parts, '\t' );
						break;
					}
				}
				break;
			}
			case Playlist::Type::Album : {
				for ( const auto& album : m_AlbumMap ) {
					const Playlist::Ptr albumPlaylist = album.second;
					if ( albumPlaylist && ( albumPlaylist->GetID() == playlist->GetID() ) ) {
						std::list<std::wstring> parts;
						const HTREEITEM parentItem = TreeView_GetParent( m_hWnd, album.first );
						if ( parentItem == m_NodeAlbums ) {
							parts = { GetItemLabel( m_NodeAlbums ), playlist->GetName() };
						} else {
							parts = { GetItemLabel( m_NodeArtists ), GetItemLabel( parentItem ), playlist->GetName() };
						}
						startupPlaylist = WideStringJoin( parts, '\t' );
						break;
					}
				}
				break;
			}
			case Playlist::Type::Genre : {
				for ( const auto& genre : m_GenreMap ) {
					const Playlist::Ptr genrePlaylist = genre.second;
					if ( genrePlaylist && ( genrePlaylist->GetID() == playlist->GetID() ) ) {
						const std::list<std::wstring> parts( { GetItemLabel( m_NodeGenres ), playlist->GetName() } );
						startupPlaylist = WideStringJoin( parts, '\t' );
						break;
					}
				}
				break;
			}
			case Playlist::Type::Year : {
				for ( const auto& year : m_YearMap ) {
					const Playlist::Ptr yearPlaylist = year.second;
					if ( yearPlaylist && ( yearPlaylist->GetID() == playlist->GetID() ) ) {
						const std::list<std::wstring> parts( { GetItemLabel( m_NodeYears ), playlist->GetName() } );
						startupPlaylist = WideStringJoin( parts, '\t' );
						break;
					}
				}
				break;
			}
			case Playlist::Type::Folder : {
				startupPlaylist = playlist->GetName();
				break;
			}
			case Playlist::Type::CDDA : {
				if ( const auto items = playlist->GetItems(); !items.empty() ) {
					std::filesystem::path path( items.front().Info.GetFilename() );
					startupPlaylist = path.root_path();
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	m_Settings.SetStartupPlaylist( startupPlaylist );
}

void WndTree::OnCommand( const UINT command )
{
	switch ( command ) {
		case ID_FILE_NEWPLAYLIST : {
			NewPlaylist( true /*editTitle*/ );
			break;
		}
		case ID_FILE_DELETEPLAYLIST : {
			DeleteSelectedPlaylist();
			break;
		}
		case ID_FILE_RENAMEPLAYLIST : {
			RenameSelectedPlaylist();
			break;
		}
		case ID_FILE_IMPORTPLAYLIST : {
			ImportPlaylist();
			break;
		}
		case ID_FILE_EXPORTPLAYLIST : {
			ExportSelectedPlaylist();
			break;
		}
		case ID_TREEMENU_FONTSTYLE : {
			OnSelectFont();
			break;
		}
		case ID_TREEMENU_FONTCOLOUR :
		case ID_TREEMENU_BACKGROUNDCOLOUR :
		case ID_TREEMENU_HIGHLIGHTCOLOUR :
		case ID_TREEMENU_ICONCOLOUR : {
			OnSelectColour( command );
			break;
		}
		case ID_TREEMENU_FAVOURITES : {
			OnFavourites();
			break;
		}
		case ID_TREEMENU_STREAMS : {
			OnStreams();
			break;
		}
		case ID_TREEMENU_ALLTRACKS : {
			OnAllTracks();
			break;
		}
		case ID_TREEMENU_ARTISTS : {
			OnArtists();
			break;
		}
		case ID_TREEMENU_ALBUMS : {
			OnAlbums();
			break;
		}
		case ID_TREEMENU_GENRES : {
			OnGenres();
			break;
		}
		case ID_TREEMENU_YEARS : {
			OnYears();
			break;
		}
		default : {
			if ( ( command >= MSG_TREEMENU_ADDTOPLAYLIST_START ) && ( command <= MSG_TREEMENU_ADDTOPLAYLIST_END ) ) {
				OnAddToPlaylist( command );
			} else {
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					vuplayer->OnCommand( command );
				}
			}
			break;
		}
	}
}

void WndTree::OnContextMenu( const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_TREE ) );
	if ( NULL != menu ) {
		HMENU treemenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != treemenu ) {
			EnableMenuItem( treemenu, ID_FILE_DELETEPLAYLIST, MF_BYCOMMAND | ( IsPlaylistDeleteEnabled() ? MF_ENABLED : MF_DISABLED ) );
			EnableMenuItem( treemenu, ID_FILE_EXPORTPLAYLIST, MF_BYCOMMAND | ( IsPlaylistExportEnabled() ? MF_ENABLED : MF_DISABLED ) );
			EnableMenuItem( treemenu, ID_FILE_RENAMEPLAYLIST, MF_BYCOMMAND | ( IsPlaylistRenameEnabled() ? MF_ENABLED : MF_DISABLED ) );

			CheckMenuItem( treemenu, ID_TREEMENU_FAVOURITES, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_FAVOURITES ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_STREAMS, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_STREAMS ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_ALLTRACKS, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_ALLTRACKS ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_ARTISTS, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_ARTISTS ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_ALBUMS, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_ALBUMS ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_GENRES, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_GENRES ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( treemenu, ID_TREEMENU_YEARS, MF_BYCOMMAND | ( IsShown( ID_TREEMENU_YEARS ) ? MF_CHECKED : MF_UNCHECKED ) );

			const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
			const Playlist::Ptr playlist = GetPlaylist( hSelectedItem );

			if ( playlist && ( Playlist::Type::CDDA == playlist->GetType() ) ) {		
				const int bufferSize = 64;
				WCHAR buffer[ bufferSize ] = {};
				LoadString( m_hInst, IDS_EXTRACT_TRACKS, buffer, bufferSize );
				ModifyMenu( menu, ID_FILE_CONVERT, MF_BYCOMMAND | MF_STRING, ID_FILE_CONVERT, buffer );
			}

			const UINT enableExtract = ( playlist && playlist->CanConvertAnyItems() ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( treemenu, ID_FILE_CONVERT, MF_BYCOMMAND | enableExtract );

			VUPlayer* vuplayer = VUPlayer::Get();
			const UINT musicbrainzEnabled = ( playlist && playlist->AllowMusicBrainzQueries() && ( nullptr != vuplayer ) && vuplayer->IsMusicBrainzEnabled() ) ? MF_ENABLED : MF_DISABLED;
			EnableMenuItem( treemenu, ID_FILE_MUSICBRAINZ_QUERY, MF_BYCOMMAND | musicbrainzEnabled );

			const UINT enableColourChoice = IsHighContrastActive() ? MF_DISABLED : MF_ENABLED;
			EnableMenuItem( menu, ID_TREEMENU_FONTCOLOUR, MF_BYCOMMAND | enableColourChoice );
			EnableMenuItem( menu, ID_TREEMENU_BACKGROUNDCOLOUR, MF_BYCOMMAND | enableColourChoice );
			EnableMenuItem( menu, ID_TREEMENU_HIGHLIGHTCOLOUR, MF_BYCOMMAND | enableColourChoice );
			EnableMenuItem( menu, ID_TREEMENU_ICONCOLOUR, MF_BYCOMMAND | enableColourChoice );

			InsertAddToPlaylists( GetSubMenu( menu, 0 ), ID_FILE_EXPORTPLAYLIST, true /*insertSeparator*/ );

			const UINT flags = TPM_RIGHTBUTTON;
			TrackPopupMenu( treemenu, flags, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}

LPARAM WndTree::OnBeginLabelEdit( WPARAM /*wParam*/, LPARAM lParam )
{
	LPARAM preventEditing = TRUE;
	LPNMTVDISPINFO tvInfo = reinterpret_cast<LPNMTVDISPINFO>( lParam );
	if ( nullptr != tvInfo ) {
		const Playlist::Type type = GetItemType( tvInfo->item.hItem );
		if ( Playlist::Type::User == type ) {
			if ( const HWND editControl = TreeView_GetEditControl( m_hWnd ); nullptr != editControl ) {
				SetWindowLongPtr( editControl, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
				m_EditControlWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( editControl, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( EditControlProc ) ) );
				preventEditing = FALSE;
			}
		}
	}
	return preventEditing;
}

BOOL WndTree::OnEndLabelEdit( WPARAM /*wParam*/, LPARAM lParam )
{
	BOOL result = FALSE;
	if ( nullptr != TreeView_GetEditControl( m_hWnd ) ) {
		LPNMTVDISPINFO tvInfo = reinterpret_cast<LPNMTVDISPINFO>( lParam );
		if ( ( nullptr != tvInfo ) && ( nullptr != tvInfo->item.pszText ) && ( wcslen( tvInfo->item.pszText ) > 0 ) ) {
			Playlist::Ptr playlist = GetPlaylist( tvInfo->item.hItem );
			if ( playlist ) {
				TVITEMEX tvItem = {};
				tvItem.mask = TVIF_HANDLE | TVIF_TEXT;
				tvItem.hItem = tvInfo->item.hItem;
				tvItem.pszText = tvInfo->item.pszText;
				TreeView_SetItem( m_hWnd, &tvItem );
				playlist->SetName( tvInfo->item.pszText );
				TreeView_SortChildren( m_hWnd, m_NodePlaylists, FALSE /*recurse*/ );
				TreeView_SelectItem( m_hWnd, tvInfo->item.hItem );
			}
			result = TRUE;
		}
	}
	return result;
}

void WndTree::LoadPlaylists()
{
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_PLAYLISTS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::User );
	tvItem.iSelectedImage = tvItem.iImage;
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = m_NodeComputer;
	tvInsert.itemex = tvItem;
	m_NodePlaylists = TreeView_InsertItem( m_hWnd, &tvInsert );

	Playlists playlists = m_Settings.GetPlaylists();
	for ( const auto& iter : playlists ) {
		const Playlist::Ptr playlist = iter;
		if ( playlist ) {
			const HTREEITEM hItem = AddItem( m_NodePlaylists, playlist->GetName(), Playlist::Type::User );
			m_PlaylistMap.insert( PlaylistMap::value_type( hItem, playlist ) );
		}
	}
}

Playlist::Ptr WndTree::NewPlaylist( const bool editTitle )
{
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_NEWPLAYLIST, buffer, bufSize );

	std::wstring uniqueName = buffer;
	const std::set<std::wstring> allNames = GetPlaylistNames();
	auto nameIter = allNames.find( uniqueName );
	int playlistNum = 1;
	while ( allNames.end() != nameIter ) {
		uniqueName = buffer;
		uniqueName += L" (" + std::to_wstring( ++playlistNum ) + L")";
		nameIter = allNames.find( uniqueName );
	}
	wcscpy_s( buffer, bufSize, uniqueName.c_str() );

	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::User );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::User, tvItem.iImage );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = m_NodePlaylists;
	tvInsert.hInsertAfter = TVI_SORT;
	tvInsert.itemex = tvItem;
	TreeView_SelectItem( m_hWnd , NULL /*hItem*/ );
	HTREEITEM hItem = TreeView_InsertItem( m_hWnd, &tvInsert );

	Playlist::Ptr playlist( new Playlist( m_Library, Playlist::Type::User ) );
	m_PlaylistMap.insert( PlaylistMap::value_type( hItem, playlist ) );

	SetFocus( m_hWnd );
	TreeView_SelectItem( m_hWnd, hItem );
	if ( editTitle ) {
		TreeView_EditLabel( m_hWnd, hItem );
	}
	return playlist;
}

void WndTree::DeleteSelectedPlaylist()
{
	HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	const auto playlistIter = m_PlaylistMap.find( hSelectedItem );
	if ( m_PlaylistMap.end() != playlistIter ) {
		const Playlist::Ptr playlist = playlistIter->second;
		if ( playlist ) {
			m_Settings.RemovePlaylist( *playlist );
			m_PlaylistMap.erase( playlistIter );
			TreeView_DeleteItem( m_hWnd, hSelectedItem );
		}
	}
}

std::set<std::wstring> WndTree::GetPlaylistNames() const
{
	std::set<std::wstring> names;
	if ( nullptr != m_NodePlaylists ) {
		HTREEITEM playlistItem = TreeView_GetChild( m_hWnd, m_NodePlaylists );
		while ( nullptr != playlistItem ) {
			names.insert( GetItemLabel( playlistItem ) );
			playlistItem = TreeView_GetNextSibling( m_hWnd, playlistItem );
		}
	}
	return names;
}

Playlist::Ptr WndTree::GetPlaylist( const HTREEITEM node )
{
	Playlist::Ptr playlist;
	const Playlist::Type type = GetItemType( node );
	switch ( type ) {
		case Playlist::Type::User : {
			const auto iter = m_PlaylistMap.find( node );
			if ( m_PlaylistMap.end() != iter ) {
				playlist = iter->second;
			}
			break;
		}
		case Playlist::Type::All : {
			playlist = m_PlaylistAll;
			break;
		}
		case Playlist::Type::Favourites : {
			playlist = m_PlaylistFavourites;
			break;
		}
		case Playlist::Type::Streams : {
			playlist = m_PlaylistStreams;
			break;
		}
		case Playlist::Type::Artist : {
			const auto artistItem = m_ArtistMap.find( node );
			if ( m_ArtistMap.end() != artistItem ) {
				playlist = artistItem->second;
			} else {
				playlist = std::make_shared<Playlist::Ptr::element_type>( m_Library, type, m_MergeDuplicates );
				m_ArtistMap.insert( PlaylistMap::value_type( node, playlist ) );

				const std::wstring artist = GetItemLabel( node );
				const MediaInfo::List mediaList = m_Library.GetMediaByArtist( artist );
				for ( const auto& mediaInfo : mediaList ) {
					playlist->AddItem( mediaInfo );
				}
			}
			break;
		}
		case Playlist::Type::Album : {
			const auto albumItem = m_AlbumMap.find( node );
			if ( m_AlbumMap.end() != albumItem ) {
				playlist = albumItem->second;
			} else {
				playlist = std::make_shared<Playlist::Ptr::element_type>( m_Library, type, m_MergeDuplicates );
				m_AlbumMap.insert( PlaylistMap::value_type( node, playlist ) );

				MediaInfo::List mediaList;
				const HTREEITEM parentNode = TreeView_GetParent( m_hWnd, node );
				const Playlist::Type parentType = GetItemType( parentNode );
				if ( Playlist::Type::Artist == parentType ) {
					const std::wstring artist = GetItemLabel( parentNode );
					const std::wstring album = GetItemLabel( node );
					mediaList = m_Library.GetMediaByArtistAndAlbum( artist, album );
				} else {
					const std::wstring album = GetItemLabel( node );
					mediaList = m_Library.GetMediaByAlbum( album );
				}

				for ( const auto& mediaInfo : mediaList ) {
					playlist->AddItem( mediaInfo );
				}
			}
			break;
		}
		case Playlist::Type::Genre : {
			const auto genreItem = m_GenreMap.find( node );
			if ( m_GenreMap.end() != genreItem ) {
				playlist = genreItem->second;
			} else {
				playlist = std::make_shared<Playlist::Ptr::element_type>( m_Library, type, m_MergeDuplicates );
				m_GenreMap.insert( PlaylistMap::value_type( node, playlist ) );

				const std::wstring genre = GetItemLabel( node );
				const MediaInfo::List mediaList = m_Library.GetMediaByGenre( genre );
				for ( const auto& mediaInfo : mediaList ) {
					playlist->AddItem( mediaInfo );
				}
			}
			break;
		}
		case Playlist::Type::Year : {
			const auto yearItem = m_YearMap.find( node );
			if ( m_YearMap.end() != yearItem ) {
				playlist = yearItem->second;
			} else {
				playlist = std::make_shared<Playlist::Ptr::element_type>( m_Library, type, m_MergeDuplicates );
				m_YearMap.insert( PlaylistMap::value_type( node, playlist ) );

				try {
					const long year = std::stol( GetItemLabel( node ) );
					const MediaInfo::List mediaList = m_Library.GetMediaByYear( year );
					for ( const auto& mediaInfo : mediaList ) {
						playlist->AddItem( mediaInfo );
					}
				} catch ( const std::logic_error& ) {
				}
			}
			break;
		}
		case Playlist::Type::CDDA : {
			const auto iter = m_CDDAMap.find( node );
			if ( m_CDDAMap.end() != iter ) {
				playlist = iter->second;
			}
			break;
		}
		case Playlist::Type::Folder : {
			std::lock_guard<std::mutex> lock( m_FolderPlaylistMapMutex );
			const auto folderItem = m_FolderPlaylistMap.find( node );
			if ( m_FolderPlaylistMap.end() != folderItem ) {
				playlist = folderItem->second;
			} else {
				playlist = std::make_shared<Playlist::Ptr::element_type>( m_Library, type );
				std::wstring path;
				GetFolderPath( node, path );
				playlist->SetName( path );
				m_FolderPlaylistMap.insert( PlaylistMap::value_type( node, playlist ) );
			}
			AddFolderTracks( node, playlist );
			break;
		}
		default : {
			break;
		}
	}
	if ( playlist && ( Playlist::Type::Folder != type ) ) {
		playlist->SetName( GetItemLabel( node ) );
	}
	return playlist;
}

Playlist::Ptr WndTree::GetSelectedPlaylist()
{
	Playlist::Ptr playlist;
	const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	if ( NULL != hSelectedItem ) {
		playlist = GetPlaylist( hSelectedItem );
	}
	return playlist;
}

void WndTree::OnDestroy()
{
	m_FolderMonitor.RemoveAllFolders();

	StopScratchListUpdateThread();
	StopFileModifiedThread();

	for ( const auto& iter : m_PlaylistMap ) {
		const Playlist::Ptr playlist = iter.second;
		if ( playlist ) {
			playlist->StopPendingThread();
		}
	}

	for ( const auto& iter : m_PlaylistMap ) {
		const Playlist::Ptr playlist = iter.second;
		if ( playlist ) {
			m_Settings.SavePlaylist( *playlist );
		}
	}

	if ( m_PlaylistFavourites ) {
		m_PlaylistFavourites->StopPendingThread();
		m_Settings.SavePlaylist( *m_PlaylistFavourites );
	}

	if ( m_PlaylistStreams ) {
		m_PlaylistStreams->StopPendingThread();
	}

	if ( m_PlaylistAll ) {
		m_PlaylistAll->StopPendingThread();
	}

	for ( const auto& iter : m_ArtistMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	for ( const auto& iter : m_AlbumMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	for ( const auto& iter : m_GenreMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	for ( const auto& iter : m_YearMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	for ( const auto& iter : m_CDDAMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	std::lock_guard<std::mutex> lock( m_FolderPlaylistMapMutex );
	for ( const auto& iter : m_FolderPlaylistMap ) {
		if ( iter.second ) {
			iter.second->StopPendingThread();
		}
	}

	SaveSettings();
}

Playlists WndTree::GetUserPlaylists()
{
	Playlists playlists;
	HTREEITEM hPlaylistItem = TreeView_GetChild( m_hWnd, m_NodePlaylists );
	while ( nullptr != hPlaylistItem ) {
		const auto iter = m_PlaylistMap.find( hPlaylistItem );
		if ( m_PlaylistMap.end() != iter ) {
			const Playlist::Ptr playlist = iter->second;
			if ( playlist ) {
				playlists.push_back( playlist );
			}
		}
		hPlaylistItem = TreeView_GetNextSibling( m_hWnd, hPlaylistItem );
	}
	return playlists;
}

void WndTree::AddPlaylist( const Playlist::Ptr playlist )
{
	if ( playlist ) {
		TVITEMEX tvItem = {};
		tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvItem.pszText = const_cast<LPWSTR>( playlist->GetName().c_str() );
		tvItem.iImage = GetIconIndex( Playlist::Type::User );
		tvItem.iSelectedImage = tvItem.iImage;
		tvItem.lParam = CreateItemData( Playlist::Type::User, tvItem.iImage );
		TVINSERTSTRUCT tvInsert = {};
		tvInsert.hParent = m_NodePlaylists;
		tvInsert.hInsertAfter = TVI_SORT;
		tvInsert.itemex = tvItem;
		HTREEITEM hItem = TreeView_InsertItem( m_hWnd, &tvInsert );
		if ( nullptr != hItem ) {
			m_PlaylistMap.insert( PlaylistMap::value_type( hItem, playlist ) );
			TreeView_SelectItem( m_hWnd, hItem );
			SetFocus( m_hWnd );
			if ( playlist->GetPendingCount() > 0 ) {
				playlist->StartPendingThread();
			}
		}
	}
}

void WndTree::ImportPlaylist( const std::wstring& filename )
{
	std::wstring playlistFilename( filename );
	if ( playlistFilename.empty() ) {
		WCHAR title[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_IMPORTPLAYLIST_TITLE, title, MAX_PATH );

		WCHAR filter[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_IMPORTPLAYLIST_FILTERPLAYLISTS, filter, MAX_PATH );
		const std::wstring filter1( filter );

		const std::set<std::wstring> playlistFiletypes = Playlist::GetSupportedPlaylistExtensions();
		std::wstring filter2;
		for ( const auto& iter : playlistFiletypes ) {
			filter2 += L"*." + iter + L";";
		}
		if ( !filter2.empty() ) {
			filter2.pop_back();
		}

		LoadString( m_hInst, IDS_CHOOSE_FILTERALL, filter, MAX_PATH );
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

		const std::wstring initialFolder = m_Settings.GetLastFolder( s_PlaylistFolderSetting );

		WCHAR buffer[ MAX_PATH ] = {};
		OPENFILENAME ofn = {};
		ofn.lStructSize = sizeof( OPENFILENAME );
		ofn.hwndOwner = m_hWnd;
		ofn.lpstrTitle = title;
		ofn.lpstrFilter = &filterStr[ 0 ];
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
		if ( FALSE != GetOpenFileName( &ofn ) ) {
			playlistFilename = ofn.lpstrFile;
			m_Settings.SetLastFolder( s_PlaylistFolderSetting, playlistFilename.substr( 0, ofn.nFileOffset ) );
		}
	}

	if ( !playlistFilename.empty() ) {
		Playlist::Ptr playlist( new Playlist( m_Library, Playlist::Type::User ) );
		if ( playlist->AddPlaylist( playlistFilename, false /*startPendingThread*/ ) ) {
			const size_t nameDelimiter = playlistFilename.find_last_of( L"/\\" );
			const size_t extDelimiter = playlistFilename.rfind( '.' );
			if ( ( std::wstring::npos != nameDelimiter ) && ( extDelimiter > nameDelimiter ) ) {
				playlist->SetName( playlistFilename.substr( nameDelimiter + 1, extDelimiter - nameDelimiter - 1 ) );
			}
			AddPlaylist( playlist );
		}
	}
}

void WndTree::ExportSelectedPlaylist()
{
	const Playlist::Ptr playlist = GetSelectedPlaylist();
	if ( playlist ) {
		WCHAR title[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_EXPORTPLAYLIST_TITLE, title, MAX_PATH );

		WCHAR filter[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_EXPORTPLAYLIST_FILTERM3U, filter, MAX_PATH );
		const std::wstring filter1( filter );
		const std::wstring filter2( L"*.m3u" );
		LoadString( m_hInst, IDS_EXPORTPLAYLIST_FILTERPLS, filter, MAX_PATH );
		const std::wstring filter3( filter );
		const std::wstring filter4( L"*.pls" );
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

		std::wstring playlistName = playlist->GetName();
		if ( Playlist::Type::Folder == playlist->GetType() ) {
			const size_t pos = playlistName.find_last_of( L"/\\" );
			if ( std::wstring::npos != pos ) {
				playlistName = playlistName.substr( 1 + pos );
			}
		}

		WCHAR buffer[ MAX_PATH ] = {};
		wcscpy_s( buffer, MAX_PATH, playlistName.c_str() );
		std::set<WCHAR> invalidCharacters = { '\\', '/', ':', '*', '?', '"', '<', '>', '|' };
		const size_t length = wcslen( buffer );
		for ( size_t index = 0; index < length; index++ ) {
			if ( invalidCharacters.end() != invalidCharacters.find( buffer[ index ] ) ) {
				buffer[ index ] = '_';
			}
		}

		const std::wstring initialFolder = m_Settings.GetLastFolder( s_PlaylistFolderSetting );

		OPENFILENAME ofn = {};
		ofn.lStructSize = sizeof( OPENFILENAME );
		ofn.hwndOwner = m_hWnd;
		ofn.lpstrTitle = title;
		ofn.lpstrFilter = &filterStr[ 0 ];
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
		if ( FALSE != GetSaveFileName( &ofn ) ) {
			const std::wstring fileExt = ( 2 == ofn.nFilterIndex ) ? L"pls" : L"m3u";
			std::wstring filename = ofn.lpstrFile;
			m_Settings.SetLastFolder( s_PlaylistFolderSetting, filename.substr( 0, ofn.nFileOffset ) );
			if ( !filename.empty() ) {
				const size_t delimiter = filename.find_last_of( '.' );
				if ( std::wstring::npos == delimiter ) {
					filename += L"." + fileExt;
				} else {
					const std::wstring checkExt = WideStringToLower( filename.substr( delimiter + 1 ) );
					if ( checkExt != fileExt ) {
						filename += L"." + fileExt;
					}
				}
				std::ofstream fileStream;
				fileStream.open( filename, std::ios::out | std::ios::trunc );
				if ( fileStream.is_open() ) {
					const Playlist::Items items = playlist->GetItems();
					if ( L"pls" == fileExt ) {
						fileStream << "[playlist]\n";
						int itemCount = 0;
						auto item = items.begin();
						while ( item != items.end() ) {
							fileStream << "File" << ++itemCount << "=" << WideStringToAnsiCodePage( item->Info.GetFilename() ) << "\n";
							++item;
						}
						fileStream << "NumberOfEntries=" << itemCount;
						fileStream << "\nVersion=2\n";
					} else {
						fileStream << "#EXTM3U\n";
						for ( const auto& item : items ) {
							fileStream << WideStringToAnsiCodePage( item.Info.GetFilename() ) << "\n";
						}
					}
					fileStream.close();
				}
			}
		}
	}
}

void WndTree::ProcessPendingPlaylists()
{
	for ( const auto& iter : m_PlaylistMap ) {
		Playlist::Ptr playlist = iter.second;
		if ( playlist && ( playlist->GetPendingCount() > 0 ) ) {
			playlist->StartPendingThread();
		}
	}
}

void WndTree::SelectPlaylist( const Playlist::Ptr playlist, const bool showNode )
{
	if ( playlist ) {
		if ( Playlist::Type::Streams == playlist->GetType() ) {
			if ( showNode && ( nullptr == m_NodeStreams ) ) {
				OnStreams();
			}
			if ( nullptr != m_NodeStreams ) {
				TreeView_SelectItem( m_hWnd, m_NodeStreams );
			}
		} else if ( Playlist::Type::Favourites == playlist->GetType() ) {
			if ( showNode && ( nullptr == m_NodeFavourites ) ) {
				OnFavourites();
			}
			if ( nullptr != m_NodeFavourites ) {
				TreeView_SelectItem( m_hWnd, m_NodeFavourites );
			}
		} else if ( Playlist::Type::All == playlist->GetType() ) {
			if ( showNode && ( nullptr == m_NodeAll ) ) {
				OnAllTracks();
			}
			if ( nullptr != m_NodeAll ) {
				TreeView_SelectItem( m_hWnd, m_NodeAll );
			}
		} else {
			for ( const auto& iter : m_PlaylistMap ) {
				if ( iter.second.get() == playlist.get() ) {
					TreeView_SelectItem( m_hWnd, iter.first );
					break;
				}
			}
		}
	}
}

void WndTree::SelectAllTracks()
{
	if ( nullptr == m_NodeAll ) {
		AddAllTracks();
	}
	TreeView_SelectItem( m_hWnd, m_NodeAll );
}

void WndTree::OnSelectFont()
{
	LOGFONT logFont = GetFont();

	CHOOSEFONT chooseFont = {};
	chooseFont.lStructSize = sizeof( CHOOSEFONT );
	chooseFont.hwndOwner = m_hWnd;
	chooseFont.Flags = CF_FORCEFONTEXIST | CF_NOVERTFONTS | CF_LIMITSIZE | CF_INITTOLOGFONTSTRUCT;
	chooseFont.nSizeMax = 36;
	chooseFont.lpLogFont = &logFont;

	if ( ( TRUE == ChooseFont( &chooseFont ) ) && ( nullptr != chooseFont.lpLogFont ) ) {
		SetFont( *chooseFont.lpLogFont );
	}
}

LOGFONT WndTree::GetFont()
{
	LOGFONT logFont = {};
	const HFONT hFont = reinterpret_cast<HFONT>( SendMessage( m_hWnd, WM_GETFONT, 0, 0 ) );
	if ( nullptr != hFont ) {
		const int bufSize = GetObject( hFont, 0, NULL );
		if ( bufSize > 0 ) {
			std::vector<char> buf( bufSize, 0 );
			if ( bufSize == GetObject( hFont, bufSize, buf.data() ) ) {
				LOGFONT* currentLogFont = reinterpret_cast<LOGFONT*>( buf.data() );
				if ( nullptr != currentLogFont ) {
					logFont = *currentLogFont;
				}
			}
		}
	}
	return logFont;
}

void WndTree::SetFont( const LOGFONT& logFont )
{
	const HFONT hNewFont = CreateFontIndirect( &logFont );
	SendMessage( m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>( hNewFont ), FALSE /*redraw*/ );
	RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
	if ( nullptr != m_ChosenFont ) {
		DeleteObject( m_ChosenFont );
	}
	m_ChosenFont = hNewFont;
	CreateImageList();
}

void WndTree::ApplySettings()
{
	LOGFONT logFont = GetFont();
	bool favourites = false;
	bool streams = false;
	bool allTracks = false;
	bool artists = false;
	bool albums = false;
	bool genres = false;
	bool years = false;
	m_Settings.GetTreeSettings( logFont, m_ColourFont, m_ColourBackground, m_ColourHighlight, m_ColourIcon, favourites, streams, allTracks, artists, albums, genres, years );
	SetColours();
	SetFont( logFont );
}

void WndTree::SaveSettings()
{
	const LOGFONT logFont = GetFont();
	const COLORREF fontColour = GetFontColour();
	const COLORREF backgroundColour = GetBackgroundColour();
	const COLORREF highlightColour = GetHighlightColour();
	const COLORREF iconColour = GetIconColour();
	const bool favourites = IsShown( ID_TREEMENU_FAVOURITES );
	const bool streams = IsShown( ID_TREEMENU_STREAMS );
	const bool allTracks = IsShown( ID_TREEMENU_ALLTRACKS );
	const bool artists = IsShown( ID_TREEMENU_ARTISTS );
	const bool albums = IsShown( ID_TREEMENU_ALBUMS );
	const bool genres = IsShown( ID_TREEMENU_GENRES );
	const bool years = IsShown( ID_TREEMENU_YEARS );
	m_Settings.SetTreeSettings( logFont, fontColour, backgroundColour, highlightColour, iconColour, favourites, streams, allTracks, artists, albums, genres, years );
}

void WndTree::OnSelectColour( const UINT commandID )
{
	COLORREF initialColour = 0;
	switch ( commandID ) {
		case ID_TREEMENU_FONTCOLOUR : {
			initialColour = GetFontColour();
			break;
		}
		case ID_TREEMENU_BACKGROUNDCOLOUR : {
			initialColour = GetBackgroundColour();
			break;
		}
		case ID_TREEMENU_HIGHLIGHTCOLOUR : {
			initialColour = GetHighlightColour();
			break;
		}
		case ID_TREEMENU_ICONCOLOUR : {
			initialColour = GetIconColour();
			break;
		}
		default : {
			break;
		}
	}

	VUPlayer* vuplayer = VUPlayer::Get();
	COLORREF* customColours = ( nullptr != vuplayer ) ? vuplayer->GetCustomColours() : nullptr;
	if ( const auto colour = ChooseColour( m_hWnd, initialColour, customColours ); colour.has_value() ) {
		switch ( commandID ) {
			case ID_TREEMENU_FONTCOLOUR : {
				m_ColourFont = colour.value();
				SetColours();
				break;
			}
			case ID_TREEMENU_BACKGROUNDCOLOUR : {
				m_ColourBackground = colour.value();
				SetColours();
				break;
			}
			case ID_TREEMENU_HIGHLIGHTCOLOUR : {
				m_ColourHighlight = colour.value();
				break;
			}
			case ID_TREEMENU_ICONCOLOUR : {
				m_ColourIcon = colour.value();
				CreateImageList();
				break;
			}
			default : {
				break;
			}
		}
		SaveSettings();
		RedrawWindow( m_hWnd, NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
	}
}

COLORREF WndTree::GetFontColour() const
{
	return m_ColourFont;
}

COLORREF WndTree::GetBackgroundColour() const
{
	return m_ColourBackground;
}

COLORREF WndTree::GetHighlightColour() const
{
	return m_ColourHighlight;
}

COLORREF WndTree::GetIconColour() const
{
	return m_ColourIcon;
}

void WndTree::AddFavourites()
{
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_FAVOURITES, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Favourites );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Favourites, tvItem.iImage, s_RootOrder[ Playlist::Type::Favourites ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Favourites );
	tvInsert.itemex = tvItem;
	m_NodeFavourites = TreeView_InsertItem( m_hWnd, &tvInsert );
}

void WndTree::AddStreams()
{
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_STREAMS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Streams );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Streams, tvItem.iImage, s_RootOrder[ Playlist::Type::Streams ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Streams );
	tvInsert.itemex = tvItem;
	m_NodeStreams = TreeView_InsertItem( m_hWnd, &tvInsert );
}

void WndTree::AddAllTracks()
{
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_ALLTRACKS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::All );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::All, tvItem.iImage, s_RootOrder[ Playlist::Type::All ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::All );
	tvInsert.itemex = tvItem;
	m_NodeAll = TreeView_InsertItem( m_hWnd, &tvInsert );
}

void WndTree::AddArtists()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_ARTISTS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Artist );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Artist, tvItem.iImage, s_RootOrder[ Playlist::Type::Artist ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Artist );
	tvInsert.itemex = tvItem;
	m_NodeArtists = TreeView_InsertItem( m_hWnd, &tvInsert );
	if ( nullptr != m_NodeArtists ) {
		const std::set<std::wstring> artists = m_Library.GetArtists();
		for ( const auto& artist : artists ) {
			const HTREEITEM artistNode = AddItem( m_NodeArtists, artist, Playlist::Type::Artist, false /*redraw*/ );
			if ( nullptr != artistNode ) {
				const std::set<std::wstring> albums = m_Library.GetAlbums( artist );
				for ( const auto& album : albums ) {
					AddItem( artistNode, album, Playlist::Type::Album, false /*redraw*/ );
				}
			}
		}
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::AddAlbums()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_ALBUMS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Album );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Album, tvItem.iImage, s_RootOrder[ Playlist::Type::Album ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Album );
	tvInsert.itemex = tvItem;
	m_NodeAlbums = TreeView_InsertItem( m_hWnd, &tvInsert );
	if ( nullptr != m_NodeAlbums ) {
		const std::set<std::wstring> albums = m_Library.GetAlbums();
		for ( const auto& album : albums ) {
			AddItem( m_NodeAlbums, album, Playlist::Type::Album, false /*redraw*/ );
		}
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::AddGenres()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_GENRES, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Genre );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Genre, tvItem.iImage, s_RootOrder[ Playlist::Type::Genre ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Genre );
	tvInsert.itemex = tvItem;
	m_NodeGenres = TreeView_InsertItem( m_hWnd, &tvInsert );
	if ( nullptr != m_NodeGenres ) {
		const std::set<std::wstring> genres = m_Library.GetGenres();
		for ( const auto& genre : genres ) {
			AddItem( m_NodeGenres, genre, Playlist::Type::Genre, false /*redraw*/ );
		}
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::AddYears()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_YEARS, buffer, bufSize );
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = buffer;
	tvItem.iImage = GetIconIndex( Playlist::Type::Year );
	tvItem.iSelectedImage = tvItem.iImage;
	tvItem.lParam = CreateItemData( Playlist::Type::Year, tvItem.iImage, s_RootOrder[ Playlist::Type::Year ] );
	TVINSERTSTRUCT tvInsert = {};
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = GetInsertAfter( Playlist::Type::Year );
	tvInsert.itemex = tvItem;
	m_NodeYears = TreeView_InsertItem( m_hWnd, &tvInsert );
	if ( nullptr != m_NodeYears ) {
		const std::set<long> years = m_Library.GetYears();
		for ( const auto& year : years ) {
			AddItem( m_NodeYears, std::to_wstring( year ), Playlist::Type::Year, false /*redraw*/ );
		}
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::AddCDDA()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const DiscManager::CDDAMediaMap cddaDrives = m_DiscManager.GetCDDADrives();
	for ( auto drive = cddaDrives.rbegin(); drive != cddaDrives.rend(); drive++ ) {
		const CDDAMedia& cddaMedia = drive->second;
		Playlist::Ptr cddaPlaylist = cddaMedia.GetPlaylist();
		if ( cddaPlaylist ) {
			TVITEMEX tvItem = {};
			tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
			tvItem.pszText = const_cast<LPWSTR>( cddaPlaylist->GetName().c_str() );
			tvItem.iImage = GetIconIndex( Playlist::Type::CDDA );
			tvItem.iSelectedImage = tvItem.iImage;
			tvItem.lParam = CreateItemData( Playlist::Type::CDDA, tvItem.iImage, s_RootOrder[ Playlist::Type::CDDA ] );
			TVINSERTSTRUCT tvInsert = {};
			tvInsert.hParent = m_NodeComputer;
			tvInsert.hInsertAfter = TVI_FIRST;
			tvInsert.itemex = tvItem;
			const HTREEITEM cddaItem = TreeView_InsertItem( m_hWnd, &tvInsert );
			if ( nullptr != cddaItem ) {
				m_CDDAMap.insert( PlaylistMap::value_type( cddaItem, cddaPlaylist ) );
			}		
		}
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

HTREEITEM WndTree::AddItem( const HTREEITEM parentItem, const std::wstring& label, const Playlist::Type type, const bool redraw )
{
	HTREEITEM addedItem = nullptr;
	if ( ( nullptr != parentItem ) && !label.empty() ) {
		const bool isFirstChildItem = ( nullptr == TreeView_GetChild( m_hWnd, parentItem ) );
		TVITEMEX tvItem = {};
		tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvItem.pszText = const_cast<LPWSTR>( label.c_str() );
		tvItem.iImage = GetIconIndex( type );
		tvItem.iSelectedImage = tvItem.iImage;
		tvItem.lParam = CreateItemData( type, tvItem.iImage );
		TVINSERTSTRUCT tvInsert = {};
		tvInsert.hParent = parentItem;
		tvInsert.hInsertAfter = TVI_SORT;
		tvInsert.itemex = tvItem;
		addedItem = TreeView_InsertItem( m_hWnd, &tvInsert );
		if ( nullptr != addedItem ) {
			if ( isFirstChildItem && redraw ) {
				RedrawWindow( m_hWnd, NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN | RDW_UPDATENOW );
			}
			AddToFolderNodesMap( addedItem );
		}
	}
	return addedItem;
}

void WndTree::RemoveItem( const HTREEITEM item )
{
	std::set<HTREEITEM> itemsToRemove( { item } );
	GetAllChildren( item, itemsToRemove );

	HTREEITEM selectNewItem = nullptr; 
	HTREEITEM selectedItem = TreeView_GetSelection( m_hWnd );
	if ( itemsToRemove.end() != itemsToRemove.find( selectedItem ) ) {
		selectNewItem = TreeView_GetNextSibling( m_hWnd, item );
		if ( nullptr == selectNewItem ) {
			selectNewItem = TreeView_GetPrevSibling( m_hWnd, item );
			if ( nullptr == selectNewItem ) {
				selectNewItem = TreeView_GetParent( m_hWnd, item );
			}
		}
	}

	for ( const auto& itemToRemove : itemsToRemove ) {
		const Playlist::Type type = GetItemType( itemToRemove );
		switch ( type ) {
			case Playlist::Type::User : {
				m_PlaylistMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::Album : {
				m_AlbumMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::Artist : {
				m_ArtistMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::Genre : {
				m_GenreMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::Year : {
				m_YearMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::CDDA : {
				m_CDDAMap.erase( itemToRemove );
				break;
			}
			case Playlist::Type::Folder : {
				{
					std::lock_guard<std::mutex> lock( m_FolderPlaylistMapMutex );
					m_FolderPlaylistMap.erase( itemToRemove );
				}
				RemoveFromFolderNodesMap( itemToRemove );
				break;
			}
			default : {
				break;
			}
		}
	}

	TreeView_DeleteItem( m_hWnd, item );

	if ( nullptr != selectNewItem ) {
		TreeView_SelectItem( m_hWnd, selectNewItem );
	}
}

Playlist::Type WndTree::GetItemType( const HTREEITEM item ) const
{
	const auto [ type, originalIconIndex, itemOrder ] = GetItemData( item );
	return type;
}

int WndTree::GetItemOrder( const HTREEITEM item ) const
{
	const auto [ type, originalIconIndex, itemOrder ] = GetItemData( item );
	return itemOrder;
}

int WndTree::GetOriginalIconIndex( const HTREEITEM item ) const
{
	const auto [ type, originalIconIndex, itemOrder ] = GetItemData( item );
	return originalIconIndex;
}

std::wstring WndTree::GetItemLabel( const HTREEITEM item ) const
{
	const int bufferSize = 256;
	WCHAR buffer[ bufferSize ] = {};
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_TEXT;
	tvItem.hItem = item;
	tvItem.cchTextMax = bufferSize;
	tvItem.pszText = buffer;
	if ( nullptr != item ) {
		TreeView_GetItem( m_hWnd, &tvItem );
	}
	const std::wstring label = buffer;
	return label;
}

void WndTree::SetItemLabel( const HTREEITEM item, const std::wstring& label ) const
{
	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_HANDLE | TVIF_TEXT;
	tvItem.hItem = item;
	tvItem.pszText = const_cast<LPWSTR>( label.c_str() );
	TreeView_SetItem( m_hWnd, &tvItem );
}

Playlist::Set WndTree::OnUpdatedMedia( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo )
{
	Playlist::Set updatedPlaylists;
	if ( MediaInfo::Source::CDDA == updatedMediaInfo.GetSource() ) {
		UpdateCDDA( updatedMediaInfo, updatedPlaylists );
	} else {
		UpdateArtists( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateAlbums( m_NodeAlbums, previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateGenres( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateYears( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdatePlaylists( updatedMediaInfo, updatedPlaylists );
	}
	return updatedPlaylists;
}

void WndTree::OnRemovedMedia( const MediaInfo::List& mediaList, const bool libraryMaintenance )
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	const Playlist::Ptr selectedPlaylist = GetSelectedPlaylist();
	const bool updateAllTracks = m_PlaylistAll && ( selectedPlaylist != m_PlaylistAll );
	const bool updateStreams = m_PlaylistStreams && ( selectedPlaylist != m_PlaylistStreams );

	for ( const auto& previousMediaInfo : mediaList ) {
		Playlist::Set updatedPlaylists;
		MediaInfo updatedMediaInfo( previousMediaInfo.GetFilename() );
    updatedMediaInfo.SetCueStart( previousMediaInfo.GetCueStart() );
    updatedMediaInfo.SetCueEnd( previousMediaInfo.GetCueEnd() );
		UpdateArtists( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateAlbums( m_NodeAlbums, previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateGenres( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		UpdateYears( previousMediaInfo, updatedMediaInfo, updatedPlaylists );
		if ( updateAllTracks && !libraryMaintenance ) {
			m_PlaylistAll->RemoveItem( previousMediaInfo );
		}
		if ( updateStreams && IsURL( previousMediaInfo.GetFilename() ) ) {
			m_PlaylistStreams->RemoveItem( previousMediaInfo );
		}
	}
  if ( m_PlaylistAll && libraryMaintenance ) {
    m_PlaylistAll->RemoveFiles( mediaList );
  }
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::UpdatePlaylists( const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	for ( const auto& playlistIter : m_ArtistMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( ( updatedPlaylists.end() == updatedPlaylists.find( playlist ) ) && playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
		}
	}
	for ( const auto& playlistIter : m_AlbumMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( ( updatedPlaylists.end() == updatedPlaylists.find( playlist ) ) && playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
		}
	}
	for ( const auto& playlistIter : m_GenreMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( ( updatedPlaylists.end() == updatedPlaylists.find( playlist ) ) && playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
		}
	}
	for ( const auto& playlistIter : m_YearMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( ( updatedPlaylists.end() == updatedPlaylists.find( playlist ) ) && playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
		}
	}

	for ( const auto& playlistIter : m_PlaylistMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
		}
	}

	{
		std::lock_guard<std::mutex> lock( m_FolderPlaylistMapMutex );
		for ( const auto& playlistIter : m_FolderPlaylistMap ) {
			const Playlist::Ptr playlist = playlistIter.second;
			if ( playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
				updatedPlaylists.insert( playlist );
			}
		}
	}

	if ( m_PlaylistAll && m_PlaylistAll->OnUpdatedMedia( updatedMediaInfo ) ) {
		updatedPlaylists.insert( m_PlaylistAll );
	}

	if ( m_PlaylistFavourites && m_PlaylistFavourites->OnUpdatedMedia( updatedMediaInfo ) ) {
		updatedPlaylists.insert( m_PlaylistFavourites );
	}

	if ( m_PlaylistStreams && m_PlaylistStreams->OnUpdatedMedia( updatedMediaInfo ) ) {
		updatedPlaylists.insert( m_PlaylistStreams );
	}
}

void WndTree::UpdateCDDA( const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	for ( const auto& playlistIter : m_CDDAMap ) {
		const Playlist::Ptr playlist = playlistIter.second;
		if ( playlist && playlist->OnUpdatedMedia( updatedMediaInfo ) ) {
			updatedPlaylists.insert( playlist );
			if ( GetItemLabel( playlistIter.first ) != playlist->GetName() ) {
				SetItemLabel( playlistIter.first, playlist->GetName() );
			}
		}
	}
}

void WndTree::UpdateArtists( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	if ( nullptr != m_NodeArtists ) {
		const std::wstring& previousArtist = previousMediaInfo.GetArtist();
		const std::wstring& updatedArtist = updatedMediaInfo.GetArtist();
		const std::wstring& previousAlbum = previousMediaInfo.GetAlbum();
		const std::wstring& updatedAlbum = updatedMediaInfo.GetAlbum();
		if ( ( previousArtist != updatedArtist ) || ( previousAlbum != updatedAlbum ) ) {

			if ( previousArtist == updatedArtist ) {
				HTREEITEM artistNode = TreeView_GetChild( m_hWnd, m_NodeArtists );
				while ( nullptr != artistNode ) {
					const std::wstring artist = GetItemLabel( artistNode );
					if ( previousArtist == artist ) {
						UpdateAlbums( artistNode, previousMediaInfo, updatedMediaInfo, updatedPlaylists );

						const auto artistPlaylist = m_ArtistMap.find( artistNode );
						if ( m_ArtistMap.end() != artistPlaylist ) {
							Playlist::Ptr playlist = artistPlaylist->second;
							if ( playlist ) {
								playlist->OnUpdatedMedia( updatedMediaInfo );
								updatedPlaylists.insert( playlist );
							}
						}
						break;
					}
					artistNode = TreeView_GetNextSibling( m_hWnd, artistNode );
				}
			} else {
				bool handledPreviousArtist = false;
				bool handledUpdatedArtist = false;
				HTREEITEM artistNodeToRemove = nullptr;
				HTREEITEM artistNode = TreeView_GetChild( m_hWnd, m_NodeArtists );

				while ( ( nullptr != artistNode ) && ( !handledPreviousArtist || !handledUpdatedArtist ) ) {
					const std::wstring artist = GetItemLabel( artistNode );
					if ( previousArtist == artist ) {			

						const auto artistPlaylist = m_ArtistMap.find( artistNode );
						if ( m_ArtistMap.end() != artistPlaylist ) {
							Playlist::Ptr playlist = artistPlaylist->second;
							if ( playlist ) {
								playlist->RemoveItem( previousMediaInfo );
								updatedPlaylists.insert( playlist );
							}
						}

						if ( !previousAlbum.empty() ) {
							HTREEITEM albumNode = TreeView_GetChild( m_hWnd, artistNode );
							while ( nullptr != albumNode ) {
								const std::wstring album = GetItemLabel( albumNode );
								if ( previousAlbum == album ) {
									const auto albumPlaylist = m_AlbumMap.find( albumNode );
									if ( m_AlbumMap.end() != albumPlaylist ) {
										Playlist::Ptr playlist = albumPlaylist->second;
										if ( playlist ) {
											playlist->RemoveItem( previousMediaInfo );
											updatedPlaylists.insert( playlist );
										}
									}
									if ( !m_Library.GetArtistAndAlbumExists( artist, album ) ) {
										RemoveItem( albumNode );
									}
									break;
								}
								albumNode = TreeView_GetNextSibling( m_hWnd, albumNode );
							}
						}

						if ( !m_Library.GetArtistExists( artist ) ) {
							artistNodeToRemove = artistNode;
						}

						handledPreviousArtist = true;
					} else if ( updatedArtist == artist ) {

						const auto artistPlaylist = m_ArtistMap.find( artistNode );
						if ( m_ArtistMap.end() != artistPlaylist ) {
							Playlist::Ptr playlist = artistPlaylist->second;
							if ( playlist ) {
								int position = 0;
								bool addedAsDuplicate = false;
								const Playlist::Item playlistItem = playlist->AddItem( updatedMediaInfo, position, addedAsDuplicate );
								VUPlayer* vuplayer = VUPlayer::Get();
								if ( nullptr != vuplayer ) {
									if ( addedAsDuplicate ) {
										vuplayer->OnPlaylistItemUpdated( playlist.get(), playlistItem );
									} else {
										vuplayer->OnPlaylistItemAdded( playlist.get(), playlistItem, position );
									}
								}
								updatedPlaylists.insert( playlist );
							}
						}

						if ( !updatedAlbum.empty() ) {
							HTREEITEM albumNode = TreeView_GetChild( m_hWnd, artistNode );
							bool handledUpdatedAlbum = false;
							while ( ( nullptr != albumNode ) && !handledUpdatedAlbum ) {
								const std::wstring album = GetItemLabel( albumNode );
								if ( updatedAlbum == album ) {
									const auto albumPlaylist = m_AlbumMap.find( albumNode );
									if ( m_AlbumMap.end() != albumPlaylist ) {
										Playlist::Ptr playlist = albumPlaylist->second;
										if ( playlist ) {
											int position = 0;
											bool addedAsDuplicate = false;
											const Playlist::Item playlistItem = playlist->AddItem( updatedMediaInfo, position, addedAsDuplicate );
											VUPlayer* vuplayer = VUPlayer::Get();
											if ( nullptr != vuplayer ) {
												if ( addedAsDuplicate ) {
													vuplayer->OnPlaylistItemUpdated( playlist.get(), playlistItem );
												} else {
													vuplayer->OnPlaylistItemAdded( playlist.get(), playlistItem, position );
												}
											}
											updatedPlaylists.insert( playlist );
										}
									}
									handledUpdatedAlbum = true;
								}
								albumNode = TreeView_GetNextSibling( m_hWnd, albumNode );
							}
							if ( !handledUpdatedAlbum ) {
								AddItem( artistNode, updatedAlbum, Playlist::Type::Album );
							}
						}

						handledUpdatedArtist = true;
					}
					artistNode = TreeView_GetNextSibling( m_hWnd, artistNode );
				}

				if ( !handledUpdatedArtist ) {
					const HTREEITEM addedArtist = AddItem( m_NodeArtists, updatedArtist, Playlist::Type::Artist );
					if ( !updatedAlbum.empty() && ( nullptr != addedArtist ) ) {
						AddItem( addedArtist, updatedAlbum, Playlist::Type::Album );
					}
				}

				if ( nullptr != artistNodeToRemove ) {
					RemoveItem( artistNodeToRemove );
				}			
			}
		}
	}
}

void WndTree::UpdateAlbums( const HTREEITEM parentItem, const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	if ( nullptr != parentItem ) {
		const std::wstring& previousAlbum = previousMediaInfo.GetAlbum();
		const std::wstring& updatedAlbum = updatedMediaInfo.GetAlbum();
		if ( previousAlbum != updatedAlbum ) {
			bool handledPrevious = false;
			bool handledUpdated = false;
			HTREEITEM albumNodeToRemove = nullptr;
			HTREEITEM albumNode = TreeView_GetChild( m_hWnd, parentItem );

			while ( ( nullptr != albumNode ) && ( !handledPrevious || !handledUpdated ) ) {
				const std::wstring album = GetItemLabel( albumNode );
				if ( previousAlbum == album ) {
					const auto albumPlaylist = m_AlbumMap.find( albumNode );
					if ( m_AlbumMap.end() != albumPlaylist ) {
						Playlist::Ptr playlist = albumPlaylist->second;
						if ( playlist ) {
							playlist->RemoveItem( previousMediaInfo );
							updatedPlaylists.insert( playlist );
						}
					}
					if ( !m_Library.GetAlbumExists( album ) ) {
						albumNodeToRemove = albumNode;
					}
					handledPrevious = true;
				} else if ( updatedAlbum == album ) {
					const auto albumPlaylist = m_AlbumMap.find( albumNode );
					if ( m_AlbumMap.end() != albumPlaylist ) {
						Playlist::Ptr playlist = albumPlaylist->second;
						if ( playlist ) {
							int position = 0;
							bool addedAsDuplicate = false;
							const Playlist::Item playlistItem = playlist->AddItem( updatedMediaInfo, position, addedAsDuplicate );
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
								if ( addedAsDuplicate ) {
									vuplayer->OnPlaylistItemUpdated( playlist.get(), playlistItem );
								} else {
									vuplayer->OnPlaylistItemAdded( playlist.get(), playlistItem, position );
								}
							}
							updatedPlaylists.insert( playlist );
						}
					}				
					handledUpdated = true;
				}
				albumNode = TreeView_GetNextSibling( m_hWnd, albumNode );
			}

			if ( !handledUpdated ) {
				AddItem( parentItem, updatedAlbum, Playlist::Type::Album );
			}
			if ( nullptr != albumNodeToRemove ) {
				RemoveItem( albumNodeToRemove );
			}
		}
	}
}

void WndTree::UpdateGenres( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	if ( nullptr != m_NodeGenres ) {
		const std::wstring& previousGenre = previousMediaInfo.GetGenre();
		const std::wstring& updatedGenre = updatedMediaInfo.GetGenre();
		if ( previousGenre != updatedGenre ) {
			bool handledPrevious = false;
			bool handledUpdated = false;
			HTREEITEM genreNodeToRemove = nullptr;
			HTREEITEM genreNode = TreeView_GetChild( m_hWnd, m_NodeGenres );

			while ( ( nullptr != genreNode ) && ( !handledPrevious || !handledUpdated ) ) {
				const std::wstring genre = GetItemLabel( genreNode );
				if ( previousGenre == genre ) {
					const auto genrePlaylist = m_GenreMap.find( genreNode );
					if ( m_GenreMap.end() != genrePlaylist ) {
						Playlist::Ptr playlist = genrePlaylist->second;
						if ( playlist ) {
							playlist->RemoveItem( previousMediaInfo );
							updatedPlaylists.insert( playlist );
						}
					}
					if ( !m_Library.GetGenreExists( genre ) ) {
						genreNodeToRemove = genreNode;
					}
					handledPrevious = true;
				} else if ( updatedGenre == genre ) {
					const auto genrePlaylist = m_GenreMap.find( genreNode );
					if ( m_GenreMap.end() != genrePlaylist ) {
						Playlist::Ptr playlist = genrePlaylist->second;
						if ( playlist ) {
							int position = 0;
							bool addedAsDuplicate = false;
							const Playlist::Item playlistItem = playlist->AddItem( updatedMediaInfo, position, addedAsDuplicate );
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
								if ( addedAsDuplicate ) {
									vuplayer->OnPlaylistItemUpdated( playlist.get(), playlistItem );
								} else {
									vuplayer->OnPlaylistItemAdded( playlist.get(), playlistItem, position );
								}
							}
							updatedPlaylists.insert( playlist );
						}
					}				
					handledUpdated = true;
				}
				genreNode = TreeView_GetNextSibling( m_hWnd, genreNode );
			}

			if ( !handledUpdated ) {
				AddItem( m_NodeGenres, updatedGenre, Playlist::Type::Genre );
			}
			if ( nullptr != genreNodeToRemove ) {
				RemoveItem( genreNodeToRemove );
			}
		}
	}
}

void WndTree::UpdateYears( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists )
{
	if ( nullptr != m_NodeYears ) {
		const long previousYearValue = previousMediaInfo.GetYear();
		const long updatedYearValue = updatedMediaInfo.GetYear();
		if ( previousYearValue != updatedYearValue ) {
			const std::wstring previousYear = std::to_wstring( previousYearValue );
			const std::wstring updatedYear = std::to_wstring( updatedYearValue );
			bool handledPrevious = false;
			bool handledUpdated = false;
			HTREEITEM yearNodeToRemove = nullptr;
			HTREEITEM yearNode = TreeView_GetChild( m_hWnd, m_NodeYears );

			while ( ( nullptr != yearNode ) && ( !handledPrevious || !handledUpdated ) ) {
				const std::wstring year = GetItemLabel( yearNode );
				if ( previousYear == year ) {
					const auto yearPlaylist = m_YearMap.find( yearNode );
					if ( m_YearMap.end() != yearPlaylist ) {
						Playlist::Ptr playlist = yearPlaylist->second;
						if ( playlist ) {
							playlist->RemoveItem( previousMediaInfo );
							updatedPlaylists.insert( playlist );
						}
					}
					if ( !m_Library.GetYearExists( previousYearValue ) ) {
						yearNodeToRemove = yearNode;
					}
					handledPrevious = true;
				} else if ( updatedYear == year ) {
					const auto yearPlaylist = m_YearMap.find( yearNode );
					if ( m_YearMap.end() != yearPlaylist ) {
						Playlist::Ptr playlist = yearPlaylist->second;
						if ( playlist ) {
							int position = 0;
							bool addedAsDuplicate = false;
							const Playlist::Item playlistItem = playlist->AddItem( updatedMediaInfo, position, addedAsDuplicate );
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
								if ( addedAsDuplicate ) {
									vuplayer->OnPlaylistItemUpdated( playlist.get(), playlistItem );
								} else {
									vuplayer->OnPlaylistItemAdded( playlist.get(), playlistItem, position );
								}
							}
							updatedPlaylists.insert( playlist );
						}
					}				
					handledUpdated = true;
				}
				yearNode = TreeView_GetNextSibling( m_hWnd, yearNode );
			}

			if ( !handledUpdated && ( updatedYearValue >= MINYEAR ) && ( updatedYearValue <= MAXYEAR ) ) {
				AddItem( m_NodeYears, updatedYear, Playlist::Type::Year );
			}
			if ( nullptr != yearNodeToRemove ) {
				RemoveItem( yearNodeToRemove );
			}
		}
	}
}

bool WndTree::IsShown( const UINT commandID ) const
{
	bool isShown = false;
	switch ( commandID ) {
		case ID_TREEMENU_FAVOURITES : {
			isShown = ( nullptr != m_NodeFavourites );
			break;
		}
		case ID_TREEMENU_STREAMS : {
			isShown = ( nullptr != m_NodeStreams );
			break;
		}
		case ID_TREEMENU_ALLTRACKS : {
			isShown = ( nullptr != m_NodeAll );
			break;
		}
		case ID_TREEMENU_ARTISTS : {
			isShown = ( nullptr != m_NodeArtists );
			break;
		}
		case ID_TREEMENU_ALBUMS : {
			isShown = ( nullptr != m_NodeAlbums );
			break;
		}
		case ID_TREEMENU_GENRES : {
			isShown = ( nullptr != m_NodeGenres );
			break;
		}
		case ID_TREEMENU_YEARS : {
			isShown = ( nullptr != m_NodeYears );
			break;
		}
	}
	return isShown;
}

void WndTree::OnFavourites()
{
	if ( nullptr == m_NodeFavourites ) {
		AddFavourites();
	} else {
		RemoveFavourites();
	}
}

void WndTree::OnStreams()
{
	if ( nullptr == m_NodeStreams ) {
		AddStreams();
	} else {
		RemoveStreams();
	}
}

void WndTree::OnAllTracks()
{
	if ( nullptr == m_NodeAll ) {
		AddAllTracks();
	} else {
		RemoveAllTracks();
	}
}

void WndTree::OnArtists()
{
	if ( nullptr == m_NodeArtists ) {
		AddArtists();
	} else {
		RemoveArtists();
	}
}

void WndTree::OnAlbums()
{
	if ( nullptr == m_NodeAlbums ) {
		AddAlbums();
	} else {
		RemoveAlbums();
	}
}

void WndTree::OnGenres()
{
	if ( nullptr == m_NodeGenres ) {
		AddGenres();
	} else {
		RemoveGenres();
	}
}

void WndTree::OnYears()
{
	if ( nullptr == m_NodeYears ) {
		AddYears();
	} else {
		RemoveYears();
	}
}

void WndTree::RemoveFavourites()
{
	if ( nullptr != m_NodeFavourites ) {
		TreeView_DeleteItem( m_hWnd, m_NodeFavourites );
		m_NodeFavourites = nullptr;
	}
}

void WndTree::RemoveStreams()
{
	if ( nullptr != m_NodeStreams ) {
		TreeView_DeleteItem( m_hWnd, m_NodeStreams );
		m_NodeStreams = nullptr;
	}
}

void WndTree::RemoveAllTracks()
{
	if ( nullptr != m_NodeAll ) {
		TreeView_DeleteItem( m_hWnd, m_NodeAll );
		m_NodeAll = nullptr;
	}
}

void WndTree::RemoveArtists()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	if ( nullptr != m_NodeArtists ) {
		HTREEITEM artistNode = TreeView_GetChild( m_hWnd, m_NodeArtists );
		while ( nullptr != artistNode ) {
			HTREEITEM albumNode = TreeView_GetChild( m_hWnd, artistNode );
			while ( nullptr != albumNode ) {
				m_AlbumMap.erase( albumNode );
				albumNode = TreeView_GetNextSibling( m_hWnd, albumNode );
			}
			artistNode = TreeView_GetNextSibling( m_hWnd, artistNode );
		}
		TreeView_DeleteItem( m_hWnd, m_NodeArtists );
		m_NodeArtists = nullptr;
		m_ArtistMap.clear();
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::RemoveAlbums()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	if ( nullptr != m_NodeAlbums ) {
		HTREEITEM albumNode = TreeView_GetChild( m_hWnd, m_NodeAlbums );
		while ( nullptr != albumNode ) {
			m_AlbumMap.erase( albumNode );
			albumNode = TreeView_GetNextSibling( m_hWnd, albumNode );
		}
		TreeView_DeleteItem( m_hWnd, m_NodeAlbums );
		m_NodeAlbums = nullptr;
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::RemoveGenres()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	if ( nullptr != m_NodeGenres ) {
		TreeView_DeleteItem( m_hWnd, m_NodeGenres );
		m_NodeGenres = nullptr;
		m_GenreMap.clear();
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::RemoveYears()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	if ( nullptr != m_NodeYears ) {
		TreeView_DeleteItem( m_hWnd, m_NodeYears );
		m_NodeYears = nullptr;
		m_YearMap.clear();
	}
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::RemoveCDDA()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );
	std::set<HTREEITEM> cddaNodes;
	HTREEITEM currentItem = TreeView_GetChild( m_hWnd, m_NodeComputer );
	while ( nullptr != currentItem ) {
		if ( Playlist::Type::CDDA == GetItemType( currentItem ) ) {
			cddaNodes.insert( currentItem );
		}
		currentItem = TreeView_GetNextSibling( m_hWnd, currentItem );
	}
	for ( const auto& node : cddaNodes ) {
		TreeView_DeleteItem( m_hWnd, node );
	}
	m_CDDAMap.clear();
	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

void WndTree::Populate()
{
	HCURSOR oldCursor = SetCursor( LoadCursor( NULL /*instance*/, IDC_WAIT ) );

	RefreshComputerNode();

	// Remove all tree items apart from folder items in the computer node.
	std::list<HTREEITEM> itemsToDelete;
	HTREEITEM rootItem = TreeView_GetRoot( m_hWnd );
	while ( nullptr != rootItem ) {
		if ( m_NodeComputer != rootItem ) {
			itemsToDelete.push_back( rootItem );
		} else {
			HTREEITEM childItem = TreeView_GetChild( m_hWnd, rootItem );
			while ( nullptr != childItem ) {
				if ( Playlist::Type::Folder != GetItemType( childItem ) ) {
					itemsToDelete.push_back( childItem );
				}
				childItem = TreeView_GetNextSibling( m_hWnd, childItem );
			}
		}
		rootItem = TreeView_GetNextSibling( m_hWnd, rootItem );
	}
	for ( const auto& item : itemsToDelete ) {
		TreeView_DeleteItem( m_hWnd, item );
	}	

	m_PlaylistMap.clear();
	m_ArtistMap.clear();
	m_AlbumMap.clear();
	m_GenreMap.clear();
	m_YearMap.clear();
	m_CDDAMap.clear();

	LoadAllTracks();
	LoadFavourites();
	LoadStreams();

	LOGFONT font = {};
	COLORREF fontColour = 0;
	COLORREF backgroundColour = 0;
	COLORREF highlightColour = 0;
	COLORREF iconColour = 0;
	bool favourites = true;
	bool streams = true;
	bool allTracks = true;
	bool artists = true;
	bool albums = true;
	bool genres = true;
	bool years = true;
	m_Settings.GetTreeSettings( font, fontColour, backgroundColour, highlightColour, iconColour, favourites, streams, allTracks, artists, albums, genres, years );
	if ( favourites ) {
		AddFavourites();
	}
	if ( streams ) {
		AddStreams();
	}
	if ( allTracks ) {
		AddAllTracks();
	}
	if ( artists ) {
		AddArtists();
	}
	if ( albums ) {
		AddAlbums();
	}
	if ( genres ) {
		AddGenres();
	}
	if ( years ) {
		AddYears();
	}
	LoadPlaylists();
	ProcessPendingPlaylists();

	AddCDDA();

	SetCursor( oldCursor );
}

void WndTree::CreateImageList()
{
	static const std::map<Playlist::Type, UINT> treeIconResourceMap = {
		{ Playlist::Type::User, IDI_PLAYLIST },
		{ Playlist::Type::All, IDI_LIBRARY },
		{ Playlist::Type::Artist, IDI_ARTIST },
		{ Playlist::Type::Album, IDI_ALBUM },
		{ Playlist::Type::CDDA, IDI_ALBUM },
		{ Playlist::Type::Genre, IDI_GENRE },
		{ Playlist::Type::Year, IDI_YEAR },
		{ Playlist::Type::Favourites, IDI_FAVOURITES },
		{ Playlist::Type::Streams, IDI_STREAMS },
		{ Playlist::Type::Folder, IDI_FOLDER },
	};

	const int iconSize = GetIconSize();
	const int imageCount = 1;
	const HIMAGELIST previousImageList = TreeView_GetImageList( m_hWnd, TVSIL_NORMAL );
	if ( m_ImageList = ImageList_Create( iconSize, iconSize, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ ); nullptr != m_ImageList ) {
		const COLORREF colour = m_IsHighContrast ? GetSysColor( COLOR_HIGHLIGHT ) : GetIconColour();
		for ( const auto &[ playlistType, iconID ] : treeIconResourceMap ) {
			if ( const HBITMAP hBitmap = CreateColourBitmap( m_hInst, iconID, iconSize, colour ); nullptr != hBitmap ) {
				const int iconIndex = ImageList_Add( m_ImageList, hBitmap, nullptr /*mask*/ );
				m_IconMap.insert( IconMap::value_type( playlistType, iconIndex ) );
				DeleteObject( hBitmap );
				if ( Playlist::Type::Folder == playlistType ) {
					m_IconIndexFolder = iconIndex;
				}
			}
		}
		if ( const HBITMAP hBitmap = CreateColourBitmap( m_hInst, IDI_DISK, iconSize, colour ); nullptr != hBitmap ) {
			m_IconIndexDrive = ImageList_Add( m_ImageList, hBitmap, nullptr /*mask*/ );
			DeleteObject( hBitmap );
		}
		if ( const HBITMAP hBitmap = CreateColourBitmap( m_hInst, IDI_COMPUTER, iconSize, colour ); nullptr != hBitmap ) {
			m_IconIndexComputer = ImageList_Add( m_ImageList, hBitmap, nullptr /*mask*/ );
			DeleteObject( hBitmap );
		}
		if ( const HBITMAP hBitmap = CreateColourBitmap( m_hInst, IDI_VOLUME, iconSize, colour ); nullptr != hBitmap ) {
			m_IconIndexPlaying = ImageList_Add( m_ImageList, hBitmap, nullptr /*mask*/ );
			DeleteObject( hBitmap );
		}
		if ( const HBITMAP hBitmap = CreateColourBitmap( m_hInst, IDI_PAUSE, iconSize, colour ); nullptr != hBitmap ) {
			m_IconIndexPaused = ImageList_Add( m_ImageList, hBitmap, nullptr /*mask*/ );
			DeleteObject( hBitmap );
		}
	}

	TreeView_SetImageList( m_hWnd, m_ImageList, TVSIL_NORMAL );
	if ( nullptr != previousImageList ) {
		ImageList_Destroy( previousImageList );
	}
}

int WndTree::GetIconIndex( const Playlist::Type type ) const
{
	int iconIndex = 0;
	const auto iter = m_IconMap.find( type );
	if ( m_IconMap.end() != iter ) {
		iconIndex = iter->second;
	}
	return iconIndex;
}

Playlist::Ptr WndTree::GetPlaylistFavourites() const
{
	return m_PlaylistFavourites;
}

Playlist::Ptr WndTree::GetPlaylistStreams() const
{
	return m_PlaylistStreams;
}

Playlist::Ptr WndTree::GetPlaylistAll() const
{
	return m_PlaylistAll;
}

void WndTree::LoadAllTracks()
{
	m_PlaylistAll.reset( new Playlist( m_Library, Playlist::Type::All ) );
	const MediaInfo::List allMedia = m_Library.GetAllMedia();
	for ( const auto& mediaInfo : allMedia ) {
		m_PlaylistAll->AddItem( mediaInfo );
	}
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_ALLTRACKS, buffer, bufSize );
	m_PlaylistAll->SetName( buffer );
}

void WndTree::LoadFavourites()
{
	m_PlaylistFavourites = m_Settings.GetFavourites();
	if ( m_PlaylistFavourites ) {
		const int bufSize = 32;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_FAVOURITES, buffer, bufSize );
		m_PlaylistFavourites->SetName( buffer );
	}
}

void WndTree::LoadStreams()
{
	m_PlaylistStreams = std::make_shared<Playlist>( m_Library, s_StreamsListID, Playlist::Type::Streams );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_STREAMS, buffer, bufSize );
	m_PlaylistStreams->SetName( buffer );
	const auto items = m_Library.GetStreams();
	for ( const auto& item : items ) {
		m_PlaylistStreams->AddItem( item );
	}
}

bool WndTree::IsPlaylistDeleteEnabled()
{
	const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	const Playlist::Ptr playlist = GetPlaylist( hSelectedItem );
	const bool enabled = ( playlist && ( playlist->GetType() == Playlist::Type::User ) );
	return enabled;
}

bool WndTree::IsPlaylistExportEnabled()
{
	const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	const Playlist::Ptr playlist = GetPlaylist( hSelectedItem );
	const bool enabled = ( playlist && ( playlist->GetType() != Playlist::Type::CDDA ) );
	return enabled;
}

bool WndTree::IsPlaylistRenameEnabled()
{
	const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	const Playlist::Ptr playlist = GetPlaylist( hSelectedItem );
	const bool enabled = ( playlist && ( playlist->GetType() == Playlist::Type::User ) );
	return enabled;
}

void WndTree::RenameSelectedPlaylist()
{
	TreeView_EditLabel( m_hWnd, TreeView_GetSelection( m_hWnd ) );
}

HTREEITEM WndTree::GetInsertAfter( const Playlist::Type type ) const
{
	HTREEITEM insertAfter = TVI_ROOT;
	const LPARAM order = s_RootOrder[ type ];
	HTREEITEM currentItem = TreeView_GetRoot( m_hWnd );
	while ( nullptr != currentItem ) {
		const LPARAM itemOrder = GetItemOrder( currentItem );
		if ( itemOrder < order ) {
			insertAfter = currentItem;
		} else {
			break;
		}
		currentItem = TreeView_GetNextSibling( m_hWnd, currentItem );
	}
	return insertAfter;
}

void WndTree::OnRefreshDiscDrives()
{
	RefreshComputerNode();

	std::string currentSelectedPlaylistID;
	if ( const HTREEITEM selectedItem = TreeView_GetSelection( m_hWnd ); nullptr != selectedItem ) {
		if ( const auto item = m_CDDAMap.find( selectedItem ); ( m_CDDAMap.end() != item ) && item->second ) {
			currentSelectedPlaylistID = item->second->GetID();
		}
	}

	std::string currentOutputPlaylistID;
	for ( const auto& [ node, playlist ] : m_CDDAMap ) {
		if ( playlist && ( node == m_NodeCurrentOutput ) ) {
			currentOutputPlaylistID = playlist->GetID();
			break;
		}
	}

	RemoveCDDA();
	AddCDDA();

	if ( !currentOutputPlaylistID.empty() ) {
		for ( const auto& [ node, playlist ] : m_CDDAMap ) {
			if ( playlist && ( playlist->GetID() == currentOutputPlaylistID ) ) {
				m_NodeCurrentOutput = node;
				break;
			}
		}
	}

	if ( !currentSelectedPlaylistID.empty() ) {
		for ( const auto& [ node, playlist ] : m_CDDAMap ) {
			if ( playlist && ( playlist->GetID() == currentSelectedPlaylistID ) ) {
				TreeView_SelectItem( m_hWnd, node );
				break;
			}
		}
	}
}

void WndTree::RefreshComputerNode()
{
	SendMessage( m_hWnd, WM_SETREDRAW, FALSE, 0 );

	if ( nullptr == m_NodeComputer ) {
		const int bufSize = 32;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_COMPUTER, buffer, bufSize );
		TVITEMEX tvItem = {};
		tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvItem.pszText = buffer;
		tvItem.iImage = m_IconIndexComputer;
		tvItem.iSelectedImage = tvItem.iImage;
		tvItem.lParam = CreateItemData( Playlist::Type::Folder, tvItem.iImage );
		TVINSERTSTRUCT tvInsert = {};
		tvInsert.hParent = TVI_ROOT;
		tvInsert.hInsertAfter = TVI_FIRST;
		tvInsert.itemex = tvItem;
		m_NodeComputer = TreeView_InsertItem( m_hWnd, &tvInsert );

		tvInsert.hParent = m_NodeComputer;
		tvInsert.hInsertAfter = TVI_LAST;
		const RootFolderInfoList userFolders = GetUserFolders();
		for ( const auto& userFolder : userFolders ) {
			tvItem.pszText = const_cast<LPWSTR>( userFolder.Name.c_str() );
			tvItem.iImage = userFolder.IconIndex;
			tvItem.iSelectedImage = tvItem.iImage;
			tvInsert.itemex = tvItem;
			const HTREEITEM hItem = TreeView_InsertItem( m_hWnd, &tvInsert );
			if ( nullptr != hItem ) {
				m_RootComputerFolders.insert( RootFolderInfoMap::value_type( hItem, userFolder ) );
				AddToFolderNodesMap( hItem );
				AddSubFolders( hItem );
			}
		}
	}

	// Remove any drives from the tree control that are no longer present.
	std::list<HTREEITEM> itemsToDelete;
	const RootFolderInfoList currentDrives = GetComputerDrives();
	auto rootFolderIter = m_RootComputerFolders.begin();
	while ( m_RootComputerFolders.end() != rootFolderIter ) {
		if ( RootFolderType::Drive == rootFolderIter->second.Type ) {
			const auto foundDrive = std::find( currentDrives.begin(), currentDrives.end(), rootFolderIter->second );
			if ( currentDrives.end() == foundDrive ) {
				itemsToDelete.push_back( rootFolderIter->first );
			}
		}
		++rootFolderIter;
	}
	for ( const auto& item : itemsToDelete ) {
		RemoveFromFolderNodesMap( item );
		const auto folderInfo = m_RootComputerFolders.find( item );
		if ( m_RootComputerFolders.end() != folderInfo ) {
			m_FolderMonitor.RemoveFolder( folderInfo->second.Path );
		}
		m_RootComputerFolders.erase( item );
		TreeView_DeleteItem( m_hWnd, item );
	}

	// Add any new drives to the tree control.
	for ( const auto& drive : currentDrives ) {
		const auto foundDrive = std::find_if( m_RootComputerFolders.begin(), m_RootComputerFolders.end(), [ &drive ] ( const RootFolderInfoMap::value_type& folderInfo )
		{
			return ( drive == folderInfo.second );
		} );

		if ( m_RootComputerFolders.end() == foundDrive ) {
			TVITEMEX tvItem = {};
			tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
			tvItem.pszText = const_cast<LPWSTR>( drive.Name.c_str() );
			tvItem.iImage = drive.IconIndex;
			tvItem.iSelectedImage = tvItem.iImage;
			tvItem.lParam = CreateItemData( Playlist::Type::Folder, tvItem.iImage );
			TVINSERTSTRUCT tvInsert = {};
			tvInsert.hParent = m_NodeComputer;

			// Maintain drive letter ordering.
			tvInsert.hInsertAfter = TVI_LAST;
			HTREEITEM childItem = TreeView_GetChild( m_hWnd, m_NodeComputer );
			while ( nullptr != childItem ) {
				const auto itemInfo = m_RootComputerFolders.find( childItem );
				if ( ( m_RootComputerFolders.end() != itemInfo ) && ( RootFolderType::Drive == itemInfo->second.Type ) && ( drive.Path < itemInfo->second.Path ) ) {
					const HTREEITEM previousItem = TreeView_GetPrevSibling( m_hWnd, childItem );
					if ( nullptr != previousItem ) {
						tvInsert.hInsertAfter = previousItem;
						break;
					}
				}
				childItem = TreeView_GetNextSibling( m_hWnd, childItem );
			}

			tvInsert.itemex = tvItem;
			const HTREEITEM hItem = TreeView_InsertItem( m_hWnd, &tvInsert );
			if ( nullptr != hItem ) {
				m_RootComputerFolders.insert( RootFolderInfoMap::value_type( hItem, drive ) );
				AddToFolderNodesMap( hItem );
				AddSubFolders( hItem );
			}

			m_FolderMonitor.AddFolder( drive.Path, [ this ]( const FolderMonitor::Event monitorEvent, const std::wstring& oldFilename, const std::wstring& newFilename )
			{
				OnFolderMonitorCallback( monitorEvent, oldFilename, newFilename );
			} );
		}
	}

	SendMessage( m_hWnd, WM_SETREDRAW, TRUE, 0 );
}

WndTree::RootFolderInfoList WndTree::GetUserFolders() const
{
	RootFolderInfoList userFolders;
	for ( const auto& knownFolderID : s_UserFolders ) {
		LPWSTR path = nullptr;
		if ( SUCCEEDED( SHGetKnownFolderPath( knownFolderID, KF_FLAG_DEFAULT, nullptr /*token*/, &path ) ) ) {
			const std::wstring folderPath( path );
			const size_t pos = folderPath.find_last_of( L"/\\" );
			if ( std::wstring::npos != pos ) {
				const std::wstring folderName = folderPath.substr( 1 + pos );
				userFolders.push_back( { folderName, folderPath, m_IconIndexFolder, RootFolderType::UserFolder } );
			}
			CoTaskMemFree( path );
		}
	}
	return userFolders;
}

WndTree::RootFolderInfoList WndTree::GetComputerDrives() const
{
	RootFolderInfoList computerDrives;
	const DWORD bufferLength = GetLogicalDriveStrings( 0 /*bufferLength*/, nullptr /*buffer*/ );
	if ( bufferLength > 0 ) {
		std::vector<WCHAR> buffer( static_cast<size_t>( bufferLength ) );
		if ( GetLogicalDriveStrings( bufferLength, buffer.data() ) <= bufferLength ) {
			const auto dataDiscDrives = m_DiscManager.GetDataDrives();
			LPWSTR driveString = buffer.data();
			size_t stringLength = wcslen( driveString );
			while ( stringLength > 0 ) {
				const UINT driveType = GetDriveType( driveString );
				if ( ( DRIVE_FIXED == driveType ) || ( DRIVE_REMOVABLE == driveType ) || ( DRIVE_REMOTE == driveType ) || ( DRIVE_CDROM == driveType ) ) {
					bool includeDrive = true;
					const auto driveLetter = driveString[ 0 ];

					if ( DRIVE_CDROM == driveType ) {
						includeDrive = ( dataDiscDrives.end() != dataDiscDrives.find( driveLetter ) );
					}

					if ( includeDrive ) {
						const int nameBufferSize = MAX_PATH;
						WCHAR nameBuffer[ nameBufferSize ] = {};
						LoadString( m_hInst, IDS_DISK, nameBuffer, nameBufferSize );
						std::wstring driveName = nameBuffer;

						if ( DRIVE_REMOTE == driveType ) {
							DWORD remoteNameSize = 512;
							std::vector<char> remoteNameBuffer( remoteNameSize );
							DWORD gotRemoteName = WNetGetUniversalName( driveString, REMOTE_NAME_INFO_LEVEL, &remoteNameBuffer[ 0 ], &remoteNameSize );
							if ( ( NO_ERROR != gotRemoteName ) && ( remoteNameSize > remoteNameBuffer.size() ) ) {
								remoteNameBuffer.resize( remoteNameSize );
								gotRemoteName = WNetGetUniversalName( driveString, REMOTE_NAME_INFO_LEVEL, &remoteNameBuffer[ 0 ], &remoteNameSize );
							}
							if ( NO_ERROR == gotRemoteName ) {
								REMOTE_NAME_INFO* remoteInfo = reinterpret_cast<REMOTE_NAME_INFO*>( &remoteNameBuffer[ 0 ] );
								driveName = remoteInfo->lpConnectionName;
							}
						} else if ( DRIVE_CDROM == driveType ) {
							if ( const auto drive = dataDiscDrives.find( driveLetter ); ( dataDiscDrives.end() != drive ) && !drive->second.empty() ) {
								driveName = drive->second;
							}
						} else if ( 0 != GetVolumeInformation( driveString, nameBuffer, nameBufferSize, nullptr, nullptr, nullptr, nullptr, 0 ) ) {
							if ( wcslen( nameBuffer ) > 0 ) {
								driveName = nameBuffer;
							}
						}

						driveName += L" (" + std::wstring( driveString, 1 ) + L":)";
						std::wstring drivePath( driveString );
						if ( !drivePath.empty() && ( ( drivePath.back() == '\\' ) || ( drivePath.back() == '/' ) ) ) {
							drivePath.pop_back();
						}
						computerDrives.push_back( { driveName, drivePath, m_IconIndexDrive, RootFolderType::Drive } );
					}
				}
				driveString += stringLength + 1;
				stringLength = wcslen( driveString );
			}
		}
	}

	return computerDrives;
}

std::set<std::wstring> WndTree::GetSubFolders( const std::wstring& parent ) const
{
	std::set<std::wstring> subfolders;
	
	std::wstring filename = parent;
	if ( !filename.empty() && ( filename.back() != '\\' ) && ( filename.back() != '/' ) ) {
		filename += '\\';
	}
	filename += '*';

	const FINDEX_INFO_LEVELS levels = FindExInfoBasic;
	const FINDEX_SEARCH_OPS searchOp = FindExSearchLimitToDirectories;
	const DWORD flags = FIND_FIRST_EX_LARGE_FETCH;
	WIN32_FIND_DATA findData = {};
	const HANDLE handle = FindFirstFileEx( filename.c_str(), levels, &findData, searchOp, nullptr /*filter*/, flags );
	if ( INVALID_HANDLE_VALUE != handle ) {
		BOOL found = TRUE;
		while ( found ) {
			if ( ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && !( findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) && !( findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ) && ( findData.cFileName[ 0 ] != '.' ) ) {
				subfolders.insert( findData.cFileName );
			}
			found = FindNextFile( handle, &findData );
		}
		FindClose( handle );
	}
	return subfolders;
}

void WndTree::GetFolderPath( const HTREEITEM item, std::wstring& path ) const
{
	const HTREEITEM parent = TreeView_GetParent( m_hWnd, item );
	if ( nullptr != parent ) {
		if ( m_NodeComputer == parent ) {
			const auto rootFolder = m_RootComputerFolders.find( item );
			if ( m_RootComputerFolders.end() != rootFolder ) {
				path = path.empty() ? rootFolder->second.Path : ( rootFolder->second.Path + L"\\" + path );
			}
		} else {
			const std::wstring folderName = GetItemLabel( item );
			path = path.empty() ? folderName : ( folderName + L"\\" + path );
			GetFolderPath( parent, path );
		}
	}
}

void WndTree::AddSubFolders( const HTREEITEM item )
{
	if ( nullptr == TreeView_GetChild( m_hWnd, item ) ) {
		std::wstring folderPath;
		GetFolderPath( item, folderPath );
		if ( !folderPath.empty() ) {
			const std::set<std::wstring> subFolders = GetSubFolders( folderPath );
			for ( const auto& subFolder : subFolders ) {
				AddItem( item, subFolder, Playlist::Type::Folder, false /*redraw*/ );
			}
		}
	}
}

void WndTree::OnItemExpanding( const HTREEITEM item )
{
	if ( Playlist::Type::Folder == GetItemType( item ) ) {
		HTREEITEM childItem = TreeView_GetChild( m_hWnd, item );
		while ( nullptr != childItem ) {
			AddSubFolders( childItem );
			childItem = TreeView_GetNextSibling( m_hWnd, childItem );
		}
	}
}

void WndTree::AddFolderTracks( const HTREEITEM item, Playlist::Ptr playlist )
{
	if ( playlist && ( Playlist::Type::Folder == playlist->GetType() ) && ( nullptr != item ) ) {
		std::set<std::wstring> fileNames;

		std::wstring folderPath;
		GetFolderPath( item, folderPath );
		if ( !folderPath.empty() ) {
			if ( !folderPath.empty() && ( folderPath.back() != '\\' ) && ( folderPath.back() != '/' ) ) {
				folderPath += '\\';
			}
			const std::wstring findName = folderPath + L"*";
			const FINDEX_INFO_LEVELS levels = FindExInfoBasic;
			const FINDEX_SEARCH_OPS searchOp = FindExSearchNameMatch;
			const DWORD flags = FIND_FIRST_EX_LARGE_FETCH;
			WIN32_FIND_DATA findData = {};
			const HANDLE handle = FindFirstFileEx( findName.c_str(), levels, &findData, searchOp, nullptr /*filter*/, flags );
			if ( INVALID_HANDLE_VALUE != handle ) {
				BOOL found = TRUE;
				while ( found ) {
					if ( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && !( findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) && !( findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ) ) {
						const std::wstring fileName = folderPath + findData.cFileName;
						IShellItem* shellItem = nullptr;
						if ( SUCCEEDED( SHCreateItemFromParsingName( fileName.c_str(), nullptr /*bindContext*/, IID_PPV_ARGS( &shellItem ) ) ) ) {
							SFGAOF attributes = 0;
							if ( SUCCEEDED( shellItem->GetAttributes( SFGAO_FOLDER, &attributes ) ) && !( attributes & SFGAO_FOLDER ) ) {
								fileNames.insert( fileName );
							}
							shellItem->Release();
						}
					}
					found = FindNextFile( handle, &findData );
				}
				FindClose( handle );
			}
		}

		if ( const auto previouslyAdded = m_AddedFolderTracks.find( item ); m_AddedFolderTracks.end() != previouslyAdded ) {
			std::vector<std::wstring> newFiles;
			std::set_difference( fileNames.begin(), fileNames.end(), previouslyAdded->second.begin(), previouslyAdded->second.end(), std::back_inserter( newFiles ) );
			if ( newFiles.empty() ) {
				fileNames.clear();
			} else {
				m_AddedFolderTracks[ item ] = fileNames;
				fileNames = { newFiles.begin(), newFiles.end() };
			}
		} else {
			m_AddedFolderTracks[ item ] = fileNames;
		}

		for ( const auto& fileName : fileNames ) {
			playlist->AddPending( fileName );
		}
	}
}

void WndTree::OnDriveArrived( const wchar_t /*drive*/ )
{
	RefreshComputerNode();
}

void WndTree::OnDriveRemoved( const wchar_t drive )
{
	std::list<HTREEITEM> itemsToDelete;
	const auto foundItem = std::find_if( m_RootComputerFolders.begin(), m_RootComputerFolders.end(), [ drive ] ( const RootFolderInfoMap::value_type& folderInfo )
	{
		const bool found = ( RootFolderType::Drive == folderInfo.second.Type ) && ( 0 == folderInfo.second.Path.find( drive ) );
		return found;
	} );

	if ( m_RootComputerFolders.end() != foundItem ) {
		RemoveItem( foundItem->first );
		m_FolderMonitor.RemoveFolder( foundItem->second.Path );
	}
}

void WndTree::OnDeviceHandleRemoved( const HANDLE handle )
{
	m_FolderMonitor.OnDeviceHandleRemoved( handle );
}

void WndTree::OnFolderMonitorCallback( const FolderMonitor::Event monitorEvent, const std::wstring& oldFilename, const std::wstring& newFilename )
{
	const std::wstring& folder = 
		( ( FolderMonitor::Event::FolderRenamed == monitorEvent ) || ( FolderMonitor::Event::FolderCreated == monitorEvent ) || ( FolderMonitor::Event::FolderDeleted == monitorEvent ) ) ?
		oldFilename :
		oldFilename.substr( 0 /*offset*/, oldFilename.find_last_of( L"/\\" ) );

	std::lock_guard<std::mutex> nodeLock( m_FolderNodesMapMutex );
	std::lock_guard<std::mutex> playlistLock( m_FolderPlaylistMapMutex );

	switch ( monitorEvent ) {
		case FolderMonitor::Event::FolderRenamed : {
			const size_t pos = newFilename.find_last_of( L"/\\" );
			if ( std::wstring::npos != pos ) {
				const auto folderIter = m_FolderNodesMap.find( folder );
				if ( m_FolderNodesMap.end() != folderIter ) {
					std::wstring* oldFolderPath = new std::wstring( oldFilename );
					std::wstring* newFolderPath = new std::wstring( newFilename );
					PostMessage( m_hWnd, MSG_FOLDERRENAME, reinterpret_cast<WPARAM>( oldFolderPath ), reinterpret_cast<LPARAM>( newFolderPath ) );					
				}
			}
			break;
		}
		case FolderMonitor::Event::FolderCreated : {
			const size_t pos = folder.find_last_of( L"/\\" );
			if ( std::wstring::npos != pos ) {
				const std::wstring parentFolder( folder.substr( 0 /*offset*/, pos ) );
				const auto folderIter = m_FolderNodesMap.find( parentFolder );
				if ( m_FolderNodesMap.end() != folderIter ) {
					for ( const auto& node : folderIter->second ) {
						std::wstring* folderName = new std::wstring( folder.substr( 1 + pos /*offset*/ ) );
						PostMessage( m_hWnd, MSG_FOLDERADD, reinterpret_cast<WPARAM>( node ), reinterpret_cast<LPARAM>( folderName ) );
					}
				}
			}
			break;
		}
		case FolderMonitor::Event::FolderDeleted : {
			const auto folderIter = m_FolderNodesMap.find( folder );
			if ( m_FolderNodesMap.end() != folderIter ) {
				for ( const auto& node : folderIter->second ) {
					PostMessage( m_hWnd, MSG_FOLDERDELETE, reinterpret_cast<WPARAM>( node ), 0 /*lParam*/ );
				}
			}
			break;
		}
		case FolderMonitor::Event::FileRenamed : {
			if ( !IgnoreFileMonitorEvent( oldFilename ) && !IgnoreFileMonitorEvent( newFilename ) ) {
				const auto folderIter = m_FolderNodesMap.find( folder );
				if ( m_FolderNodesMap.end() != folderIter ) {
					const std::set<HTREEITEM>& nodes = folderIter->second;
					for ( const auto& node : nodes ) {
						const auto playlistIter = m_FolderPlaylistMap.find( node );
						if ( m_FolderPlaylistMap.end() != playlistIter ) {
							Playlist::Ptr playlist = playlistIter->second;
							if ( playlist ) {
							  playlist->RemoveItem( MediaInfo( oldFilename ) );
                if ( playlist->ContainsFile( newFilename ) ) {
				          std::lock_guard<std::mutex> lock( m_FilesModifiedMutex );
				          m_FilesModified.insert( newFilename );
				          SetEvent( m_FileModifiedWakeEvent );
                } else {
							    playlist->AddPending( newFilename );
                }
							}
						}
					}
				}
			}
			break;
		}
		case FolderMonitor::Event::FileCreated : {
			if ( !IgnoreFileMonitorEvent( newFilename ) ) {
				const auto folderIter = m_FolderNodesMap.find( folder );
				if ( m_FolderNodesMap.end() != folderIter ) {
					const std::set<HTREEITEM>& nodes = folderIter->second;
					for ( const auto& node : nodes ) {
						const auto playlistIter = m_FolderPlaylistMap.find( node );
						if ( m_FolderPlaylistMap.end() != playlistIter ) {
							Playlist::Ptr playlist = playlistIter->second;
							if ( playlist ) {
                if ( playlist->ContainsFile( newFilename ) ) {
				          std::lock_guard<std::mutex> lock( m_FilesModifiedMutex );
				          m_FilesModified.insert( newFilename );
				          SetEvent( m_FileModifiedWakeEvent );
                } else {
								  playlist->AddPending( newFilename );
                }
							}
						}
					}
				}
			}
			break;
		}
		case FolderMonitor::Event::FileModified : {
			if ( !IgnoreFileMonitorEvent( newFilename ) ) {
				std::lock_guard<std::mutex> lock( m_FilesModifiedMutex );
				m_FilesModified.insert( newFilename );
				SetEvent( m_FileModifiedWakeEvent );
			}
			break;
		}
		case FolderMonitor::Event::FileDeleted : {
			if ( !IgnoreFileMonitorEvent( oldFilename ) ) {
				const auto folderIter = m_FolderNodesMap.find( folder );
				if ( m_FolderNodesMap.end() != folderIter ) {
					const std::set<HTREEITEM>& nodes = folderIter->second;
					for ( const auto& node : nodes ) {
						const auto playlistIter = m_FolderPlaylistMap.find( node );
						if ( m_FolderPlaylistMap.end() != playlistIter ) {
							Playlist::Ptr playlist = playlistIter->second;
							if ( playlist ) {
								playlist->RemoveItem( MediaInfo( oldFilename ) );
							}
						}
					}
				}
			}
			break;
		}
		default : {
			break;
		}
	}
}

void WndTree::OnFileModifiedHandler()
{
	const HANDLE eventHandles[ 2 ] = { m_FileModifiedStopEvent, m_FileModifiedWakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		std::wstring filename;
		{
			std::lock_guard<std::mutex> lock( m_FilesModifiedMutex );
			const auto iter = m_FilesModified.begin();
			if ( m_FilesModified.end() != iter ) {
				filename = *iter;
				m_FilesModified.erase( iter );
			}
		}
		if ( filename.empty() ) {
			ResetEvent( m_FileModifiedWakeEvent );
		} else {
			MediaInfo mediaInfo( filename );
			if ( m_Library.GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/ ) ) {
			  m_Library.GetMediaInfo( mediaInfo );
      }
		}
	}
}

void WndTree::GetAllChildren( const HTREEITEM item, std::set<HTREEITEM>& children ) const
{
	HTREEITEM childItem = TreeView_GetChild( m_hWnd, item );
	while ( nullptr != childItem ) {
		GetAllChildren( childItem, children );
		children.insert( childItem );
		childItem = TreeView_GetNextSibling( m_hWnd, childItem );
	}
}

HTREEITEM WndTree::OnFolderAdd( const HTREEITEM parent, const std::wstring& folder )
{
	AddSubFolders( parent );

	std::set<HTREEITEM> childItems;
	GetAllChildren( parent, childItems );
	HTREEITEM addedItem = nullptr;
	auto childItem = childItems.begin();
	while ( ( nullptr == addedItem ) && ( childItems.end() != childItem ) ) {
		if ( folder == GetItemLabel( *childItem ) ) {
			addedItem = *childItem;
		}
		++childItem;
	}

	if ( nullptr == addedItem ) {
		addedItem = AddItem( parent, folder, Playlist::Type::Folder );
	}

	if ( nullptr != addedItem ) {
		AddSubFolders( addedItem );
	}
	return addedItem;
}

void WndTree::OnFolderDelete( const HTREEITEM item )
{
	RemoveItem( item );
}

void WndTree::OnFolderRename( const std::wstring& oldFolderPath, const std::wstring& newFolderPath )
{
	// Determine whether to select the renamed folder.
	const HTREEITEM hSelectedItem = TreeView_GetSelection( m_hWnd );
	std::wstring selectedFolderPath;
	GetFolderPath( hSelectedItem, selectedFolderPath );
	bool selectRenamedFolder = ( !oldFolderPath.empty() && !selectedFolderPath.empty() && ( 0 == selectedFolderPath.find( oldFolderPath ) ) );

	// Delete the old folder nodes (if present).
	std::set<HTREEITEM> nodesToDelete;
	{
		std::lock_guard<std::mutex> lock( m_FolderNodesMapMutex );
		const auto oldFolderIter = m_FolderNodesMap.find( oldFolderPath );
		if ( m_FolderNodesMap.end() != oldFolderIter ) {
			nodesToDelete = oldFolderIter->second;
		}
	}
	for ( const auto& item : nodesToDelete ) {
		RemoveItem( item );
	}

	// Get the parent nodes of the new folder path.
	const size_t pos = newFolderPath.find_last_of( L"/\\" );
	if ( std::wstring::npos != pos ) {
		const std::wstring parentFolderPath = newFolderPath.substr( 0 /*offset*/, pos );
		const std::wstring newFolderName = newFolderPath.substr( 1 + pos );
		std::set<HTREEITEM> parentNodes;
		{
			std::lock_guard<std::mutex> lock( m_FolderNodesMapMutex );
			const auto parentFolderIter = m_FolderNodesMap.find( parentFolderPath );
			if ( m_FolderNodesMap.end() != parentFolderIter ) {
				parentNodes = parentFolderIter->second;
			}
		}
		for ( const auto& item : parentNodes ) {
			const HTREEITEM addedItem = AddItem( item, newFolderName, Playlist::Type::Folder );
			if ( nullptr != addedItem ) {
				AddSubFolders( addedItem );
			}
			if ( selectRenamedFolder ) {
				TreeView_SelectItem( m_hWnd, addedItem );
				TreeView_Expand( m_hWnd, addedItem, TVE_EXPAND );
				selectRenamedFolder = false;
			}
		}
	}
}

void WndTree::AddToFolderNodesMap( const HTREEITEM item )
{
	if ( Playlist::Type::Folder == GetItemType( item ) ) {
		std::wstring path;
		GetFolderPath( item, path );
		if ( !path.empty() ) {
			std::lock_guard<std::mutex> lock( m_FolderNodesMapMutex );
			auto iter = m_FolderNodesMap.insert( StringToNodesMap::value_type( path, std::set<HTREEITEM>() ) ).first;
			if ( m_FolderNodesMap.end() != iter ) {
				iter->second.insert( item );
			}
		}
	}
}

void WndTree::RemoveFromFolderNodesMap( const HTREEITEM item )
{
	if ( Playlist::Type::Folder == GetItemType( item ) ) {
		std::wstring path;
		GetFolderPath( item, path );
		std::lock_guard<std::mutex> lock( m_FolderNodesMapMutex );
		auto iter = m_FolderNodesMap.find( path );
		if ( m_FolderNodesMap.end() != iter ) {
			std::set<HTREEITEM>& nodes = iter->second;
			nodes.erase( item );
			if ( nodes.empty() ) {
				m_FolderNodesMap.erase( path );
			}
		}
	}
}

HTREEITEM WndTree::GetComputerFolderNode( const std::wstring& folder )
{
	HTREEITEM foundItem = nullptr;
	const DWORD attributes = GetFileAttributes( folder.c_str() );
	if ( ( INVALID_FILE_ATTRIBUTES != attributes ) && ( FILE_ATTRIBUTE_DIRECTORY & attributes ) ) {

		auto foundRoot = m_RootComputerFolders.end();
		HTREEITEM folderItem = TreeView_GetChild( m_hWnd, m_NodeComputer );
		while ( nullptr != folderItem ) {
			const auto folderInfo = m_RootComputerFolders.find( folderItem );
			if ( ( m_RootComputerFolders.end() != folderInfo ) && ( 0 == folder.find( folderInfo->second.Path ) ) ) {
				foundRoot = folderInfo;
				break;
			}
			folderItem = TreeView_GetNextSibling( m_hWnd, folderItem );
		}

		if ( m_RootComputerFolders.end() != foundRoot ) {
			HTREEITEM item = foundRoot->first;
			const auto& info = foundRoot->second;
			if ( info.Path == folder ) {
				TreeView_SelectItem( m_hWnd, item );
				foundItem = item;
			} else {
				AddSubFolders( item );

				std::wstring path( folder );
				size_t pos = 1 + info.Path.size();
				if ( pos < path.size() ) {
					path = path.substr( pos );
				}

				while ( !path.empty() ) {
					pos = path.find( '\\' );
					const std::wstring& folderName = ( std::wstring::npos == pos ) ? path : path.substr( 0 /*offset*/, pos );

					HTREEITEM childItem = TreeView_GetChild( m_hWnd, item );
					HTREEITEM foundChild = nullptr;
					while ( nullptr != childItem ) {
						AddSubFolders( childItem );
						const std::wstring itemLabel = GetItemLabel( childItem );
						if ( itemLabel == folderName ) {
							foundChild = childItem;
						}
						childItem = TreeView_GetNextSibling( m_hWnd, childItem );
					}

					if ( nullptr != foundChild ) {
						item = foundChild;
						if ( std::wstring::npos == pos ) {
							TreeView_SelectItem( m_hWnd, foundChild );
							path.clear();
							foundItem = foundChild;
						} else {
							path = path.substr( 1 + pos );
						}
					} else {
						path.clear();
					}
				}
			}
		}
	}
	return foundItem;
}

Playlist::Ptr WndTree::SetScratchList( const MediaInfo::List& mediaList )
{
	StopScratchListUpdateThread();

	Playlist::Ptr scratchList( new Playlist( m_Library, s_ScratchListID, Playlist::Type::User ) );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_SCRATCHLIST, buffer, bufSize );
	scratchList->SetName( buffer );
	for ( const auto& mediaInfo : mediaList ) {
		scratchList->AddItem( mediaInfo );
	}

	bool foundScratchList = false;
	for ( auto& playlistIter : m_PlaylistMap ) {
		if ( playlistIter.second && ( s_ScratchListID == playlistIter.second->GetID() ) ) {
			foundScratchList = true;
			scratchList->SetName( playlistIter.second->GetName() );
			playlistIter.second = scratchList;

			const HTREEITEM currentSelection = TreeView_GetSelection( m_hWnd );
			if ( currentSelection == playlistIter.first ) {
				// Reselect the tree item to ensure a tree selection change notification gets sent.
				TreeView_SelectItem( m_hWnd, m_NodePlaylists );
			}
			TreeView_SelectItem( m_hWnd, playlistIter.first );
			break;
		}
	}

	if ( !foundScratchList ) {
		AddPlaylist( scratchList );
	}

	StartScratchListUpdateThread( scratchList );

	return scratchList;
}

void WndTree::StartScratchListUpdateThread( Playlist::Ptr scratchList )
{
	StopScratchListUpdateThread();
	if ( nullptr != m_ScratchListUpdateStopEvent ) {
		MediaInfo::List mediaList;
		const Playlist::Items items = scratchList->GetItems();
		for ( const auto& item : items ) {
			mediaList.push_back( item.Info );
		}
		ScratchListUpdateInfo* info = new ScratchListUpdateInfo( m_Library, m_ScratchListUpdateStopEvent, mediaList );
		ResetEvent( info->StopEvent );
		m_ScratchListUpdateThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, ScratchListUpdateProc, info /*param*/, 0 /*flags*/, NULL /*threadId*/ );
	}
}

void WndTree::StopScratchListUpdateThread()
{
	if ( ( nullptr != m_ScratchListUpdateStopEvent ) && ( nullptr != m_ScratchListUpdateThread ) ) {
		SetEvent( m_ScratchListUpdateStopEvent );
		WaitForSingleObject( m_ScratchListUpdateThread, INFINITE );
		CloseHandle( m_ScratchListUpdateThread );
		m_ScratchListUpdateThread = nullptr;
	}
}

void WndTree::StartFileModifiedThread()
{
	StopFileModifiedThread();
	if ( ( nullptr != m_FileModifiedStopEvent ) && ( nullptr != m_FileModifiedWakeEvent ) ) {
		ResetEvent( m_FileModifiedStopEvent );
		m_FileModifiedThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, FileModifiedProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
	}
}

void WndTree::StopFileModifiedThread()
{
	if ( nullptr != m_FileModifiedThread ) {
		SetEvent( m_FileModifiedStopEvent );
		WaitForSingleObject( m_FileModifiedThread, INFINITE );
		CloseHandle( m_FileModifiedThread );
	}
}

void WndTree::SetMergeDuplicates( const bool mergeDuplicates )
{
	if ( mergeDuplicates != m_MergeDuplicates ) {
		m_MergeDuplicates = mergeDuplicates;
		std::set<Playlist::Ptr> playlists;
		for ( const auto& iter : m_ArtistMap ) {
			playlists.insert( iter.second );
		}
		for ( const auto& iter : m_AlbumMap ) {
			playlists.insert( iter.second );
		}
		for ( const auto& iter : m_GenreMap ) {
			playlists.insert( iter.second );
		}
		for ( const auto& iter : m_YearMap ) {
			playlists.insert( iter.second );
		}
		
		for ( const auto& playlist : playlists ) {
			if ( playlist ) {
				playlist->SetMergeDuplicates( m_MergeDuplicates );
			}
		}
	}
}

bool WndTree::IgnoreFileMonitorEvent( const std::wstring& filename ) const
{
	bool ignore = true;
  if ( !m_Library.HasRecentlyWrittenTag( filename ) ) {
	  const std::filesystem::path filepath( filename );
	  const std::wstring folder = filepath.parent_path();

	  // Note that the folder playlist map mutex has already been locked by the callback handler.
	  for ( const auto& [ treeItem, playlist ] : m_FolderPlaylistMap ) {
		  if ( playlist && ( folder == playlist->GetName() ) ) {
			  ignore = false;
			  break;
		  }
	  }
  }
  return ignore;
}

Playlist::Ptr WndTree::SelectAudioCD( const wchar_t drive )
{
	Playlist::Ptr cdPlaylist;
	const std::wstring drivename = WideStringToLower( std::wstring( 1, drive ) );
	if ( !drivename.empty() ) {
		for ( const auto& item : m_CDDAMap ) {
			const Playlist::Ptr playlist = item.second;
			if ( playlist ) {
				const auto tracks = playlist->GetItems();
				if ( !tracks.empty() ) {
					const std::wstring filename = WideStringToLower( tracks.front().Info.GetFilename() );
					if ( !filename.empty() && ( filename.front() == drivename.front() ) ) {
						TreeView_SelectItem( m_hWnd, item.first );
						cdPlaylist = playlist;
						break;
					}
				}
			}
		}
	}
	return cdPlaylist;
}

WNDPROC WndTree::GetEditControlWndProc()
{
	return m_EditControlWndProc;
}

int WndTree::GetIconSize() const
{
	int iconSize = static_cast<int>( 16 * GetDPIScaling() );
	constexpr int iconModulus = 4;
	if ( HDC dc = GetDC( m_hWnd ); nullptr != dc ) {
		const Gdiplus::Font font( dc, m_ChosenFont );
		const Gdiplus::Graphics graphics( dc );
		const Gdiplus::PointF origin;
		Gdiplus::RectF bounds;
		if ( Gdiplus::Ok == graphics.MeasureString( L"Ay96", -1, &font, origin, &bounds ) ) {
			iconSize = std::max<int>( 16, iconModulus * ( static_cast<int>( bounds.Height ) / iconModulus ) );
		}
		ReleaseDC( m_hWnd, dc );
	}
	return iconSize;
}

void WndTree::SetColours()
{
	if ( m_IsHighContrast ) {
		TreeView_SetTextColor( m_hWnd, GetSysColor( COLOR_WINDOWTEXT ) );
		TreeView_SetBkColor( m_hWnd, GetSysColor( COLOR_WINDOW ) );	
	} else {
		TreeView_SetTextColor( m_hWnd, m_ColourFont );
		TreeView_SetBkColor( m_hWnd, m_ColourBackground );
	}
}

void WndTree::OnSysColorChange( const bool isHighContrast )
{
	m_IsHighContrast = isHighContrast;
	CreateImageList();
	SetColours();
}

void WndTree::UpdateOutputIcon()
{
	UpdateIcon( m_NodeCurrentOutput, m_Output.GetState() );
}

void WndTree::OnOutputPlaylistChange( const Playlist::Ptr playlist )
{
	HTREEITEM outputNode = nullptr;
	if ( playlist ) {
		switch ( playlist->GetType() ) {
			case Playlist::Type::User : {
				outputNode = FindPlaylist( m_PlaylistMap, playlist );
				break;
			}
			case Playlist::Type::All : {
				outputNode = m_NodeAll;
				break;
			}
			case Playlist::Type::Artist : {
				outputNode = FindPlaylist( m_ArtistMap, playlist );
				break;
			}
			case Playlist::Type::Album : {
				outputNode = FindPlaylist( m_AlbumMap, playlist );
				break;
			}
			case Playlist::Type::Genre : {
				outputNode = FindPlaylist( m_GenreMap, playlist );
				break;
			}
			case Playlist::Type::Year : {
				outputNode = FindPlaylist( m_YearMap, playlist );
				break;
			}
			case Playlist::Type::Favourites : {
				outputNode = m_NodeFavourites;
				break;
			}
			case Playlist::Type::CDDA : {
				outputNode = FindPlaylist( m_CDDAMap, playlist );
				break;
			}
			case Playlist::Type::Folder : {
				std::lock_guard<std::mutex> lock( m_FolderPlaylistMapMutex );
				outputNode = FindPlaylist( m_FolderPlaylistMap, playlist );
				break;
			}
			case Playlist::Type::Streams : {
				outputNode = m_NodeStreams;
				break;
			}
		}
	}

	if ( outputNode != m_NodeCurrentOutput ) {
		UpdateIcon( m_NodeCurrentOutput, Output::State::Stopped );
		m_NodeCurrentOutput = outputNode;
		UpdateIcon( m_NodeCurrentOutput, m_Output.GetState() );
	}
}

void WndTree::UpdateIcon( const HTREEITEM item, const Output::State state )
{
	if ( nullptr != item ) {
		int iconIndex = GetOriginalIconIndex( item );
		if ( Output::State::Playing == state ) {
			iconIndex = m_IconIndexPlaying;
		} else if ( Output::State::Paused == state ) {
			iconIndex = m_IconIndexPaused;
		}
		TVITEMEX tvItem = {};
		tvItem.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvItem.hItem = item;
		tvItem.iImage = iconIndex;
		tvItem.iSelectedImage = tvItem.iImage;
		TreeView_SetItem( m_hWnd, &tvItem );
	}
}

HTREEITEM WndTree::FindPlaylist( const PlaylistMap& playlistMap, const Playlist::Ptr& playlist )
{
	const auto item = std::find_if( playlistMap.begin(), playlistMap.end(), [ playlist ] ( const PlaylistMap::value_type& entry ) {
		return entry.second == playlist;
	} );
	return ( playlistMap.end() == item ) ? nullptr : item->first;
}

LPARAM WndTree::CreateItemData( const Playlist::Type playlistType, const int originalIconIndex, const int itemOrder )
{
	return MAKELPARAM( WORD( playlistType ), MAKEWORD( BYTE( originalIconIndex ), BYTE( itemOrder ) ) );
}

std::tuple<Playlist::Type, int, int> WndTree::GetItemData( const HTREEITEM item ) const
{
	auto [ type, originalIconIndex, itemOrder ] = std::make_tuple( Playlist::Type::_Undefined, 0, 0 );

	TVITEMEX tvItem = {};
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = item;
	TreeView_GetItem( m_hWnd, &tvItem );
	const WORD loWord = LOWORD( tvItem.lParam );
	if ( ( loWord < WORD( Playlist::Type::_Undefined ) ) && ( loWord >= WORD( Playlist::Type::User ) ) ) {
		type = static_cast<Playlist::Type>( loWord );
	}

	const WORD hiWord = HIWORD( tvItem.lParam );
	originalIconIndex = int( LOBYTE( hiWord ) );
	itemOrder = int( HIBYTE( hiWord ) );

	return { type, originalIconIndex, itemOrder };
}

void WndTree::OnCutCopy( const bool cut )
{
	if ( const HWND editControl = TreeView_GetEditControl( m_hWnd ); nullptr != editControl ) {
		if ( const int textLength = Edit_GetTextLength( editControl ); textLength > 0 ) {
			std::vector<WCHAR> text( 1 + textLength, 0 );
			if ( Edit_GetText( editControl, text.data(), 1 + textLength ) >= textLength ) {
				DWORD startSel = 0;
				DWORD endSel = 0;
				if ( ( SendMessage( editControl, EM_GETSEL, WPARAM( &startSel ), LPARAM ( &endSel ) ) > 0 ) && ( endSel > startSel ) && ( startSel < text.size() ) && ( endSel < text.size() ) ) {
					const std::wstring clipboardText = std::wstring( text.begin() + startSel, text.begin() + endSel );
					if ( OpenClipboard( m_hWnd ) ) {
						if ( const HGLOBAL mem = GlobalAlloc( GMEM_MOVEABLE, ( clipboardText.size() + 1 ) * sizeof( WCHAR ) ); nullptr != mem ) {
							if ( LPWSTR destString = static_cast<LPWSTR>( GlobalLock( mem ) ); nullptr != destString ) {
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
						Edit_ReplaceSel( editControl, L"" );
					}
				}
			}
		}
	}
}

void WndTree::OnPaste()
{
	if ( const HWND editControl = TreeView_GetEditControl( m_hWnd ); nullptr != editControl ) {
		if ( OpenClipboard( m_hWnd ) ) {
			bool clipboardHasText = false;
			std::wstring clipboardText;
			HANDLE handle = GetClipboardData( CF_UNICODETEXT );
			if ( nullptr != handle ) {
				LPCWSTR str = static_cast<LPCWSTR>( GlobalLock( handle ) );
				if ( nullptr != str ) {
					clipboardText = str;
					GlobalUnlock( handle );
					clipboardHasText = true;
				}	
			} else {
				handle = GetClipboardData( CF_TEXT );
				if ( nullptr != handle ) {
					LPCSTR str = static_cast<LPCSTR>( GlobalLock( handle ) );
					if ( nullptr != str ) {
						clipboardText = AnsiCodePageToWideString( str );
						GlobalUnlock( handle );
						clipboardHasText = true;
					}	
				}
			}
			CloseClipboard();
			if ( clipboardHasText ) {
				Edit_ReplaceSel( editControl, clipboardText.c_str() );
			}
		}
	}
}

void WndTree::OnSelectAll()
{
	if ( const HWND editControl = TreeView_GetEditControl( m_hWnd ); nullptr != editControl ) {
		Edit_SetSel( editControl, 0, -1 );
	}
}

void WndTree::InsertAddToPlaylists( const HMENU menu, const UINT insertAfterItemID, const bool insertSeparator )
{
	m_AddToPlaylistMenuMap.clear();
	UINT commandID = MSG_TREEMENU_ADDTOPLAYLIST_START;
	const int itemCount = GetMenuItemCount( menu );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		if ( insertAfterItemID == GetMenuItemID( menu, itemIndex ) ) {
			++itemIndex;
			if ( HMENU playlistMenu = CreatePopupMenu(); nullptr != playlistMenu ) {
				const Playlist::Ptr currentPlaylist = GetSelectedPlaylist();
				bool enableMenu = currentPlaylist && ( Playlist::Type::CDDA != currentPlaylist->GetType() );
				if ( enableMenu ) {
					if ( const HTREEITEM parentItem = TreeView_GetParent( m_hWnd, TreeView_GetSelection( m_hWnd ) ); nullptr == parentItem ) {
						const auto playlistType = currentPlaylist->GetType();
						switch ( playlistType ) {
							case Playlist::Type::All :
							case Playlist::Type::Favourites :
							case Playlist::Type::Streams : {
								enableMenu = true;
								break;
							}
							default: {
								enableMenu = false;
								break;
							}
						}
					}
				}

				if ( enableMenu ) {
					const Playlists playlists = GetUserPlaylists();
					for ( const auto& playlist : playlists ) {
						if ( playlist ) {
							m_AddToPlaylistMenuMap.insert( PlaylistMenuMap::value_type( commandID, playlist ) );
							AppendMenu( playlistMenu, ( playlist == currentPlaylist ) ? MF_DISABLED : MF_ENABLED, commandID, playlist->GetName().c_str() );
							if ( ++commandID > MSG_TREEMENU_ADDTOPLAYLIST_END ) {
								break;
							}
						}		
					}
				}
				if ( insertSeparator ) {
					InsertMenu( menu, itemIndex++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr );
				}
				const int buffersize = 32;
				WCHAR buffer[ buffersize ] = {};
				LoadString( m_hInst, IDS_ADDTOPLAYLIST, buffer, buffersize );
				InsertMenu( menu, itemIndex, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>( playlistMenu ), buffer );
				EnableMenuItem( menu, itemIndex, MF_BYPOSITION | ( enableMenu ? MF_ENABLED : MF_DISABLED ) );
			}
			break;
		}
	}
}

void WndTree::OnAddToPlaylist( const UINT command )
{
	if ( const auto playlistIter = m_AddToPlaylistMenuMap.find( command ); m_AddToPlaylistMenuMap.end() != playlistIter ) {
		Playlist::Ptr sourcePlaylist = GetSelectedPlaylist();
		Playlist::Ptr targetPlaylist = playlistIter->second;
		if ( sourcePlaylist && targetPlaylist && ( sourcePlaylist != targetPlaylist ) ) {
			const auto sourcePlaylistType = sourcePlaylist->GetType();
			switch ( sourcePlaylistType ) {
				case Playlist::Type::User :
				case Playlist::Type::All :
				case Playlist::Type::Album :
				case Playlist::Type::Genre :
				case Playlist::Type::Year :
				case Playlist::Type::Favourites :
				case Playlist::Type::Streams : {
					const auto items = sourcePlaylist->GetItems();
					for ( const auto& item : items ) {
						targetPlaylist->AddPending( item.Info );
					}
					break;
				}
				case Playlist::Type::Artist : {
					const auto& artist = sourcePlaylist->GetName();
					const auto items = m_Library.GetMediaByArtist( artist );
					for ( const auto& item : items ) {
						targetPlaylist->AddPending( item );
					}
					break;
				}
				case Playlist::Type::Folder : {
					for ( const auto& entry : std::filesystem::recursive_directory_iterator( sourcePlaylist->GetName() ) ) {
						std::error_code errorCode = {};
						if ( entry.is_regular_file( errorCode ) && !entry.is_directory( errorCode ) ) {
							targetPlaylist->AddPending( MediaInfo( entry.path() ) );
						}
					}
					break;
				}
				default : {
					break;
				}
			}
			SelectPlaylist( targetPlaylist );
		}
	}
}

void WndTree::RefreshUserPlaylistLabel( const Playlist::Ptr playlist )
{
  if ( !playlist )
    return;

  for ( const auto& [ itemNode, itemPlaylist ] : m_PlaylistMap ) {
    if ( itemPlaylist == playlist ) {
      SetItemLabel( itemNode, playlist->GetName() );
      TreeView_SortChildren( m_hWnd, m_NodePlaylists, FALSE /*recurse*/ );
      break;
    }
  }
}
