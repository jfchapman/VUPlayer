#include "Settings.h"

#include "Utility.h"
#include "VUMeter.h"
#include "VUPlayer.h"

// Pitch range options
static const Settings::PitchRangeMap s_PitchRanges = {
		{ Settings::PitchRange::Small, 0.1f },
		{ Settings::PitchRange::Medium, 0.2f },
		{ Settings::PitchRange::Large, 0.3f }
};

// Default conversion/extraction filename format.
static const wchar_t s_DefaultExtractFilename[] = L"%A\\%D\\%N - %T";

Settings::Settings( Database& database, Library& library ) :
	m_Database( database ),
	m_Library( library )
{
	UpdateDatabase();
}

Settings::~Settings()
{
}

void Settings::UpdateDatabase()
{
	UpdateSettingsTable();
	UpdatePlaylistColumnsTable();
	UpdatePlaylistsTable();
	UpdateHotkeysTable();
}

void Settings::UpdateSettingsTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the settings table (if necessary).
		const std::string settingsTableQuery = "CREATE TABLE IF NOT EXISTS Settings(Setting, Value, PRIMARY KEY(Setting));";
		sqlite3_exec( database, settingsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the settings table.
		const std::string settingsInfoQuery = "PRAGMA table_info('Settings')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, settingsInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
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

			if ( ( columns.find( "Setting" ) == columns.end() ) || ( columns.find( "Value" ) == columns.end() ) ) {
				// Drop the table and recreate
				const std::string dropTableQuery = "DROP TABLE Settings;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, settingsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::UpdatePlaylistColumnsTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the playlist columns table (if necessary).
		const std::string columnsTableQuery = "CREATE TABLE IF NOT EXISTS PlaylistColumns(Col, Width);";
		sqlite3_exec( database, columnsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the playlist columns table.
		const std::string columnsInfoQuery = "PRAGMA table_info('PlaylistColumns')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, columnsInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
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

			if ( ( columns.find( "Col" ) == columns.end() ) || ( columns.find( "Width" ) == columns.end() ) ) {
				// Drop the table and recreate
				const std::string dropTableQuery = "DROP TABLE PlaylistColumns;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, columnsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::UpdatePlaylistsTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the playlists table (if necessary).
		const std::string playlistsTableQuery = "CREATE TABLE IF NOT EXISTS Playlists(ID,Name, PRIMARY KEY(ID));";
		sqlite3_exec( database, playlistsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the playlists table.
		const std::string columnsInfoQuery = "PRAGMA table_info('Playlists')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, columnsInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
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

			if ( ( columns.find( "ID" ) == columns.end() ) || ( columns.find( "Name" ) == columns.end() ) ) {
				// Drop the table and recreate
				const std::string dropTableQuery = "DROP TABLE Playlists;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, playlistsTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::UpdatePlaylistTable( const std::string& table )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the playlists table (if necessary).
		std::string createTableQuery = "CREATE TABLE IF NOT EXISTS \"";
		createTableQuery += table;
		createTableQuery += "\"(File,Pending);";
		sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the playlists table.
		std::string columnsInfoQuery = "PRAGMA table_info('";
		columnsInfoQuery += table + "')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, columnsInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
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

			if ( ( columns.find( "File" ) == columns.end() ) || ( columns.find( "Pending" ) == columns.end() ) ) {
				// Drop the table and recreate
				std::string dropTableQuery = "DROP TABLE \"";
				dropTableQuery += table + "\";";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::UpdateHotkeysTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the hotkeys table (if necessary).
		const std::string hotkeyTableQuery = "CREATE TABLE IF NOT EXISTS Hotkeys(ID,Hotkey,Alt,Ctrl,Shift);";
		sqlite3_exec( database, hotkeyTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the hotkeys table.
		const std::string hotkeyInfoQuery = "PRAGMA table_info('Hotkeys')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, hotkeyInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
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

			if ( ( columns.find( "ID" ) == columns.end() ) || ( columns.find( "Hotkey" ) == columns.end() ) ||
					( columns.find( "Alt" ) == columns.end() ) || ( columns.find( "Ctrl" ) == columns.end() ) ||
					( columns.find( "Shift" ) == columns.end() ) ) {
				// Drop the table and recreate
				const std::string dropTableQuery = "DROP TABLE Hotkeys;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, hotkeyTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::GetPlaylistSettings( PlaylistColumns& columns, LOGFONT& font,
			COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		std::string query = "SELECT * FROM PlaylistColumns ORDER BY rowid ASC;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				PlaylistColumn playlistColumn;
				const int columnCount = sqlite3_column_count( stmt );
				for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
					const std::string columnName = sqlite3_column_name( stmt, columnIndex );
					if ( columnName == "Col" ) {
						playlistColumn.ID = sqlite3_column_int( stmt, columnIndex );
					} else if ( columnName == "Width" ) {
						playlistColumn.Width = sqlite3_column_int( stmt, columnIndex );
					}
				}
				columns.push_back( playlistColumn );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListFont';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int bytes = sqlite3_column_bytes( stmt, 0 /*columnIndex*/ );
				if ( sizeof( LOGFONT ) == bytes ) {
					font = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, 0 /*columnIndex*/ ) );
				}
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				fontColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListBackgroundColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				backgroundColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListHighlightColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				highlightColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetPlaylistSettings( const PlaylistColumns& columns, const LOGFONT& font,
			const COLORREF& fontColour, const COLORREF& backgroundColour, const COLORREF& highlightColour )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string clearTableQuery = "DELETE FROM PlaylistColumns;";
		sqlite3_exec( database, clearTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		std::string insertQuery = "INSERT INTO PlaylistColumns (Col,Width) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, insertQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			for ( const auto& columnIter : columns ) {
				sqlite3_bind_int( stmt, 1, columnIter.ID );
				sqlite3_bind_int( stmt, 2, columnIter.Width );
				sqlite3_step( stmt );
				sqlite3_reset( stmt );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		insertQuery = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, insertQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ListFont", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_blob( stmt, 2, &font, sizeof( LOGFONT ), SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ListFontColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( fontColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ListBackgroundColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( backgroundColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ListHighlightColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( highlightColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetTreeSettings( LOGFONT& font, COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour,
		bool& showFavourites, bool& showAllTracks, bool& showArtists, bool& showAlbums, bool& showGenres, bool& showYears )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='TreeFont';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int bytes = sqlite3_column_bytes( stmt, 0 /*columnIndex*/ );
				if ( sizeof( LOGFONT ) == bytes ) {
					font = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, 0 /*columnIndex*/ ) );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				fontColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeBackgroundColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				backgroundColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeHighlightColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				highlightColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeFavourites';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showFavourites = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeAllTracks';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showAllTracks = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeArtists';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showArtists = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeAlbums';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showAlbums = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeGenres';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showGenres = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeYears';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showYears = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetTreeSettings( const LOGFONT& font, const COLORREF& fontColour, const COLORREF& backgroundColour, const COLORREF& highlightColour,
		const bool showFavourites, const bool showAllTracks, const bool showArtists, const bool showAlbums, const bool showGenres, const bool showYears )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string insertQuery = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, insertQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "TreeFont", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_blob( stmt, 2, &font, sizeof( LOGFONT ), SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeFontColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( fontColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeBackgroundColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( backgroundColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeHighlightColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( highlightColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeFavourites", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showFavourites ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeAllTracks", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showAllTracks ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeArtists", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showArtists ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeAlbums", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showAlbums ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeGenres", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showGenres ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "TreeYears", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showYears ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

Playlists Settings::GetPlaylists()
{
	Playlists playlists;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT * FROM Playlists;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				std::string playlistID;
				std::wstring playlistName;
				const int columnCount = sqlite3_column_count( stmt );
				for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
					const std::string columnName = sqlite3_column_name( stmt, columnIndex );
					if ( columnName == "ID" ) {
						const unsigned char* text = sqlite3_column_text( stmt, columnIndex );
						if ( nullptr != text ) {
							playlistID = reinterpret_cast<const char*>( text );
						}
					} else if ( columnName == "Name" ) {
						const unsigned char* text = sqlite3_column_text( stmt, columnIndex );
						if ( nullptr != text ) {
							playlistName = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
						}
					}
				}
				if ( !playlistID.empty() ) {
					Playlist::Ptr playlist( new Playlist( m_Library, playlistID, Playlist::Type::User ) );
					playlist->SetName( playlistName );
					ReadPlaylistFiles( *playlist );
					playlists.push_back( playlist );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return playlists;
}

Playlist::Ptr Settings::GetFavourites()
{
	Playlist::Ptr playlist( new Playlist( m_Library, Playlist::Type::Favourites ) );
	ReadPlaylistFiles( *playlist );
	return playlist;
}

void Settings::ReadPlaylistFiles( Playlist& playlist )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string tableName = ( Playlist::Type::Favourites == playlist.GetType() ) ? "Favourites" : playlist.GetID();
		if ( IsValidGUID( tableName ) || ( Playlist::Type::Favourites == playlist.GetType() ) ) {
			std::string query = "SELECT * FROM \"";
			query += tableName;
			query += "\" ORDER BY rowid ASC;";

			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					bool pending = false;
					std::wstring filename;
					const int columnCount = sqlite3_column_count( stmt );
					for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
						const std::string columnName = sqlite3_column_name( stmt, columnIndex );
						if ( columnName == "File" ) {
							const unsigned char* text = sqlite3_column_text( stmt, columnIndex );
							if ( nullptr != text ) {
								filename = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
							}
						} else if ( columnName == "Pending" ) {
							pending = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
						}
					}
					if ( !filename.empty() ) {
						if ( pending ) {
							playlist.AddPending( filename, false /*startPendingThread*/ );
						} else {
							MediaInfo mediaInfo( filename );
							if ( m_Library.GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/ ) ) {
								playlist.AddItem( mediaInfo );
							} else {
								playlist.AddPending( filename, false /*startPendingThread*/ );
							}
						}
					}
				}
				sqlite3_finalize( stmt );
			}
		}
	}
}

bool Settings::IsValidGUID( const std::string& guid )
{
	RPC_CSTR rpcStr = new unsigned char[ 1 + guid.size() ];
	int pos = 0;
	for ( const auto& iter : guid ) {
		rpcStr[ pos++ ] = static_cast<unsigned char>( iter ); 
	}
	rpcStr[ pos ] = 0;
	UUID uuid;
	const bool valid = ( RPC_S_OK == UuidFromStringA( rpcStr, &uuid ) );
	delete [] rpcStr;

	return valid;
}

void Settings::RemovePlaylist( const Playlist& playlist )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string& playlistID = playlist.GetID();
		if ( IsValidGUID( playlistID ) ) {
			const std::string dropFilesTableQuery = "DROP TABLE \"" + playlistID + "\";";
			sqlite3_exec( database, dropFilesTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

			const std::string removePlaylistQuery = "DELETE FROM Playlists WHERE ID = ?1;";
			sqlite3_stmt* stmt = nullptr;
			if ( ( SQLITE_OK == sqlite3_prepare_v2( database, removePlaylistQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
					( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, playlistID.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) ) {
				sqlite3_step( stmt );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SavePlaylist( Playlist& playlist )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string playlistID = ( Playlist::Type::Favourites == playlist.GetType() ) ? "Favourites" : playlist.GetID();
		if ( IsValidGUID( playlistID ) || ( Playlist::Type::Favourites == playlist.GetType() ) ) {
			UpdatePlaylistTable( playlistID );

			std::string clearTableQuery = "DELETE FROM \"";
			clearTableQuery += playlistID + "\";";
			sqlite3_exec( database, clearTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

			sqlite3_exec( database, "BEGIN TRANSACTION;", NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			std::string insertFileQuery = "INSERT INTO \"";
			insertFileQuery += playlistID;
			insertFileQuery += "\" (File, Pending) VALUES (?1,?2);";
			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, insertFileQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				bool pending = false;
				const Playlist::ItemList itemList = playlist.GetItems();
				for ( const auto& iter : itemList ) {
					const std::string filename = WideStringToUTF8( iter.Info.GetFilename() );
					if ( !filename.empty() ) {
						sqlite3_bind_text( stmt, 1 /*param*/, filename.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( pending ) );
						sqlite3_step( stmt );
						sqlite3_reset( stmt );
					}
				}
				pending = true;
				const std::list<std::wstring> pendingList = playlist.GetPending();
				for ( const auto& iter : pendingList ) {
					const std::string filename = WideStringToUTF8( iter );
					if ( !filename.empty() ) {
						sqlite3_bind_text( stmt, 1 /*param*/, filename.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( pending ) );
						sqlite3_step( stmt );
						sqlite3_reset( stmt );
					}
				}
				sqlite3_finalize( stmt );
			}
			sqlite3_exec( database, "END TRANSACTION;", NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

			if ( Playlist::Type::Favourites != playlist.GetType() ) {
				const std::string insertPlaylistQuery = "REPLACE INTO Playlists (ID,Name) VALUES (?1,?2);";
				const std::string playlistName = WideStringToUTF8( playlist.GetName() );
				stmt = nullptr;
				if ( ( SQLITE_OK == sqlite3_prepare_v2( database, insertPlaylistQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
						( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, playlistID.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) &&
						( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, playlistName.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) ) {
					sqlite3_step( stmt );
					sqlite3_finalize( stmt );
				}
			}
		}
	}
}

COLORREF Settings::GetOscilloscopeColour()
{
	COLORREF colour = {};
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='OscilloscopeColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				colour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return colour;
}

void Settings::SetOscilloscopeColour( const COLORREF colour )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OscilloscopeColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( colour ) );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

COLORREF Settings::GetOscilloscopeBackground()
{
	COLORREF colour = 0xffffffff;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='OscilloscopeBackground';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				colour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return colour;
}

void Settings::SetOscilloscopeBackground( const COLORREF colour )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OscilloscopeBackground", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( colour ) );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

float Settings::GetOscilloscopeWeight()
{
	float weight = 2.0f;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='OscilloscopeWeight';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const float value = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
				if ( ( value >= 0.5f ) && ( value <= 5.0f ) ) {
					weight = value;
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return weight;
}

void Settings::SetOscilloscopeWeight( const float weight )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OscilloscopeWeight", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, weight );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

float Settings::GetVUMeterDecay()
{
	float decay = VUMeterDecayNormal;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='VUMeterDecay';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const float value = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
				if ( value < VUMeterDecayMinimum ) {
					decay = VUMeterDecayMinimum;
				} else if ( value > VUMeterDecayMaximum ) {
					decay = VUMeterDecayMaximum;
				} else {
					decay = value;
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return decay;
}

void Settings::SetVUMeterDecay( const float decay )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "VUMeterDecay", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, decay );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::GetSpectrumAnalyserSettings( COLORREF& base, COLORREF& peak, COLORREF& background )
{
	base = RGB( 0 /*red*/, 122 /*green*/, 217 /*blue*/ );
	peak = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	background = RGB( 0x00 /*red*/, 0x00 /*green*/, 0x00 /*blue*/ );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='SpectrumAnalyserBase';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				base = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}

		query = "SELECT Value FROM Settings WHERE Setting='SpectrumAnalyserPeak';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				peak = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}

		query = "SELECT Value FROM Settings WHERE Setting='SpectrumAnalyserBackground';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				background = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
	}
}

void Settings::SetSpectrumAnalyserSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "SpectrumAnalyserBase", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( base ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "SpectrumAnalyserPeak", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( peak ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "SpectrumAnalyserBackground", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( background ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetPeakMeterSettings( COLORREF& base, COLORREF& peak, COLORREF& background )
{
	base = RGB( 0 /*red*/, 122 /*green*/, 217 /*blue*/ );
	peak = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	background = RGB( 0 /*red*/, 0 /*green*/, 0 /*blue*/ );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='PeakMeterBase';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				base = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}

		query = "SELECT Value FROM Settings WHERE Setting='PeakMeterPeak';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				peak = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}

		query = "SELECT Value FROM Settings WHERE Setting='PeakMeterBackground';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				background = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
	}
}

void Settings::SetPeakMeterSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "PeakMeterBase", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( base ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "PeakMeterPeak", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( peak ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "PeakMeterBackground", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( background ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetStartupPosition( int& x, int& y, int& width, int& height, bool& maximised, bool& minimised )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='StartupX';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				x = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='StartupY';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				y = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='StartupWidth';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				width = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='StartupHeight';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				height = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='StartupMaximised';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				maximised = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='StartupMinimised';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				minimised = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetStartupPosition( const int x, const int y, const int width, const int height, const bool maximised, const bool minimised )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "StartupX", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, x );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_bind_text( stmt, 1, "StartupY", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, y );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_bind_text( stmt, 1, "StartupWidth", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, width );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_bind_text( stmt, 1, "StartupHeight", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, height );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_bind_text( stmt, 1, "StartupMaximised", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, maximised );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_bind_text( stmt, 1, "StartupMinimised", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, minimised );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

int Settings::GetVisualID()
{
	int visualID = 0;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='VisualID';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				visualID = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
	}
	return visualID;
}

void Settings::SetVisualID( const int visualID )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "VisualID", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, visualID );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

int Settings::GetSplitWidth()
{
	int width = 0;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='SplitWidth';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				width = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
	}
	return width;
}

void Settings::SetSplitWidth( const int width )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "SplitWidth", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, width );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

float Settings::GetVolume()
{
	float volume = 1.0f;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='Volume';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				volume = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return volume;
}

void Settings::SetVolume( const float volume )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "Volume", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, volume );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

std::wstring Settings::GetStartupPlaylist()
{
	std::wstring playlist;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='StartupPlaylist';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					playlist = UTF8ToWideString( text );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return playlist;
}

void Settings::SetStartupPlaylist( const std::wstring& playlist )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "StartupPlaylist", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( playlist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

std::wstring Settings::GetStartupFilename()
{
	std::wstring filename;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='StartupFilename';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					filename = UTF8ToWideString( text );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return filename;
}

void Settings::SetStartupFilename( const std::wstring& filename )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "StartupFilename", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::GetCounterSettings( LOGFONT& font, COLORREF& colour, bool& showRemaining )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='CounterFont';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int bytes = sqlite3_column_bytes( stmt, 0 /*columnIndex*/ );
				if ( sizeof( LOGFONT ) == bytes ) {
					font = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, 0 /*columnIndex*/ ) );
				}
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='CounterFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				colour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='CounterRemaining';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showRemaining = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetCounterSettings( const LOGFONT& font, const COLORREF colour, const bool showRemaining )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "CounterFont", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_blob( stmt, 2, &font, sizeof( LOGFONT ), SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "CounterFontColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( colour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "CounterRemaining", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, showRemaining ? 1 : 0 );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

std::wstring Settings::GetOutputDevice()
{
	std::wstring name;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='OutputDevice';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					name = UTF8ToWideString( text );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return name;
}

void Settings::SetOutputDevice( const std::wstring& name )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OutputDevice", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( name ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::GetDefaultMODSettings( long long& mod, long long& mtm, long long& s3m, long long& xm, long long& it )
{
	long long panning = 35;
	panning <<= 32;
	mod = panning + ( BASS_MUSIC_RAMPS | VUPLAYER_MUSIC_FADEOUT );
	mtm = panning + ( BASS_MUSIC_RAMPS | VUPLAYER_MUSIC_FADEOUT );
	s3m = panning + ( BASS_MUSIC_RAMPS | VUPLAYER_MUSIC_FADEOUT );
	xm = panning + ( BASS_MUSIC_RAMP | VUPLAYER_MUSIC_FADEOUT );
	it = panning + ( BASS_MUSIC_RAMPS | VUPLAYER_MUSIC_FADEOUT );
}

void Settings::GetMODSettings( long long& mod, long long& mtm, long long& s3m, long long& xm, long long& it )
{
	GetDefaultMODSettings( mod, mtm, s3m, xm, it );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='MOD';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				mod = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='MTM';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				mtm = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='S3M';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				s3m = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='XM';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				xm = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='IT';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				it = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetMODSettings( const long long mod, const long long mtm, const long long s3m, const long long xm, const long long it )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "MOD", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int64( stmt, 2, mod );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "MTM", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int64( stmt, 2, mtm );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "S3M", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int64( stmt, 2, s3m );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "XM", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int64( stmt, 2, xm );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "IT", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int64( stmt, 2, it );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetDefaultReplaygainSettings( ReplaygainMode& mode, float& preamp, bool& hardlimit )
{
	mode = ReplaygainMode::Disabled;
	preamp = 6.0f;
	hardlimit = false;
}

void Settings::GetReplaygainSettings( ReplaygainMode& mode, float& preamp, bool& hardlimit )
{
	GetDefaultReplaygainSettings( mode, preamp, hardlimit );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='ReplayGainMode';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value < static_cast<int>( ReplaygainMode::Disabled ) ) || ( value > static_cast<int>( ReplaygainMode::Album ) ) ) {
					mode = ReplaygainMode::Disabled;
				} else {
					mode = static_cast<ReplaygainMode>( value );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ReplayGainPreamp';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				preamp = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ReplayGainHardlimit';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				hardlimit = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetReplaygainSettings( const ReplaygainMode mode, const float preamp, const bool hardlimit )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ReplayGainMode", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( mode ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ReplayGainPreamp", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, preamp );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ReplayGainHardlimit", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, hardlimit );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetSystraySettings( bool& enable, SystrayCommand& singleClick, SystrayCommand& doubleClick )
{
	enable = false;
	singleClick = SystrayCommand::None;
	doubleClick = SystrayCommand::None;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='SysTrayEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				enable = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='SysTraySingleClick';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( SystrayCommand::None ) ) && ( value <= static_cast<int>( SystrayCommand::ShowHide ) ) ) {
					singleClick = static_cast<SystrayCommand>( value );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='SysTrayDoubleClick';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( SystrayCommand::None ) ) && ( value <= static_cast<int>( SystrayCommand::ShowHide ) ) ) {
					doubleClick = static_cast<SystrayCommand>( value );
				}
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetSystraySettings( const bool enable, const SystrayCommand singleClick, const SystrayCommand doubleClick )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "SysTrayEnable", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enable );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "SysTraySingleClick", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( singleClick ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "SysTrayDoubleClick", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( doubleClick ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetPlaybackSettings( bool& randomPlay, bool& repeatTrack, bool& repeatPlaylist, bool& crossfade )
{
	randomPlay = false;
	repeatTrack = false;
	repeatPlaylist = false;
	crossfade = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='RandomPlay';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				randomPlay = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='RepeatTrack';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				repeatTrack = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='RepeatPlaylist';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				repeatPlaylist = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='Crossfade';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				crossfade = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
	if ( randomPlay ) {
		repeatTrack = repeatPlaylist = false;
	} else if ( repeatTrack ) {
		randomPlay = repeatPlaylist = false;
	} else if ( repeatPlaylist ) {
		randomPlay = repeatTrack = false;
	}
}

void Settings::SetPlaybackSettings( const bool randomPlay, const bool repeatTrack, const bool repeatPlaylist, const bool crossfade )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "RandomPlay", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, randomPlay );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "RepeatTrack", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, repeatTrack );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "RepeatPlaylist", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, repeatPlaylist );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "Crossfade", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, crossfade );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetHotkeySettings( bool& enable, HotkeyList& hotkeys )
{
	enable = false;
	hotkeys.clear();
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='EnableHotkeys';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				enable = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
		}

		sqlite3_finalize( stmt );
		stmt = nullptr;
		query = "SELECT * FROM Hotkeys;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				Hotkey hotkey = {};
				const int columnCount = sqlite3_column_count( stmt );
				for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
					const std::string columnName = sqlite3_column_name( stmt, columnIndex );
					if ( columnName == "ID" ) {
						hotkey.ID = sqlite3_column_int( stmt, columnIndex );
					} else if ( columnName == "Hotkey" ) {
						hotkey.Key = sqlite3_column_int( stmt, columnIndex );
					} else if ( columnName == "Alt" ) {
						hotkey.Alt = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					} else if ( columnName == "Ctrl" ) {
						hotkey.Ctrl = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					} else if ( columnName == "Shift" ) {
						hotkey.Shift = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					}
				}
				if ( ( 0 != hotkey.ID ) && ( 0 != hotkey.Key ) ) {
					hotkeys.push_back( hotkey );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetHotkeySettings( const bool enable, const HotkeyList& hotkeys )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "EnableHotkeys", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enable );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		query = "DELETE FROM Hotkeys;";
		sqlite3_exec( database, query.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
		
		if ( !hotkeys.empty() ) {
			stmt = nullptr;
			query = "INSERT INTO Hotkeys (ID,Hotkey,Alt,Ctrl,Shift) VALUES (?1,?2,?3,?4,?5);";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				for ( const auto& hotkey : hotkeys ) {
					sqlite3_bind_int( stmt, 1, hotkey.ID );
					sqlite3_bind_int( stmt, 2, hotkey.Key );
					sqlite3_bind_int( stmt, 3, hotkey.Alt ? 1 : 0 );
					sqlite3_bind_int( stmt, 4, hotkey.Ctrl ? 1 : 0 );
					sqlite3_bind_int( stmt, 5, hotkey.Shift ? 1 : 0 );
					sqlite3_step( stmt );
					sqlite3_reset( stmt );
				}
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
	}
}

Settings::PitchRange Settings::GetPitchRange()
{
	PitchRange range = PitchRange::Small;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='PitchRange';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( PitchRange::Small ) ) && ( value <= static_cast<int>( PitchRange::Large ) ) ) {
					range = static_cast<PitchRange>( value );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return range;
}

void Settings::SetPitchRange( const PitchRange range )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "PitchRange", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( range ) );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

Settings::PitchRangeMap Settings::GetPitchRangeOptions() const
{
	return s_PitchRanges;
}

int Settings::GetOutputControlType()
{
	int type = 0;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='OutputControlType';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				type = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
	}
	return type;
}

void Settings::SetOutputControlType( const int type )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OutputControlType", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, type );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::GetExtractSettings( std::wstring& folder, std::wstring& filename, bool& addToLibrary, bool& joinTracks )
{
	folder.clear();
	filename.clear();
	addToLibrary = true;
	joinTracks = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='ExtractFolder';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					folder = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractFilename';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					filename = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractToLibrary';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				addToLibrary = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractJoin';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				joinTracks = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
	if ( folder.empty() || !FolderExists( folder ) ) {
		PWSTR path = nullptr;
		HRESULT hr = SHGetKnownFolderPath( FOLDERID_Music, KF_FLAG_DEFAULT, NULL /*token*/, &path );
		if ( SUCCEEDED( hr ) ) {
			folder = path;
			CoTaskMemFree( path );
		}
	}
	if ( filename.empty() ) {
		filename = s_DefaultExtractFilename;
	}
}

void Settings::SetExtractSettings( const std::wstring& folder, const std::wstring& filename, const bool addToLibrary, const bool joinTracks )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ExtractFolder", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( folder ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ExtractFilename", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ExtractToLibrary", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, addToLibrary );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ExtractJoin", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, joinTracks );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetGracenoteSettings( std::string& userID, bool& enable, bool& enableLog )
{
	enable = true;
	enableLog = true;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='GracenoteUserID';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					userID = reinterpret_cast<const char*>( text );
				}
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='GracenoteEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				enable = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='GracenoteLog';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				enableLog = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
}

void Settings::SetGracenoteSettings( const std::string& userID, const bool enable, const bool enableLog )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "GracenoteUserID", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, userID.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "GracenoteEnable", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enable );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "GracenoteLog", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enableLog );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

Settings::EQ Settings::GetEQSettings()
{
	EQ eq;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='EQVisible';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.Visible = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQX';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.X = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQY';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.Y = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.Enabled = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQPreamp';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				float preamp = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
				if ( preamp < EQ::MinGain ) {
					preamp = EQ::MinGain;
				} else if ( preamp > EQ::MaxGain ) {
					preamp = EQ::MaxGain;
				}
				eq.Preamp = preamp;
				sqlite3_finalize( stmt );
			}
		}

		for ( auto& gainIter : eq.Gains ) {
			stmt = nullptr;
			query = "SELECT Value FROM Settings WHERE Setting='EQ" + std::to_string( gainIter.first ) + "';";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
					float gain = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
					if ( gain < EQ::MinGain ) {
						gain = EQ::MinGain;
					} else if ( gain > EQ::MaxGain ) {
						gain = EQ::MaxGain;
					}
					gainIter.second = gain;
					sqlite3_finalize( stmt );
				}
			}
		}
	}
	return eq;
}

void Settings::SetEQSettings( const EQ& eq )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "EQVisible", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, eq.Visible );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "EQX", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, eq.X );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "EQY", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, eq.Y );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "EQEnable", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, eq.Enabled );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "EQPreamp", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, eq.Preamp );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			for ( auto& gainIter : eq.Gains ) {
				const std::string setting = "EQ" + std::to_string( gainIter.first );
				const float gain = gainIter.second;
				sqlite3_bind_text( stmt, 1, setting.c_str(), -1 /*strLen*/, SQLITE_STATIC );
				sqlite3_bind_double( stmt, 2, gain );
				sqlite3_step( stmt );
				sqlite3_reset( stmt );
			}

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

std::wstring Settings::GetEncoder()
{
	std::wstring encoderName;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='Encoder';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					encoderName = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
				sqlite3_finalize( stmt );
			}
		}
	}
	return encoderName;
}

void Settings::SetEncoder( const std::wstring& encoder )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "Encoder", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( encoder ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

std::string Settings::GetEncoderSettings( const std::wstring& encoder )
{
	std::string settings;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string settingName = WideStringToUTF8( L"Encoder_" + encoder );
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting=?1;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, settingName.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					settings = reinterpret_cast<const char*>( text );
				}
				sqlite3_finalize( stmt );
			}
		}
	}
	return settings;
}

void Settings::SetEncoderSettings( const std::wstring& encoder, const std::string& settings )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string settingName = WideStringToUTF8( L"Encoder_" + encoder );
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, settingName.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, settings.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

std::wstring Settings::GetSoundFont()
{
	std::wstring soundFont;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='SoundFont';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					soundFont = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
				sqlite3_finalize( stmt );
			}
		}
	}
	return soundFont;
}

void Settings::SetSoundFont( const std::wstring& filename )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "SoundFont", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

bool Settings::GetToolbarEnabled( const int toolbarID )
{
	bool enabled = true;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string idString = "Toolbar" + std::to_string( toolbarID );
		const std::string query = "SELECT Value FROM Settings WHERE Setting='" + idString + "';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				enabled = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				sqlite3_finalize( stmt );
			}
		}
	}
	return enabled;
}

void Settings::SetToolbarEnabled( const int toolbarID, const bool enabled )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string idString = "Toolbar" + std::to_string( toolbarID );
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, idString.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enabled );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

bool Settings::GetMergeDuplicates()
{
	bool mergeDuplicates = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='HideDuplicates';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				mergeDuplicates = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return mergeDuplicates;
}

void Settings::SetMergeDuplicates( const bool mergeDuplicates )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "HideDuplicates", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, mergeDuplicates );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

std::wstring Settings::GetLastFolder( const std::string& folderType )
{
	std::wstring lastFolder;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='Folder" + folderType + "';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					lastFolder = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
				sqlite3_finalize( stmt );
			}
		}
	}
	if ( !lastFolder.empty() ) {
		if ( ( lastFolder.back() == '\\'  ) || ( lastFolder.back() == '/' ) ) {
			lastFolder.pop_back();
		}
		if ( !FolderExists( lastFolder ) ) {
			lastFolder.clear();
		}
	}
	return lastFolder;
}

void Settings::SetLastFolder( const std::string& folderType, const std::wstring& folder )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			const std::string folderSetting = "Folder" + folderType;
			sqlite3_bind_text( stmt, 1, folderSetting.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			std::string folderValue = WideStringToUTF8( folder );
			if ( !folderValue.empty() ) {
				if ( ( folderValue.back() == '\\'  ) || ( folderValue.back() == '/' ) ) {
					folderValue.pop_back();
				}
			}
			sqlite3_bind_text( stmt, 2, folderValue.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}
