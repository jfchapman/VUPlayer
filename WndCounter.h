#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"
#include "WndRebarItem.h"

class WndCounter : public WndRebarItem
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	WndCounter( HINSTANCE instance, HWND parent, Settings& settings, Output& output );

	virtual ~WndCounter();

	// Gets the counter window handle.
	HWND GetWindowHandle();

	// Refreshes the counter display.
	void Refresh();

	// Returns true if showing track remaining or false if showing track elapsed.
	bool GetTrackRemaining() const;

	// Sets the counter to track elapsed or track remaining.
	// 'showRemaining' - true to show track remaining or false to show track elapsed.
	void SetTrackRemaining( const bool showRemaining );

	// Displays the font selection dialog for changing the counter font.
	void OnSelectFont();

	// Displays the colour selection dialog for the counter font.
	void OnSelectColour();

	// Saves counter settings to application settings.
	void SaveSettings();

	// Returns the rebar item ID.
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

	// Called when the rebar item 'settings' have changed.z
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

private:
	// Window procedure
	static LRESULT CALLBACK CounterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the window needs painting with the 'ps'.
	void OnPaint( const PAINTSTRUCT& ps );

	// Called when a 'command' is received.
	void OnCommand( const UINT command );

	// Sets the counter font.
	void SetCounterFont( const LOGFONT& font );

	// Calculate the vertical extents of the current font so that text can be centered when drawing the counter.
	void CentreFont();

	// Redraws the counter.
	void Redraw();

	// Toggles the counter between track elapsed & track remaining.
	void Toggle();

	// Sets the control width, based on the 'settings'.
	void SetWidth( Settings& settings );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Counter text.
	std::wstring m_Text;

	// Counter font.
	LOGFONT m_Font;

	// Font colour.
	COLORREF m_FontColour;

	// Background colour.
	COLORREF m_BackgroundColour;

	// True to show track remaining, false to show track elapsed.
	bool m_ShowRemaining;

	// Effective font vertical midpoint, used when drawing the counter text.
	float m_FontMidpoint;

	// Control width.
	std::optional<int> m_Width;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;

	// Indicates whether the Windows classic theme is active.
	bool m_IsClassicTheme;
};
