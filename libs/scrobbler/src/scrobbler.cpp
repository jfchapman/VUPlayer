// Scrobbler 2.0 interface for VUPlayer

#include "audioscrobbler.h"
#include "scrobbler_impl.h"

// VUPlayer application name.
static constexpr wchar_t s_VUPlayer[] = L"VUPlayer";

// Global scrobbler instance.
static Scrobbler* g_scrobbler = nullptr;

int32_t init()
{
	// Do a simple check to see if we're being called from the VUPlayer application, and get the version number.
	const HMODULE hInstance = GetModuleHandle( nullptr );
	wchar_t appName[ 32 ] = {};
	LoadString( hInstance, 103, appName, 32 );
	const bool appCheckOK = ( 0 == wcscmp( s_VUPlayer, appName ) );
	if ( appCheckOK ) {
		if ( nullptr == g_scrobbler ) {
			g_scrobbler = new Scrobbler();
		}
	}
	const int32_t result = appCheckOK ? scrobbler_success : scrobbler_error;
	return result;
}

void close()
{
	delete g_scrobbler;
	g_scrobbler = nullptr;
}

void authorize( const char* token )
{
	if ( nullptr != g_scrobbler ) {
		g_scrobbler->Authorize( token );
	}
}

int32_t get_token( char* token, const int32_t tokenSize )
{
	int32_t result = scrobbler_error;
	if ( nullptr != g_scrobbler ) {
		result = g_scrobbler->GetToken( token, tokenSize );
	}
	return result;
}

int32_t get_session( const char* token, char* sk, const int32_t skSize )
{
	int32_t result = scrobbler_error;
	if ( nullptr != g_scrobbler ) {
		result = g_scrobbler->GetSessionKey( token, sk, skSize );
	}
	return result;
}

int32_t now_playing( const char* sk, const scrobbler_track* track )
{
	int32_t result = scrobbler_error;
	if ( nullptr != g_scrobbler ) {
		result = g_scrobbler->NowPlaying( sk, track );
	}
	return result;
}

int32_t scrobble( const char* sk, const scrobbler_track** tracks, const int32_t trackCount )
{
	int32_t result = scrobbler_error;
	if ( nullptr != g_scrobbler ) {
		result = g_scrobbler->Scrobble( sk, tracks, trackCount );
	}
	return result;
}
