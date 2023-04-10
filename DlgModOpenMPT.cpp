#include "DlgModOpenMPT.h"

#include "resource.h"
#include "windowsx.h"
#include "Utility.h"

INT_PTR CALLBACK DlgModOpenMPT::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgModOpenMPT* dialog = reinterpret_cast<DlgModOpenMPT*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
				return TRUE;
			}
			break;
		}
		case WM_DESTROY : {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDOK : {
					DlgModOpenMPT* dialog = reinterpret_cast<DlgModOpenMPT*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->SaveSettings( hwnd );
					}
				}
				case IDCANCEL : {
					EndDialog( hwnd, 0 );
					return TRUE;
				}
			  case IDC_OPTIONS_OPENMPT_RESET : {
					DlgModOpenMPT* dialog = reinterpret_cast<DlgModOpenMPT*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->ResetToDefaults( hwnd );
					}
				  break;
			  }
				default : {
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

DlgModOpenMPT::DlgModOpenMPT( const HINSTANCE instance, const HWND parent, Settings& settings ) :
  m_hInst( instance ),
	m_Settings( settings )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_MOD_OPENMPT ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgModOpenMPT::~DlgModOpenMPT()
{
}

void DlgModOpenMPT::OnInitDialog( const HWND hwnd )
{
	CentreDialog( hwnd );

  bool fadeout = false;
  long separation = 100;
  long ramping = -1;
  long interpolation = 0;
  m_Settings.GetOpenMPTSettings( fadeout, separation, ramping, interpolation );

  // Fade out looped songs checkbox
  Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_FADEOUT ), ( fadeout ? BST_CHECKED : BST_UNCHECKED ) );

  // Stereo separation slider
	SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_PAN ), TBM_SETRANGEMIN, 0, 0 );
	SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_PAN ), TBM_SETRANGEMAX, 0, 200 );
	SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_PAN ), TBM_SETPOS, TRUE, separation );

  const std::map<long, int> kRampingDescriptions = {
    { -1, IDS_OPENMPT_RAMPING_DEFAULT },
    { 0,  IDS_OPENMPT_RAMPING_DISABLED },
  };

  const std::map<long, int> kInterpolationDescriptions = {
    { 0, IDS_OPENMPT_INTERPOLATION_DEFAULT },
    { 1, IDS_OPENMPT_INTERPOLATION_NONE },
    { 2, IDS_OPENMPT_INTERPOLATION_LINEAR },
    { 4, IDS_OPENMPT_INTERPOLATION_CUBIC },
    { 8, IDS_OPENMPT_INTERPOLATION_SINC },
  };

	const int bufferSize = MAX_PATH;
	WCHAR buffer[ bufferSize ];

	// Ramping combo
	if ( const HWND wndRamping = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_RAMPING ); nullptr != wndRamping ) {
    const auto supportedRamping = Settings::GetOpenMPTSupportedRamping();
    for ( const auto value : supportedRamping ) {
      std::wstring name = std::to_wstring( value );
      if ( const auto it = kRampingDescriptions.find( value ); kRampingDescriptions.end() != it ) {
	      LoadString( m_hInst, it->second, buffer, bufferSize );
        name += std::wstring( L" (" ) + buffer + std::wstring( L")" );
      }
		  ComboBox_AddString( wndRamping, name.c_str() );
		  ComboBox_SetItemData( wndRamping, ComboBox_GetCount( wndRamping ) - 1, static_cast<LPARAM>( value ) );
		  if ( value == ramping ) {
			  ComboBox_SetCurSel( wndRamping, ComboBox_GetCount( wndRamping ) - 1 );
		  }
    }
  }

	// Interpolation combo
	if ( const HWND wndInterpolation = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_INTERPOLATION ); nullptr != wndInterpolation ) {
    const auto supportedInterpolation = Settings::GetOpenMPTSupportedInterpolation();
    for ( const auto value : supportedInterpolation ) {
      std::wstring name = std::to_wstring( value );
      if ( const auto it = kInterpolationDescriptions.find( value ); kInterpolationDescriptions.end() != it ) {
	      LoadString( m_hInst, it->second, buffer, bufferSize );
        name += std::wstring( L" (" ) + buffer + std::wstring( L")" );
      }
		  ComboBox_AddString( wndInterpolation, name.c_str() );
		  ComboBox_SetItemData( wndInterpolation, ComboBox_GetCount( wndInterpolation ) - 1, static_cast<LPARAM>( value ) );
		  if ( value == interpolation ) {
			  ComboBox_SetCurSel( wndInterpolation, ComboBox_GetCount( wndInterpolation ) - 1 );
		  }
    }
  }
}

void DlgModOpenMPT::SaveSettings( const HWND hwnd )
{
  bool fadeout = false;
  long separation = 100;
  long ramping = -1;
  long interpolation = 0;
  m_Settings.GetOpenMPTSettings( fadeout, separation, ramping, interpolation );

  fadeout = ( BST_CHECKED == Button_GetCheck( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_FADEOUT ) ) );
	separation = static_cast<long>( SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_PAN ), TBM_GETPOS, 0, 0 ) );

	if ( const HWND wndRamping = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_RAMPING ); nullptr != wndRamping ) {
		const int selectedRamping = ComboBox_GetCurSel( wndRamping );
		if ( selectedRamping >= 0 ) {
			ramping = static_cast<long>( ComboBox_GetItemData( wndRamping, selectedRamping ) );
		}
	}

  if ( const HWND wndInterpolation = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_INTERPOLATION ); nullptr != wndInterpolation ) {
		const int selectedInterpolation = ComboBox_GetCurSel( wndInterpolation );
		if ( selectedInterpolation >= 0 ) {
			interpolation = static_cast<long>( ComboBox_GetItemData( wndInterpolation, selectedInterpolation ) );
		}
	}

  m_Settings.SetOpenMPTSettings( fadeout, separation, ramping, interpolation );
}

void DlgModOpenMPT::ResetToDefaults( const HWND hwnd )
{
  bool fadeout = false;
  long separation = 100;
  long ramping = -1;
  long interpolation = 0;
  m_Settings.GetDefaultOpenMPTSettings( fadeout, separation, ramping, interpolation );

  Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_FADEOUT ), ( fadeout ? BST_CHECKED : BST_UNCHECKED ) );

  SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_PAN ), TBM_SETPOS, TRUE, separation );

	if ( const HWND wndRamping = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_RAMPING ); nullptr != wndRamping ) {
    const int itemCount = ComboBox_GetCount( wndRamping );
    for ( int index = 0; index < itemCount; index++ ) {
			const long value = static_cast<long>( ComboBox_GetItemData( wndRamping, index ) );
      if ( value == ramping ) {
        ComboBox_SetCurSel( wndRamping, index );
        break;
      }
    }
  }

	if ( const HWND wndInterpolation = GetDlgItem( hwnd, IDC_OPTIONS_OPENMPT_INTERPOLATION ); nullptr != wndInterpolation ) {
    const int itemCount = ComboBox_GetCount( wndInterpolation );
    for ( int index = 0; index < itemCount; index++ ) {
			const long value = static_cast<long>( ComboBox_GetItemData( wndInterpolation, index ) );
      if ( value == interpolation ) {
        ComboBox_SetCurSel( wndInterpolation, index );
        break;
      }
    }
  }
}
