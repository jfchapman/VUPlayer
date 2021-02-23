#include "OptionsMod.h"

#include "resource.h"
#include "windowsx.h"

OptionsMod::OptionsMod( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output ),
	m_hWnd( nullptr ),
	m_MODSettings( 0 ),
	m_MTMSettings( 0 ),
	m_S3MSettings( 0 ),
	m_XMSettings( 0 ),
	m_ITSettings( 0 ),
	m_CurrentSettings( &m_MODSettings )
{
	GetSettings().GetMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
}

void OptionsMod::OnInit( const HWND hwnd )
{
	m_hWnd = hwnd;

	const int bufferSize = MAX_PATH;
	WCHAR buffer[ bufferSize ];

	const HINSTANCE instance = GetInstanceHandle();

	// Surround sound combo
	HWND wndSurround = GetDlgItem( hwnd, IDC_OPTIONS_MOD_SURROUND );
	LoadString( instance, IDS_SURROUND_OFF, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );
	LoadString( instance, IDS_SURROUND_MODE1, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );
	LoadString( instance, IDS_SURROUND_MODE2, buffer, bufferSize );
	ComboBox_AddString( wndSurround, buffer );

	// Ramping combo
	HWND wndRamping = GetDlgItem( hwnd, IDC_OPTIONS_MOD_RAMPING );
	LoadString( instance, IDS_RAMPING_OFF, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );
	LoadString( instance, IDS_RAMPING_NORMAL, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );
	LoadString( instance, IDS_RAMPING_SENSITIVE, buffer, bufferSize );
	ComboBox_AddString( wndRamping, buffer );

	// Interpolation combo
	HWND wndInterpolation = GetDlgItem( hwnd, IDC_OPTIONS_MOD_INTERPOLATION );
	LoadString( instance, IDS_INTERPOLATION_OFF, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );
	LoadString( instance, IDS_INTERPOLATION_LINEAR, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );
	LoadString( instance, IDS_INTERPOLATION_SINC, buffer, bufferSize );
	ComboBox_AddString( wndInterpolation, buffer );

	// Looping combo
	HWND wndLooping = GetDlgItem( hwnd, IDC_OPTIONS_MOD_LOOPING );
	LoadString( instance, IDS_LOOPING_PREVENT, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );
	LoadString( instance, IDS_LOOPING_ALLOW, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );
	LoadString( instance, IDS_LOOPING_FADE, buffer, bufferSize );
	ComboBox_AddString( wndLooping, buffer );

	Button_SetCheck( GetDlgItem( hwnd, IDC_OPTIONS_MOD_TYPEMOD ), BST_CHECKED );
	UpdateControls( hwnd );
}

void OptionsMod::OnSave( const HWND hwnd )
{
	if ( nullptr != m_CurrentSettings ) {
		long long panning = SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_GETPOS, 0, 0 );
		panning <<= 32;
		*m_CurrentSettings &= 0xffffffff;
		*m_CurrentSettings |= panning;
	}
	GetSettings().SetMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
}

void OptionsMod::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	const WORD controlID = LOWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		if ( ( nullptr != m_CurrentSettings ) && ( IDC_OPTIONS_SOUNDFONT_BROWSE != controlID ) ) {
			long long panning = SendMessage( GetDlgItem( hwnd, IDC_OPTIONS_MOD_PAN ), TBM_GETPOS, 0, 0 );
			panning <<= 32;
			*m_CurrentSettings &= 0xffffffff;
			*m_CurrentSettings |= panning;
		}
		switch ( controlID ) {
			case IDC_OPTIONS_MOD_TYPEMOD : {
				m_CurrentSettings = &m_MODSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPEMTM : {
				m_CurrentSettings = &m_MTMSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPES3M : {
				m_CurrentSettings = &m_S3MSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPEXM : {
				m_CurrentSettings = &m_XMSettings;
				break;
			}
			case IDC_OPTIONS_MOD_TYPEIT : {
				m_CurrentSettings = &m_ITSettings;
				break;
			}
			case IDC_OPTIONS_MOD_RESET : {
				GetSettings().GetDefaultMODSettings( m_MODSettings, m_MTMSettings, m_S3MSettings, m_XMSettings, m_ITSettings );
				break;
			}
			case IDC_OPTIONS_SOUNDFONT_BROWSE : {
				ChooseSoundFont();
				break;
			}
			default : {
				break;
			}
		}
		UpdateControls( hwnd );
	} else if ( CBN_SELCHANGE == notificationCode ) {
		switch ( controlID ) {
			case IDC_OPTIONS_MOD_SURROUND : {
				*m_CurrentSettings &= ~( BASS_MUSIC_SURROUND | BASS_MUSIC_SURROUND2 );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_SURROUND ) );
				switch ( selected ) {
					case 1 : {
						*m_CurrentSettings |= BASS_MUSIC_SURROUND;
						break;
					}
					case 2 : {
						*m_CurrentSettings |= BASS_MUSIC_SURROUND2;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_RAMPING : {
				*m_CurrentSettings &= ~( BASS_MUSIC_RAMP | BASS_MUSIC_RAMPS );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_RAMPING ) );
				switch ( selected ) {
					case 1 : {
						*m_CurrentSettings |= BASS_MUSIC_RAMP;
						break;
					}
					case 2 : {
						*m_CurrentSettings |= BASS_MUSIC_RAMPS;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_INTERPOLATION : {
				*m_CurrentSettings &= ~( BASS_MUSIC_NONINTER | BASS_MUSIC_SINCINTER );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_INTERPOLATION ) );
				switch ( selected ) {
					case 0 : {
						*m_CurrentSettings |= BASS_MUSIC_NONINTER;
						break;
					}
					case 2 : {
						*m_CurrentSettings |= BASS_MUSIC_SINCINTER;
						break;
					}
				}
				break;
			}
			case IDC_OPTIONS_MOD_LOOPING : {
				*m_CurrentSettings &= ~( BASS_MUSIC_STOPBACK | VUPLAYER_MUSIC_FADEOUT );
				const int selected = ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_OPTIONS_MOD_LOOPING ) );
				switch ( selected ) {
					case 0 : {
						*m_CurrentSettings |= BASS_MUSIC_STOPBACK;
						break;
					}
					case 2 : {
						*m_CurrentSettings |= VUPLAYER_MUSIC_FADEOUT;
						break;
					}
				}
				break;
			}
		}
	}
}

void OptionsMod::UpdateControls( const HWND hwnd )
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

	const std::wstring soundFont = GetSettings().GetSoundFont();
	SetWindowText( GetDlgItem( hwnd, IDC_OPTIONS_SOUNDFONT ), soundFont.c_str() );
}

void OptionsMod::ChooseSoundFont()
{
	WCHAR title[ MAX_PATH ] = {};
	const HINSTANCE hInst = GetInstanceHandle();
	LoadString( hInst, IDS_CHOOSESOUNDFONT_TITLE, title, MAX_PATH );
	WCHAR filter[ MAX_PATH ] = {};
	LoadString( hInst, IDS_CHOOSESOUNDFONT_FILTER, filter, MAX_PATH );
	const std::wstring filter1( filter );
	const std::wstring filter2( L"*.sf2;*.sfz" );
	LoadString( hInst, IDS_CHOOSE_FILTERALL, filter, MAX_PATH );
	const std::wstring filter3( filter );
	const std::wstring filter4( L"*.*" );
	std::vector<WCHAR> filterStr;
	filterStr.reserve( MAX_PATH );
	filterStr.insert( filterStr.end(), filter1.begin(), filter1.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter2.begin(), filter2.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter3.begin(), filter3.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter4.begin(), filter4.end() );
	filterStr.push_back( 0 );
	filterStr.push_back( 0 );

	const std::string initialFolderSetting = "SoundFont";
	const std::wstring initialFolder = GetSettings().GetLastFolder( initialFolderSetting );

	WCHAR buffer[ MAX_PATH ] = {};
	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrTitle = title;
	ofn.lpstrFilter = &filterStr[ 0 ];
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
	if ( FALSE != GetOpenFileName( &ofn ) ) {
		const std::wstring filename = ofn.lpstrFile;
		GetSettings().SetSoundFont( filename );
		GetSettings().SetLastFolder( initialFolderSetting, filename.substr( 0, ofn.nFileOffset ) );
		UpdateControls( m_hWnd );
	}
}
