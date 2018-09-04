#pragma once

#include "Gracenote.h"
#include "LibraryMaintainer.h"
#include "Playlist.h"
#include "ReplayGain.h"

class WndStatus
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndStatus( HINSTANCE instance, HWND parent );

	virtual ~WndStatus();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Gets the status bar window handle.
	HWND GetWindowHandle();

	// Called when the control is resized to a new 'width'.
	void Resize( const int width );

	// Sets the current playlist.
	void SetPlaylist( const Playlist::Ptr playlist );

	// Called when an 'playlist' is updated.
	void Update( Playlist* playlist );

	// Updates the status text based on the 'replayGain', 'libraryMaintainer' & 'gracenote' state.
	void Update( const ReplayGain& replayGain, const LibraryMaintainer& libraryMaintainer, const Gracenote& gracenote );

	// Refreshes the status bar contents.
	void Refresh();

private:
	// Window procedure
	static LRESULT CALLBACK StatusProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Playlist.
	Playlist::Ptr m_Playlist;

	// Indicates the number of pending ReplayGain calculations currently displayed.
	int m_ReplayGainStatusCount;

	// Indicates the number of pending library maintenance items currently displayed.
	int m_LibraryStatusCount;

	// Indicates whether the Gracenote activity state is currently displayed.
	bool m_GracenoteActive;

	// The idle status text.
	std::wstring m_IdleText;
};

