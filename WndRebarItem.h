#pragma once

#include "stdafx.h"

#include <optional>

class Settings;

class WndRebarItem
{
public:
	WndRebarItem() = default;

	// Returns the rebar item ID.
	virtual int GetID() const = 0;

	// Returns the rebar item window handle.
	virtual HWND GetWindowHandle() const = 0;

	// Gets the rebar item fixed width.
	virtual std::optional<int> GetWidth() const = 0;

	// Gets the rebar item fixed height.
	virtual std::optional<int> GetHeight() const = 0;

	// Returns whether the rebar item has a vertical divider.
	virtual bool HasDivider() const = 0;

	// Returns whether the rebar item can be hidden.
	virtual bool CanHide() const = 0;

	// Called when the rebar item 'settings' have changed.
	virtual void OnChangeRebarItemSettings( Settings& settings ) = 0;

	// Rebar item custom draw callback, using the 'nmcd' structure.
	// Return the appropriate value for the draw stage, or nullopt if the rebar item does not handle custom draw.
	virtual std::optional<LRESULT> OnCustomDraw( LPNMCUSTOMDRAW nmcd ) = 0;

	// Displays the context menu for the rebar item at the specified 'position', in screen coordinates.
	// Return true if the rebar item handled the context menu, or false to display the rebar context menu.
	virtual bool ShowContextMenu( const POINT& position ) = 0;

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	// 'isClassicTheme' - indicates whether the Windows classic theme is active.
	virtual void OnSysColorChange( const bool isHighContrast, const bool isClassicTheme ) = 0;
};
