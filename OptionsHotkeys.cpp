#include "OptionsHotkeys.h"

#include "DlgHotkey.h"

#include "resource.h"
#include "windowsx.h"

#include <sstream>

const OptionsHotkeys::HotkeyNameMap OptionsHotkeys::s_KeyNames = {
	{ VK_VOLUME_MUTE,				IDS_KEYNAME_VOLUMEMUTE },
	{ VK_VOLUME_DOWN,				IDS_KEYNAME_VOLUMEDOWN },
	{ VK_VOLUME_UP,					IDS_KEYNAME_VOLUMEUP },
	{ VK_MEDIA_NEXT_TRACK,	IDS_KEYNAME_NEXTTRACK },
	{ VK_MEDIA_PREV_TRACK,	IDS_KEYNAME_PREVIOUSTRACK },
	{ VK_MEDIA_STOP,				IDS_KEYNAME_STOP },
	{ VK_MEDIA_PLAY_PAUSE,	IDS_KEYNAME_PLAYPAUSE }
};

OptionsHotkeys::OptionsHotkeys( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output ),
	m_HotkeyMap()
{
	bool enable = false;
	Settings::HotkeyList hotkeys;
	settings.GetHotkeySettings( enable, hotkeys );
	for ( const auto& hotkey : hotkeys ) {
		m_HotkeyMap.insert( HotkeyMap::value_type( static_cast<Hotkeys::ID>( hotkey.ID ), hotkey ) );
	}
}

void OptionsHotkeys::OnInit( const HWND hwnd )
{
	bool enable = false;
	Settings::HotkeyList hotkeyList;
	GetSettings().GetHotkeySettings( enable, hotkeyList );
	Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_ENABLE ), enable ? BST_CHECKED : BST_UNCHECKED );

	const HWND listWnd = GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_LIST );
	ListView_SetExtendedListViewStyle( listWnd, LVS_EX_FULLROWSELECT );
	RECT rect = {};
	GetClientRect( listWnd, &rect );
	LVCOLUMN lvc = {};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.cx = ( rect.right - rect.left - 40 ) / 2;
	lvc.fmt = LVCFMT_LEFT;
	lvc.iSubItem = 0;
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( GetInstanceHandle(), IDS_OPTIONS_HOTKEY_ACTION, buffer, bufSize );
	lvc.pszText = buffer;
	ListView_InsertColumn( listWnd, 0, &lvc );
	lvc.iSubItem = 1;
	LoadString( GetInstanceHandle(), IDS_OPTIONS_HOTKEY_KEY, buffer, bufSize );
	ListView_InsertColumn( listWnd, 1, &lvc );

	const std::set<Hotkeys::ID> hotkeys = Hotkeys::GetHotkeyIDs();
	for ( const auto& hotkey : hotkeys ) {
		LVITEM item = {};
		item.mask = LVIF_PARAM;
		item.iItem = ListView_GetItemCount( listWnd );
		item.iSubItem = 0;
		item.lParam = static_cast<LPARAM>( hotkey );
		item.iItem = ListView_InsertItem( listWnd, &item );
		if( item.iItem >= 0 ) {
			const std::wstring description = Hotkeys::GetDescription( GetInstanceHandle(), hotkey );
			ListView_SetItemText( listWnd, item.iItem, 0, const_cast<LPWSTR>( description.c_str() ) );
			const auto iter = m_HotkeyMap.find( hotkey );
			if ( m_HotkeyMap.end() != iter ) {
				const std::wstring keystroke = GetKeyDescription( iter->second );
				ListView_SetItemText( listWnd, item.iItem, 1, const_cast<LPWSTR>( keystroke.c_str() ) );
			}
		}
	}
}

void OptionsHotkeys::OnSave( const HWND hwnd )
{
	const bool enable = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_ENABLE ) ) );
	Settings::HotkeyList hotkeys;
	for ( const auto iter : m_HotkeyMap ) {
		hotkeys.push_back( iter.second );
	}
	GetSettings().SetHotkeySettings( enable, hotkeys );
}

void OptionsHotkeys::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		const WORD controlID = LOWORD( wParam );
		switch ( controlID ) {
			case IDC_OPTIONS_HOTKEYS_CLEAR : {
				ClearHotkey( hwnd );
				break;
			}
			case IDC_OPTIONS_HOTKEYS_ASSIGN : {
				AssignHotkey( hwnd );
				break;
			}
			default : {
				break;
			}
		}
	}
}

void OptionsHotkeys::OnNotify( const HWND hwnd, const WPARAM /*wParam*/, const LPARAM lParam )
{
	const HWND listWnd = GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_LIST );
	LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
	if ( nullptr != nmhdr ) {
		if ( listWnd == nmhdr->hwndFrom ) {
			if ( NM_DBLCLK == nmhdr->code ) {
				AssignHotkey( hwnd );
			}
		}
	}
}

std::wstring OptionsHotkeys::GetKeyDescription( const Settings::Hotkey& hotkey ) const
{
	std::wstring description;
	const int bufSize = 64;
	WCHAR buffer[ bufSize ] = {};
	if ( hotkey.Alt ) {
		LoadString( GetInstanceHandle(), IDS_ALT, buffer, bufSize );
		description += buffer;
		description += L"+";
	}
	if ( hotkey.Ctrl ) {
		LoadString( GetInstanceHandle(), IDS_CONTROL, buffer, bufSize );
		description += buffer;
		description += L"+";
	}
	if ( hotkey.Shift ) {
		LoadString( GetInstanceHandle(), IDS_SHIFT, buffer, bufSize );
		description += buffer;
		description += L"+";
	}
	if ( hotkey.Name.empty() ) {
		const auto nameIter = s_KeyNames.find( hotkey.Code );
		if ( s_KeyNames.end() != nameIter ) {
			LoadString( GetInstanceHandle(), nameIter->second, buffer, bufSize );
			description += buffer;
		} else {
			const UINT scancode = MapVirtualKey( hotkey.Code, MAPVK_VK_TO_VSC );
			if ( 0 != GetKeyNameText( scancode << 16, buffer, bufSize ) ) {
				description += buffer;
			} else {
				std::wstringstream ss;
				ss << L'[' << std::hex << std::showbase << hotkey.Code << L']';
				description += ss.str();
			}
		}
	} else {
		description += hotkey.Name;
	}
	return description;
}

void OptionsHotkeys::AssignHotkey( const HWND hwnd )
{
	DlgHotkey dialog( GetInstanceHandle(), hwnd );
	Settings::Hotkey hotkey = {};
	if ( dialog.GetHotkey( hotkey ) ) {
		const std::wstring description = GetKeyDescription( hotkey );
		const HWND listWnd = GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_LIST );
		if ( nullptr != listWnd ) {
			const int itemIndex = ListView_GetNextItem( listWnd, -1, LVNI_SELECTED );
			if ( itemIndex >= 0 ) {
				LVITEM item = {};
				item.mask = LVIF_PARAM;
				item.iItem = itemIndex;
				if ( TRUE == ListView_GetItem( listWnd, &item ) ) {
					hotkey.ID = static_cast<int>( item.lParam );
					const Hotkeys::ID id = static_cast<Hotkeys::ID>( item.lParam );
					m_HotkeyMap.erase( id );
					m_HotkeyMap.insert( HotkeyMap::value_type( id, hotkey ) );
					ListView_SetItemText( listWnd, itemIndex, 1, const_cast<LPWSTR>( description.c_str() ) );
				}
			}
		}
	}
}

void OptionsHotkeys::ClearHotkey( const HWND hwnd )
{
	const HWND listWnd = GetDlgItem( hwnd, IDC_OPTIONS_HOTKEYS_LIST );
	if ( nullptr != listWnd ) {
		const int itemIndex = ListView_GetNextItem( listWnd, -1, LVNI_SELECTED );
		if ( itemIndex >= 0 ) {
			LVITEM item = {};
			item.mask = LVIF_PARAM;
			item.iItem = itemIndex;
			if ( TRUE == ListView_GetItem( listWnd, &item ) ) {
				const Hotkeys::ID id = static_cast<Hotkeys::ID>( item.lParam );
				m_HotkeyMap.erase( id );
				const std::wstring blank;
				ListView_SetItemText( listWnd, itemIndex, 1, const_cast<LPWSTR>( blank.c_str() ) );
			}
		}
	}
}