#include "DlgHotkey.h"

#include "resource.h"
#include "Utility.h"

INT_PTR CALLBACK DlgHotkey::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgHotkey* dialog = reinterpret_cast<DlgHotkey*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
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

LRESULT CALLBACK DlgHotkey::ButtonProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	DlgHotkey* dialog = reinterpret_cast<DlgHotkey*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != dialog ) {
		switch ( message ) {
			case WM_KEYDOWN: {
				if ( dialog->OnKeyDown( wParam, lParam ) ) {
					PostMessage( GetParent( hwnd ), WM_COMMAND, IDOK, 0 );
				} else {
					switch ( wParam ) {
						case VK_ESCAPE :
						case VK_RETURN : {
							PostMessage( GetParent( hwnd ), WM_COMMAND, IDCANCEL, 0 );
							break;
						}
					}
				}
				break;
			}
			case WM_GETDLGCODE : {
				return DLGC_WANTALLKEYS;
			}
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( dialog->GetDefaultButtonProc() ) );
				break;
			}
			default : {
				break;
			}
		}
	}
	return CallWindowProc( dialog->GetDefaultButtonProc(), hwnd, message, wParam, lParam );
}

DlgHotkey::DlgHotkey( const HINSTANCE instance, const HWND parent ) :
	m_Hotkey( {} ),
	m_Valid( false ),
	m_UnmodifiedHotkeys(),
	m_ModifiedHotkeys(),
	m_hWnd( nullptr ),
	m_DefaultButtonProc( nullptr )
{
	InitAllowedHotkeys();
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_HOTKEY ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgHotkey::~DlgHotkey()
{
}

void DlgHotkey::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );
	const HWND buttonWnd = GetDlgItem( m_hWnd, IDCANCEL );
	SetWindowLongPtr( buttonWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
	m_DefaultButtonProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( buttonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( ButtonProc ) ) );
}

WNDPROC DlgHotkey::GetDefaultButtonProc()
{
	return m_DefaultButtonProc;
}

bool DlgHotkey::GetHotkey( Settings::Hotkey& hotkey ) const
{
	hotkey = m_Hotkey;
	return m_Valid;
}

bool DlgHotkey::OnKeyDown( WPARAM wParam, LPARAM lParam )
{
	const int keycode = static_cast<int>( wParam );
	bool alt = false;
	bool control = false;
	bool shift = false;
	GetKeyStates( alt, control, shift );
	const bool modified = ( alt || control || shift );
	m_Valid = ( m_UnmodifiedHotkeys.end() != m_UnmodifiedHotkeys.find( keycode ) );
	if ( !m_Valid && modified ) {
		m_Valid = ( m_ModifiedHotkeys.end() != m_ModifiedHotkeys.find( keycode ) );
	}
	if ( m_Valid ) {
		m_Hotkey.Code = keycode;
		m_Hotkey.Alt = alt;
		m_Hotkey.Ctrl = control;
		m_Hotkey.Shift = shift;

		const int bufSize = 64;
		WCHAR buffer[ bufSize ] = {};
		if ( 0 != GetKeyNameText( static_cast<LONG>( lParam ), buffer, bufSize ) ) {
			m_Hotkey.Name = buffer;
		}
	}
	return m_Valid;
}

void DlgHotkey::InitAllowedHotkeys()
{
	for ( int key = VK_F1; key <= VK_F11; key++ ) {
		m_UnmodifiedHotkeys.insert( key );
	}
	for ( int key = VK_F13; key <= VK_F24; key++ ) {
		m_UnmodifiedHotkeys.insert( key );
	}
	for ( int key = VK_VOLUME_MUTE; key <= VK_MEDIA_PLAY_PAUSE; key++ ) {
		m_UnmodifiedHotkeys.insert( key );
	}
	for ( int key = 0x30 /* 0 */; key <= 0x39 /* 9 */; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = 0x41 /* A */; key <= 0x5A /* Z */; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = VK_NUMPAD0; key <= VK_DIVIDE; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = VK_SPACE; key <= VK_HELP; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = VK_BROWSER_BACK; key <= VK_LAUNCH_APP2; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = VK_OEM_1; key <= VK_OEM_3; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	for ( int key = VK_OEM_4; key <= VK_OEM_8; key++ ) {
		m_ModifiedHotkeys.insert( key );
	}
	m_ModifiedHotkeys.insert( VK_PAUSE );
	m_ModifiedHotkeys.insert( VK_SLEEP );
}

void DlgHotkey::GetKeyStates( bool& alt, bool &control, bool& shift ) const
{
	SHORT value = GetAsyncKeyState( VK_MENU );
	alt = value < 0;
	value = GetAsyncKeyState( VK_CONTROL );
	control = value < 0;
	value = GetAsyncKeyState( VK_SHIFT );
	shift = value < 0;
}
