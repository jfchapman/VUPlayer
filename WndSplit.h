#pragma once

#include "stdafx.h"

#include "Settings.h"

class WndSplit
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'wndRebar' - rebar window handle.
	// 'wndStatus' - status bar window handle.
	// 'wndTree' - tree control window handle.
	// 'wndVisual' - visual control window handle.
	// 'wndList' - list control window handle.
	// 'settings' - application settings.
	WndSplit( HINSTANCE instance, HWND parent, HWND wndRebar, HWND wndStatus, HWND wndTree, HWND wndVisual, HWND wndList, Settings& settings );

	virtual ~WndSplit();

	// Gets the splitter window handle.
	HWND GetWindowHandle();

	// Returns whether the splitter is currently being dragged.
	bool GetIsDragging() const;

	// Sets whether the splitter is currently being dragged.
	void SetIsDragging( const bool dragging );

	// Returns whether the splitter is currently tracking the mouse.
	bool GetIsTracking() const;

	// Sets whether the splitter is currently tracking the mouse.
	void SetIsTracking( const bool tracking );

	// Called when the splitter is dragged.
	// 'position' - split position in client coordinates.
	void OnDrag( const POINT& position );

	// Resizes the split window and all child windows.
	void Resize();

private:
	// Next available child window ID.
	static UINT_PTR s_WndSplitID;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Split window handle.
	HWND m_hWnd;

	// Parent window handle.
	HWND m_hParent;

	// Rebar window handle.
	HWND m_hRebar;

	// Status bar window handle.
	HWND m_hStatus;

	// Tree control window handle.
	HWND m_hTree;

	// Visual control window handle.
	HWND m_hVisual;

	// List control window handle.
	HWND m_hList;

	// Split position.
	int m_SplitPosition;

	// Minimum split position.
	int m_MinSplit;

	// Maximum split position.
	int m_MaxSplit;
	
	// Indicates whether the splitter is currently being dragged.
	bool m_IsDragging;

	// Indicates whether the splitter is currently tracking the mouse.
	bool m_IsTracking;

	// Indicates whether the splitter is current being resized.
	bool m_IsSizing;

	// Application settings.
	Settings& m_Settings;
};

