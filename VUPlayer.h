#pragma once

#include "stdafx.h"

#include "resource.h"

#include "CDDAManager.h"
#include "Database.h"
#include "Gracenote.h"
#include "Hotkeys.h"
#include "Library.h"
#include "LibraryMaintainer.h"
#include "Output.h"
#include "ReplayGain.h"
#include "Settings.h"

#include "DlgEQ.h"

#include "WndCounter.h"
#include "WndList.h"
#include "WndRebar.h"
#include "WndSplit.h"
#include "WndStatus.h"
#include "WndToolbarConvert.h"
#include "WndToolbarCrossfade.h"
#include "WndToolbarEQ.h"
#include "WndToolbarFavourites.h"
#include "WndToolbarFile.h"
#include "WndToolbarFlow.h"
#include "WndToolbarInfo.h"
#include "WndToolbarOptions.h"
#include "WndToolbarPlayback.h"
#include "WndToolbarPlaylist.h"
#include "WndToolbarTrackEnd.h"
#include "WndTrackbarSeek.h"
#include "WndTrackbarVolume.h"
#include "WndTray.h"
#include "WndTree.h"
#include "WndVisual.h"

// Message ID for signalling that media information has been updated.
// 'wParam' : pointer to previous MediaInfo, to be deleted by the message handler.
// 'lParam' : pointer to updated MediaInfo, to be deleted by the message handler.
static const UINT MSG_MEDIAUPDATED = WM_APP + 77;

// Message ID for signalling that the media library has been refreshed.
// 'wParam' : unused.
// 'lParam' : unused.
static const UINT MSG_LIBRARYREFRESHED = WM_APP + 78;

// Message ID for signalling that the available CD audio discs has been refreshed.
// 'wParam' : unused.
// 'lParam' : unused.
static const UINT MSG_CDDAREFRESHED = WM_APP + 79;

// Message ID for signalling that a Gracenote result has arrived.
// 'wParam' : pointer to Gracenote::Result, to be deleted by the message handler.
// 'lParam' : non-zero to force a dialog to be shown even for an exact match.
static const UINT MSG_GRACENOTEQUERYRESULT = WM_APP + 80;

// Main application singleton
class VUPlayer
{
public:
	// Gets the main application instance.
	static VUPlayer* Get();

	// Returns (and creates if necessary) the VUPlayer documents folder (with a trailing slash).
	static std::wstring DocumentsFolder();

	// 'instance' - module instance handle.
	// 'hwnd' - main window handle.
	// 'startupFilenames' - tracks to play (or the playlist to open) on startup.
	VUPlayer( const HINSTANCE instance, const HWND hwnd, const std::list<std::wstring>& startupFilenames );

	virtual ~VUPlayer();

	// Called when the application window is resized.
	void OnSize( WPARAM wParam, LPARAM lParam );

	// Called when a notification message is received.
	// Returns true if the notification message is handled, in which case the 'result' parameter is valid.
	bool OnNotify( WPARAM wParam, LPARAM lParam, LRESULT& result );

	// Called when a timer message is received.
	// 'timerID' - timer identifier.
	// Returns true if the timer message was handled.
	bool OnTimer( const UINT_PTR timerID );

	// Called when the application window is about to be resized.
	void OnMinMaxInfo( LPMINMAXINFO minMaxInfo );

	// Called when the application window is about to be destroyed.
	void OnDestroy();

	// Called when the application window receives a command.
	void OnCommand( const int commandID );

	// Called when a 'menu' is about to be shown.
	void OnInitMenu( const HMENU menu );

	// Called when an 'item' is added to the 'playlist' at a (0-based) 'position'.
	void OnPlaylistItemAdded( Playlist* playlist, const Playlist::Item& item, const int position );

	// Called when an 'item' is removed from the 'playlist'.
	void OnPlaylistItemRemoved( Playlist* playlist, const Playlist::Item& item );

	// Called when an 'item' is updated in the 'playlist'.
	void OnPlaylistItemUpdated( Playlist* playlist, const Playlist::Item& item );

	// Called when information in the media database is updated.
	// 'previousMediaInfo' - the previous media information.
	// 'updatedMediaInfo' - the updated media information.
	void OnMediaUpdated( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo );

	// Called when the media library has been refreshed.
	void OnLibraryRefreshed();

	// Handles the update of 'previousMediaInfo' to 'updatedMediaInfo', from the main thread.
	void OnHandleMediaUpdate( const MediaInfo* previousMediaInfo, const MediaInfo* updatedMediaInfo );

	// Handles a media library refresh, from the main thread.
	void OnHandleLibraryRefreshed();

	// Handles the refreshing of available CD audio discs.
	void OnHandleCDDARefreshed();

	// Restarts playback from a playlist 'itemID'.
	void OnRestartPlayback( const long itemID );

	// Returns the custom colours to use for colour selection dialogs.
	COLORREF* GetCustomColours();

	// Returns the placeholder image.
	std::shared_ptr<Gdiplus::Bitmap> GetPlaceholderImage();

	// Returns the Gracenote logo, suitably resized for 'rect'.
	HBITMAP GetGracenoteLogo( const RECT& rect );

	// Returns the application settings.
	Settings& GetApplicationSettings();

	// Called when a notification area icon message is received.
	void OnTrayNotify( WPARAM wParam, LPARAM lParam );

	// Called when a hotkey message is received.
	void OnHotkey( WPARAM wParam );

	// Creates and returns a new playlist.
	Playlist::Ptr NewPlaylist();

	// Gets the currently displayed playlist.
	Playlist::Ptr GetSelectedPlaylist();

	// Returns the BASS library version.
	std::wstring GetBassVersion() const;

	// Inserts the Add to Playlists sub menu into the 'menu'.
	// 'addPrefix' - whether to add an alt prefix to the sub menu.
	void InsertAddToPlaylists( const HMENU menu, const bool addPrefix );

	// Processes a device change message.
	void OnDeviceChange( const WPARAM wParam, const LPARAM lParam );

	// Performs a Gracenote query for the current Audio CD.
	void OnGracenoteQuery();

	// Called when a Gracenote query is received.
	// 'result' - the query result.
	// 'forceDialog' - whether a dialog should be shown even for an exact match.
	void OnGracenoteResult( const Gracenote::Result& result, const bool forceDialog );

	// Returns whether Gracenote library is loaded.
	bool IsGracenoteAvailable();

	// Returns whether Gracenote is enabled.
	bool IsGracenoteEnabled();

	// Returns the accelerator table.
	HACCEL GetAcceleratorTable() const;

	// Removes the 'mediaList' from the media library and updates the tree control.
	void OnRemoveFromLibrary( const MediaInfo::List& mediaList );

	// Handles any 'filenames' passed in via the command line.
	// Returns whether any valid files were received.
	bool OnCommandLineFiles( const std::list<std::wstring>& filenames );

private:
	// Main application instance.
	static VUPlayer* s_VUPlayer;

	// Maps a menu command ID to a playlist.
	typedef std::map<UINT,Playlist::Ptr> PlaylistMenuMap;

	// Reads and applies windows position from settings.
	void ReadWindowSettings();

	// Writes windows position to settings.
	void WriteWindowSettings();

	// Handles a change in the current output 'item'.
	void OnOutputChanged( const Output::Item& item );

	// Called when the selection changes on the list control.
	void OnListSelectionChanged();

	// Displays the options dialog.
	void OnOptions();

	// Returns whether to allow a skip backwards or forwards.
	bool AllowSkip() const;

	// Resets the last skip count to the current performance count.
	void ResetLastSkipCount();

	// Shows the track information dialog.
	void OnTrackInformation();

	// Called when the Calculate ReplayGain command is received.
	void OnCalculateReplayGain();

	// Called when the Add to Favourites command is received.
	void OnAddToFavourites();

	// Called when the Add to Playlist 'command' is received.
	void OnAddToPlaylist( const UINT command );

	// Called when the convert tracks command is received.
	void OnConvert();

	// Loads a PNG resource and returns a bitmap.
	// Returns null if the resource was not loaded.
	std::shared_ptr<Gdiplus::Bitmap> LoadResourcePNG( const WORD resourceID );

	// Sets the application title bar text according to the 'item'.
	void SetTitlebarText( const Output::Item& item );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Application window handle.
	HWND m_hWnd;

	// Accelerator table.
	HACCEL m_hAccel;

	// Audio format handlers.
	Handlers m_Handlers;

	// Database.
	Database m_Database;

	// Media library.
	Library m_Library;

	// Media library maintainer.
	LibraryMaintainer m_Maintainer;

	// Application settings.
	Settings m_Settings;

	// Output.
	Output m_Output;

	// ReplayGain calculator.
	ReplayGain m_ReplayGain;

	// Gracenote manager.
	Gracenote m_Gracenote;

	// CD audio manager.
	CDDAManager m_CDDAManager;

	// Rebar control.
	WndRebar m_Rebar;

	// Status bar control.
	WndStatus m_Status;

	// Tree control.
	WndTree m_Tree;

	// Visual control.
	WndVisual m_Visual;

	// List control.
	WndList m_List;

	// Seek control.
	WndTrackbarSeek m_SeekControl;

	// Volume control.
	WndTrackbarVolume m_VolumeControl;

	// Toolbar (crossfade).
	WndToolbarCrossfade m_ToolbarCrossfade;

	// Toolbar (file).
	WndToolbarFile m_ToolbarFile;

	// Toolbar (random/repeat).
	WndToolbarFlow m_ToolbarFlow;

	// Toolbar (track information).
	WndToolbarInfo m_ToolbarInfo;

	// Toolbar (options).
	WndToolbarOptions m_ToolbarOptions;

	// Toolbar (playback).
	WndToolbarPlayback m_ToolbarPlayback;

	// Toolbar (playlist).
	WndToolbarPlaylist m_ToolbarPlaylist;

	// Toolbar (favourites).
	WndToolbarFavourites m_ToolbarFavourites;

	// Toolbar (EQ).
	WndToolbarEQ m_ToolbarEQ;

	// Toolbar (convert).
	WndToolbarConvert m_ToolbarConvert;

	// Toolbar (track end).
	WndToolbarTrackEnd m_ToolbarTrackEnd;

	// Counter control.
	WndCounter m_Counter;

	// Splitter control.
	WndSplit m_Splitter;

	// Notification area control.
	WndTray m_Tray;

	// EQ dialog.
	DlgEQ m_EQ;

	// The current output item.
	Output::Item m_CurrentOutput;

	// Custom colours for colour selection dialogs.
	COLORREF m_CustomColours[ 16 ];

	// Hotkeys.
	Hotkeys m_Hotkeys;

	// Performance count at which the last skip backwards or forwards occured.
	LARGE_INTEGER m_LastSkipCount;

	// Maps a menu command ID to a playlist for the Add to Playlist sub menu.
	PlaylistMenuMap m_AddToPlaylistMenuMap;
};