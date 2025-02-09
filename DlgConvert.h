#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgConvert
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'handlers' - audio format handlers.
	// 'tracks' - the available tracks to convert.
	// 'selectedTracks' - in/out, the selected tracks.
	DlgConvert( const HINSTANCE instance, const HWND parent, Settings& settings, Handlers& handlers, const Playlist::Items& tracks, Playlist::Items& selectedTracks );

	virtual ~DlgConvert();

	// Returns the selected encoder handler.
	const Handler::Ptr GetSelectedHandler() const;

	// Returns the output filename, if joining tracks into a single file.
	const std::wstring& GetJoinFilename() const;

private:
	// Maps an encoder description to its handle.
	typedef std::map<std::wstring, Handler::Ptr> EncoderMap;

	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Browse folder callback procedure.
	static int CALLBACK BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Chooses the output folder location.
	void ChooseFolder();

	// Updates the output folder control.
	void UpdateFolderControl();

	// Updates the currently selected encoder handler.
	void UpdateSelectedEncoder();

	// Updates the checked items in the track list.
	void UpdateCheckedItems( const int itemIndex );

	// Updates the output destination controls.
	void UpdateDestinationControls();

	// Displays the filename format dialog.
	void OnFilenameFormat();

	// Called when the dialog is closed.
	// 'ok' - whether the dialog was okayed.
	// Returns whether the dialog can be closed.
	bool OnClose( const bool ok );

	// Launches the encoder configuration dialog.
	void OnConfigure();

	// Returns the selected tracks.
	Playlist::Items GetSelectedTracks() const;

	// Returns the filename to use when joining files into a single track, or an empty string if a filename was not chosen.
	std::wstring ChooseJoinFilename() const;

	// Returns whether list control notifications are suppressed.
	bool GetSuppressNotifications() const;

	// Sets whether to 'suppress' list control notifications.
	void SetSuppressNotifications( const bool suppress );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Audio format handlers.
	Handlers& m_Handlers;

	// The available tracks.
	const Playlist::Items& m_Tracks;

	// The selected tracks to convert.
	Playlist::Items& m_SelectedTracks;

	// The set of checked items.
	std::set<long> m_CheckedItems;

	// Indicates whether list control notifications are suppressed.
	bool m_SuppressNotifications;

	// The encoder handler to use for conversion.
	Handler::Ptr m_SelectedEncoder;

	// The available encoders.
	EncoderMap m_Encoders;

	// The output filename, when joining tracks into a single file.
	std::wstring m_JoinFilename;
};
