#pragma once

#include "stdafx.h"

#include "Settings.h"

class DlgConvertFilename
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	DlgConvertFilename( const HINSTANCE instance, const HWND parent, Settings& settings );

	virtual ~DlgConvertFilename();

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Sets the default filename format.
	void OnDefault();

	// Called when the dialog is closed.
	// 'ok' - whether the dialog was okayed.
	void OnClose( const bool ok );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;
};
