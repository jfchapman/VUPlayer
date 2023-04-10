#include "OptionsMod.h"

#include "resource.h"
#include "windowsx.h"

#include "DlgModBASS.h"
#include "DlgModOpenMPT.h"

OptionsMod::OptionsMod( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output ),
	m_hWnd( nullptr )
{
}

void OptionsMod::OnInit( const HWND hwnd )
{
	m_hWnd = hwnd;

  const std::vector<std::pair<Settings::MODDecoder, std::wstring>>modDecoders = {
	  std::make_pair( Settings::MODDecoder::BASS, GetOutput().GetHandlers().GetBassVersion() ),
    std::make_pair( Settings::MODDecoder::OpenMPT, GetOutput().GetHandlers().GetOpenMPTVersion() )
  };

  const auto preferredDecoder = GetSettings().GetMODDecoder();
	
	if ( const HWND hwndDecoder = GetDlgItem( hwnd, IDC_OPTIONS_MOD_DECODER ); nullptr != hwndDecoder ) {
		for ( const auto& [ decoder, name ] : modDecoders ) {
			ComboBox_AddString( hwndDecoder, name.c_str() );
			ComboBox_SetItemData( hwndDecoder, ComboBox_GetCount( hwndDecoder ) - 1, static_cast<LPARAM>( decoder ) );
			if ( decoder == preferredDecoder ) {
				ComboBox_SetCurSel( hwndDecoder, ComboBox_GetCount( hwndDecoder ) - 1 );
			}
  	}
	}

  const auto samplerateSetting = GetSettings().GetMODSamplerate();

	if ( const HWND hwndSamplerate = GetDlgItem( hwnd, IDC_OPTIONS_MOD_SAMPLERATE ); nullptr != hwndSamplerate ) {
    constexpr auto samplerates = Settings::GetMODSupportedSamplerates();
		const int bufferSize = 16;
		WCHAR hz[ bufferSize ];
		const HINSTANCE instance = GetInstanceHandle();
    LoadString( instance, IDS_UNITS_HZ, hz, bufferSize );
		for ( const auto samplerate : samplerates ) {
      const std::wstring str = std::to_wstring( samplerate ) + L" " + hz;
			ComboBox_AddString( hwndSamplerate, str.c_str() );
			ComboBox_SetItemData( hwndSamplerate, ComboBox_GetCount( hwndSamplerate ) - 1, static_cast<LPARAM>( samplerate ) );
			if ( samplerate == samplerateSetting ) {
				ComboBox_SetCurSel( hwndSamplerate, ComboBox_GetCount( hwndSamplerate ) - 1 );
			}
  	}
	}

  m_SoundFont = GetSettings().GetSoundFont();
  SetWindowText( GetDlgItem( hwnd, IDC_OPTIONS_SOUNDFONT ), m_SoundFont.c_str() );
}

void OptionsMod::OnSave( const HWND hwnd )
{
  GetSettings().SetMODDecoder( GetPreferredDecoder( hwnd ) );

	if ( const HWND hwndSamplerate = GetDlgItem( hwnd, IDC_OPTIONS_MOD_SAMPLERATE ); nullptr != hwndSamplerate ) {
		const int selectedSamplerate = ComboBox_GetCurSel( hwndSamplerate );
		if ( selectedSamplerate >= 0 ) {
			GetSettings().SetMODSamplerate( static_cast<uint32_t>( ComboBox_GetItemData( hwndSamplerate, selectedSamplerate ) ) );
		}
	}

	GetSettings().SetSoundFont( m_SoundFont );
}

void OptionsMod::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	const WORD controlID = LOWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		switch ( controlID ) {
      case IDC_OPTIONS_MOD_SETTINGS : {
        const auto preferredDecoder = GetPreferredDecoder( hwnd );
        switch ( preferredDecoder ) {
          case Settings::MODDecoder::BASS : {
				    DlgModBASS dialog( GetInstanceHandle(), hwnd, GetSettings() );
            break;
          }
          case Settings::MODDecoder::OpenMPT : {
				    DlgModOpenMPT dialog( GetInstanceHandle(), hwnd, GetSettings() );
            break;
          }
        }
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
	}
}

Settings::MODDecoder OptionsMod::GetPreferredDecoder( const HWND hwnd ) const
{
  Settings::MODDecoder decoder = Settings::MODDecoder::BASS;
	if ( const HWND hwndDecoder = GetDlgItem( hwnd, IDC_OPTIONS_MOD_DECODER ); nullptr != hwndDecoder ) {
		const int selectedDecoder = ComboBox_GetCurSel( hwndDecoder );
		if ( selectedDecoder >= 0 ) {
			decoder = static_cast<Settings::MODDecoder>( ComboBox_GetItemData( hwndDecoder, selectedDecoder ) );
		}
	}
  return decoder;
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
		m_SoundFont = ofn.lpstrFile;
		GetSettings().SetLastFolder( initialFolderSetting, m_SoundFont.substr( 0, ofn.nFileOffset ) );
		SetWindowText( GetDlgItem( m_hWnd, IDC_OPTIONS_SOUNDFONT ), m_SoundFont.c_str() );
	}
}
