#include "Library.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <iomanip>
#include <list>
#include <fstream>
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
		Columns::value_type( "PeakTrack", Column::PeakTrack ),
		Columns::value_type( "PeakAlbum", Column::PeakAlbum ),
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
	UpdateArtworkTable();
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

bool Library::GetMediaInfo( MediaInfo& mediaInfo, const bool checkFileAttributes, const bool scanMedia )
{
	bool success = false;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		MediaInfo info( mediaInfo );
		const std::string query = "SELECT * FROM Media WHERE Filename = ?1;";
		sqlite3_stmt* stmt = nullptr;
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( info.GetFilename() ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
			// Should be a maximum of one entry.
			const int result = sqlite3_step( stmt );
			if ( SQLITE_ROW == result ) {
				ExtractMediaInfo( stmt, info );
				if ( checkFileAttributes ) {
					long long filetime = 0;
					long long filesize = 0;
					GetFileInfo( info.GetFilename(), filetime, filesize );
					success = ( info.GetFiletime() == filetime ) && ( info.GetFilesize() == filesize );
				} else {
					success = true;
				}
			}

			sqlite3_finalize( stmt );

			if ( !success && scanMedia ) {
				success = GetDecoderInfo( info );
				if ( success ) {
					success = UpdateMediaLibrary( info );
					if ( success ) {
						VUPlayer* vuplayer = VUPlayer::Get();
						if ( nullptr != vuplayer ) {
							vuplayer->OnMediaUpdated( mediaInfo /*previousInfo*/, info /*updatedInfo*/ );
						}
					}
				}
			}

			if ( success ) {
				mediaInfo = info;
			}
		}
	}
	return success;
}

bool Library::GetFileInfo( const std::wstring& filename, long long& lastModified, long long& fileSize ) const
{
	bool success = false;
	fileSize = 0;
	lastModified = 0;
	const HANDLE handle = CreateFile( filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL /*security*/, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*template*/ );
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

		Handler::Tags tags;
		if ( m_Handlers.GetTags( mediaInfo.GetFilename(), tags ) ) {
			for ( const auto& iter : tags ) {
				switch ( iter.first ) {
					case Handler::Tag::Album : {
						mediaInfo.SetAlbum( iter.second );
						break;
					}
					case Handler::Tag::Artist : {
						mediaInfo.SetArtist( iter.second );
						break;
					}
					case Handler::Tag::Comment : {
						mediaInfo.SetComment( iter.second );
						break;
					}
					case Handler::Tag::Version : {
						mediaInfo.SetVersion( iter.second );
						break;
					}
					case Handler::Tag::Genre : {
						mediaInfo.SetGenre( iter.second );
						break;
					}
					case Handler::Tag::Title : {
						mediaInfo.SetTitle( iter.second );
						break;
					}
					case Handler::Tag::GainAlbum : {
						try {
							mediaInfo.SetGainAlbum( std::stof( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::GainTrack : {
						try {
							mediaInfo.SetGainTrack( std::stof( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::PeakAlbum : {
						try {
							mediaInfo.SetPeakAlbum( std::stof( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::PeakTrack : {
						try {
							mediaInfo.SetPeakTrack( std::stof( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::Track : {
						try {
							mediaInfo.SetTrack( std::stol( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::Year : {
						try {
							mediaInfo.SetYear( std::stol( iter.second ) );
						} catch ( ... ) {
						}
						break;
					}
					case Handler::Tag::Artwork : {
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
		for ( int columnIndex = 0; columnIndex < columnCount; columnIndex++ ) {
			const auto columnIter = m_MediaColumns.find( sqlite3_column_name( stmt, columnIndex ) );
			if ( columnIter != m_MediaColumns.end() ) {
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
						mediaInfo.SetBitsPerSample( static_cast<long>( sqlite3_column_int( stmt, columnIndex ) ) );
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
					case Column::PeakTrack : {						
						mediaInfo.SetPeakTrack( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );
						break;
					}
					case Column::PeakAlbum : {						
						mediaInfo.SetPeakAlbum( static_cast<float>( sqlite3_column_double( stmt, columnIndex ) ) );
						break;
					}
					case Column::Artwork : {
						const char* text = reinterpret_cast<const char*>( sqlite3_column_text( stmt, columnIndex ) );
						if ( nullptr != text ) {
							mediaInfo.SetArtworkID( UTF8ToWideString( text ) );
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
		std::string columns = " (";
		std::string values = " VALUES (";
		int param = 0;
		for ( const auto& iter : m_MediaColumns ) {
			columns += iter.first + ",";
			values += "?" + std::to_string( ++param ) + ",";
		}
		columns.back() = ')';
		values.back() = ')';
		const std::string query = "REPLACE INTO Media" + columns + values + ";";
		sqlite3_stmt* stmt = nullptr;
		if ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) {
			param = 0;
			for ( const auto& iter : m_MediaColumns ) {
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
						sqlite3_bind_int( stmt, ++param, static_cast<int>( mediaInfo.GetBitsPerSample() ) );
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
						const float gain = mediaInfo.GetGainAlbum();
						if ( REPLAYGAIN_NOVALUE == gain ) {
							sqlite3_bind_null( stmt, ++param );
						} else {
							sqlite3_bind_double( stmt, ++param, gain );
						}
						break;
					}
					case Column::GainTrack : {
						const float gain = mediaInfo.GetGainTrack();
						if ( REPLAYGAIN_NOVALUE == gain ) {
							sqlite3_bind_null( stmt, ++param );
						} else {
							sqlite3_bind_double( stmt, ++param, gain );
						}
						break;
					}
					case Column::PeakAlbum : {
						const float peak = mediaInfo.GetPeakAlbum();
						if ( REPLAYGAIN_NOVALUE == peak ) {
							sqlite3_bind_null( stmt, ++param );
						} else {
							sqlite3_bind_double( stmt, ++param, peak );
						}
						break;
					}
					case Column::PeakTrack : {
						const float peak = mediaInfo.GetPeakTrack();
						if ( REPLAYGAIN_NOVALUE == peak ) {
							sqlite3_bind_null( stmt, ++param );
						} else {
							sqlite3_bind_double( stmt, ++param, peak );
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
	Handler::Tags tags;
	if ( previousMediaInfo.GetAlbum() != updatedMediaInfo.GetAlbum() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Album, updatedMediaInfo.GetAlbum() ) );	
	}
	if ( previousMediaInfo.GetArtist() != updatedMediaInfo.GetArtist() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Artist, updatedMediaInfo.GetArtist() ) );
	}
	if ( previousMediaInfo.GetComment() != updatedMediaInfo.GetComment() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Comment, updatedMediaInfo.GetComment() ) );
	}
	if ( previousMediaInfo.GetGenre() != updatedMediaInfo.GetGenre() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Genre, updatedMediaInfo.GetGenre() ) );		
	}
	if ( previousMediaInfo.GetTitle() != updatedMediaInfo.GetTitle() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Title, updatedMediaInfo.GetTitle() ) );
	}
	if ( previousMediaInfo.GetTrack() != updatedMediaInfo.GetTrack() ) {
		const std::wstring trackStr = ( updatedMediaInfo.GetTrack() > 0 ) ? std::to_wstring( updatedMediaInfo.GetTrack() ) : std::wstring();
		tags.insert( Handler::Tags::value_type( Handler::Tag::Track, trackStr ) );
	}
	if ( previousMediaInfo.GetYear() != updatedMediaInfo.GetYear() ) {
		const std::wstring yearStr = ( updatedMediaInfo.GetYear() > 0 ) ? std::to_wstring( updatedMediaInfo.GetYear() ) : std::wstring();
		tags.insert( Handler::Tags::value_type( Handler::Tag::Year, yearStr ) );
	}
	if ( previousMediaInfo.GetGainAlbum() != updatedMediaInfo.GetGainAlbum() ) {
		if ( REPLAYGAIN_NOVALUE == updatedMediaInfo.GetGainAlbum() ) {
			tags.insert( Handler::Tags::value_type( Handler::Tag::GainAlbum, std::wstring() ) );
		} else {
			const std::wstring previousGain = GainToString( previousMediaInfo.GetGainAlbum() );
			const std::wstring updatedGain = GainToString( updatedMediaInfo.GetGainAlbum() );
			if ( previousGain != updatedGain ) {
				tags.insert( Handler::Tags::value_type( Handler::Tag::GainAlbum, updatedGain ) );
			}
		}
	}
	if ( previousMediaInfo.GetGainTrack() != updatedMediaInfo.GetGainTrack() ) {
		if ( REPLAYGAIN_NOVALUE == updatedMediaInfo.GetGainTrack() ) {
			tags.insert( Handler::Tags::value_type( Handler::Tag::GainTrack, std::wstring() ) );
		} else {
			const std::wstring previousGain = GainToString( previousMediaInfo.GetGainTrack() );
			const std::wstring updatedGain = GainToString( updatedMediaInfo.GetGainTrack() );
			if ( previousGain != updatedGain ) {
				tags.insert( Handler::Tags::value_type( Handler::Tag::GainTrack, updatedGain ) );
			}
		}
	}
	if ( previousMediaInfo.GetPeakAlbum() != updatedMediaInfo.GetPeakAlbum() ) {
		if ( REPLAYGAIN_NOVALUE == updatedMediaInfo.GetPeakAlbum() ) {
			tags.insert( Handler::Tags::value_type( Handler::Tag::PeakAlbum, std::wstring() ) );
		} else {
			const std::wstring previousPeak = PeakToString( previousMediaInfo.GetPeakAlbum() );
			const std::wstring updatedPeak = PeakToString( updatedMediaInfo.GetPeakAlbum() );
			if ( previousPeak != updatedPeak ) {
				tags.insert( Handler::Tags::value_type( Handler::Tag::PeakAlbum, updatedPeak ) );		
			}
		}
	}
	if ( previousMediaInfo.GetPeakTrack() != updatedMediaInfo.GetPeakTrack() ) {
		if ( REPLAYGAIN_NOVALUE == updatedMediaInfo.GetPeakTrack() ) {
			tags.insert( Handler::Tags::value_type( Handler::Tag::PeakTrack, std::wstring() ) );
		} else {
			const std::wstring previousPeak = PeakToString( previousMediaInfo.GetPeakTrack() );
			const std::wstring updatedPeak = PeakToString( updatedMediaInfo.GetPeakTrack() );
			if ( previousPeak != updatedPeak ) {
				tags.insert( Handler::Tags::value_type( Handler::Tag::PeakTrack, updatedPeak ) );		
			}
		}
	}
	if ( previousMediaInfo.GetArtworkID() != updatedMediaInfo.GetArtworkID() ) {
		std::vector<BYTE> imageBytes = GetMediaArtwork( updatedMediaInfo );
		if ( imageBytes.empty() ) {
			tags.insert( Handler::Tags::value_type( Handler::Tag::Artwork, std::wstring() ) );
		} else {
			const std::wstring encodedArtwork = Base64Encode( &imageBytes[ 0 ], static_cast<int>( imageBytes.size() ) );
			tags.insert( Handler::Tags::value_type( Handler::Tag::Artwork, encodedArtwork ) );
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

void Library::WriteFileTags( MediaInfo& mediaInfo, const Handler::Tags& tags )
{
	if ( m_Handlers.SetTags( mediaInfo.GetFilename(), tags ) ) {
		GetDecoderInfo( mediaInfo );
	} else {
		m_PendingTags.push_back( FileTag( mediaInfo.GetFilename(), tags ) );
	}
	UpdateMediaLibrary( mediaInfo );
}

std::wstring Library::GainToString( const float gain ) const
{
	std::wstringstream ss;
	if ( gain > 0 ) {
		ss << L"+";
	}
	ss << std::fixed << std::setprecision( 2 ) << gain << L" dB";
	return ss.str();
}

std::wstring Library::PeakToString( const float peak ) const
{
	std::wstringstream ss;
	ss << std::fixed << std::setprecision( 6 ) << peak;
	return ss.str();
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
				sqlite3_reset( stmt );
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
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( image.size() ) ) ) ) {
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
			if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
					( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artworkID ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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
		const std::wstring encodedImage = ConvertImage( image );
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
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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

MediaInfo::List Library::GetMediaByAlbum( const std::wstring& album )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT * FROM Media WHERE Album=?1 ORDER BY Filename;";
		sqlite3_stmt* stmt = nullptr;
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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

MediaInfo::List Library::GetMediaByArtistAndAlbum( const std::wstring& artist, const std::wstring& album )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT * FROM Media WHERE Artist=?1 AND Album=?2 ORDER BY Filename;";
		sqlite3_stmt* stmt = nullptr;
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( artist ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 2 /*param*/, WideStringToUTF8( album ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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

MediaInfo::List Library::GetMediaByGenre( const std::wstring& genre )
{
	MediaInfo::List mediaList;
	sqlite3* database = m_Database.GetDatabase();
	if ( nullptr != database ) {
		const std::string query = "SELECT * FROM Media WHERE Genre=?1 ORDER BY Filename;";
		sqlite3_stmt* stmt = nullptr;
		if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
				( SQLITE_OK == sqlite3_bind_text( stmt, 1 /*param*/, WideStringToUTF8( genre ).c_str(), -1 /*strLen*/, SQLITE_TRANSIENT ) ) ) {
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

MediaInfo::List Library::GetMediaByYear( const long year )
{
	MediaInfo::List mediaList;
	if ( ( year >= MINYEAR ) && ( year <= MAXYEAR ) ) {
		sqlite3* database = m_Database.GetDatabase();
		if ( nullptr != database ) {
			const std::string query = "SELECT * FROM Media WHERE Year=?1 ORDER BY Filename;";
			sqlite3_stmt* stmt = nullptr;
			if ( ( SQLITE_OK == sqlite3_prepare_v2( database, query.c_str(), -1 /*nByte*/, &stmt, nullptr /*tail*/ ) ) &&
					( SQLITE_OK == sqlite3_bind_int( stmt, 1 /*param*/, static_cast<int>( year ) ) ) ) {
				while ( SQLITE_ROW == sqlite3_step( stmt ) ) {
					MediaInfo mediaInfo;
					ExtractMediaInfo( stmt, mediaInfo );
					mediaList.push_back( mediaInfo );
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
