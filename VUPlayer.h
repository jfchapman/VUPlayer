#pragma once

#include "stdafx.h"

#include "resource.h"

#include "Database.h"
#include "DiscManager.h"
#include "GainCalculator.h"
#include "Hotkeys.h"
#include "Library.h"
#include "LibraryMaintainer.h"
#include "MusicBrainz.h"
#include "Output.h"
#include "Scrobbler.h"
#include "Settings.h"

#include "DlgEQ.h"

#include "WndCounter.h"
#include "WndList.h"
#include "WndRebar.h"
#include "WndRebarItem.h"
#include "WndSplit.h"
#include "WndStatus.h"
#include "WndTaskbar.h"
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
#include "WndToolbarVolume.h"
#include "WndTrackbarSeek.h"
#include "WndTrackbarVolume.h"
#include "WndTray.h"
#include "WndTree.h"
#include "WndVisual.h"

// Message ID for signalling that media information has been updated.
// 'wParam' : pointer to previous MediaInfo, to be deleted by the message handler.
// 'lParam' : pointer to updated MediaInfo, to be deleted by the message handler.
static constexpr UINT MSG_MEDIAUPDATED = WM_APP + 77;

// Message ID for signalling that media library maintenance has finished.
// 'wParam' : pointer to MediaInfo::List of items removed from the library, to be deleted by the message handler.
// 'lParam' : unused.
static constexpr UINT MSG_LIBRARYREFRESHED = WM_APP + 78;

// Message ID for signalling that the list of available optical discs has been refreshed.
// 'wParam' : unused.
// 'lParam' : unused.
static constexpr UINT MSG_DISCREFRESHED = WM_APP + 79;

// Message ID for signalling that a MusicBrainz result has arrived.
// 'wParam' : pointer to MusicBrainz::Result, to be deleted by the message handler.
// 'lParam' : non-zero to force a dialog to be shown even for a single match.
static constexpr UINT MSG_MUSICBRAINZQUERYRESULT = WM_APP + 80;

// Default icon colour.
static constexpr COLORREF DEFAULT_ICONCOLOUR = RGB( 0, 122, 217 );

// Main application singleton
class VUPlayer
{
public:
	// Gets the main application instance.
	static VUPlayer* Get();

	// Returns (and creates if necessary) the VUPlayer documents folder (with a trailing slash).
	static std::filesystem::path DocumentsFolder();

	// Returns the application folder.
	static std::filesystem::path ApplicationFolder();

	// 'instance' - module instance handle.
	// 'hwnd' - main window handle.
	// 'startupFilenames' - tracks to play (or the playlist to open) on startup.
	// 'portable' - whether to run in 'portable' mode (i.e. no persistent database).
	// 'databaseMode' - database access mode.
	VUPlayer( const HINSTANCE instance, const HWND hwnd, const std::list<std::wstring>& startupFilenames,
		const bool portable, const Database::Mode databaseMode );

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

  // Handles the update of 'previousMediaInfo' to 'updatedMediaInfo', from the main thread.
	void OnHandleMediaUpdate( const MediaInfo* previousMediaInfo, const MediaInfo* updatedMediaInfo );

  // Handles the updating of playlists when media library maintenance has finished, from the main thread.
  // 'removedFiles' - files that have been removed from the media library.
	void OnHandleLibraryRefreshed( const MediaInfo::List* removedFiles );

  // Handles the refreshing of available optical discs.
	void OnHandleDiscRefreshed();

	// Restarts playback from a playlist 'itemID'.
	void OnRestartPlayback( const long itemID );

	// Returns the custom colours to use for colour selection dialogs.
	COLORREF* GetCustomColours();

	// Returns the placeholder image.
	// 'useSettings' - returns the image specified in the application settings, if possible.
	std::unique_ptr<Gdiplus::Bitmap> GetPlaceholderImage( const bool useSettings = true );

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

	// Selects and returns the streams playlist.
	Playlist::Ptr SelectStreamsPlaylist();

	// Returns the BASS library version.
	std::wstring GetBassVersion() const;

	// Inserts the Add to Playlists sub menu into the 'menu'.
	// 'insertAfterItemID' - the menu item ID after which to add the sub menu.
	// 'addPrefix' - whether to add an alt prefix to the sub menu.
	void InsertAddToPlaylists( const HMENU menu, const UINT insertAfterItemID, const bool addPrefix );

	// Processes a device change message.
	void OnDeviceChange( const WPARAM wParam, const LPARAM lParam );

	// Returns whether Scrobbler library is loaded.
	bool IsScrobblerAvailable();

	// Launches a web browser to authorise VUPlayer for scrobbling.
	void ScrobblerAuthorise();

	// Returns the accelerator table.
	HACCEL GetAcceleratorTable() const;

	// Removes the 'mediaList' from the media library and updates the tree control.
	void OnRemoveFromLibrary( const MediaInfo::List& mediaList );

	// Handles any 'filenames' passed in via the command line.
	// Returns whether any valid files were received.
	bool OnCommandLineFiles( const std::list<std::wstring>& filenames );

	// Returns the EQ modeless dialog window handle.
	HWND GetEQ() const;

	// Performs a MusicBrainz query for the current Audio CD.
	void OnMusicBrainzQuery();

	// Called when a MusicBrainz query is received.
	// 'result' - the query result.
	// 'forceDialog' - whether a dialog should be shown even for a single match.
	void OnMusicBrainzResult( const MusicBrainz::Result& result, const bool forceDialog );

	// Returns whether MusicBrainz functionality is enabled.
	bool IsMusicBrainzEnabled();

	// Displays the context menu for the volume/pitch control at the specified 'position', in screen coordinates.
	void ShowVolumeControlContextMenu( const POINT& position );

	// Called on a system colour change event.
	void OnSysColorChange();

	// Called when the window needs painting with the 'ps'.
	void OnPaint( const PAINTSTRUCT& ps );

	// Called when the application taskbar button has been created.
	void OnTaskbarButtonCreated();

  // Called when a power management event 'type' has occurred.
  void OnPowerBroadcast( const WPARAM type );

private:
	// Main application instance.
	static VUPlayer* s_VUPlayer;

	// Maps a menu command ID to a playlist.
	using PlaylistMenuMap = std::map<UINT,Playlist::Ptr>;

	// Reads and applies windows position from settings.
	void ReadWindowSettings();

	// Writes windows position to settings.
	void WriteWindowSettings();

	// Handles a change in the current output from the 'previousItem' to the 'currentItem'.
	void OnOutputChanged( const Output::Item& previousItem, const Output::Item& currentItem );

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

	// Called when the Calculate Gain command is received.
	void OnCalculateGain();

	// Called when the Add to Favourites command is received.
	void OnAddToFavourites();

	// Called when the Add to Playlist 'command' is received.
	void OnAddToPlaylist( const UINT command );

	// Called when the convert tracks command is received.
	void OnConvert();

	// Loads the default artwork, defined in the application settings, and returns a bitmap.
	// Returns null if the artwork was not loaded.
	std::unique_ptr<Gdiplus::Bitmap> LoadDefaultArtwork();

	// Loads a PNG resource and returns a bitmap.
	// Returns null if the resource was not loaded.
	std::unique_ptr<Gdiplus::Bitmap> LoadResourcePNG( const WORD resourceID );

	// Sets the application title bar text according to the 'item'.
	void SetTitlebarText( const Output::Item& item );

	// Updates the scrobbler when the output state changes.
	// 'previousItem' - previous output item.
	// 'currentItem' - current output item.
	void UpdateScrobbler( const Output::Item& previousItem, const Output::Item& currentItem );

	// Saves various applications settings.
	void SaveSettings();

	// Resizes the rebar when the toolbar size changes.
	void ResizeRebar();

	// Initialises the rebar controls.
	void InitialiseRebar();

	// Creates a modified accelerator table to use when editing labels.
	HACCEL CreateModifiedAcceleratorTable() const;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Application window handle.
	HWND m_hWnd;

	// Accelerator table.
	const HACCEL m_hAccel;

	// Modified accelerator table to use when editing labels.
	const HACCEL m_hAccelEditLabel;

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

	// Gain calculator.
	GainCalculator m_GainCalculator;

	// Scrobbler manager.
	Scrobbler m_Scrobbler;

	// MusicBrainz manager.
	MusicBrainz m_MusicBrainz;

	// Optical disc manager.
	DiscManager m_DiscManager;

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

	// Toolbar (volume/pitch).
	WndToolbarVolume m_ToolbarVolume;

	// Counter control.
	WndCounter m_Counter;

	// Splitter control.
	WndSplit m_Splitter;

	// Notification area control.
	WndTray m_Tray;

	// Taskbar extension control.
	WndTaskbar m_Taskbar;

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

	// Timestamp of the last output state change.
	time_t m_LastOutputStateChange;

	// Maps a menu command ID to a playlist for the Add to Playlist sub menu.
	PlaylistMenuMap m_AddToPlaylistMenuMap;

	// The current title bar text.
	std::wstring m_TitlebarText;

	// The idle title bar text.
	std::wstring m_IdleText;

	// Whether high contrast mode is active.
	bool m_IsHighContrast;

	// Whether a tree item label is being edited.
	bool m_IsTreeLabelEdit;

	// A flag to allow first-time initialisation to be performed in the timer handler.
	bool m_IsFirstTimeStartup;

	// Whether a label is being edited.
	bool m_IsEditingLabel;

	// Whether the application is currently converting files.
	bool m_IsConverting;
};