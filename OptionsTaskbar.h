#pragma once

#include "Options.h"

class OptionsTaskbar : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsTaskbar( HINSTANCE instance, Settings& settings, Output& output );

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

private:
	// Enables the systray controls based on whether the systray is enabled.
	// 'hwnd' - dialog window handle.
	void EnableSystrayControls( const HWND hwnd );

	// Thumbnail preview toolbar button colour.
	COLORREF m_ToolbarButtonColour = 0;
};
