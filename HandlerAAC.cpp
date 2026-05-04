#include "HandlerAAC.h"

#include "resource.h"

#include "EncoderAAC.h"
#include "Utility.h"
#include "ShellMetadata.h"

HandlerAAC::HandlerAAC()
{
}

std::wstring HandlerAAC::GetDescription() const
{
	constexpr wchar_t kDescription[] = L"Advanced Audio Coding (AAC)";
	return kDescription;
}

std::set<std::wstring> HandlerAAC::GetSupportedFileExtensions() const
{
	return { L"m4a" };
}

bool HandlerAAC::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerAAC::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	return SetShellMetadata( filename, tags );
}

Decoder::Ptr HandlerAAC::OpenDecoder( const std::wstring& /*filename*/, const Decoder::Context /*context*/ ) const
{
	return nullptr;
}

Encoder::Ptr HandlerAAC::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderAAC() );
	return encoder;
}

bool HandlerAAC::IsDecoder() const
{
	return false;
}

bool HandlerAAC::IsEncoder() const
{
	return true;
}

bool HandlerAAC::CanConfigureEncoder() const
{
	return true;
}

INT_PTR CALLBACK HandlerAAC::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( lParam );
			if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
				SetWindowLongPtr( hwnd, DWLP_USER, reinterpret_cast<LPARAM>( config ) );
				config->m_Handler->OnConfigureInit( hwnd, config->m_Settings );
			}
			break;
		}
		case WM_DESTROY: {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_COMMAND: {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL:
				case IDOK: {
					ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
						config->m_Handler->OnConfigureClose( hwnd, config->m_Settings );
					}
					EndDialog( hwnd, IDOK == LOWORD( wParam ) );
					return TRUE;
				}
				case IDC_ENCODER_FFMPEG_DEFAULT: {
					ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
						config->m_Handler->OnConfigureDefault( hwnd, config->m_Settings );
					}
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		default: {
			break;
		}
	}
	return FALSE;
}

void HandlerAAC::OnConfigureInit( const HWND hwnd, const std::string& settings ) const
{
	CentreDialog( hwnd );

	const int bufSize = 128;
	WCHAR buffer[ bufSize ];
	GetDlgItemText( hwnd, IDC_ENCODER_FFMPEG_BITRATE_LABEL, buffer, bufSize );
	std::wstring label( buffer );
	WideStringReplace( label, L"%1", std::to_wstring( EncoderAAC::GetMinimumBitrate() ) );
	WideStringReplace( label, L"%2", std::to_wstring( EncoderAAC::GetMaximumBitrate() ) );
	SetDlgItemText( hwnd, IDC_ENCODER_FFMPEG_BITRATE_LABEL, label.c_str() );

	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderAAC::GetBitrate( settings ) );
	}
}

void HandlerAAC::OnConfigureDefault( const HWND hwnd, std::string& settings ) const
{
	settings.clear();
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderAAC::GetBitrate( settings ) );
	}
}

void HandlerAAC::OnConfigureClose( const HWND hwnd, std::string& settings ) const
{
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		settings = std::to_string( GetBitrate( bitrateWnd ) );
	}
}

int HandlerAAC::GetBitrate( const HWND control ) const
{
	TCHAR text[ 8 ] = {};
	GetWindowText( control, text, 8 );
	const std::string setting = WideStringToUTF8( text );
	const int bitrate = EncoderAAC::GetBitrate( setting );
	return bitrate;
}

void HandlerAAC::SetBitrate( const HWND control, const int bitrate ) const
{
	int limitedBitrate = bitrate;
	EncoderAAC::LimitBitrate( limitedBitrate );
	const std::wstring setting = std::to_wstring( limitedBitrate );
	SetWindowText( control, setting.c_str() );
}

bool HandlerAAC::ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const
{
	ConfigurationInfo* config = new ConfigurationInfo( { settings, this, instance } );
	const bool configured = static_cast<bool>( DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ENCODER_FFMPEG ), parent, DialogProc, reinterpret_cast<LPARAM>( config ) ) );
	delete config;
	return configured;
}

void HandlerAAC::SettingsChanged( Settings& /*settings*/ )
{
}
