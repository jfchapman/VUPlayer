#include "Settings.h"

#include "Utility.h"
#include "VUMeter.h"
#include "VUPlayer.h"

#undef min
#undef max
#undef GetObject

#include "document.h"
#include "stringbuffer.h"
#include "prettywriter.h"

#include <array>

// Pitch ranges
const Settings::PitchRangeMap Settings::s_PitchRanges = {
	{ Settings::PitchRange::Small, 0.1f },
	{ Settings::PitchRange::Medium, 0.2f },
	{ Settings::PitchRange::Large, 0.3f }
};

// Button sizes
const Settings::ButtonSizeMap Settings::s_ButtonSizes = {
	{ Settings::ToolbarSize::Small, 24 },
	{ Settings::ToolbarSize::Medium, 32 },
	{ Settings::ToolbarSize::Large, 40 }
};

// Default conversion/extraction filename format.
static const wchar_t s_DefaultExtractFilename[] = L"%A\\%D\\%N - %T";

Settings::Settings( Database& database, Library& library, const std::string& settings ) :
	m_Database( database ),
	m_Library( library )
{
	UpdateDatabase();
	if ( !settings.empty() ) {
		ImportSettings( settings );
	}
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
	UpdateFontSettings();
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

void Settings::UpdateFontSettings()
{
	// Apply DPI scaling, if necessary, to all logfont blobs in the settings table.
	constexpr std::array allFontSettings { "ListFont", "TreeFont", "CounterFont" };

	// Get the current pixel count per logical inch.
	const HDC dc = GetDC( 0 );
	const int currentLogPixels = ( nullptr != dc ) ? GetDeviceCaps( dc, LOGPIXELSX ) : 0;
	if ( nullptr != dc ) {
		ReleaseDC( 0, dc );
	}
	
	if ( currentLogPixels > 0 ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			int settingsLogPixels = currentLogPixels;

			// Get the pixel count per logical inch which applies to logfont blobs in the settings table.
			sqlite3_stmt* stmt = nullptr;
			std::string query = "SELECT Value FROM Settings WHERE Setting='LogPixels';";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					settingsLogPixels = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				}
				sqlite3_finalize( stmt );
			}

			// Set the pixel count per logical inch which applies to logfont blobs in the settings table.
			query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
			stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				sqlite3_bind_text( stmt, 1, "LogPixels", -1 /*strLen*/, SQLITE_STATIC );
				sqlite3_bind_int( stmt, 2, currentLogPixels );
				sqlite3_step( stmt );
				sqlite3_reset( stmt );
				sqlite3_finalize( stmt );
			}

			if ( ( settingsLogPixels != currentLogPixels ) && ( settingsLogPixels > 0 ) ) {
				const float scaling = static_cast<float>( currentLogPixels ) / settingsLogPixels;

				// Extract logfont blobs from the settings table and apply DPI scaling.
				std::map<std::string,LOGFONT> fontSettingsToUpdate;
				stmt = nullptr;
				query = "SELECT Value FROM Settings WHERE Setting = ?1;";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					for ( const auto& setting : allFontSettings ) {
						sqlite3_bind_text( stmt, 1, setting, -1 /*strLen*/, SQLITE_STATIC );
						if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
							const int bytes = sqlite3_column_bytes( stmt, 0 /*columnIndex*/ );
							if ( sizeof( LOGFONT ) == bytes ) {
								LOGFONT font = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, 0 /*columnIndex*/ ) );
								font.lfHeight = std::lroundf( scaling * font.lfHeight );
								fontSettingsToUpdate.insert( { setting, font } );
							}
						}
						sqlite3_reset( stmt );
					}
					sqlite3_finalize( stmt );
				}

				// Update settings table with DPI scaled logfont blobs.
				stmt = nullptr;
				query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					for ( const auto& setting : fontSettingsToUpdate ) {
						sqlite3_bind_text( stmt, 1, setting.first.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_blob( stmt, 2, &setting.second, sizeof( LOGFONT ), SQLITE_STATIC );
						sqlite3_step( stmt );
						sqlite3_reset( stmt );
					}
					sqlite3_finalize( stmt );
				}
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
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				fontColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListBackgroundColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				backgroundColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListHighlightColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				highlightColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
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
		bool& showFavourites, bool& showStreams, bool& showAllTracks, bool& showArtists, bool& showAlbums, bool& showGenres, bool& showYears )
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
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				fontColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeBackgroundColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				backgroundColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeHighlightColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				highlightColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeFavourites';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showFavourites = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeStreams';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showStreams = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeAllTracks';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showAllTracks = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeArtists';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showArtists = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeAlbums';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showAlbums = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeGenres';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showGenres = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='TreeYears';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showYears = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetTreeSettings( const LOGFONT& font, const COLORREF& fontColour, const COLORREF& backgroundColour, const COLORREF& highlightColour,
		const bool showFavourites, const bool showStreams, const bool showAllTracks, const bool showArtists, const bool showAlbums, const bool showGenres, const bool showYears )
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

			sqlite3_bind_text( stmt, 1, "TreeStreams", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showStreams ) );
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
			if ( SQLITE_OK == sqlite3_prepare_v2( database, removePlaylistQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, playlistID.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) {
					sqlite3_step( stmt );
				}
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
				if ( SQLITE_OK == sqlite3_prepare_v2( database, insertPlaylistQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					if ( ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, playlistID.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) &&
							( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, playlistName.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) ) {
						sqlite3_step( stmt );
					}
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
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		query = "SELECT Value FROM Settings WHERE Setting='SpectrumAnalyserPeak';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				peak = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		query = "SELECT Value FROM Settings WHERE Setting='SpectrumAnalyserBackground';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				background = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
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
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		query = "SELECT Value FROM Settings WHERE Setting='PeakMeterPeak';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				peak = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}

		query = "SELECT Value FROM Settings WHERE Setting='PeakMeterBackground';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				background = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
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
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='CounterFontColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				colour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='CounterRemaining';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showRemaining = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
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

void Settings::GetOutputSettings( std::wstring& deviceName, OutputMode& mode )
{
	deviceName.clear();
	mode = OutputMode::Standard;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='OutputDevice';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
					deviceName = UTF8ToWideString( text );
				}
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='OutputMode';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				mode = static_cast<OutputMode>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetOutputSettings( const std::wstring& deviceName, const OutputMode mode )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "OutputDevice", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, WideStringToUTF8( deviceName ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "OutputMode", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( mode ) );
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
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='MTM';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				mtm = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='S3M';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				s3m = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='XM';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				xm = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='IT';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				it = sqlite3_column_int64( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
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

void Settings::GetDefaultGainSettings( GainMode& gainMode, LimitMode& limitMode, float& preamp )
{
	gainMode = GainMode::Disabled;
	limitMode = LimitMode::Soft;
	preamp = 4.0f;
}

void Settings::GetGainSettings( GainMode& gainMode, LimitMode& limitMode, float& preamp )
{
	GetDefaultGainSettings( gainMode, limitMode, preamp );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='GainMode';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( GainMode::Disabled ) ) && ( value <= static_cast<int>( GainMode::Album ) ) ) {
					gainMode = static_cast<GainMode>( value );
				}
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='GainPreamp';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				preamp = static_cast<float>( sqlite3_column_double( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='GainLimit';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( LimitMode::None ) ) && ( value <= static_cast<int>( LimitMode::Soft ) ) ) {
					limitMode = static_cast<LimitMode>( value );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetGainSettings( const GainMode gainMode, const LimitMode limitMode, const float preamp )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "GainMode", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( gainMode ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "GainPreamp", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_double( stmt, 2, preamp );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "GainLimit", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( limitMode ) );
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
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='SysTraySingleClick';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( SystrayCommand::None ) ) && ( value <= static_cast<int>( SystrayCommand::ShowHide ) ) ) {
					singleClick = static_cast<SystrayCommand>( value );
				}
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='SysTrayDoubleClick';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( SystrayCommand::None ) ) && ( value <= static_cast<int>( SystrayCommand::ShowHide ) ) ) {
					doubleClick = static_cast<SystrayCommand>( value );
				}
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='RepeatTrack';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				repeatTrack = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='RepeatPlaylist';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				repeatPlaylist = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='Crossfade';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				crossfade = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
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
			sqlite3_finalize( stmt );
		}

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
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractFilename';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					filename = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
				}
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractToLibrary';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				addToLibrary = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ExtractJoin';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				joinTracks = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQX';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.X = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQY';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.Y = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='EQEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				eq.Enabled = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
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
				}
				sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
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
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, settingName.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) {
				if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
					const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
					if ( nullptr != text ) {
						settings = reinterpret_cast<const char*>( text );
					}
				}
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
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
			}
			sqlite3_finalize( stmt );
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

bool Settings::GetScrobblerEnabled()
{
	bool enabled = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='ScrobblerEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				enabled = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return enabled;
}

void Settings::SetScrobblerEnabled( const bool enabled )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ScrobblerEnable", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, enabled );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

std::string Settings::GetScrobblerKey()
{
	std::string key;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='ScrobblerKey';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					key = reinterpret_cast<const char*>( text );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	std::string decryptedKey;
	if ( !key.empty() ) {
		// Decrypt key from storage.
		std::vector<BYTE> bytes = Base64Decode( key );
		DATA_BLOB dataIn = { static_cast<DWORD>( bytes.size() ), &bytes[ 0 ] };
		DATA_BLOB dataOut = {};
		if ( CryptUnprotectData( &dataIn, nullptr /*dataDesc*/, nullptr /*entropy*/, nullptr /*reserved*/, nullptr /*prompt*/, 0 /*flags*/, &dataOut ) ) {
			decryptedKey = std::string( reinterpret_cast<char*>( dataOut.pbData ), static_cast<size_t>( dataOut.cbData ) );
			LocalFree( dataOut.pbData );
		}
	}
	return decryptedKey;
}

void Settings::SetScrobblerKey( const std::string& key )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		std::string encryptedKey;
		if ( !key.empty() ) {
			// Encrypt key for storage.
			DATA_BLOB dataIn = { static_cast<DWORD>( key.size() ), const_cast<BYTE*>( reinterpret_cast<const BYTE*>( key.c_str() ) ) };
			DATA_BLOB dataOut = {};
			if ( CryptProtectData( &dataIn, nullptr /*dataDesc*/, nullptr /*entropy*/, nullptr /*reserved*/, nullptr /*prompt*/, 0 /*flags*/, &dataOut ) ) {
				encryptedKey = Base64Encode( dataOut.pbData, dataOut.cbData );
				LocalFree( dataOut.pbData );
			}
		}
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ScrobblerKey", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_text( stmt, 2, encryptedKey.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::ExportSettings( std::string& output )
{
	output.clear();
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		rapidjson::Document document;
		document.SetObject();
		auto& allocator = document.GetAllocator();

		// Settings table.
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Setting, Value FROM Settings ORDER BY Setting ASC;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			rapidjson::Value settingsObject;
			settingsObject.SetObject();
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const unsigned char* text = sqlite3_column_text( stmt, 0 /*columnIndex*/ );
				if ( nullptr != text ) {
					const std::string setting = reinterpret_cast<const char*>( text );
					rapidjson::Value value;
					const int type = sqlite3_column_type( stmt, 1 /*columnIndex*/ );
					switch ( type ) {
						case SQLITE_INTEGER : {
							value.SetInt64( sqlite3_column_int64( stmt, 1 /*columnIndex*/ ) );
							break;
						}
						case SQLITE_FLOAT : {
							value.SetDouble( sqlite3_column_double( stmt, 1 /*columnIndex*/ ) );
							break;
						}
						case SQLITE_TEXT : {
							const std::string str = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 1 /*columnIndex*/ ) );
							if ( str.empty() ) {
								value.SetNull();
							} else {
								value.SetString( str.c_str(), static_cast<rapidjson::SizeType>( str.size() ), allocator );
							}
							break;
						}
						case SQLITE_BLOB : {
							// Note that blob fields should only contain LOGFONT structures.
							const int bytes = sqlite3_column_bytes( stmt, 1 /*columnIndex*/ );
							if ( sizeof( LOGFONT ) == bytes ) {
								LOGFONT logfont = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, 1 /*columnIndex*/ ) );
								logfont.lfFaceName[ LF_FACESIZE - 1 ] = 0;
								const std::string facename = WideStringToUTF8( logfont.lfFaceName );
								if ( facename.empty() ) {
									value.SetNull();
								} else {
									value.SetObject();
									const std::map<std::string,int> fields = {
										{ "lfHeight", logfont.lfHeight },
										{ "lfWidth", logfont.lfWidth },
										{ "lfEscapement", logfont.lfEscapement },
										{ "lfOrientation", logfont.lfOrientation },
										{ "lfWeight", logfont.lfWeight },
										{ "lfItalic", logfont.lfItalic },
										{ "lfUnderline", logfont.lfUnderline },
										{ "lfStrikeOut", logfont.lfStrikeOut },
										{ "lfCharSet", logfont.lfCharSet },
										{ "lfOutPrecision", logfont.lfOutPrecision },
										{ "lfClipPrecision", logfont.lfClipPrecision },
										{ "lfQuality", logfont.lfQuality },
										{ "lfPitchAndFamily", logfont.lfPitchAndFamily }
									};
									rapidjson::Value v;
									for ( const auto& field : fields ) {
										rapidjson::Value key( field.first.c_str(), static_cast<rapidjson::SizeType>( field.first.size() ), allocator );
										v.SetInt( field.second );
										value.AddMember( key, v, allocator );
									}
									v = rapidjson::Value( facename.c_str(), static_cast<rapidjson::SizeType>( facename.size() ), allocator );
									value.AddMember( "lfFaceName", v, allocator );
								}
							}
							break;
						}
						default : {
							value.SetNull();
							break;
						}
					}
					if ( !value.IsNull() ) {
						rapidjson::Value key( setting.c_str(), static_cast<rapidjson::SizeType>( setting.size() ), allocator );
						settingsObject.AddMember( key, value, allocator );
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
			if ( !settingsObject.ObjectEmpty() ) {
				document.AddMember( "Settings", settingsObject, allocator );
			}
		}

		// PlaylistColumns table.
		query = "SELECT Col,Width FROM PlaylistColumns ORDER BY rowid ASC;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			rapidjson::Value columnsArray;
			columnsArray.SetArray();
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int col = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				const int width = sqlite3_column_int( stmt, 1 /*columnIndex*/ );
				if ( ( col > 0 ) && ( width > 0 ) ) {
					rapidjson::Value columnObject;
					columnObject.SetObject();
					rapidjson::Value value;
					value.SetInt( col );
					columnObject.AddMember( "Col", value, allocator );
					value.SetInt( width );
					columnObject.AddMember( "Width", value, allocator );
					columnsArray.PushBack( columnObject, allocator );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
			if ( columnsArray.Size() > 0 ) {
				document.AddMember( "PlaylistColumns", columnsArray, allocator );
			}
		}

		// Hotkeys table.
		query = "SELECT ID,Hotkey,Alt,Ctrl,Shift FROM Hotkeys;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			rapidjson::Value hotkeysArray;
			hotkeysArray.SetArray();
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int id = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				const int hotkey = sqlite3_column_int( stmt, 1 /*columnIndex*/ );
				const bool alt = ( 0 != sqlite3_column_int( stmt, 2 /*columnIndex*/ ) );
				const bool ctrl = ( 0 != sqlite3_column_int( stmt, 3 /*columnIndex*/ ) );
				const bool shift = ( 0 != sqlite3_column_int( stmt, 4 /*columnIndex*/ ) );
				if ( ( id > 0 ) && ( hotkey > 0 ) && ( alt || ctrl || shift ) ) {
					rapidjson::Value hotkeyObject;
					hotkeyObject.SetObject();
					rapidjson::Value value;
					value.SetInt( id );
					hotkeyObject.AddMember( "ID", value, allocator );
					value.SetInt( hotkey );
					hotkeyObject.AddMember( "Hotkey", value, allocator );
					value.SetBool( alt );
					hotkeyObject.AddMember( "Alt", value, allocator );
					value.SetBool( ctrl );
					hotkeyObject.AddMember( "Ctrl", value, allocator );
					value.SetBool( shift );
					hotkeyObject.AddMember( "Shift", value, allocator );
					hotkeysArray.PushBack( hotkeyObject, allocator );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
			if ( hotkeysArray.Size() > 0 ) {
				document.AddMember( "Hotkeys", hotkeysArray, allocator );
			}
		}

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( buffer );
		document.Accept( writer );
		output = buffer.GetString();
	}
}

void Settings::ImportSettings( const std::string& input )
{
	rapidjson::Document document;
	document.Parse( input.c_str() );
	if ( !document.HasParseError() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			// Settings object.
			const auto settingsIter = document.FindMember( "Settings" );
			if ( ( document.MemberEnd() != settingsIter ) && settingsIter->value.IsObject() ) {
				const auto settingsObject = settingsIter->value.GetObject();
				sqlite3_stmt* stmt = nullptr;
				const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					for ( const auto& member : settingsObject ) {
						const std::string name = member.name.GetString();
						if ( member.value.IsDouble() ) {
							sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC );
							sqlite3_bind_double( stmt, 2, member.value.GetDouble() );
							sqlite3_step( stmt );
							sqlite3_reset( stmt );
						} else if ( member.value.IsInt64() ) {
							sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC );
							sqlite3_bind_int64( stmt, 2, member.value.GetInt64() );
							sqlite3_step( stmt );
							sqlite3_reset( stmt );
						} else if ( member.value.IsString() ) {
							sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC );
							sqlite3_bind_text( stmt, 2, member.value.GetString(), -1 /*strLen*/, SQLITE_STATIC );
							sqlite3_step( stmt );
							sqlite3_reset( stmt );
						} else if ( member.value.IsObject() ) {
							// Note that we should only have sub-objects which represent LOGFONT structures.
							const auto fontObject = member.value.GetObject();
							if ( fontObject.HasMember( "lfFaceName" ) ) {
								const std::wstring fontname = UTF8ToWideString( fontObject[ "lfFaceName" ].GetString() );
								LOGFONT logfont = {};
								wcscpy_s( logfont.lfFaceName, LF_FACESIZE - 1, fontname.c_str() );
								const std::map<std::string,LONG*> longfields = {
									{ "lfHeight", &logfont.lfHeight },
									{ "lfWidth", &logfont.lfWidth },
									{ "lfEscapement", &logfont.lfEscapement },
									{ "lfOrientation", &logfont.lfOrientation },
									{ "lfWeight", &logfont.lfWeight }
								};
								const std::map<std::string,BYTE*> bytefields = {
									{ "lfItalic", &logfont.lfItalic },
									{ "lfUnderline", &logfont.lfUnderline },
									{ "lfStrikeOut", &logfont.lfStrikeOut },
									{ "lfCharSet", &logfont.lfCharSet },
									{ "lfOutPrecision", &logfont.lfOutPrecision },
									{ "lfClipPrecision", &logfont.lfClipPrecision },
									{ "lfQuality", &logfont.lfQuality },
									{ "lfPitchAndFamily", &logfont.lfPitchAndFamily }
								};
								for ( const auto& fontMember : fontObject ) {
									if ( fontMember.value.IsNumber() ) {
										const auto longfield = longfields.find( fontMember.name.GetString() );
										if ( longfields.end() != longfield ) {
											*longfield->second = static_cast<LONG>( fontMember.value.GetInt() );
										} else {
											const auto bytefield = bytefields.find( fontMember.name.GetString() );
											if ( bytefields.end() != bytefield ) {
												*bytefield->second = static_cast<BYTE>( fontMember.value.GetInt() );
											}
										}
									}
								}
								sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC );
								sqlite3_bind_blob( stmt, 2, &logfont, sizeof( LOGFONT ), SQLITE_STATIC );
								sqlite3_step( stmt );
								sqlite3_reset( stmt );
							}
						}
					}
					sqlite3_finalize( stmt );
					stmt = nullptr;
				}
			}

			// PlaylistColumns array.
			const auto playlistColumnsIter = document.FindMember( "PlaylistColumns" );
			if ( ( document.MemberEnd() != playlistColumnsIter ) && playlistColumnsIter->value.IsArray() ) {
				const auto columnsArray = playlistColumnsIter->value.GetArray();
				sqlite3_stmt* stmt = nullptr;
				std::string query = "DELETE FROM PlaylistColumns;";
				sqlite3_exec( database, query.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				query = "INSERT INTO PlaylistColumns (Col,Width) VALUES (?1,?2);";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					for ( const auto& columnObject : columnsArray ) {
						if ( columnObject.IsObject() ) {
							const auto colIter = columnObject.FindMember( "Col" );
							const auto widthIter = columnObject.FindMember( "Width" );
							if ( ( columnObject.MemberEnd() != colIter ) && ( columnObject.MemberEnd() != widthIter ) ) {
								const int col = colIter->value.GetInt();
								const int width = widthIter->value.GetInt();
								if ( ( col > 0 ) && ( width > 0 ) ) {
									sqlite3_bind_int( stmt, 1, col );
									sqlite3_bind_int( stmt, 2, width );
									sqlite3_step( stmt );
									sqlite3_reset( stmt );
								}
							}
						}
					}
					sqlite3_finalize( stmt );
					stmt = nullptr;
				}
			}

			// Hotkeys array.
			const auto hotkeysIter = document.FindMember( "Hotkeys" );
			if ( ( document.MemberEnd() != hotkeysIter ) && hotkeysIter->value.IsArray() ) {
				sqlite3_stmt* stmt = nullptr;
				std::string query = "DELETE FROM Hotkeys;";
				sqlite3_exec( database, query.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				query = "INSERT INTO Hotkeys (ID,Hotkey,Alt,Ctrl,Shift) VALUES (?1,?2,?3,?4,?5);";
				if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
					const auto hotkeysArray = hotkeysIter->value.GetArray();
					for ( const auto& hotkeyObject : hotkeysArray ) {
						if ( hotkeyObject.IsObject() ) {
							const auto idIter = hotkeyObject.FindMember( "ID" );
							const auto hotkeyIter = hotkeyObject.FindMember( "Hotkey" );
							const auto altIter = hotkeyObject.FindMember( "Alt" );
							const auto ctrlIter = hotkeyObject.FindMember( "Ctrl" );
							const auto shiftIter = hotkeyObject.FindMember( "Shift" );
							if ( ( hotkeyObject.MemberEnd() != idIter ) && ( hotkeyObject.MemberEnd() != hotkeyIter ) &&
									( hotkeyObject.MemberEnd() != altIter ) && ( hotkeyObject.MemberEnd() != ctrlIter ) && ( hotkeyObject.MemberEnd() != shiftIter ) ) {
								const int id = idIter->value.GetInt();
								const int hotkey = hotkeyIter->value.GetInt();
								const int alt = altIter->value.IsTrue() ? 1 : 0;
								const int ctrl = ctrlIter->value.IsTrue() ? 1 : 0;
								const int shift = shiftIter->value.IsTrue() ? 1 : 0;
								if ( ( id > 0 ) && ( hotkey > 0 ) && ( ( alt > 0 ) || ( ctrl > 0 ) || ( shift > 0 ) ) ) {
									sqlite3_bind_int( stmt, 1, id );
									sqlite3_bind_int( stmt, 2, hotkey );
									sqlite3_bind_int( stmt, 3, alt );
									sqlite3_bind_int( stmt, 4, ctrl );
									sqlite3_bind_int( stmt, 5, shift );
									sqlite3_step( stmt );
									sqlite3_reset( stmt );
								}
							}
						}
					}
					sqlite3_finalize( stmt );
					stmt = nullptr;
				}
			}
		}
	}
}

void Settings::GetDefaultAdvancedWasapiExclusiveSettings( bool& useDeviceDefaultFormat, int& bufferLength, int& leadIn, int& maxBufferLength, int& maxLeadIn )
{
	useDeviceDefaultFormat = false;
	bufferLength = 100;
	leadIn = 0;
	maxBufferLength = 500;
	maxLeadIn = 2000;
}

void Settings::GetAdvancedWasapiExclusiveSettings( bool& useDeviceDefaultFormat, int& bufferLength, int& leadIn )
{
	int maxBufferLength = 0;
	int maxLeadIn = 0;
	GetDefaultAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn, maxBufferLength, maxLeadIn );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='WasapiExclusiveUseDeviceFormat';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				useDeviceDefaultFormat = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='WasapiExclusiveBufferLength';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				bufferLength = std::clamp( sqlite3_column_int( stmt, 0 /*columnIndex*/ ), 0, maxBufferLength );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='WasapiExclusiveLeadIn';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				leadIn = std::clamp( sqlite3_column_int( stmt, 0 /*columnIndex*/ ), 0, maxLeadIn );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetAdvancedWasapiExclusiveSettings( const bool useDeviceDefaultFormat, const int bufferLength, const int leadIn )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "WasapiExclusiveUseDeviceFormat", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, useDeviceDefaultFormat );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "WasapiExclusiveBufferLength", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, bufferLength );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "WasapiExclusiveLeadIn", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, leadIn );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetDefaultAdvancedASIOSettings( bool& useDefaultSamplerate, int& defaultSamplerate, int& leadIn, int& maxDefaultSamplerate, int& maxLeadIn )
{
	useDefaultSamplerate = false;
	defaultSamplerate = 48000;
	leadIn = 0;
	maxDefaultSamplerate = 192000;
	maxLeadIn = 2000;
}

void Settings::GetAdvancedASIOSettings( bool& useDefaultSamplerate, int& defaultSamplerate, int& leadIn )
{
	int maxDefaultSamplerate = 0;
	int maxLeadIn = 0;
	GetDefaultAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadIn, maxDefaultSamplerate, maxLeadIn );
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		std::string query = "SELECT Value FROM Settings WHERE Setting='ASIOUseDefaultSamplerate';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				useDefaultSamplerate = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ASIODefaultSamplerate';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				defaultSamplerate = std::clamp( sqlite3_column_int( stmt, 0 /*columnIndex*/ ), 0, maxDefaultSamplerate );
			}
			sqlite3_finalize( stmt );
		}
		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ASIOLeadIn';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				leadIn = std::clamp( sqlite3_column_int( stmt, 0 /*columnIndex*/ ), 0, maxLeadIn );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetAdvancedASIOSettings( const bool useDefaultSamplerate, const int defaultSamplerate, const int leadIn )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ASIOUseDefaultSamplerate", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, useDefaultSamplerate );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ASIODefaultSamplerate", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, defaultSamplerate );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ASIOLeadIn", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, leadIn );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

Settings::ToolbarSize Settings::GetToolbarSize()
{
	ToolbarSize size = ToolbarSize::Small;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting='ToolbarSize';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int value = sqlite3_column_int( stmt, 0 /*columnIndex*/ );
				if ( ( value >= static_cast<int>( ToolbarSize::Small ) ) && ( value <= static_cast<int>( ToolbarSize::Large ) ) ) {
					size = static_cast<ToolbarSize>( value );
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return size;
}

void Settings::SetToolbarSize( const ToolbarSize size )
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, "ToolbarSize", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( size ) );
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

int Settings::GetToolbarButtonSize( const ToolbarSize size )
{
	auto iter = s_ButtonSizes.find( size );
	if ( s_ButtonSizes.end() == iter ) {
		iter = s_ButtonSizes.begin();
	}
	const int buttonSize = static_cast<int>( iter->second * GetDPIScaling() );
	return buttonSize;
}
