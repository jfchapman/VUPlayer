#pragma once

#include "WndToolbar.h"

class WndToolbarPlayback : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndToolbarPlayback( HINSTANCE instance, HWND parent, Settings& settings );

	// Updates the toolbar state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	void Update( Output& output, const Playlist::Ptr playlist, const Playlist::Item& selectedItem );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	UINT GetTooltip( const UINT commandID ) const override;

private:
	// Creates the buttons.
	void CreateButtons();

	// Sets whether the 'paused' button is shown.
	void SetPaused( const bool paused ) const;

	// Returns whether the pause button is shown.
	bool IsPauseShown() const;
};

