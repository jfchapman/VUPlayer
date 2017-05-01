#pragma once

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
	void SetPlaylist( const Playlist::Ptr& playlist );

	// Called when an 'playlist' is updated.
	void Update( Playlist* playlist );

	// Updates the status text based on the 'replayGain' state.
	void Update( const ReplayGain& replayGain );

	// Refreshes the status bar contents.
	void Refresh();

private:
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

	// The idle status text.
	std::wstring m_IdleText;
};

