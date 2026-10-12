#pragma once

#include "stdafx.h"

#include <array>

// Dark mode scrollbars for the list control.
// List view controls cannot be given a dark mode scrollbar theme without also changing how the list itself is drawn,
// so in dark mode, (dark themed) stock scrollbar controls are overlaid on top of the list's own vertical & horizontal scrollbars,
// and the list's window region excludes its own scrollbars, so that they are never drawn.
// When dark mode is not active, the overlays are hidden and the list is left in its default state.
class WndListScrollbar
{
public:
	// 'instance' - module instance handle.
	// 'list' - list control window handle.
	WndListScrollbar( HINSTANCE instance, HWND list );

	virtual ~WndListScrollbar();

	// Sets whether the overlays are 'active' (i.e. when dark mode is active).
	void SetActive( const bool active );

	// Synchronises the overlays with the list's own scrollbars (position, range, visibility & location).
	// This should be called whenever the list might have scrolled, changed size or changed contents.
	void Update();

private:
	// Scrollbar overlay.
	struct Overlay {
		// Scrollbar type (SB_VERT or SB_HORZ).
		int Bar = SB_VERT;

		// Scrollbar object ID (OBJID_VSCROLL or OBJID_HSCROLL).
		LONG ObjectID = OBJID_VSCROLL;

		// Host window handle (which receives notifications from the scrollbar control).
		HWND Host = nullptr;

		// Scrollbar control window handle.
		HWND Scrollbar = nullptr;

		// Indicates whether the overlay thumb is currently being dragged.
		bool IsTracking = false;

		// The scroll information last applied to the scrollbar control.
		SCROLLINFO ScrollInfo = {};
	};

	// Window procedure for the host windows.
	static LRESULT CALLBACK HostProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Creates the 'overlay' host window & scrollbar control.
	// 'instance' - module instance handle.
	// 'bar' - scrollbar type (SB_VERT or SB_HORZ).
	void CreateOverlay( Overlay& overlay, const HINSTANCE instance, const int bar );

	// Returns the overlay which uses the 'host' window, or nullptr if there is no match.
	Overlay* GetOverlay( const HWND host );

	// Called when the 'overlay' scrollbar is scrolled using the scroll 'request' code.
	void OnScroll( Overlay& overlay, const int request );

	// Positions the 'window' over the 'screenRect' (or hides the window if the rect is empty).
	void PositionWindow( const HWND window, const RECT& screenRect );

	// Synchronises the 'overlay' scroll information & enabled state with the list's own scrollbar, using the 'scrollbarInfo' for the list's own scrollbar.
	void UpdateScrollInfo( Overlay& overlay, const SCROLLBARINFO& scrollbarInfo );

	// Sets the list's window region to exclude the 'excludedRects' (in list window coordinates), or removes the region if all rects are empty.
	void SetListRegion( const std::array<RECT, 3>& excludedRects );

	// Hides the overlays & removes the list's window region.
	void Hide();

	// Moves the list above any overlapping group boxes in the z-order, so that the list is not clipped by them.
	void RaiseAboveGroupBoxes();

	// Paints the corner window (between the vertical & horizontal scrollbars).
	void OnPaintCorner();

	// List control window handle.
	const HWND m_hList;

	// Vertical scrollbar overlay.
	Overlay m_Vertical;

	// Horizontal scrollbar overlay.
	Overlay m_Horizontal;

	// Corner window handle, which covers the gap between the vertical & horizontal scrollbars.
	HWND m_hCorner = nullptr;

	// Indicates whether the overlays are active.
	bool m_IsActive = false;

	// Indicates whether an update is in progress (to prevent re-entrancy).
	bool m_IsUpdating = false;

	// Indicates whether the WS_CLIPSIBLINGS style has been added to the list (so that it is only removed if it was not originally set).
	bool m_AddedClipSiblings = false;

	// The rects (in list window coordinates) which have been excluded from the list's window region (all empty if no region is set).
	std::array<RECT, 3> m_RegionRects = {};
};
