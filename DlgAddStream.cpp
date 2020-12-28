#include "DlgAddStream.h"

#include "resource.h"
#include "Utility.h"

INT_PTR CALLBACK DlgAddStream::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgAddStream* dialog = reinterpret_cast<DlgAddStream*>( lParam );
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
				case IDCANCEL : 
				case IDOK : {
					DlgAddStream* dialog = reinterpret_cast<DlgAddStream*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnClose( ( IDOK == LOWORD( wParam ) ) );
					}
					EndDialog( hwnd, 0 );
					return TRUE;
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

DlgAddStream::DlgAddStream( const HINSTANCE instance, const HWND parent ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_URL()
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ADD_STREAM ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

void DlgAddStream::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );
}

void DlgAddStream::OnClose( const bool ok )
{
	if ( ok ) {
		const int bufferSize = 2048;
		WCHAR buffer[ bufferSize ] = {};
		if ( 0 != GetDlgItemText( m_hWnd, IDC_STREAM_URL, buffer, bufferSize ) ) {
			m_URL = buffer;
		}
	}
}

const std::wstring& DlgAddStream::GetURL() const
{
	return m_URL;
}
