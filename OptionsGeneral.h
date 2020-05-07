#pragma once

#include "Options.h"

class OptionsGeneral : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsGeneral( HINSTANCE instance, Settings& settings, Output& output );

	virtual ~OptionsGeneral();

	// Called when the options page should be initialised.
	// 'hwnd' - dialog window handle.
	virtual void OnInit( const HWND hwnd );

	// Called when the options page should be saved.
	// 'hwnd' - dialog window handle.
	virtual void OnSave( const HWND hwnd );

	// Called when a command on the options page should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	virtual void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam );

	// Called when a notification message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - notification parameter.
	// 'lParam' - notification parameter.
	virtual void OnNotify( const HWND hwnd, const WPARAM wParam, const LPARAM lParam );

private:
	// Refreshes the output device list based on the currently selected output mode.
	// 'hwnd' - dialog window handle.
	void RefreshOutputDeviceList( const HWND hwnd );

	// Returns the currently selected output mode.
	Settings::OutputMode GetSelectedMode( const HWND hwnd ) const;

	// Returns the currently selected device name.
	std::wstring GetSelectedDeviceName( const HWND hwnd ) const;

	// Available output modes, paired with the resource ID of the description.
	static std::vector<std::pair<Settings::OutputMode,int>> s_OutputModes;
};
