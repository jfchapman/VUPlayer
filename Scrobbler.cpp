#include "Scrobbler.h"

#include "Utility.h"
#include "VUPlayer.h"

// Maximum number of scrobbles per request.
const size_t s_ScrobbleBatchSize = 20;

// Maximum length of time to cache scrobbles, in seconds.
const time_t s_ScrobblerCacheLength = 60 * 60 * 24 * 7;

Scrobbler::Scrobbler( Database& database, Settings& settings, const bool disable ) :
	m_Database( database ),
	m_Settings( settings ),
	m_Handle( disable ? nullptr : LoadLibrary( L"scrobbler.dll" ) ),
	m_Init( nullptr ),
	m_Close( nullptr ),
	m_Authorize( nullptr ),
	m_GetToken( nullptr ),
	m_GetSession( nullptr ),
	m_NowPlaying( nullptr ),
	m_Scrobble( nullptr ),
	m_Initialised( false ),
	m_TrackNowPlaying( {} ),
	m_PendingScrobbles(),
	m_Mutex(),
	m_ScrobblerThread( nullptr ),
	m_Token(),
	m_SessionKey(),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) )
{
	if ( nullptr != m_Handle ) {
		m_Init = reinterpret_cast<scrobbler_init>( GetProcAddress( m_Handle, "init" ) );
		m_Close = reinterpret_cast<scrobbler_close>( GetProcAddress( m_Handle, "close" ) );
		m_Authorize = reinterpret_cast<scrobbler_authorize>( GetProcAddress( m_Handle, "authorize" ) );
		m_GetToken = reinterpret_cast<scrobbler_get_token>( GetProcAddress( m_Handle, "get_token" ) );
		m_GetSession = reinterpret_cast<scrobbler_get_session>( GetProcAddress( m_Handle, "get_session" ) );
		m_NowPlaying = reinterpret_cast<scrobbler_now_playing>( GetProcAddress( m_Handle, "now_playing" ) );
		m_Scrobble = reinterpret_cast<scrobbler_scrobble>( GetProcAddress( m_Handle, "scrobble" ) );

		if ( ( nullptr == m_Init ) || ( nullptr == m_Close ) || ( nullptr == m_Authorize ) ||
				( nullptr == m_GetToken ) || ( nullptr == m_GetSession ) || ( nullptr == m_NowPlaying ) || ( nullptr == m_Scrobble ) ) {
			FreeLibrary( m_Handle );
			m_Handle = nullptr;
		} else {
			m_Initialised = ( scrobbler_success == m_Init() );
			if ( m_Initialised ) {
				SetSessionKey( m_Settings.GetScrobblerKey(), false /*updateSettings*/ );
				m_ScrobblerThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, ScrobblerThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
			}
		}
	}
}

Scrobbler::~Scrobbler()
{
	if ( nullptr != m_ScrobblerThread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_ScrobblerThread, INFINITE );
		m_ScrobblerThread = nullptr;
		m_Close();
	}
	CloseHandle( m_StopEvent );
	CloseHandle( m_WakeEvent );

	if ( nullptr != m_Handle ) {
		FreeLibrary( m_Handle );
		m_Handle = nullptr;
	}
}

DWORD WINAPI Scrobbler::ScrobblerThreadProc( LPVOID lParam )
{
	Scrobbler* scrobbler = static_cast<Scrobbler*>( lParam );
	if ( nullptr != scrobbler ) {
		scrobbler->ScrobblerHandler();
	}
	return 0;
}

bool Scrobbler::IsAvailable() const
{
	return m_Initialised;
}

bool Scrobbler::IsEnabled() const
{
	const bool enabled = m_Settings.GetScrobblerEnabled();
	return enabled;
}

void Scrobbler::Authorise()
{
	if ( IsAvailable() ) {
		const int32_t tokenSize = 256;
		char token[ tokenSize ] = {};
		if ( scrobbler_success ==	m_GetToken( token, tokenSize ) ) {
			SetToken( token );
			m_Authorize( token );
		}
	}
}

void Scrobbler::NowPlaying( const MediaInfo& mediaInfo )
{
	if ( IsAvailable() && IsEnabled() ) {
		std::lock_guard<std::mutex> lock( m_Mutex );
		m_TrackNowPlaying.Artist = WideStringToUTF8( mediaInfo.GetArtist() );
		m_TrackNowPlaying.Title = WideStringToUTF8( mediaInfo.GetTitle() );
		m_TrackNowPlaying.Album = WideStringToUTF8( mediaInfo.GetAlbum() );
		m_TrackNowPlaying.Tracknumber = static_cast<int32_t>( mediaInfo.GetTrack() );
		m_TrackNowPlaying.Duration = static_cast<int32_t>( mediaInfo.GetDuration() );
		SetEvent( m_WakeEvent );
	}
}

void Scrobbler::Scrobble( const MediaInfo& mediaInfo, const time_t startTime )
{
	if ( IsAvailable() && IsEnabled() ) {
		// Check that the track meets the requirements for scrobbling.
		const time_t trackSeconds = static_cast<time_t>( mediaInfo.GetDuration() );
		if ( trackSeconds >= 30 ) {
			const time_t now = time( nullptr );
			const time_t playedSeconds = now - startTime;
			if ( ( playedSeconds >= 240 ) || ( ( playedSeconds * 2 ) >= trackSeconds ) ) {
				std::lock_guard<std::mutex> lock( m_Mutex );
				const TrackInfo trackInfo = { 
					WideStringToUTF8( mediaInfo.GetArtist() ),
					WideStringToUTF8( mediaInfo.GetTitle() ),
					WideStringToUTF8( mediaInfo.GetAlbum() ),
					static_cast<int32_t>( mediaInfo.GetTrack() ),
					static_cast<int32_t>( mediaInfo.GetDuration() )
				};
				if ( !trackInfo.Artist.empty() && !trackInfo.Title.empty() ) {
					m_PendingScrobbles.insert( PendingScrobbles::value_type( startTime, trackInfo ) );
				}
				SetEvent( m_WakeEvent );
			}
		}
	}
}

std::string Scrobbler::GetToken()
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	return m_Token;
}

void Scrobbler::SetToken( const std::string& token )
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	m_Token = token;
}

std::string Scrobbler::GetSessionKey()
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	return m_SessionKey;
}

void Scrobbler::SetSessionKey( const std::string& key, const bool updateSettings )
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	m_SessionKey = key;
	if ( updateSettings ) {
		m_Settings.SetScrobblerKey( key );
	}
}

void Scrobbler::WakeUp()
{
	if ( IsAvailable() ) {	
		SetEvent( m_WakeEvent );
	}
}

void Scrobbler::ScrobblerHandler()
{
	ReadCachedScrobbles();
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		// Get the session key, if possible.
		std::string sk = GetSessionKey();
		if ( sk.empty() ) {
			std::string token = GetToken();
			if ( !token.empty() ) {
				const int32_t skBufferSize = 256;
				char skBuffer[ skBufferSize ] = {};
				const int32_t result = m_GetSession( token.c_str(), skBuffer, skBufferSize );
				switch ( result ) {
					case scrobbler_success : {
						SetSessionKey( skBuffer );
						break;
					}
					case scrobbler_error_expiredtoken : {
						SetToken( std::string() );
						break;
					}
				}
			}
			sk = GetSessionKey();
		}

		if ( !sk.empty() ) {
			// Update now playing.
			TrackInfo nowPlaying = {};
			{
				std::lock_guard<std::mutex> lock( m_Mutex );
				nowPlaying = m_TrackNowPlaying;
				m_TrackNowPlaying = {};
			}
			if ( !nowPlaying.Artist.empty() && !nowPlaying.Title.empty() && ( nowPlaying.Duration > 0 ) ) {
				const scrobbler_track track( nowPlaying.Artist.c_str(), nowPlaying.Title.c_str(), 0 /*timeStamp*/,
					nowPlaying.Album.c_str(), nullptr /*albumArtist*/, nowPlaying.Tracknumber, nowPlaying.Duration );
				const int32_t result = m_NowPlaying( sk.c_str(), &track );
				switch ( result ) {
					case scrobbler_error_invalidsessionkey :
					case scrobbler_error_authenticationfailed : {
						SetSessionKey( std::string() );
						break;
					}
				}
			}

			// Process pending scrobbles.
			PendingScrobbles scrobbles;
			{
				std::lock_guard<std::mutex> lock( m_Mutex );
				scrobbles = m_PendingScrobbles;
			}
			while ( !scrobbles.empty() && !sk.empty() && ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) ) {
				std::vector<scrobbler_track> scrobbler_tracks;
				scrobbler_tracks.reserve( std::min<size_t>( scrobbles.size(), s_ScrobbleBatchSize ) );
				size_t scrobbleCount = 0;
				auto pendingScrobble = scrobbles.begin();
				while ( ( scrobbles.end() != pendingScrobble ) && ( scrobbleCount < s_ScrobbleBatchSize ) ) {
					const time_t timestamp = pendingScrobble->first;
					const TrackInfo& info = pendingScrobble->second;
					const scrobbler_track track( info.Artist.c_str(), info.Title.c_str(), timestamp, info.Album.c_str(), nullptr /*albumArtist*/, info.Tracknumber, info.Duration );
					scrobbler_tracks.push_back( track );
					++pendingScrobble;
					++scrobbleCount;
				}
				std::vector<const scrobbler_track*> tracks;
				tracks.reserve( scrobbler_tracks.size() );
				for ( const auto& track : scrobbler_tracks ) {
					tracks.push_back( &track );
				}
				const int32_t result = m_Scrobble( sk.c_str(), &tracks[ 0 ], static_cast<int32_t>( tracks.size() ) );
				switch( result ) {
					case scrobbler_success : {
						Timestamps scrobbledTimestamps;
						for ( const auto& track : scrobbler_tracks ) {
							scrobbledTimestamps.insert( track.Timestamp );
							scrobbles.erase( track.Timestamp );
						}
						RemovePendingScrobbles( scrobbledTimestamps );
						break;
					}
					case scrobbler_error_invalidsessionkey :
					case scrobbler_error_authenticationfailed : {
						scrobbles.clear();
						SetSessionKey( std::string() );
						break;
					}
					default : {
						scrobbles.clear();
						break;
					}
				}
			}
		}
		ResetEvent( m_WakeEvent );
	}
	SaveCachedScrobbles();
}

void Scrobbler::ReadCachedScrobbles()
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Drop any cached scrobbles that are too old.
		const time_t now = time( nullptr );
		const time_t cutoff = now - s_ScrobblerCacheLength;
		const std::string dropQuery = "DELETE FROM Scrobbles WHERE Timestamp < " + std::to_string( cutoff );
		sqlite3_exec( database, dropQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Read in the remaining cached scrobbles.
		sqlite3_stmt* stmt = nullptr;
		const std::string selectQuery = "SELECT Timestamp, Artist, Title, Album, Track, Duration FROM Scrobbles;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, selectQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const time_t timestamp = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				TrackInfo info = {};
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 1 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					info.Artist = text;
				}
				text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 2 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					info.Title = text;
				}
				text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 3 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					info.Album = text;
				}
				info.Tracknumber = static_cast<int32_t>( sqlite3_column_int( stmt, 4 /*columnIndex*/ ) );
				info.Duration = static_cast<int32_t>( sqlite3_column_int( stmt, 5 /*columnIndex*/ ) );
				if ( !info.Artist.empty() && !info.Title.empty() ) {
					m_PendingScrobbles.insert( PendingScrobbles::value_type( timestamp, info ) );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	if ( !m_PendingScrobbles.empty() ) {
		SetEvent( m_WakeEvent );
	}
}

void Scrobbler::SaveCachedScrobbles()
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	if ( !m_PendingScrobbles.empty() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			// Ensure the cached scrobbles table exists in the application database.
			const std::string scrobblerTableQuery = "CREATE TABLE IF NOT EXISTS Scrobbles(Timestamp, Artist, Title, Album, Track, Duration, PRIMARY KEY(Timestamp));";
			sqlite3_exec( database, scrobblerTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

			// Check the columns in the cached scrobbles table.
			const std::string scrobblerInfoQuery = "PRAGMA table_info('Scrobbles')";
			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, scrobblerInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				std::set<std::string> columns;
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					const int columnCount = sqlite3_column_count( stmt );
					for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
						const std::string columnName = sqlite3_column_name( stmt, columnIndex );
						if ( columnName == "name" ) {
							const std::string name = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
							columns.insert( name );
							break;
						}
					}
				}
				sqlite3_finalize( stmt );
				stmt = nullptr;

				if ( ( columns.find( "Timestamp" ) == columns.end() ) || ( columns.find( "Artist" ) == columns.end() ) || ( columns.find( "Title" ) == columns.end() ) ||
						( columns.find( "Album" ) == columns.end() ) || ( columns.find( "Track" ) == columns.end() ) || ( columns.find( "Duration" ) == columns.end() ) ) {
					// Drop the table and recreate.
					const std::string dropTableQuery = "DROP TABLE Scrobbles;";
					sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					sqlite3_exec( database, scrobblerTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				}
			}

			// Remove any pending scrobbles that are too old.
			const time_t now = time( nullptr );
			time_t cutoff = now - s_ScrobblerCacheLength;
			auto pendingScrobble = m_PendingScrobbles.begin();
			while ( m_PendingScrobbles.end() != pendingScrobble ) {
				const time_t timestamp = pendingScrobble->first;
				if ( timestamp < cutoff ) {
					pendingScrobble = m_PendingScrobbles.erase( pendingScrobble );
				} else {
					break;
				}
			}

			if ( m_PendingScrobbles.empty() ) {
				// Ensure all cached scrobbles are cleared.
				const std::string dropQuery = "DELETE FROM Scrobbles;";
				sqlite3_exec( database, dropQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			} else {
				// Ensure the database does not contain cached scrobbles that are too old.
				cutoff = m_PendingScrobbles.begin()->first;
				const std::string dropQuery = "DELETE FROM Scrobbles WHERE Timestamp < " + std::to_string( cutoff );
				sqlite3_exec( database, dropQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

				// Insert any pending scrobbles that are not already cached.
				const std::string insertQuery = "INSERT OR IGNORE INTO Scrobbles (Timestamp,Artist,Title,Album,Track,Duration) VALUES (?1,?2,?3,?4,?5,?6);";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, insertQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					for ( const auto& scrobble : m_PendingScrobbles ) {
						const time_t timestamp = scrobble.first;
						const TrackInfo& info = scrobble.second;
						sqlite3_bind_int64( stmt, 1, timestamp );
						sqlite3_bind_text( stmt, 2, info.Artist.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_text( stmt, 3, info.Title.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_text( stmt, 4, info.Album.c_str(), -1 /*strLen*/, SQLITE_STATIC );		
						sqlite3_bind_int( stmt, 5, static_cast<int>( info.Tracknumber ) );
						sqlite3_bind_int( stmt, 6, static_cast<int>( info.Duration ) );
						sqlite3_step( stmt );
						sqlite3_reset( stmt );
					}
					sqlite3_finalize( stmt );
					stmt = nullptr;
				}
			}
		}
	}
}

void Scrobbler::RemovePendingScrobbles( const Timestamps& timestamps )
{
	if ( !timestamps.empty() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			sqlite3_stmt* stmt = nullptr;
			const std::string dropQuery = "DELETE FROM Scrobbles WHERE Timestamp == ?1;";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, dropQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				for ( const auto& timestamp : timestamps ) {
					sqlite3_bind_int64( stmt, 1, timestamp );
					sqlite3_step( stmt );
					sqlite3_reset( stmt );
				}
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
		std::lock_guard<std::mutex> lock( m_Mutex );
		for ( const auto& timestamp : timestamps ) {
			m_PendingScrobbles.erase( timestamp );
		}
	}
}
