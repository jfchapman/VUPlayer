// Scrobbler 2.0 interface for VUPlayer
#pragma once

#include <inttypes.h>

#ifdef SCROBBLER_EXPORTS
#define scrobbler_export extern "C" __declspec( dllexport )
#else
#define scrobbler_export
#endif

#define scrobbler_success												0

#define scrobbler_error													1

#define scrobbler_error_invalidservice					2
#define scrobbler_error_invalidmethod						3
#define scrobbler_error_authenticationfailed		4
#define scrobbler_error_invalidformat						5
#define scrobbler_error_invalidparameters				6
#define scrobbler_error_invalidresource					7
#define scrobbler_error_operationfailed					8
#define scrobbler_error_invalidsessionkey				9
#define scrobbler_error_invalidapikey						10
#define scrobbler_error_serviceoffline					11
#define scrobbler_error_subscribersonly					12
#define scrobbler_error_invalidmethodsignature	13
#define scrobbler_error_unauthorizedtoken				14
#define scrobbler_error_expiredtoken						15
#define scrobbler_error_serviceunavailable			16

// Track information.
struct scrobbler_track
{
	scrobbler_track() : Artist( nullptr ), Title( nullptr ), Timestamp( 0 ), Album( nullptr ), AlbumArtist( nullptr ), Tracknumber( 0 ), Duration( 0 ) {}

	scrobbler_track( const char* artist, const char* title, int64_t timestamp, const char* album, const char* albumartist, const int32_t tracknumber, const int32_t duration )
		: Artist( artist ), Title( title ), Timestamp( timestamp ), Album( album ), AlbumArtist( albumartist ), Tracknumber( tracknumber ), Duration( duration ) {}

	const char*	Artist;				// Track artist, UTF-8 encoded (REQUIRED)
	const char*	Title;				// Track title, UTF-8 encoded (REQUIRED)
	int64_t			Timestamp;		// UNIX timestamp in UTC (REQUIRED)
	const char*	Album;				// Album, UTF-8 encoded (optional)
	const char* AlbumArtist;	// Album artist if different from track artist, UTF-8 encoded (optional)
	int32_t			Tracknumber;	// Track number (optional)
	int32_t			Duration;			// Track length in seconds (optional)
};

// Initialises scrobbler functionality.
// Returns scrobbler_success, or an error code.
scrobbler_export int32_t init();

// Closes scrobbler functionality.
scrobbler_export void close();

// Opens a web page, allowing a user to authorize the application for their scrobbler account. 
// 'token' - authorization token.
scrobbler_export void authorize( const char* token );

// Gets an authentication token.
// 'token' - out, authentication token.
// 'tokenSize' - maximum token size, in characters.
// Returns scrobbler_success, or an error code.
scrobbler_export int32_t get_token( char* token, const int32_t tokenSize );

// Gets a session key.
// 'token' - authentication token.
// 'sk' - out, session key.
// 'skSize' - maximum key size, in characters.
// Returns scrobbler_success, or an error code.
scrobbler_export int32_t get_session( const char* token, char* sk, const int32_t skSize );

// Marks a track as now playing.
// 'sk' - session key.
// 'track' - track information.
// Returns scrobbler_success, or an error code.
scrobbler_export int32_t now_playing( const char* sk, const scrobbler_track* track );

// Scrobbles tracks.
// 'sk' - session key.
// 'tracks' - track information.
// 'trackCount' - number of tracks (up to a maximum of 50).
// Returns scrobbler_success, or an error code.
scrobbler_export int32_t scrobble( const char* sk, const scrobbler_track** tracks, const int32_t trackCount );
