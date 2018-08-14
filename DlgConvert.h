#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgConvert
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'tracks' - the available tracks to convert.
	DlgConvert( const HINSTANCE instance, const HWND parent, Settings& settings, const MediaInfo::List& tracks );

	virtual ~DlgConvert();

	// Returns the track selection after the dialog has been closed.
	const MediaInfo::List& GetSelectedTracks() const;

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Browse folder callback procedure.
	static int CALLBACK DlgConvert::BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Chooses the output folder location.
	void ChooseFolder();

	// Updates the folder control.
	void UpdateFolderControl();

	// Displays the filename format dialog.
	void OnFilenameFormat();

	// Called when the dialog is closed.
	// 'ok' - whether the dialog was okayed.
	void OnClose( const bool ok );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// The tracks to convert.
	MediaInfo::List m_Tracks;
};
