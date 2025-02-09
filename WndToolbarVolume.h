#pragma once

#include "WndToolbar.h"
#include "WndTrackbar.h"

class WndToolbarVolume : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndToolbarVolume( HINSTANCE instance, HWND parent, Settings& settings );

	// Updates the toolbar state.
	// 'output' - output object.
	// 'trackbarType' - currently displayed trackbar type.
	void Update( const Output& output, const WndTrackbar::Type trackbarType );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	UINT GetTooltip( const UINT commandID ) const override;

	// Returns whether the rebar item can be hidden.
	bool CanHide() const override;

	// Returns whether the rebar item has a vertical divider.
	bool HasDivider() const override;

	// Displays the context menu for the rebar item at the specified 'position', in screen coordinates.
	// Return true if the rebar item handled the context menu, or false to display the rebar context menu.
	bool ShowContextMenu( const POINT& position ) override;

private:
	// Creates the buttons.
	// 'trackbarType' - currently displayed trackbar type.
	void CreateButtons( const WndTrackbar::Type trackbarType );
};
