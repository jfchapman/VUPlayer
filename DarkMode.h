#pragma once

#include "stdafx.h"

// Dark mode support.
// Note that this relies on undocumented uxtheme.dll functionality, which is only available on Windows 10 1809 (build 17763) and later.
namespace DarkMode
{
	// Dark mode preference.
	enum class Mode {
		System,		// Follow the Windows app colour scheme
		Light,		// Always light
		Dark			// Always dark (Windows 10 1903 or later, otherwise follows the system)
	};

	// Initialises dark mode support using the 'mode' preference.
	// This should be called before any windows are created.
	void Initialise( const Mode mode = Mode::System );

	// Returns whether dark mode is supported on this version of Windows.
	bool IsSupported();

	// Returns whether dark mode is currently active.
	bool IsActive();

	// Sets the dark mode preference.
	// Returns true if the active state changed (in which case ApplyToMainWindow should be called & all windows redrawn).
	bool SetMode( const Mode mode );

	// Re-evaluates whether dark mode should be active (e.g. after the system colour scheme or high contrast state has changed).
	// Returns true if the active state changed (in which case ApplyToMainWindow should be called & all windows redrawn).
	bool Refresh();

	// Applies the current dark mode state to the frame (title bar & menu bar) of the main window 'hwnd'.
	void ApplyToMainWindow( const HWND hwnd );

	// Applies the current dark mode state to the common control 'hwnd' (e.g. a list view or tree view), so that its scrollbars are drawn dark.
	// 'lightTheme' - the theme to restore when dark mode is deactivated (or nullptr for the default theme).
	// The control is left in its existing state unless dark mode is (or has been) active.
	void ApplyToControl( const HWND hwnd, LPCWSTR lightTheme = nullptr );

	// Applies the current dark mode state to the header control 'hwnd', so that its background is drawn dark.
	// Note that the header text colour also needs to be set via custom draw, using GetTextColour, when dark mode is active.
	// The control is left in its default state unless dark mode is (or has been) active.
	void ApplyToHeader( const HWND hwnd );

	// Applies the current dark mode state to the tooltips which belong to the child controls of the window 'hwnd' (e.g. toolbars, trackbars, list & tree views).
	// The tooltips are left in their default state unless dark mode is (or has been) active.
	void ApplyToChildTooltips( const HWND hwnd );

	// Applies dark mode to the dialog 'hwnd' (title bar, background & controls), if dark mode is active.
	// When dark mode is not active, only progress bars are affected (these are drawn without the moving highlight, for consistency between modes).
	// This should be called when the dialog is initialised (i.e. on WM_INITDIALOG).
	void ApplyToDialog( const HWND hwnd );

	// Draws the corner between dark mode vertical & horizontal scrollbars onto the 'dc', using the 'rect'.
	void DrawScrollbarCorner( const HDC dc, const RECT& rect );

	// Draws the corner between the vertical & horizontal scrollbars of the window 'hwnd' (if both are visible), so that it matches the dark mode scrollbars.
	// This should be called after the window's non-client area has been painted.
	void DrawScrollbarCorner( const HWND hwnd );

	// Handles main window messages for the dark mode menu bar.
	// Returns true if the message was handled, in which case 'result' should be returned from the window procedure.
	bool HandleMainWindowMessage( const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam, LRESULT& result );

	// Returns whether a WM_SETTINGCHANGE message 'lParam' indicates that the Windows app colour scheme has changed.
	bool IsColourSchemeChange( const LPARAM lParam );

	// Dark mode colours.
	COLORREF GetBackgroundColour();
	COLORREF GetHotColour();
	COLORREF GetPressedColour();
	COLORREF GetTextColour();
	COLORREF GetDisabledTextColour();
	COLORREF GetEdgeColour();
	COLORREF GetGripColour();

	// Default dark mode list colours (the main window list colours can be customised, but dialog lists always use these colours).
	COLORREF GetListTextColour();
	COLORREF GetListBackgroundColour();

	// Default dark mode accent colour, used for highlights, icons, toolbar buttons & the counter font.
	COLORREF GetAccentColour();

	// Dark mode trackbar colours.
	COLORREF GetTrackbarChannelColour();
	COLORREF GetTrackbarChannelBorderColour();
	COLORREF GetTrackbarHotThumbColour();
	COLORREF GetTrackbarDisabledThumbColour();

	// Draws a dark mode trackbar channel onto the 'dc', using the 'rect'.
	void DrawTrackbarChannel( const HDC dc, const RECT& rect );

	// Dark mode brushes (these are owned by the DarkMode module, and should not be deleted).
	HBRUSH GetBackgroundBrush();
	HBRUSH GetHotBrush();
	HBRUSH GetPressedBrush();
	HBRUSH GetEdgeBrush();
	HBRUSH GetGripBrush();
}
