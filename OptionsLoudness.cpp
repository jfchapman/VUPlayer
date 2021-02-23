#include "OptionsLoudness.h"

#include "resource.h"
#include "windowsx.h"

// Maps an index to a pre-amp dB value.
static const std::map<int,int> sPreampValues = {
	{ 0,	+12 },
	{ 1,	+11 },
	{ 2,	+10 },
	{ 3,	+9 },
	{ 4,	+8 },
	{ 5,	+7 },
	{ 6,	+6 },
	{ 7,	+5 },
	{ 8,	+4 },
	{ 9,	+3 },
	{ 10,	+2 },
	{ 11, +1 },
	{ 12, +0 },
	{ 13, -1 },
	{ 14, -2 },
	{ 15, -3 },
	{ 16, -4 },
	{ 17, -5 },
	{ 18, -6 }
};

OptionsLoudness::OptionsLoudness( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

void OptionsLoudness::OnInit( const HWND hwnd )
{
	Settings::GainMode gainMode = Settings::GainMode::Disabled;
	Settings::LimitMode limitMode = Settings::LimitMode::None;
	float preamp = 0;
	GetSettings().GetGainSettings( gainMode, limitMode, preamp );

	switch ( gainMode ) {
		case Settings::GainMode::Disabled : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_DISABLE ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMPLABEL ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIPLABEL ), FALSE );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), FALSE );
			break;
		}
		case Settings::GainMode::Track : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLETRACK ), BST_CHECKED );
			break;
		}
		case Settings::GainMode::Album : {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLEALBUM ), BST_CHECKED );
			break;
		}
	}

	const int bufSize = MAX_PATH;
	WCHAR buf[ bufSize ] = {};
	LoadString( GetInstanceHandle(), IDS_UNITS_DB, buf, bufSize );
	HWND hwndPreamp = GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP );
	for ( const auto& iter : sPreampValues ) {
		const int dbValue = static_cast<int>( iter.second );
		std::wstring dbStr;
		if ( dbValue >= 0 ) {
			dbStr += L"+";
		}
		dbStr += std::to_wstring( dbValue ) + L" " + buf;
		const int itemIndex = ComboBox_AddString( hwndPreamp, dbStr.c_str() );
		if ( iter.second == preamp ) {
			ComboBox_SetCurSel( hwndPreamp, itemIndex );
		}
	}

	HWND hwndClip = GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP );
	LoadString( GetInstanceHandle(), IDS_CLIPPREVENT_NONE, buf, bufSize );
	ComboBox_AddString( hwndClip, buf );
	LoadString( GetInstanceHandle(), IDS_CLIPPREVENT_HARDLIMIT, buf, bufSize );
	ComboBox_AddString( hwndClip, buf );
	LoadString( GetInstanceHandle(), IDS_CLIPPREVENT_SOFTLIMIT, buf, bufSize );
	ComboBox_AddString( hwndClip, buf );
	switch ( limitMode ) {
		case Settings::LimitMode::None : {
			ComboBox_SetCurSel( hwndClip, 0 );
			break;
		}
		case Settings::LimitMode::Hard : {
			ComboBox_SetCurSel( hwndClip, 1 );
			break;
		}
		case Settings::LimitMode::Soft : {
			ComboBox_SetCurSel( hwndClip, 2 );
			break;
		}
	}
}

void OptionsLoudness::OnSave( const HWND hwnd )
{
	Settings::GainMode gainMode = Settings::GainMode::Disabled;
	Settings::LimitMode limitMode = Settings::LimitMode::None;
	float preamp = 0;
	GetSettings().GetGainSettings( gainMode, limitMode, preamp );

	if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_DISABLE ) ) ) {
		gainMode = Settings::GainMode::Disabled;
	} else if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLETRACK ) ) ) {
		gainMode = Settings::GainMode::Track;
	} else if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLEALBUM ) ) ) {
		gainMode = Settings::GainMode::Album;
	}

	const int selectedPreamp = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ) );
	const auto preampIter = sPreampValues.find( selectedPreamp );
	if ( sPreampValues.end() != preampIter ) {
		preamp = static_cast<float>( preampIter->second );
	}

	const int selectedClip = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ) );
	switch ( selectedClip ) {
		case 0 : {
			limitMode = Settings::LimitMode::None;
			break;
		}
		case 1 : {
			limitMode = Settings::LimitMode::Hard;
			break;
		}
		case 2 : {
			limitMode = Settings::LimitMode::Soft;
			break;
		}
	}
	GetSettings().SetGainSettings( gainMode, limitMode, preamp );
}

void OptionsLoudness::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		const WORD controlID = LOWORD( wParam );
		switch ( controlID ) {
			case IDC_OPTIONS_GAIN_DISABLE : {
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), FALSE );
				if ( 0 != IsWindowEnabled( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ) ) ) {
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMPLABEL ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIPLABEL ), FALSE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), FALSE );
				}
				break;
			}
			case IDC_OPTIONS_GAIN_ENABLETRACK :
			case IDC_OPTIONS_GAIN_ENABLEALBUM : {
				if ( 0 == IsWindowEnabled( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ) ) ) {
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMPLABEL ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIPLABEL ), TRUE );
					EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), TRUE );
				}
				break;
			}
			case IDC_OPTIONS_GAIN_RESET : {
				Settings::GainMode gainMode = Settings::GainMode::Disabled;
				Settings::LimitMode limitMode = Settings::LimitMode::None;
				float preamp = 0;
				GetSettings().GetDefaultGainSettings( gainMode, limitMode, preamp );
				switch ( gainMode ) {
					case Settings::GainMode::Disabled : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_DISABLE ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLETRACK ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLEALBUM ), BST_UNCHECKED );
						break;
					}
					case Settings::GainMode::Track : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLETRACK ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_DISABLE ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLEALBUM ), BST_UNCHECKED );
						break;
					}
					case Settings::GainMode::Album : {
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLEALBUM ), BST_CHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_DISABLE ), BST_UNCHECKED );
						Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_ENABLETRACK ), BST_UNCHECKED );
						break;
					}
				}
				for ( const auto& iter : sPreampValues ) {
					if ( iter.second == static_cast<int>( preamp ) ) {
						ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ), iter.first );
						break;
					}
				}
				switch ( limitMode ) {
					case Settings::LimitMode::None : {
						ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), 0 );
						break;
					}
					case Settings::LimitMode::Hard : {
						ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), 1 );
						break;
					}
					case Settings::LimitMode::Soft : {
						ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), 2 );
						break;
					}
				}
				const BOOL enable = ( Settings::GainMode::Disabled != gainMode );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_OPTIONSGROUP ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMPLABEL ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_PREAMP ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIPLABEL ), enable );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GAIN_CLIP ), enable );
				break;
			}
			default : {
				break;
			}
		}
	}
}
