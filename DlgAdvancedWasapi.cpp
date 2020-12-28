#include "DlgAdvancedWasapi.h"

#include "resource.h"
#include "Utility.h"

INT_PTR CALLBACK DlgAdvancedWasapi::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgAdvancedWasapi* dialog = reinterpret_cast<DlgAdvancedWasapi*>( lParam );
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
					DlgAdvancedWasapi* dialog = reinterpret_cast<DlgAdvancedWasapi*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->SaveSettings( hwnd );
					}
				}
				case IDCANCEL : {
					EndDialog( hwnd, 0 );
					return TRUE;
				}
				case IDC_WASAPI_RESET : {
					DlgAdvancedWasapi* dialog = reinterpret_cast<DlgAdvancedWasapi*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->ResetToDefaults( hwnd );
					}
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

DlgAdvancedWasapi::DlgAdvancedWasapi( const HINSTANCE instance, const HWND parent, Settings& settings ) :
	m_Settings( settings )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ADVANCED_WASAPI ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgAdvancedWasapi::~DlgAdvancedWasapi()
{
}

void DlgAdvancedWasapi::OnInitDialog( const HWND hwnd )
{
	CentreDialog( hwnd );
	bool useDeviceDefaultFormat = false;
	int bufferLength = 0;
	int leadIn = 0;
	m_Settings.GetAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn );
	SetDlgItemInt( hwnd, IDC_WASAPI_BUFFERLENGTH, static_cast<UINT>( bufferLength ), FALSE );
	SetDlgItemInt( hwnd, IDC_WASAPI_LEADIN, static_cast<UINT>( leadIn ), FALSE );
	CheckRadioButton( hwnd, IDC_WASAPI_OUTPUT_DEFAULT, IDC_WASAPI_OUTPUT_SOURCE,
		useDeviceDefaultFormat ? IDC_WASAPI_OUTPUT_DEFAULT : IDC_WASAPI_OUTPUT_SOURCE );
}

void DlgAdvancedWasapi::SaveSettings( const HWND hwnd )
{
	bool useDeviceDefaultFormat = false;
	int bufferLength = 0;
	int leadIn = 0;
	m_Settings.GetAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn );

	useDeviceDefaultFormat = ( BST_CHECKED == IsDlgButtonChecked( hwnd, IDC_WASAPI_OUTPUT_DEFAULT ) );
	BOOL success = FALSE;
	UINT value = GetDlgItemInt( hwnd, IDC_WASAPI_BUFFERLENGTH, &success, FALSE );
	if ( success ) {
		bufferLength = static_cast<int>( value );
	}
	value = GetDlgItemInt( hwnd, IDC_WASAPI_LEADIN, &success, FALSE );
	if ( success ) {
		leadIn = static_cast<int>( value );
	}

	m_Settings.SetAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn );
}

void DlgAdvancedWasapi::ResetToDefaults( const HWND hwnd )
{
	bool useDeviceDefaultFormat = false;
	int bufferLength = 0;
	int leadIn = 0;
	int maxBufferLength = 0;
	int maxLeadIn = 0;
	m_Settings.GetDefaultAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn, maxBufferLength, maxLeadIn );
	SetDlgItemInt( hwnd, IDC_WASAPI_BUFFERLENGTH, static_cast<UINT>( bufferLength ), TRUE /*signed*/ );
	SetDlgItemInt( hwnd, IDC_WASAPI_LEADIN, static_cast<UINT>( leadIn ), TRUE /*signed*/ );
	CheckRadioButton( hwnd, IDC_WASAPI_OUTPUT_DEFAULT, IDC_WASAPI_OUTPUT_SOURCE,
		useDeviceDefaultFormat ? IDC_WASAPI_OUTPUT_DEFAULT : IDC_WASAPI_OUTPUT_SOURCE );
}
