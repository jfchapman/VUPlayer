#include "OptionsReplaygain.h"

#include "resource.h"
#include "windowsx.h"

// Maps an index to a pre-amp dB value.
static const std::map<int,float> sPreampValues = {
	{ 0, 12.0f },
	{ 1, 11.0f },
	{ 2, 10.0f },
	{ 3, 9.0f },
	{ 4, 8.0f },
	{ 5, 7.0f },
	{ 6, 6.0f },
	{ 7, 5.0f },
	{ 8, 4.0f },
	{ 9, 3.0f },
	{ 10, 2.0f },
	{ 11, 1.0f },
	{ 12, 0.0f }
};

OptionsReplaygain::OptionsReplaygain( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

OptionsReplaygain::~OptionsReplaygain()
{
}

void OptionsReplaygain::OnInit( const HWND hwnd )
{
	Settings::ReplaygainMode mode = Settings::ReplaygainMode::Disabled;
	float preamp = 0;
	bool hardlimit = false;
	GetSettings().GetReplaygainSettings( mode, preamp, hardlimit );

	switch ( mode ) {
		case Settings::ReplaygainMode::Disabled : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_DISABLE ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMPLABEL ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIPLABEL ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ), FALSE );
			break;
		}
		case Settings::ReplaygainMode::Track : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLETRACK ), BST_CHECKED );
			break;
		}
		case Settings::ReplaygainMode::Album : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM ), BST_CHECKED );
			break;
		}
	}

	const int bufSize = MAX_PATH;
	WCHAR buf[ bufSize ] = {};
	LoadString( GetInstanceHandle(), IDS_UNITS_DB, buf, bufSize );
	HWND hwndPreamp = GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP );
	for ( const auto& iter : sPreampValues ) {
		const int dbValue = static_cast<int>( iter.second );
		const std::wstring dbStr = L"+" + std::to_wstring( dbValue ) + L" " + buf;
		const int itemIndex = ComboBox_AddString( hwndPreamp, dbStr.c_str() );
		if ( iter.second == preamp ) {
			ComboBox_SetCurSel( hwndPreamp, itemIndex );
		}
	}

	HWND hwndClip = GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP );
	LoadString( GetInstanceHandle(), IDS_CLIPPREVENT_NONE, buf, bufSize );
	ComboBox_AddString( hwndClip, buf );
	LoadString( GetInstanceHandle(), IDS_CLIPPREVENT_HARDLIMIT, buf, bufSize );
	ComboBox_AddString( hwndClip, buf );
	ComboBox_SetCurSel( hwndClip, hardlimit ? 1 : 0 );
}

void OptionsReplaygain::OnSave( const HWND hwnd )
{
	Settings::ReplaygainMode mode = Settings::ReplaygainMode::Disabled;
	float preamp = 0;
	bool hardlimit = false;
	GetSettings().GetReplaygainSettings( mode, preamp, hardlimit );

	if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_DISABLE ) ) ) {
		mode = Settings::ReplaygainMode::Disabled;
	} else if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLETRACK ) ) ) {
		mode = Settings::ReplaygainMode::Track;
	} else if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM ) ) ) {
		mode = Settings::ReplaygainMode::Album;
	}

	const int selectedPreamp = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ) );
	const auto preampIter = sPreampValues.find( selectedPreamp );
	if ( sPreampValues.end() != preampIter ) {
		preamp = preampIter->second;
	}

	const int selectedClip = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ) );
	if ( CB_ERR != selectedClip ) {
		hardlimit = ( 1 == selectedClip );
	}

	GetSettings().SetReplaygainSettings( mode, preamp, hardlimit );
}

void OptionsReplaygain::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		const WORD controlID = LOWORD( wParam );
		switch ( controlID ) {
			case IDC_OPTIONS_REPLAYGAIN_DISABLE : {
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), FALSE );
				if ( 0 != IsWindowEnabled( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ) ) ) {
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMPLABEL ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIPLABEL ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ), FALSE );
				}
				break;
			}
			case IDC_OPTIONS_REPLAYGAIN_ENABLETRACK :
			case IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM : {
				if ( 0 == IsWindowEnabled( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ) ) ) {
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMPLABEL ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIPLABEL ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ), TRUE );
				}
				break;
			}
			case IDC_OPTIONS_REPLAYGAIN_RESET : {
				Settings::ReplaygainMode mode = Settings::ReplaygainMode::Disabled;
				float preamp = 0;
				bool hardlimit = false;
				GetSettings().GetDefaultReplaygainSettings( mode, preamp, hardlimit );
				switch ( mode ) {
					case Settings::ReplaygainMode::Disabled : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_DISABLE ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLETRACK ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM ), BST_UNCHECKED );
						break;
					}
					case Settings::ReplaygainMode::Track : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLETRACK ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_DISABLE ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM ), BST_UNCHECKED );
						break;
					}
					case Settings::ReplaygainMode::Album : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLEALBUM ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_DISABLE ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_ENABLETRACK ), BST_UNCHECKED );
						break;
					}
				}
				for ( const auto& iter : sPreampValues ) {
					if ( iter.second == preamp ) {
						ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ), iter.first );
						break;
					}
				}
				ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ), hardlimit ? 1 : 0 );
				const BOOL enable = ( Settings::ReplaygainMode::Disabled != mode );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_OPTIONSGROUP ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMPLABEL ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_PREAMP ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIPLABEL ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_REPLAYGAIN_CLIP ), enable );
				break;
			}
			default : {
				break;
			}
		}
	}
}
