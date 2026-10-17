// Scrobbler 2.0 implementation for VUPlayer

#include "scrobbler_impl.h"
#include "audioscrobbler.h"

#include <bcrypt.h>

// The API key & secret are kept out of the repository.
// Copy scrobbler_keys.template.h to scrobbler_keys.h, and fill in the values.
#if __has_include("scrobbler_keys.h")
#include "scrobbler_keys.h"
#else
#error "scrobbler_keys.h not found. Copy scrobbler_keys.template.h to scrobbler_keys.h, and fill in the API key & secret."
#endif

#include <iomanip>
#include <sstream>
#include <vector>

// Application name for internet calls.
static constexpr char s_Agent[] =					"VUPlayer";

// Accepted types for internet requests.
static const char* s_AcceptTypes[] =	{ "application/json", nullptr };

// Scrobbler server address.
static constexpr char s_server[] =				"ws.audioscrobbler.com";

// Scrobbler root URL.
static constexpr char s_url[] =						"/2.0/";

// Scrobbler API key.
static constexpr char s_apikey[] =				SCROBBLER_API_KEY;

// Scrobbler API secret.
static constexpr char s_apisecret[] =			SCROBBLER_API_SECRET;

// Scrobbler JSON attributes.
static constexpr char s_attr_error[] =		"error";
static constexpr char s_attr_token[] =		"token";
static constexpr char s_attr_session[] =	"session";
static constexpr char s_attr_key[] =			"key";

// HTTP content type for POST requests.
static constexpr char s_content_type[] =	"Content-Type: application/x-www-form-urlencoded\r\n";

// Authorization root URL.
static constexpr char s_authorization[] =	"https://www.last.fm/api/auth/";

// Connect, send & receive timeout for internet requests, in milliseconds.
static constexpr DWORD s_Timeout =				15000;

// Maximum response size, in bytes.
static constexpr size_t s_MaxResponseSize = 1024 * 1024;

// Request flags (all requests use HTTPS).
static constexpr DWORD s_RequestFlags =		INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI;

Scrobbler::Scrobbler() :
	m_hSession( OpenSession() ),
	m_hConnection( ( nullptr == m_hSession ) ? nullptr : InternetConnectA( m_hSession, s_server, INTERNET_DEFAULT_HTTPS_PORT,
		nullptr /*username*/, nullptr /*password*/, INTERNET_SERVICE_HTTP, 0 /*flags*/, 0 /*context*/ ) )
{
}

Scrobbler::~Scrobbler()
{
	if ( nullptr != m_hConnection ) {
		InternetCloseHandle( m_hConnection );
	}
	if ( nullptr != m_hSession ) {
		InternetCloseHandle( m_hSession );
	}
}

HINTERNET Scrobbler::OpenSession()
{
	// Timeouts are set on the session handle before the connection handle is created, so that they are inherited by the connection and its requests.
	HINTERNET hSession = InternetOpenA( s_Agent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr /*proxy*/, nullptr /*proxyBypass*/, 0 /*flags*/ );
	if ( nullptr != hSession ) {
		DWORD timeout = s_Timeout;
		InternetSetOptionA( hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof( timeout ) );
		InternetSetOptionA( hSession, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof( timeout ) );
		InternetSetOptionA( hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof( timeout ) );
	}
	return hSession;
}

bool Scrobbler::SendRequest( const Verb verb, const std::string& target, const std::string& content, std::string& response )
{
	bool success = false;
	response.clear();
	if ( nullptr != m_hConnection ) {
		HINTERNET hRequest = HttpOpenRequestA( m_hConnection, ( Verb::Post == verb ) ? "POST" : "GET", target.c_str(), nullptr /*version*/, nullptr /*referrer*/, s_AcceptTypes,
			s_RequestFlags, 0 /*context*/ );
		if ( nullptr != hRequest ) {
			const bool sent = ( Verb::Post == verb ) ?
				HttpSendRequestA( hRequest, s_content_type, static_cast<DWORD>( -1 ) /*headersLength*/, const_cast<char*>( content.c_str() ), static_cast<DWORD>( content.size() ) ) :
				HttpSendRequestA( hRequest, nullptr /*headers*/, 0 /*headersLength*/, nullptr /*optional*/, 0 /*optionalLength*/ );
			if ( sent ) {
				DWORD statusCode = 0;
				DWORD bufferLength = sizeof( statusCode );
				const bool statusOK = HttpQueryInfoA( hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &bufferLength, nullptr /*index*/ ) &&
					( HTTP_STATUS_OK == statusCode );

				// Read the response (which can contain error information, even when the status code is not OK).
				bool readOK = true;
				std::vector<char> buffer( 8192 );
				DWORD bytesRead = 0;
				while ( ( readOK = ( FALSE != InternetReadFile( hRequest, buffer.data(), static_cast<DWORD>( buffer.size() ), &bytesRead ) ) ) && ( bytesRead > 0 ) ) {
					if ( ( response.size() + bytesRead ) > s_MaxResponseSize ) {
						readOK = false;
						break;
					}
					response.append( buffer.data(), bytesRead );
				}
				if ( !readOK ) {
					response.clear();
				}
				success = statusOK && readOK;
			}
			InternetCloseHandle( hRequest );
			hRequest = nullptr;
		}
	}
	return success;
}

void Scrobbler::Authorize( const char* token )
{
	if ( ( nullptr != token ) && ( strlen( token ) > 0 ) ) {
		const std::string url = std::string( s_authorization ) + std::string( "?api_key=" ) + std::string( s_apikey ) + std::string( "&token=" ) + UrlEncode( token );
		ShellExecuteA( NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL );
	}
}

int32_t Scrobbler::GetToken( char* token, const int32_t tokenSize )
{
	int32_t result = scrobbler_error;
	if ( ( nullptr != token ) && ( tokenSize > 0 ) ) {
		StringMap parameters = { { "method", "auth.getToken" }, { "api_key", s_apikey } };
		const std::string sig = ConstructMethodSignature( parameters );
		parameters.insert( StringMap::value_type( "format", "json" ) );
		const std::string method = ConstructMethod( parameters ) + sig;
		const std::string target = std::string( s_url ) + std::string( "?" ) + method;
		std::string response;
		if ( SendRequest( Verb::Get, target, std::string(), response ) ) {
			const nlohmann::json document = nlohmann::json::parse( response, nullptr /*callback*/, false /*allowExceptions*/ );
			if ( const auto value = GetString( document, s_attr_token ); !value.empty() && ( value.size() < static_cast<size_t>( tokenSize ) ) ) {
				strcpy_s( token, tokenSize, value.c_str() );
				result = scrobbler_success;
			}
		} else if ( !response.empty() ) {
			result = GetErrorCode( response );
		}
	}
	return result;
}

int32_t Scrobbler::GetSessionKey( const char* token, char* sk, const int32_t skSize )
{
	int32_t result = scrobbler_error;
	if ( ( nullptr != token ) && ( nullptr != sk ) && ( skSize > 0 ) ) {
		StringMap parameters = { { "method", "auth.getSession" }, { "api_key", s_apikey }, { "token", token } };
		const std::string sig = ConstructMethodSignature( parameters );
		parameters.insert( StringMap::value_type( "format", "json" ) );
		const std::string method = ConstructMethod( parameters ) + sig;
		const std::string target = std::string( s_url ) + std::string( "?" ) + method;
		std::string response;
		if ( SendRequest( Verb::Get, target, std::string(), response ) ) {
			const nlohmann::json document = nlohmann::json::parse( response, nullptr /*callback*/, false /*allowExceptions*/ );
			if ( const auto session = document.find( s_attr_session ); ( document.end() != session ) ) {
				if ( const auto value = GetString( *session, s_attr_key ); !value.empty() && ( value.size() < static_cast<size_t>( skSize ) ) ) {
					strcpy_s( sk, skSize, value.c_str() );
					result = scrobbler_success;
				}
			}
		} else if ( !response.empty() ) {
			result = GetErrorCode( response );
		}
	}
	return result;
}

int32_t Scrobbler::NowPlaying( const char* sk, const scrobbler_track* track )
{
	int32_t result = scrobbler_error;
	if ( ( nullptr != sk ) && ( strlen( sk ) > 0 ) && IsValidTrack( track ) ) {
		StringMap parameters = { { "method", "track.updateNowPlaying" }, { "api_key", s_apikey }, { "sk", sk } };
		parameters.insert( StringMap::value_type( "artist", track->Artist ) );
		parameters.insert( StringMap::value_type( "track", track->Title ) );
		if ( ( nullptr != track->Album ) && ( strlen( track->Album ) > 0 ) ) {
			parameters.insert( StringMap::value_type( "album", track->Album ) );
		}
		if ( ( nullptr != track->AlbumArtist ) && ( strlen( track->AlbumArtist ) > 0 ) ) {
			parameters.insert( StringMap::value_type( "albumArtist", track->AlbumArtist ) );
		}
		if ( track->Tracknumber > 0 ) {
			parameters.insert( StringMap::value_type( "trackNumber", std::to_string( track->Tracknumber ) ) );
		}
		if ( track->Duration > 0 ) {
			parameters.insert( StringMap::value_type( "duration", std::to_string( track->Duration ) ) );
		}
		const std::string target = std::string( s_url );
		const std::string sig = ConstructMethodSignature( parameters );
		parameters.insert( StringMap::value_type( "format", "json" ) );
		const std::string content = ConstructMethod( parameters ) + sig;
		std::string response;
		if ( SendRequest( Verb::Post, target, content, response ) ) {
			result = scrobbler_success;
		} else if ( !response.empty() ) {
			result = GetErrorCode( response );
		}
	}
	return result;
}

int32_t Scrobbler::Scrobble( const char* sk, const scrobbler_track** tracks, const int32_t trackCount )
{
	int32_t result = scrobbler_error;
	if ( ( nullptr != sk ) && ( strlen( sk ) > 0 ) && ( trackCount > 0 ) && ( trackCount <= 50 ) ) {
		StringMap parameters = { { "method", "track.scrobble" }, { "api_key", s_apikey }, { "sk", sk } };
		int scrobbleIndex = 0;
		for ( int32_t trackIndex = 0; trackIndex < trackCount; trackIndex++ ) {
			const scrobbler_track* track = tracks[ trackIndex ];
			if ( IsValidTrack( track ) && ( track->Timestamp > 0 ) && ( track->Duration >= 30 ) ) {
				const std::string i = "[" + std::to_string( scrobbleIndex++ ) + "]";
				parameters.insert( StringMap::value_type( "timestamp" + i, std::to_string( track->Timestamp ) ) );
				parameters.insert( StringMap::value_type( "artist" + i, track->Artist ) );
				parameters.insert( StringMap::value_type( "track" + i, track->Title ) );
				if ( ( nullptr != track->Album ) && ( strlen( track->Album ) > 0 ) ) {
					parameters.insert( StringMap::value_type( "album" + i, track->Album ) );
				}
				if ( ( nullptr != track->AlbumArtist ) && ( strlen( track->AlbumArtist ) > 0 ) ) {
					parameters.insert( StringMap::value_type( "albumArtist" + i, track->AlbumArtist ) );
				}
				if ( track->Tracknumber > 0 ) {
					parameters.insert( StringMap::value_type( "trackNumber" + i, std::to_string( track->Tracknumber ) ) );
				}
				if ( track->Duration > 0 ) {
					parameters.insert( StringMap::value_type( "duration" + i, std::to_string( track->Duration ) ) );
				}
			}
		}
		const std::string target = std::string( s_url );
		const std::string sig = ConstructMethodSignature( parameters );
		parameters.insert( StringMap::value_type( "format", "json" ) );
		const std::string content = ConstructMethod( parameters ) + sig;
		std::string response;
		if ( SendRequest( Verb::Post, target, content, response ) ) {
			result = scrobbler_success;
		} else if ( !response.empty() ) {
			result = GetErrorCode( response );
		}
	}
	return result;
}

std::string Scrobbler::ConstructMethodSignature( const Scrobbler::StringMap& parameters )
{
	std::string signature;
	for ( const auto& iter : parameters ) {
		signature += iter.first;
		signature += iter.second;
	}
	signature += s_apisecret;

	UCHAR digest[ 16 ] = {};
	BCRYPT_ALG_HANDLE hAlgorithm = nullptr;
	if ( BCRYPT_SUCCESS( BCryptOpenAlgorithmProvider( &hAlgorithm, BCRYPT_MD5_ALGORITHM, nullptr /*implementation*/, 0 /*flags*/ ) ) ) {
		BCRYPT_HASH_HANDLE hHash = nullptr;
		if ( BCRYPT_SUCCESS( BCryptCreateHash( hAlgorithm, &hHash, nullptr /*hashObject*/, 0 /*hashObjectSize*/, nullptr /*secret*/, 0 /*secretSize*/, 0 /*flags*/ ) ) ) {
			if ( BCRYPT_SUCCESS( BCryptHashData( hHash, reinterpret_cast<PUCHAR>( signature.data() ), static_cast<ULONG>( signature.size() ), 0 /*flags*/ ) ) ) {
				BCryptFinishHash( hHash, digest, static_cast<ULONG>( sizeof( digest ) ), 0 /*flags*/ );
			}
			BCryptDestroyHash( hHash );
		}
		BCryptCloseAlgorithmProvider( hAlgorithm, 0 /*flags*/ );
	}

	std::ostringstream ss;
	for ( int i = 0; i < 16; i++ ) {
		const unsigned int value = static_cast<unsigned char>( digest[ i ] );
		ss << std::setfill( '0' ) << std::setw( 2 ) << std::hex << value;
	}
	const std::string hashedsignature = "api_sig=" + ss.str();
	return hashedsignature;
}

std::string Scrobbler::ConstructMethod( const Scrobbler::StringMap& parameters )
{
	std::string method;
	for ( const auto& iter : parameters ) {
		method += iter.first;
		method += "=";
		method += UrlEncode( iter.second );
		method += "&";
	}
	return method;
}

int32_t Scrobbler::GetErrorCode( const std::string& response )
{
	int32_t result = scrobbler_error;
	const nlohmann::json document = nlohmann::json::parse( response, nullptr /*callback*/, false /*allowExceptions*/ );
	if ( const auto error = document.find( s_attr_error ); ( document.end() != error ) && error->is_number_integer() ) {
		const int64_t value = error->get<int64_t>();
		if ( ( value > 0 ) && ( value <= INT32_MAX ) ) {
			result = static_cast<int32_t>( value );
		}
	}
	return result;
}

std::string Scrobbler::GetString( const nlohmann::json& object, const char* name )
{
	std::string result;
	if ( const auto value = object.find( name ); ( object.end() != value ) && value->is_string() ) {
		result = value->get<std::string>();
	}
	return result;
}

std::string Scrobbler::UrlEncode( const std::string& content )
{
	std::ostringstream ss;
	for ( const auto& c : content ) {
		if ( ( ( c >= '0' ) && ( c <= '9' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) || ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( c == '-' ) || ( c == '_' ) || ( c == '.' ) || ( c == '~' ) ) {
			ss << c;
		} else {
			const unsigned int value = static_cast<unsigned char>( c );
			ss << '%' << std::setfill( '0' ) << std::setw( 2 ) << std::hex << std::uppercase << value;
		}
	}
	const std::string result = ss.str();
	return result;
}

bool Scrobbler::IsValidTrack( const scrobbler_track* track )
{
	const bool valid = ( nullptr != track ) && ( nullptr != track->Artist ) && ( strlen( track->Artist ) > 0 ) && ( nullptr != track->Title ) && ( strlen( track->Title ) > 0 );
	return valid;
}
