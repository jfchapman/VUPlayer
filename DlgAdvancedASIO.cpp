#include "DlgAdvancedASIO.h"

#include "resource.h"
#include "Utility.h"

INT_PTR CALLBACK DlgAdvancedASIO::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgAdvancedASIO* dialog = reinterpret_cast<DlgAdvancedASIO*>( lParam );
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
					DlgAdvancedASIO* dialog = reinterpret_cast<DlgAdvancedASIO*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->SaveSettings( hwnd );
					}
				}
				case IDCANCEL : {
					EndDialog( hwnd, 0 );
					return TRUE;
				}
				case IDC_ASIO_RESET : {
					DlgAdvancedASIO* dialog = reinterpret_cast<DlgAdvancedASIO*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
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

DlgAdvancedASIO::DlgAdvancedASIO( const HINSTANCE instance, const HWND parent, Settings& settings ) :
	m_Settings( settings )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ADVANCED_ASIO ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgAdvancedASIO::~DlgAdvancedASIO()
{
}

void DlgAdvancedASIO::OnInitDialog( const HWND hwnd )
{
	CentreDialog( hwnd );
	bool useDefaultSamplerate = false;
	int defaultSamplerate = 0;
	int leadIn = 0;
	m_Settings.GetAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadIn );
	SetDlgItemInt( hwnd, IDC_ASIO_DEFAULTRATE, static_cast<UINT>( defaultSamplerate ), FALSE );
	SetDlgItemInt( hwnd, IDC_ASIO_LEADIN, static_cast<UINT>( leadIn ), FALSE );
	CheckRadioButton( hwnd, IDC_ASIO_OUTPUT_DEFAULT, IDC_ASIO_OUTPUT_SOURCE,
		useDefaultSamplerate ? IDC_ASIO_OUTPUT_DEFAULT : IDC_ASIO_OUTPUT_SOURCE );
}

void DlgAdvancedASIO::SaveSettings( const HWND hwnd )
{
	bool useDefaultSamplerate = false;
	int defaultSamplerate = 0;
	int leadIn = 0;
	m_Settings.GetAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadIn );

	useDefaultSamplerate = ( BST_CHECKED == IsDlgButtonChecked( hwnd, IDC_ASIO_OUTPUT_DEFAULT ) );
	BOOL success = FALSE;
	UINT value = GetDlgItemInt( hwnd, IDC_ASIO_DEFAULTRATE, &success, FALSE );
	if ( success ) {
		defaultSamplerate = static_cast<int>( value );
	}
	value = GetDlgItemInt( hwnd, IDC_ASIO_LEADIN, &success, FALSE );
	if ( success ) {
		leadIn = static_cast<int>( value );
	}

	m_Settings.SetAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadIn );
}

void DlgAdvancedASIO::ResetToDefaults( const HWND hwnd )
{
	bool useDefaultSamplerate = false;
	int defaultSamplerate = 0;
	int leadIn = 0;
	int maxDefaultSamplerate = 0;
	int maxLeadIn = 0;
	m_Settings.GetDefaultAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadIn, maxDefaultSamplerate, maxLeadIn );
	SetDlgItemInt( hwnd, IDC_ASIO_DEFAULTRATE, static_cast<UINT>( defaultSamplerate ), TRUE /*signed*/ );
	SetDlgItemInt( hwnd, IDC_ASIO_LEADIN, static_cast<UINT>( leadIn ), TRUE /*signed*/ );
	CheckRadioButton( hwnd, IDC_ASIO_OUTPUT_DEFAULT, IDC_ASIO_OUTPUT_SOURCE,
		useDefaultSamplerate ? IDC_ASIO_OUTPUT_DEFAULT : IDC_ASIO_OUTPUT_SOURCE );
}
