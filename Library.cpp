#include "Library.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <iomanip>
#include <list>
#include <sstream>

Library::Library( Database& database, const Handlers& handlers ) :
	m_Database( database ),
	m_Handlers( handlers ),
	m_PendingTags(),
	m_LastTagWriteTime( 0 ),
	m_TagsWritten(),
	m_TagsWrittenMutex(),
	m_MediaColumns( {
		Columns::value_type( "Filename", Column::Filename ),
		Columns::value_type( "Filetime", Column::Filetime ),
		Columns::value_type( "Filesize", Column::Filesize ),
		Columns::value_type( "Duration", Column::Duration ),
		Columns::value_type( "SampleRate", Column::SampleRate ),
		Columns::value_type( "BitsPerSample", Column::BitsPerSample ),
		Columns::value_type( "Channels", Column::Channels ),
		Columns::value_type( "Artist", Column::Artist ),
		Columns::value_type( "Title", Column::Title ),
		Columns::value_type( "Album", Column::Album ),
		Columns::value_type( "Genre", Column::Genre ),
		Columns::value_type( "Year", Column::Year ),
		Columns::value_type( "Comment", Column::Comment ),
		Columns::value_type( "Track", Column::Track ),
		Columns::value_type( "Version", Column::Version ),
		Columns::value_type( "GainTrack", Column::GainTrack ),
		Columns::value_type( "GainAlbum", Column::GainAlbum ),
		Columns::value_type( "Artwork", Column::Artwork ),
		Columns::value_type( "Bitrate", Column::Bitrate ),
		Columns::value_type( "Composer", Column::Composer ),
		Columns::value_type( "Conductor", Column::Conductor ),
		Columns::value_type( "Publisher", Column::Publisher )
	} ),
	m_CDDAColumns( {
		Columns::value_type( "CDDB", Column::CDDB ),
		Columns::value_type( "Track", Column::Track ),
		Columns::value_type( "Filesize", Column::Filesize ),
		Columns::value_type( "Duration", Column::Duration ),
		Columns::value_type( "Artist", Column::Artist ),
		Columns::value_type( "Title", Column::Title ),
		Columns::value_type( "Album", Column::Album ),
		Columns::value_type( "Genre", Column::Genre ),
		Columns::value_type( "Year", Column::Year ),
		Columns::value_type( "Comment", Column::Comment ),
		Columns::value_type( "GainTrack", Column::GainTrack ),
		Columns::value_type( "GainAlbum", Column::GainAlbum ),
		Columns::value_type( "Artwork", Column::Artwork ),
		Columns::value_type( "Composer", Column::Composer ),
		Columns::value_type( "Conductor", Column::Conductor ),
		Columns::value_type( "Publisher", Column::Publisher )
	} )
{
  m_CueColumns = m_MediaColumns;
  m_CueColumns.insert( { "CueStart", Column::CueStart } );
  m_CueColumns.insert( { "CueEnd", Column::CueEnd } );

  // Generate the matching fields for union queries (note that cue columns is a superset of media columns).
  for ( const auto& [ columnName, columnType ] : m_CueColumns ) {
    m_CueFields += columnName + ',';
    if ( const auto column = m_MediaColumns.find( columnName ); m_MediaColumns.end() == column ) {
      m_MediaFields += "NULL AS ";
    }
    m_MediaFields += columnName + ',';
  }
  m_CueFields.pop_back();
  m_MediaFields.pop_back();

	UpdateDatabase();
}

Library::~Library()
{
	for ( const auto& filename : m_PendingTags ) {
    if ( MediaInfo mediaInfo( filename ); GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ ) ) {
		  if ( m_Handlers.SetTags( mediaInfo, *this ) ) {
			  if ( GetDecoderInfo( mediaInfo, false /*getTags*/ ) ) {
	        UpdateMediaLibrary( mediaInfo );
        }
		  }
    }
	}
}

void Library::UpdateDatabase()
{
	UpdateMediaTable( false /*cuesTable*/ );
  UpdateMediaTable( true /*cuesTable*/ );
	UpdateCDDATable();
	UpdateArtworkTable();
	CreateIndices();
}

void Library::UpdateMediaTable(const bool cuesTable)
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
    // Create the appropriate table, if necessary.
    const std::string tableName = cuesTable ? "Cues" : "Media";
	  std::string createTableQuery = "CREATE TABLE IF NOT EXISTS " + tableName + "(";
    if ( cuesTable ) {
		  for ( const auto& iter : m_CueColumns ) {
			  const std::string& columnName = iter.first;
			  createTableQuery += columnName + ",";
		  }
		  createTableQuery += "PRIMARY KEY(Filename,CueStart,CueEnd));";
    } else {
		  for ( const auto& iter : m_MediaColumns ) {
			  const std::string& columnName = iter.first;
			  createTableQuery += columnName + ",";
		  }
		  createTableQuery += "PRIMARY KEY(Filename));";
    }
		sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the media table.
		const std::string tableInfoQuery = "PRAGMA table_info('" + tableName + "')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, tableInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			Columns missingColumns( cuesTable ? m_CueColumns : m_MediaColumns );
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

			if ( !missingColumns.empty() ) {
				if ( missingColumns.find( "Filename" ) != missingColumns.end() ) {
					// No primary index, just drop the table and recreate.
					const std::string dropTableQuery = "DROP TABLE " + tableName + ";";
					sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				} else {
					for ( const auto& iter : missingColumns ) {
						std::string addColumnQuery = "ALTER TABLE " + tableName + " ADD COLUMN ";
						addColumnQuery += iter.first + ";";
						sqlite3_exec( database, addColumnQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					}
				}
			}
		}
	}
}

void Library::UpdateCDDATable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the CDDA table (if necessary).
		std::string createTableQuery = "CREATE TABLE IF NOT EXISTS CDDA(";
		for ( const auto& iter : m_CDDAColumns ) {
			const std::string& columnName = iter.first;
			createTableQuery += columnName + ",";
		}
		createTableQuery += "PRIMARY KEY(CDDB,Track));";
		sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the CDDA table.
		const std::string tableInfoQuery = "PRAGMA table_info('CDDA')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, tableInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			Columns missingColumns( m_CDDAColumns );
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

			if ( !missingColumns.empty() ) {
				if ( ( missingColumns.find( "CDDB" ) != missingColumns.end() ) || ( missingColumns.find( "Track" ) != missingColumns.end() ) ) {
					// No primary index, just drop the table and recreate.
					const std::string dropTableQuery = "DROP TABLE CDDA;";
					sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				} else {
					for ( const auto& iter : missingColumns ) {
						std::string addColumnQuery = "ALTER TABLE CDDA ADD COLUMN ";
						addColumnQuery += iter.first + ";";
						sqlite3_exec( database, addColumnQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					}
				}
			}
		}
	}
}

void Library::UpdateArtworkTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the artwork table (if necessary).
		const std::string artworkTableQuery = "CREATE TABLE IF NOT EXISTS Artwork(ID,Size,Image, PRIMARY KEY(ID));";
		sqlite3_exec( database, artworkTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the artwork table.
		const std::string columnsInfoQuery = "PRAGMA table_info('Artwork')";
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

			if ( ( columns.find( "ID" ) == columns.end() ) || ( columns.find( "Size" ) == columns.end() ) || ( columns.find( "Image" ) == columns.end() ) ) {
				// Drop the table and recreate
				const std::string dropTableQuery = "DROP TABLE Artwork;";
				sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				sqlite3_exec( database, artworkTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
			}
		}
	}
}

void Library::CreateIndices()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string artistIndex = "CREATE INDEX IF NOT EXISTS MediaIndex_Artist ON Media(Artist);";
		sqlite3_exec( database, artistIndex.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
	}
}

bool Library::GetMediaInfo( MediaInfo& mediaInfo, const bool checkFileAttributes, const bool scanMedia, const bool sendNotification, const bool removeMissing )
{
	bool success = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		MediaInfo info( mediaInfo );
		std::string query;
    if ( MediaInfo::Source::CDDA == info.GetSource() ) {
      query = "SELECT * FROM CDDA WHERE CDDB=?1 AND Track=?2;";
    } else {
      if ( mediaInfo.GetCueStart() ) {
        query = "SELECT * FROM Cues WHERE Filename=?1 AND CueStart=?2 AND CueEnd=?3;";
      } else {
        query = "SELECT * FROM Media WHERE Filename=?1;";
      }
    }
		sqlite3_stmt* stmt = nullptr;
		success = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) );
		if ( success ) {
      if ( MediaInfo::Source::CDDA == info.GetSource() ) {
        success = ( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( info.GetCDDB() ) ) ) && ( SQLITE_OK == sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( info.GetTrack() ) ) );
      } else {
        if ( mediaInfo.GetCueStart() ) {
          success = ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( info.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) && 
            ( SQLITE_OK == sqlite3_bind_int64( stmt, 2 /*param*/, *info.GetCueStart() ) ) && ( SQLITE_OK == sqlite3_bind_int64( stmt, 3 /*param*/, info.GetCueEnd().value_or( -1 ) ) );
        } else {
          success = ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( info.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) );
        }
      }
			if ( success ) {
				// Should be a maximum of one entry.
				const int result = sqlite3_step( stmt );
				success = ( SQLITE_ROW == result );
				if ( success ) {
					if ( !ExtractMediaInfo( stmt, info ) && ( MediaInfo::Source::File == info.GetSource() ) ) {
            // Rescan file to update any missing table data.
            GetDecoderInfo( info, true /*getTags*/ );
            UpdateMediaLibrary( info );
          } else if ( checkFileAttributes ) {
						long long filetime = 0;
						long long filesize = 0;
						GetFileInfo( info.GetFilename(), filetime, filesize );
						success = ( info.GetFiletime() == filetime ) && ( info.GetFilesize() == filesize );
						if ( !success ) {
							info = mediaInfo;
						}
					}
				}

				if ( !success && scanMedia && ( MediaInfo::Source::File == info.GetSource() ) ) {
					success = GetDecoderInfo( info, true /*getTags*/ );
					if ( success ) {
						success = UpdateMediaLibrary( info );
						if ( success && sendNotification ) {
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
                MediaInfo previousInfo( mediaInfo.GetFilename() );
                previousInfo.SetCueStart( mediaInfo.GetCueStart() );
                previousInfo.SetCueEnd( mediaInfo.GetCueEnd() );
								vuplayer->OnMediaUpdated( previousInfo, info /*updatedInfo*/ );
							}
						}
					} else if ( removeMissing ) {
						RemoveFromLibrary( info );
					}
				}

				if ( success ) {
					mediaInfo = info;
				}
			}
			sqlite3_finalize( stmt );
		}
	}
	return success;
}

bool Library::GetFileInfo( const std::wstring& filename, long long& lastModified, long long& fileSize ) const
{
	bool success = false;
	if ( !IsURL( filename ) ) {
		fileSize = 0;
		lastModified = 0;
		const DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		const HANDLE handle = CreateFile( filename.c_str(), GENERIC_READ, shareMode, NULL /*security*/, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*template*/ );
		if ( INVALID_HANDLE_VALUE != handle ) {
			LARGE_INTEGER size = {};
			if ( GetFileSizeEx( handle, &size ) ) {
				fileSize = static_cast<long long>( size.QuadPart );
			}
			FILETIME lastWriteTime = {};
			if ( GetFileTime( handle, NULL /*creationTime*/, NULL /*lastAccessTime*/, &lastWriteTime ) ) {
				lastModified = ( static_cast<long long>( lastWriteTime.dwHighDateTime ) << 32 ) + lastWriteTime.dwLowDateTime;
			}
			CloseHandle( handle );
			success = true;
		}
	}
	return success;
}

bool Library::GetDecoderInfo( MediaInfo& mediaInfo, const bool getTags )
{
	bool success = false;
	Decoder::Ptr stream = m_Handlers.OpenDecoder( mediaInfo, Decoder::Context::Temporary, false /*applyCues*/ );
	if ( stream ) {
		mediaInfo.SetBitsPerSample( stream->GetBPS() );
		mediaInfo.SetChannels( stream->GetChannels() );
		mediaInfo.SetDuration( stream->GetDuration() );
		mediaInfo.SetSampleRate( stream->GetSampleRate() );
		mediaInfo.SetBitrate( stream->GetBitrate() );

    if ( getTags ) {
		  Tags tags;
		  if ( m_Handlers.GetTags( mediaInfo.GetFilename(), tags ) ) {
			  UpdateMediaInfoFromTags( mediaInfo, tags );
		  }
    }

		long long filetime = 0;
		long long filesize = 0;
		GetFileInfo( mediaInfo.GetFilename(), filetime, filesize );
		mediaInfo.SetFiletime( filetime );
		mediaInfo.SetFilesize( filesize );

		success = true;
	}
	return success;
}

bool Library::ExtractMediaInfo( sqlite3_stmt* stmt, MediaInfo& mediaInfo )
{
  bool missingData = false;
	if ( nullptr != stmt ) {
		const int columnCount = sqlite3_column_count( stmt );
    // Use cue columns (as a superset of media columns).
		const Columns& columns = m_CueColumns;
		for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
			const auto columnIter = columns.find( sqlite3_column_name( stmt, columnIndex ) );
			if ( columnIter != columns.end() ) {
        const bool isNull = ( SQLITE_NULL == sqlite3_column_type( stmt, columnIndex ) );
        const auto column = columnIter->second;
				switch ( column ) {
					case Column::Filename : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetFilename( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Filetime : {						
						mediaInfo.SetFiletime( static_cast<long long>( sqlite3_column_int64( stmt, columnIndex ) ) );
						break;
					}
					case Column::Filesize : {						
						mediaInfo.SetFilesize( static_cast<long long>( sqlite3_column_int64( stmt, columnIndex ) ) );
						break;
					}
					case Column::Duration : {						
						mediaInfo.SetDuration( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );
						break;
					}
					case Column::SampleRate : {						
						mediaInfo.SetSampleRate( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
						break;
					}
					case Column::BitsPerSample : {						
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
							mediaInfo.SetBitsPerSample( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
						}
						break;
					}
					case Column::Channels : {						
						mediaInfo.SetChannels( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
						break;
					}
					case Column::Artist : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetArtist( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Title : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetTitle( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Album : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetAlbum( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Genre : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetGenre( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Year : {		
            if ( isNull ) {
              missingData = true;
            } else {
						  mediaInfo.SetYear( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
            }
						break;
					}
					case Column::Comment : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetComment( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Track : {
            if ( isNull ) {
              missingData = true;
            } else {
						  mediaInfo.SetTrack( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
            }
						break;
					}
					case Column::Version : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetVersion( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::GainTrack : {	
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
							mediaInfo.SetGainTrack( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );							
						}					
						break;
					}
					case Column::GainAlbum : {						
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
							mediaInfo.SetGainAlbum( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );
						}
						break;
					}
					case Column::Artwork : {
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetArtworkID( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Bitrate : {
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
							mediaInfo.SetBitrate( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );
						}
						break;
					}
          case Column::CueStart : {
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
              mediaInfo.SetCueStart( static_cast<long>( sqlite3_column_int64( stmt, columnIndex ) ) );
            }
            break;
          }
          case Column::CueEnd : {
						if ( SQLITE_NULL != sqlite3_column_type( stmt, columnIndex ) ) {
              mediaInfo.SetCueEnd( static_cast<long>( sqlite3_column_int64( stmt, columnIndex ) ) );
            }
            break;
          }
					case Column::Composer : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetComposer( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Conductor : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetConductor( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
					case Column::Publisher : {						
						if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) ); nullptr != text ) {
							mediaInfo.SetPublisher( UTF8ToWideString( text ) );
						} else if ( isNull ) {
              missingData = true;
            }     
						break;
					}
				}
			}
		}
	}
  return !missingData;
}

bool Library::UpdateMediaLibrary( const MediaInfo& mediaInfo )
{
	bool success = false;

	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {

		const Columns& columnMap = GetColumns( mediaInfo );
		const std::string tableName = ( MediaInfo::Source::CDDA == mediaInfo.GetSource() ) ? "CDDA" : ( mediaInfo.GetCueStart() ? "Cues" : "Media" );

		std::string columns = " (";
		std::string values = " VALUES (";
		int param = 0;
		for ( const auto& iter : columnMap ) {
			columns += iter.first + ",";
			values += "?" + std::to_string( ++param ) + ",";
		}
		columns.back() = ')';
		values.back() = ')';
		const std::string query = "REPLACE INTO " + tableName + columns + values + ";";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			param = 0;
			for ( const auto& iter : columnMap ) {
				switch ( iter.second ) {
					case Column::Album : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetAlbum() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Artist : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetArtist() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::BitsPerSample : {
						const auto bps = mediaInfo.GetBitsPerSample();
						if ( bps.has_value() ) {
							sqlite3_bind_int( stmt, ++param, static_cast<int>( bps.value() ) );
						} else {
							sqlite3_bind_null( stmt, ++param );
						}
						break;
					}
					case Column::Channels : {
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetChannels() ) );
						break;
					}
					case Column::Comment : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetComment() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Duration : {
						sqlite3_bind_double( stmt, ++param, mediaInfo.GetDuration( false /*applyCues*/ ) );
						break;
					}
					case Column::Filename : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Filesize : {
						sqlite3_bind_int64( stmt, ++param, static_cast<sqlite3_int64>( mediaInfo.GetFilesize() ) );
						break;
					}
					case Column::Filetime : {
						sqlite3_bind_int64( stmt, ++param, static_cast<sqlite3_int64>( mediaInfo.GetFiletime() ) );
						break;
					}
					case Column::GainAlbum : {
						const auto gain = mediaInfo.GetGainAlbum();
						if ( gain.has_value() ) {
							sqlite3_bind_double( stmt, ++param, gain.value() );
						} else {
							sqlite3_bind_null( stmt, ++param );
						}
						break;
					}
					case Column::GainTrack : {
						const auto gain = mediaInfo.GetGainTrack();
						if ( gain.has_value() ) {
							sqlite3_bind_double( stmt, ++param, gain.value() );
						} else {
							sqlite3_bind_null( stmt, ++param );
						}
						break;
					}
					case Column::Genre : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetGenre() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::SampleRate : {
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetSampleRate() ) );
						break;
					}
					case Column::Title : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetTitle() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Track : {
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetTrack() ) );
						break;
					}
					case Column::Version : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetVersion() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Year : {
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetYear() ) );
						break;
					}
					case Column::Artwork : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetArtworkID() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::CDDB : {
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetCDDB() ) );
						break;
					}
					case Column::Bitrate : {
						const auto bitrate = mediaInfo.GetBitrate();
						if ( bitrate.has_value() ) {
							sqlite3_bind_double( stmt, ++param, bitrate.value() );
						} else {
							sqlite3_bind_null( stmt, ++param );
						}
						break;
					}
          case Column::CueStart : {
						const auto cue = mediaInfo.GetCueStart();
						sqlite3_bind_int64( stmt, ++param, cue.value_or( -1 ) );
						break;
          }
          case Column::CueEnd : {
            // Cues are part of the primary key in the Cues table, so use a negative value to indicate no cue, rather than null.
						const auto cue = mediaInfo.GetCueEnd();
						sqlite3_bind_int64( stmt, ++param, cue.value_or( -1 ) );
						break;
          }
					case Column::Composer : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetComposer() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Conductor : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetConductor() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					case Column::Publisher : {
						sqlite3_bind_text( stmt, ++param, WideStringToUTF8( mediaInfo.GetPublisher() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
						break;
					}
					default : {
						break;
					}
				}
			}
			const int result = sqlite3_step( stmt );
			success = ( SQLITE_DONE == result );
			sqlite3_finalize( stmt );
		}
	}
	return success;
}

void Library::UpdateMediaTags( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo )
{
  const bool updateTags =
    ( previousMediaInfo.GetAlbum() != updatedMediaInfo.GetAlbum() ) ||
    ( previousMediaInfo.GetArtist() != updatedMediaInfo.GetArtist() ) ||
    ( previousMediaInfo.GetComment() != updatedMediaInfo.GetComment() ) ||
    ( previousMediaInfo.GetGenre() != updatedMediaInfo.GetGenre() ) ||
    ( previousMediaInfo.GetTitle() != updatedMediaInfo.GetTitle() ) ||
    ( previousMediaInfo.GetTrack() != updatedMediaInfo.GetTrack() ) ||
    ( previousMediaInfo.GetYear() != updatedMediaInfo.GetYear() ) ||
    ( previousMediaInfo.GetGainAlbum() != updatedMediaInfo.GetGainAlbum() ) ||
    ( previousMediaInfo.GetGainTrack() != updatedMediaInfo.GetGainTrack() ) ||
    ( previousMediaInfo.GetArtworkID() != updatedMediaInfo.GetArtworkID() ) ||
    ( previousMediaInfo.GetComposer() != updatedMediaInfo.GetComposer() ) ||
    ( previousMediaInfo.GetConductor() != updatedMediaInfo.GetConductor() ) ||
    ( previousMediaInfo.GetPublisher() != updatedMediaInfo.GetPublisher() );

	if ( updateTags ) {
		MediaInfo mediaInfo( updatedMediaInfo );
		WriteFileTags( mediaInfo );
	  UpdateMediaLibrary( mediaInfo );

		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			vuplayer->OnMediaUpdated( previousMediaInfo, mediaInfo );
		}
	}
}

void Library::WriteFileTags( MediaInfo& mediaInfo )
{
	if ( ( MediaInfo::Source::File == mediaInfo.GetSource() ) && !mediaInfo.GetCueStart() ) {
		const std::wstring& filename = mediaInfo.GetFilename();
		SetRecentlyWrittenTag( filename );
		if ( m_Handlers.SetTags( mediaInfo, *this ) ) {
			m_PendingTags.erase( filename );
			GetDecoderInfo( mediaInfo, false /*getTags*/ );
		} else {
      m_PendingTags.insert( filename );
		}
	}
}

bool Library::AddArtwork( const std::wstring& id, const std::vector<BYTE>& image )
{
	bool success = false;
	if ( !image.empty() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			sqlite3_stmt* stmt = nullptr;
			const std::string insertQuery = "REPLACE INTO Artwork (ID,Size,Image) VALUES (?1,?2,?3);";
			if ( SQLITE_OK == sqlite3_prepare_v2( database, insertQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				sqlite3_bind_text( stmt, 1, WideStringToUTF8( id ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT );
				sqlite3_bind_int( stmt, 2, static_cast<int>( image.size() ) );
				sqlite3_bind_blob( stmt, 3, &image[ 0 ], static_cast<int>( image.size() ), SQLITE_STATIC );
				success = ( SQLITE_DONE == sqlite3_step( stmt ) );
				sqlite3_finalize( stmt );
			}
		}
	}
	return success;
}

std::wstring Library::FindArtwork( const std::vector<BYTE>& image )
{
	std::wstring result;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		std::string query = "SELECT ID,Image FROM Artwork WHERE Size=?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( image.size() ) ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					const size_t numBytes = static_cast<size_t>( sqlite3_column_bytes( stmt, 1 /*columnIndex*/ ) );
					if ( ( numBytes == image.size() ) && ( numBytes > 0 ) ) {
						const BYTE* bytes = static_cast<const BYTE*>( sqlite3_column_blob( stmt, 1 /*columnIndex*/ ) );
						if ( nullptr != bytes ) {
							if ( 0 == memcmp( bytes, &image[ 0 ], numBytes ) ) {
								if ( const char* id = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) ); nullptr != id ) {
									result = UTF8ToWideString( id );
									break;
								}
							}
						}
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return result;
}

std::vector<BYTE> Library::GetMediaArtwork( const MediaInfo& mediaInfo )
{
	std::vector<BYTE> result;
	const std::wstring artworkID = mediaInfo.GetArtworkID();
	if ( !artworkID.empty() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			const std::string query = "SELECT Image FROM Artwork WHERE ID=?1;";
			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artworkID ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
					if ( SQLITE_ROW == sqlite3_step( stmt ) ) {
						const size_t numBytes = static_cast<size_t>( sqlite3_column_bytes( stmt, 0 /*columnIndex*/ ) );
						if ( numBytes > 0 ) {
							const BYTE* bytes = static_cast<const BYTE*>( sqlite3_column_blob( stmt, 0 /*columnIndex*/ ) );
							if ( nullptr != bytes ) {
								result.resize( numBytes );
								memcpy( &result[ 0 ], bytes, numBytes );
							}
						}
					}
				}
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
	}
	return result;
}

std::wstring Library::AddArtwork( const std::vector<BYTE>& image )
{
	std::wstring artworkID;
	if ( !image.empty() ) {
		const std::string encodedImage = ConvertImage( image );
		if ( !encodedImage.empty() ) {
			std::vector<BYTE> convertedImage = Base64Decode( encodedImage );
			artworkID = FindArtwork( convertedImage );
			if ( artworkID.empty() ) {
				artworkID = UTF8ToWideString( GenerateGUIDString() );
				if ( !AddArtwork( artworkID, convertedImage ) ) {			
					artworkID.clear();
				}
			}
		}
	}
	return artworkID;
}

std::set<std::wstring> Library::GetArtists()
{
	std::set<std::wstring> artists;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT Artist FROM Media UNION SELECT Artist FROM Cues;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) ); nullptr != text ) {
					const std::wstring artist = UTF8ToWideString( text );
					if ( !artist.empty() ) {
						artists.insert( artist );
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return artists;
}

std::set<std::wstring> Library::GetAlbums()
{
	std::set<std::wstring> albums;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT Album FROM Media UNION SELECT Album FROM Cues;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) ); nullptr != text ) {
					const std::wstring album = UTF8ToWideString( text );
					if ( !album.empty() ) {
						albums.insert( album );
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return albums;
}

std::set<std::wstring> Library::GetAlbums( const std::wstring artist )
{
	std::set<std::wstring> albums;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT Album FROM Media WHERE Artist=?1 UNION SELECT Album FROM Cues WHERE Artist=?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) ); nullptr != text ) {
						const std::wstring album = UTF8ToWideString( text );
						if ( !album.empty() ) {
							albums.insert( album );
						}
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return albums;
}

std::set<std::wstring> Library::GetGenres()
{
	std::set<std::wstring> genres;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT Genre FROM Media UNION SELECT Genre FROM Cues;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				if ( const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) ); nullptr != text ) {
					const std::wstring genre = UTF8ToWideString( text );
					if ( !genre.empty() ) {
						genres.insert( genre );
					}
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return genres;
}

std::set<long> Library::GetYears()
{
	std::set<long> years;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT Year FROM Media UNION SELECT Year FROM Cues;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const long year = static_cast<long>( sqlite3_column_int( stmt, 0 /*columnIndex*/ ) );
				if ( ( year >= MINYEAR ) && ( year <= MAXYEAR ) ) { 
					years.insert( year );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return years;
}

MediaInfo::List Library::GetMediaByArtist( const std::wstring& artist )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT " + m_MediaFields + " FROM Media WHERE Artist=?1 UNION SELECT " + m_CueFields + " FROM Cues WHERE Artist=?1 ORDER BY Filename,CueStart COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					MediaInfo mediaInfo;
					ExtractMediaInfo( stmt, mediaInfo );
					mediaList.push_back( mediaInfo );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetMediaByAlbum( const std::wstring& album )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT " + m_MediaFields + " FROM Media WHERE Album=?1 UNION SELECT " + m_CueFields + " FROM Cues WHERE Album=?1 ORDER BY Filename,CueStart COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					MediaInfo mediaInfo;
					ExtractMediaInfo( stmt, mediaInfo );
					mediaList.push_back( mediaInfo );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetMediaByArtistAndAlbum( const std::wstring& artist, const std::wstring& album )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT " + m_MediaFields + " FROM Media WHERE Artist=?1 AND Album=?2 UNION SELECT " + m_CueFields + " FROM Cues WHERE Artist=?1 AND Album=?2 ORDER BY Filename,CueStart COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
					( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					MediaInfo mediaInfo;
					ExtractMediaInfo( stmt, mediaInfo );
					mediaList.push_back( mediaInfo );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetMediaByGenre( const std::wstring& genre )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT " + m_MediaFields + " FROM Media WHERE Genre=?1 UNION SELECT " + m_CueFields + " FROM Cues WHERE Genre=?1 ORDER BY Filename,CueStart COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( genre ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					MediaInfo mediaInfo;
					ExtractMediaInfo( stmt, mediaInfo );
					mediaList.push_back( mediaInfo );
				}
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetMediaByYear( const long year )
{
	MediaInfo::List mediaList;
	if ( ( year >= MINYEAR ) && ( year <= MAXYEAR ) ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
		  const std::string query = "SELECT " + m_MediaFields + " FROM Media WHERE Year=?1 UNION SELECT " + m_CueFields + " FROM Cues WHERE Year=?1 ORDER BY Filename,CueStart COLLATE NOCASE;";
			sqlite3_stmt* stmt = nullptr;
			if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
				if ( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( year ) ) ) {
					while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
						MediaInfo mediaInfo;
						ExtractMediaInfo( stmt, mediaInfo );
						mediaList.push_back( mediaInfo );
					}
				}
				sqlite3_finalize( stmt );
				stmt = nullptr;
			}
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetAllMedia()
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT " + m_MediaFields + " FROM Media UNION SELECT " + m_CueFields + " FROM Cues ORDER BY Filename,CueStart COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				MediaInfo mediaInfo;
				ExtractMediaInfo( stmt, mediaInfo );
				mediaList.push_back( mediaInfo );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

MediaInfo::List Library::GetStreams()
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT * FROM Media WHERE Filename LIKE 'http:%' OR Filename LIKE 'https:%' OR Filename LIKE 'ftp:%' ORDER BY Filename COLLATE NOCASE;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				MediaInfo mediaInfo;
				ExtractMediaInfo( stmt, mediaInfo );
				mediaList.push_back( mediaInfo );
			}
			sqlite3_finalize( stmt );
			stmt = nullptr;
		}
	}
	return mediaList;
}

bool Library::GetArtistExists( const std::wstring& artist )
{
	bool exists = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS(SELECT 1 FROM Media WHERE Artist=?1) UNION SELECT 1 FROM Cues WHERE EXISTS(SELECT 1 FROM Cues WHERE Artist=?1);";
		sqlite3_stmt* stmt = nullptr;
		exists = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_ROW == sqlite3_step( stmt ) );
		sqlite3_finalize( stmt );
	}
	return exists;
}

bool Library::GetAlbumExists( const std::wstring& album )
{
	bool exists = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS(SELECT 1 FROM Media WHERE Album=?1) UNION SELECT 1 FROM Cues WHERE EXISTS(SELECT 1 FROM Cues WHERE Album=?1);";
		sqlite3_stmt* stmt = nullptr;
		exists = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_ROW == sqlite3_step( stmt ) );
		sqlite3_finalize( stmt );
	}
	return exists;
}

bool Library::GetArtistAndAlbumExists( const std::wstring& artist, const std::wstring& album )
{
	bool exists = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS(SELECT 1 FROM Media WHERE Artist=?1 AND Album=?2) UNION SELECT 1 FROM Cues WHERE EXISTS(SELECT 1 FROM Cues WHERE Artist=?1 AND Album=?2);";
		sqlite3_stmt* stmt = nullptr;
		exists = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_ROW == sqlite3_step( stmt ) );
		sqlite3_finalize( stmt );
	}
	return exists;
}

bool Library::GetGenreExists( const std::wstring& genre )
{
	bool exists = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS(SELECT 1 FROM Media WHERE Genre=?1) UNION SELECT 1 FROM Cues WHERE EXISTS(SELECT 1 FROM Cues WHERE Genre=?1);";
		sqlite3_stmt* stmt = nullptr;
		exists = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( genre ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_ROW == sqlite3_step( stmt ) );
		sqlite3_finalize( stmt );
	}
	return exists;
}

bool Library::GetYearExists( const long year )
{
	bool exists = false;
	if ( ( year >= MINYEAR ) && ( year <= MAXYEAR ) ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			const std::string query = "SELECT 1 FROM Media WHERE EXISTS(SELECT 1 FROM Media WHERE Year=?1) UNION SELECT 1 FROM Cues WHERE EXISTS(SELECT 1 FROM Cues WHERE Year=?1);";
			sqlite3_stmt* stmt = nullptr;
			exists = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
					( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( year ) ) ) &&
					( SQLITE_ROW == sqlite3_step( stmt ) );
			sqlite3_finalize( stmt );
		}
	}
	return exists;
}

bool Library::RemoveFromLibrary( const MediaInfo& mediaInfo )
{
	bool removed = false;
	sqlite3* database = m_Database.GetDatabase();
	const std::wstring& filename = mediaInfo.GetFilename();
	if ( ( nullptr != database ) && !filename.empty() && ( MediaInfo::Source::File == mediaInfo.GetSource() ) ) {
		const std::string query = mediaInfo.GetCueStart() ? "DELETE FROM Cues WHERE Filename=?1 AND CueStart=?2 AND CueEnd=?3;" : "DELETE FROM Media WHERE Filename=?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
      bool ok = false;
      if ( mediaInfo.GetCueStart() ) {
        ok = ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
          ( SQLITE_OK == sqlite3_bind_int64( stmt, 2 /*param*/, *mediaInfo.GetCueStart() ) ) && ( SQLITE_OK == sqlite3_bind_int64( stmt, 3 /*param*/, mediaInfo.GetCueEnd().value_or( -1 ) ) );
      } else {
        ok = ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) );
      }
			if ( ok ) {
				// Should be a maximum of one entry.
				removed = ( SQLITE_DONE == sqlite3_step( stmt ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return removed;
}

const Library::Columns& Library::GetColumns( const MediaInfo& mediaInfo ) const
{
	const Columns& columns = ( MediaInfo::Source::CDDA == mediaInfo.GetSource() ) ? m_CDDAColumns : ( mediaInfo.GetCueStart() ? m_CueColumns : m_MediaColumns );
	return columns;
}

void Library::UpdateMediaInfoFromTags( MediaInfo& mediaInfo, const Tags& tags )
{
	for ( const auto& iter : tags ) {
    const auto tagType = iter.first;
    if ( mediaInfo.GetCueStart() && ( Tag::Version != tagType ) && ( Tag::Artwork != tagType ) )
      continue;
    
    const std::wstring value = UTF8ToWideString( iter.second );
		switch ( tagType ) {
			case Tag::Album : {
				mediaInfo.SetAlbum( value );
				break;
			}
			case Tag::Artist : {
				mediaInfo.SetArtist( value );
				break;
			}
			case Tag::Comment : {
				mediaInfo.SetComment( value );
				break;
			}
			case Tag::Version : {
				mediaInfo.SetVersion( value );
				break;
			}
			case Tag::Genre : {
				mediaInfo.SetGenre( value );
				break;
			}
			case Tag::Title : {
				mediaInfo.SetTitle( value );
				break;
			}
      case Tag::Composer : {
        mediaInfo.SetComposer( value );
        break;
      }
      case Tag::Conductor : {
        mediaInfo.SetConductor( value );
        break;
      }
      case Tag::Publisher : {
        mediaInfo.SetPublisher( value );
        break;
      }
			case Tag::GainAlbum : {
				try {
					mediaInfo.SetGainAlbum( std::stof( iter.second ) );
				} catch ( const std::logic_error& ) {
				}
				break;
			}
			case Tag::GainTrack : {
				try {
					mediaInfo.SetGainTrack( std::stof( iter.second ) );
				} catch ( const std::logic_error& ) {
				}
				break;
			}
			case Tag::Track : {
				try {
					mediaInfo.SetTrack( std::stol( value ) );
				} catch ( const std::logic_error& ) {
				}
				break;
			}
			case Tag::Year : {
				try {
					mediaInfo.SetYear( std::stol( value ) );
				} catch ( const std::logic_error& ) {
				}
				break;
			}
			case Tag::Artwork : {
				const std::vector<BYTE> image = Base64Decode( iter.second );
				if ( !image.empty() ) {
					std::wstring artworkID = FindArtwork( image );
					if ( !artworkID.empty() ) {
						mediaInfo.SetArtworkID( artworkID );
					} else {
						artworkID = UTF8ToWideString( GenerateGUIDString() );
						if ( AddArtwork( artworkID, image ) ) {			
							mediaInfo.SetArtworkID( artworkID );
						}
					}
				}
				break;
			}
			default : {
				break;
			}
		}
	}
}

std::set<std::wstring> Library::GetAllSupportedFileExtensions() const
{
	const std::set<std::wstring> fileExtensions = m_Handlers.GetAllSupportedFileExtensions();
	return fileExtensions;
}

Tags Library::GetTags( const MediaInfo& mediaInfo )
{
	Tags tags = mediaInfo;
  if ( const std::vector<BYTE> imageBytes = GetMediaArtwork( mediaInfo ); !imageBytes.empty() ) {
		if ( const std::string encodedArtwork = Base64Encode( imageBytes.data(), static_cast<int>( imageBytes.size() ) ); !encodedArtwork.empty() ) {
		  tags.insert( Tags::value_type( Tag::Artwork, encodedArtwork ) );
    }
	}
	return tags;
}

bool Library::UpdateTrackGain( const MediaInfo& previousInfo, const MediaInfo& updatedInfo, const bool sendNotification )
{
	bool updated = false;
	if ( previousInfo.GetGainTrack() != updatedInfo.GetGainTrack() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			std::string query;
      if ( MediaInfo::Source::CDDA == updatedInfo.GetSource() ) {
        query = "UPDATE CDDA SET GainTrack=?1 WHERE CDDB=?2 AND Track=?3;";
      } else {
        if ( updatedInfo.GetCueStart() ) {
				  query = "UPDATE Cues SET GainTrack=?1 WHERE Filename=?2 AND CueStart=?3 AND CueEnd=?4;";
        } else {
				  query = "UPDATE Media SET GainTrack=?1 WHERE Filename=?2;";
        }
      }
			sqlite3_stmt* stmt = nullptr;
			updated = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) );
			if ( updated ) {
				const auto gain = updatedInfo.GetGainTrack();
				updated = gain.has_value() ? ( SQLITE_OK == sqlite3_bind_double( stmt, 1 /*param*/, gain.value() ) ) : ( SQLITE_OK == sqlite3_bind_null( stmt, 1 /*param*/ ) );
				if ( updated ) {
					if ( MediaInfo::Source::CDDA == updatedInfo.GetSource() ) {
						updated = ( ( SQLITE_OK == sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( updatedInfo.GetCDDB() ) ) ) &&
							( SQLITE_OK == sqlite3_bind_int( stmt, 3 /*param*/, static_cast<int>( updatedInfo.GetTrack() ) ) ) );
					} else {
            if ( updatedInfo.GetCueStart() ) {
						  updated = ( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( updatedInfo.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
                ( SQLITE_OK == sqlite3_bind_int64( stmt, 3 /*param*/, *updatedInfo.GetCueStart() ) ) && ( SQLITE_OK == sqlite3_bind_int64( stmt, 4 /*param*/, updatedInfo.GetCueEnd().value_or( -1 ) ) );
            } else {
						  updated = ( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( updatedInfo.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) );
            }
					}
					if ( updated ) {
						updated = ( SQLITE_DONE == sqlite3_step( stmt ) );
					}
				}
				sqlite3_finalize( stmt );
			}
		}
	}
	if ( updated && sendNotification ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			vuplayer->OnMediaUpdated( previousInfo, updatedInfo );
		}
	}
	return updated;
}

void Library::UpdateMediaInfoFromDecoder( MediaInfo& mediaInfo, const Decoder& decoder, const bool sendNotification )
{
	MediaInfo originalInfo( mediaInfo );
	GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ );
	mediaInfo.SetChannels( decoder.GetChannels() );
	mediaInfo.SetBitsPerSample( decoder.GetBPS() );
	mediaInfo.SetDuration( decoder.GetDuration() );
	mediaInfo.SetSampleRate( decoder.GetSampleRate() );
	mediaInfo.SetBitrate( decoder.GetBitrate() );

	bool updated = 
		( mediaInfo.GetChannels() != originalInfo.GetChannels() ) || 
		( mediaInfo.GetBitsPerSample().value_or( 0 ) != originalInfo.GetBitsPerSample().value_or( 0 ) ) ||
		( mediaInfo.GetDuration() != originalInfo.GetDuration() ) ||
		( mediaInfo.GetSampleRate() != originalInfo.GetSampleRate() ) ||	
		!AreRoughlyEqual( mediaInfo.GetBitrate().value_or( 0 ), originalInfo.GetBitrate().value_or( 0 ), 0.5f );

	if ( updated ) {
		updated = UpdateMediaLibrary( mediaInfo );
		if ( updated && sendNotification ) {
			VUPlayer* vuplayer = VUPlayer::Get();
			if ( nullptr != vuplayer ) {
				vuplayer->OnMediaUpdated( originalInfo, mediaInfo );
			}
		}
	}
}

void Library::SetRecentlyWrittenTag( const std::wstring& filename )
{
	std::lock_guard<std::mutex> lock( m_TagsWrittenMutex );
	m_LastTagWriteTime = GetCurrentTimestamp();
	m_TagsWritten[ filename ] = m_LastTagWriteTime;
}

bool Library::HasRecentlyWrittenTag( const std::wstring& filename ) const
{
	constexpr long long kRecentTimeSpan = 5 /*sec*/ * 10000000ll /*100-nanosecond intervals*/;

	bool recentTagWrite = false;
	std::lock_guard<std::mutex> lock( m_TagsWrittenMutex );
	if ( !m_TagsWritten.empty() ) {
		const long long current = GetCurrentTimestamp();
		if ( ( current - m_LastTagWriteTime ) < kRecentTimeSpan ) {
			if ( const auto iter = m_TagsWritten.find( filename ); m_TagsWritten.end() != iter ) {
				const long long lastWrite = iter->second;
				recentTagWrite = ( current - lastWrite ) < kRecentTimeSpan;
			}
		} else {
			m_TagsWritten.clear();
		}
	}
	return recentTagWrite;
}
