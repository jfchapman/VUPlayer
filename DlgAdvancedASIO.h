#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"

class DlgAdvancedASIO
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	// 'deviceName' - selected ASIO device name, or an empty string for the default device.
	DlgAdvancedASIO( const HINSTANCE instance, const HWND parent, Settings& settings, Output& output, const std::wstring& deviceName );

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

	// Shows the ASIO driver control panel.
	void ShowControlPanel();

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Selected ASIO device name.
	const std::wstring m_DeviceName;

	// Device name to use for device specific settings (with the default device resolved to the actual device).
	const std::wstring m_SettingsDeviceName;
};
