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
};

