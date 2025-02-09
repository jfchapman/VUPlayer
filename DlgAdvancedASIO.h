#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgAdvancedASIO
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	DlgAdvancedASIO( const HINSTANCE instance, const HWND parent, Settings& settings );

	virtual ~DlgAdvancedASIO();

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Saves the current settings.
	void SaveSettings( const HWND hwnd );

	// Resets controls to the default state.
	void ResetToDefaults( const HWND hwnd );

	// Application settings.
	Settings& m_Settings;
};
