#pragma once

#include "WndToolbar.h"

class WndToolbarConvert : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndToolbarConvert( HINSTANCE instance, HWND parent, Settings& settings );

	// Updates the toolbar state.
	// 'playlist' - currently displayed playlist.
	void Update( const Playlist::Ptr playlist );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	UINT GetTooltip( const UINT commandID ) const override;

private:
	// Creates the buttons.
	void CreateButtons();

	// The current tooltip resource ID.
	WORD m_TooltipID;
};
