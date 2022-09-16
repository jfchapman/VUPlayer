#pragma once

#include "stdafx.h"

#include "Settings.h"

#include "audioscrobbler.h"

// Scrobbler wrapper.
class Scrobbler
{
public:
	// 'database' - application database.
	// 'settings' - application settings.
	Scrobbler( Database& database, Settings& settings );

	virtual ~Scrobbler();

	// Returns whether Scrobbler functionality is available.
	bool IsAvailable() const;

	// Launches a web browser to authorise VUPlayer for scrobbling.
	void Authorise();

	// Sets the track now playing.
	// 'mediaInfo' - media information.
	void NowPlaying( const MediaInfo& mediaInfo );

	// Scrobbles a track.
	// 'mediaInfo' - media information.
	// 'startTime' - timestamp for when the track started playing (UTC).
	void Scrobble( const MediaInfo& mediaInfo, const time_t startTime );

	// Returns the current authorisation token.
	std::string GetToken();

	// Wakes up the scrobbler.
	void WakeUp();

private:
	// Init function type.
	typedef int32_t (*scrobbler_init)();

	// Close function type.
	typedef void (*scrobbler_close)();

	// Authorize function type.
	typedef void (*scrobbler_authorize)( const char* token );

	// Get Token function type.
	typedef int32_t (*scrobbler_get_token)( char* token, const int32_t tokenSize );

	// Get Session Key function type.
	typedef int32_t (*scrobbler_get_session)( const char* token, char* sk, const int32_t skSize );

	// Now Playing function type.
	typedef int32_t (*scrobbler_now_playing)( const char* sk, const scrobbler_track* track );

	// Scrobble function type.
	typedef int32_t (*scrobbler_scrobble)( const char* sk, const scrobbler_track** tracks, const int32_t trackCount );

	// Track information.
	struct TrackInfo
	{
		std::string	Artist;				// Track artist, UTF-8 encoded (REQUIRED)
		std::string	Title;				// Track title, UTF-8 encoded (REQUIRED)
		std::string	Album;				// Album, UTF-8 encoded (optional)
		int32_t			Tracknumber;	// Track number (optional)
		int32_t			Duration;			// Track length in seconds (optional)
	};

	// Maps a timestamp to track information.
	typedef std::map<time_t,TrackInfo> PendingScrobbles;

	// A set of timestamps.
	typedef std::set<time_t> Timestamps;

	// Scrobbler thread procedure.
	// 'lParam' - thread parameter.
	static DWORD WINAPI ScrobblerThreadProc( LPVOID lParam );

	// Scrobbler thread handler.
	void ScrobblerHandler();

	// Returns whether scrobbling is enabled.
	bool IsEnabled() const;

	// Sets the authorisation token.
	void SetToken( const std::string& token );

	// Gets the session key.
	std::string GetSessionKey();

	// Sets the session key.
	// 'key' - session key.
	// 'updateSettings' - whether to update the application settings.
	void SetSessionKey( const std::string& key, const bool updateSettings = true );

	// Reads cached scrobbles from the application database and adds them to the pending queue.
	void ReadCachedScrobbles();

	// Writes pending scrobbles to the application database.
	void SaveCachedScrobbles();

	// Removes scrobbles corresponding to the 'timestamps', both from the pending queue, and the database cache.
	void RemovePendingScrobbles( const Timestamps& timestamps );

	// Application database.
	Database& m_Database;

	// Application settings.
	Settings& m_Settings;

	// Scrobbler library handle.
	HMODULE m_Handle;

	// Init function.
	scrobbler_init m_Init;

	// Close function.
	scrobbler_close m_Close;

	// Authorize function.
	scrobbler_authorize m_Authorize;

	// Get Token function.
	scrobbler_get_token m_GetToken;

	// Get Session Key function.
	scrobbler_get_session m_GetSession;

	// Now Playing function type.
	scrobbler_now_playing m_NowPlaying;

	// Scrobble function type.
	scrobbler_scrobble m_Scrobble;

	// Indicates whether scrobbler functionality has been initialised.
	bool m_Initialised;

	// Track now playing.
	TrackInfo m_TrackNowPlaying;

	// Pending scrobbles.
	PendingScrobbles m_PendingScrobbles;

	// Mutex.
	std::mutex m_Mutex;

	// Authorisation token.
	std::string m_Token;

	// Session key.
	std::string m_SessionKey;

	// Scrobbler thread handle.
	HANDLE m_ScrobblerThread;

	// Event handle with which to stop the scrobbler thread.
	HANDLE m_StopEvent;

	// Event handle with which to wake the scrobbler thread.
	HANDLE m_WakeEvent;
};
