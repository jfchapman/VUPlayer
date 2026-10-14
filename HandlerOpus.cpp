#include "HandlerOpus.h"

#include "DecoderOpus.h"
#include "EncoderOpus.h"

#include "OggComment.h"

#include "Opusfile.h"

#include "resource.h"

#include "DarkMode.h"

#include "MediaInfo.h"
#include "Utility.h"

// R128 reference level in LUFS.
static constexpr float LOUDNESS_R128 = -23.0f;

// Supported tags and their Opus field names.
static const std::map<Tag, std::string> s_SupportedTags = {
	{ Tag::Album,      "ALBUM" },
	{ Tag::Artist,     "ARTIST" },
	{ Tag::Comment,    "COMMENT" },
	{ Tag::GainAlbum,  "R128_ALBUM_GAIN" },
	{ Tag::GainTrack,  "R128_TRACK_GAIN" },
	{ Tag::Genre,      "GENRE" },
	{ Tag::Title,      "TITLE" },
	{ Tag::Track,      "TRACKNUMBER" },
	{ Tag::Year,       "DATE" },
	{ Tag::Composer,   "COMPOSER" },
	{ Tag::Conductor,  "CONDUCTOR" },
	{ Tag::Publisher,  "PUBLISHER" }
};

HandlerOpus::HandlerOpus() :
	Handler()
{
}

HandlerOpus::~HandlerOpus()
{
}

std::wstring HandlerOpus::GetDescription() const
{
	const char* version = opus_get_version_string();
	const std::wstring description = ( nullptr != version ) ? UTF8ToWideString( version ) : L"Opus";
	return description;
}

std::set<std::wstring> HandlerOpus::GetSupportedFileExtensions() const
{
	return { L"opus" };
}

bool HandlerOpus::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = true;
	try {
		const OggComment oggComment( filename, OggComment::Type::Opus );
		if ( !oggComment.GetVendor().empty() ) {
			tags.insert( Tags::value_type( Tag::Version, oggComment.GetVendor() ) );
		}
		const auto& comments = oggComment.GetUserComments();
		for ( const auto& comment : comments ) {
			const std::string& field = comment.first;
			const std::string& value = comment.second;
			if ( !field.empty() && !value.empty() ) {
				if ( 0 == _stricmp( field.c_str(), "ARTIST" ) ) {
					tags.insert( Tags::value_type( Tag::Artist, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "TITLE" ) ) {
					tags.insert( Tags::value_type( Tag::Title, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "ALBUM" ) ) {
					tags.insert( Tags::value_type( Tag::Album, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "GENRE" ) ) {
					tags.insert( Tags::value_type( Tag::Genre, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "YEAR" ) ) {
					tags.insert( Tags::value_type( Tag::Year, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "DATE" ) ) {
					// Prefer 'date' over 'year' (both map to the same tag type).
					tags[ Tag::Year ] = value;
				} else if ( 0 == _stricmp( field.c_str(), "COMMENT" ) ) {
					tags.insert( Tags::value_type( Tag::Comment, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "TRACK" ) ) {
					tags.insert( Tags::value_type( Tag::Track, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "TRACKNUMBER" ) ) {
					// Prefer 'tracknumber' over 'track' (both map to the same tag type).
					tags[ Tag::Track ] = value;
				} else if ( 0 == _stricmp( field.c_str(), "COMPOSER" ) ) {
					tags.insert( Tags::value_type( Tag::Composer, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "CONDUCTOR" ) ) {
					tags.insert( Tags::value_type( Tag::Conductor, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "LABEL" ) ) {
					tags.insert( Tags::value_type( Tag::Publisher, value ) );
				} else if ( 0 == _stricmp( field.c_str(), "PUBLISHER" ) ) {
					// Prefer 'publisher' over 'label' (both map to the same tag type).
					tags[ Tag::Publisher ] = value;
				} else if ( 0 == _stricmp( field.c_str(), "R128_ALBUM_GAIN" ) ) {
					const std::string gain = R128ToGain( value );
					if ( !gain.empty() ) {
						tags.insert( Tags::value_type( Tag::GainAlbum, gain ) );
					}
				} else if ( 0 == _stricmp( field.c_str(), "R128_TRACK_GAIN" ) ) {
					const std::string gain = R128ToGain( value );
					if ( !gain.empty() ) {
						tags.insert( Tags::value_type( Tag::GainTrack, gain ) );
					}
				}
			}
		}
		if ( const std::string artwork = oggComment.GetArtwork(); !artwork.empty() ) {
			tags.insert( Tags::value_type( Tag::Artwork, artwork ) );
		}
	} catch ( const std::runtime_error& ) {
		success = false;
	}
	return success;
}

bool HandlerOpus::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	bool success = false;
	try {
		OggComment oggComment( filename, OggComment::Type::Opus, false /*readonly*/ );
		for ( const auto& tag : tags ) {
			const auto match = s_SupportedTags.find( tag.first );
			if ( s_SupportedTags.end() != match ) {
				const std::string& field = match->second;
				const std::string value = ( ( Tag::GainAlbum == tag.first ) || ( Tag::GainTrack == tag.first ) ) ? GainToR128( tag.second ) : tag.second;
				oggComment.RemoveUserComment( field );
				if ( !value.empty() ) {
					oggComment.AddUserComment( field, value );
				}
			} else if ( Tag::Artwork == tag.first ) {
				oggComment.SetArtwork( tag.second );
			}
		}
		success = oggComment.WriteComments();
	} catch ( const std::runtime_error& ) {
	}
	return success;
}

Decoder::Ptr HandlerOpus::OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const
{
	DecoderOpus* streamOpus = nullptr;
	try {
		streamOpus = new DecoderOpus( filename, context );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( streamOpus );
	return stream;
}

Encoder::Ptr HandlerOpus::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderOpus() );
	return encoder;
}

bool HandlerOpus::IsDecoder() const
{
	return true;
}

bool HandlerOpus::IsEncoder() const
{
	return true;
}

bool HandlerOpus::CanConfigureEncoder() const
{
	return true;
}

void HandlerOpus::SettingsChanged( Settings& /*settings*/ )
{
}

INT_PTR CALLBACK HandlerOpus::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			ConfigurationInfo* config = reinterpret_cast<ConfigurationInfo*>( lParam );
			if ( ( nullptr != config ) && ( nullptr != config->m_Handler ) ) {
				SetWindowLongPtr( hwnd, DWLP_USER, reinterpret_cast<LPARAM>( config ) );
				config->m_Handler->OnConfigureInit( hwnd, config->m_Settings );
				DarkMode::ApplyToDialog( hwnd );
				LayoutDialogTextControls( hwnd );
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
				case IDC_ENCODER_OPUS_DEFAULT: {
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

bool HandlerOpus::ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const
{
	ConfigurationInfo* config = new ConfigurationInfo( { settings, this, instance } );
	const bool configured = static_cast<bool>( DialogBoxParam( instance, MAKEINTRESOURCE( IDD_ENCODER_OPUS ), parent, DialogProc, reinterpret_cast<LPARAM>( config ) ) );
	delete config;
	return configured;
}

void HandlerOpus::OnConfigureInit( const HWND hwnd, const std::string& settings ) const
{
	CentreDialog( hwnd );

	const int bufSize = 128;
	WCHAR buffer[ bufSize ];
	GetDlgItemText( hwnd, IDC_ENCODER_OPUS_BITRATE_LABEL, buffer, bufSize );
	std::wstring label( buffer );
	WideStringReplace( label, L"%1", std::to_wstring( EncoderOpus::GetMinimumBitrate() ) );
	WideStringReplace( label, L"%2", std::to_wstring( EncoderOpus::GetMaximumBitrate() ) );
	SetDlgItemText( hwnd, IDC_ENCODER_OPUS_BITRATE_LABEL, label.c_str() );

	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_OPUS_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderOpus::GetBitrate( settings ) );
	}
}

void HandlerOpus::OnConfigureDefault( const HWND hwnd, std::string& settings ) const
{
	settings.clear();
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_OPUS_BITRATE );
	if ( nullptr != bitrateWnd ) {
		SetBitrate( bitrateWnd, EncoderOpus::GetBitrate( settings ) );
	}
}

void HandlerOpus::OnConfigureClose( const HWND hwnd, std::string& settings ) const
{
	const HWND bitrateWnd = GetDlgItem( hwnd, IDC_ENCODER_OPUS_BITRATE );
	if ( nullptr != bitrateWnd ) {
		settings = std::to_string( GetBitrate( bitrateWnd ) );
	}
}

int HandlerOpus::GetBitrate( const HWND control ) const
{
	TCHAR text[ 8 ] = {};
	GetWindowText( control, text, 8 );
	const std::string setting = WideStringToUTF8( text );
	const int bitrate = EncoderOpus::GetBitrate( setting );
	return bitrate;
}

void HandlerOpus::SetBitrate( const HWND control, const int bitrate ) const
{
	int limitedBitrate = bitrate;
	EncoderOpus::LimitBitrate( limitedBitrate );
	const std::wstring setting = std::to_wstring( limitedBitrate );
	SetWindowText( control, setting.c_str() );
}

std::string HandlerOpus::GainToR128( const std::string& gain )
{
	// Convert from floating point to Q7.8 fixed point.
	std::string str;
	try {
		const long r128 = std::lround( 256 * ( std::stof( gain ) - LOUDNESS_REFERENCE + LOUDNESS_R128 ) );
		if ( ( r128 >= -32768 ) && ( r128 <= 32767 ) ) {
			str = std::to_string( r128 );
		}
	} catch ( const std::logic_error& ) {
	}
	return str;
}

std::string HandlerOpus::R128ToGain( const std::string& gain )
{
	// Convert from Q7.8 fixed point to floating point.
	std::wstring str;
	try {
		const long r128 = std::stol( gain );
		if ( ( r128 >= -32768 ) && ( r128 <= 32767 ) ) {
			str = GainToString( GetModuleHandle( nullptr ), LOUDNESS_REFERENCE - LOUDNESS_R128 + static_cast<float>( r128 ) / 256 );
		}
	} catch ( const std::logic_error& ) {
	}
	return WideStringToUTF8( str );
}
