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
		Columns::value_type( "Bitrate", Column::Bitrate )
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
		Columns::value_type( "Artwork", Column::Artwork )
	} )
{
	UpdateDatabase();
}

Library::~Library()
{
	for ( const auto& tagIter : m_PendingTags ) {
		if ( m_Handlers.SetTags( tagIter.first /*filename*/, tagIter.second /*tags*/ ) ) {
			MediaInfo mediaInfo( tagIter.first );
			GetMediaInfo( mediaInfo );
		}
	}
}

void Library::UpdateDatabase()
{
	UpdateMediaTable();
	UpdateCDDATable();
	UpdateArtworkTable();
	CreateIndices();
}

void Library::UpdateMediaTable()
{
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		// Create the media table (if necessary).
		std::string createTableQuery = "CREATE TABLE IF NOT EXISTS Media(";
		for ( const auto& iter : m_MediaColumns ) {
			const std::string& columnName = iter.first;
			createTableQuery += columnName + ",";
		}
		createTableQuery += "PRIMARY KEY(Filename ASC));";
		sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );

		// Check the columns in the media table.
		const std::string tableInfoQuery = "PRAGMA table_info('Media')";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, tableInfoQuery.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			Columns missingColumns( m_MediaColumns );
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const int columnCount = sqlite3_column_count( stmt );
				for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
					const std::string columnName = sqlite3_column_name( stmt, columnIndex );
					if ( columnName == "name" ) {
						const std::string name = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						missingColumns.erase( name );
						break;
					}
				}
			}
			sqlite3_finalize( stmt );

			if ( !missingColumns.empty() ) {
				if ( missingColumns.find( "Filename" ) != missingColumns.end() ) {
					// No primary index, just drop the table and recreate.
					const std::string dropTableQuery = "DROP TABLE Media;";
					sqlite3_exec( database, dropTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
					sqlite3_exec( database, createTableQuery.c_str(), NULL /*callback*/, NULL /*arg*/, NULL /*errMsg*/ );
				} else {
					for ( const auto& iter : missingColumns ) {
						std::string addColumnQuery = "ALTER TABLE Media ADD COLUMN ";
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
						const std::string name = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						missingColumns.erase( name );
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
						const std::string name = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						columns.insert( name );
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
		const std::string query = ( MediaInfo::Source::CDDA == info.GetSource() ) ? "SELECT * FROM CDDA WHERE CDDB=?1 AND Track=?2;" : "SELECT * FROM Media WHERE Filename=?1;";
		sqlite3_stmt* stmt = nullptr;
		success = ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) );
		if ( success ) {
			success = ( MediaInfo::Source::CDDA == mediaInfo.GetSource() ) ?
				( ( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( info.GetCDDB() ) ) ) && ( SQLITE_OK == sqlite3_bind_int( stmt, 2 /*param*/, static_cast<int>( info.GetTrack() ) ) ) ) :
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( info.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) );
			if ( success ) {
				// Should be a maximum of one entry.
				const int result = sqlite3_step( stmt );
				success = ( SQLITE_ROW == result );
				if ( success ) {
					ExtractMediaInfo( stmt, info );
					if ( checkFileAttributes ) {
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
					success = GetDecoderInfo( info );
					if ( success ) {
						Tags pendingTags;
						if ( GetPendingTags( info.GetFilename(), pendingTags ) ) {
							UpdateMediaInfoFromTags( info, pendingTags );
						}

						success = UpdateMediaLibrary( info );
						if ( success && sendNotification ) {
							VUPlayer* vuplayer = VUPlayer::Get();
							if ( nullptr != vuplayer ) {
								vuplayer->OnMediaUpdated( mediaInfo /*previousInfo*/, info /*updatedInfo*/ );
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
			if ( FALSE != GetFileSizeEx( handle, &size ) ) {
				fileSize = static_cast<long long>( size.QuadPart );
			}
			FILETIME lastWriteTime;
			if ( FALSE != GetFileTime( handle, NULL /*creationTime*/, NULL /*lastAccessTime*/, &lastWriteTime ) ) {
				lastModified = ( static_cast<long long>( lastWriteTime.dwHighDateTime ) << 32 ) + lastWriteTime.dwLowDateTime;
			}
			CloseHandle( handle );
			success = true;
		}
	}
	return success;
}

bool Library::GetDecoderInfo( MediaInfo& mediaInfo )
{
	bool success = false;
	Decoder::Ptr stream = m_Handlers.OpenDecoder( mediaInfo.GetFilename() );
	if ( stream ) {
		mediaInfo.SetBitsPerSample( stream->GetBPS() );
		mediaInfo.SetChannels( stream->GetChannels() );
		mediaInfo.SetDuration( stream->GetDuration() );
		mediaInfo.SetSampleRate( stream->GetSampleRate() );
		mediaInfo.SetBitrate( stream->GetBitrate() );

		Tags tags;
		if ( m_Handlers.GetTags( mediaInfo.GetFilename(), tags ) ) {
			UpdateMediaInfoFromTags( mediaInfo, tags );
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

void Library::ExtractMediaInfo( sqlite3_stmt* stmt, MediaInfo& mediaInfo )
{
	if ( nullptr != stmt ) {
		const int columnCount = sqlite3_column_count( stmt );
		const Columns& columns = GetColumns( mediaInfo.GetSource() );
		for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
			const auto columnIter = columns.find( sqlite3_column_name( stmt, columnIndex ) );
			if ( columnIter != columns.end() ) {
				switch ( columnIter->second ) {
					case Column::Filename : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
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
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetArtist( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Title : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetTitle( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Album : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetAlbum( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Genre : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetGenre( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Year : {						
						mediaInfo.SetYear( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
						break;
					}
					case Column::Comment : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetComment( UTF8ToWideString( text ) );
						}
						break;
					}
					case Column::Track : {						
						mediaInfo.SetTrack( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
						break;
					}
					case Column::Version : {						
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
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
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
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
				}
			}
		}
	}
}

bool Library::UpdateMediaLibrary( const MediaInfo& mediaInfo )
{
	bool success = false;

	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {

		const Columns& columnMap = GetColumns( mediaInfo.GetSource() );
		const std::string tableName = ( MediaInfo::Source::CDDA == mediaInfo.GetSource() ) ? "CDDA" : "Media";

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
						sqlite3_bind_double( stmt, ++param, mediaInfo.GetDuration() );
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
	Tags tags;
	if ( previousMediaInfo.GetAlbum() != updatedMediaInfo.GetAlbum() ) {
		tags.insert( Tags::value_type( Tag::Album, WideStringToUTF8( updatedMediaInfo.GetAlbum() ) ) );	
	}
	if ( previousMediaInfo.GetArtist() != updatedMediaInfo.GetArtist() ) {
		tags.insert( Tags::value_type( Tag::Artist, WideStringToUTF8( updatedMediaInfo.GetArtist() ) ) );
	}
	if ( previousMediaInfo.GetComment() != updatedMediaInfo.GetComment() ) {
		tags.insert( Tags::value_type( Tag::Comment, WideStringToUTF8( updatedMediaInfo.GetComment() ) ) );
	}
	if ( previousMediaInfo.GetGenre() != updatedMediaInfo.GetGenre() ) {
		tags.insert( Tags::value_type( Tag::Genre, WideStringToUTF8( updatedMediaInfo.GetGenre() ) ) );		
	}
	if ( previousMediaInfo.GetTitle() != updatedMediaInfo.GetTitle() ) {
		tags.insert( Tags::value_type( Tag::Title, WideStringToUTF8( updatedMediaInfo.GetTitle() ) ) );
	}
	if ( previousMediaInfo.GetTrack() != updatedMediaInfo.GetTrack() ) {
		const std::string trackStr = ( updatedMediaInfo.GetTrack() > 0 ) ? std::to_string( updatedMediaInfo.GetTrack() ) : std::string();
		tags.insert( Tags::value_type( Tag::Track, trackStr ) );
	}
	if ( previousMediaInfo.GetYear() != updatedMediaInfo.GetYear() ) {
		const std::string yearStr = ( updatedMediaInfo.GetYear() > 0 ) ? std::to_string( updatedMediaInfo.GetYear() ) : std::string();
		tags.insert( Tags::value_type( Tag::Year, yearStr ) );
	}
	if ( previousMediaInfo.GetGainAlbum() != updatedMediaInfo.GetGainAlbum() ) {
		const auto previousGain = previousMediaInfo.GetGainAlbum();
		const auto updatedGain = updatedMediaInfo.GetGainAlbum();
		if ( updatedGain.has_value() ) {
			const std::string previousGainStr = GainToString( previousGain );
			const std::string updatedGainStr = GainToString( updatedGain );
			if ( previousGainStr != updatedGainStr ) {
				tags.insert( Tags::value_type( Tag::GainAlbum, updatedGainStr ) );
			}		
		} else {
			tags.insert( Tags::value_type( Tag::GainAlbum, std::string() ) );
		}
	}
	if ( previousMediaInfo.GetGainTrack() != updatedMediaInfo.GetGainTrack() ) {
		const auto previousGain = previousMediaInfo.GetGainTrack();
		const auto updatedGain = updatedMediaInfo.GetGainTrack();
		if ( updatedGain.has_value() ) {
			const std::string previousGainStr = GainToString( previousGain );
			const std::string updatedGainStr = GainToString( updatedGain );
			if ( previousGainStr != updatedGainStr ) {
				tags.insert( Tags::value_type( Tag::GainTrack, updatedGainStr ) );
			}
		} else {
			tags.insert( Tags::value_type( Tag::GainTrack, std::string() ) );
		}
	}
	if ( previousMediaInfo.GetArtworkID() != updatedMediaInfo.GetArtworkID() ) {
		std::vector<BYTE> imageBytes = GetMediaArtwork( updatedMediaInfo );
		if ( imageBytes.empty() ) {
			tags.insert( Tags::value_type( Tag::Artwork, std::string() ) );
		} else {
			const std::string encodedArtwork = Base64Encode( &imageBytes[ 0 ], static_cast<int>( imageBytes.size() ) );
			tags.insert( Tags::value_type( Tag::Artwork, encodedArtwork ) );
		}	
	}

	if ( !tags.empty() ) {
		MediaInfo mediaInfo( updatedMediaInfo );
		WriteFileTags( mediaInfo, tags );

		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			vuplayer->OnMediaUpdated( previousMediaInfo, updatedMediaInfo );
		}
	}
}

void Library::WriteFileTags( MediaInfo& mediaInfo, const Tags& tags )
{
	if ( MediaInfo::Source::File == mediaInfo.GetSource() ) {
		
		Tags allTags;
		const bool hasPendingTags = GetPendingTags( mediaInfo.GetFilename(), allTags );
		if ( hasPendingTags ) {
			for ( const auto& tag : tags ) {
				allTags[ tag.first ] = tag.second;
			}
		} else {
			allTags = tags;
		}

		if ( m_Handlers.SetTags( mediaInfo.GetFilename(), allTags ) ) {
			m_PendingTags.erase( mediaInfo.GetFilename() );
			GetDecoderInfo( mediaInfo );
		} else {
			AddPendingTags( mediaInfo.GetFilename(), tags );
		}
	}
	UpdateMediaLibrary( mediaInfo );
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
								const char* id = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
								if ( nullptr != id ) {
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
		const std::string query = "SELECT DISTINCT Artist FROM Media;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
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
		const std::string query = "SELECT DISTINCT Album FROM Media;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
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
		const std::string query = "SELECT Album FROM Media WHERE Artist=?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
					if ( nullptr != text ) {
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
		const std::string query = "SELECT DISTINCT Genre FROM Media;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
				const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, 0 /*columnIndex*/ ) );
				if ( nullptr != text ) {
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
		const std::string query = "SELECT DISTINCT Year FROM Media;";
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
		const std::string query = "SELECT * FROM Media WHERE Artist=?1 ORDER BY Filename;";
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
		const std::string query = "SELECT * FROM Media WHERE Album=?1 ORDER BY Filename;";
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
		const std::string query = "SELECT * FROM Media WHERE Artist=?1 AND Album=?2 ORDER BY Filename;";
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
		const std::string query = "SELECT * FROM Media WHERE Genre=?1 ORDER BY Filename;";
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
			const std::string query = "SELECT * FROM Media WHERE Year=?1 ORDER BY Filename;";
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
		const std::string query = "SELECT * FROM Media ORDER BY Filename;";
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
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS( SELECT 1 FROM Media WHERE Artist=?1 );";
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
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS( SELECT 1 FROM Media WHERE Album=?1 );";
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
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS( SELECT 1 FROM Media WHERE Artist=?1 AND Album=?2 );";
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
		const std::string query = "SELECT 1 FROM Media WHERE EXISTS( SELECT 1 FROM Media WHERE Genre=?1 );";
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
			const std::string query = "SELECT 1 FROM Media WHERE EXISTS( SELECT 1 FROM Media WHERE Year=?1 );";
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
		const std::string query = "DELETE FROM Media WHERE Filename=?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			if ( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( filename ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) {
				// Should be a maximum of one entry.
				removed = ( SQLITE_DONE == sqlite3_step( stmt ) );
			}
			sqlite3_finalize( stmt );
		}
	}
	return removed;
}

const Library::Columns& Library::GetColumns( const MediaInfo::Source source ) const
{
	const Columns& columns = ( MediaInfo::Source::CDDA == source ) ? m_CDDAColumns : m_MediaColumns;
	return columns;
}

void Library::UpdateMediaInfoFromTags( MediaInfo& mediaInfo, const Tags& tags )
{
	for ( const auto& iter : tags ) {
		const std::wstring value = UTF8ToWideString( iter.second );
		switch ( iter.first ) {
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
			case Tag::GainAlbum : {
				try {
					mediaInfo.SetGainAlbum( std::stof( iter.second ) );
				} catch ( ... ) {
				}
				break;
			}
			case Tag::GainTrack : {
				try {
					mediaInfo.SetGainTrack( std::stof( iter.second ) );
				} catch ( ... ) {
				}
				break;
			}
			case Tag::Track : {
				try {
					mediaInfo.SetTrack( std::stol( value ) );
				} catch ( ... ) {
				}
				break;
			}
			case Tag::Year : {
				try {
					mediaInfo.SetYear( std::stol( value ) );
				} catch ( ... ) {
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

void Library::AddPendingTags( const std::wstring& filename, const Tags& tags )
{
	auto tagIter = m_PendingTags.find( filename );
	if ( m_PendingTags.end() != tagIter ) {
		Tags& pendingTags = tagIter->second;
		for ( const auto& tag : tags ) {
			pendingTags[ tag.first ] = tag.second;
		}
	} else {
		m_PendingTags.insert( FileTags::value_type( filename, tags ) );
	}
}

bool Library::GetPendingTags( const std::wstring& filename, Tags& tags ) const
{
	tags.clear();
	const auto tagIter = m_PendingTags.find( filename );
	if ( m_PendingTags.end() != tagIter ) {
		tags = tagIter->second;
	}
	const bool anyPending = !tags.empty();
	return anyPending;
}

std::set<std::wstring> Library::GetAllSupportedFileExtensions() const
{
	const std::set<std::wstring> fileExtensions = m_Handlers.GetAllSupportedFileExtensions();
	return fileExtensions;
}

Tags Library::GetTags( const MediaInfo& mediaInfo )
{
	Tags tags = static_cast<Tags>( mediaInfo );
	const std::vector<BYTE> imageBytes = GetMediaArtwork( mediaInfo );
	if ( !imageBytes.empty() ) {
		const std::string encodedArtwork = Base64Encode( &imageBytes[ 0 ], static_cast<int>( imageBytes.size() ) );
		tags.insert( Tags::value_type( Tag::Artwork, encodedArtwork ) );
	}
	return tags;
}

bool Library::UpdateTrackGain( const MediaInfo& previousInfo, const MediaInfo& updatedInfo, const bool sendNotification )
{
	bool updated = false;
	if ( previousInfo.GetGainTrack() != updatedInfo.GetGainTrack() ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			const std::string query = ( MediaInfo::Source::CDDA == updatedInfo.GetSource() ) ?
				"UPDATE CDDA SET GainTrack=?1 WHERE CDDB=?2 AND Track=?3;" :
				"UPDATE Media SET GainTrack=?1 WHERE Filename=?2;";
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
						updated = ( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( updatedInfo.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) );
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
