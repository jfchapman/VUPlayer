#pragma once

#include "stdafx.h"

#include <Shobjidl.h>

#include <vector>

class Settings;
class WndToolbarPlayback;
class WndVisual;

class WndTaskbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndTaskbar( HINSTANCE instance, HWND parent );

	virtual ~WndTaskbar();

	// Initialises the taskbar thumbnail preview buttons, using the application settings.
	void Init( Settings& settings );

	// Updates the taskbar state.
	// 'toolbarPlayback' - playback toolbar.
	void Update( WndToolbarPlayback& toolbarPlayback );

	// Sets the thumbnail preview toolbar button colour, using the application settings.
	void SetToolbarButtonColour( Settings& settings );

private:
	// Module instance handle.
	const HINSTANCE m_hInst;

	// Parent window handle.
	const HWND m_hWnd;

	// Thumbnail preview toolbar image list.
	HIMAGELIST m_ImageList = nullptr;

	// Thumbnail preview toolbar buttons.
	std::vector<THUMBBUTTON> m_ThumbButtons;

	// Taskbar list instance.
	ITaskbarList3* m_TaskbarList = nullptr;
};
