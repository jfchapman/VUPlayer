#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgHotkey
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	DlgHotkey( const HINSTANCE instance, const HWND parent );

	virtual ~DlgHotkey();

	// Gets the 'hotkey', returning true if the hot key is valid.
	bool GetHotkey( Settings::Hotkey& hotkey ) const;

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Cancel button procedure.
	static LRESULT CALLBACK ButtonProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// WM_KEYDOWN message handler, returns true if a valid hotkey was pressed.
	bool OnKeyDown( WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Returns the default window procedure for the dialog button.
	WNDPROC GetDefaultButtonProc();

	// Initialises the allowed hotkeys.
	void InitAllowedHotkeys();

	// Returns whether the 'alt', 'control' & 'shift' keys are pressed.
	void GetKeyStates( bool& alt, bool &control, bool& shift ) const;

	// Hotkey information.
	Settings::Hotkey m_Hotkey;

	// Whether the user has pressed a valid hotkey.
	bool m_Valid;

	// Allowed unmodified hotkeys.
	std::set<int> m_UnmodifiedHotkeys;

	// Allowed modified hotkeys.
	std::set<int> m_ModifiedHotkeys;

	// Dialog window handle.
	HWND m_hWnd;

	// Default button window procedure.
	WNDPROC m_DefaultButtonProc;
};

