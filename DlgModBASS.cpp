#include "DlgModBASS.h"

#include "bass.h"

#include "resource.h"
#include "windowsx.h"
#include "Utility.h"

INT_PTR CALLBACK DlgModBASS::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			DlgModBASS* dialog = reinterpret_cast<DlgModBASS*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
				return TRUE;
			}
			break;
		}
		case WM_DESTROY: {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_COMMAND: {
			switch ( LOWORD( wParam ) ) {
				case IDOK: {
					DlgModBASS* dialog = reinterpret_cast<DlgModBASS*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->SaveSettings( hwnd );
					}
				}
				case IDCANCEL: {
					EndDialog( hwnd, 0 );
					return TRUE;
				}
				default: {
					DlgModBASS* dialog = reinterpret_cast<DlgModBASS*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnCommand( hwnd, wParam, lParam );
					}
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

DlgModBASS::DlgModBASS( const HINSTANCE instance, const HWND parent, Settings& settings ) :
	m_hInst( instance ),
	m_Settings( settings ),
	m_MODSettings( 0 ),
	m_MTMSettings( 0 ),
	m_S3MSettings( 0 ),
	m_XMSettings( 0 ),
	m_ITSettings( 0 ),
	m_CurrentSettings( &m_MODSettings )
{
	settings.GetMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_MOD_BASS ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgModBASS::~DlgModBASS()
{
}

void DlgModBASS::OnInitDialog( const HWND hwnd )
{
	CentreDialog( hwnd );

	const int bufferSize = MAX_PATH;
	WCHAR buffer[ bufferSize ];

	// Stereo separation slider
	SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_SETRANGEMIN, 0, 0 );
	SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_SETRANGEMAX, 0, 100 );

	// Surround sound combo
	HWND wndSurround = GetDlgItem( hwnd, IDC_OPTIONS_MOD_SURROUND );
	LoadString( m_hInst, IDS_SURROUND_OFF, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );
	LoadString( m_hInst, IDS_SURROUND_MODE1, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );
	LoadString( m_hInst, IDS_SURROUND_MODE2, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );

	// Ramping combo
	HWND wndRamping = GetDlgItem( hwnd, IDC_OPTIONS_MOD_RAMPING );
	LoadString( m_hInst, IDS_RAMPING_OFF, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );
	LoadString( m_hInst, IDS_RAMPING_NORMAL, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );
	LoadString( m_hInst, IDS_RAMPING_SENSITIVE, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );

	// Interpolation combo
	HWND wndInterpolation = GetDlgItem( hwnd, IDC_OPTIONS_MOD_INTERPOLATION );
	LoadString( m_hInst, IDS_INTERPOLATION_OFF, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );
	LoadString( m_hInst, IDS_INTERPOLATION_LINEAR, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );
	LoadString( m_hInst, IDS_INTERPOLATION_SINC, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );

	// Looping combo
	HWND wndLooping = GetDlgItem( hwnd, IDC_OPTIONS_MOD_LOOPING );
	LoadString( m_hInst, IDS_LOOPING_PREVENT, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );
	LoadString( m_hInst, IDS_LOOPING_ALLOW, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );
	LoadString( m_hInst, IDS_LOOPING_FADE, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );

	Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_MOD_TYPEMOD ), BST_CHECKED );
	UpdateControls( hwnd );
}

void DlgModBASS::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	const WORD controlID = LOWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		if ( nullptr != m_CurrentSettings ) {
			long long panning = SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_GETPOS, 0, 0 );
			panning <<= 32;
			*m_CurrentSettings &= 0xffffffff;
			*m_CurrentSettings |= panning;
		}
		switch ( controlID ) {
			case IDC_OPTIONS_MOD_TYPEMOD: {
				m_CurrentSettings = &m_MODSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPEMTM: {
				m_CurrentSettings = &m_MTMSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPES3M: {
				m_CurrentSettings = &m_S3MSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPEXM: {
				m_CurrentSettings = &m_XMSettings;
				break;
			}
			case IDC_OPTIONS_MOD_RESET: {
				m_Settings.GetDefaultMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
				break;
			}
			default: {
				break;
			}
		}
		UpdateControls( hwnd );
	} else if ( CBN_SELCHANGE == notificationCode ) {
		switch ( controlID ) {
			case IDC_OPTIONS_MOD_SURROUND: {
				*m_CurrentSettings &= ~( BASS_MUSIC_SURROUND | BASS_MUSIC_SURROUND2 );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_SURROUND ) );
				switch ( selected ) {
					case 1: {
						*m_CurrentSettings |= BASS_MUSIC_SURROUND;
						break;
					}
					case 2: {
						*m_CurrentSettings |= BASS_MUSIC_SURROUND2;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_RAMPING: {
				*m_CurrentSettings &= ~( BASS_MUSIC_RAMP | BASS_MUSIC_RAMPS );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_RAMPING ) );
				switch ( selected ) {
					case 1: {
						*m_CurrentSettings |= BASS_MUSIC_RAMP;
						break;
					}
					case 2: {
						*m_CurrentSettings |= BASS_MUSIC_RAMPS;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_INTERPOLATION: {
				*m_CurrentSettings &= ~( BASS_MUSIC_NONINTER | BASS_MUSIC_SINCINTER );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_INTERPOLATION ) );
				switch ( selected ) {
					case 0: {
						*m_CurrentSettings |= BASS_MUSIC_NONINTER;
						break;
					}
					case 2: {
						*m_CurrentSettings |= BASS_MUSIC_SINCINTER;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_LOOPING: {
				*m_CurrentSettings &= ~( BASS_MUSIC_STOPBACK | VUPLAYER_MUSIC_FADEOUT );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_LOOPING ) );
				switch ( selected ) {
					case 0: {
						*m_CurrentSettings |= BASS_MUSIC_STOPBACK;
						break;
					}
					case 2: {
						*m_CurrentSettings |= VUPLAYER_MUSIC_FADEOUT;
						break;
					}
				}
				break;
			}
		}
	}
}

void DlgModBASS::UpdateControls( const HWND hwnd )
{
	if ( nullptr != m_CurrentSettings ) {
		int surroundIndex = 0;
		if ( *m_CurrentSettings & BASS_MUSIC_SURROUND ) {
			surroundIndex = 1;
		} else if ( *m_CurrentSettings & BASS_MUSIC_SURROUND2 ) {
			surroundIndex = 2;
		}
		ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_SURROUND ), surroundIndex );

		int rampingIndex = 0;
		if ( *m_CurrentSettings & BASS_MUSIC_RAMP ) {
			rampingIndex = 1;
		} else if ( *m_CurrentSettings & BASS_MUSIC_RAMPS ) {
			rampingIndex = 2;
		}
		ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_RAMPING ), rampingIndex );

		int interpolationIndex = 1;
		if ( *m_CurrentSettings & BASS_MUSIC_NONINTER ) {
			interpolationIndex = 0;
		} else if ( *m_CurrentSettings & BASS_MUSIC_SINCINTER ) {
			interpolationIndex = 2;
		}
		ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_INTERPOLATION ), interpolationIndex );

		int loopingIndex = 1;
		if ( *m_CurrentSettings & BASS_MUSIC_STOPBACK ) {
			loopingIndex = 0;
		} else if ( *m_CurrentSettings & VUPLAYER_MUSIC_FADEOUT ) {
			loopingIndex = 2;
		}
		ComboBox_SetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_LOOPING ), loopingIndex );

		int panning = static_cast<int>( *m_CurrentSettings >> 32 );
		if ( panning < 0 ) {
			panning = 0;
		} else if ( panning > 100 ) {
			panning = 100;
		}
		SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_SETPOS, TRUE /*redraw*/, panning );
	}
}

void DlgModBASS::SaveSettings( const HWND hwnd )
{
	if ( nullptr != m_CurrentSettings ) {
		long long panning = SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_GETPOS, 0, 0 );
		panning <<= 32;
		*m_CurrentSettings &= 0xffffffff;
		*m_CurrentSettings |= panning;
	}
	m_Settings.SetMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
}
