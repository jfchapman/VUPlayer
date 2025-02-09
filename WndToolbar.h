#pragma once

#include "stdafx.h"

#include "Output.h"

#include "WndRebarItem.h"

class WndToolbar : public WndRebarItem
{
public:
	// List of icon resource IDs.
	typedef std::list<UINT> IconList;

	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'id' - toolbar ID.
	// 'settings' - application settings.
	// 'iconList' - list of icon resource IDs.
	WndToolbar( HINSTANCE instance, HWND parent, const long long id, Settings& settings, const IconList& iconList );

	virtual ~WndToolbar();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle() const;

	// Returns whether the button corresponding to 'commandID' is enabled.
	bool IsButtonEnabled( const UINT commandID ) const;

	// Returns whether the button corresponding to 'commandID' is checked.
	bool IsButtonChecked( const UINT commandID ) const;

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const = 0;

	// Returns the toolbar button size.
	int GetButtonSize() const;

	// Returns the toolbar ID.
	int GetID() const override;

	// Returns the rebar item window handle.
	HWND GetWindowHandle() const override;

	// Gets the rebar item fixed width.
	std::optional<int> GetWidth() const override;

	// Gets the rebar item fixed height.
	std::optional<int> GetHeight() const override;

	// Returns whether the rebar item has a vertical divider.
	bool HasDivider() const override;

	// Returns whether the rebar item can be hidden.
	bool CanHide() const override;

	// Called when the rebar item 'settings' have changed.
	void OnChangeRebarItemSettings( Settings& settings ) override;

	// Rebar item custom draw callback, using the 'nmcd' structure.
	// Return the appropriate value for the draw stage, or nullopt if the rebar item does not handle custom draw.
	std::optional<LRESULT> OnCustomDraw( LPNMCUSTOMDRAW nmcd ) override;

	// Displays the context menu for the rebar item at the specified 'position', in screen coordinates.
	// Return true if the rebar item handled the context menu, or false to display the rebar context menu.
	bool ShowContextMenu( const POINT& position ) override;

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	// 'isClassicTheme' - indicates whether the Windows classic theme is active.
	void OnSysColorChange( const bool isHighContrast, const bool isClassicTheme ) override;

protected:
	// Sets whether the button corresponding to 'commandID' is 'enabled'.
	void SetButtonEnabled( const UINT commandID, const bool enabled );

	// Sets whether the button corresponding to 'commandID' is 'checked'.
	void SetButtonChecked( const UINT commandID, const bool checked );

	// Returns the image list.
	HIMAGELIST GetImageList() const;

private:
	// Window procedure
	static LRESULT CALLBACK ToolbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Creates and sets the toolbar image list.
	void CreateImageList();

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Application settings.
	Settings& m_Settings;

	// List of icon resource IDs.
	IconList m_IconList;

	// Image list.
	HIMAGELIST m_ImageList;

	// Toolbar button colour.
	COLORREF m_ButtonColour;

	// Toolbar background colour.
	COLORREF m_BackgroundColour;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;

	// Indicates whether the Windows classic theme is active.
	bool m_IsClassicTheme;
};
