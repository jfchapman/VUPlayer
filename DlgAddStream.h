#pragma once

#include "stdafx.h"

#include <string>

class DlgAddStream
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	DlgAddStream( const HINSTANCE instance, const HWND parent );

	// Returns the stream URL if the dialog was okayed, or an empty string if cancelled.
	const std::wstring& GetURL() const;

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Called when the dialog is closed.
	// 'ok' - whether the dialog was okayed.
	void OnClose( const bool ok );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Stream URL.
	std::wstring m_URL;
};
