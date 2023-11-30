#include "Settings.h"

#include "Utility.h"
#include "VUMeter.h"
#include "VUPlayer.h"

#include <array>
#include <type_traits>

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

template <typename T>
std::optional<T> Settings::ReadSetting( const std::string& name )
{
	std::optional<T> value;
	if ( sqlite3* database = m_Database.GetDatabase(); nullptr != database ) {
		sqlite3_stmt* stmt = nullptr;
		const std::string query = "SELECT Value FROM Settings WHERE Setting=?1;";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC ) ) {
				if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					constexpr int kColumnIndex = 0;
					if constexpr ( std::is_floating_point_v<T> ) {
						value = static_cast<T>( sqlite3_column_double( stmt, kColumnIndex ) );
					} else if constexpr ( std::is_integral_v<T> ) {
						value = static_cast<T>( sqlite3_column_int64( stmt, kColumnIndex ) );
					} else if constexpr ( std::is_enum_v<T> ) {
						value = static_cast<T>( sqlite3_column_int( stmt, kColumnIndex ) );
					} else if constexpr ( std::is_same_v<T, std::string> ) {
						if ( const unsigned char* text = sqlite3_column_text( stmt, kColumnIndex ); nullptr != text ) {
							value = reinterpret_cast<const char*>( text );
						}
					} else if constexpr ( std::is_same_v<T, std::wstring> ) {
						if ( const unsigned char* text = sqlite3_column_text( stmt, kColumnIndex ); nullptr != text ) {
							value = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
						}
					} else if constexpr ( std::is_same_v<T, LOGFONT> ) {
				    if ( const int bytes = sqlite3_column_bytes( stmt, kColumnIndex ); sizeof( LOGFONT ) == bytes ) {
              value = *reinterpret_cast<const LOGFONT*>( sqlite3_column_blob( stmt, kColumnIndex ) );
            }
          } else {
						static_assert( !sizeof( T ), "Settings::ReadSetting - unsupported type" );
					}
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return value;
}

template <typename T>
void Settings::WriteSetting( const std::string& name, const T& value )
{
	if ( sqlite3* database = m_Database.GetDatabase(); nullptr != database ) {
		const std::string query = "REPLACE INTO Settings (Setting,Value) VALUES (?1,?2);";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			sqlite3_bind_text( stmt, 1, name.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			if constexpr ( std::is_floating_point_v<T> ) {
				sqlite3_bind_double( stmt, 2, value );
			} else if constexpr ( std::is_integral_v<T> ) {
				sqlite3_bind_int64( stmt, 2, value );
			} else if constexpr ( std::is_enum_v<T> ) {
				sqlite3_bind_int( stmt, 2, static_cast<int>( value ) );
			} else if constexpr ( std::is_same_v<T, std::string> ) {
				sqlite3_bind_text( stmt, 2, value.c_str(), -1 /*strLen*/, SQLITE_STATIC );
			} else if constexpr ( std::is_same_v<T, std::wstring> ) {
				sqlite3_bind_text( stmt, 2, WideStringToUTF8( value ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
			} else if constexpr (std::is_same_v<T, LOGFONT> ) {
  			sqlite3_bind_blob( stmt, 2, &value, sizeof( LOGFONT ), SQLITE_STATIC );
      } else {
				static_assert( !sizeof( T ), "Settings::WriteSetting - unsupported type" );
			}
			sqlite3_step( stmt );
			sqlite3_finalize( stmt );
		}
	}
}

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
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
              const std::string name = reinterpret_cast<const char*>( text );
						  columns.insert( name );
            }
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
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
						  const std::string name = reinterpret_cast<const char*>( text );
						  columns.insert( name );
            }
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
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
						  const std::string name = reinterpret_cast<const char*>( text );
						  columns.insert( name );
            }
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
		createTableQuery += "\"(File,Pending,CueStart,CueEnd);";
		sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the playlists table.
    std::set<std::string> missingColumns = { "File", "Pending", "CueStart", "CueEnd" };
    std::string columnsInfoQuery = "PRAGMA table_info('";
		columnsInfoQuery += table + "')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, columnsInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int columnCount = sqlite3_column_count( stmt );
				for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
					const std::string columnName = sqlite3_column_name( stmt, columnIndex );
					if ( columnName == "name" ) {
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
						  const std::string name = reinterpret_cast<const char*>( text );
						  missingColumns.erase( name );
            }
						break;
					}
				}
			}
			sqlite3_finalize( stmt );

			for ( auto column = missingColumns.rbegin(); missingColumns.rend() != column; column++ ) {
		    std::string addColumnQuery = "ALTER TABLE \"" + table + "\" ADD COLUMN ";
			  addColumnQuery += *column + ";";
				sqlite3_exec( database, addColumnQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Settings::UpdateHotkeysTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the hotkeys table (if necessary).
		const std::string hotkeyTableQuery = "CREATE TABLE IF NOT EXISTS Hotkeys(ID,Hotkey,Alt,Ctrl,Shift,Keyname);";
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
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
						  const std::string name = reinterpret_cast<const char*>( text );
						  columns.insert( name );
            }
						break;
					}
				}
			}
			sqlite3_finalize( stmt );

			if ( ( columns.find( "ID" ) == columns.end() ) || ( columns.find( "Hotkey" ) == columns.end() ) ||
					( columns.find( "Alt" ) == columns.end() ) || ( columns.find( "Ctrl" ) == columns.end() ) ||
					( columns.find( "Shift" ) == columns.end() ) ) {
				// Drop the table and recreate.
				const std::string dropTableQuery = "DROP TABLE Hotkeys;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, hotkeyTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			} else if ( columns.find( "Keyname" ) == columns.end() ) {
				// The 'Keyname' column was added in a later version, so update the old table.
				const std::string addColumnQuery = "ALTER TABLE Hotkeys ADD COLUMN Keyname;";
				sqlite3_exec( database, addColumnQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
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

void Settings::GetPlaylistSettings( PlaylistColumns& columns, bool& showStatusIcon, LOGFONT& font,
			COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour, COLORREF& statusIconColour )
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

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListStatusIconColour';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				statusIconColour = static_cast<COLORREF>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}

		stmt = nullptr;
		query = "SELECT Value FROM Settings WHERE Setting='ListStatusIconEnable';";
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_ROW == sqlite3_step( stmt ) ) && ( 1 == sqlite3_column_count( stmt ) ) ) {
				showStatusIcon = ( 0 != sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
			}
			sqlite3_finalize( stmt );
		}
	}
}

void Settings::SetPlaylistSettings( const PlaylistColumns& columns, const bool showStatusIcon, const LOGFONT& font,
			const COLORREF fontColour, const COLORREF backgroundColour, const COLORREF highlightColour, const COLORREF statusIconColour )
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

			sqlite3_bind_text( stmt, 1, "ListStatusIconColour", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( statusIconColour ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_bind_text( stmt, 1, "ListStatusIconEnable", -1 /*strLen*/, SQLITE_STATIC );
			sqlite3_bind_int( stmt, 2, static_cast<int>( showStatusIcon ) );
			sqlite3_step( stmt );
			sqlite3_reset( stmt );

			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
}

void Settings::GetTreeSettings( LOGFONT& font, COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour, COLORREF& iconColour,
  bool& showFavourites, bool& showStreams, bool& showAllTracks, bool& showArtists, bool& showAlbums, bool& showGenres, bool& showYears,
  bool& showPublishers, bool& showComposers, bool& showConductors )
{
	if ( const auto value = ReadSetting<LOGFONT>( "TreeFont" ); value ) {
		font = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "TreeFontColour" ); value ) {
		fontColour = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "TreeBackgroundColour" ); value ) {
		backgroundColour = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "TreeHighlightColour" ); value ) {
		highlightColour = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "TreeIconColour" ); value ) {
		iconColour = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeFavourites" ); value ) {
		showFavourites = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeStreams" ); value ) {
		showStreams = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeAllTracks" ); value ) {
		showAllTracks = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeArtists" ); value ) {
		showArtists = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeAlbums" ); value ) {
		showAlbums = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeGenres" ); value ) {
		showGenres = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeYears" ); value ) {
		showYears = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreePublishers" ); value ) {
		showPublishers = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeComposers" ); value ) {
		showComposers = *value;
	}
	if ( const auto value = ReadSetting<bool>( "TreeConductors" ); value ) {
		showConductors = *value;
	}
}

void Settings::SetTreeSettings( const LOGFONT& font, const COLORREF fontColour, const COLORREF backgroundColour, const COLORREF highlightColour, const COLORREF iconColour,
  const bool showFavourites, const bool showStreams, const bool showAllTracks, const bool showArtists, const bool showAlbums, const bool showGenres, const bool showYears,
  const bool showPublishers, const bool showComposers, const bool showConductors )
{
  WriteSetting( "TreeFont", font );
  WriteSetting( "TreeFontColour", fontColour );
  WriteSetting( "TreeBackgroundColour", backgroundColour );
  WriteSetting( "TreeHighlightColour", highlightColour );
  WriteSetting( "TreeIconColour", iconColour );
  WriteSetting( "TreeFavourites", showFavourites );
  WriteSetting( "TreeStreams", showStreams );
  WriteSetting( "TreeAllTracks", showAllTracks );
  WriteSetting( "TreeArtists", showArtists );
  WriteSetting( "TreeAlbums", showAlbums );
  WriteSetting( "TreeGenres", showGenres );
  WriteSetting( "TreeYears", showYears );
  WriteSetting( "TreePublishers", showPublishers );
  WriteSetting( "TreeComposers", showComposers );
  WriteSetting( "TreeConductors", showConductors );
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
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
							playlistID = reinterpret_cast<const char*>( text );
						}
					} else if ( columnName == "Name" ) {
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
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
					MediaInfo mediaInfo;
					const int columnCount = sqlite3_column_count( stmt );
					for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
						const std::string columnName = sqlite3_column_name( stmt, columnIndex );
						if ( columnName == "File" ) {
							if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
								mediaInfo.SetFilename( UTF8ToWideString( reinterpret_cast<const char*>( text ) ) );
							}
						} else if ( columnName == "Pending" ) {
							pending = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
						} else if ( columnName == "CueStart" ) {
              if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
                mediaInfo.SetCueStart( static_cast<long>( sqlite3_column_int64( stmt, columnIndex ) ) );
              }              
						} else if ( columnName == "CueEnd" ) {
              if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
                mediaInfo.SetCueEnd( static_cast<long>( sqlite3_column_int64( stmt, columnIndex ) ) );
              }              
            }
					}
					if ( !mediaInfo.GetFilename().empty() ) {
						if ( pending ) {
							playlist.AddPending( mediaInfo, false /*startPendingThread*/ );
						} else {
							if ( m_Library.GetMediaInfo( mediaInfo, false /*scanMedia*/ ) ) {
								playlist.AddItem( mediaInfo );
              } else if ( mediaInfo.GetCueStart() ) {
                m_Library.GetDecoderInfo( mediaInfo, false /*getTags*/ );
								playlist.AddItem( mediaInfo );
							} else {
								playlist.AddPending( mediaInfo, false /*startPendingThread*/ );
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
	std::vector<unsigned char> rpcStr( 1 + guid.size(), 0 );
	std::copy( guid.begin(), guid.end(), rpcStr.begin() );
	UUID uuid = {};
	const bool valid = ( RPC_S_OK == UuidFromStringA( rpcStr.data(), &uuid ) );
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
			insertFileQuery += "\" (File,Pending,CueStart,CueEnd) VALUES (?1,?2,?3,?4);";
			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, insertFileQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				bool pending = false;
				const Playlist::Items itemList = playlist.GetItems();
				for ( const auto& iter : itemList ) {
					const std::string filename = WideStringToUTF8( iter.Info.GetFilename() );
					if ( !filename.empty() ) {
						sqlite3_bind_text( stmt, 1 /*param*/, filename.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( pending ) );
            if ( iter.Info.GetCueStart() ) {
              sqlite3_bind_int64( stmt, 3 /*param*/, *iter.Info.GetCueStart() );
            } else {
              sqlite3_bind_null( stmt, 3 /*param*/ );
            }
            if ( iter.Info.GetCueEnd() ) {
              sqlite3_bind_int64( stmt, 4 /*param*/, *iter.Info.GetCueEnd() );
            } else {
              sqlite3_bind_null( stmt, 4 /*param*/ );
            }
						sqlite3_step( stmt );
						sqlite3_reset( stmt );
					}
				}
				pending = true;
				const std::list<MediaInfo> pendingList = playlist.GetPending();
				for ( const auto& iter : pendingList ) {
					const std::string filename = WideStringToUTF8( iter.GetFilename() );
					if ( !filename.empty() ) {
						sqlite3_bind_text( stmt, 1 /*param*/, filename.c_str(), -1 /*strLen*/, SQLITE_STATIC );
						sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( pending ) );
            if ( iter.GetCueStart() ) {
              sqlite3_bind_int64( stmt, 3 /*param*/, *iter.GetCueStart() );
            } else {
              sqlite3_bind_null( stmt, 3 /*param*/ );
            }
            if ( iter.GetCueEnd() ) {
              sqlite3_bind_int64( stmt, 4 /*param*/, *iter.GetCueEnd() );
            } else {
              sqlite3_bind_null( stmt, 4 /*param*/ );
            }
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

std::filesystem::path Settings::GetDefaultArtwork()
{
	return ReadSetting<std::wstring>( "DefaultArtwork" ).value_or( std::wstring() );
}

void Settings::SetDefaultArtwork( const std::filesystem::path& artwork )
{
  WriteSetting( "DefaultArtwork", artwork.native() );
}

COLORREF Settings::GetOscilloscopeColour()
{
  constexpr COLORREF kDefaultBackground = RGB( 0, 0, 0 );

  return ReadSetting<COLORREF>( "OscilloscopeColour" ).value_or( kDefaultBackground );
}

void Settings::SetOscilloscopeColour( const COLORREF colour )
{
  WriteSetting( "OscilloscopeColour", colour );
}

COLORREF Settings::GetOscilloscopeBackground()
{
  constexpr COLORREF kDefaultBackground = RGB( 255, 255, 255 );

  return ReadSetting<COLORREF>( "OscilloscopeBackground" ).value_or( kDefaultBackground );
}

void Settings::SetOscilloscopeBackground( const COLORREF colour )
{
  WriteSetting( "OscilloscopeBackground", colour );
}

float Settings::GetOscilloscopeWeight()
{
  constexpr float kDefaultWeight = 2.0f;
  constexpr float kMinWeight = 0.5f;
  constexpr float kMaxWeight = 5.0f;

  return std::clamp( ReadSetting<float>( "OscilloscopeWeight" ).value_or( kDefaultWeight ), kMinWeight, kMaxWeight );
}

void Settings::SetOscilloscopeWeight( const float weight )
{
  WriteSetting( "OscilloscopeWeight", weight );
}

float Settings::GetVUMeterDecay()
{
  return std::clamp( ReadSetting<float>( "VUMeterDecay" ).value_or( VUMeterDecayNormal ), VUMeterDecayMinimum, VUMeterDecayMaximum );
}

void Settings::SetVUMeterDecay( const float decay )
{
  WriteSetting( "VUMeterDecay", decay );
}

void Settings::GetSpectrumAnalyserSettings( COLORREF& base, COLORREF& peak, COLORREF& background )
{
	constexpr COLORREF kBase = RGB( 0 /*red*/, 122 /*green*/, 217 /*blue*/ );
	constexpr COLORREF kPeak = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	constexpr COLORREF kBackground = RGB( 0x00 /*red*/, 0x00 /*green*/, 0x00 /*blue*/ );

  base = ReadSetting<COLORREF>( "SpectrumAnalyserBase" ).value_or( kBase );
  peak = ReadSetting<COLORREF>( "SpectrumAnalyserPeak" ).value_or( kPeak );
  background = ReadSetting<COLORREF>( "SpectrumAnalyserBackground" ).value_or( kBackground );
}

void Settings::SetSpectrumAnalyserSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background )
{
  WriteSetting( "SpectrumAnalyserBase", base );
  WriteSetting( "SpectrumAnalyserPeak", peak );
  WriteSetting( "SpectrumAnalyserBackground", background );
}

void Settings::GetPeakMeterSettings( COLORREF& base, COLORREF& peak, COLORREF& background )
{
	constexpr COLORREF kBase = RGB( 0 /*red*/, 122 /*green*/, 217 /*blue*/ );
	constexpr COLORREF kPeak = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	constexpr COLORREF kBackground = RGB( 0 /*red*/, 0 /*green*/, 0 /*blue*/ );

  base = ReadSetting<COLORREF>( "PeakMeterBase" ).value_or( kBase );
  peak = ReadSetting<COLORREF>( "PeakMeterPeak" ).value_or( kPeak );
  background = ReadSetting<COLORREF>( "PeakMeterBackground" ).value_or( kBackground );
}

void Settings::SetPeakMeterSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background )
{
  WriteSetting( "PeakMeterBase", base );
  WriteSetting( "PeakMeterPeak", peak );
  WriteSetting( "PeakMeterBackground", background );
}

void Settings::GetStartupPosition( int& x, int& y, int& width, int& height, bool& maximised, bool& minimised )
{
	if ( const auto value = ReadSetting<int>( "StartupX" ); value ) {
		x = *value;
	}
	if ( const auto value = ReadSetting<int>( "StartupY" ); value ) {
		y = *value;
	}
	if ( const auto value = ReadSetting<int>( "StartupWidth" ); value ) {
		width = *value;
	}
	if ( const auto value = ReadSetting<int>( "StartupHeight" ); value ) {
		height = *value;
	}
	if ( const auto value = ReadSetting<bool>( "StartupMaximised" ); value ) {
		maximised = *value;
	}
	if ( const auto value = ReadSetting<bool>( "StartupMinimised" ); value ) {
		minimised = *value;
	}
}

void Settings::SetStartupPosition( const int x, const int y, const int width, const int height, const bool maximised, const bool minimised )
{
  WriteSetting( "StartupX", x );
  WriteSetting( "StartupY", y );
  WriteSetting( "StartupWidth", width );
  WriteSetting( "StartupHeight", height );
  WriteSetting( "StartupMaximised", maximised );
  WriteSetting( "StartupMinimised", minimised );
}

int Settings::GetVisualID()
{
  return ReadSetting<int>( "VisualID" ).value_or( 0 );
}

void Settings::SetVisualID( const int visualID )
{
  WriteSetting( "VisualID", visualID );
}

int Settings::GetSplitWidth()
{
  return ReadSetting<int>( "SplitWidth" ).value_or( 0 );
}

void Settings::SetSplitWidth( const int width )
{
  WriteSetting( "SplitWidth", width );
}

float Settings::GetVolume()
{
  return ReadSetting<float>( "Volume" ).value_or( 1.0f );
}

void Settings::SetVolume( const float volume )
{
  WriteSetting( "Volume", volume );
}

std::wstring Settings::GetStartupPlaylist()
{
  return ReadSetting<std::wstring>( "StartupPlaylist" ).value_or( std::wstring() );
}

void Settings::SetStartupPlaylist( const std::wstring& playlist )
{
  WriteSetting( "StartupPlaylist", playlist );
}

std::tuple<std::wstring, long, long> Settings::GetStartupFile()
{
  const auto filename = ReadSetting<std::wstring>( "StartupFilename" ).value_or( std::wstring() );
  const auto cueStart = ReadSetting<long>( "StartupCueStart" ).value_or( -1 );
  const auto cueEnd = ReadSetting<long>( "StartupCueEnd" ).value_or( -1 );
  return std::make_tuple( filename, cueStart, cueEnd );
}

void Settings::SetStartupFile( const std::wstring& filename, const std::optional<long>& cueStart, const std::optional<long>& cueEnd )
{
  WriteSetting( "StartupFilename", filename );
  WriteSetting( "StartupCueStart", cueStart.value_or( -1 ) );
  WriteSetting( "StartupCueEnd", cueEnd.value_or( -1 ) );
}

void Settings::GetCounterSettings( LOGFONT& font, COLORREF& fontColour, bool& showRemaining )
{
	if ( const auto value = ReadSetting<LOGFONT>( "CounterFont" ); value ) {
		font = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "CounterFontColour" ); value ) {
		fontColour = *value;
	}
	if ( const auto value = ReadSetting<bool>( "CounterRemaining" ); value ) {
		showRemaining = *value;
	}
}

void Settings::SetCounterSettings( const LOGFONT& font, const COLORREF fontColour, const bool showRemaining )
{
  WriteSetting( "CounterFont", font );
  WriteSetting( "CounterFontColour", fontColour );
  WriteSetting( "CounterRemaining", showRemaining );
}

void Settings::GetOutputSettings( std::wstring& deviceName, OutputMode& mode )
{
	deviceName.clear();
	mode = OutputMode::Standard;

	if ( const auto value = ReadSetting<std::wstring>( "OutputDevice" ); value ) {
		deviceName = *value;
	}
	if ( const auto value = ReadSetting<OutputMode>( "OutputMode" ); value ) {
		mode = std::clamp( *value, OutputMode::Standard, OutputMode::ASIO );
	}
}

void Settings::SetOutputSettings( const std::wstring& deviceName, const OutputMode mode )
{
  WriteSetting( "OutputDevice", deviceName );
  WriteSetting( "OutputMode", mode );
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

	if ( const auto value = ReadSetting<long long>( "MOD" ); value ) {
		mod = *value;
	}
	if ( const auto value = ReadSetting<long long>( "MTM" ); value ) {
		mtm = *value;
	}
	if ( const auto value = ReadSetting<long long>( "S3M" ); value ) {
		s3m = *value;
	}
	if ( const auto value = ReadSetting<long long>( "XM" ); value ) {
		xm = *value;
	}
	if ( const auto value = ReadSetting<long long>( "IT" ); value ) {
		it = *value;
	}
}

void Settings::SetMODSettings( const long long mod, const long long mtm, const long long s3m, const long long xm, const long long it )
{
  WriteSetting( "MOD", mod );
  WriteSetting( "MTM", mtm );
  WriteSetting( "S3M", s3m );
  WriteSetting( "XM", xm );
  WriteSetting( "IT", it );
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

	if ( const auto value = ReadSetting<GainMode>( "GainMode" ); value ) {
		gainMode = std::clamp( *value, GainMode::Disabled, GainMode::Album );
	}
	if ( const auto value = ReadSetting<LimitMode>( "GainLimit" ); value ) {
		limitMode = std::clamp( *value, LimitMode::None, LimitMode::Soft );
	}
	if ( const auto value = ReadSetting<float>( "GainPreamp" ); value ) {
    constexpr float kMinPreamp = -6.0f;
    constexpr float kMaxPreamp = 12.0f;
		preamp = std::clamp( *value, kMinPreamp, kMaxPreamp );
	}
}

void Settings::SetGainSettings( const GainMode gainMode, const LimitMode limitMode, const float preamp )
{
  WriteSetting( "GainMode", gainMode );
  WriteSetting( "GainLimit", limitMode );
  WriteSetting( "GainPreamp", preamp );
}

void Settings::GetSystraySettings( bool& enable, bool& minimise, SystrayCommand& singleClick, SystrayCommand& doubleClick, SystrayCommand& tripleClick, SystrayCommand& quadClick )
{
	enable = false;
	minimise = false;
	singleClick = SystrayCommand::None;
	doubleClick = SystrayCommand::None;
	tripleClick = SystrayCommand::None;
	quadClick = SystrayCommand::None;

	if ( const auto value = ReadSetting<bool>( "SysTrayEnable" ); value ) {
		enable = *value;
	}
	if ( const auto value = ReadSetting<bool>( "SysTrayMinimise" ); value ) {
		minimise = *value;
	}
	if ( const auto value = ReadSetting<SystrayCommand>( "SysTraySingleClick" ); value ) {
		singleClick = std::clamp( *value, SystrayCommand::None, SystrayCommand::ShowHide );
	}
	if ( const auto value = ReadSetting<SystrayCommand>( "SysTrayDoubleClick" ); value ) {
		doubleClick = std::clamp( *value, SystrayCommand::None, SystrayCommand::ShowHide );
	}
	if ( const auto value = ReadSetting<SystrayCommand>( "SysTrayTripleClick" ); value ) {
		tripleClick = std::clamp( *value, SystrayCommand::None, SystrayCommand::ShowHide );
	}
	if ( const auto value = ReadSetting<SystrayCommand>( "SysTrayQuadrupleClick" ); value ) {
		quadClick = std::clamp( *value, SystrayCommand::None, SystrayCommand::ShowHide );
	}
}

void Settings::SetSystraySettings( const bool enable, const bool minimise, const SystrayCommand singleClick, const SystrayCommand doubleClick, const SystrayCommand tripleClick, const SystrayCommand quadClick )
{
  WriteSetting( "SysTrayEnable", enable );
  WriteSetting( "SysTrayMinimise", minimise );
  WriteSetting( "SysTraySingleClick", singleClick );
  WriteSetting( "SysTrayDoubleClick", doubleClick );
  WriteSetting( "SysTrayTripleClick", tripleClick );
  WriteSetting( "SysTrayQuadrupleClick", quadClick );
}

void Settings::GetPlaybackSettings( bool& randomPlay, bool& repeatTrack, bool& repeatPlaylist, bool& crossfade )
{
	randomPlay = false;
	repeatTrack = false;
	repeatPlaylist = false;
	crossfade = false;

	if ( const auto value = ReadSetting<bool>( "RandomPlay" ); value ) {
		randomPlay = *value;
	}
	if ( const auto value = ReadSetting<bool>( "RepeatTrack" ); value ) {
		repeatTrack = *value;
	}
	if ( const auto value = ReadSetting<bool>( "RepeatPlaylist" ); value ) {
		repeatPlaylist = *value;
	}
	if ( const auto value = ReadSetting<bool>( "Crossfade" ); value ) {
		crossfade = *value;
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
  WriteSetting( "RandomPlay", randomPlay );
  WriteSetting( "RepeatTrack", repeatTrack );
  WriteSetting( "RepeatPlaylist", repeatPlaylist );
  WriteSetting( "Crossfade", crossfade );
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
						hotkey.Code = sqlite3_column_int( stmt, columnIndex );
					} else if ( columnName == "Alt" ) {
						hotkey.Alt = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					} else if ( columnName == "Ctrl" ) {
						hotkey.Ctrl = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					} else if ( columnName == "Shift" ) {
						hotkey.Shift = ( 0 != sqlite3_column_int( stmt, columnIndex ) );
					} else if ( columnName == "Keyname" ) {
						if ( const unsigned char* text = sqlite3_column_text( stmt, columnIndex ); nullptr != text ) {
							hotkey.Name = UTF8ToWideString( reinterpret_cast<const char*>( text ) );
						}
					}
				}
				if ( ( 0 != hotkey.ID ) && ( 0 != hotkey.Code ) ) {
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
			query = "INSERT INTO Hotkeys (ID,Hotkey,Alt,Ctrl,Shift,Keyname) VALUES (?1,?2,?3,?4,?5,?6);";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				for ( const auto& hotkey : hotkeys ) {
					sqlite3_bind_int( stmt, 1, hotkey.ID );
					sqlite3_bind_int( stmt, 2, hotkey.Code );
					sqlite3_bind_int( stmt, 3, hotkey.Alt ? 1 : 0 );
					sqlite3_bind_int( stmt, 4, hotkey.Ctrl ? 1 : 0 );
					sqlite3_bind_int( stmt, 5, hotkey.Shift ? 1 : 0 );
					sqlite3_bind_text( stmt, 6, WideStringToUTF8( hotkey.Name ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
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
	return std::clamp<PitchRange>( ReadSetting<PitchRange>( "PitchRange" ).value_or( PitchRange::Small ), PitchRange::Small, PitchRange::Large );
}

void Settings::SetPitchRange( const PitchRange range )
{
  WriteSetting( "PitchRange", range );
}

Settings::PitchRangeMap Settings::GetPitchRangeOptions() const
{
	return s_PitchRanges;
}

int Settings::GetOutputControlType()
{
  return ReadSetting<int>( "OutputControlType" ).value_or( 0 );
}

void Settings::SetOutputControlType( const int type )
{
  WriteSetting( "OutputControlType", type );
}

void Settings::GetExtractSettings( std::wstring& folder, std::wstring& filename, bool& addToLibrary, bool& joinTracks )
{
	folder.clear();
	filename.clear();
	addToLibrary = true;
	joinTracks = false;

	if ( const auto value = ReadSetting<std::wstring>( "ExtractFolder" ); value ) {
		folder = *value;
	}
	if ( const auto value = ReadSetting<std::wstring>( "ExtractFilename" ); value ) {
		filename = *value;
	}
	if ( const auto value = ReadSetting<bool>( "ExtractToLibrary" ); value ) {
		addToLibrary = *value;
	}
	if ( const auto value = ReadSetting<bool>( "ExtractJoin" ); value ) {
		joinTracks = *value;
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
  WriteSetting( "ExtractFolder", folder );
  WriteSetting( "ExtractFilename", filename );
  WriteSetting( "ExtractToLibrary", addToLibrary );
  WriteSetting( "ExtractJoin", joinTracks );
}

Settings::EQ Settings::GetEQSettings()
{
	EQ eq;
	if ( const auto value = ReadSetting<bool>( "EQVisible" ); value ) {
		eq.Visible = *value;
	}
	if ( const auto value = ReadSetting<int>( "EQX" ); value ) {
		eq.X = *value;
	}
	if ( const auto value = ReadSetting<int>( "EQY" ); value ) {
		eq.Y = *value;
	}
	if ( const auto value = ReadSetting<bool>( "EQEnable" ); value ) {
		eq.Enabled = *value;
	}
	if ( const auto value = ReadSetting<float>( "EQPreamp" ); value ) {
		eq.Preamp = std::clamp( *value, EQ::MinGain, EQ::MaxGain );
	}
	for ( auto& [ freq, gain ] : eq.Gains ) {
    const std::string settingName = "EQ" + std::to_string( freq );
	  if ( const auto value = ReadSetting<float>( settingName ); value ) {
		  gain = std::clamp( *value, EQ::MinGain, EQ::MaxGain );
	  }
  }
	return eq;
}

void Settings::SetEQSettings( const EQ& eq )
{
  WriteSetting( "EQVisible", eq.Visible );
  WriteSetting( "EQX", eq.X );
  WriteSetting( "EQY", eq.Y );
  WriteSetting( "EQEnable", eq.Enabled );
  WriteSetting( "EQPreamp", eq.Preamp );
	for ( const auto& [ freq, gain ] : eq.Gains ) {
    const std::string settingName = "EQ" + std::to_string( freq );
    WriteSetting( settingName, gain );
  }
}

std::wstring Settings::GetEncoder()
{
  return ReadSetting<std::wstring>( "Encoder" ).value_or( std::wstring() );
}

void Settings::SetEncoder( const std::wstring& encoder )
{
  WriteSetting( "Encoder", encoder );
}

std::string Settings::GetEncoderSettings( const std::wstring& encoder )
{
	const std::string settingName = WideStringToUTF8( L"Encoder_" + encoder );
  return ReadSetting<std::string>( settingName ).value_or( std::string() );
}

void Settings::SetEncoderSettings( const std::wstring& encoder, const std::string& settings )
{
	const std::string settingName = WideStringToUTF8( L"Encoder_" + encoder );
  WriteSetting( settingName, settings );
}

std::wstring Settings::GetSoundFont()
{
  return ReadSetting<std::wstring>( "SoundFont" ).value_or( std::wstring() );
}

void Settings::SetSoundFont( const std::wstring& filename )
{
  WriteSetting( "SoundFont", filename );
}

bool Settings::GetToolbarEnabled( const int toolbarID )
{
	const std::string toolbarName = "Toolbar" + std::to_string( toolbarID );
	return ReadSetting<bool>( toolbarName ).value_or( true );
}

void Settings::SetToolbarEnabled( const int toolbarID, const bool enabled )
{
	const std::string toolbarName = "Toolbar" + std::to_string( toolbarID );
  WriteSetting( toolbarName, enabled );
}

bool Settings::GetMergeDuplicates()
{
	return ReadSetting<bool>( "HideDuplicates" ).value_or( false );
}

void Settings::SetMergeDuplicates( const bool mergeDuplicates )
{
  WriteSetting( "HideDuplicates", mergeDuplicates );
}

std::wstring Settings::GetLastFolder( const std::string& folderType )
{
  const std::string settingName = "Folder" + folderType;
	std::wstring lastFolder = ReadSetting<std::wstring>( settingName ).value_or( std::wstring() );
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
	const std::string settingName = "Folder" + folderType;
  WriteSetting( settingName, folder );
}

bool Settings::GetScrobblerEnabled()
{
	return ReadSetting<bool>( "ScrobblerEnable" ).value_or( false );
}

void Settings::SetScrobblerEnabled( const bool enabled )
{
	WriteSetting( "ScrobblerEnable", enabled );
}

std::string Settings::GetScrobblerKey()
{
	std::string decryptedKey;
	if ( const std::string key = ReadSetting<std::string>( "ScrobblerKey" ).value_or( std::string() ); !key.empty() ) {
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
	WriteSetting( "ScrobblerKey", encryptedKey );
}

bool Settings::GetMusicBrainzEnabled()
{
	return ReadSetting<bool>( "MusicBrainzEnable" ).value_or( true );
}

void Settings::SetMusicBrainzEnabled( const bool enabled )
{
	WriteSetting( "MusicBrainzEnable", enabled );
}

void Settings::GetDefaultAdvancedWasapiExclusiveSettings( bool& useDeviceDefaultFormat, int& bufferLength, int& leadIn, int& maxBufferLength, int& maxLeadIn )
{
	useDeviceDefaultFormat = false;
	bufferLength = 250;
	leadIn = 0;
	maxBufferLength = 1000;
	maxLeadIn = 2000;
}

void Settings::GetAdvancedWasapiExclusiveSettings( bool& useDeviceDefaultFormat, int& bufferLength, int& leadIn )
{
	int maxBufferLength = 0;
	int maxLeadIn = 0;
	GetDefaultAdvancedWasapiExclusiveSettings( useDeviceDefaultFormat, bufferLength, leadIn, maxBufferLength, maxLeadIn );
	if ( const auto value = ReadSetting<bool>( "WasapiExclusiveUseDeviceFormat" ); value ) {
		useDeviceDefaultFormat = *value;
	}
	if ( const auto value = ReadSetting<int>( "WasapiExclusiveBufferLength" ); value ) {
		bufferLength = std::clamp( *value, 0, maxBufferLength );
	}
	if ( const auto value = ReadSetting<int>( "WasapiExclusiveLeadIn" ); value ) {
		leadIn = std::clamp( *value, 0, maxLeadIn );
	}
}

void Settings::SetAdvancedWasapiExclusiveSettings( const bool useDeviceDefaultFormat, const int bufferLength, const int leadIn )
{
	WriteSetting( "WasapiExclusiveUseDeviceFormat", useDeviceDefaultFormat );
	WriteSetting( "WasapiExclusiveBufferLength", bufferLength );
	WriteSetting( "WasapiExclusiveLeadIn", leadIn );
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
	if ( const auto value = ReadSetting<bool>( "ASIOUseDefaultSamplerate" ); value ) {
		useDefaultSamplerate = *value;
	}
	if ( const auto value = ReadSetting<int>( "ASIODefaultSamplerate" ); value ) {
		defaultSamplerate = std::clamp( *value, 0, maxDefaultSamplerate );
	}
	if ( const auto value = ReadSetting<int>( "ASIOLeadIn" ); value ) {
		leadIn = std::clamp( *value, 0, maxLeadIn );
	}
}

void Settings::SetAdvancedASIOSettings( const bool useDefaultSamplerate, const int defaultSamplerate, const int leadIn )
{
	WriteSetting( "ASIOUseDefaultSamplerate", useDefaultSamplerate );
	WriteSetting( "ASIODefaultSamplerate", defaultSamplerate );
	WriteSetting( "ASIOLeadIn", leadIn );
}

Settings::ToolbarSize Settings::GetToolbarSize()
{
	return std::clamp<ToolbarSize>( ReadSetting<ToolbarSize>( "ToolbarSize" ).value_or( ToolbarSize::Small ), ToolbarSize::Small, ToolbarSize::Large );
}

void Settings::SetToolbarSize( const ToolbarSize size )
{
	WriteSetting( "ToolbarSize", size );
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

void Settings::GetToolbarColours( COLORREF& buttonColour, COLORREF& backgroundColour )
{
	if ( const auto value = ReadSetting<COLORREF>( "ToolbarButtonColour" ); value ) {
		buttonColour = *value;
	}
	if ( const auto value = ReadSetting<COLORREF>( "ToolbarBackgroundColour" ); value ) {
		backgroundColour = *value;
	}
}

void Settings::SetToolbarColours( const COLORREF buttonColour, const COLORREF backgroundColour )
{
	WriteSetting( "ToolbarButtonColour", buttonColour );
	WriteSetting( "ToolbarBackgroundColour", backgroundColour );
}

bool Settings::GetHardwareAccelerationEnabled()
{
	return ReadSetting<bool>( "VisualHardwareAcceleration" ).value_or( true );
}

void Settings::SetHardwareAccelerationEnabled( const bool enabled )
{
	WriteSetting( "VisualHardwareAcceleration", enabled );
}

bool Settings::GetAlwaysOnTop()
{
	return ReadSetting<bool>( "AlwaysOnTop" ).value_or( false );
}

void Settings::SetAlwaysOnTop( const bool alwaysOnTop )
{
	WriteSetting( "AlwaysOnTop", alwaysOnTop );
}

bool Settings::GetRetainStopAtTrackEnd()
{
	return ReadSetting<bool>( "RetainStopAtTrackEnd" ).value_or( false );
}

void Settings::SetRetainStopAtTrackEnd( const bool retain )
{
	WriteSetting( "RetainStopAtTrackEnd", retain );
}

bool Settings::GetStopAtTrackEnd()
{
	return ReadSetting<bool>( "StopAtTrackEnd" ).value_or( false );
}

void Settings::SetStopAtTrackEnd( const bool enabled )
{
	WriteSetting( "StopAtTrackEnd", enabled );
}

bool Settings::GetRetainPitchBalance()
{
	return ReadSetting<bool>( "RetainPitchBalance" ).value_or( false );
}

void Settings::SetRetainPitchBalance( const bool retain )
{
	WriteSetting( "RetainPitchBalance", retain );
}

std::pair<float /*pitch*/, float /*balance*/> Settings::GetPitchBalance()
{
	constexpr float kDefaultPitch = 1.0f;
	constexpr float kDefaultBalance = 0.0f;

	auto setting = std::make_pair( kDefaultPitch, kDefaultBalance );
	auto& [ pitch, balance ] = setting;
	pitch = ReadSetting<float>( "Pitch" ).value_or( kDefaultPitch );
	balance = ReadSetting<float>( "Balance" ).value_or( kDefaultBalance );
	return setting;
}

void Settings::SetPitchBalance( const float pitch, const float balance )
{
	WriteSetting( "Pitch", pitch );
	WriteSetting( "Balance", balance );
}

int Settings::GetLastOptionsPage()
{
	return ReadSetting<int>( "OptionsPage" ).value_or( 0 );
}

void Settings::SetLastOptionsPage( const int index )
{
	WriteSetting( "OptionsPage", index );
}

COLORREF Settings::GetTaskbarButtonColour()
{
	constexpr COLORREF kDefaultColour = RGB( 55, 165, 255 );

	return ReadSetting<COLORREF>( "TaskbarButtonColour" ).value_or( kDefaultColour );
}

void Settings::SetTaskbarButtonColour( const COLORREF colour )
{
	WriteSetting( "TaskbarButtonColour", colour );
}

bool Settings::GetTaskbarShowProgress()
{
  return ReadSetting<bool>( "TaskbarProgress" ).value_or( true );
}

void Settings::SetTaskbarShowProgress( const bool showProgress )
{
	WriteSetting( "TaskbarProgress", showProgress );
}

bool Settings::GetWriteFileTags()
{
  return ReadSetting<bool>( "WriteFileTags" ).value_or( true );
}

void Settings::SetWriteFileTags( const bool write )
{
	WriteSetting( "WriteFileTags", write );
}

bool Settings::GetPreserveLastModifiedTime()
{
  return ReadSetting<bool>( "PreserveLastModified" ).value_or( false );
}

void Settings::SetPreserveLastModifiedTime( const bool preserve )
{
	WriteSetting( "PreserveLastModified", preserve );
}

Settings::MODDecoder Settings::GetMODDecoder()
{
#ifndef _DEBUG
  return std::clamp<MODDecoder>( ReadSetting<MODDecoder>( "MODDecoder" ).value_or( MODDecoder::OpenMPT ), MODDecoder::BASS, MODDecoder::OpenMPT );
#else
  return MODDecoder::BASS;
#endif
}

void Settings::SetMODDecoder( const MODDecoder decoder )
{
  WriteSetting<MODDecoder>( "MODDecoder", decoder );
}

uint32_t Settings::GetMODSamplerate()
{
  constexpr uint32_t kDefaultSamplerate = 48000;
  uint32_t samplerate = ReadSetting<uint32_t>( "MODSamplerate" ).value_or( kDefaultSamplerate );
  constexpr auto supportedSampleRates = GetMODSupportedSamplerates();
  if ( const auto it = std::find( supportedSampleRates.begin(), supportedSampleRates.end(), samplerate ); supportedSampleRates.end() == it ) {
    samplerate = kDefaultSamplerate;
  }
  return samplerate;
}

void Settings::SetMODSamplerate( const uint32_t samplerate )
{
  WriteSetting<uint32_t>( "MODSamplerate", samplerate );
}

void Settings::GetDefaultOpenMPTSettings( bool& fadeout, long& separation, long& ramping, long& interpolation )
{
  fadeout = false;
  separation = 100;
  ramping = -1;
  interpolation = 0;
}

void Settings::GetOpenMPTSettings( bool& fadeout, long& separation, long& ramping, long& interpolation )
{
  GetDefaultOpenMPTSettings( fadeout, separation, ramping, interpolation );
  if ( const auto setting = ReadSetting<bool>( "openmptFadeout" ) ) {
    fadeout = *setting;
  }
  if ( const auto setting = ReadSetting<long>( "openmptSeparation" ) ) {
    separation = std::clamp( *setting, 0l, 200l );
  }
  if ( const auto setting = ReadSetting<long>( "openmptRamping" ) ) {
    constexpr auto supportedRamping = GetOpenMPTSupportedRamping();
    if ( const auto it = std::find( supportedRamping.begin(), supportedRamping.end(), *setting ); supportedRamping.end() != it ) {
      ramping = *setting;
    }
  }
  if ( const auto setting = ReadSetting<long>( "openmptInterpolation" ) ) {
    constexpr auto supportedInterpolation = GetOpenMPTSupportedInterpolation();
    if ( const auto it = std::find( supportedInterpolation.begin(), supportedInterpolation.end(), *setting ); supportedInterpolation.end() != it ) {
      interpolation = *setting;
    }
  }
}

void Settings::SetOpenMPTSettings( const bool fadeout, const long separation, const long ramping, const long interpolation )
{
  WriteSetting<bool>( "openmptFadeout", fadeout );
  WriteSetting<long>( "openmptSeparation", separation );
  WriteSetting<long>( "openmptRamping", ramping );
  WriteSetting<long>( "openmptInterpolation", interpolation ); 
}

bool Settings::GetLoudnessNormalisation()
{
  bool enabled = false;
  if ( const auto setting = ReadSetting<bool>( "LoudnessNormalisation" ) ) {
    enabled = *setting;
  } else if ( const auto gainMode = ReadSetting<GainMode>( "GainMode" ) ) {
    // If the loudness normalisation setting is not present, initialise it based on the gain mode.
	  enabled = ( GainMode::Track == *gainMode ) || ( GainMode::Album == *gainMode );
  }
  return enabled;
}

void Settings::SetLoudnessNormalisation( const bool enable )
{
	WriteSetting( "LoudnessNormalisation", enable );
}

Settings::TitleBarFormat Settings::GetTitleBarFormat()
{
  return std::clamp<TitleBarFormat>( ReadSetting<TitleBarFormat>( "TitleBarFormat" ).value_or( TitleBarFormat::ArtistTitle ), TitleBarFormat::ArtistTitle, TitleBarFormat::ApplicationName );
}

void Settings::SetTitleBarFormat( const TitleBarFormat format )
{
  WriteSetting<TitleBarFormat>( "TitleBarFormat", format );
}
