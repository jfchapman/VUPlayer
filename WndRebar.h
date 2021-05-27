#pragma once

#include "stdafx.h"

#include "WndRebarItem.h"
#include "Settings.h"

#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

class WndRebar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndRebar( HINSTANCE instance, HWND parent, Settings& settings );

	virtual ~WndRebar();

	// Gets the rebar window handle.
	HWND GetWindowHandle();

	// Adds a rebar item.
	void AddItem( WndRebarItem* item );

	// Toggles the visibility of the 'toolbarID'.
	void ToggleToolbar( const int toolbarID );

	// Rearranges the rebar items.
	void RearrangeItems();

	// Called when the rebar settings have been changed.
	void OnChangeSettings();

	// Displays the colour selection dialog for the rebar, for the 'commandID'.
	void OnSelectColour( const UINT commandID );

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	// 'isClassicTheme' - indicates whether whether the Windows classic theme is active.
	void OnSysColorChange( const bool isHighContrast, const bool isClassicTheme );

private:
	// Window procedure.
	static LRESULT CALLBACK RebarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Displays the context menu for the 'hwnd' at the specified 'position', in screen coordinates.
	void OnContextMenu( const HWND hwnd, const POINT& position );

	// Dispatches custom draw notifications to the 'hwnd', using the 'nmcd' structure.
	// Returns the custom draw response result, or nullopt if the rebar item did not handle the custom draw notification.
	std::optional<LRESULT> OnCustomDraw( const HWND hwnd, LPNMCUSTOMDRAW nmcd );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Rebar items.
	std::vector<WndRebarItem*> m_Items;

	// Maps a window handle to the corresponding rebar item.
	std::map<HWND, WndRebarItem*> m_ItemMap;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;

	// Indicates whether the Windows classic theme is active.
	bool m_IsClassicTheme;
};
