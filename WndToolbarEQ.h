#pragma once

#include "WndToolbar.h"

class WndToolbarEQ : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndToolbarEQ( HINSTANCE instance, HWND parent, Settings& settings );

	// Updates the toolbar state.
	// 'eqVisible' - indicates whether the EQ dialog is visible.
	void Update( const bool eqVisible );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	UINT GetTooltip( const UINT commandID ) const override;

private:
	// Creates the buttons.
	void CreateButtons();
};
