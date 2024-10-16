#include "VUPlayer.h"

#include "CDDAExtract.h"
#include "Converter.h"

#include "DlgConvert.h"
#include "DlgOptions.h"
#include "DlgTrackInfo.h"

#include "Utility.h"

#include <dbt.h>
#include <windowsx.h>

#include <fstream>

// Main application instance.
VUPlayer* VUPlayer::s_VUPlayer = nullptr;

// Timer ID.
static const UINT_PTR s_TimerID = 42;

// Timer millisecond interval.
static const UINT s_TimerInterval = 100;

// Minimum application width.
static const int s_MinAppWidth = 640;

// Minimum application height.
static const int s_MinAppHeight = 480;

// Skip duration, in seconds.
static const float s_SkipDuration = 5.0f;

// Skip repeat limit interval, in seconds.
static const float s_SkipLimitInterval = 0.1f;

// Command ID of the first playlist entry on the Add to Playlist sub menu.
static const UINT MSG_PLAYLISTMENUSTART = WM_APP + 0xF00;

// Command ID of the last playlist entry on the Add to Playlist sub menu.
static const UINT MSG_PLAYLISTMENUEND = MSG_PLAYLISTMENUSTART + 50;

// Online documentation location.
static const wchar_t s_OnlineDocs[] = L"https://github.com/jfchapman/vuplayer/wiki";

// Database filename.
#ifdef _DEBUG
static const wchar_t s_Database[] = L"VUPlayerDebug.db";
#else
static const wchar_t s_Database[] = L"VUPlayer.db";
#endif

VUPlayer* VUPlayer::Get()
{
	return s_VUPlayer;
}

VUPlayer::VUPlayer( const HINSTANCE instance, const HWND hwnd, const std::list<std::wstring>& startupFilenames,
		const bool portable, const Database::Mode databaseMode ) :
	m_hInst( instance ),
	m_hWnd( hwnd ),
	m_hAccel( LoadAccelerators( m_hInst, MAKEINTRESOURCE( IDC_VUPLAYER ) ) ),
	m_hAccelEditLabel( CreateModifiedAcceleratorTable() ),
	m_Handlers(),
	m_Database( ( portable ? ( ApplicationFolder() / s_Database ) : ( DocumentsFolder() / s_Database ) ), databaseMode ),
	m_Library( m_Database, m_Handlers ),
	m_Maintainer( m_hInst, m_Library, m_Handlers ),
	m_Settings( m_Database, m_Library ),
	m_Output( m_hInst, m_hWnd, m_Handlers, m_Settings ),
	m_GainCalculator( m_Library, m_Handlers ),
	m_Scrobbler( m_Database, m_Settings ),
	m_MusicBrainz( m_hInst, m_hWnd ),
	m_DiscManager( m_hInst, m_hWnd, m_Library, m_Handlers, m_MusicBrainz ),
	m_Rebar( m_hInst, m_hWnd, m_Settings ),
	m_Status( m_hInst, m_hWnd ),
	m_Tree( m_hInst, m_hWnd, m_Library, m_Settings, m_DiscManager, m_Output ),
	m_Visual( m_hInst, m_hWnd, m_Rebar.GetWindowHandle(), m_Status.GetWindowHandle(), m_Settings, m_Output, m_Library ),
	m_List( m_hInst, m_hWnd, m_Settings, m_Output ),
	m_SeekControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output, m_Settings ),
	m_VolumeControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output, m_Settings ),
	m_ToolbarCrossfade( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarFile( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarFlow( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarInfo( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarOptions( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarPlayback( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarPlaylist( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarFavourites( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarEQ( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarConvert( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarTrackEnd( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_ToolbarVolume( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
  m_ToolbarLoudness( m_hInst, m_Rebar.GetWindowHandle(), m_Settings ),
	m_Counter( m_hInst, m_Rebar.GetWindowHandle(), m_Settings, m_Output ),
	m_Splitter( m_hInst, m_hWnd, m_Rebar.GetWindowHandle(), m_Status.GetWindowHandle(), m_Tree.GetWindowHandle(), m_Visual.GetWindowHandle(), m_List.GetWindowHandle(), m_Settings ),
	m_Tray( m_hInst, m_hWnd, m_Library, m_Settings, m_Output, m_Tree, m_List ),
	m_Taskbar( m_hInst, m_hWnd ),
	m_EQ( m_hInst, m_List.GetWindowHandle(), m_Settings, m_Output ),
	m_CurrentOutput(),
	m_CustomColours(),
	m_Hotkeys( m_hWnd, m_Settings ),
	m_LastSkipCount( {} ),
	m_LastOutputStateChange( 0 ),
	m_AddToPlaylistMenuMap(),
	m_TitlebarText(),
	m_IdleText(),
	m_IsHighContrast( IsHighContrastActive() ),
	m_IsTreeLabelEdit( false ),
	m_IsFirstTimeStartup( true ),
	m_IsEditingLabel( false ),
	m_IsConverting( false ),
  m_TitleBarFormat( m_Settings.GetTitleBarFormat() )
{
	s_VUPlayer = this;

	ReadWindowSettings();

	InitialiseRebar();

	for ( auto& iter : m_CustomColours ) {
		iter = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	}

	m_Output.SetPlaylistChangeCallback( [ this ] ( Playlist::Ptr playlist )
  {
    const WPARAM playlistID = reinterpret_cast<WPARAM>( playlist.get() );
    const LPARAM playlistType = static_cast<LPARAM>( playlist ? playlist->GetType() : Playlist::Type::_Undefined );
    PostMessage( m_Tree.GetWindowHandle(), MSG_OUTPUTPLAYLISTCHANGED, playlistID, playlistType );
  } );
  m_Output.SetSelectFollowedTrackCallback( [ this ] ( const long itemID )
  {
    PostMessage( m_List.GetWindowHandle(), MSG_SELECTPLAYLISTITEM, itemID, 0 );
  } );

	m_Tree.Initialise();

	if ( OnCommandLineFiles( startupFilenames ) ) {
		m_List.SetPlaylist( m_Tree.GetSelectedPlaylist() );
	} else {
		const auto& [ initialFilename, cueStart, cueEnd ] = m_Settings.GetStartupFile();
    MediaInfo startupInfo( initialFilename );
    startupInfo.SetCueStart( cueStart );
    startupInfo.SetCueEnd( cueEnd );
		m_List.SetPlaylist( m_Tree.GetSelectedPlaylist(), false, startupInfo );
	}
  m_Output.SetPlaylistInformationToFollow( m_List.GetPlaylist(), m_List.GetSelectedPlaylistItems() );

	m_Status.SetPlaylist( m_List.GetPlaylist() );

	OnListSelectionChanged();

	m_EQ.Init( m_hWnd );

	SetWindowPos( m_Tree.GetWindowHandle(), m_List.GetWindowHandle(), 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE );
	SetFocus( m_List.GetWindowHandle() );

	if ( m_Settings.GetAlwaysOnTop() ) {
		SetWindowPos( m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	}

	m_Handlers.Init( m_Settings );

	const int idleSize = 32;
	WCHAR idleText[ idleSize ] = {};
	LoadString( m_hInst, IDS_APP_TITLE, idleText, idleSize );
	m_IdleText = idleText;

	SetTimer( m_hWnd, s_TimerID, s_TimerInterval, NULL /*timerProc*/ );
}

VUPlayer::~VUPlayer()
{
}

void VUPlayer::ReadWindowSettings()
{
	int x = -1;
	int y = -1;
	int width = -1;
	int height = -1;
	bool maximised = false;
	bool minimised = false;
	m_Settings.GetStartupPosition( x, y, width, height, maximised, minimised );
	const float dpiScaling = GetDPIScaling();
	if ( ( width >= static_cast<int>( s_MinAppWidth * dpiScaling ) ) && ( height >= static_cast<int>( s_MinAppHeight * dpiScaling ) ) ) {
		// Check that some portion of the title bar is visible.
		const int captionSize = GetSystemMetrics( SM_CYCAPTION ) / 2;
		const std::list<POINT> bounds = { { x + captionSize, y + captionSize }, { x + width - captionSize, y + captionSize } };
		for ( const auto& point : bounds ) {
			const HMONITOR monitor = MonitorFromPoint( point, MONITOR_DEFAULTTONULL );
			if ( nullptr != monitor ) {
				MoveWindow( m_hWnd, x, y, width, height, FALSE /*repaint*/ );
				break;
			}
		}
	}

	bool systrayEnable = false;
	bool systrayMinimise = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayTripleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayQuadClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnable, systrayMinimise, systraySingleClick, systrayDoubleClick, systrayTripleClick, systrayQuadClick );
	if ( minimised ) {
		ShowWindow( m_hWnd, SW_SHOWMINIMIZED );
		if ( systrayEnable && systrayMinimise ) {
			ShowWindow( m_hWnd, SW_HIDE );
		}
	} else if ( maximised ) {
		ShowWindow( m_hWnd, SW_SHOWMAXIMIZED );
	} else {
		ShowWindow( m_hWnd, SW_SHOW );
	}
	if ( systrayEnable ) {
		m_Tray.Show();
	}
	UpdateWindow( m_hWnd );

	// Force the status bar to update.
	RECT rect = {};
	GetClientRect( m_hWnd, &rect );
	LPARAM lParam = MAKELPARAM( static_cast<WORD>( rect.right - rect.left ), static_cast<WORD>( rect.bottom - rect.top ) );
	SendMessage( m_Status.GetWindowHandle(), WM_SIZE, 0 /*wParam*/, lParam );
	RedrawWindow( m_Status.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
}

void VUPlayer::WriteWindowSettings()
{
	int x = -1;
	int y = -1;
	int width = -1;
	int height = -1;
	bool maximised = false;
	bool minimised = false;
	m_Settings.GetStartupPosition( x, y, width, height, maximised, minimised );
	maximised = ( FALSE != IsZoomed( m_hWnd ) );
	minimised = ( FALSE != IsIconic( m_hWnd ) );
	if ( !maximised && !minimised ) {
		RECT rect = {};
		GetWindowRect( m_hWnd, &rect );
		x = rect.left;
		y = rect.top;
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}
	m_Settings.SetStartupPosition( x, y, width, height, maximised, minimised );
}

std::filesystem::path VUPlayer::DocumentsFolder()
{
	std::filesystem::path folder;
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath( FOLDERID_Documents, KF_FLAG_DEFAULT, NULL /*token*/, &path );
	if ( SUCCEEDED( hr ) ) {
		folder = path;
		CoTaskMemFree( path );
		folder /= L"VUPlayer";
		CreateDirectory( folder.c_str(), NULL /*attributes*/ ); 
	}
	return folder;
}

std::filesystem::path VUPlayer::ApplicationFolder()
{
	std::filesystem::path folder;
	constexpr size_t kPathSize = 1024;
	std::vector<TCHAR> filename( kPathSize );
	if ( GetModuleFileName( nullptr, filename.data(), kPathSize - 1 ) && ( ERROR_INSUFFICIENT_BUFFER != GetLastError() ) ) {
		const std::filesystem::path filepath( filename.data() );
		folder = filepath.parent_path();
	}
	return folder;
}

void VUPlayer::OnSize( WPARAM wParam, LPARAM lParam )
{
	if ( SIZE_MINIMIZED == wParam ) {
		bool systrayEnable = false;
		bool systrayMinimise = false;
		Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
		Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
		Settings::SystrayCommand systrayTripleClick = Settings::SystrayCommand::None;
		Settings::SystrayCommand systrayQuadClick = Settings::SystrayCommand::None;
		m_Settings.GetSystraySettings( systrayEnable, systrayMinimise, systraySingleClick, systrayDoubleClick, systrayTripleClick, systrayQuadClick );
		if ( systrayEnable && systrayMinimise ) {
			ShowWindow( m_hWnd, SW_HIDE );
		}
	} else {
		SendMessage( m_Rebar.GetWindowHandle(), WM_SIZE, wParam, lParam );
		SendMessage( m_Status.GetWindowHandle(), WM_SIZE, wParam, lParam );
		SendMessage( m_Splitter.GetWindowHandle(), WM_SIZE, wParam, lParam );
	}
}

bool VUPlayer::OnNotify( WPARAM wParam, LPARAM lParam, LRESULT& result )
{
	bool handled = false;
	LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
	if ( nullptr != nmhdr ) {
		switch ( nmhdr->code ) {
			case TVN_BEGINLABELEDIT : {
				result = m_Tree.OnBeginLabelEdit( wParam, lParam );
				m_IsEditingLabel = !result;
				m_IsTreeLabelEdit = true;
				handled = true;
				break;
			}
			case TVN_ENDLABELEDIT : {
				result = m_Tree.OnEndLabelEdit( wParam, lParam );
				m_IsEditingLabel = false;
				m_IsTreeLabelEdit = false;
				break;
			}
			case TVN_SELCHANGED : {
				LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>( lParam );
				if ( ( nullptr != nmTreeView ) && ( nullptr != nmTreeView->itemNew.hItem ) ) {
					Playlist::Ptr playlist = m_Tree.GetPlaylist( nmTreeView->itemNew.hItem );
					m_List.SetPlaylist( playlist );
					m_Status.SetPlaylist( playlist );
          m_Output.SetPlaylistInformationToFollow( m_List.GetPlaylist(), m_List.GetSelectedPlaylistItems() );
				}
				break;
			}
			case TVN_ITEMEXPANDING : {
				LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>( lParam );
				if ( ( nullptr != nmTreeView ) && ( nullptr != nmTreeView->itemNew.hItem ) && ( TVE_EXPAND == nmTreeView->action ) ) {
					m_Tree.OnItemExpanding( nmTreeView->itemNew.hItem );
				}
				break;
			}
			case NM_RCLICK : {
				if ( m_Tree.GetWindowHandle() == nmhdr->hwndFrom ) {
					POINT pt = {};
					GetCursorPos( &pt );
					m_Tree.OnContextMenu( pt );
				}
				break;
			}
			case HDN_ITEMCLICK : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( nullptr != hdr ) {
					HDITEM headerItem = {};
					headerItem.mask = HDI_LPARAM;
					if ( TRUE == Header_GetItem( hdr->hdr.hwndFrom, hdr->iItem, &headerItem ) ) {
						const Playlist::Column column = static_cast<Playlist::Column>( headerItem.lParam );
						m_List.SortPlaylist( column );
					}
				}
				break;
			}
			case NM_CUSTOMDRAW : {		
				if ( LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam ); ( nullptr != hdr ) && !m_IsHighContrast ) {
					if ( hdr->hwndFrom == m_List.GetWindowHandle() ) {
						LPNMLVCUSTOMDRAW nmlvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>( lParam );
						handled = true;
						switch ( nmlvcd->nmcd.dwDrawStage ) {
							case CDDS_PREPAINT : {
								result = CDRF_NOTIFYITEMDRAW;
								break;
							}
							case CDDS_ITEMPREPAINT : {
								const COLORREF backColour = ListView_GetBkColor( hdr->hwndFrom );
								const COLORREF highlightColour = m_List.GetHighlightColour();
								const bool selected = ( LVIS_SELECTED == ListView_GetItemState( hdr->hwndFrom, nmlvcd->nmcd.dwItemSpec, LVIS_SELECTED ) );
								if ( selected ) {
									nmlvcd->clrText = backColour;
									nmlvcd->clrTextBk = highlightColour;
								} else if ( m_CurrentOutput.PlaylistItem.ID == m_List.GetPlaylistItemID( static_cast<int>( nmlvcd->nmcd.dwItemSpec ) ) ) {
									nmlvcd->clrText = highlightColour;
									nmlvcd->clrTextBk = backColour;
								}

								// Mask out selection state so that the custom highlight colour takes effect.
								nmlvcd->nmcd.uItemState &= ~CDIS_SELECTED;

								// Mask out focus state so that a focus rectangle is not drawn.
								if ( nmlvcd->nmcd.uItemState & CDIS_FOCUS ) {
									nmlvcd->nmcd.uItemState ^= CDIS_FOCUS;
								}

								result = CDRF_DODEFAULT;
								break;
							}
							default : {
								result = CDRF_DODEFAULT;
								break;
							}
						}
					} else if ( hdr->hwndFrom == m_Tree.GetWindowHandle() ) {
						LPNMTVCUSTOMDRAW nmtvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>( lParam );
						handled = true;
						switch ( nmtvcd->nmcd.dwDrawStage ) {
							case CDDS_PREPAINT : {
								result = CDRF_NOTIFYITEMDRAW;
								break;
							}
							case CDDS_ITEMPREPAINT : {
								const COLORREF backColour = TreeView_GetBkColor( hdr->hwndFrom );
								const COLORREF highlightColour = m_Tree.GetHighlightColour();
								const HTREEITEM treeItem = reinterpret_cast<HTREEITEM>( nmtvcd->nmcd.dwItemSpec );
								const bool selected = ( TreeView_GetSelection( m_Tree.GetWindowHandle() ) == treeItem );
								if ( selected ) {
									nmtvcd->clrText = backColour;
									nmtvcd->clrTextBk = highlightColour;
								}

								// Mask out selection state so that the custom highlight colour takes effect.
								nmtvcd->nmcd.uItemState &= ~CDIS_SELECTED;

								// Mask out focus state so that a focus rectangle is not drawn.
								if ( nmtvcd->nmcd.uItemState & CDIS_FOCUS ) {
									nmtvcd->nmcd.uItemState ^= CDIS_FOCUS;
								}

								result = CDRF_DODEFAULT;
								break;
							}
							default : {
								result = CDRF_DODEFAULT;
								break;
							}
						}
					}
				}
				break;
			}
			case LVN_BEGINLABELEDIT : {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != hdr ) && ( hdr->hwndFrom == m_List.GetWindowHandle() ) ) {
					NMLVDISPINFO* dispInfo = reinterpret_cast<NMLVDISPINFO*>( lParam );
					result = m_List.OnBeginLabelEdit( dispInfo->item );
					m_IsEditingLabel = !result;
					handled = true;
				}
				break;
			}
			case LVN_ENDLABELEDIT : {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != hdr ) && ( hdr->hwndFrom == m_List.GetWindowHandle() ) ) {
					NMLVDISPINFO* dispInfo = reinterpret_cast<NMLVDISPINFO*>( lParam );
					m_List.OnEndLabelEdit( dispInfo->item );
					result = FALSE;
					m_IsEditingLabel = false;
					handled = true;
				}
				break;
			}
			case LVN_BEGINDRAG : {
				LPNMLISTVIEW nmListView = reinterpret_cast<LPNMLISTVIEW>( lParam );
				if ( nullptr != nmListView ) {
					m_List.OnBeginDrag( nmListView->iItem );
				}
				break;
			}
			case LVN_ITEMCHANGED : {
				LPNMLISTVIEW nmListView = reinterpret_cast<LPNMLISTVIEW>( lParam );
				if ( nullptr != nmListView ) {
					if ( nmListView->uNewState & LVIS_FOCUSED ) {
						OnListSelectionChanged();
					} else if ( ( -1 == nmListView->iItem ) && ( 0 == nmListView->uNewState ) ) {
            m_Output.SetPlaylistInformationToFollow( m_List.GetPlaylist(), m_List.GetSelectedPlaylistItems() );
          }
				}
				break;				
			}
      case LVN_ODSTATECHANGED : {
				OnListSelectionChanged();
        break;
      }
			case LVN_DELETEITEM : {
				const int itemCount = m_List.GetCount();
				if ( 1 == itemCount ) {
					// Last item is being deleted.
					OnListSelectionChanged();
				}
				break;
			}
      case LVN_GETDISPINFO : {
				if ( NMLVDISPINFO* dispInfo = reinterpret_cast<NMLVDISPINFO*>( lParam ); nullptr != dispInfo ) {
          m_List.OnDisplayInfo( dispInfo->item );
        }
        break;
      }
      case LVN_ODFINDITEM : {
        handled = true;
        result = -1;
        if ( NMLVFINDITEM* findItem = reinterpret_cast<NMLVFINDITEM*>( lParam ); nullptr != findItem ) {
          result = m_List.OnFindItem( findItem->lvfi, findItem->iStart );
        }
        break;
      }
			case HDN_BEGINTRACK : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( ( nullptr != hdr ) && ( 0 == hdr->iItem ) ) {
					// Prevent tracking of dummy column.
					result = TRUE;
					handled = true;
				}
				break;
			}
			case HDN_ENDDRAG : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( nullptr != hdr ) {
					m_List.OnEndDragColumn();
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return handled;
}

bool VUPlayer::OnTimer( const UINT_PTR timerID )
{
	bool handled = true;
	if ( s_TimerID == timerID ) {
		if ( m_IsFirstTimeStartup ) {
			m_IsFirstTimeStartup = false;
			RedrawWindow( m_Status.GetWindowHandle(), NULL /*updateRect*/, NULL /*updateRegion*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
		}

		Output::Item currentPlaying = m_Output.GetCurrentPlaying();
		if ( m_CurrentOutput.PlaylistItem.ID != currentPlaying.PlaylistItem.ID ) {
			OnOutputChanged( m_CurrentOutput, currentPlaying );
		}
		m_CurrentOutput = currentPlaying;
		SetTitlebarText( m_CurrentOutput );

		const Playlist::Ptr currentPlaylist = m_List.GetPlaylist();
		const Playlist::Item currentSelectedPlaylistItem = m_List.GetCurrentSelectedItem();
		
		if ( 0 == currentPlaying.PlaylistItem.ID ) {
			const Playlist::Item currentSelectedOutputItem = m_Output.GetCurrentSelectedPlaylistItem();
			if ( currentSelectedPlaylistItem.ID != currentSelectedOutputItem.ID ) {
				m_Output.SetCurrentSelectedPlaylistItem( currentSelectedPlaylistItem );
				if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
					m_Splitter.Resize();
					m_Visual.DoRender();
				}
			}
		}

		m_SeekControl.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarFile.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarPlaylist.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarFavourites.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarOptions.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarInfo.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarCrossfade.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
    m_ToolbarLoudness.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarFlow.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarPlayback.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarTrackEnd.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarEQ.Update( m_EQ.IsVisible() );
		m_ToolbarConvert.Update( currentPlaylist );
		m_ToolbarVolume.Update( m_Output, m_VolumeControl.GetType() );
		m_Counter.Refresh();
		m_Status.Update( m_GainCalculator, m_Maintainer, m_MusicBrainz );
		m_Tray.Update( m_CurrentOutput );
		if ( !m_IsConverting ) {
			m_Taskbar.Update( m_ToolbarPlayback, m_Output );
		}
		m_List.UpdateStatusIcon();
		m_Tree.UpdateOutputIcon();
	} else if ( TIMER_SYSTRAY == timerID ) {
		m_Tray.OnTimerElapsed();
	} else {
		handled = false;
	}
	return handled;
}

void VUPlayer::OnOutputChanged( const Output::Item& previousItem, const Output::Item& currentItem )
{
	UpdateScrobbler( previousItem, currentItem );
	if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
		m_Splitter.Resize();
		m_Visual.DoRender();
	}
	RedrawWindow( m_List.GetWindowHandle(), NULL /*rect*/, NULL /*region*/, RDW_INVALIDATE | RDW_NOERASE );
}

void VUPlayer::OnMinMaxInfo( LPMINMAXINFO minMaxInfo )
{
	if ( nullptr != minMaxInfo ) {
		const float dpiScaling = GetDPIScaling();
		minMaxInfo->ptMinTrackSize = { static_cast<LONG>( s_MinAppWidth * dpiScaling ), static_cast<LONG>( s_MinAppHeight * dpiScaling ) };
	}
}

void VUPlayer::OnPlaylistItemAdded( Playlist* playlist, const Playlist::Item& item, const int position )
{
	if ( ( nullptr != playlist ) && ( item.ID > 0 ) ) {
		m_List.OnFileAdded( playlist, item, position );

		if ( Playlist::Type::All != playlist->GetType() ) {
			const Playlist::Ptr playlistAll = m_Tree.GetPlaylistAll();
			if ( playlistAll ) {
				playlistAll->AddPending( item.Info );
			}
		}

		if ( IsURL( item.Info.GetFilename() ) ) {
			const Playlist::Ptr playlistStreams = m_Tree.GetPlaylistStreams();
			if ( playlistStreams ) {
				playlistStreams->AddPending( item.Info );
			}
		}

		m_Status.Update( playlist );
	}
}

void VUPlayer::OnPlaylistItemRemoved( Playlist* playlist, const Playlist::Item& item )
{
	m_List.OnFileRemoved( playlist, item );
	m_Status.Update( playlist );
}

void VUPlayer::OnPlaylistItemUpdated( Playlist* playlist, const Playlist::Item& item )
{
	m_List.OnItemUpdated( playlist, item );
}

void VUPlayer::OnDestroy()
{
	KillTimer( m_hWnd, s_TimerID );

	SaveSettings();

	m_Output.Stop();

	UpdateScrobbler( m_CurrentOutput, m_Output.GetCurrentPlaying() );

	m_GainCalculator.Stop();
	m_Maintainer.Stop();

	WriteWindowSettings();
}

void VUPlayer::OnCommand( const int commandID )
{
	switch ( commandID ) {
		case ID_VISUAL_VUMETER_STEREO :
		case ID_VISUAL_VUMETER_MONO :
		case ID_VISUAL_OSCILLOSCOPE :
		case ID_VISUAL_SPECTRUMANALYSER :
		case ID_VISUAL_ARTWORK :
		case ID_VISUAL_PEAKMETER :
		case ID_VISUAL_NONE : {
			m_Visual.SelectVisual( commandID );
			m_Splitter.Resize();
			break;
		}
		case ID_OSCILLOSCOPE_COLOUR : {
			m_Visual.OnOscilloscopeColour();
			break;
		}
		case ID_OSCILLOSCOPE_BACKGROUNDCOLOUR : {
			m_Visual.OnOscilloscopeBackground();
			break;
		}
		case ID_OSCILLOSCOPE_WEIGHT_FINE : 
		case ID_OSCILLOSCOPE_WEIGHT_NORMAL :
		case ID_OSCILLOSCOPE_WEIGHT_BOLD : {
			m_Visual.OnOscilloscopeWeight( commandID );
			break;
		}
		case ID_SPECTRUMANALYSER_BASECOLOUR :
		case ID_SPECTRUMANALYSER_PEAKCOLOUR : 
		case ID_SPECTRUMANALYSER_BACKGROUNDCOLOUR : {
			m_Visual.OnSpectrumAnalyserColour( commandID );
			break;
		}
		case ID_PEAKMETER_BASECOLOUR :
		case ID_PEAKMETER_PEAKCOLOUR :
		case ID_PEAKMETER_BACKGROUNDCOLOUR : {
			m_Visual.OnPeakMeterColour( commandID );
			break;
		}
		case ID_VUMETER_SLOWDECAY :
		case ID_VUMETER_NORMALDECAY :
		case ID_VUMETER_FASTDECAY : {
			m_Visual.OnVUMeterDecay( commandID );
			break;
		}
		case ID_VISUAL_HARDWAREACCELERATION : {
			m_Visual.ToggleHardwareAccelerationEnabled();
			break;
		}
		case ID_CONTROL_PLAY :
		case ID_TRAY_PLAY : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Pause();
					break;
				}
				default : {
					const Playlist::Ptr playlist = m_List.GetPlaylist();
					if ( playlist ) {
						Playlist::Item item = m_List.GetCurrentSelectedItem();
						if ( ( 0 == item.ID ) && ( playlist->GetCount() > 0 ) ) {
							if ( m_Output.GetRandomPlay() ) {
								item = playlist->GetRandomItem( {} );
							} else {
								item = playlist->GetFirstItem();
							}
							m_List.SelectPlaylistItem( item.ID );
						}
						if ( 0 != item.ID ) {
							m_Output.Play( playlist, item.ID );
						}
					}
					break;
				}
			}
			break;
		}
		case ID_CONTROL_STOP : {
			m_Output.Stop();
			if ( m_Output.GetStopAtTrackEnd() && !m_Settings.GetRetainStopAtTrackEnd() ) {
				m_Output.ToggleStopAtTrackEnd();
			}
			break;
		}
		case ID_CONTROL_PREVIOUSTRACK : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Previous();
					const Playlist::Item item = m_Output.GetCurrentPlaying().PlaylistItem;
					m_List.EnsureVisible( item );
					break;
				}
				default : {
					m_List.SelectPreviousItem();
					break;
				}
			}
			break;
		}
		case ID_CONTROL_NEXTTRACK : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Next();
					const Playlist::Item item = m_Output.GetCurrentPlaying().PlaylistItem;
					m_List.EnsureVisible( item );
					break;
				}
				default : {
					m_List.SelectNextItem();
					break;
				}
			}
			break;
		}
    case ID_CONTROL_FOLLOWSELECTION : {
      m_Output.ToggleFollowTrackSelection();
      break;
    }
		case ID_CONTROL_STOPTRACKEND : {
			m_Output.ToggleStopAtTrackEnd();
			break;
		}
		case ID_CONTROL_FADEOUT : {
			m_Output.ToggleFadeOut();
			break;
		}
		case ID_CONTROL_FADETONEXT : {
			m_Output.ToggleFadeToNext();
			break;
		}
		case ID_CONTROL_MUTE : {
			m_Output.ToggleMuted();
			break;
		}
		case ID_CONTROL_VOLUMEDOWN : {
			float volume = m_Output.GetVolume() - 0.01f;
			if ( volume < 0.0f ) {
				volume = 0.0f;
			}
			m_Output.SetVolume( volume );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_VOLUMEUP : {
			float volume = m_Output.GetVolume() + 0.01f;
			if ( volume > 1.0f ) {
				volume = 1.0f;
			}
			m_Output.SetVolume( volume );
			m_VolumeControl.Update();
			break;
		}
		case ID_VOLUME_100 :
		case ID_VOLUME_90 :
		case ID_VOLUME_80 :
		case ID_VOLUME_70 :
		case ID_VOLUME_60 :
		case ID_VOLUME_50 :
		case ID_VOLUME_40 :
		case ID_VOLUME_30 :
		case ID_VOLUME_20 :
		case ID_VOLUME_10 :
		case ID_VOLUME_0 : {
			m_Output.SetVolume( m_Tray.GetMenuVolume( commandID ) );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_SKIPBACKWARDS : {
			if ( AllowSkip() ) {
				const Output::State state = m_Output.GetState();
				if ( ( Output::State::Playing == state ) || ( Output::State::Paused == state ) ) {
					const Output::Item item = m_Output.GetCurrentPlaying();
					const double position = item.Position - s_SkipDuration;
					if ( position < 0 ) {
						m_Output.Previous( true /*forcePrevious*/, -s_SkipDuration );
					} else {
						m_Output.Play( item.PlaylistItem.ID, position );
					}
				}
				ResetLastSkipCount();
			}
			break;
		}
		case ID_CONTROL_SKIPFORWARDS : {
			if ( AllowSkip() ) {
				const Output::State state = m_Output.GetState();
				if ( ( Output::State::Playing == state ) || ( Output::State::Paused == state ) ) {
					const Output::Item item = m_Output.GetCurrentPlaying();
					const double position = item.Position + s_SkipDuration;
					if ( position > item.PlaylistItem.Info.GetDuration() ) {
						m_Output.Next();
					} else {
						m_Output.Play( item.PlaylistItem.ID, position );
					}
				}
				ResetLastSkipCount();
			}
			break;
		}
		case ID_CONTROL_PITCHDOWN : 
		case ID_CONTROL_PITCHUP : {
			const float pitchRange = m_Settings.GetPitchRangeOptions()[ m_Settings.GetPitchRange() ];
			if ( pitchRange > 0 ) {
				const float pitchAdjustment = pitchRange * ( ( ID_CONTROL_PITCHDOWN == commandID ) ? -1 : 1 ) / 100;
				m_Output.SetPitch( m_Output.GetPitch() + pitchAdjustment );
				m_VolumeControl.Update();
			}
			break;
		}
		case ID_CONTROL_PITCHRESET : {
			m_Output.SetPitch( 1.0f );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_BALANCELEFT : 
		case ID_CONTROL_BALANCERIGHT : {
			const float balance = m_Output.GetBalance() + ( ( ID_CONTROL_BALANCELEFT == commandID ) ? -0.01f : 0.01f );
			m_Output.SetBalance( balance );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_BALANCERESET : {
			m_Output.SetBalance( 0 );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_PITCHRANGE_SMALL : 
		case ID_CONTROL_PITCHRANGE_MEDIUM : 
		case ID_CONTROL_PITCHRANGE_LARGE : {
			const Settings::PitchRange previousPitchRangeSetting = m_Settings.GetPitchRange();
			const Settings::PitchRange currentPitchRangeSetting = ( ID_CONTROL_PITCHRANGE_SMALL == commandID ) ? Settings::PitchRange::Small :
				( ( ID_CONTROL_PITCHRANGE_MEDIUM == commandID ) ? Settings::PitchRange::Medium : Settings::PitchRange::Large );
			if ( currentPitchRangeSetting != previousPitchRangeSetting ) {
				m_Settings.SetPitchRange( currentPitchRangeSetting );
				const float previousPitch =	m_Output.GetPitch();
				if ( previousPitch != 1.0f ) {
					const float previousPitchRange = m_Settings.GetPitchRangeOptions()[ previousPitchRangeSetting ];
					const float currentPitchRange = m_Settings.GetPitchRangeOptions()[ currentPitchRangeSetting ];
					if ( ( previousPitchRange > 0 ) && ( currentPitchRange > 0 ) ) {
						const float updatedPitch = 1.0f + ( previousPitch - 1.0f ) * ( currentPitchRange / previousPitchRange );
						m_Output.SetPitch( updatedPitch );
					}
				}
			}
			break;
		}
		case ID_CONTROL_CROSSFADE : {
			m_Output.SetCrossfade( !m_Output.GetCrossfade() );
			break;
		}
		case ID_CONTROL_LOUDNESS : {
			m_Output.SetLoudnessNormalisation( !m_Output.GetLoudnessNormalisation() );
			break;
		}
		case ID_CONTROL_RANDOMPLAY : {
			m_Output.SetRandomPlay( !m_Output.GetRandomPlay() );
			break;
		}
		case ID_CONTROL_REPEATTRACK : {
			m_Output.SetRepeatTrack( !m_Output.GetRepeatTrack() );
			break;
		}
		case ID_CONTROL_REPEATPLAYLIST : {
			m_Output.SetRepeatPlaylist( !m_Output.GetRepeatPlaylist() );
			break;
		}
		case ID_FILE_CALCULATEGAIN : {
			OnCalculateGain();
			break;
		}
		case ID_VIEW_TRACKINFORMATION : {
			OnTrackInformation();
			break;
		}
		case ID_FILE_NEWPLAYLIST : {
			m_Tree.NewPlaylist( true /*editTitle*/ );
			break;
		}
		case ID_FILE_DELETEPLAYLIST : {
			m_Tree.DeleteSelectedPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_RENAMEPLAYLIST : {
			m_Tree.RenameSelectedPlaylist();
			break;
		}
		case ID_FILE_IMPORTPLAYLIST : {
			m_Tree.ImportPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_EXPORTPLAYLIST : {
			m_Tree.ExportSelectedPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDSTREAM : {
			m_List.OnCommandAddStream();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDFOLDER : {
			m_List.OnCommandAddFolder();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			m_List.OnCommandAddFiles();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTREMOVEFILES : {
			m_List.DeleteSelectedItems();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_ADDTOFAVOURITES : {
			OnAddToFavourites();
			break;
		}
		case ID_FILE_CUT : 
		case ID_FILE_COPY : {
			if ( m_IsTreeLabelEdit ) {
				m_Tree.OnCutCopy( ID_FILE_CUT == commandID );
			} else {
				m_List.OnCutCopy( ID_FILE_CUT == commandID );
			}
			break;
		}
		case ID_FILE_PASTE : {
			if ( m_IsTreeLabelEdit ) {
				m_Tree.OnPaste();
			} else {
				m_List.OnPaste();
			}
			break;
		}
		case ID_FILE_SELECTALL : {
			if ( m_IsTreeLabelEdit ) {
				m_Tree.OnSelectAll();
			} else {
				m_List.OnSelectAll();
			}
			break;
		}
		case ID_FILE_OPTIONS : {
			OnOptions();
			break;
		}
		case ID_FILE_REFRESHMEDIALIBRARY : {
			m_Maintainer.Start( 
        [ playlistAll = m_Tree.GetPlaylistAll() ] ( const std::filesystem::path& file ) {
				  if ( playlistAll ) {
					  playlistAll->AddPending( MediaInfo( file ) );
				  }
			  },
        [ this ] ( const MediaInfo::List& removedFiles ) {
          if ( !removedFiles.empty() ) {
            MediaInfo::List* mediaList = new MediaInfo::List( removedFiles.begin(), removedFiles.end() );
            PostMessage( m_hWnd, MSG_LIBRARYREFRESHED, reinterpret_cast<WPARAM>( mediaList ), 0 );
          }
        }
      );
			break;
		}
		case ID_FILE_CONVERT : {
			OnConvert();
			break;
		}
		case ID_FILE_MUSICBRAINZ_QUERY : {
			OnMusicBrainzQuery();
			break;
		}
		case ID_VIEW_COUNTER_FONTSTYLE : {
			m_Counter.OnSelectFont();
			break;
		}
		case ID_VIEW_COUNTER_FONTCOLOUR : {
			m_Counter.OnSelectColour();
			break;
		}
		case ID_VIEW_COUNTER_TRACKELAPSED : 
		case ID_VIEW_COUNTER_TRACKREMAINING : {
			m_Counter.SetTrackRemaining( ID_VIEW_COUNTER_TRACKREMAINING == commandID );
			break;
		}
		case ID_VIEW_TRACKBAR_VOLUME : {
			m_VolumeControl.SetType( WndTrackbar::Type::Volume );
			break;
		}
		case ID_VIEW_TRACKBAR_PITCH : {
			m_VolumeControl.SetType( WndTrackbar::Type::Pitch );
			break;
		}
		case ID_VIEW_TRACKBAR_BALANCE : {
			m_VolumeControl.SetType( WndTrackbar::Type::Balance );
			break;
		}
		case ID_VIEW_EQ : {
			m_EQ.ToggleVisibility();
			break;
		}
		case ID_VIEW_ALWAYSONTOP : {
			const bool alwaysOnTop = !m_Settings.GetAlwaysOnTop();
			m_Settings.SetAlwaysOnTop( alwaysOnTop );
			if ( alwaysOnTop ) {
				SetWindowPos( m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			} else {
				SetWindowPos( m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			}
			break;
		}
		case ID_SHOWCOLUMNS_TITLE :
		case ID_SHOWCOLUMNS_ARTIST :
		case ID_SHOWCOLUMNS_ALBUM :
		case ID_SHOWCOLUMNS_GENRE :
    case ID_SHOWCOLUMNS_COMPOSER :
    case ID_SHOWCOLUMNS_CONDUCTOR :
    case ID_SHOWCOLUMNS_PUBLISHER :
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
		case ID_SHOWCOLUMNS_FILEPATH :
		case ID_SHOWCOLUMNS_FILENAME :
		case ID_SHOWCOLUMNS_FILETIME :
		case ID_SHOWCOLUMNS_TRACKGAIN :
		case ID_SHOWCOLUMNS_ALBUMGAIN :
		case ID_SHOWCOLUMNS_STATUS : {
			m_List.OnShowColumn( commandID );
			break;
		}
		case ID_SORTPLAYLIST_ARTIST :
		case ID_SORTPLAYLIST_ALBUM :
		case ID_SORTPLAYLIST_GENRE :
    case ID_SORTPLAYLIST_COMPOSER :
    case ID_SORTPLAYLIST_CONDUCTOR :
    case ID_SORTPLAYLIST_PUBLISHER :
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
		case ID_SORTPLAYLIST_FILEPATH :
		case ID_SORTPLAYLIST_FILENAME :
		case ID_SORTPLAYLIST_FILETIME :
		case ID_SORTPLAYLIST_TRACKGAIN :
		case ID_SORTPLAYLIST_ALBUMGAIN : {
			m_List.OnSortPlaylist( commandID );
			break;
		}
		case ID_LISTMENU_FONTSTYLE : {
			m_List.OnSelectFont();
			break;
		}
		case ID_LISTMENU_FONTCOLOUR : 
		case ID_LISTMENU_BACKGROUNDCOLOUR :
		case ID_LISTMENU_HIGHLIGHTCOLOUR : 
		case ID_LISTMENU_STATUSICONCOLOUR : {
			m_List.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_FONTSTYLE : {
			m_Tree.OnSelectFont();
			break;
		}
		case ID_TREEMENU_FONTCOLOUR : 
		case ID_TREEMENU_BACKGROUNDCOLOUR :
		case ID_TREEMENU_HIGHLIGHTCOLOUR :
		case ID_TREEMENU_ICONCOLOUR : {
			m_Tree.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_FAVOURITES : {
			m_Tree.OnFavourites();
			break;
		}
		case ID_TREEMENU_STREAMS : {
			m_Tree.OnStreams();
			break;
		}
		case ID_TREEMENU_ALLTRACKS : {
			m_Tree.OnAllTracks();
			break;
		}
		case ID_TREEMENU_ARTISTS : {
			m_Tree.OnArtists();
			break;
		}
		case ID_TREEMENU_PUBLISHERS : {
			m_Tree.OnPublishers();
			break;
		}
		case ID_TREEMENU_COMPOSERS : {
			m_Tree.OnComposers();
			break;
		}
		case ID_TREEMENU_CONDUCTORS : {
			m_Tree.OnConductors();
			break;
		}
		case ID_TREEMENU_ALBUMS : {
			m_Tree.OnAlbums();
			break;
		}
		case ID_TREEMENU_GENRES : {
			m_Tree.OnGenres();
			break;
		}
		case ID_TREEMENU_YEARS : {
			m_Tree.OnYears();
			break;
		}
		case ID_TRAYMENU_SHOWVUPLAYER : {
			if ( IsIconic( m_hWnd ) ) {
				if ( !IsWindowVisible( m_hWnd ) ) {
					ShowWindow( m_hWnd, SW_SHOWMINIMIZED );
				}
				ShowWindow( m_hWnd, SW_RESTORE );
			} else {
				ShowWindow( m_hWnd, SW_MINIMIZE );
			}
			break;
		}
		case ID_HELP_ONLINEDOCUMENTATION : {
			ShellExecute( NULL, L"open", s_OnlineDocs, NULL, NULL, SW_SHOWNORMAL );
			break;
		}
		case ID_TOOLBAR_FILE :
		case ID_TOOLBAR_PLAYLIST :
		case ID_TOOLBAR_FAVOURITES :
		case ID_TOOLBAR_CONVERT :
		case ID_TOOLBAR_OPTIONS :
		case ID_TOOLBAR_INFO :
		case ID_TOOLBAR_EQ :
		case ID_TOOLBAR_CROSSFADE :
    case ID_TOOLBAR_LOUDNESS :
		case ID_TOOLBAR_TRACKEND :
		case ID_TOOLBAR_FLOW :
		case ID_TOOLBAR_PLAYBACK : {
			m_Rebar.ToggleToolbar( commandID );
			break;
		}
		case ID_TOOLBARSIZE_SMALL :
		case ID_TOOLBARSIZE_MEDIUM :
		case ID_TOOLBARSIZE_LARGE : {
			const Settings::ToolbarSize previousSize = m_Settings.GetToolbarSize();
			const Settings::ToolbarSize size = ( ID_TOOLBARSIZE_SMALL == commandID ) ? Settings::ToolbarSize::Small :
				( ( ID_TOOLBARSIZE_MEDIUM == commandID ) ? Settings::ToolbarSize::Medium : Settings::ToolbarSize::Large );
			if ( previousSize != size ) {
				m_Settings.SetToolbarSize( size );
				ResizeRebar();
			}
			break;
		}
		case ID_TOOLBAR_COLOUR_BUTTON : 
		case ID_TOOLBAR_COLOUR_BACKGROUND : {
			m_Rebar.OnSelectColour( commandID );
			break;
		}
		case IDOK : {
			if ( GetFocus() == m_List.GetWindowHandle() ) {
				m_List.PlaySelected();
			}
			break;
		}
		default : {
			if ( ( commandID >= MSG_TRAYMENUSTART ) && ( commandID <= MSG_TRAYMENUEND ) ) {
				m_Tray.OnContextMenuCommand( commandID );
			} else if ( ( commandID >= MSG_PLAYLISTMENUSTART ) && ( commandID <= MSG_PLAYLISTMENUEND ) ) {
				OnAddToPlaylist( commandID );
			}
			break;
		}
	}
}

void VUPlayer::OnInitMenu( const HMENU menu )
{
	if ( nullptr != menu ) {
		// File menu
		Playlist::Ptr playlist = m_List.GetPlaylist();
    const auto playlistType = playlist ? playlist->GetType() : Playlist::Type::_Undefined;

		const bool selectedItems = ( m_List.GetSelectedCount() > 0 );
		const UINT deletePlaylistEnabled = m_Tree.IsPlaylistDeleteEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_DELETEPLAYLIST, MF_BYCOMMAND | deletePlaylistEnabled );
		const UINT renamePlaylistEnabled = m_Tree.IsPlaylistRenameEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_RENAMEPLAYLIST, MF_BYCOMMAND | renamePlaylistEnabled );
		const UINT exportPlaylistEnabled = m_Tree.IsPlaylistExportEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_EXPORTPLAYLIST, MF_BYCOMMAND | exportPlaylistEnabled );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFOLDER, MF_BYCOMMAND | MF_ENABLED );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFILES, MF_BYCOMMAND | MF_ENABLED );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDSTREAM, MF_BYCOMMAND | MF_ENABLED );
		const UINT addToFavouritesEnabled = ( ( Playlist::Type::Favourites != playlistType ) && ( Playlist::Type::CDDA != playlistType ) && ( Playlist::Type::_Undefined != playlistType ) && selectedItems ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_ADDTOFAVOURITES, MF_BYCOMMAND | addToFavouritesEnabled );
		const UINT gainCalculatorEnabled = selectedItems ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_CALCULATEGAIN, MF_BYCOMMAND | gainCalculatorEnabled );
		const UINT refreshLibraryEnabled = m_Maintainer.IsActive() ? MF_DISABLED : MF_ENABLED;
		EnableMenuItem( menu, ID_FILE_REFRESHMEDIALIBRARY, MF_BYCOMMAND | refreshLibraryEnabled );
		const UINT musicbrainzEnabled = ( playlist && playlist->AllowMusicBrainzQueries() ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_MUSICBRAINZ_QUERY, MF_BYCOMMAND | musicbrainzEnabled );

		const int bufferSize = 256;
		WCHAR buffer[ bufferSize ] = {};
		const bool isCDDAPlaylist = ( Playlist::Type::CDDA == playlistType );
		if ( 0 != GetMenuString( menu, ID_FILE_CONVERT, buffer, bufferSize, MF_BYCOMMAND ) ) {
			const std::wstring originalMenuStr = buffer;
			LoadString( m_hInst, isCDDAPlaylist ? IDS_EXTRACT_TRACKS_MENU : IDS_CONVERT_TRACKS_MENU, buffer, bufferSize );
			const size_t accelDelimiter = originalMenuStr.find( '\t' );
			const std::wstring convertMenuStr = ( std::wstring::npos == accelDelimiter ) ? buffer : ( buffer + originalMenuStr.substr( accelDelimiter ) );
			ModifyMenu( menu, ID_FILE_CONVERT, MF_BYCOMMAND | MF_STRING, ID_FILE_CONVERT, convertMenuStr.c_str() );
		}
		const UINT convertEnabled = ( playlist && playlist->CanConvertAnyItems() ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_CONVERT, MF_BYCOMMAND | convertEnabled );

	  const UINT removeFilesEnabled = ( selectedItems && ( Playlist::Type::Folder != playlistType ) && ( Playlist::Type::CDDA != playlistType ) && ( Playlist::Type::_Undefined != playlistType ) ) ? MF_ENABLED : MF_DISABLED;
	  EnableMenuItem( menu, ID_FILE_PLAYLISTREMOVEFILES, MF_BYCOMMAND | removeFilesEnabled );

		// View menu
		CheckMenuItem( menu, ID_TREEMENU_FAVOURITES, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_FAVOURITES ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_STREAMS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_STREAMS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ALLTRACKS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ALLTRACKS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ARTISTS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ARTISTS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_PUBLISHERS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_PUBLISHERS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_COMPOSERS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_COMPOSERS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_CONDUCTORS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_CONDUCTORS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ALBUMS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ALBUMS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_GENRES, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_GENRES ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_YEARS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_YEARS ) ? MF_CHECKED : MF_UNCHECKED ) );

		const UINT trackInfoEnabled = ( m_List.GetCurrentSelectedIndex() >= 0 ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_VIEW_TRACKINFORMATION, MF_BYCOMMAND | trackInfoEnabled );
		const bool trackRemaining = m_Counter.GetTrackRemaining();
		CheckMenuItem( menu, ID_VIEW_COUNTER_TRACKREMAINING, trackRemaining ? ( MF_BYCOMMAND | MF_CHECKED ) : ( MF_BYCOMMAND | MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_VIEW_COUNTER_TRACKELAPSED, !trackRemaining ? ( MF_BYCOMMAND | MF_CHECKED ) : ( MF_BYCOMMAND | MF_UNCHECKED ) );
	
		LoadString( m_hInst, isCDDAPlaylist ? IDS_TOOLBAR_EXTRACT : IDS_TOOLBAR_CONVERT, buffer, bufferSize );
		ModifyMenu( menu, ID_TOOLBAR_CONVERT, MF_BYCOMMAND | MF_STRING, ID_TOOLBAR_CONVERT, buffer );
		CheckMenuItem( menu, ID_TOOLBAR_FILE, m_Settings.GetToolbarEnabled( m_ToolbarFile.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_PLAYLIST, m_Settings.GetToolbarEnabled( m_ToolbarPlaylist.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_FAVOURITES, m_Settings.GetToolbarEnabled( m_ToolbarFavourites.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_CONVERT, m_Settings.GetToolbarEnabled( m_ToolbarConvert.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_OPTIONS, m_Settings.GetToolbarEnabled( m_ToolbarOptions.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_INFO, m_Settings.GetToolbarEnabled( m_ToolbarInfo.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_EQ, m_Settings.GetToolbarEnabled( m_ToolbarEQ.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_CROSSFADE, m_Settings.GetToolbarEnabled( m_ToolbarCrossfade.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_LOUDNESS, m_Settings.GetToolbarEnabled( m_ToolbarLoudness.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_TRACKEND, m_Settings.GetToolbarEnabled( m_ToolbarTrackEnd.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_FLOW, m_Settings.GetToolbarEnabled( m_ToolbarFlow.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_PLAYBACK, m_Settings.GetToolbarEnabled( m_ToolbarPlayback.GetID() ) ? MF_CHECKED : MF_UNCHECKED );

		const auto toolbarSize = m_Settings.GetToolbarSize();
		CheckMenuItem( menu, ID_TOOLBARSIZE_SMALL, ( Settings::ToolbarSize::Small == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBARSIZE_MEDIUM, ( Settings::ToolbarSize::Medium == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBARSIZE_LARGE, ( Settings::ToolbarSize::Large == toolbarSize ) ? MF_CHECKED : MF_UNCHECKED );

		std::set<UINT> shownColumns;
		std::set<UINT> hiddenColumns;
		m_List.GetColumnVisibility( shownColumns, hiddenColumns );
		for ( const auto& hiddenColumn : hiddenColumns ) {
			CheckMenuItem( menu, hiddenColumn, MF_BYCOMMAND | MF_UNCHECKED );
		}
		for ( const auto& shownColumn : shownColumns ) {
			CheckMenuItem( menu, shownColumn, MF_BYCOMMAND | MF_CHECKED );
		}
		const UINT statusIconEnabled = m_List.GetStatusIconEnabled() ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_SHOWCOLUMNS_STATUS, MF_BYCOMMAND | statusIconEnabled );

		if ( ( 1 == shownColumns.size() ) && ( *shownColumns.begin() == ID_SHOWCOLUMNS_FILEPATH ) ) {
			// If the only visible column is the file path column, disable the option to hide it.
			EnableMenuItem( menu, ID_SHOWCOLUMNS_FILEPATH, MF_BYCOMMAND | MF_DISABLED );
		} else {
			EnableMenuItem( menu, ID_SHOWCOLUMNS_FILEPATH, MF_BYCOMMAND | MF_ENABLED );
		}

		const UINT currentVisual = m_Visual.GetCurrentVisualID();
		const std::set<UINT> visualIDs = m_Visual.GetVisualIDs();
		for ( const auto& visualID : visualIDs ) {
			if ( currentVisual == visualID ) {
				CheckMenuItem( menu, visualID, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, visualID, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}
		const float oscilloscopeWeight = m_Settings.GetOscilloscopeWeight();
		const std::map<UINT,float> oscilloscopeWeights = m_Visual.GetOscilloscopeWeights();
		for ( const auto& weight : oscilloscopeWeights ) {
			if ( oscilloscopeWeight == weight.second ) {
				CheckMenuItem( menu, weight.first, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, weight.first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}
		const float vumeterDecay = m_Settings.GetVUMeterDecay();
		const std::map<UINT,float> vumeterDecayRates = m_Visual.GetVUMeterDecayRates();
		for ( const auto& decay : vumeterDecayRates ) {
			if ( vumeterDecay == decay.second ) {
				CheckMenuItem( menu, decay.first, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, decay.first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}
		const UINT hardwareAccelerationChecked = m_Settings.GetHardwareAccelerationEnabled() ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_VISUAL_HARDWAREACCELERATION, MF_BYCOMMAND | hardwareAccelerationChecked ); 

		const WndTrackbar::Type trackbarType = m_VolumeControl.GetType();
		CheckMenuItem( menu, ID_VIEW_TRACKBAR_VOLUME, ( ( WndTrackbar::Type::Volume == trackbarType ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_VIEW_TRACKBAR_PITCH, ( ( WndTrackbar::Type::Pitch == trackbarType ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_VIEW_TRACKBAR_BALANCE, ( ( WndTrackbar::Type::Balance == trackbarType ) ? MF_CHECKED : MF_UNCHECKED ) );

		CheckMenuItem( menu, ID_VIEW_ALWAYSONTOP, m_Settings.GetAlwaysOnTop() ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_VIEW_EQ, m_EQ.IsVisible() ? MF_CHECKED : MF_UNCHECKED );

		// Control menu
		const Output::State outputState = m_Output.GetState();
		LoadString( m_hInst, ( Output::State::Playing == outputState ) ? IDS_CONTROL_PAUSEMENU : IDS_CONTROL_PLAYMENU, buffer, bufferSize );
		ModifyMenu( menu, ID_CONTROL_PLAY, MF_BYCOMMAND | MF_STRING, ID_CONTROL_PLAY, buffer );

		const UINT playEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_PLAY ) ? MF_ENABLED : MF_DISABLED;
		const UINT stopEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_STOP ) ? MF_ENABLED : MF_DISABLED;
		const UINT previousEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_PREVIOUSTRACK ) ? MF_ENABLED : MF_DISABLED;
		const UINT nextEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_NEXTTRACK ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_PLAY, MF_BYCOMMAND | playEnabled );
		EnableMenuItem( menu, ID_CONTROL_STOP, MF_BYCOMMAND | stopEnabled );
		EnableMenuItem( menu, ID_CONTROL_PREVIOUSTRACK, MF_BYCOMMAND | previousEnabled );
		EnableMenuItem( menu, ID_CONTROL_NEXTTRACK, MF_BYCOMMAND | nextEnabled );

		EnableMenuItem( menu, ID_CONTROL_PITCHRESET, ( ( m_Output.GetPitch() != 1.0f ) ? MF_ENABLED : MF_DISABLED ) );
		EnableMenuItem( menu, ID_CONTROL_BALANCERESET, ( ( m_Output.GetBalance() != 0 ) ? MF_ENABLED : MF_DISABLED ) );
		const Settings::PitchRange pitchRange = m_Settings.GetPitchRange();
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_SMALL, ( Settings::PitchRange::Small == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_MEDIUM, ( Settings::PitchRange::Medium == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_LARGE, ( Settings::PitchRange::Large == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );

		const bool isFollowTrackSelection = m_Output.GetFollowTrackSelection();
    const bool isStopAtTrackEnd = m_Output.GetStopAtTrackEnd();
		const bool isMuted = m_Output.GetMuted();
		const bool isFadeOut = m_Output.GetFadeOut();
		const bool isFadeToNext = m_Output.GetFadeToNext();
		const bool isCrossfade = m_Output.GetCrossfade();
		const bool isStream =	IsURL( m_Output.GetCurrentPlaying().PlaylistItem.Info.GetFilename() );

		const UINT enableStopAtTrackEnd = ( isFadeOut || isFadeToNext ) ? MF_DISABLED : MF_ENABLED;
    const UINT enableFollowTrackSelection = ( isFadeOut || isFadeToNext || isStopAtTrackEnd ) ? MF_DISABLED : MF_ENABLED;
		const UINT enableFadeOut = ( !isFadeToNext && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
		const UINT enableFadeToNext = ( !isFadeOut && !isStream && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
    EnableMenuItem( menu, ID_CONTROL_FOLLOWSELECTION, MF_BYCOMMAND | enableFollowTrackSelection );
		EnableMenuItem( menu, ID_CONTROL_STOPTRACKEND, MF_BYCOMMAND | enableStopAtTrackEnd );
		EnableMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | enableFadeOut );
		EnableMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | enableFadeToNext );

		const UINT checkStopAtTrackEnd = isStopAtTrackEnd ? MF_CHECKED : MF_UNCHECKED;
    const UINT checkFollowTrackSelection = isFollowTrackSelection ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkMuted = isMuted ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeOut = isFadeOut ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeToNext = isFadeToNext ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkCrossfade = isCrossfade ? MF_CHECKED : MF_UNCHECKED;
    const UINT checkLoudness = m_Output.GetLoudnessNormalisation() ? MF_CHECKED : MF_UNCHECKED;
    
    CheckMenuItem( menu, ID_CONTROL_FOLLOWSELECTION, MF_BYCOMMAND | checkFollowTrackSelection );
		CheckMenuItem( menu, ID_CONTROL_STOPTRACKEND, MF_BYCOMMAND | checkStopAtTrackEnd );
		CheckMenuItem( menu, ID_CONTROL_MUTE, MF_BYCOMMAND | checkMuted );
		CheckMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | checkFadeOut );
		CheckMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | checkFadeToNext );
		CheckMenuItem( menu, ID_CONTROL_CROSSFADE, MF_BYCOMMAND | checkCrossfade );
    CheckMenuItem( menu, ID_CONTROL_LOUDNESS, MF_BYCOMMAND | checkLoudness );

		const UINT enableSkip = ( Output::State::Stopped != outputState ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_SKIPBACKWARDS, MF_BYCOMMAND | enableSkip );
		EnableMenuItem( menu, ID_CONTROL_SKIPFORWARDS, MF_BYCOMMAND | enableSkip );

		const UINT randomPlay = m_Output.GetRandomPlay() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatTrack = m_Output.GetRepeatTrack() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatPlaylist = m_Output.GetRepeatPlaylist() ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_CONTROL_RANDOMPLAY, MF_BYCOMMAND | randomPlay );
		CheckMenuItem( menu, ID_CONTROL_REPEATTRACK, MF_BYCOMMAND | repeatTrack );
		CheckMenuItem( menu, ID_CONTROL_REPEATPLAYLIST, MF_BYCOMMAND | repeatPlaylist );

		UINT enableColourChoice = ( IsHighContrastActive() || IsClassicThemeActive() ) ? MF_DISABLED : MF_ENABLED;
		EnableMenuItem( menu, ID_TOOLBAR_COLOUR_BACKGROUND, MF_BYCOMMAND | enableColourChoice );

		enableColourChoice = IsHighContrastActive() ? MF_DISABLED : MF_ENABLED;
		EnableMenuItem( menu, ID_VIEW_COUNTER_FONTCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_TOOLBAR_COLOUR_BUTTON, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_TREEMENU_FONTCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_TREEMENU_BACKGROUNDCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_TREEMENU_HIGHLIGHTCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_TREEMENU_ICONCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_LISTMENU_FONTCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_LISTMENU_BACKGROUNDCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_LISTMENU_HIGHLIGHTCOLOUR, MF_BYCOMMAND | enableColourChoice );
		EnableMenuItem( menu, ID_LISTMENU_STATUSICONCOLOUR, MF_BYCOMMAND | enableColourChoice );

		InsertAddToPlaylists( GetSubMenu( menu, 0 ), ID_FILE_ADDTOFAVOURITES, true /*addPrefix*/ );
	}
}

void VUPlayer::OnMediaUpdated( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo )
{
	MediaInfo* previousInfo = new MediaInfo( previousMediaInfo );
	MediaInfo* updatedInfo = new MediaInfo( updatedMediaInfo );
	PostMessage( m_hWnd, MSG_MEDIAUPDATED, reinterpret_cast<WPARAM>( previousInfo ), reinterpret_cast<LPARAM>( updatedInfo ) );
}

void VUPlayer::OnHandleMediaUpdate( const MediaInfo* previousMediaInfo, const MediaInfo* updatedMediaInfo )
{
	if ( ( nullptr != previousMediaInfo ) && ( nullptr != updatedMediaInfo ) && ( previousMediaInfo->GetSource() == updatedMediaInfo->GetSource() ) ) {
		const Playlist::Set updatedPlaylists = m_Tree.OnUpdatedMedia( *previousMediaInfo, *updatedMediaInfo );
		const Playlist::Ptr currentPlaylist = m_List.GetPlaylist();
		if ( currentPlaylist && ( updatedPlaylists.end() != updatedPlaylists.find( currentPlaylist ) ) ) {
			m_List.OnUpdatedMedia( *updatedMediaInfo );
		}

		if ( m_Output.OnUpdatedMedia( *updatedMediaInfo ) ) {
			if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
				m_Splitter.Resize();
				m_Visual.DoRender();
			}
			if ( Output::State::Stopped != m_Output.GetState() ) {
				SetTitlebarText( m_Output.GetCurrentPlaying() );
			}
		}
	}
}

void VUPlayer::OnHandleLibraryRefreshed( const MediaInfo::List* removedFiles )
{
  if ( nullptr != removedFiles ) {
    m_Tree.OnRemovedMedia( *removedFiles, true );
    if ( const auto playlist = m_List.GetPlaylist(); playlist && Playlist::Type::All == playlist->GetType() ) {
      m_List.RefreshPlaylist();
      if ( !m_List.SelectPlaylistItem( m_Output.GetCurrentPlaying().PlaylistItem.ID ) ) {
        m_List.SelectPlaylistItem( playlist->GetItemID( 0 ) );
      }
      m_Status.Refresh();
    }
  }
}

void VUPlayer::OnHandleDiscRefreshed()
{
	const auto currentSelection = m_List.GetCurrentSelectedItem();
	
	m_Tree.OnRefreshDiscDrives();

	if ( const auto item = m_Output.GetCurrentPlaying(); MediaInfo::Source::CDDA == item.PlaylistItem.Info.GetSource() ) {
		if ( const auto& filename = item.PlaylistItem.Info.GetFilename(); !filename.empty() ) {
			const auto currentDrive = filename.front();
			const DiscManager::CDDAMediaMap cddaDrives = m_DiscManager.GetCDDADrives();
			bool stopPlayback = true;
			for ( const auto& [ drive, media ] : cddaDrives ) {
				if ( currentDrive == drive ) {
					stopPlayback = false;
					break;
				}
			}
			if ( stopPlayback ) {
				m_Output.Stop();
			}
		}
	}
	
	if ( MediaInfo::Source::CDDA == currentSelection.Info.GetSource() ) {
		if ( const auto playlist = m_List.GetPlaylist(); playlist && ( Playlist::Type::CDDA == playlist->GetType() ) ) {
			const auto items = playlist->GetItems();
			const auto foundItem = std::find_if( items.begin(), items.end(), [ currentSelection ] ( const Playlist::Item& item )
			{
				return currentSelection.Info.GetFilename() == item.Info.GetFilename();
			} );
			if ( items.end() != foundItem ) {
				m_List.SelectPlaylistItem( foundItem->ID );
			}
		}
	}
}

void VUPlayer::OnRestartPlayback( const long itemID )
{
	m_Output.Play( itemID );
}

void VUPlayer::OnTrackInformation()
{
	Playlist::Items selectedItems = m_List.GetSelectedPlaylistItems();
	if ( !selectedItems.empty() ) {
		DlgTrackInfo trackInfo( m_hInst, m_hWnd, m_Library, m_Settings, selectedItems );
		SetFocus( m_List.GetWindowHandle() );
	}
}

void VUPlayer::OnListSelectionChanged()
{
	const Playlist::Item currentSelectedPlaylistItem = m_List.GetCurrentSelectedItem();
	const Playlist::Item currentSelectedOutputItem = m_Output.GetCurrentSelectedPlaylistItem();
	m_Output.SetCurrentSelectedPlaylistItem( currentSelectedPlaylistItem );
  m_Output.SetPlaylistInformationToFollow( m_List.GetPlaylist(), m_List.GetSelectedPlaylistItems() );
	if ( ( Output::State::Stopped == m_Output.GetState() ) && ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) &&
			( currentSelectedPlaylistItem.Info.GetArtworkID( true /*checkFolder*/ ) != currentSelectedOutputItem.Info.GetArtworkID( true /*checkFolder*/ ) ) ) {
		m_Splitter.Resize();
		m_Visual.DoRender();
	}
}

COLORREF* VUPlayer::GetCustomColours()
{
	return m_CustomColours;
}

std::unique_ptr<Gdiplus::Bitmap> VUPlayer::LoadDefaultArtwork()
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	if ( const auto artworkPath = m_Settings.GetDefaultArtwork(); std::filesystem::exists( artworkPath ) ) {
		try {
			bitmap = std::make_unique<Gdiplus::Bitmap>( artworkPath.c_str() );
		} catch ( ... ) {}
	}
	if ( bitmap && ( ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) ) {
		bitmap.reset();
	}
	return bitmap;
}

std::unique_ptr<Gdiplus::Bitmap> VUPlayer::LoadResourcePNG( const WORD resourceID )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	HRSRC resource = FindResource( m_hInst, MAKEINTRESOURCE( resourceID ), L"PNG" );
	if ( nullptr != resource ) {
		HGLOBAL hGlobal = LoadResource( m_hInst, resource );
		if ( nullptr != hGlobal ) {
			LPVOID resourceBytes = LockResource( hGlobal );
			if ( nullptr != resourceBytes ) {
				const DWORD resourceSize = SizeofResource( m_hInst, resource );
				if ( resourceSize > 0 ) {
					IStream* stream = nullptr;
					if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
						if ( SUCCEEDED( stream->Write( resourceBytes, static_cast<ULONG>( resourceSize ), NULL /*bytesWritten*/ ) ) ) {
							try {
								bitmap = std::make_unique<Gdiplus::Bitmap>( stream );
							} catch ( ... ) {
							}
						}
						stream->Release();
					}				
				}
			}
		}
	}
	if ( bitmap && ( ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) ) {
		bitmap.reset();
	}
	return bitmap;
}

std::unique_ptr<Gdiplus::Bitmap> VUPlayer::GetPlaceholderImage( const bool useSettings )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap = useSettings ? LoadDefaultArtwork() : nullptr;
	if ( !bitmap ) {
		bitmap = LoadResourcePNG( IDB_PLACEHOLDER );
	}
	return bitmap;
}

void VUPlayer::OnOptions()
{
	const std::string previousScrobblerToken = m_Scrobbler.GetToken();
	const auto previousPlaceholderArtwork = m_Settings.GetDefaultArtwork();
	const COLORREF previousTaskbarButtonColour = m_Settings.GetTaskbarButtonColour();

	m_Hotkeys.Unregister();
	DlgOptions options( m_hInst, m_hWnd, m_Settings, m_Output );
	m_Hotkeys.Update();
	SetFocus( m_List.GetWindowHandle() );

	const std::string currentScrobblerToken = m_Scrobbler.GetToken();
	if ( ( previousScrobblerToken != currentScrobblerToken ) && !currentScrobblerToken.empty() ) {
		// Wake up the scrobbler, so that a session key can be requested.
		m_Scrobbler.WakeUp();
	}

	bool systrayEnable = false;
	bool systrayMinimise = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayTripleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayQuadClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnable, systrayMinimise, systraySingleClick, systrayDoubleClick, systrayTripleClick, systrayQuadClick );
	if ( !systrayEnable && m_Tray.IsShown() ) {
		m_Tray.Hide();
	} else if ( systrayEnable && !m_Tray.IsShown() ) {
		m_Tray.Show();
	}
	m_Tray.OnChangeSettings();

	m_Tree.SetMergeDuplicates( m_Settings.GetMergeDuplicates() );

	const auto placeholderArtwork = m_Settings.GetDefaultArtwork();
	if ( placeholderArtwork != previousPlaceholderArtwork ) {
		m_Visual.OnPlaceholderArtworkChanged();
	}
	if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
		m_Splitter.Resize();
		m_Visual.DoRender();
	}

	if ( const COLORREF taskbarButtonColour = m_Settings.GetTaskbarButtonColour(); taskbarButtonColour != previousTaskbarButtonColour ) {
		m_Taskbar.SetToolbarButtonColour( m_Settings );
	}
	m_Taskbar.EnableProgressBar( m_Settings.GetTaskbarShowProgress() );

  m_TitleBarFormat = m_Settings.GetTitleBarFormat();
}

Settings& VUPlayer::GetApplicationSettings()
{
	return m_Settings;
}

void VUPlayer::OnTrayNotify( WPARAM wParam, LPARAM lParam )
{
	m_Tray.OnNotify( wParam, lParam );
}

void VUPlayer::OnHotkey( WPARAM wParam )
{
	m_Hotkeys.OnHotkey( wParam );
}

void VUPlayer::OnCalculateGain()
{
	MediaInfo::List mediaList;
	const Playlist::Items selectedItems = m_List.GetSelectedPlaylistItems();
	m_GainCalculator.Calculate( selectedItems );
}

Playlist::Ptr VUPlayer::NewPlaylist()
{
	Playlist::Ptr playlist = m_Tree.NewPlaylist();
	return playlist;
}

Playlist::Ptr VUPlayer::GetSelectedPlaylist()
{
	Playlist::Ptr playlist = m_List.GetPlaylist();
	return playlist;
}

Playlist::Ptr VUPlayer::SelectStreamsPlaylist()
{
	Playlist::Ptr playlist = m_Tree.GetPlaylistStreams();
	m_Tree.SelectPlaylist( playlist );
	return playlist;
}

bool VUPlayer::AllowSkip() const
{
	LARGE_INTEGER perfFreq = {};
	LARGE_INTEGER perfCount = {};
	QueryPerformanceFrequency( &perfFreq );
	QueryPerformanceCounter( &perfCount );
	const float secondsSinceLastSkip = static_cast<float>( perfCount.QuadPart - m_LastSkipCount.QuadPart ) / perfFreq.QuadPart;
	const bool allowSkip = ( secondsSinceLastSkip > s_SkipLimitInterval ) || ( secondsSinceLastSkip < 0 );
	return allowSkip;
}

void VUPlayer::ResetLastSkipCount()
{
	QueryPerformanceCounter( &m_LastSkipCount );
}

std::wstring VUPlayer::GetBassVersion() const
{
	const std::wstring version = m_Handlers.GetBassVersion();
	return version;
}

void VUPlayer::OnAddToFavourites()
{
	Playlist::Ptr favourites = m_Tree.GetPlaylistFavourites();
	if ( favourites ) {
		const Playlist::Items selectedItems = m_List.GetSelectedPlaylistItems();
		for ( const auto& item : selectedItems ) {
			favourites->AddPending( item.Info );
		}
	}
}

void VUPlayer::OnAddToPlaylist( const UINT command )
{
	const auto playlistIter = m_AddToPlaylistMenuMap.find( command );
	if ( m_AddToPlaylistMenuMap.end() != playlistIter ) {
		Playlist::Ptr playlist = playlistIter->second;
		if ( playlist ) {
			const Playlist::Items selectedItems = m_List.GetSelectedPlaylistItems();
			for ( const auto& item : selectedItems ) {
				playlist->AddPending( item.Info );
			}
		}
	}
}

void VUPlayer::InsertAddToPlaylists( const HMENU menu, const UINT insertAfterItemID, const bool addPrefix )
{
	m_AddToPlaylistMenuMap.clear();
	UINT commandID = MSG_PLAYLISTMENUSTART;
	const int itemCount = GetMenuItemCount( menu );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		if ( insertAfterItemID == GetMenuItemID( menu, itemIndex ) ) {
			// Remove, if necessary, the previous Add to Playlist sub menu, and recreate.
			HMENU playlistMenu = GetSubMenu( menu, ++itemIndex );
			if ( nullptr != playlistMenu ) {
				DeleteMenu( menu, itemIndex, MF_BYPOSITION );
			}
			playlistMenu = CreatePopupMenu();
			if ( nullptr != playlistMenu ) {
				const Playlist::Ptr currentPlaylist = m_List.GetPlaylist();
				const bool enableMenu = ( m_List.GetSelectedCount() > 0 ) && currentPlaylist && ( Playlist::Type::CDDA != currentPlaylist->GetType() );
				if ( enableMenu ) {
					const Playlists playlists = m_Tree.GetUserPlaylists();
					for ( const auto& playlist : playlists ) {
						if ( playlist ) {
							m_AddToPlaylistMenuMap.insert( PlaylistMenuMap::value_type( commandID, playlist ) );
							AppendMenu( playlistMenu, MF_STRING, commandID, playlist->GetName().c_str() );
							if ( ++commandID > MSG_PLAYLISTMENUEND ) {
								break;
							}
						}		
					}
				}
				const int buffersize = 32;
				WCHAR buffer[ buffersize ] = {};
				LoadString( m_hInst, ( addPrefix ? IDS_ADDTOPLAYLISTPREFIXED : IDS_ADDTOPLAYLIST ), buffer, buffersize );
				InsertMenu( menu, itemIndex, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>( playlistMenu ), buffer );
				EnableMenuItem( menu, itemIndex, MF_BYPOSITION | ( enableMenu ? MF_ENABLED : MF_DISABLED ) );
			}
			break;
		}
	}
}

void VUPlayer::OnDeviceChange( const WPARAM wParam, const LPARAM lParam )
{
	PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>( lParam );
	if ( nullptr != hdr ) {
		if ( DBT_DEVTYP_VOLUME == hdr->dbch_devicetype ) {
			PDEV_BROADCAST_VOLUME volume = reinterpret_cast<PDEV_BROADCAST_VOLUME>( lParam );
			DWORD unitMask = volume->dbcv_unitmask;
			for ( wchar_t drive = 'A'; drive <= 'Z'; drive++ ) {
				if ( ( unitMask & 1 ) && ( ( DBT_DEVICEARRIVAL == wParam ) || ( DBT_DEVICEREMOVEPENDING == wParam ) || ( DBT_DEVICEREMOVECOMPLETE == wParam ) ) ) {
					const std::wstring driveString = std::wstring( 1, drive ) + L":\\";
					if ( DRIVE_CDROM == GetDriveType( driveString.c_str() ) ) {
						m_DiscManager.OnDeviceChange( drive, ( DBT_DEVICEARRIVAL == wParam ) ? DiscManager::ChangeType::Added : DiscManager::ChangeType::Removed );
					} else {
						if ( DBT_DEVICEARRIVAL == wParam ) {
							m_Tree.OnDriveArrived( drive );
						} else if ( ( DBT_DEVICEREMOVEPENDING == wParam ) || ( DBT_DEVICEREMOVECOMPLETE == wParam ) ) {
							m_Tree.OnDriveRemoved( drive );
						}
					}
				}
				unitMask >>= 1;
			}
		} else if ( ( DBT_DEVTYP_HANDLE == hdr->dbch_devicetype ) && ( DBT_DEVICEQUERYREMOVE == wParam ) ) {
			PDEV_BROADCAST_HANDLE handle = reinterpret_cast<PDEV_BROADCAST_HANDLE>( lParam );
			m_Tree.OnDeviceHandleRemoved( handle->dbch_handle );
		}
	}
}

void VUPlayer::OnConvert()
{
	Playlist::Ptr playlist = m_List.GetPlaylist();
	Playlist::Items itemList = playlist ? playlist->GetItems() : Playlist::Items();
	for ( auto itemIter = itemList.begin(); itemList.end() != itemIter; ) {
		if ( IsURL( itemIter->Info.GetFilename() ) ) {
			itemIter = itemList.erase( itemIter );
		} else {
			++itemIter;
		}
	}

	if ( !itemList.empty() ) {
		Playlist::Items selectedItems = m_List.GetSelectedPlaylistItems();
		for ( auto itemIter = selectedItems.begin(); selectedItems.end() != itemIter; ) {
			if ( IsURL( itemIter->Info.GetFilename() ) ) {
				itemIter = selectedItems.erase( itemIter );
			} else {
				++itemIter;
			}
		}
		if ( selectedItems.empty() ) {
			selectedItems = itemList;
		}

		DlgConvert dlgConvert( m_hInst, m_hWnd, m_Settings, m_Handlers, itemList, selectedItems );
		if ( !selectedItems.empty() ) {
			const Handler::Ptr handler = dlgConvert.GetSelectedHandler();
			if ( handler ) {
				m_Settings.SetEncoder( handler->GetDescription() );
				Playlist::Ptr outputPlaylist = m_Output.GetPlaylist();
				if ( ( Playlist::Type::CDDA == playlist->GetType() ) && outputPlaylist && ( Playlist::Type::CDDA == outputPlaylist->GetType() ) ) {
					m_Output.Stop();
				}
				m_IsConverting = true;
				const std::wstring& joinFilename = dlgConvert.GetJoinFilename();
				if ( Playlist::Type::CDDA == playlist->GetType() ) {
					CDDAExtract extract( m_hInst, m_hWnd, m_Library, m_Settings, m_Handlers, m_DiscManager, selectedItems, handler, joinFilename, m_Taskbar );
				} else {
					Converter converter( m_hInst, m_hWnd, m_Library, m_Settings, m_Handlers, selectedItems, handler, joinFilename, m_Taskbar );
				}
				m_IsConverting = false;
			}
		}
		SetFocus( m_List.GetWindowHandle() );
	}
}

bool VUPlayer::IsScrobblerAvailable()
{
	const bool available = m_Scrobbler.IsAvailable();
	return available;
}

void VUPlayer::ScrobblerAuthorise()
{
	m_Scrobbler.Authorise();
}

void VUPlayer::OnMusicBrainzQuery( Playlist* const cueList )
{
  if ( nullptr != cueList ) {
    if ( const auto id = CDDAMedia::GetMusicBrainzPlaylistID( cueList ) ) {
      const auto& [ discID, toc, startCues, backingFile ] = *id;
      m_MusicBrainz.Query( discID, toc, true /*forceDialog*/, cueList->GetID(), startCues, backingFile );
    }
  } else if ( const Playlist::Ptr playlist = m_List.GetPlaylist(); playlist ) {
    if ( Playlist::Type::CDDA == playlist->GetType() ) {
		  if ( const Playlist::Items playlistItems = playlist->GetItems(); !playlistItems.empty() ) {
			  const long cddbID = playlistItems.front().Info.GetCDDB();
			  const DiscManager::CDDAMediaMap drives = m_DiscManager.GetCDDADrives();
			  for ( const auto& drive : drives ) {
				  if ( cddbID == drive.second.GetCDDB() ) {
					  const auto [ discID, toc ] = drive.second.GetMusicBrainzDiscID();
					  m_MusicBrainz.Query( discID, toc, true /*forceDialog*/, playlist->GetID() );
					  break;
				  }
			  }
      }
    } else if ( const auto id = CDDAMedia::GetMusicBrainzPlaylistID( playlist.get() ) ) {
      const auto& [ discID, toc, startCues, backingFile ] = *id;
      m_MusicBrainz.Query( discID, toc, true /*forceDialog*/, playlist->GetID(), startCues, backingFile );
    }
	}
}

void VUPlayer::OnMusicBrainzResult( const MusicBrainz::Result& result, const bool forceDialog )
{
  // Note (to self) that although this method can display a modal dialog, 
  // it is still possible for this method to be called again on the main thread, before the dialog closes,
  // if another result is received and dispatched from the main message loop.
  // 
  // So, just guard this method to allow only one result to be processed at a time,
  // and queue up any other results received while the dialog box is (potentially) open, to be processed later.
  if ( m_IsProcessingMusicBrainzResult ) {
    m_PendingMusicBrainzResults.push( result );
    return;
  }
  m_IsProcessingMusicBrainzResult = true;

	const int selectedResult = ( ( 1 == result.Albums.size() ) && !forceDialog ) ? 0 : m_MusicBrainz.ShowMatchesDialog( result );
	if ( ( selectedResult >= 0 ) && ( selectedResult < static_cast<int>( result.Albums.size() ) ) ) {
    Playlist::Ptr playlist;
    
    const auto userPlaylists = m_Tree.GetUserPlaylists();
    for ( const auto& userPlaylist : userPlaylists ) {
      if ( userPlaylist && ( userPlaylist->GetID() == result.PlaylistID ) ) {
        playlist = userPlaylist;
        break;
      }
    }

    if ( !playlist )
      playlist = m_Tree.GetFolderPlaylist( result.PlaylistID );

    if ( !playlist ) {
		  const DiscManager::CDDAMediaMap drives = m_DiscManager.GetCDDADrives();
		  for ( const auto& drive : drives ) {
			  const auto [ discID, toc ] = drive.second.GetMusicBrainzDiscID();
			  if ( result.DiscID == discID ) {
			  	const CDDAMedia& cddaMedia = drive.second;
				  playlist = cddaMedia.GetPlaylist();
          break;
        }
      }
    }

		if ( playlist ) {
  		const MusicBrainz::Album& album = result.Albums[ selectedResult ];
			const Playlist::Items items = playlist->GetItems();
      const auto backingFile = WideStringToLower( result.BackingFile.value_or( std::wstring() ) );
			for ( const auto& item : items ) {
        auto matchingTrack = album.Tracks.end();
        if ( result.StartCues ) {
          const auto& itemStartCue = item.Info.GetCueStart();
          if ( itemStartCue && ( backingFile == WideStringToLower( item.Info.GetFilename() ) ) ) {
            if ( const auto matchingCue = result.StartCues->find( *itemStartCue ); result.StartCues->end() != matchingCue ) {
              matchingTrack = album.Tracks.find( 1 + static_cast<long>( std::distance( result.StartCues->begin(), matchingCue ) ) );
            }
          }          
        } else {
          matchingTrack = album.Tracks.find( item.Info.GetTrack() );
        }

			  if ( album.Tracks.end() != matchingTrack ) {
			    const MediaInfo previousMediaInfo( item.Info );
			    MediaInfo mediaInfo( item.Info );
			    mediaInfo.SetAlbum( album.Title );
			    mediaInfo.SetArtist( album.Artist );
			    mediaInfo.SetYear( album.Year );
          mediaInfo.SetPublisher( album.Label );
          mediaInfo.SetComposer( album.Composer );
          mediaInfo.SetConductor( album.Conductor );

			    mediaInfo.SetArtworkID( m_Library.AddArtwork( album.Artwork ) );

          const auto& [ trackTitle, trackArtist, trackYear ] = matchingTrack->second;
				  mediaInfo.SetTitle( trackTitle );
				  if ( !trackArtist.empty() ) {
					  mediaInfo.SetArtist( trackArtist );
				  }
				  if ( trackYear > 0 ) {
					  mediaInfo.SetYear( trackYear );
				  }

			    m_Library.UpdateMediaTags( previousMediaInfo, mediaInfo );
			  }
			}

      if ( ( Playlist::Type::User == playlist->GetType() ) && ( !album.Title.empty() && ( playlist->GetName() != album.Title ) && ( static_cast<size_t>( playlist->GetCount() ) == album.Tracks.size() ) ) ) {
        playlist->SetName( album.Title );
        m_Tree.RefreshUserPlaylistLabel( playlist );
      }
		}
	}

  m_IsProcessingMusicBrainzResult = false;
  if ( !m_PendingMusicBrainzResults.empty() ) {
    const auto pendingResult = m_PendingMusicBrainzResults.front();
    m_PendingMusicBrainzResults.pop();
    OnMusicBrainzResult( pendingResult, forceDialog );
  }
}

bool VUPlayer::IsMusicBrainzEnabled()
{
	return m_Settings.GetMusicBrainzEnabled();
}

HACCEL VUPlayer::GetAcceleratorTable() const
{
	return m_IsEditingLabel ? m_hAccelEditLabel : m_hAccel;
}

void VUPlayer::OnRemoveFromLibrary( const MediaInfo::List& mediaList )
{
	for ( const auto& mediaInfo : mediaList ) {
		m_Library.RemoveFromLibrary( mediaInfo );
	}
	m_Tree.OnRemovedMedia( mediaList );
}

bool VUPlayer::OnCommandLineFiles( const std::list<std::wstring>& filenames )
{
	bool validCommandLine = false;
	if ( !filenames.empty() ) {
		const std::set<std::wstring> trackExtensions = m_Handlers.GetAllSupportedFileExtensions();
		const std::set<std::wstring> playlistExtensions = Playlist::GetSupportedPlaylistExtensions();

		const std::wstring& firstFilename = filenames.front();
		std::wstring extension = GetFileExtension( firstFilename );
		const bool importPlaylist = ( playlistExtensions.end() != playlistExtensions.find( extension ) );

		if ( importPlaylist ) {
			m_Tree.ImportPlaylist( firstFilename );
			validCommandLine = true;
		} else {
			MediaInfo::List mediaList;
			for ( const auto& filename : filenames ) {
				extension = GetFileExtension( filename );
				if ( trackExtensions.end() != trackExtensions.find( extension ) ) {
					MediaInfo mediaInfo( filename );
					m_Library.GetMediaInfo( mediaInfo, false /*scanMedia*/, false /*sendNotification*/ );
					mediaList.push_back( mediaInfo );
				}
			}
			if ( !mediaList.empty() ) {
				const Playlist::Ptr scratchList = m_Tree.SetScratchList( mediaList );
				m_Output.Play( scratchList );
				validCommandLine = true;
			} else {
				if ( 1 == filenames.size() ) {
					const std::wstring& filename = filenames.front();
					if ( !filename.empty() ) {
						const UINT driveType = GetDriveType( filename.c_str() );
						if ( DRIVE_CDROM == driveType ) {
				      // Handle Audio CD autoplay
              if ( CDDAMedia::ContainsCDAudio( filename.front() ) ) {
							  const Playlist::Ptr playlist = m_Tree.SelectAudioCD( filename.front() );
							  if ( playlist ) {
								  m_Output.Play( playlist );
							  }
              }
						} else {
              std::error_code ec;
              if ( std::filesystem::is_directory( filename, ec ) ) {
                // Create a scratch list for the folder.
                std::set<std::filesystem::path> filepaths;
				        for ( const auto& entry : std::filesystem::recursive_directory_iterator( filename, ec ) ) {
					        if ( entry.is_regular_file( ec ) && !entry.is_directory( ec ) ) {
                    filepaths.insert( entry.path() );
					        }
				        }
                for ( const auto& filepath : filepaths ) {
				          MediaInfo mediaInfo( filepath );
				          m_Library.GetMediaInfo( mediaInfo, false /*scanMedia*/, false /*sendNotification*/ );
				          mediaList.push_back( mediaInfo );
                }
			          const Playlist::Ptr scratchList = m_Tree.SetScratchList( mediaList );
                m_Output.Play( scratchList );
                validCommandLine = true;
              }
            }
					}
				}
			}
		}
	}
	return validCommandLine;
}

void VUPlayer::SetTitlebarText( const Output::Item& item )
{
	std::wstring titlebarText = m_IdleText;

	if ( item.PlaylistItem.ID > 0 ) {
    std::wstring title;
    std::wstring artist;
    std::wstring composer;
		if ( !item.StreamTitle.empty() ) {
			title = item.StreamTitle;
		} else {
			title = item.PlaylistItem.Info.GetTitle( true /*filenameAsTitle*/ );
      artist = item.PlaylistItem.Info.GetArtist();
      composer = item.PlaylistItem.Info.GetComposer();
		}

    switch ( m_TitleBarFormat ) {
      case Settings::TitleBarFormat::ArtistTitle : {
        titlebarText = artist;
        if ( !titlebarText.empty() ) {
          titlebarText += L" - ";
        }
        titlebarText += title;
        break;
      }
      case Settings::TitleBarFormat::TitleArtist : {
        titlebarText = title;
        if ( !artist.empty() ) {
          titlebarText += L" - " + artist;
        }
        break;
      }
      case Settings::TitleBarFormat::ComposerTitle : {
        titlebarText = composer;
        if ( !titlebarText.empty() ) {
          titlebarText += L" - ";
        }
        titlebarText += title;
        break;
      }
      case Settings::TitleBarFormat::TitleComposer : {
        titlebarText = title;
        if ( !composer.empty() ) {
          titlebarText += L" - " + composer;
        }
        break;
      }
      case Settings::TitleBarFormat::Title : {
        titlebarText = title;
        break;
      }
      case Settings::TitleBarFormat::ApplicationName :
      default : {
        break;
      }
    }
  }

  if ( m_TitlebarText != titlebarText ) {
		m_TitlebarText = titlebarText;
		SetWindowText( m_hWnd, m_TitlebarText.c_str() );
	}
}

void VUPlayer::UpdateScrobbler( const Output::Item& previousItem, const Output::Item& currentItem )
{
	const time_t now = time( nullptr );
	m_Scrobbler.NowPlaying( currentItem.PlaylistItem.Info );
	if ( 0 != previousItem.PlaylistItem.ID ) {
		m_Scrobbler.Scrobble( previousItem.PlaylistItem.Info, m_LastOutputStateChange );
	}
	m_LastOutputStateChange = now;
}

void VUPlayer::SaveSettings()
{
	const Output::Item currentPlaying = m_Output.GetCurrentPlaying();
	Playlist::Ptr playlist = m_Output.GetPlaylist();
	MediaInfo info = currentPlaying.PlaylistItem.Info;
	if ( ( 0 == currentPlaying.PlaylistItem.ID ) || !playlist || ( Playlist::Type::_Undefined == playlist->GetType() ) ) {
		playlist = m_List.GetPlaylist();
		info = m_List.GetCurrentSelectedItem().Info;
	}

	m_Tree.SaveStartupPlaylist( playlist );

	m_Settings.SetStartupFile( info.GetFilename(), info.GetCueStart(), info.GetCueEnd() );

	m_Settings.SetVolume( m_Output.GetVolume() );
	m_Settings.SetPlaybackSettings( m_Output.GetRandomPlay(), m_Output.GetRepeatTrack(), m_Output.GetRepeatPlaylist(), m_Output.GetCrossfade() );
	m_Settings.SetOutputControlType( static_cast<int>( m_VolumeControl.GetType() ) );
  m_Settings.SetLoudnessNormalisation( m_Output.GetLoudnessNormalisation() );
  m_Settings.SetFollowTrackSelection( m_Output.GetFollowTrackSelection() );
	m_Settings.SetStopAtTrackEnd( m_Settings.GetRetainStopAtTrackEnd() ? m_Output.GetStopAtTrackEnd() : false );

	if ( m_Settings.GetRetainPitchBalance() ) {
		m_Settings.SetPitchBalance( m_Output.GetPitch(), m_Output.GetBalance() );
	} else {
		m_Settings.SetPitchBalance( 1.0f, 0 );
	}
}

HWND VUPlayer::GetEQ() const
{
	return m_EQ.GetWindowHandle();
}

void VUPlayer::ResizeRebar()
{
	m_Rebar.OnChangeSettings();
	m_Rebar.RearrangeItems();
	m_Splitter.Resize();
}

void VUPlayer::InitialiseRebar()
{
	m_Rebar.AddItem( &m_SeekControl );
	m_Rebar.AddItem( &m_Counter );
	m_Rebar.AddItem( &m_ToolbarFile );
	m_Rebar.AddItem( &m_ToolbarPlaylist );
	m_Rebar.AddItem( &m_ToolbarFavourites );
	m_Rebar.AddItem( &m_ToolbarConvert );
	m_Rebar.AddItem( &m_ToolbarOptions );
	m_Rebar.AddItem( &m_ToolbarInfo );
	m_Rebar.AddItem( &m_ToolbarEQ );
	m_Rebar.AddItem( &m_ToolbarCrossfade );
  m_Rebar.AddItem( &m_ToolbarLoudness );
	m_Rebar.AddItem( &m_ToolbarTrackEnd );
	m_Rebar.AddItem( &m_ToolbarFlow );
	m_Rebar.AddItem( &m_ToolbarPlayback );
	m_Rebar.AddItem( &m_ToolbarVolume );
	m_Rebar.AddItem( &m_VolumeControl );

	m_Rebar.RearrangeItems();
	m_Splitter.Resize();
	RedrawWindow( m_Rebar.GetWindowHandle(), NULL /*rect*/, NULL /*rgn*/, RDW_UPDATENOW );
}

void VUPlayer::ShowVolumeControlContextMenu( const POINT& position )
{
	m_VolumeControl.ShowContextMenu( position );
}

void VUPlayer::OnSysColorChange()
{
	m_IsHighContrast = IsHighContrastActive();
	m_Tree.OnSysColorChange( m_IsHighContrast );
	m_List.OnSysColorChange( m_IsHighContrast );
	m_Rebar.OnSysColorChange( m_IsHighContrast, IsClassicThemeActive() );
	m_Visual.OnSysColorChange();
}

void VUPlayer::OnPaint( const PAINTSTRUCT& ps )
{
	const std::array clipChildren = {
		m_List.GetWindowHandle(),
		m_Tree.GetWindowHandle(),
		m_Rebar.GetWindowHandle(),
		m_Status.GetWindowHandle(),
		m_Splitter.GetWindowHandle(),
		m_Visual.GetWindowHandle()
	};
	for ( const auto hwnd : clipChildren ) {
		RECT rect = {};
		GetWindowRect( hwnd, &rect );
		MapWindowRect( nullptr, m_hWnd, &rect );
		ExcludeClipRect( ps.hdc, rect.left, rect.top, rect.right, rect.bottom );
	}
	FillRect( ps.hdc, &ps.rcPaint, HBRUSH( COLOR_3DFACE + 1 ) );
}

void VUPlayer::OnTaskbarButtonCreated()
{
	m_Taskbar.Init( m_Settings );
}

HACCEL VUPlayer::CreateModifiedAcceleratorTable() const
{
	HACCEL modifiedAcceleratorTable = nullptr;
	if ( const int entries = CopyAcceleratorTable( m_hAccel, nullptr, 0 ); entries > 0 ) {
		std::vector<ACCEL> accelerators( entries );
		if ( const int copied = CopyAcceleratorTable( m_hAccel, accelerators.data(), entries ); copied == entries ) {
			constexpr std::array<ACCEL, 1> kIgnoredAccelerators = { { FVIRTKEY | FNOINVERT, VK_SPACE, ID_CONTROL_PLAY } };
			for ( auto accel = accelerators.begin(); accelerators.end() != accel; ) {
				const auto ignored = std::find_if( kIgnoredAccelerators.begin(), kIgnoredAccelerators.end(), [ accel ] ( const ACCEL& ignoredAccel ) 
					{
						return ( accel->fVirt == ignoredAccel.fVirt ) && ( accel->key == ignoredAccel.key ) && ( accel->cmd == ignoredAccel.cmd );
					} );
				if ( kIgnoredAccelerators.end() != ignored ) {
					accel = accelerators.erase( accel );
				} else {
					++accel;
				}
			}
			modifiedAcceleratorTable = CreateAcceleratorTable( accelerators.data(), static_cast<int>( accelerators.size() ) );
		}
	}
	return modifiedAcceleratorTable;
}

void VUPlayer::OnPowerBroadcast( const WPARAM type )
{
  if ( PBT_APMSUSPEND == type ) {
	  m_Output.Stop();
  }
}

void VUPlayer::OnDisplayChange()
{
  m_Visual.OnDisplayChange();
}
