#pragma once

#include "Options.h"

class OptionsMixing : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsMixing( HINSTANCE instance, Settings& settings, Output& output );

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
	// Displays a soundfont file chooser dialog.
	void ChooseSoundFont();

	// Returns the currently selected preferred decoder.
	// 'hwnd' - dialog window handle.
	Settings::MODDecoder GetPreferredDecoder( const HWND hwnd ) const;

	// Dialog window handle.
	HWND m_hWnd;

	// Soundfont filename.
	std::wstring m_SoundFont;
};

