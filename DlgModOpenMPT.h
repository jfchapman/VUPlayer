#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgModOpenMPT
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	DlgModOpenMPT( const HINSTANCE instance, const HWND parent, Settings& settings );

	virtual ~DlgModOpenMPT();

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Saves the current settings.
	void SaveSettings( const HWND hwnd );

	// Resets to default settings.
	void ResetToDefaults( const HWND hwnd );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Application settings.
	Settings& m_Settings;
};
