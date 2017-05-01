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
	typedef std::function<void()> ClickCallback;

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Gets the rebar window handle.
	HWND GetWindowHandle();

	// Adds a control to a new rebar band.
	// 'hwnd' - control window handle.
	// 'iconCallback' - callback for returning the icon resource ID for the control.
	void AddControl( HWND hwnd );

	// Adds a control to a new rebar band, with an associated icon.
	// 'hwnd' - control window handle.
	// 'icons' - the set of icons which can be displayed.
	// 'iconCallback' - callback for returning the current icon resource ID for the control.
	// 'iconCallback' - callback which handles clicking on the rebar band icon.
	void AddControl( HWND hwnd, const std::set<UINT>& icons, IconCallback iconCallback, ClickCallback clickCallback );

	// Returns the (0-based) position of the 'hwnd' in the rebar control.
	int GetPosition( const HWND hwnd ) const;

	// Moves the control at the 'currentPosition' to the start of the rebar control.
	void MoveToStart( const int currentPosition );

	// Updates the rebar state.
	void Update();

	// Called when the caption of a 'bandID' is clicked.
	void OnClickCaption( const UINT bandID );

private:
	// Creates the image list.
	void CreateImageList();

	// Maps an icon resource ID to an image list index.
	typedef std::map<UINT,int> ImageListMap;

	// Maps a rebar band ID to an icon callback.
	typedef std::map<UINT,IconCallback> IconCallbackMap;

	// Maps a rebar band ID to an click callback.
	typedef std::map<UINT,ClickCallback> ClickCallbackMap;

	// Returns the current image list index value for a rebar 'bandID'.
	int GetImageListIndex( const UINT bandID ) const;

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
};
