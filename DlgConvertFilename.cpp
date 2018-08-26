#include "DlgConvertFilename.h"

#include "resource.h"
#include "Utility.h"

INT_PTR CALLBACK DlgConvertFilename::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgConvertFilename* dialog = reinterpret_cast<DlgConvertFilename*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL : 
				case IDOK : {
					DlgConvertFilename* dialog = reinterpret_cast<DlgConvertFilename*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnClose( ( IDOK == LOWORD( wParam ) ) );
					}
					EndDialog( hwnd, LOWORD( lParam ) );
					return TRUE;
				}
				case IDC_CONVERT_FILENAME_DEFAULT : {
					DlgConvertFilename* dialog = reinterpret_cast<DlgConvertFilename*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnDefault();
					}
					break;
				}
				default : {
					break;
				}
			}
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

DlgConvertFilename::DlgConvertFilename( const HINSTANCE instance, const HWND parent, Settings& settings ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT_FILENAME ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgConvertFilename::~DlgConvertFilename()
{
}

void DlgConvertFilename::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );
	std::wstring folder;
	std::wstring filename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( folder, filename, extractToLibrary );
	SetDlgItemText( m_hWnd, IDC_CONVERT_FILENAME_FORMAT, filename.c_str() );
}

void DlgConvertFilename::OnDefault()
{
	std::wstring folder;
	std::wstring filename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( folder, filename, extractToLibrary );
	m_Settings.SetExtractSettings( folder, std::wstring(), extractToLibrary );
	m_Settings.GetExtractSettings( folder, filename, extractToLibrary );
	SetDlgItemText( m_hWnd, IDC_CONVERT_FILENAME_FORMAT, filename.c_str() );
}

void DlgConvertFilename::OnClose( const bool ok )
{
	if ( ok ) {
		WCHAR buffer[ MAX_PATH ] = {};
		GetDlgItemText( m_hWnd, IDC_CONVERT_FILENAME_FORMAT, buffer, MAX_PATH );
		std::wstring folder;
		std::wstring filename;
		bool extractToLibrary = false;
		m_Settings.GetExtractSettings( folder, filename, extractToLibrary );
		m_Settings.SetExtractSettings( folder, buffer, extractToLibrary );
	}
}
