#include "WndTray.h"

#include "resource.h"

#include "Utility.h"

#include "WndList.h"
#include "WndToolbar.h"
#include "WndTree.h"

// Maximum length of notification area tooltip.
static const int sMaxTooltip = 127;

// Maximum number of playlists to show on the context menu.
static const int sMaxPlaylists = 30;

// Maximum number of columns to show on a playlist menu.
static const int sMaxPlaylistColumns = 4;

// Maximum string length of an entry on the playlist menu.
static const size_t sMaxPlaylistItemLength = 64;

// Maps a volume menu ID to a volume level.
std::map<UINT,float> WndTray::s_VolumeMenuMap = {
	{ ID_VOLUME_100, 1.0f },
	{ ID_VOLUME_90, 0.9f },
	{ ID_VOLUME_80, 0.8f },
	{ ID_VOLUME_70, 0.7f },
	{ ID_VOLUME_60, 0.6f },
	{ ID_VOLUME_50, 0.5f },
	{ ID_VOLUME_40, 0.4f },
	{ ID_VOLUME_30, 0.3f },
	{ ID_VOLUME_20, 0.2f },
	{ ID_VOLUME_10, 0.1f },
	{ ID_VOLUME_0, 0.0f }
};

WndTray::WndTray( HINSTANCE instance, HWND parent, Library& library, Settings& settings, Output& output, WndTree& wndTree, WndList& wndList ) :
	m_hInst( instance ),
	m_Library( library ),
	m_Settings( settings ),
	m_Output( output ),
	m_NotifyIconData( {} ),
	m_IsShown( false ),
	m_DefaultTooltip(),
	m_Tree( wndTree ),
	m_List( wndList ),
	m_PlaylistMenuItems(),
	m_NextPlaylistMenuItemID( 0 )
{
	m_NotifyIconData.cbSize = sizeof( NOTIFYICONDATA );
	m_NotifyIconData.hWnd = parent;
	m_NotifyIconData.uCallbackMessage = MSG_TRAYNOTIFY;
	m_NotifyIconData.uVersion = NOTIFYICON_VERSION_4;
	m_NotifyIconData.guidItem = GenerateGUID();
	LoadIconMetric( instance, MAKEINTRESOURCE( IDI_VUPLAYER ), LIM_SMALL, &m_NotifyIconData.hIcon );

	LoadString( instance, IDS_APP_TITLE, m_NotifyIconData.szTip, sMaxTooltip );
	m_DefaultTooltip = m_NotifyIconData.szTip;
}

WndTray::~WndTray()
{
	Hide();
	DestroyIcon( m_NotifyIconData.hIcon );
}

float WndTray::GetMenuVolume( const UINT volumeMenuID )
{
	const auto iter = s_VolumeMenuMap.find( volumeMenuID );
	const float volume = ( s_VolumeMenuMap.end() != iter ) ? iter->second : 1.0f;
	return volume;
}

bool WndTray::IsShown() const
{
	return m_IsShown;
}

void WndTray::Show()
{
	if ( !m_IsShown ) {
		m_NotifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
		Shell_NotifyIcon( NIM_ADD, &m_NotifyIconData );
		Shell_NotifyIcon( NIM_SETVERSION, &m_NotifyIconData );
		m_IsShown = true;
	}
}

void WndTray::Hide()
{
	if ( m_IsShown ) {
		m_NotifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
		Shell_NotifyIcon( NIM_DELETE, &m_NotifyIconData );
		m_IsShown = false;
	}
}

void WndTray::Update( const Output::Item& item )
{
	if ( m_IsShown ) {
		bool modified = false;
		if ( item.PlaylistItem.ID > 0 ) {
			const std::wstring position = DurationToString( m_hInst, item.Position, true /*colonDelimited*/ );
			const std::wstring duration = DurationToString( m_hInst, item.PlaylistItem.Info.GetDuration(), true /*colonDelimited*/ );
			const std::wstring counter = position + L"/" + duration;
			std::wstring artist = item.PlaylistItem.Info.GetArtist();
			WideStringReplace( artist, L"&", L"&&&" );
			std::wstring title = item.PlaylistItem.Info.GetTitle( true /*filenameAsTitle*/ );
			WideStringReplace( title, L"&", L"&&&" );
			std::wstring tooltip;
			if ( artist.empty() ) {
				tooltip = counter + L"\r\n" + title;
				if ( tooltip.size() > sMaxTooltip ) {
					tooltip = tooltip.substr( 0, sMaxTooltip - 3 ) + L"...";
				}
			} else {
				tooltip = counter + L"\r\n" + artist + L"\r\n" + title;
				if ( tooltip.size() > sMaxTooltip ) {
					tooltip = counter + L"\r\n" + title;
					if ( tooltip.size() > sMaxTooltip ) {
						tooltip = tooltip.substr( 0, sMaxTooltip - 3 ) + L"...";
					}
				}
			}
			if ( tooltip != m_NotifyIconData.szTip ) {
				wcscpy_s( m_NotifyIconData.szTip, 1 + tooltip.size(), tooltip.c_str() );
				modified = true;
			}
		} else {
			if ( m_DefaultTooltip != m_NotifyIconData.szTip ) {
				wcscpy_s( m_NotifyIconData.szTip, 1 + m_DefaultTooltip.size(), m_DefaultTooltip.c_str() );
				modified = true;
			}
		}
		if ( modified ) {
			m_NotifyIconData.uFlags = NIF_TIP | NIF_GUID | NIF_SHOWTIP;
			Shell_NotifyIcon( NIM_MODIFY, &m_NotifyIconData );
		}
	}
}

void WndTray::OnNotify( WPARAM /*wParam*/, LPARAM lParam )
{
	const WORD message = LOWORD( lParam );
	switch ( message ) {
		case WM_LBUTTONDOWN : {
			const UINT elapse = GetDoubleClickTime();
			SetTimer( m_NotifyIconData.hWnd, TIMER_SYSTRAY, elapse, NULL /*timerProc*/ );
			break;
		}
		case WM_LBUTTONDBLCLK : {
			KillTimer( m_NotifyIconData.hWnd, TIMER_SYSTRAY );
			OnDoubleClick();
			break;
		}
		case WM_CONTEXTMENU : {
			ShowContextMenu();
			break;
		}
		default : {
			break;
		}
	}
}

void WndTray::OnContextMenuCommand( const UINT command )
{
	const auto menuItemIter = m_PlaylistMenuItems.find( command );
	if ( m_PlaylistMenuItems.end() != menuItemIter ) {
		Playlist::Item playlistItem = { menuItemIter->second, MediaInfo() };
		Playlists playlists = m_Tree.GetPlaylists();
		playlists.push_back( m_Tree.GetPlaylistFavourites() );
		for ( const auto& playlist : playlists ) {
			if ( playlist && playlist->GetItem( playlistItem ) ) {
				m_Output.SetPlaylist( playlist );
				m_Output.Play( playlistItem.ID );
				m_Tree.SelectPlaylist( playlist );
				m_List.EnsureVisible( playlistItem );
				break;
			}
		}
	}
	m_PlaylistMenuItems.clear();
}

void WndTray::ShowContextMenu()
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_TRAY ) );
	if ( NULL != menu ) {
		HMENU traymenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != traymenu ) {
			const int buffersize = 32;
			WCHAR buffer[ buffersize ] = {};
			const Output::State state = m_Output.GetState();
			LoadString( m_hInst, ( Output::State::Playing == state ) ? IDS_CONTROL_PAUSE : IDS_CONTROL_PLAY, buffer, buffersize );
			ModifyMenu( menu, ID_TRAY_PLAY, MF_BYCOMMAND | MF_STRING, ID_TRAY_PLAY, buffer );

			LoadString( m_hInst, IsIconic( m_NotifyIconData.hWnd ) ? IDS_TRAY_SHOW : IDS_TRAY_HIDE, buffer, buffersize );
			ModifyMenu( menu, ID_TRAYMENU_SHOWVUPLAYER, MF_BYCOMMAND | MF_STRING, ID_TRAYMENU_SHOWVUPLAYER, buffer );

			m_PlaylistMenuItems.clear();
			m_NextPlaylistMenuItemID = MSG_TRAYMENUSTART;

			// Favourites.
			const Playlist::Ptr favourites = m_Tree.GetPlaylistFavourites();
			if ( favourites ) {
				const int itemCount = GetMenuItemCount( traymenu );
				for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
					HMENU playlistsMenu = GetSubMenu( traymenu, itemIndex );
					if ( ( nullptr != playlistsMenu ) && ( ID_TRAYMENU_PLAYLISTS == GetMenuItemID( playlistsMenu, 0 /*itemIndex*/ ) ) ) {
						++itemIndex;
						LoadString( m_hInst, IDS_FAVOURITES, buffer, buffersize );
						HMENU favouritesMenu = CreatePlaylistMenu( favourites );
						const bool disableFavourites = ( nullptr == favouritesMenu );
						if ( nullptr == favouritesMenu ) {
							favouritesMenu = CreatePopupMenu();
						}
						if ( nullptr != favouritesMenu ) {
							InsertMenu( traymenu, itemIndex, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>( favouritesMenu ), buffer );
						}
						if ( disableFavourites ) {
							EnableMenuItem( traymenu, itemIndex, MF_BYPOSITION | MF_DISABLED );
						}
						break;
					}
				}
			}

			// Playlists.
			const int itemCount = GetMenuItemCount( traymenu );
			for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				HMENU playlistsMenu = GetSubMenu( traymenu, itemIndex );
				if ( ( nullptr != playlistsMenu ) && ( ID_TRAYMENU_PLAYLISTS == GetMenuItemID( playlistsMenu, 0 /*itemIndex*/ ) ) ) {
					const Playlists playlists = m_Tree.GetPlaylists();
					if ( playlists.empty() ) {
						EnableMenuItem( traymenu, itemIndex, MF_BYPOSITION | MF_DISABLED );
					} else {
						int playlistMenuCount = 0;
						for ( auto playlistIter = playlists.begin(); ( playlistMenuCount < sMaxPlaylists ) && ( playlistIter != playlists.end() ); playlistIter++ ) {
							const Playlist::Ptr playlist = *playlistIter;
							HMENU playlistMenu = CreatePlaylistMenu( playlist );
							if ( nullptr != playlistMenu ) {
								InsertMenu( playlistsMenu, ++playlistMenuCount, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>( playlistMenu ), playlist->GetName().c_str() );
							}
						}
						DeleteMenu( playlistsMenu, 0 /*itemIndex*/, MF_BYPOSITION );
					}
					break;
				}
			}

			// Volume.
			const float volume = m_Output.GetVolume();
			for ( const auto& iter : s_VolumeMenuMap ) {
				const UINT volumeID = iter.first;
				const UINT checked = ( volume == iter.second ) ? MF_CHECKED : MF_UNCHECKED;
				CheckMenuItem( menu, volumeID, MF_BYCOMMAND | checked );
			}

			NOTIFYICONIDENTIFIER notifyIconID = {};
			notifyIconID.cbSize = sizeof( NOTIFYICONIDENTIFIER );
			notifyIconID.guidItem = m_NotifyIconData.guidItem;
			RECT rect = {};
			Shell_NotifyIconGetRect( &notifyIconID, &rect );

			SetForegroundWindow( m_NotifyIconData.hWnd );
			TrackPopupMenu( traymenu, TPM_RIGHTBUTTON, rect.left, rect.top, 0 /*reserved*/, m_NotifyIconData.hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}

HMENU WndTray::CreatePlaylistMenu( const Playlist::Ptr playlist )
{
	HMENU playlistMenu = NULL;
	if ( playlist ) {
		const Playlist::ItemList playlistItems = playlist->GetItems();
		if ( !playlistItems.empty() ) {
			playlistMenu = CreatePopupMenu();
			if ( nullptr != playlistMenu ) {

				int itemsPerColumn = 0;
				const int menuItemHeight = GetSystemMetrics( SM_CYMENU );
				const int screenHeight = GetSystemMetrics( SM_CYSCREEN );
				if ( ( menuItemHeight > 0 ) && ( screenHeight > 0 ) ) {
					itemsPerColumn = screenHeight / menuItemHeight - 1;
				}

				const int maxPlaylistEntries = sMaxPlaylistColumns * itemsPerColumn;

				int columnCount = 0;
				int playlistItemMenuIndex = 0;
				auto playlistItemIter = playlistItems.begin();

				Playlist::Item currentPlayingItem = m_Output.GetCurrentPlaying().PlaylistItem;
				int currentPlayingItemIndex = -1;
				if ( playlist->GetItem( currentPlayingItem, currentPlayingItemIndex ) ) {
					const int playlistItemCount = static_cast<int>( playlistItems.size() );
					if ( ( playlistItemCount > maxPlaylistEntries ) && ( currentPlayingItemIndex > maxPlaylistEntries / 2 ) ) {
						int itemsToAdvance = currentPlayingItemIndex - maxPlaylistEntries / 2;
						if ( ( playlistItemCount - itemsToAdvance ) < maxPlaylistEntries ) {
							itemsToAdvance = playlistItemCount - maxPlaylistEntries;
						}
						std::advance( playlistItemIter, itemsToAdvance );
					}
				}

				for ( ; ( playlistItemMenuIndex < maxPlaylistEntries ) && ( playlistItemIter != playlistItems.end() ) && ( m_NextPlaylistMenuItemID < MSG_TRAYMENUEND ); playlistItemIter++, playlistItemMenuIndex++ ) {
					const Playlist::Item& playlistItem = *playlistItemIter;
					std::wstring entryText;
					std::wstring artist = playlistItem.Info.GetArtist();
					WideStringReplace( artist, L"&", L"&&" );
					std::wstring title = playlistItem.Info.GetTitle( true /*filenameAsTitle*/ );
					WideStringReplace( title, L"&", L"&&" );
					if ( artist.empty() ) {
						entryText = title;
						if ( entryText.size() > sMaxPlaylistItemLength ) {
							entryText = entryText.substr( 0, sMaxPlaylistItemLength - 3 );
							entryText += L"...";
						}
					} else {
						entryText = artist + L" - " + title;
						if ( entryText.size() > sMaxPlaylistItemLength ) {
							entryText = title;
							if ( entryText.size() > sMaxPlaylistItemLength ) {
								entryText = entryText.substr( 0, sMaxPlaylistItemLength - 3 );
								entryText += L"...";
							}
						}
					}

					UINT flags = MF_STRING;
					if ( ( 0 == ( playlistItemMenuIndex % itemsPerColumn ) ) && ( 0 != playlistItemMenuIndex ) ) {
						if ( ++columnCount < sMaxPlaylistColumns ) {
							flags |= MF_MENUBARBREAK;
						}
					}
					m_PlaylistMenuItems.insert( PlaylistMenuItemMap::value_type( m_NextPlaylistMenuItemID, playlistItem.ID ) );

					AppendMenu( playlistMenu, flags, m_NextPlaylistMenuItemID, entryText.c_str() );
					if ( playlistItem.ID == currentPlayingItem.ID ) {
						CheckMenuItem( playlistMenu, m_NextPlaylistMenuItemID, MF_BYCOMMAND | MF_CHECKED );
					}

					++m_NextPlaylistMenuItemID;
				}
			}
		}
	}
	return playlistMenu;
}

void WndTray::OnSingleClick()
{
	bool enable = false;
	Settings::SystrayCommand singleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand doubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( enable, singleClick, doubleClick );
	DoCommand( singleClick );
}

void WndTray::OnDoubleClick()
{
	bool enable = false;
	Settings::SystrayCommand singleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand doubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( enable, singleClick, doubleClick );
	DoCommand( doubleClick );
}

void WndTray::DoCommand( const Settings::SystrayCommand command )
{
	switch ( command ) {
		case Settings::SystrayCommand::Play : {
			SendMessage( m_NotifyIconData.hWnd, WM_COMMAND, ID_CONTROL_PLAY /*wParam*/, 0 /*lParam*/ );
			break;
		}
		case Settings::SystrayCommand::Stop : {
			SendMessage( m_NotifyIconData.hWnd, WM_COMMAND, ID_CONTROL_STOP /*wParam*/, 0 /*lParam*/ );
			break;
		}
		case Settings::SystrayCommand::Previous : {
			SendMessage( m_NotifyIconData.hWnd, WM_COMMAND, ID_CONTROL_PREVIOUSTRACK /*wParam*/, 0 /*lParam*/ );
			break;
		}
		case Settings::SystrayCommand::Next : {
			SendMessage( m_NotifyIconData.hWnd, WM_COMMAND, ID_CONTROL_NEXTTRACK /*wParam*/, 0 /*lParam*/ );
			break;
		}
		case Settings::SystrayCommand::ShowHide : {
			SendMessage( m_NotifyIconData.hWnd, WM_COMMAND, ID_TRAYMENU_SHOWVUPLAYER /*wParam*/, 0 /*lParam*/ );
			break;
		}
		default : {
			break;
		}
	}
}
