#include "OptionsTaskbar.h"

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

#include "windowsx.h"

#include <array>

OptionsTaskbar::OptionsTaskbar( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

void OptionsTaskbar::OnInit( const HWND hwnd )
{
	// Notification area settings
	bool systrayEnable = false;
	bool systrayMinimise = false;
	std::array clickActions{
		Settings::SystrayCommand::None,
		Settings::SystrayCommand::None,
		Settings::SystrayCommand::None,
		Settings::SystrayCommand::None
	};
	GetSettings().GetSystraySettings( systrayEnable, systrayMinimise, clickActions[ 0 ], clickActions[ 1 ], clickActions[ 2 ], clickActions[ 3 ] );
	if ( HWND hwndEnable = GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_ENABLE ); nullptr != hwndEnable ) {
		Button_SetCheck( hwndEnable, ( systrayEnable ? BST_CHECKED : BST_UNCHECKED ) );
	}
	if ( HWND hwndMinimise = GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_MINIMISE ); nullptr != hwndMinimise ) {
		Button_SetCheck( hwndMinimise, ( systrayMinimise ? BST_CHECKED : BST_UNCHECKED ) );
	}

	std::array clickIDs{
		std::make_pair( IDC_OPTIONS_SYSTRAY_SINGLECLICK, clickActions[ 0 ] ),
		std::make_pair( IDC_OPTIONS_SYSTRAY_DOUBLECLICK, clickActions[ 1 ] ),
		std::make_pair( IDC_OPTIONS_SYSTRAY_TRIPLECLICK, clickActions[ 2 ] ),
		std::make_pair( IDC_OPTIONS_SYSTRAY_QUADCLICK, clickActions[ 3 ] )
	};
	const int bufferSize = MAX_PATH;
	WCHAR buffer[ bufferSize ];
	const HINSTANCE instance = GetInstanceHandle();
	for ( const auto [clickID, clickAction] : clickIDs ) {
		if ( const auto hwndClick = GetDlgItem( hwnd, clickID ); nullptr != hwndClick ) {
			LoadString( instance, IDS_OPTIONS_SYSTRAY_NOACTION, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			LoadString( instance, IDS_OPTIONS_SYSTRAY_PLAYPAUSE, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			LoadString( instance, IDS_OPTIONS_SYSTRAY_STOP, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			LoadString( instance, IDS_OPTIONS_SYSTRAY_PREVIOUS, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			LoadString( instance, IDS_OPTIONS_SYSTRAY_NEXT, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			LoadString( instance, IDS_OPTIONS_SYSTRAY_SHOWHIDE, buffer, bufferSize );
			ComboBox_AddString( hwndClick, buffer );
			ComboBox_SetCurSel( hwndClick, static_cast<int>( clickAction ) );
		}
	}
	EnableSystrayControls( hwnd );

	// Taskbar settings
	if ( IsWindows10() ) {
		m_ToolbarButtonColour = GetSettings().GetTaskbarButtonColour();
		if ( HWND hwndProgress = GetDlgItem( hwnd, IDC_OPTIONS_TASKBAR_PROGRESS ); nullptr != hwndProgress ) {
			Button_SetCheck( hwndProgress, ( GetSettings().GetTaskbarShowProgress() ? BST_CHECKED : BST_UNCHECKED ) );
		}
	} else {
		ShowWindow( GetDlgItem( hwnd, IDC_OPTIONS_TASKBAR_THUMBNAILGROUP ), SW_HIDE );
		ShowWindow( GetDlgItem( hwnd, IDC_OPTIONS_TASKBAR_BUTTONCOLOUR ), SW_HIDE );
		ShowWindow( GetDlgItem( hwnd, IDC_OPTIONS_TASKBAR_PROGRESS ), SW_HIDE );
	}
}

void OptionsTaskbar::OnSave( const HWND hwnd )
{
	// Notification area settings
	const bool enable = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_ENABLE ) ) );
	const bool minimise = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_MINIMISE ) ) );
	const Settings::SystrayCommand singleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_SINGLECLICK ) ) );
	const Settings::SystrayCommand doubleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_DOUBLECLICK ) ) );
	const Settings::SystrayCommand tripleClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_TRIPLECLICK ) ) );
	const Settings::SystrayCommand quadClick = static_cast<Settings::SystrayCommand>( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_QUADCLICK ) ) );
	GetSettings().SetSystraySettings( enable, minimise, singleClick, doubleClick, tripleClick, quadClick );

	// Thumbnail preview settings
	GetSettings().SetTaskbarButtonColour( m_ToolbarButtonColour );

	// Progress bar
	GetSettings().SetTaskbarShowProgress( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_TASKBAR_PROGRESS ) ) );
}

void OptionsTaskbar::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		const WORD controlID = LOWORD( wParam );
		switch ( controlID ) {
			case IDC_OPTIONS_SYSTRAY_ENABLE: {
				EnableSystrayControls( hwnd );
				break;
			}
			case IDC_OPTIONS_TASKBAR_BUTTONCOLOUR: {
				VUPlayer* vuplayer = VUPlayer::Get();
				COLORREF* customColours = ( nullptr != vuplayer ) ? vuplayer->GetCustomColours() : nullptr;
				if ( const auto colour = ChooseColour( hwnd, m_ToolbarButtonColour, customColours ); colour.has_value() ) {
					m_ToolbarButtonColour = *colour;
				}
				break;
			}
		}
	}
}

void OptionsTaskbar::EnableSystrayControls( const HWND hwnd )
{
	const BOOL enable = IsDlgButtonChecked( hwnd, IDC_OPTIONS_SYSTRAY_ENABLE );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_MINIMISE ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_SINGLELABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_SINGLECLICK ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_DOUBLELABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_DOUBLECLICK ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_TRIPLELABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_TRIPLECLICK ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_QUADLABEL ), enable );
	EnableWindow( GetDlgItem( hwnd, IDC_OPTIONS_SYSTRAY_QUADCLICK ), enable );
}
