#pragma once

#include "stdafx.h"

#include "Output.h"

class WndTrackbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	// 'minValue' - minimum trackbar value.
	// 'maxValue' maximum trackbar value.
	WndTrackbar( HINSTANCE instance, HWND parent, Output& output, const int minValue, const int maxValue );

	virtual ~WndTrackbar();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Returns the trackbar window handle.
	HWND GetWindowHandle();

	// Returns whether the trackbar is enabled.
	bool GetEnabled() const;

	// Sets whether the trackbar is enabled.
	void SetEnabled( const bool enabled );

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText() = 0;

	// Called when the trackbar 'position' has changed.
	virtual void OnPositionChanged( const int position ) = 0;

	// Called when the user drags the trackbar thumb to a 'position'.
	virtual void OnDrag( const int position ) = 0;

protected:
	// Returns the output object.
	Output& GetOutput();

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle();

	// Returns the trackbar thumb position.
	int GetPosition() const;

	// Sets the trackbar thumb position.
	void SetPosition( const int position );

	// Returns the trackbar range.
	int GetRange() const;

private:
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
};

