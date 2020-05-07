#include "VUPlayer.h"

#include "CDDAExtract.h"
#include "Converter.h"

#include "DlgConvert.h"
#include "DlgOptions.h"
#include "DlgTrackInfo.h"

#include "Utility.h"

#include <dbt.h>

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
		const bool portable, const std::string& portableSettings, const Database::Mode databaseMode ) :
	m_hInst( instance ),
	m_hWnd( hwnd ),
	m_hAccel( LoadAccelerators( m_hInst, MAKEINTRESOURCE( IDC_VUPLAYER ) ) ),
	m_Handlers(),
	m_Database( ( portable ? std::wstring() : ( DocumentsFolder() + s_Database ) ), databaseMode ),
	m_Library( m_Database, m_Handlers ),
	m_Maintainer( m_Library ),
	m_Settings( m_Database, m_Library, portableSettings ),
	m_Output( m_hWnd, m_Handlers, m_Settings, m_Settings.GetVolume() ),
	m_GainCalculator( m_Library, m_Handlers ),
	m_Gracenote( m_hInst, m_hWnd, m_Settings, portable /*disable*/ ),
	m_Scrobbler( m_Database, m_Settings, portable /*disable*/ ),
	m_CDDAManager( m_hInst, m_hWnd, m_Library, m_Handlers, m_Gracenote ),
	m_Rebar( m_hInst, m_hWnd, m_Settings ),
	m_Status( m_hInst, m_hWnd ),
	m_Tree( m_hInst, m_hWnd, m_Library, m_Settings, m_CDDAManager ),
	m_Visual( m_hInst, m_hWnd, m_Rebar.GetWindowHandle(), m_Status.GetWindowHandle(), m_Settings, m_Output, m_Library ),
	m_List( m_hInst, m_hWnd, m_Settings, m_Output ),
	m_SeekControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output ),
	m_VolumeControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output, m_Settings ),
	m_ToolbarCrossfade( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarFile( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarFlow( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarInfo( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarOptions( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarPlayback( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarPlaylist( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarFavourites( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarEQ( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarConvert( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarTrackEnd( m_hInst, m_Rebar.GetWindowHandle() ),
	m_Counter( m_hInst, m_Rebar.GetWindowHandle(), m_Settings, m_Output, m_ToolbarPlayback.GetHeight() - 1 ),
	m_Splitter( m_hInst, m_hWnd, m_Rebar.GetWindowHandle(), m_Status.GetWindowHandle(), m_Tree.GetWindowHandle(), m_Visual.GetWindowHandle(), m_List.GetWindowHandle(), m_Settings ),
	m_Tray( m_hInst, m_hWnd, m_Library, m_Settings, m_Output, m_Tree, m_List ),
	m_EQ( m_hInst, m_Settings, m_Output ),
	m_CurrentOutput(),
	m_CustomColours(),
	m_Hotkeys( m_hWnd, m_Settings ),
	m_LastSkipCount( {} ),
	m_LastOutputStateChange( 0 ),
	m_AddToPlaylistMenuMap()
{
	s_VUPlayer = this;

	ReadWindowSettings();

	m_Rebar.AddControl( m_SeekControl.GetWindowHandle(), false /*canHide*/ );
	m_Rebar.AddControl( m_Counter.GetWindowHandle(), false /*canHide*/ );
	m_Rebar.AddControl( m_ToolbarFile.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarPlaylist.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarFavourites.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarConvert.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarOptions.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarInfo.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarEQ.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarCrossfade.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarTrackEnd.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarFlow.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarPlayback.GetWindowHandle() );

	WndRebar::IconCallback iconCallback( [ &output = m_Output, &control = m_VolumeControl ]() -> UINT {
		const UINT iconID = ( control.GetType() == WndTrackbar::Type::Pitch ) ? IDI_PITCH : ( output.GetMuted() ? IDI_VOLUME_MUTE : IDI_VOLUME );
		return iconID;
	} );
	WndRebar::ClickCallback clickCallback( [ &output = m_Output, &control = m_VolumeControl ]( const bool rightClick ) -> void {
		if ( rightClick ) {
			POINT point = {};
			if ( GetCursorPos( &point ) ) {
				control.OnContextMenu( point );
			}
		} else {
			if ( WndTrackbar::Type::Volume == control.GetType() ) {
				output.ToggleMuted();
			}
		}
	} );
	m_Rebar.AddControl( m_VolumeControl.GetWindowHandle(), { IDI_VOLUME, IDI_VOLUME_MUTE, IDI_PITCH }, iconCallback, clickCallback, false /*canHide*/ );

	const int seekPosition = m_Rebar.GetPosition( m_SeekControl.GetWindowHandle() );
	if ( 0 != seekPosition ) {
		m_Rebar.MoveToStart( seekPosition );
	}

	m_Rebar.Init();

	m_Splitter.Resize();

	for ( auto& iter : m_CustomColours ) {
		iter = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	}

	RedrawWindow( m_Rebar.GetWindowHandle(), NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );

	m_Tree.Initialise();

	if ( OnCommandLineFiles( startupFilenames ) ) {
		m_List.SetPlaylist( m_Tree.GetSelectedPlaylist() );
	} else {
		const std::wstring initialFilename = m_Settings.GetStartupFilename();
		m_List.SetPlaylist( m_Tree.GetSelectedPlaylist(), false, initialFilename );
	}

	m_Status.SetPlaylist( m_List.GetPlaylist() );

	OnListSelectionChanged();

	m_EQ.Init( m_hWnd );

	m_Handlers.Init( m_Settings );

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

	bool systrayEnabled = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnabled, systraySingleClick, systrayDoubleClick );
	if ( minimised ) {
		ShowWindow( m_hWnd, SW_SHOWMINIMIZED );
	} else if ( maximised ) {
		ShowWindow( m_hWnd, SW_SHOWMAXIMIZED );
	} else {
		ShowWindow( m_hWnd, SW_SHOW );
	}
	if ( systrayEnabled ) {
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

std::wstring VUPlayer::DocumentsFolder()
{
	std::wstring folder;
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath( FOLDERID_Documents, KF_FLAG_DEFAULT, NULL /*token*/, &path );
	if ( SUCCEEDED( hr ) ) {
		folder = path;
		CoTaskMemFree( path );
		folder += L"\\VUPlayer\\";
		CreateDirectory( folder.c_str(), NULL /*attributes*/ ); 
	}
	return folder;
}

void VUPlayer::OnSize( WPARAM wParam, LPARAM lParam )
{
	if ( SIZE_MINIMIZED != wParam ) {
		SendMessage( m_Rebar.GetWindowHandle(), WM_SIZE, wParam, lParam );
		RedrawWindow( m_Rebar.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
		SendMessage( m_Status.GetWindowHandle(), WM_SIZE, wParam, lParam );
		RedrawWindow( m_Status.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
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
				handled = true;
				break;
			}
			case TVN_ENDLABELEDIT : {
				m_Tree.OnEndLabelEdit( wParam, lParam );
				break;
			}
			case TVN_SELCHANGED : {
				LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>( lParam );
				if ( ( nullptr != nmTreeView ) && ( nullptr != nmTreeView->itemNew.hItem ) ) {
					Playlist::Ptr playlist = m_Tree.GetPlaylist( nmTreeView->itemNew.hItem );
					m_List.SetPlaylist( playlist );
					m_Status.SetPlaylist( playlist );
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
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( nullptr != hdr ) {
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
								} else if ( m_CurrentOutput.PlaylistItem.ID == nmlvcd->nmcd.lItemlParam ) {
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
					}
				}
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
	bool handled = ( s_TimerID == timerID );
	if ( handled ) {
		Output::Item currentPlaying = m_Output.GetCurrentPlaying();
		if ( m_CurrentOutput.PlaylistItem.ID != currentPlaying.PlaylistItem.ID ) {
			OnOutputChanged( m_CurrentOutput, currentPlaying );
		}
		m_CurrentOutput = currentPlaying;

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
		m_ToolbarFlow.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarPlayback.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarTrackEnd.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarEQ.Update( m_EQ.IsVisible() );
		m_ToolbarConvert.Update( currentPlaylist );
		m_Rebar.Update();
		m_Counter.Refresh();
		m_Status.Update( m_GainCalculator, m_Maintainer, m_Gracenote );
		m_Tray.Update( m_CurrentOutput );
	}
	if ( !handled ) {
		handled = ( TIMER_SYSTRAY == timerID );
		if ( handled ) {
			KillTimer( m_hWnd, TIMER_SYSTRAY );
			m_Tray.OnSingleClick();
		}
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
	SetTitlebarText( currentItem );
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
				playlistAll->AddPending( item.Info.GetFilename() );
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
						const Playlist::Item item = m_List.GetCurrentSelectedItem();
						if ( 0 != item.ID ) {
							m_Output.SetPlaylist( playlist );
							m_Output.Play( item.ID );
						}
					}
					break;
				}
			}
			break;
		}
		case ID_CONTROL_STOP : {
			m_Output.Stop();
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
					float position = item.Position - s_SkipDuration;
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
					float position = item.Position + s_SkipDuration;
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
			m_Tree.NewPlaylist();
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
		case ID_FILE_PLAYLISTADDFOLDER : {
			const Playlist::Ptr playlist = m_List.GetPlaylist();
			if ( !playlist || ( playlist->GetType() != Playlist::Type::User ) ) {
				m_Tree.NewPlaylist();
			}
			m_List.OnCommandAddFolder();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			const Playlist::Ptr playlist = m_List.GetPlaylist();
			if ( !playlist || ( playlist->GetType() != Playlist::Type::User ) ) {
				m_Tree.NewPlaylist();
			}
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
		case ID_FILE_CUT : {
			m_List.OnCutCopy( true /*cut*/ );
			break;
		}
		case ID_FILE_COPY : {
			m_List.OnCutCopy( false /*cut*/ );
			break;
		}
		case ID_FILE_PASTE : {
			m_List.OnPaste();
			break;
		}
		case ID_FILE_SELECTALL : {
			m_List.SelectAllItems();
			break;
		}
		case ID_FILE_OPTIONS : {
			OnOptions();
			break;
		}
		case ID_FILE_REFRESHMEDIALIBRARY : {
			m_Maintainer.Start();
			break;
		}
		case ID_FILE_CONVERT : {
			OnConvert();
			break;
		}
		case ID_FILE_GRACENOTE_QUERY : {
			OnGracenoteQuery();
			break;
		}
		case ID_FILE_EXPORTSETTINGS : {
			OnExportSettings();
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
		case ID_VIEW_EQ : {
			m_EQ.ToggleVisibility();
			break;
		}
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
			m_List.OnShowColumn( commandID );
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
			m_List.OnSortPlaylist( commandID );
			break;
		}
		case ID_LISTMENU_FONTSTYLE : {
			m_List.OnSelectFont();
			break;
		}
		case ID_LISTMENU_FONTCOLOUR : 
		case ID_LISTMENU_BACKGROUNDCOLOUR :
		case ID_LISTMENU_HIGHLIGHTCOLOUR : {
			m_List.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_FONTSTYLE : {
			m_Tree.OnSelectFont();
			break;
		}
		case ID_TREEMENU_FONTCOLOUR : 
		case ID_TREEMENU_BACKGROUNDCOLOUR :
		case ID_TREEMENU_HIGHLIGHTCOLOUR : {
			m_Tree.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_FAVOURITES : {
			m_Tree.OnFavourites();
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
		case ID_BLING1 : 
		case ID_BLING2 :
		case ID_BLING3 :
		case ID_BLING4 : {
			const int bling = ( ID_BLING1 == commandID ) ? 1 : ( ( ID_BLING2 == commandID ) ? 2 : ( ( ID_BLING3 == commandID ) ? 3 : 4 ) );
			m_Output.Bling( bling );
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
		case ID_TOOLBAR_TRACKEND :
		case ID_TOOLBAR_FLOW :
		case ID_TOOLBAR_PLAYBACK : {
			m_Rebar.ToggleToolbar( commandID );
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
		const bool selectedItems = ( m_List.GetSelectedCount() > 0 );
		const UINT deletePlaylistEnabled = m_Tree.IsPlaylistDeleteEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_DELETEPLAYLIST, MF_BYCOMMAND | deletePlaylistEnabled );
		const UINT renamePlaylistEnabled = m_Tree.IsPlaylistRenameEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_RENAMEPLAYLIST, MF_BYCOMMAND | renamePlaylistEnabled );
		const UINT exportPlaylistEnabled = m_Tree.IsPlaylistExportEnabled() ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_EXPORTPLAYLIST, MF_BYCOMMAND | exportPlaylistEnabled );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFOLDER, MF_BYCOMMAND | MF_ENABLED );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFILES, MF_BYCOMMAND | MF_ENABLED );
		const UINT removeFilesEnabled = ( playlist && selectedItems && ( Playlist::Type::CDDA != playlist->GetType() ) && ( Playlist::Type::Folder != playlist->GetType() ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_PLAYLISTREMOVEFILES, MF_BYCOMMAND | removeFilesEnabled );
		const UINT addToFavouritesEnabled = ( playlist && ( Playlist::Type::Favourites != playlist->GetType() ) && ( Playlist::Type::CDDA != playlist->GetType() ) && selectedItems ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_ADDTOFAVOURITES, MF_BYCOMMAND | addToFavouritesEnabled );
		const UINT gainCalculatorEnabled = selectedItems ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_CALCULATEGAIN, MF_BYCOMMAND | gainCalculatorEnabled );
		const UINT refreshLibraryEnabled = ( 0 == m_Maintainer.GetPendingCount() ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_REFRESHMEDIALIBRARY, MF_BYCOMMAND | refreshLibraryEnabled );
		const UINT gracenoteEnabled = ( playlist && ( Playlist::Type::CDDA == playlist->GetType() ) && IsGracenoteEnabled() ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_GRACENOTE_QUERY, MF_BYCOMMAND | gracenoteEnabled );

		const int bufferSize = 64;
		WCHAR buffer[ bufferSize ] = {};
		const bool isCDDAPlaylist = playlist && ( Playlist::Type::CDDA == playlist->GetType() );
		if ( 0 != GetMenuString( menu, ID_FILE_CONVERT, buffer, bufferSize, MF_BYCOMMAND ) ) {
			LoadString( m_hInst, isCDDAPlaylist ? IDS_EXTRACT_TRACKS_MENU : IDS_CONVERT_TRACKS_MENU, buffer, bufferSize );
			std::wstring convertMenuTitle = buffer;
			const size_t accelDelimiter = convertMenuTitle.find( '\t' );
			if ( std::wstring::npos == accelDelimiter ) {
				convertMenuTitle = buffer;
			} else {
				convertMenuTitle = buffer + convertMenuTitle.substr( accelDelimiter );
			}
			ModifyMenu( menu, ID_FILE_CONVERT, MF_BYCOMMAND | MF_STRING, ID_FILE_CONVERT, convertMenuTitle.c_str() );
		}
		const UINT convertEnabled = ( playlist && ( playlist->GetCount() > 0 ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_CONVERT, MF_BYCOMMAND | convertEnabled );

		// View menu
		CheckMenuItem( menu, ID_TREEMENU_FAVOURITES, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_FAVOURITES ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ALLTRACKS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ALLTRACKS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ARTISTS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ARTISTS ) ? MF_CHECKED : MF_UNCHECKED ) );
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
		CheckMenuItem( menu, ID_TOOLBAR_TRACKEND, m_Settings.GetToolbarEnabled( m_ToolbarTrackEnd.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_FLOW, m_Settings.GetToolbarEnabled( m_ToolbarFlow.GetID() ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TOOLBAR_PLAYBACK, m_Settings.GetToolbarEnabled( m_ToolbarPlayback.GetID() ) ? MF_CHECKED : MF_UNCHECKED );

		std::set<UINT> shownColumns;
		std::set<UINT> hiddenColumns;
		m_List.GetColumnVisibility( shownColumns, hiddenColumns );
		for ( const auto& hiddenColumn : hiddenColumns ) {
			CheckMenuItem( menu, hiddenColumn, MF_BYCOMMAND | MF_UNCHECKED );
		}
		for ( const auto& shownColumn : shownColumns ) {
			CheckMenuItem( menu, shownColumn, MF_BYCOMMAND | MF_CHECKED );
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

		const WndTrackbar::Type trackbarType = m_VolumeControl.GetType();
		CheckMenuItem( menu, ID_VIEW_TRACKBAR_VOLUME, ( ( WndTrackbar::Type::Volume == trackbarType ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_VIEW_TRACKBAR_PITCH, ( ( WndTrackbar::Type::Pitch == trackbarType ) ? MF_CHECKED : MF_UNCHECKED ) );

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
		const Settings::PitchRange pitchRange = m_Settings.GetPitchRange();
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_SMALL, ( Settings::PitchRange::Small == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_MEDIUM, ( Settings::PitchRange::Medium == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CONTROL_PITCHRANGE_LARGE, ( Settings::PitchRange::Large == pitchRange ) ? MF_CHECKED : MF_UNCHECKED );

		const bool isStopAtTrackEnd = m_Output.GetStopAtTrackEnd();
		const bool isMuted = m_Output.GetMuted();
		const bool isFadeOut = m_Output.GetFadeOut();
		const bool isFadeToNext = m_Output.GetFadeToNext();
		const bool isCrossfade = m_Output.GetCrossfade();

		const UINT enableFadeOut = ( !isFadeToNext && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
		const UINT enableFadeToNext = ( !isFadeOut && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | enableFadeOut );
		EnableMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | enableFadeOut );

		const UINT checkStopAtTrackEnd = isStopAtTrackEnd ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkMuted = isMuted ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeOut = isFadeOut ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeToNext = isFadeToNext ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkCrossfade = isCrossfade ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_CONTROL_STOPTRACKEND, MF_BYCOMMAND | checkStopAtTrackEnd );
		CheckMenuItem( menu, ID_CONTROL_MUTE, MF_BYCOMMAND | checkMuted );
		CheckMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | checkFadeOut );
		CheckMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | checkFadeToNext );
		CheckMenuItem( menu, ID_CONTROL_CROSSFADE, MF_BYCOMMAND | checkCrossfade );

		const UINT enableSkip = ( Output::State::Stopped != outputState ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_SKIPBACKWARDS, MF_BYCOMMAND | enableSkip );
		EnableMenuItem( menu, ID_CONTROL_SKIPFORWARDS, MF_BYCOMMAND | enableSkip );

		const UINT randomPlay = m_Output.GetRandomPlay() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatTrack = m_Output.GetRepeatTrack() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatPlaylist = m_Output.GetRepeatPlaylist() ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_CONTROL_RANDOMPLAY, MF_BYCOMMAND | randomPlay );
		CheckMenuItem( menu, ID_CONTROL_REPEATTRACK, MF_BYCOMMAND | repeatTrack );
		CheckMenuItem( menu, ID_CONTROL_REPEATPLAYLIST, MF_BYCOMMAND | repeatPlaylist );

		InsertAddToPlaylists( GetSubMenu( menu, 0 ), true /*addPrefix*/ );
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

void VUPlayer::OnLibraryRefreshed()
{
	PostMessage( m_hWnd, MSG_LIBRARYREFRESHED, 0 /*wParam*/, 0 /*lParam*/ );
}

void VUPlayer::OnHandleLibraryRefreshed()
{
	m_Tree.OnMediaLibraryRefreshed();
}

void VUPlayer::OnHandleCDDARefreshed()
{
	m_Tree.OnCDDARefreshed();
}

void VUPlayer::OnRestartPlayback( const long itemID )
{
	m_Output.Play( itemID );
}

void VUPlayer::OnTrackInformation()
{
	Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
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

std::shared_ptr<Gdiplus::Bitmap> VUPlayer::LoadResourcePNG( const WORD resourceID )
{
	std::shared_ptr<Gdiplus::Bitmap> bitmapPtr;
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
								bitmapPtr = std::shared_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap( stream ) );
							} catch ( ... ) {
							}
						}
						stream->Release();
					}				
				}
			}
		}
	}
	if ( bitmapPtr && ( ( bitmapPtr->GetWidth() == 0 ) || ( bitmapPtr->GetHeight() == 0 ) ) ) {
		bitmapPtr.reset();
	}
	return bitmapPtr;
}

std::shared_ptr<Gdiplus::Bitmap> VUPlayer::GetPlaceholderImage()
{
	const std::shared_ptr<Gdiplus::Bitmap> bitmap = LoadResourcePNG( IDB_PLACEHOLDER );
	return bitmap;
}

HBITMAP VUPlayer::GetGracenoteLogo( const RECT& rect )
{
	HBITMAP hBitmap = nullptr;
	const std::shared_ptr<Gdiplus::Bitmap> gracenoteLogo = LoadResourcePNG( IDB_GRACENOTE );
	if ( gracenoteLogo ) {
		Gdiplus::Bitmap bitmap( rect.right - rect.left, rect.bottom - rect.top, PixelFormat32bppARGB );
		Gdiplus::Graphics graphics( &bitmap );
		graphics.SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBicubic );
		const Gdiplus::RectF destRect( 0, 0, static_cast<Gdiplus::REAL>( bitmap.GetWidth() ), static_cast<Gdiplus::REAL>( bitmap.GetHeight() ) );
		const Gdiplus::RectF logoRect( 0, 0, static_cast<Gdiplus::REAL>( gracenoteLogo->GetWidth() ), static_cast<Gdiplus::REAL>( gracenoteLogo->GetHeight() ) );
		if ( ( destRect.Width > 0 ) && ( destRect.Height > 0 ) && ( logoRect.Width > 0 ) && ( logoRect.Height > 0 ) ) {
			Gdiplus::REAL x = 0;
			Gdiplus::REAL y = 0;
			Gdiplus::REAL width = 0;
			Gdiplus::REAL height = 0;
			const Gdiplus::REAL destAspectRatio = destRect.Width / destRect.Height;
			const Gdiplus::REAL logoAspectRatio = logoRect.Width / logoRect.Height;
			if ( destAspectRatio < logoAspectRatio ) {
				width = destRect.Width;
				height = destRect.Width / logoAspectRatio;
				y = ( destRect.Height - height ) / 2;
			} else {
				height = destRect.Height;
				width = destRect.Height * logoAspectRatio;
				x = ( destRect.Width - width ) / 2;
			}
			if ( Gdiplus::Ok == graphics.DrawImage( gracenoteLogo.get(), x, y, width, height ) ) {
				const Gdiplus::Color background( static_cast<Gdiplus::ARGB>( Gdiplus::Color::Transparent ) );
				bitmap.GetHBITMAP( background, &hBitmap );
			}
		}
	}
	return hBitmap;
}

void VUPlayer::OnOptions()
{
	const std::string previousScrobblerToken = m_Scrobbler.GetToken();

	m_Hotkeys.Unregister();
	DlgOptions options( m_hInst, m_hWnd, m_Settings, m_Output );
	m_Hotkeys.Update();

	const std::string currentScrobblerToken = m_Scrobbler.GetToken();
	if ( ( previousScrobblerToken != currentScrobblerToken ) && !currentScrobblerToken.empty() ) {
		// Wake up the scrobbler, so that a session key can be requested.
		m_Scrobbler.WakeUp();
	}

	bool systrayEnable = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnable, systraySingleClick, systrayDoubleClick );
	if ( !systrayEnable && m_Tray.IsShown() ) {
		m_Tray.Hide();
	} else if ( systrayEnable && !m_Tray.IsShown() ) {
		m_Tray.Show();
	}

	m_Tree.SetMergeDuplicates( m_Settings.GetMergeDuplicates() );
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
	const Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
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
		const Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
		for ( const auto& item : selectedItems ) {
			favourites->AddPending( item.Info.GetFilename() );
		}
	}
}

void VUPlayer::OnAddToPlaylist( const UINT command )
{
	const auto playlistIter = m_AddToPlaylistMenuMap.find( command );
	if ( m_AddToPlaylistMenuMap.end() != playlistIter ) {
		Playlist::Ptr playlist = playlistIter->second;
		if ( playlist ) {
			const Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
			for ( const auto& item : selectedItems ) {
				playlist->AddPending( item.Info.GetFilename() );
			}
		}
	}
}

void VUPlayer::InsertAddToPlaylists( const HMENU menu, const bool addPrefix )
{
	m_AddToPlaylistMenuMap.clear();
	UINT commandID = MSG_PLAYLISTMENUSTART;
	const int itemCount = GetMenuItemCount( menu );
	for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
		if ( ID_FILE_ADDTOFAVOURITES == GetMenuItemID( menu, itemIndex ) ) {
			// Remove, if necessary, the previous Add to Playlists sub menu, and recreate.
			HMENU playlistMenu = GetSubMenu( menu, ++itemIndex );
			if ( nullptr != playlistMenu ) {
				DeleteMenu( menu, itemIndex, MF_BYPOSITION );
			}
			playlistMenu = CreatePopupMenu();
			if ( nullptr != playlistMenu ) {
				const Playlist::Ptr currentPlaylist = m_List.GetPlaylist();
				const bool enableMenu = ( m_List.GetSelectedCount() > 0 ) && currentPlaylist && ( Playlist::Type::CDDA != currentPlaylist->GetType() );
				if ( enableMenu ) {
					const Playlists playlists = m_Tree.GetPlaylists();
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
				const Output::State state = m_Output.GetState();
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
	m_CDDAManager.OnDeviceChange();

	PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>( lParam );
	if ( nullptr != hdr ) {
		if ( DBT_DEVTYP_VOLUME == hdr->dbch_devicetype ) {
			PDEV_BROADCAST_VOLUME volume = reinterpret_cast<PDEV_BROADCAST_VOLUME>( lParam );
			DWORD unitMask = volume->dbcv_unitmask;
			for ( wchar_t drive = 'A'; drive <= 'Z'; drive++ ) {
				if ( unitMask & 1 ) {
					if ( DBT_DEVICEARRIVAL == wParam ) {
						m_Tree.OnDriveArrived( drive );
					} else if ( ( DBT_DEVICEREMOVEPENDING == wParam ) || ( DBT_DEVICEREMOVECOMPLETE == wParam ) ) {
						m_Tree.OnDriveRemoved( drive );
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
	const Playlist::ItemList itemList = playlist ? playlist->GetItems() : Playlist::ItemList();
	if ( !itemList.empty() ) {
		Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
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

				const std::wstring& joinFilename = dlgConvert.GetJoinFilename();
				if ( Playlist::Type::CDDA == playlist->GetType() ) {
					CDDAExtract extract( m_hInst, m_hWnd, m_Library, m_Settings, m_Handlers, m_CDDAManager, selectedItems, handler, joinFilename );
				} else {
					Converter converter( m_hInst, m_hWnd, m_Library, m_Settings, m_Handlers, selectedItems, handler, joinFilename );
				}
			}
		}
	}
}

void VUPlayer::OnGracenoteQuery()
{
	const Playlist::Ptr playlist = m_List.GetPlaylist();
	if ( playlist && ( Playlist::Type::CDDA == playlist->GetType() ) ) {
		const Playlist::ItemList playlistItems = playlist->GetItems();
		if ( !playlistItems.empty() ) {
			const long cddbID = playlistItems.front().Info.GetCDDB();
			const CDDAManager::CDDAMediaMap drives = m_CDDAManager.GetCDDADrives();
			for ( const auto& drive : drives ) {
				if ( cddbID == drive.second.GetCDDB() ) {
					const std::string toc = drives.begin()->second.GetGracenoteTOC();
					m_Gracenote.Query( toc, true /*forceDialog*/ );
					break;
				}
			}
		}
	}
}

void VUPlayer::OnGracenoteResult( const Gracenote::Result& result, const bool forceDialog )
{
	const int selectedResult = ( result.ExactMatch && !forceDialog ) ? 0 : m_Gracenote.ShowMatchesDialog( result );
	if ( ( selectedResult >= 0 ) && ( selectedResult < static_cast<int>( result.Albums.size() ) ) ) {
		const Gracenote::Album& album = result.Albums[ selectedResult ];
		const CDDAManager::CDDAMediaMap drives = m_CDDAManager.GetCDDADrives();
		for ( const auto& drive : drives ) {
			if ( result.TOC == drive.second.GetGracenoteTOC() ) {
				const CDDAMedia& cddaMedia = drive.second;
				const Playlist::Ptr playlist = cddaMedia.GetPlaylist();
				if ( playlist ) {
					const Playlist::ItemList items = playlist->GetItems();
					for ( const auto& item : items ) {
						const MediaInfo previousMediaInfo( item.Info );
						MediaInfo mediaInfo( item.Info );
						mediaInfo.SetAlbum( album.Title );
						mediaInfo.SetArtist( album.Artist );
						mediaInfo.SetGenre( album.Genre );
						mediaInfo.SetYear( album.Year );

						mediaInfo.SetArtworkID( m_Library.AddArtwork( album.Artwork ) );

						const auto trackIter = album.Tracks.find( mediaInfo.GetTrack() );
						if ( album.Tracks.end() != trackIter ) {
							const Gracenote::Track& track = trackIter->second;
							mediaInfo.SetTitle( track.Title );
							if ( !track.Artist.empty() ) {
								mediaInfo.SetArtist( track.Artist );
							}
							if ( !track.Genre.empty() ) {
								mediaInfo.SetGenre( track.Genre );
							}
							if ( 0 != track.Year ) {
								mediaInfo.SetYear( track.Year );
							}
						}
						m_Library.UpdateMediaTags( previousMediaInfo, mediaInfo );
					}
				}
				break;
			}
		}
	}
}

bool VUPlayer::IsGracenoteAvailable()
{
	const bool available = m_Gracenote.IsAvailable();
	return available;
}

bool VUPlayer::IsGracenoteEnabled()
{
	bool enabled = IsGracenoteAvailable();
	if ( enabled ) {
		std::string userID;
		bool enableLog = true;
		m_Settings.GetGracenoteSettings( userID, enabled, enableLog );
	}
	return enabled;
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

HACCEL VUPlayer::GetAcceleratorTable() const
{
	return m_hAccel;
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
		const std::set<std::wstring> playlistExtensions = { L"vpl", L"m3u", L"pls" };

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
					m_Library.GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ );
					mediaList.push_back( mediaInfo );
				}
			}
			if ( !mediaList.empty() ) {
				const Playlist::Ptr scratchList = m_Tree.SetScratchList( mediaList );
				m_Output.SetPlaylist( scratchList );
				m_Output.Play();
				validCommandLine = true;
			} else {
				// Handle Audio CD autoplay
				if ( 1 == filenames.size() ) {
					const std::wstring& filename = filenames.front();
					if ( !filename.empty() ) {
						const UINT driveType = GetDriveType( filename.c_str() );
						if ( ( DRIVE_CDROM == driveType ) && CDDAMedia::ContainsCDAudio( filename.front() ) ) {
							const Playlist::Ptr playlist = m_Tree.SelectAudioCD( filename.front() );
							if ( playlist ) {
								m_Output.SetPlaylist( playlist );
								m_Output.Play();
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
	std::wstring titlebarText;
	if ( item.PlaylistItem.ID > 0 ) {
		const std::wstring title = item.PlaylistItem.Info.GetTitle( true /*filenameAsTitle*/ );
		if ( !title.empty() ) {
			titlebarText = item.PlaylistItem.Info.GetArtist();
			if ( !titlebarText.empty() ) {
				titlebarText += L" - ";
			}
			titlebarText += title;
		} 
	}
	if ( titlebarText.empty() ) {
		const int buffersize = 32;
		WCHAR buffer[ buffersize ] = {};
		LoadString( m_hInst, IDS_APP_TITLE, buffer, buffersize );
		titlebarText = buffer;
	}
	SetWindowText( m_hWnd, titlebarText.c_str() );
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

	const std::wstring& filename = ( MediaInfo::Source::File == info.GetSource() ) ? info.GetFilename() : std::wstring(); 
	m_Settings.SetStartupFilename( filename );

	m_Settings.SetVolume( m_Output.GetVolume() );
	m_Settings.SetPlaybackSettings( m_Output.GetRandomPlay(), m_Output.GetRepeatTrack(), m_Output.GetRepeatPlaylist(), m_Output.GetCrossfade() );
	m_Settings.SetOutputControlType( static_cast<int>( m_VolumeControl.GetType() ) );
}

void VUPlayer::OnExportSettings()
{
	std::string settings;

	// Ensure all database settings are up to date, before exporting.
	m_List.SaveSettings();
	m_Tree.SaveSettings();
	m_EQ.SaveSettings();
	WriteWindowSettings();
	SaveSettings();
	m_Counter.SaveSettings();

	m_Settings.ExportSettings( settings );
	if ( !settings.empty() ) {
		WCHAR title[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_EXPORTSETTINGS_TITLE, title, MAX_PATH );

		WCHAR filter[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_EXPORTSETTINGS_FILTER, filter, MAX_PATH );
		const std::wstring filter1( filter );
		const std::wstring filter2( L"*.ini" );
		std::vector<WCHAR> filterStr;
		filterStr.reserve( MAX_PATH );
		filterStr.insert( filterStr.end(), filter1.begin(), filter1.end() );
		filterStr.push_back( 0 );
		filterStr.insert( filterStr.end(), filter2.begin(), filter2.end() );
		filterStr.push_back( 0 );
		filterStr.push_back( 0 );

		WCHAR buffer[ MAX_PATH ] = {};
		LoadString( m_hInst, IDS_EXPORTSETTINGS_DEFAULT, buffer, MAX_PATH );

		OPENFILENAME ofn = {};
		ofn.lStructSize = sizeof( OPENFILENAME );
		ofn.hwndOwner = m_hWnd;
		ofn.lpstrTitle = title;
		ofn.lpstrFilter = &filterStr[ 0 ];
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = MAX_PATH;
		if ( FALSE != GetSaveFileName( &ofn ) ) {
			try {
				std::ofstream fileStream;
				fileStream.open( ofn.lpstrFile, std::ios::out | std::ios::trunc );
				if ( fileStream.is_open() ) {
					fileStream << settings;
					fileStream.close();
				}
			} catch ( ... ) {
			}
		}
	}
}
