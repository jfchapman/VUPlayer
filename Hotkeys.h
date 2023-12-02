#pragma once

#include "stdafx.h"

#include "Settings.h"

class Hotkeys
{
public:
	// Hotkey identifiers
	enum class ID {
		Play = 1,
		Stop,
		Previous,
		Next,
		SkipBackwards,
		SkipForwards,
		StopAtTrackEnd,
		FadeOut,
		FadeToNext,
		MuteVolume,
		DecreaseVolume,
		IncreaseVolume,
		DecreasePitch,
		IncreasePitch,
		ResetPitch,
		BalanceLeft,
		BalanceRight,
		ResetBalance,
    FollowTrackSelection
	};

	// Constructor.
	// 'wnd' - application window handle.
	// 'settings' - application settings.
	Hotkeys( const HWND wnd, Settings& settings );

	virtual ~Hotkeys();

	// Updates the currently registered hotkeys based on the current application settings.
	void Update();

	// Unregisters all hotkeys.
	void Unregister();

	// Called when a hotkey message is received.
	// 'wParam' - hotkey identifier.
	void OnHotkey( const WPARAM wParam );

	// Returns a description of a hotkey.
	// 'instance' - module instance handle.
	// 'id' - hotkey ID.
	static std::wstring GetDescription( const HINSTANCE instance, const ID id );

	// Returns all the hotkey IDs.
	static std::set<ID> GetHotkeyIDs();

private:
	// Application window handle.
	HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Registered hotkey IDs.
	std::set<int> m_RegisteredHotkeys;

	// All hotkey IDs.
	static std::set<ID> s_Hotkeys;

	// Maps a hotkey ID to a command ID.
	static std::map<ID,WPARAM> s_CommandMap;

	// Maps a hotkey ID to a string resource ID.
	static std::map<ID,UINT> s_DescriptionMap;
};

