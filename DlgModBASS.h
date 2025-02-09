#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgModBASS
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	DlgModBASS( const HINSTANCE instance, const HWND parent, Settings& settings );

	virtual ~DlgModBASS();

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Called when a command on the dialog should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam );

	// Updates controls based on the currently selected format type.
	// 'hwnd' - dialog window handle.
	void UpdateControls( const HWND hwnd );

	// Saves the current settings.
	void SaveSettings( const HWND hwnd );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Application settings.
	Settings& m_Settings;

	// MOD settings.
	long long m_MODSettings;

	// MTM settings.
	long long m_MTMSettings;

	// S3M settings.
	long long m_S3MSettings;

	// XM settings.
	long long m_XMSettings;

	// IT settings.
	long long m_ITSettings;

	// Currently displayed settings.
	long long* m_CurrentSettings;
};
