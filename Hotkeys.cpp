#include "Hotkeys.h"

#include "resource.h"

// All hotkey IDs.
std::set<Hotkeys::ID> Hotkeys::s_Hotkeys = {
	ID::Play,
	ID::Stop,
	ID::Previous,
	ID::Next,
	ID::StopAtTrackEnd,
	ID::FadeOut,
	ID::FadeToNext,
	ID::MuteVolume,
	ID::DecreaseVolume,
	ID::IncreaseVolume,
	ID::SkipBackwards,
	ID::SkipForwards,
	ID::DecreasePitch,
	ID::IncreasePitch,
	ID::ResetPitch,
	ID::BalanceLeft,
	ID::BalanceRight,
	ID::ResetBalance,
};

// Hotkey commands.
std::map<Hotkeys::ID,WPARAM> Hotkeys::s_CommandMap = {
	{ ID::Play,						ID_CONTROL_PLAY },
	{ ID::Stop,						ID_CONTROL_STOP },
	{ ID::Previous,				ID_CONTROL_PREVIOUSTRACK },
	{ ID::Next,						ID_CONTROL_NEXTTRACK },
	{ ID::SkipBackwards,	ID_CONTROL_SKIPBACKWARDS },
	{ ID::SkipForwards,		ID_CONTROL_SKIPFORWARDS },
	{ ID::StopAtTrackEnd,	ID_CONTROL_STOPTRACKEND },
	{ ID::FadeOut,				ID_CONTROL_FADEOUT },
	{ ID::FadeToNext,			ID_CONTROL_FADETONEXT },
	{ ID::MuteVolume,			ID_CONTROL_MUTE },
	{ ID::DecreaseVolume,	ID_CONTROL_VOLUMEDOWN },
	{ ID::IncreaseVolume,	ID_CONTROL_VOLUMEUP },
	{ ID::DecreasePitch,	ID_CONTROL_PITCHDOWN },
	{ ID::IncreasePitch,	ID_CONTROL_PITCHUP },
	{ ID::ResetPitch,     ID_CONTROL_PITCHRESET },
	{ ID::BalanceLeft,	  ID_CONTROL_BALANCELEFT },
	{ ID::BalanceRight,	  ID_CONTROL_BALANCERIGHT },
	{ ID::ResetBalance,   ID_CONTROL_BALANCERESET }
};

// Hotkey descriptions.
std::map<Hotkeys::ID,UINT> Hotkeys::s_DescriptionMap = {
	{ ID::Play,						IDS_HOTKEY_PLAY },
	{ ID::Stop,						IDS_HOTKEY_STOP },
	{ ID::Previous,				IDS_HOTKEY_PREVIOUS },
	{ ID::Next,						IDS_HOTKEY_NEXT },
	{ ID::SkipBackwards,	IDS_HOTKEY_SKIPBACKWARDS },
	{ ID::SkipForwards,		IDS_HOTKEY_SKIPFORWARDS },
	{ ID::StopAtTrackEnd,	IDS_HOTKEY_STOPATEND },
	{ ID::FadeOut,				IDS_HOTKEY_FADEOUT },
	{ ID::FadeToNext,			IDS_HOTKEY_FADETONEXT },
	{ ID::MuteVolume,			IDS_HOTKEY_MUTE },
	{ ID::DecreaseVolume,	IDS_HOTKEY_VOLUMEDOWN },
	{ ID::IncreaseVolume,	IDS_HOTKEY_VOLUMEUP },
	{ ID::DecreasePitch,	IDS_HOTKEY_PITCHDOWN },
	{ ID::IncreasePitch,	IDS_HOTKEY_PITCHUP },
	{ ID::ResetPitch,     IDS_HOTKEY_PITCHRESET },
	{ ID::BalanceLeft,	  IDS_HOTKEY_BALANCELEFT },
	{ ID::BalanceRight,	  IDS_HOTKEY_BALANCERIGHT },
	{ ID::ResetBalance,   IDS_HOTKEY_BALANCERESET }
};

Hotkeys::Hotkeys( const HWND wnd, Settings& settings ) :
	m_hWnd( wnd ),
	m_Settings( settings ),
	m_RegisteredHotkeys()
{
	Update();
}

Hotkeys::~Hotkeys()
{
	Unregister();
}

void Hotkeys::Update()
{
	Unregister();
	bool enable = false;
	Settings::HotkeyList hotkeys;
	m_Settings.GetHotkeySettings( enable, hotkeys );
	if ( enable ) {
		for ( const auto& hotkey : hotkeys ) {
			if ( ( 0 != hotkey.ID ) && ( 0 != hotkey.Code ) ) {
				UINT modifiers = 0;
				if ( hotkey.Alt ) {
					modifiers |= MOD_ALT;
				}
				if ( hotkey.Ctrl ) {
					modifiers |= MOD_CONTROL;
				}
				if ( hotkey.Shift ) {
					modifiers |= MOD_SHIFT;
				}
				if ( FALSE != RegisterHotKey( m_hWnd, hotkey.ID, modifiers, static_cast<UINT>( hotkey.Code ) ) ) {
					m_RegisteredHotkeys.insert( hotkey.ID );
				}
			}
		}
	}
}

void Hotkeys::Unregister()
{
	for ( const auto& id : m_RegisteredHotkeys ) {
		UnregisterHotKey( m_hWnd, id );
	}
	m_RegisteredHotkeys.clear();
}

void Hotkeys::OnHotkey( const WPARAM wParam )
{
	const ID id = static_cast<ID>( wParam );
	const auto iter = s_CommandMap.find( id );
	if ( s_CommandMap.end() != iter ) {
		const WPARAM command = iter->second;
		SendMessage( m_hWnd, WM_COMMAND, command, 0 );
	}
}

std::wstring Hotkeys::GetDescription( const HINSTANCE instance, const ID id )
{
	std::wstring description;
	const auto iter = s_DescriptionMap.find( id );
	if ( s_DescriptionMap.end() != iter ) {
		const UINT stringID = iter->second;
		const int bufSize = 64;
		WCHAR buffer[ bufSize ] = {};
		LoadString( instance, stringID, buffer, bufSize );
		description = buffer;
	}
	return description;
}

std::set<Hotkeys::ID> Hotkeys::GetHotkeyIDs()
{
	return s_Hotkeys;
}
