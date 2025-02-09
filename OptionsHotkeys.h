#pragma once

#include "Hotkeys.h"
#include "Options.h"

class OptionsHotkeys : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsHotkeys( HINSTANCE instance, Settings& settings, Output& output );

	// Called when the options page should be initialised.
	// 'hwnd' - dialog window handle.
	void OnInit( const HWND hwnd ) override;

	// Called when the options page should be saved.
	// 'hwnd' - dialog window handle.
	void OnSave( const HWND hwnd ) override;

	// Called when a command on the options page should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) override;

	// Called when a notification message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - notification parameter.
	// 'lParam' - notification parameter.
	void OnNotify( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) override;

private:
	// Maps a hotkey ID to its settings.
	typedef std::map<Hotkeys::ID, Settings::Hotkey> HotkeyMap;

	// Maps a hotkey code to a string resource ID.
	typedef std::map<int, UINT> HotkeyNameMap;

	// Returns a 'hotkey' description.
	std::wstring GetKeyDescription( const Settings::Hotkey& hotkey ) const;

	// Launches the hotkey assignment dialog.
	// 'hwnd' - dialog window handle.
	void AssignHotkey( const HWND hwnd );

	// Clears the hotkey for the current selection.
	// 'hwnd' - dialog window handle.
	void ClearHotkey( const HWND hwnd );

	// Maps a hotkey ID to its settings.
	HotkeyMap m_HotkeyMap;

	// Maps a hotkey code to a string resource ID (used for certain extended keys, when querying the key name fails).
	static const HotkeyNameMap s_KeyNames;
};
