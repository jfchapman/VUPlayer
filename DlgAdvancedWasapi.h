#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"

class DlgAdvancedWasapi
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	// 'deviceName' - selected WASAPI device name, or an empty string for the default device.
	DlgAdvancedWasapi( const HINSTANCE instance, const HWND parent, Settings& settings, Output& output, const std::wstring& deviceName );

	virtual ~DlgAdvancedWasapi();

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

	// Device name to use for device specific settings (with the default device resolved to the actual device).
	const std::wstring m_SettingsDeviceName;
};
