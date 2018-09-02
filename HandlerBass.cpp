#include "HandlerBass.h"

#include "DecoderBass.h"
#include "Utility.h"

#include "vcedit.h"

#include <list>
#include <sstream>

HandlerBass::HandlerBass() :
	Handler()
{
}

HandlerBass::~HandlerBass()
{
}

std::wstring HandlerBass::GetDescription() const
{
	const DWORD version = BASS_GetVersion();
	const DWORD d = version & 0xff;
	const DWORD c = ( version >> 8 ) & 0xff;
	const DWORD b = ( version >> 16 ) & 0xff;
	const DWORD a = ( version >> 24 ) & 0xff;
	std::wstringstream string;
	string << L"BASS " << a << L"." << b << L"." << c << L"." << d;
	return string.str();
}

std::set<std::wstring> HandlerBass::GetSupportedFileExtensions() const
{
	const std::set<std::wstring> fileTypes = { L"mod", L"s3m", L"xm", L"it", L"mtm", L"mo3", L"umx", L"mp3", L"ogg", L"wav", L"mp4", L"m4a" };
	return fileTypes;
}

bool HandlerBass::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	tags.clear();
	DWORD flags = BASS_UNICODE | BASS_MUSIC_NOSAMPLE;
	const HMUSIC music = BASS_MusicLoad( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags, 0 /*freq*/ );
	if ( music != 0 ) {
		const char* author = BASS_ChannelGetTags( music, BASS_TAG_MUSIC_AUTH );
		if ( author != nullptr ) {
			tags.insert( Tags::value_type( Tag::Artist, UTF8ToWideString( author ) ) );
		}
		const char* title = BASS_ChannelGetTags( music, BASS_TAG_MUSIC_NAME );
		if ( title != nullptr ) {
			tags.insert( Tags::value_type( Tag::Title, UTF8ToWideString( title ) ) );
		}
		const char* comment = BASS_ChannelGetTags( music, BASS_TAG_MUSIC_MESSAGE );
		if ( comment != nullptr ) {
			tags.insert( Tags::value_type( Tag::Comment, UTF8ToWideString( comment ) ) );
		}
		BASS_MusicFree( music );
		success = true;
	} else {
		flags = BASS_UNICODE;
		const HSTREAM stream = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
		if ( stream != 0 ) {
			const char* oggTags = BASS_ChannelGetTags( stream, BASS_TAG_OGG );
			if ( nullptr != oggTags ) {
				ReadOggTags( oggTags, tags );
				success = true;
			}
			BASS_StreamFree( stream );
		}
	}
	return success;
}

bool HandlerBass::SetTags( const std::wstring& filename, const Handler::Tags& tags ) const
{
	bool success = false;
	const DWORD flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
	const HSTREAM handle = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
	if ( 0 != handle ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( handle, &info );
		BASS_StreamFree( handle );
		if ( BASS_CTYPE_STREAM_OGG == info.ctype ) {
			success = WriteOggTags( filename, tags );
		}
	}
	return success;
}

Decoder::Ptr HandlerBass::OpenDecoder( const std::wstring& filename ) const
{
	DecoderBass* streamBass = nullptr;
	try {
		streamBass = new DecoderBass( filename );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( streamBass );
	return stream;
}

Encoder::Ptr HandlerBass::OpenEncoder() const
{
	return nullptr;
}

bool HandlerBass::IsDecoder() const
{
	return true;
}

bool HandlerBass::IsEncoder() const
{
	return false;
}

bool HandlerBass::CanConfigureEncoder() const
{
	return false;
}

bool HandlerBass::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerBass::ReadOggTags( const char* oggTags, Tags& tags ) const
{
	if ( nullptr != oggTags ) {
		const char* currentTag = oggTags;
		while ( 0 != *currentTag ) {
			const size_t tagLength = strlen( currentTag );
			const std::string tag = currentTag;
			const size_t pos = tag.find( '=' );
			if ( std::string::npos != pos ) {
				const std::string field = tag.substr( 0 /*offset*/, pos );
				const std::string value = tag.substr( 1 + pos /*offset*/ );
				if ( !field.empty() && !value.empty() ) {
					if ( 0 == _stricmp( field.c_str(), "artist" ) ) {
						tags.insert( Tags::value_type( Tag::Artist, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "title" ) ) {
						tags.insert( Tags::value_type( Tag::Title, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "album" ) ) {
						tags.insert( Tags::value_type( Tag::Album, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "genre" ) ) {
						tags.insert( Tags::value_type( Tag::Genre, UTF8ToWideString( value ) ) );
					} else if ( ( 0 == _stricmp( field.c_str(), "year" ) ) || ( 0 == _stricmp( field.c_str(), "date" ) ) ) {
						tags.insert( Tags::value_type( Tag::Year, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "comment" ) ) {
						tags.insert( Tags::value_type( Tag::Comment, UTF8ToWideString( value ) ) );
					} else if ( ( 0 == _stricmp( field.c_str(), "track" ) ) || ( 0 == _stricmp( field.c_str(), "tracknumber" ) ) ) {
						tags.insert( Tags::value_type( Tag::Track, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "replaygain_track_gain" ) ) {
						tags.insert( Tags::value_type( Tag::GainTrack, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "replaygain_track_peak" ) ) {
						tags.insert( Tags::value_type( Tag::PeakTrack, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "replaygain_album_gain" ) ) {
						tags.insert( Tags::value_type( Tag::GainAlbum, UTF8ToWideString( value ) ) );
					} else if ( 0 == _stricmp( field.c_str(), "replaygain_album_peak" ) ) {
						tags.insert( Tags::value_type( Tag::PeakAlbum, UTF8ToWideString( value ) ) );
					}			
				}
			}
			currentTag += 1 + tagLength;
		}
	}
}

bool HandlerBass::WriteOggTags( const std::wstring& filename, const Tags& tags ) const
{
	bool handled = true;

	// Check if there are any supported tags.
	for ( auto iter = tags.begin(); handled && ( iter != tags.end() ); iter++ ) {
		const Tag tag = iter->first;
		switch ( tag ) {
			case Tag::Album :
			case Tag::Artist :
			case Tag::Comment :
			case Tag::GainAlbum : 
			case Tag::GainTrack :
			case Tag::Genre :
			case Tag::PeakAlbum :
			case Tag::PeakTrack :
			case Tag::Title :
			case Tag::Track :
			case Tag::Year : {
				handled = false;
				break;
			}
			default : {
				break;
			}
		}
	}

	if ( !handled ) {
		const std::wstring tempFilename = GetTemporaryFilename();
		FILE* inputStream = nullptr;
		if ( 0 == _wfopen_s( &inputStream, filename.c_str(), L"rb" ) ) {
			FILE* outputStream = nullptr;
			if ( 0 == _wfopen_s( &outputStream, tempFilename.c_str(), L"wb" ) ) {
				vcedit_state* state = vcedit_new_state();
				if ( 0 == vcedit_open( state, inputStream ) ) {
					vorbis_comment* originalComments = vcedit_comments( state );
					if ( nullptr != originalComments ) {
						vorbis_comment modifiedComments = {};
						vorbis_comment_init( &modifiedComments );
						const std::string vendor = originalComments->vendor;

						std::set<int> originalCommentsToDrop;

						for ( const auto& tagIter : tags ) {
							const Tag tag = tagIter.first;
							const std::string tagValue = WideStringToUTF8( tagIter.second );
							std::list<std::string> tagFields;
							switch ( tag ) {
								case Tag::Album : {
									tagFields = { "ALBUM" };
									break;
								}
								case Tag::Artist : {
									tagFields = { "ARTIST" };
									break;
								}
								case Tag::Comment : {
									tagFields = { "COMMENT" };
									break;								
								}
								case Tag::Genre : {
									tagFields = { "GENRE" };
									break;
								}
								case Tag::Title : {
									tagFields = { "TITLE" };
									break;
								}
								case Tag::Track : {
									tagFields = { "TRACKNUMBER", "TRACK" };
									break;
								}
								case Tag::Year : {
									tagFields = { "DATE", "YEAR" };
									break;
								}
								case Tag::GainAlbum : {
									tagFields = { "REPLAYGAIN_ALBUM_GAIN" };
									break;
								}
								case Tag::PeakAlbum : {
									tagFields = { "REPLAYGAIN_ALBUM_PEAK" };
									break;
								}
								case Tag::GainTrack : {
									tagFields = { "REPLAYGAIN_TRACK_GAIN" };
									break;
								}
								case Tag::PeakTrack : {
									tagFields = { "REPLAYGAIN_TRACK_PEAK" };
									break;
								}
								default : {
									break;
								}
							}
							if ( !tagFields.empty() ) {
								for ( int index = 0; index < originalComments->comments; ++index ) {
									const std::string comment = originalComments->user_comments[ index ];
									const size_t delimiter = comment.find( '=' );
									if ( ( std::string::npos != delimiter ) && ( delimiter > 0 ) ) {
										const std::string field = StringToUpper( comment.substr( 0 /*offset*/, delimiter /*count*/ ) );
										if ( tagFields.end() != std::find( tagFields.begin(), tagFields.end(), field ) ) {
											originalCommentsToDrop.insert( index );						
										}
									}
								}
								const std::string comment = tagFields.front() + '=' + tagValue;
								vorbis_comment_add( &modifiedComments, comment.c_str() );							
							}			
						}

						for ( int index = 0; index < originalComments->comments; ++index ) {
							if ( originalCommentsToDrop.end() == originalCommentsToDrop.find( index ) ) {
								vorbis_comment_add( &modifiedComments, originalComments->user_comments[ index ] );							
							}
						}

						vorbis_comment_clear( originalComments );
						vorbis_comment_init( originalComments );
						for ( int index = 0; index < modifiedComments.comments; ++index ) {
							vorbis_comment_add( originalComments, modifiedComments.user_comments[ index ] );
						}
						originalComments->vendor = const_cast<char*>( vendor.c_str() );

						if ( 0 == vcedit_write( state, outputStream ) ) {
							handled = true;
						}

						originalComments->vendor = nullptr;
						vorbis_comment_clear( originalComments );
						vorbis_comment_clear( &modifiedComments );
					}
				}
				vcedit_clear(state);
				fclose( outputStream );
			}
			fclose( inputStream );
		}

		if ( handled ) {
			if ( 0 == MoveFileEx( tempFilename.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH ) ) {
				DeleteFile( tempFilename.c_str() );
				handled = false;
			}
		}
	}
	return handled;
}

std::wstring HandlerBass::GetTemporaryFilename() const
{
	std::wstring filename;
	WCHAR pathName[ MAX_PATH ];
	if ( 0 != GetTempPath( MAX_PATH, pathName ) ) {
		filename = pathName + UTF8ToWideString( GenerateGUIDString() );
	}
	return filename;
}
