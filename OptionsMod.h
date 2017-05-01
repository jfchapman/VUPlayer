#pragma once

#include "Options.h"

class OptionsMod : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsMod( HINSTANCE instance, Settings& settings, Output& output );

	virtual ~OptionsMod();

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

private:
	// Updates controls based on the currently selected format type.
	// 'hwnd' - dialog window handle.
	void UpdateControls( const HWND hwnd );

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

