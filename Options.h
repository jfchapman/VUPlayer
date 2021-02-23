#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"

// Options page interface.
class Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	Options( HINSTANCE instance, Settings& settings, Output& output );

	virtual ~Options();

	// Called when the options page should be initialised.
	// 'hwnd' - dialog window handle.
	virtual void OnInit( const HWND hwnd ) = 0;

	// Called when the options page should be saved.
	// 'hwnd' - dialog window handle.
	virtual void OnSave( const HWND hwnd ) = 0;

	// Called when a command on the options page should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	virtual void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) = 0;

	// Called when a notification message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - notification parameter.
	// 'lParam' - notification parameter.
	virtual void OnNotify( const HWND hwnd, const WPARAM wParam, const LPARAM lParam );

	// Called when a draw item message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - draw item parameter.
	// 'lParam' - draw item parameter.
	virtual void OnDrawItem( const HWND, const WPARAM wParam, const LPARAM lParam );

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle() const;

	// Returns the application settings.
	Settings& GetSettings();

	// Returns the output object.
	Output& GetOutput();

private:
	// Module instance handle.
	HINSTANCE m_hInst;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;
};
