#pragma once

#include "stdafx.h"

#include "Output.h"

class WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbar( HINSTANCE instance, HWND parent );

	virtual ~WndToolbar();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Returns the toolbar window handle.
	HWND GetWindowHandle() const;

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle() const;

	// Returns the toolbar height.
	int GetHeight() const;

	// Returns whether the button corresponding to 'commandID' is enabled.
	bool IsButtonEnabled( const UINT commandID ) const;

		// Returns whether the button corresponding to 'commandID' is checked.
	bool IsButtonChecked( const UINT commandID ) const;

	// Updates the tooolbar state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	virtual void Update( Output& output, const Playlist::Ptr& playlist, const Playlist::Item& selectedItem ) = 0;

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const = 0;

protected:
	// Sets whether the button corresponding to 'commandID' is 'enabled'.
	void SetButtonEnabled( const UINT commandID, const bool enabled );

	// Sets whether the button corresponding to 'commandID' is 'checked'.
	void SetButtonChecked( const UINT commandID, const bool checked );

private:
	// Next available child window ID.
	static UINT_PTR s_WndToolbarID;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;
};

