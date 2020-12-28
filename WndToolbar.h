#pragma once

#include "stdafx.h"

#include "Output.h"

class WndToolbar
{
public:
	// List of icon resource IDs.
	typedef std::list<UINT> IconList;

	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'id' - toolbar ID.
	// 'settings' - application settings.
	// 'iconList' - list of icon resource IDs.
	WndToolbar( HINSTANCE instance, HWND parent, const long long id, Settings& settings, const IconList& iconList );

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

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const = 0;

	// Returns the toolbar ID.
	int GetID() const;

	// Returns the toolbar button size.
	int GetButtonSize() const;

	// Called when the toolbar size changes.
	void OnChangeToolbarSize();

protected:
	// Sets whether the button corresponding to 'commandID' is 'enabled'.
	void SetButtonEnabled( const UINT commandID, const bool enabled );

	// Sets whether the button corresponding to 'commandID' is 'checked'.
	void SetButtonChecked( const UINT commandID, const bool checked );

	// Returns the image list.
	HIMAGELIST GetImageList() const;

private:
	// Window procedure
	static LRESULT CALLBACK ToolbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Creates and returns an image list.
	HIMAGELIST CreateImageList() const;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Application settings.
	Settings& m_Settings;

	// List of icon resource IDs.
	IconList m_IconList;

	// Image list.
	HIMAGELIST m_ImageList;
};

