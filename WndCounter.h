#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"

class WndCounter
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	// 'height' - counter height.
	WndCounter( HINSTANCE instance, HWND parent, Settings& settings, Output& output, const int height );

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

	// Displays the colour selection dialog for the counter.
	void OnSelectColour();

	// Saves counter settings to application settings.
	void SaveSettings();

private:
	// Window procedure
	static LRESULT CALLBACK CounterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the control needs painting.
	void OnPaint( const PAINTSTRUCT& ps );

	// Displays the context menu at the specified 'position', in screen coordinates.
	void OnContextMenu( const POINT& position );

	// Called when a 'command' is received.
	void OnCommand( const UINT command );

	// Sets the counter font.
	void SetCounterFont( const LOGFONT& font );

	// Redraws the counter.
	void Redraw();

	// Toggles the counter between track elapsed & track remaining.
	void Toggle();

	// Returns the background on which to draw the counter.
	std::shared_ptr<Gdiplus::Bitmap> GetBackgroundBitmap();

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

	// Counter colour.
	COLORREF m_Colour;

	// True to show track remaining, false to show track elapsed.
	bool m_ShowRemaining;

	// Effective font vertical midpoint, used when drawing the counter text.
	float m_FontMidpoint;

	// Background on which to draw the counter.
	std::shared_ptr<Gdiplus::Bitmap> fBackgroundBitmap;
};

