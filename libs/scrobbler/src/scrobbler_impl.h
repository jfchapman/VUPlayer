// Scrobbler 2.0 implementation for VUPlayer
#pragma once

#include <Windows.h>
#include <WinInet.h>

#include "json.hpp"

#include <map>
#include <string>

struct scrobbler_track;

class Scrobbler
{
public:
	Scrobbler();

	virtual ~Scrobbler();

// Opens a web page, allowing a user to authorize the application for their scrobbler account. 
// 'token' - authorization token.
	void Authorize( const char* token );

	// Gets an authentication token.
	// 'token' - out, authentication token.
	// 'tokenSize' - maximum token size, in characters.
	// Returns scrobbler_success, or an error code.
	int32_t GetToken( char* token, const int32_t tokenSize );

	// Gets a session key.
	// 'token' - authentication token.
	// 'sk' - out, session key.
	// 'skSize' - maximum key size, in characters.
	// Returns scrobbler_success, or an error code.
	int32_t GetSessionKey( const char* token, char* sk, const int32_t skSize );

	// Marks a track as now playing.
	// 'sk' - session key.
	// 'track' - track information.
	// Returns scrobbler_success, or an error code.
	int32_t NowPlaying( const char* sk, const scrobbler_track* track );

	// Scrobbles tracks.
	// 'sk' - session key.
	// 'tracks' - track information.
	// 'trackCount' - number of tracks (up to a maximum of 50).
	// Returns scrobbler_success, or an error code.
	int32_t Scrobble( const char* sk, const scrobbler_track** tracks, const int32_t trackCount );

private:
	// Maps a string name to a string value.
	typedef std::map<std::string,std::string> StringMap;

	// Method verbs.
	enum class Verb { Get, Post };

	// Opens the top level internet session, with request timeouts applied.
	// Returns the session handle, or nullptr on failure.
	static HINTERNET OpenSession();

	// Performs an HTTPS request.
	// 'verb' - request verb.
	// 'target' - UTF-8 encoded target.
	// 'content' - UTF-8 encoded content (POST requests only).
	// 'response' - out, UTF-8 encoded response (which can contain error information when the request fails).
	// Returns whether the request succeeded.
	bool SendRequest( const Verb verb, const std::string& target, const std::string& content, std::string& response );

	// Returns a scrobbler method signature from the 'parameters'.
	static std::string ConstructMethodSignature( const StringMap& parameters );

	// Returns a URL encoded scrobbler method from the 'parameters'.
	static std::string ConstructMethod( const StringMap& parameters );

	// Returns an error code from the 'response'.
	static int32_t GetErrorCode( const std::string& response );

	// Returns the string value of the 'name' attribute of the JSON 'object', or an empty string if there is no such string attribute.
	static std::string GetString( const nlohmann::json& object, const char* name );

	// Returns the URL encoding of the 'content'.
	static std::string UrlEncode( const std::string& content );

	// Returns whether 'track' is valid for scrobbling.
	static bool IsValidTrack( const scrobbler_track* track );

	// Top level internet session handle.
	const HINTERNET m_hSession;

	// Server connection handle.
	const HINTERNET m_hConnection;
};
