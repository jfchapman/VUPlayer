#pragma once

#include "stdafx.h"

#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>

class WndRebar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndRebar( HINSTANCE instance, HWND parent );

	virtual ~WndRebar();

	// Callback for returning the icon resource ID for a rebar band.
	typedef std::function<UINT()> IconCallback;

	// Callback which handles clicking on a rebar band icon.
	// 'rightClick' - true indicates a right click, false indicates a left click.
	typedef std::function<void( const bool rightClick )> ClickCallback;

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Gets the rebar window handle.
	HWND GetWindowHandle();

	// Adds a control to a new rebar band.
	// 'hwnd' - control window handle.
	// 'iconCallback' - callback for returning the icon resource ID for the control.
	// 'canHide' - indicates whether the control can be hidden when the rebar is shrunk.
	void AddControl( HWND hwnd, const bool canHide = true );

	// Adds a control to a new rebar band, with an associated icon.
	// 'hwnd' - control window handle.
	// 'icons' - the set of icons which can be displayed.
	// 'iconCallback' - callback for returning the current icon resource ID for the control.
	// 'iconCallback' - callback which handles clicking on the rebar band icon.
	// 'canHide' - indicates whether the control can be hidden when the rebar is shrunk.
	void AddControl( HWND hwnd, const std::set<UINT>& icons, IconCallback iconCallback, ClickCallback clickCallback, const bool canHide = true );

	// Returns the (0-based) position of the 'hwnd' in the rebar control.
	int GetPosition( const HWND hwnd ) const;

	// Moves the control at the 'currentPosition' to the start of the rebar control.
	void MoveToStart( const int currentPosition );

	// Updates the rebar state.
	void Update();

private:
	// Window procedure.
	static LRESULT CALLBACK RebarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Maps an icon resource ID to an image list index.
	typedef std::map<UINT,int> ImageListMap;

	// Maps a rebar band ID to an icon callback.
	typedef std::map<UINT,IconCallback> IconCallbackMap;

	// Maps a rebar band ID to an click callback.
	typedef std::map<UINT,ClickCallback> ClickCallbackMap;

	// A set of rebar band IDs.
	typedef std::set<UINT> BandIDSet;

	// Maps a rebar band ID to a child window.
	typedef std::map<UINT,HWND> BandChildMap;

	// Returns the current image list index value for a rebar 'bandID'.
	int GetImageListIndex( const UINT bandID ) const;

	// Called when the caption of a 'bandID' is clicked, and whether it was a 'rightClick'.
	void OnClickCaption( const UINT bandID, const bool rightClick );

	// Creates the image list.
	void CreateImageList();

	// Rearranges the rebar bands on initialisation or resizing, to ensure there is only a single row of buttons.
	void RearrangeBands();

	// Reorders rebar bands by ID.
	void ReorderBandsByID();

	// Next available rebar band ID.
	static UINT s_BandID;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Image list.
	HIMAGELIST m_ImageList;

	// The set of rebar bands which contain an icon.
	std::set<UINT> m_IconBands;

	// Maps an icon resource ID to an image list index.
	ImageListMap m_ImageListMap;

	// Maps a rebar band ID to an icon callback.
	IconCallbackMap m_IconCallbackMap;

	// Maps a rebar band ID to a click callback.
	ClickCallbackMap m_ClickCallbackMap;

	// The set of rebar bands that can be hidden.
	BandIDSet m_BandCanBeHidden;

	// The set of rebar bands that are hidden.
	BandIDSet m_HiddenBands;

	// Maps a rebar band ID to its child window handle.
	BandChildMap m_BandChildWindows;

	// Indicates whether to prevent rearrangement of rebar bands.
	bool m_PreventBandRearrangement;

	// The previous rebar width.
	int m_PreviousRebarWidth;
};
