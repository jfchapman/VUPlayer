#pragma once

#include "stdafx.h"

#include <Shobjidl.h>

#include <vector>

class Output;
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
	// 'output' - output object.
	void Update( WndToolbarPlayback& toolbarPlayback, Output& output );

	// Sets the thumbnail preview toolbar button colour, using the application settings.
	void SetToolbarButtonColour( Settings& settings );

	// Sets whether the progress bar is enabled.
	void EnableProgressBar( const bool enable );

	// Updates the progress bar.
	// 'progress' - progress in the range [0.0, 1.0]
	void UpdateProgress( const float progress );

private:
	// Updates the thumbnail preview toolbar buttons.
	// 'toolbarPlayback' - playback toolbar.
	void UpdateToolbarButtons( WndToolbarPlayback& toolbarPlayback );

	// Updates the progress bar.
	// 'output' - output object.
	void UpdateProgress( Output& output );

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

	// Current progress bar state.
	TBPFLAG m_ProgressState = TBPF_NOPROGRESS;

	// Current progress bar counter.
	unsigned long long m_ProgressCounter = 0;

	// Whether the progress bar is enabled.
	bool m_ProgressEnabled = true;
};
