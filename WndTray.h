#pragma once

#include "stdafx.h"

#include "Shellapi.h"

#include "Library.h"
#include "Output.h"
#include "Settings.h"

// Message ID for notification icon messages.
static const UINT MSG_TRAYNOTIFY = WM_APP + 84;

// Command ID of the first playlist item entry on the context menu.
static const UINT MSG_TRAYMENUSTART = WM_APP + 0x1000;

// Command ID of the first playlist item entry on the context menu.
static const UINT MSG_TRAYMENUEND = WM_APP + 0x3000;

// SysTray timer ID.
static const UINT_PTR TIMER_SYSTRAY = 73;

class WndList;
class WndTree;
class WndToolbar;

class WndTray
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'library' - media library.
	// 'settings' - application settings.
	// 'output' - output object.
	// 'wndTree' - tree control.
	// 'wndList' - list control.
	WndTray( HINSTANCE instance, HWND parent, Library& library, Settings& settings, Output& output, WndTree& wndTree, WndList& wndList );

	virtual ~WndTray();

	// Returns a volume level corresponding to a 'volumeMenuID'.
	static float GetMenuVolume( const UINT volumeMenuID );

	// Shows the notification area icon if it is not already shown.
	void Show();

	// Hides the notification area icon if it is shown.
	void Hide();

	// Updates the notification area icon based on the current output 'item'.
	void Update( const Output::Item& item );

	// Called when a notification area icon message is received.
	void OnNotify( WPARAM wParam, LPARAM lParam );

	// Called when a context menu 'command' is received.
	void OnContextMenuCommand( const UINT command );

	// Returns whether the notification are icon is shown.
	bool IsShown() const;

	// Called when a single click event occurs on the notification area icon.
	void OnSingleClick();

	// Called when a double click event occurs on the notification area icon.
	void OnDoubleClick();

private:
	// Maps a context menu command ID to a playlist item ID.
	typedef std::map<UINT,long> PlaylistMenuItemMap;

	// Shows the context menu.
	void ShowContextMenu();

	// Returns a 'playlist' menu, or a NULL menu if the playlist is empty.
	HMENU CreatePlaylistMenu( const Playlist::Ptr& playlist );

	// Does a 'command'.
	void DoCommand( const Settings::SystrayCommand command );

	// Maps a volume menu ID to a volume level.
	static std::map<UINT,float> s_VolumeMenuMap;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Media library.
	Library& m_Library;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Notification data.
	NOTIFYICONDATA m_NotifyIconData;

	// Indicates whether the notification are icon is shown.
	bool m_IsShown;

	// Default notification area icon tooltip.
	std::wstring m_DefaultTooltip;

	// Tree control.
	WndTree& m_Tree;
	
	// List control.
	WndList& m_List;

	// Maps a context menu command ID to a playlist item ID.
	PlaylistMenuItemMap m_PlaylistMenuItems;

	// The next available playlist menu item ID.
	UINT m_NextPlaylistMenuItemID;
};
