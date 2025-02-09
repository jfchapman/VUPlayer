#include "OptionsGeneral.h"

#include "resource.h"
#include "VUPlayer.h"
#include "windowsx.h"

#include "DlgAdvancedASIO.h"
#include "DlgAdvancedWasapi.h"

#include <array>

std::vector<std::pair<Settings::OutputMode, int>> OptionsGeneral::s_OutputModes = {
	std::make_pair( Settings::OutputMode::Standard,             IDS_OPTIONS_MODE_STANDARD ),
	std::make_pair( Settings::OutputMode::WASAPIExclusive,      IDS_OPTIONS_MODE_WASAPI_EXCLUSIVE ),
	std::make_pair( Settings::OutputMode::ASIO,                 IDS_OPTIONS_MODE_ASIO )
};

std::vector<std::pair<Settings::TitleBarFormat, int>> OptionsGeneral::s_TitleBarFormats = {
	std::make_pair( Settings::TitleBarFormat::ArtistTitle,      IDS_OPTIONS_TITLEBAR_ARTISTTITLE ),
	std::make_pair( Settings::TitleBarFormat::TitleArtist,      IDS_OPTIONS_TITLEBAR_TITLEARTIST ),
	std::make_pair( Settings::TitleBarFormat::ComposerTitle,    IDS_OPTIONS_TITLEBAR_COMPOSERTITLE ),
	std::make_pair( Settings::TitleBarFormat::TitleComposer,    IDS_OPTIONS_TITLEBAR_TITLECOMPOSER ),
	std::make_pair( Settings::TitleBarFormat::Title,            IDS_OPTIONS_TITLEBAR_TITLE ),
	std::make_pair( Settings::TitleBarFormat::ApplicationName,  IDS_OPTIONS_TITLEBAR_APPNAME )
};

OptionsGeneral::OptionsGeneral( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

void OptionsGeneral::OnInit( const HWND hwnd )
{
	// Output settings
	std::wstring settingDevice;
	Settings::OutputMode settingMode;
	GetSettings().GetOutputSettings( settingDevice, settingMode );

	if ( const HWND hwndMode = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MODE ); nullptr != hwndMode ) {
		const int bufferSize = MAX_PATH;
		WCHAR buffer[ bufferSize ];
		const HINSTANCE instance = GetInstanceHandle();
		for ( const auto& mode : s_OutputModes ) {
			if ( !GetOutput().GetDevices( mode.first ).empty() ) {
				LoadString( instance, mode.second, buffer, bufferSize );
				ComboBox_AddString( hwndMode, buffer );
				ComboBox_SetItemData( hwndMode, ComboBox_GetCount( hwndMode ) - 1, static_cast<LPARAM>( mode.first ) );
				if ( mode.first == settingMode ) {
					ComboBox_SetCurSel( hwndMode, ComboBox_GetCount( hwndMode ) - 1 );
				}
			}
		}
		if ( -1 == ComboBox_GetCurSel( hwndMode ) ) {
			if ( ComboBox_GetCount( hwndMode ) > 0 ) {
				ComboBox_SetCurSel( hwndMode, 0 );
			} else {
				EnableWindow( hwndMode, FALSE );
			}
		}
	}
	RefreshOutputDeviceList( hwnd );

	// Miscellaneous settings
	const auto settingTitleBar = GetSettings().GetTitleBarFormat();
	if ( const HWND hwndTitleBar = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_TITLEBAR ); nullptr != hwndTitleBar ) {
		const int bufferSize = MAX_PATH;
		WCHAR buffer[ bufferSize ];
		const HINSTANCE instance = GetInstanceHandle();
		for ( const auto& [format, resourceID] : s_TitleBarFormats ) {
			LoadString( instance, resourceID, buffer, bufferSize );
			ComboBox_AddString( hwndTitleBar, buffer );
			ComboBox_SetItemData( hwndTitleBar, ComboBox_GetCount( hwndTitleBar ) - 1, static_cast<LPARAM>( format ) );
			if ( format == settingTitleBar ) {
				ComboBox_SetCurSel( hwndTitleBar, ComboBox_GetCount( hwndTitleBar ) - 1 );
			}
		}
		if ( -1 == ComboBox_GetCurSel( hwndTitleBar ) ) {
			ComboBox_SetCurSel( hwndTitleBar, 0 );
		}
	}

	const bool mergeDuplicates = GetSettings().GetMergeDuplicates();
	if ( const HWND hwndMergeDuplicates = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES ); nullptr != hwndMergeDuplicates ) {
		Button_SetCheck( hwndMergeDuplicates, ( mergeDuplicates ? BST_CHECKED : BST_UNCHECKED ) );
	}

	const bool retainStopAtTrackEnd = GetSettings().GetRetainStopAtTrackEnd();
	if ( const HWND hwndRetainStopAtTrackEnd = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_RETAIN_STOPATTRACKEND ); nullptr != hwndRetainStopAtTrackEnd ) {
		Button_SetCheck( hwndRetainStopAtTrackEnd, ( retainStopAtTrackEnd ? BST_CHECKED : BST_UNCHECKED ) );
	}

	const bool retainPitchBalance = GetSettings().GetRetainPitchBalance();
	if ( const HWND hwndRetainPitchBalance = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_RETAIN_PITCHBALANCE ); nullptr != hwndRetainPitchBalance ) {
		Button_SetCheck( hwndRetainPitchBalance, ( retainPitchBalance ? BST_CHECKED : BST_UNCHECKED ) );
	}

	// Remote settings
	if ( const HWND hwndMusicBrainz = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MUSICBRAINZ_ENABLE ); nullptr != hwndMusicBrainz ) {
		const bool musicbrainzEnabled = GetSettings().GetMusicBrainzEnabled();
		Button_SetCheck( hwndMusicBrainz, ( musicbrainzEnabled ? BST_CHECKED : BST_UNCHECKED ) );
	}

	VUPlayer* vuplayer = VUPlayer::Get();
	const bool scrobblerAvailable = ( nullptr != vuplayer ) && vuplayer->IsScrobblerAvailable();
	const HWND hwndScrobblerEnable = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SCROBBLER_ENABLE );
	if ( nullptr != hwndScrobblerEnable ) {
		const bool scrobblerEnabled = GetSettings().GetScrobblerEnabled();
		Button_SetCheck( hwndScrobblerEnable, ( scrobblerEnabled ? BST_CHECKED : BST_UNCHECKED ) );
		EnableWindow( hwndScrobblerEnable, scrobblerAvailable ? TRUE : FALSE );
	}
	const HWND hwndScrobblerAuthorise = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SCROBBLER_AUTHORISE );
	if ( ( nullptr != hwndScrobblerAuthorise ) && !scrobblerAvailable ) {
		ShowWindow( hwndScrobblerAuthorise, SW_HIDE );
	}

	// Tagging settings
	const bool writeFileTags = GetSettings().GetWriteFileTags();
	if ( const HWND hwndWriteFileTags = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_WRITETAGS ); nullptr != hwndWriteFileTags ) {
		Button_SetCheck( hwndWriteFileTags, ( writeFileTags ? BST_CHECKED : BST_UNCHECKED ) );
	}

	if ( const HWND hwndPreserveFiletime = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_PRESERVE_FILETIME ); nullptr != hwndPreserveFiletime ) {
		const bool preserveFiletime = GetSettings().GetPreserveLastModifiedTime();
		Button_SetCheck( hwndPreserveFiletime, ( preserveFiletime ? BST_CHECKED : BST_UNCHECKED ) );
		EnableWindow( hwndPreserveFiletime, writeFileTags ? TRUE : FALSE );
	}
}

void OptionsGeneral::OnSave( const HWND hwnd )
{
	// Output device settings
	const Settings::OutputMode mode = GetSelectedMode( hwnd );
	const std::wstring device = GetSelectedDeviceName( hwnd );
	GetSettings().SetOutputSettings( device, mode );

	// Miscellaneous settings
	const auto titleBarFormat = GetSelectedTitleBarFormat( hwnd );
	GetSettings().SetTitleBarFormat( titleBarFormat );

	const bool mergeDuplicates = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES ) ) );
	GetSettings().SetMergeDuplicates( mergeDuplicates );

	const bool retainStopAtTrackEnd = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_RETAIN_STOPATTRACKEND ) ) );
	GetSettings().SetRetainStopAtTrackEnd( retainStopAtTrackEnd );

	const bool retainPitchBalance = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_RETAIN_PITCHBALANCE ) ) );
	GetSettings().SetRetainPitchBalance( retainPitchBalance );

	// Remote settings
	const bool musicbrainzEnabled = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MUSICBRAINZ_ENABLE ) ) );
	GetSettings().SetMusicBrainzEnabled( musicbrainzEnabled );

	const bool enableScrobbler = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SCROBBLER_ENABLE ) ) );
	GetSettings().SetScrobblerEnabled( enableScrobbler );

	// Tagging settings
	const bool writeFileTags = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_WRITETAGS ) ) );
	GetSettings().SetWriteFileTags( writeFileTags );

	const bool preserveFiletime = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_PRESERVE_FILETIME ) ) );
	GetSettings().SetPreserveLastModifiedTime( preserveFiletime );
}

void OptionsGeneral::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	const WORD controlID = LOWORD( wParam );
	if ( CBN_SELCHANGE == notificationCode ) {
		if ( IDC_OPTIONS_GENERAL_MODE == controlID ) {
			RefreshOutputDeviceList( hwnd );
		}
	} else if ( BN_CLICKED == notificationCode ) {
		switch ( controlID ) {
			case IDC_OPTIONS_MODE_ADVANCED: {
				const Settings::OutputMode mode = GetSelectedMode( hwnd );
				switch ( mode ) {
					case Settings::OutputMode::WASAPIExclusive: {
						DlgAdvancedWasapi dialog( GetInstanceHandle(), hwnd, GetSettings() );
						break;
					}
					case Settings::OutputMode::ASIO: {
						DlgAdvancedASIO dialog( GetInstanceHandle(), hwnd, GetSettings() );
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_GENERAL_WRITETAGS: {
				const bool enablePreserveFiletime = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_WRITETAGS ) ) );
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_PRESERVE_FILETIME ), enablePreserveFiletime ? TRUE : FALSE );
				break;
			}
		}
	}
}

void OptionsGeneral::OnNotify( const HWND /*hwnd*/, const WPARAM /*wParam*/, const LPARAM lParam )
{
	PNMLINK nmlink = reinterpret_cast<PNMLINK>( lParam );
	if ( nullptr != nmlink ) {
		if ( NM_CLICK == nmlink->hdr.code ) {
			if ( IDC_OPTIONS_GENERAL_SCROBBLER_AUTHORISE == nmlink->hdr.idFrom ) {
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					vuplayer->ScrobblerAuthorise();
				}
			}
		}
	}
}

void OptionsGeneral::RefreshOutputDeviceList( const HWND hwnd )
{
	std::wstring settingDevice;
	Settings::OutputMode settingMode;
	GetSettings().GetOutputSettings( settingDevice, settingMode );

	HWND hwndMode = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MODE );
	HWND hwndDevice = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE );
	if ( ( nullptr != hwndMode ) && ( nullptr != hwndDevice ) ) {
		const int modeIndex = ComboBox_GetCurSel( hwndMode );
		if ( modeIndex >= 0 ) {
			ComboBox_ResetContent( hwndDevice );
			const Settings::OutputMode currentMode = static_cast<Settings::OutputMode>( ComboBox_GetItemData( hwndMode, modeIndex ) );
			Output::Devices devices = GetOutput().GetDevices( currentMode );
			for ( const auto& device : devices ) {
				int itemIndex = -1;
				if ( -1 == device.first ) {
					const int bufferSize = MAX_PATH;
					WCHAR buffer[ bufferSize ];
					const HINSTANCE instance = GetInstanceHandle();
					LoadString( instance, IDS_OPTIONS_DEVICE_DEFAULT, buffer, bufferSize );
					itemIndex = ComboBox_AddString( hwndDevice, buffer );
				} else {
					itemIndex = ComboBox_AddString( hwndDevice, device.second.c_str() );
				}
				if ( settingDevice == device.second ) {
					ComboBox_SetCurSel( hwndDevice, itemIndex );
				}
			}
			if ( -1 == ComboBox_GetCurSel( hwndDevice ) ) {
				ComboBox_SetCurSel( hwndDevice, 0 );
			}

			HWND hwndAdvanced = GetDlgItem( hwnd, IDC_OPTIONS_MODE_ADVANCED );
			if ( nullptr != hwndAdvanced ) {
				if ( Settings::OutputMode::Standard != GetSelectedMode( hwnd ) ) {
					EnableWindow( hwndAdvanced, TRUE );
				} else {
					EnableWindow( hwndAdvanced, FALSE );
				}
			}
		} else {
			EnableWindow( hwndDevice, FALSE );
			HWND hwndAdvanced = GetDlgItem( hwnd, IDC_OPTIONS_MODE_ADVANCED );
			if ( nullptr != hwndAdvanced ) {
				EnableWindow( hwndAdvanced, FALSE );
			}
		}
	}
}

Settings::OutputMode OptionsGeneral::GetSelectedMode( const HWND hwnd ) const
{
	Settings::OutputMode mode = Settings::OutputMode::Standard;
	if ( const HWND hwndMode = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MODE ); nullptr != hwndMode ) {
		if ( const int selectedMode = ComboBox_GetCurSel( hwndMode ); selectedMode >= 0 ) {
			mode = static_cast<Settings::OutputMode>( ComboBox_GetItemData( hwndMode, selectedMode ) );
		}
	}
	return mode;
}

Settings::TitleBarFormat OptionsGeneral::GetSelectedTitleBarFormat( const HWND hwnd ) const
{
	Settings::TitleBarFormat format = Settings::TitleBarFormat::ArtistTitle;
	if ( const HWND hwndFormat = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_TITLEBAR ); nullptr != hwndFormat ) {
		if ( const int selectedFormat = ComboBox_GetCurSel( hwndFormat ); selectedFormat >= 0 ) {
			format = static_cast<Settings::TitleBarFormat>( ComboBox_GetItemData( hwndFormat, selectedFormat ) );
		}
	}
	return format;
}

std::wstring OptionsGeneral::GetSelectedDeviceName( const HWND hwnd ) const
{
	std::wstring device;
	HWND hwndDevice = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE );
	if ( nullptr != hwndDevice ) {
		const int selectedDevice = ComboBox_GetCurSel( hwndDevice );
		if ( selectedDevice > 0 ) {
			const int bufferSize = ComboBox_GetLBTextLen( hwndDevice, selectedDevice );
			if ( bufferSize > 0 ) {
				std::vector<WCHAR> buffer( bufferSize + 1, 0 );
				ComboBox_GetLBText( hwndDevice, selectedDevice, &buffer[ 0 ] );
				device = &buffer[ 0 ];
			}
		}
	}
	return device;
}
