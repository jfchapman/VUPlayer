#include "OptionsGeneral.h"

#include "resource.h"
#include "VUPlayer.h"
#include "windowsx.h"

#include "DlgAdvancedASIO.h"
#include "DlgAdvancedWasapi.h"

std::vector<std::pair<Settings::OutputMode,int>> OptionsGeneral::s_OutputModes = {
	std::make_pair( Settings::OutputMode::Standard, IDS_OPTIONS_MODE_STANDARD ),
	std::make_pair( Settings::OutputMode::WASAPIExclusive, IDS_OPTIONS_MODE_WASAPI_EXCLUSIVE ),
	std::make_pair( Settings::OutputMode::ASIO, IDS_OPTIONS_MODE_ASIO )
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

	HWND hwndMode = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MODE );
	if ( nullptr != hwndMode ) {
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
	VUPlayer* vuplayer = VUPlayer::Get();

	const bool mergeDuplicates = GetSettings().GetMergeDuplicates();
	const HWND hwndMergeDuplicates = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES );
	if ( nullptr != hwndMergeDuplicates ) {
		Button_SetCheck( hwndMergeDuplicates, ( mergeDuplicates ? BST_CHECKED : BST_UNCHECKED ) );
	}

	if ( const HWND hwndMusicBrainz = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MUSICBRAINZ_ENABLE ); nullptr != hwndMusicBrainz ) {
		const bool musicbrainzAvailable = ( nullptr != vuplayer ) && vuplayer->IsMusicBrainzAvailable();
		const bool musicbrainzEnabled = GetSettings().GetMusicBrainzEnabled();
		Button_SetCheck( hwndMusicBrainz, ( musicbrainzEnabled ? BST_CHECKED : BST_UNCHECKED ) );
		EnableWindow( hwndMusicBrainz, musicbrainzAvailable ? TRUE : FALSE );
	}

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

	// Notification area settings
	bool systrayEnable = false;
	bool systrayMinimise = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	GetSettings().GetSystraySettings( systrayEnable, systrayMinimise, systraySingleClick, systrayDoubleClick );
	if ( HWND hwndEnable = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE ); nullptr != hwndEnable ) {
		Button_SetCheck( hwndEnable, ( systrayEnable ? BST_CHECKED : BST_UNCHECKED ) );
	}
	if ( HWND hwndMinimise = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_MINIMISE ); nullptr != hwndMinimise ) {
		Button_SetCheck( hwndMinimise, ( systrayMinimise ? BST_CHECKED : BST_UNCHECKED ) );
	}

	HWND hwndSingleClick = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_SINGLECLICK );
	HWND hwndDoubleClick = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_DOUBLECLICK );
	if ( ( nullptr != hwndSingleClick ) && ( nullptr != hwndDoubleClick ) ) {
		const int bufferSize = MAX_PATH;
		WCHAR buffer[ bufferSize ];
		const HINSTANCE instance = GetInstanceHandle();
		LoadString( instance, IDS_OPTIONS_SYSTRAY_NOACTION, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );
		LoadString( instance, IDS_OPTIONS_SYSTRAY_PLAYPAUSE, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );
		LoadString( instance, IDS_OPTIONS_SYSTRAY_STOP, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );
		LoadString( instance, IDS_OPTIONS_SYSTRAY_PREVIOUS, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );
		LoadString( instance, IDS_OPTIONS_SYSTRAY_NEXT, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );
		LoadString( instance, IDS_OPTIONS_SYSTRAY_SHOWHIDE, buffer, bufferSize );
		ComboBox_AddString( hwndSingleClick, buffer );
		ComboBox_AddString( hwndDoubleClick, buffer );

		ComboBox_SetCurSel( hwndSingleClick, static_cast<int>( systraySingleClick ) );
		ComboBox_SetCurSel( hwndDoubleClick, static_cast<int>( systrayDoubleClick ) );
	}
	EnableSystrayControls( hwnd );
}

void OptionsGeneral::OnSave( const HWND hwnd )
{
	// Output device settings
	const Settings::OutputMode mode = GetSelectedMode( hwnd );
	const std::wstring device = GetSelectedDeviceName( hwnd );
	GetSettings().SetOutputSettings( device, mode );

	// Miscellaneous settings
	const bool mergeDuplicates = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES ) ) );
	GetSettings().SetMergeDuplicates( mergeDuplicates );

	const bool musicbrainzEnabled = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MUSICBRAINZ_ENABLE ) ) );
	GetSettings().SetMusicBrainzEnabled( musicbrainzEnabled );

	const bool enableScrobbler = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SCROBBLER_ENABLE ) ) );
	GetSettings().SetScrobblerEnabled( enableScrobbler );

	// Notification area settings
	const bool enable = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE ) ) );
	const bool minimise = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_MINIMISE ) ) );
	const Settings::SystrayCommand singleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_SINGLECLICK ) ) );
	const Settings::SystrayCommand doubleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_DOUBLECLICK ) ) );
	GetSettings().SetSystraySettings( enable, minimise, singleClick, doubleClick );
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
		if ( IDC_OPTIONS_MODE_ADVANCED == controlID ) {
			const Settings::OutputMode mode = GetSelectedMode( hwnd );
			switch ( mode ) {
				case Settings::OutputMode::WASAPIExclusive : {
					DlgAdvancedWasapi dialog( GetInstanceHandle(), hwnd, GetSettings() );
					break;
				}
				case Settings::OutputMode::ASIO : {
					DlgAdvancedASIO dialog( GetInstanceHandle(), hwnd, GetSettings() );
					break;
				}
			}
		} else if ( IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE == controlID ) {
			EnableSystrayControls( hwnd );
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
	HWND hwndMode = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_MODE );
	if ( nullptr != hwndMode ) {
		const int selectedMode = ComboBox_GetCurSel( hwndMode );
		if ( selectedMode >= 0 ) {
			mode = static_cast<Settings::OutputMode>( ComboBox_GetItemData( hwndMode, selectedMode ) );
		}
	}
	return mode;
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

void OptionsGeneral::EnableSystrayControls( const HWND hwnd )
{
	const BOOL enable = IsDlgButtonChecked( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_MINIMISE ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_SINGLELABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_SINGLECLICK ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_DOUBLELABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_DOUBLECLICK ), enable );
}
