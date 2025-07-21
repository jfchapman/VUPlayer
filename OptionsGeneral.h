#pragma once

#include "Options.h"

class OptionsGeneral : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsGeneral( HINSTANCE instance, Settings& settings, Output& output );

	// Called when the options page should be initialised.
	// 'hwnd' - dialog window handle.
	void OnInit( const HWND hwnd ) override;

	// Called when the options page should be saved.
	// 'hwnd' - dialog window handle.
	void OnSave( const HWND hwnd ) override;

	// Called when a command on the options page should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) override;

	// Called when a notification message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - notification parameter.
	// 'lParam' - notification parameter.
	void OnNotify( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) override;

private:
	// Available output modes, paired with the resource ID of the description.
	static std::vector<std::pair<Settings::OutputMode, int>> s_OutputModes;

	// Available title bar text formats, paired with the the resource ID of the description.
	static std::vector<std::pair<Settings::TitleBarFormat, int>> s_TitleBarFormats;

	// Available startup states, paired with the the resource ID of the description.
	static std::vector<std::pair<Settings::StartupState, int>> s_StartupStates;

	// Refreshes the output device list based on the currently selected output mode.
	// 'hwnd' - dialog window handle.
	void RefreshOutputDeviceList( const HWND hwnd );

	// Returns the currently selected output mode.
	Settings::OutputMode GetSelectedMode( const HWND hwnd ) const;

	// Returns the currently selected title bar format.
	Settings::TitleBarFormat GetSelectedTitleBarFormat( const HWND hwnd ) const;

	// Returns the currently selected startup state.
	Settings::StartupState GetSelectedStartupState( const HWND hwnd ) const;

	// Returns the currently selected device name.
	std::wstring GetSelectedDeviceName( const HWND hwnd ) const;
};
