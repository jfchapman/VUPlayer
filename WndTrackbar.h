#pragma once

#include "stdafx.h"

#include "Output.h"

class WndTrackbar
{
public:
	// Trackbar type
	enum class Type {
		Seek,
		Volume,
		Pitch
	};

	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	// 'settings' - application settings.
	// 'minValue' - minimum trackbar value.
	// 'maxValue' - maximum trackbar value.
	// 'type' - trackbar type.
	WndTrackbar( HINSTANCE instance, HWND parent, Output& output, Settings& settings, const int minValue, const int maxValue, const Type type );

	virtual ~WndTrackbar();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Returns the trackbar window handle.
	HWND GetWindowHandle();

	// Returns whether the trackbar is enabled.
	bool GetEnabled() const;

	// Sets whether the trackbar is enabled.
	void SetEnabled( const bool enabled );

	// Sets whether the trackbar is visible.
	void SetVisible( const bool visible );

	// Gets the trackbar type.
	Type GetType() const;

	// Sets the trackbar type.
	virtual void SetType( const Type type );

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText() = 0;

	// Called when the trackbar 'position' has changed.
	virtual void OnPositionChanged( const int position ) = 0;

	// Called when the user drags the trackbar thumb to a 'position'.
	virtual void OnDrag( const int position ) = 0;

	// Displays the context menu at the specified 'position', in screen coordinates.
	virtual void OnContextMenu( const POINT& position );

protected:
	// Returns the output object.
	Output& GetOutput();

	// Returns the output object.
	const Output& GetOutput() const;

	// Returns the application settings.
	Settings& GetSettings();

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle();

	// Returns the trackbar thumb position.
	int GetPosition() const;

	// Sets the trackbar thumb position.
	void SetPosition( const int position );

	// Returns the trackbar range.
	int GetRange() const;

private:
	// Window procedure
	static LRESULT CALLBACK TrackbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Next available child window ID.
	static UINT_PTR s_WndTrackbarID;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Output object.
	Output& m_Output;

	// Application settings.
	Settings& m_Settings;

	// Trackbar type.
	Type m_Type;
};

