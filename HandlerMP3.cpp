#include "HandlerMP3.h"

#include "EncoderMP3.h"

#include "resource.h"
#include "Utility.h"

HandlerMP3::HandlerMP3() :
	Handler()
{
}

HandlerMP3::~HandlerMP3()
{
}

std::wstring HandlerMP3::GetDescription() const
{
	std::wstring description = L"LAME MP3";
	const char* version = get_lame_version();
	if ( nullptr != version ) {
		description += L" " + UTF8ToWideString( version );
	}
	return description;
}

std::set<std::wstring> HandlerMP3::GetSupportedFileExtensions() const
{
	return std::set<std::wstring>();
}

bool HandlerMP3::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerMP3::SetTags( const std::wstring& /*filename*/, const Handler::Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerMP3::OpenDecoder( const std::wstring& /*filename*/ ) const
{
	return nullptr;
}

Encoder::Ptr HandlerMP3::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderMP3() );
	return encoder;
}

bool HandlerMP3::IsDecoder() const
{
	return false;
}

bool HandlerMP3::IsEncoder() const
{
	return true;
}

bool HandlerMP3::CanConfigureEncoder() const
{
	return true;
}

INT_PTR CALLBACK HandlerMP3::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( lParam );
			if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
				SetWindowLongPtr( hwnd, DWLP_USER, reinterpret_cast<LPARAM>( config ) );
				config->m_Handler->OnConfigureInit( hwnd, config->m_Settings );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL : 
				case IDOK : {
					ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
						config->m_Handler->OnConfigureClose( hwnd, config->m_Settings );
					}
					EndDialog( hwnd, IDOK == LOWORD( wParam ) );
					return TRUE;
				}
				case IDC_CONVERT_FILENAME_DEFAULT : {
					ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
						config->m_Handler->OnConfigureDefault( hwnd, config->m_Settings );
					}
					break;
				}
				default : {
					break;
				}
			}
			break;
		}
		case WM_NOTIFY : {
			LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
			if ( ( nullptr != nmhdr ) && ( nmhdr->code == TTN_GETDISPINFO ) ) {
				ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
				if ( nullptr != config ) {
					const HWND sliderWnd = reinterpret_cast<HWND>( nmhdr->idFrom );
					const std::wstring tooltip = config->m_Handler->GetTooltip( config->m_hInst, sliderWnd );
					LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
					wcscpy_s( info->szText, tooltip.c_str() );			
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

bool HandlerMP3::ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const
{
	ConfigurationInfo* config = new ConfigurationInfo( { settings, this, instance } );
	const bool configured = DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ENCODER_MP3 ), parent, DialogProc, reinterpret_cast<LPARAM>( config ) );
	delete config;
	return configured;
}

void HandlerMP3::OnConfigureInit( const HWND hwnd, const std::string& settings ) const
{
	CentreDialog( hwnd );
	const HWND sliderWnd = GetDlgItem( hwnd, IDC_ENCODER_MP3_VBR );
	if ( nullptr != sliderWnd ) {
		SendMessage( sliderWnd, TBM_SETRANGEMIN, TRUE /*redraw*/, 0 );
		SendMessage( sliderWnd, TBM_SETRANGEMAX, TRUE /*redraw*/, 9 );
		SendMessage( sliderWnd, TBM_SETTICFREQ, 1, 0 );

		SetQuality( sliderWnd, EncoderMP3::GetVBRQuality( settings ) );
	}
}

void HandlerMP3::OnConfigureDefault( const HWND hwnd, std::string& settings ) const
{
	settings.clear();
	const HWND sliderWnd = GetDlgItem( hwnd, IDC_ENCODER_MP3_VBR );
	if ( nullptr != sliderWnd ) {
		SetQuality( sliderWnd, EncoderMP3::GetVBRQuality( settings ) );
	}
}

void HandlerMP3::OnConfigureClose( const HWND hwnd, std::string& settings ) const
{
	const HWND sliderWnd = GetDlgItem( hwnd, IDC_ENCODER_MP3_VBR );
	if ( nullptr != sliderWnd ) {
		settings = std::to_string( GetQuality( sliderWnd ) );
	}
}

int HandlerMP3::GetQuality( const HWND slider ) const
{
	const int quality = static_cast<int>( SendMessage( slider, TBM_GETRANGEMAX, 0, 0 ) ) - static_cast<int>( SendMessage( slider, TBM_GETPOS, 0, 0 ) );
	return quality;
}

void HandlerMP3::SetQuality( const HWND slider, const int quality ) const
{
	const int position = static_cast<int>( SendMessage( slider, TBM_GETRANGEMAX, 0, 0 ) ) - quality;
	SendMessage( slider, TBM_SETPOS, TRUE /*redraw*/, position );
}

std::wstring HandlerMP3::GetTooltip( const HINSTANCE instance, const HWND slider ) const
{
	const int quality = GetQuality( slider );
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( instance, IDS_QUALITY_VBR, buffer, bufSize );
	const std::wstring tooltip = std::wstring( buffer ) + L": " + std::to_wstring( quality );
	return tooltip;
}
