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
	DlgConvert( const HINSTANCE instance, const HWND parent, Settings& settings, Handlers& handlers, const Playlist::ItemList& tracks, Playlist::ItemList& selectedTracks );

	virtual ~DlgConvert();

	// Returns the selected encoder handler.
	const Handler::Ptr GetSelectedHandler() const;

private:
	// Maps an encoder description to its handle.
	typedef std::map<std::wstring,Handler::Ptr> EncoderMap;

	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Browse folder callback procedure.
	static int CALLBACK DlgConvert::BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData );

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

	// Displays the filename format dialog.
	void OnFilenameFormat();

	// Called when the dialog is closed.
	// 'ok' - whether the dialog was okayed.
	void OnClose( const bool ok );

	// Launches the encoder configuration dialog.
	void OnConfigure();

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Audio format handlers.
	Handlers& m_Handlers;

	// The available tracks.
	const Playlist::ItemList& m_Tracks;

	// The selected tracks to convert.
	Playlist::ItemList& m_SelectedTracks;

	// The set of checked items.
	std::set<long> m_CheckedItems;

	// Indicates whether the dialog has been initialised.
	bool m_Initialised;

	// The encoder handler to use for conversion.
	Handler::Ptr m_SelectedEncoder;

	// The available encoders.
	EncoderMap m_Encoders;
};
