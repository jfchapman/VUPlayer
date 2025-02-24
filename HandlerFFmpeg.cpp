#include "HandlerFFmpeg.h"

#include "resource.h"

#include "DecoderFFmpeg.h"
#include "EncoderFFmpeg.h"
#include "Utility.h"
#include "ShellMetadata.h"

HandlerFFmpeg::HandlerFFmpeg()
{
}

std::wstring HandlerFFmpeg::GetDescription() const
{
	// Note that the description is used for populating the encoder list.
	constexpr wchar_t kDescription[] = L"MPEG-4 (AAC)";
	return kDescription;
}

std::set<std::wstring> HandlerFFmpeg::GetSupportedFileExtensions() const
{
	// Although FFmpeg is the fallback decoder, return some of the extensions not handled by other decoders (so that they pass the default filter in file selection dialogs). 
	return { L"aac", L"asf", L"avi", L"m4a", L"m4b", L"mkv", L"mov", L"mp4", L"mpeg", L"mpg", L"shn", L"tta", L"webm", L"mpc", L"mp+", L"mpp", L"ape", L"apl" };
}

bool HandlerFFmpeg::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerFFmpeg::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	return SetShellMetadata( filename, tags );
}

Decoder::Ptr HandlerFFmpeg::OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const
{
	DecoderFFmpeg* decoderFFmpeg = nullptr;
	try {
		decoderFFmpeg = new DecoderFFmpeg( filename, context );
	} catch ( const std::runtime_error& ) {
	}
	const Decoder::Ptr stream( decoderFFmpeg );
	return stream;
}

Encoder::Ptr HandlerFFmpeg::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderFFmpeg() );
	return encoder;
}

bool HandlerFFmpeg::IsDecoder() const
{
	return true;
}

bool HandlerFFmpeg::IsEncoder() const
{
	return true;
}

bool HandlerFFmpeg::CanConfigureEncoder() const
{
	return true;
}

INT_PTR CALLBACK HandlerFFmpeg::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
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

void HandlerFFmpeg::OnConfigureInit( const HWND hwnd, const std::string& settings ) const
{
	CentreDialog( hwnd );

	const int bufSize = 128;
	WCHAR buffer[ bufSize ];
	GetDlgItemText( hwnd, IDC_ENCODER_FFMPEG_BITRATE_LABEL, buffer, bufSize );
	std::wstring label( buffer );
	WideStringReplace( label, L"%1", std::to_wstring( EncoderFFmpeg::GetMinimumBitrate() ) );
	WideStringReplace( label, L"%2", std::to_wstring( EncoderFFmpeg::GetMaximumBitrate() ) );
	SetDlgItemText( hwnd, IDC_ENCODER_FFMPEG_BITRATE_LABEL, label.c_str() );

	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderFFmpeg::GetBitrate( settings ) );
	}
}

void HandlerFFmpeg::OnConfigureDefault( const HWND hwnd, std::string& settings ) const
{
	settings.clear();
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderFFmpeg::GetBitrate( settings ) );
	}
}

void HandlerFFmpeg::OnConfigureClose( const HWND hwnd, std::string& settings ) const
{
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_FFMPEG_BITRATE );
	if ( nullptr != bitrateWnd ) {
		settings = std::to_string( GetBitrate( bitrateWnd ) );
	}
}

int HandlerFFmpeg::GetBitrate( const HWND control ) const
{
	TCHAR text[ 8 ] = {};
	GetWindowText( control, text, 8 );
	const std::string setting = WideStringToUTF8( text );
	const int bitrate = EncoderFFmpeg::GetBitrate( setting );
	return bitrate;
}

void HandlerFFmpeg::SetBitrate( const HWND control, const int bitrate ) const
{
	int limitedBitrate = bitrate;
	EncoderFFmpeg::LimitBitrate( limitedBitrate );
	const std::wstring setting = std::to_wstring( limitedBitrate );
	SetWindowText( control, setting.c_str() );
}

bool HandlerFFmpeg::ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const
{
	ConfigurationInfo* config = new ConfigurationInfo( { settings, this, instance } );
	const bool configured = static_cast<bool>( DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ENCODER_FFMPEG ), parent, DialogProc, reinterpret_cast<LPARAM>( config ) ) );
	delete config;
	return configured;
}

void HandlerFFmpeg::SettingsChanged( Settings& /*settings*/ )
{
}
