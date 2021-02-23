#pragma once

#include "Options.h"

class OptionsMod : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsMod( HINSTANCE instance, Settings& settings, Output& output );

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
	// Updates controls based on the currently selected format type.
	// 'hwnd' - dialog window handle.
	void UpdateControls( const HWND hwnd );

	// Displays a soundfont file chooser dialog.
	void ChooseSoundFont();

	// Dialog window handle.
	HWND m_hWnd;

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

