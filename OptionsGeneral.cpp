#include "OptionsGeneral.h"

#include "resource.h"
#include "VUPlayer.h"
#include "windowsx.h"

OptionsGeneral::OptionsGeneral( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

OptionsGeneral::~OptionsGeneral()
{
}

void OptionsGeneral::OnInit( const HWND hwnd )
{
	// Output device settings
	HWND hwndDevices = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE );
	if ( nullptr != hwndDevices ) {
		const std::wstring currentOutputDevice = GetOutput().GetCurrentDevice();
		Output::Devices devices = GetOutput().GetDevices();
		for ( const auto& device : devices ) {
			const int itemIndex = ComboBox_AddString( hwndDevices, device.second.c_str() );
			if ( currentOutputDevice == device.second ) {
				ComboBox_SetCurSel( hwndDevices, itemIndex );
			}
		}

		const std::wstring deviceName = GetSettings().GetOutputDevice();
		if ( deviceName.empty() ) {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_USEDEFAULT ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), FALSE );
		} else {
			Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_USESPECIFIC ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), TRUE );
		}
	}

	// Miscellaneous settings
	VUPlayer* vuplayer = VUPlayer::Get();
	const bool gracenoteAvailable = ( nullptr != vuplayer ) && vuplayer->IsGracenoteAvailable();
	std::string userID;
	bool gracenoteEnabled = true;
	bool gracenoteLog = true;
	GetSettings().GetGracenoteSettings( userID, gracenoteEnabled, gracenoteLog );
	const HWND hwndGracenote = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_GRACENOTE_ENABLE );
	if ( nullptr != hwndGracenote ) {
		Button_SetCheck( hwndGracenote, ( gracenoteEnabled ? BST_CHECKED : BST_UNCHECKED ) );
		EnableWindow( hwndGracenote, gracenoteAvailable ? TRUE : FALSE );
	}

	const bool mergeDuplicates = GetSettings().GetMergeDuplicates();
	const HWND hwndMergeDuplicates = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES );
	if ( nullptr != hwndMergeDuplicates ) {
		Button_SetCheck( hwndMergeDuplicates, ( mergeDuplicates ? BST_CHECKED : BST_UNCHECKED ) );
	}

	// Notification area settings
	bool systrayEnable = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	GetSettings().GetSystraySettings( systrayEnable, systraySingleClick, systrayDoubleClick );
	HWND hwndEnable = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE );
	if ( nullptr != hwndEnable ) {
		Button_SetCheck( hwndEnable, ( systrayEnable ? BST_CHECKED : BST_UNCHECKED ) );
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
}

void OptionsGeneral::OnSave( const HWND hwnd )
{
	// Output device settings
	std::wstring deviceName;
	if ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_USESPECIFIC ) ) ) {
		HWND hwndDevices = GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE );
		WCHAR text[ MAX_PATH ] = {};
		if ( 0 != ComboBox_GetText( hwndDevices, text, MAX_PATH ) ) {
			deviceName = text;
		}
	}
	GetSettings().SetOutputDevice( deviceName );

	// Miscellaneous settings
	std::string userID;
	bool gracenoteEnabled = true;
	bool gracenoteLog = true;
	GetSettings().GetGracenoteSettings( userID, gracenoteEnabled, gracenoteLog );
	gracenoteEnabled = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_GRACENOTE_ENABLE ) ) );
	GetSettings().SetGracenoteSettings( userID, gracenoteEnabled, gracenoteLog );

	const bool mergeDuplicates = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_HIDEDUPLICATES ) ) );
	GetSettings().SetMergeDuplicates( mergeDuplicates );

	// Notification area settings
	const bool enable = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_ENABLE ) ) );
	const Settings::SystrayCommand singleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_SINGLECLICK ) ) );
	const Settings::SystrayCommand doubleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_SYSTRAY_DOUBLECLICK ) ) );
	GetSettings().SetSystraySettings( enable, singleClick, doubleClick );
}

void OptionsGeneral::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		const WORD controlID = LOWORD( wParam );
		switch ( controlID ) {
			case IDC_OPTIONS_GENERAL_USEDEFAULT : {
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), FALSE );
				break;
			}
			case IDC_OPTIONS_GENERAL_USESPECIFIC : {
				EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_GENERAL_DEVICE ), TRUE );
				break;
			}
			default : {
				break;
			}
		}
	}
}
