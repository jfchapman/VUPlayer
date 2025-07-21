#include "HandlerBass.h"

#include "DecoderBass.h"
#include "Settings.h"
#include "Utility.h"

#include "vcedit.h"
#include "TagReader.h"

#include <list>
#include <sstream>
#include <regex>
#include <fstream>

HandlerBass::HandlerBass() :
	Handler(),
	m_BassMidi( BASS_PluginLoad( L"bassmidi.dll", BASS_UNICODE ) ),
	m_BassDSD( BASS_PluginLoad( L"bassdsd.dll", BASS_UNICODE ) ),
	m_BassHLS( BASS_PluginLoad( L"basshls.dll", BASS_UNICODE ) ),
	m_BassMidiSoundFont( 0 ),
	m_SoundFontFilename()
{
}

HandlerBass::~HandlerBass()
{
	if ( 0 != m_BassMidiSoundFont ) {
		BASS_MIDI_FontFree( m_BassMidiSoundFont );
		m_BassMidiSoundFont = 0;
	}
	if ( 0 != m_BassMidi ) {
		BASS_PluginFree( m_BassMidi );
		m_BassMidi = 0;
	}
	if ( 0 != m_BassDSD ) {
		BASS_PluginFree( m_BassDSD );
		m_BassDSD = 0;
	}
	if ( 0 != m_BassHLS ) {
		BASS_PluginFree( m_BassHLS );
		m_BassHLS = 0;
	}
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
	return { L"mod", L"s3m", L"xm", L"mtm", L"mo3", L"umx", L"mp3", L"ogg", L"wav", L"mid", L"midi", L"dsd", L"dsf" };
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
			tags.insert( Tags::value_type( Tag::Artist, author ) );
		}
		const char* title = BASS_ChannelGetTags( music, BASS_TAG_MUSIC_NAME );
		if ( title != nullptr ) {
			tags.insert( Tags::value_type( Tag::Title, title ) );
		}
		const char* comment = BASS_ChannelGetTags( music, BASS_TAG_MUSIC_MESSAGE );
		if ( comment != nullptr ) {
			tags.insert( Tags::value_type( Tag::Comment, comment ) );
		}
		BASS_MusicFree( music );
		success = true;
	} else {
		flags = BASS_UNICODE;
		const HSTREAM stream = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
		if ( stream != 0 ) {
			BASS_CHANNELINFO info = {};
			BASS_ChannelGetInfo( stream, &info );
			if ( BASS_CTYPE_STREAM_OGG == info.ctype ) {
				const char* oggTags = BASS_ChannelGetTags( stream, BASS_TAG_OGG );
				if ( nullptr != oggTags ) {
					ReadOggTags( oggTags, tags );
					success = true;
				}
			} else if ( BASS_CTYPE_STREAM_MIDI == info.ctype ) {
				const char* midiTags = BASS_ChannelGetTags( stream, BASS_TAG_MIDI_TRACK );
				if ( ( nullptr != midiTags ) && ( strlen( midiTags ) > 0 ) ) {
					tags.insert( Tags::value_type( Tag::Title, midiTags ) );
					success = true;
				}
			} else if ( BASS_CTYPE_STREAM_DSD == info.ctype ) {
				if ( ReadDSDTag( filename, tags ) ) {
					success = true;
				} else {
					const char* dsdArtist = BASS_ChannelGetTags( stream, BASS_TAG_DSD_ARTIST );
					if ( ( nullptr != dsdArtist ) && ( strlen( dsdArtist ) > 0 ) ) {
						tags.insert( Tags::value_type( Tag::Artist, dsdArtist ) );
						success = true;
					}
					const char* dsdTitle = BASS_ChannelGetTags( stream, BASS_TAG_DSD_TITLE );
					if ( ( nullptr != dsdTitle ) && ( strlen( dsdTitle ) > 0 ) ) {
						tags.insert( Tags::value_type( Tag::Title, dsdTitle ) );
						success = true;
					}
				}
				if ( float rate = 0; BASS_ChannelGetAttribute( stream, BASS_ATTRIB_DSD_RATE, &rate ) ) {
					std::string format;
					constexpr int kDSDBaseRate = 2822400;
					switch ( static_cast<int>( rate ) ) {
						case kDSDBaseRate:
							format = "DSD64";
							break;
						case 2 * kDSDBaseRate:
							format = "DSD128";
							break;
						case 4 * kDSDBaseRate:
							format = "DSD256";
							break;
						case 8 * kDSDBaseRate:
							format = "DSD512";
							break;
						case 16 * kDSDBaseRate:
							format = "DSD1024";
							break;
					}
					if ( !format.empty() ) {
						tags.insert( { Tag::Version, format } );
						success = true;
					}
				}
			} else if ( BASS_CTYPE_STREAM_WAV & info.ctype ) {
				if ( const char* riffTags = BASS_ChannelGetTags( stream, BASS_TAG_RIFF_INFO ); nullptr != riffTags ) {
					ReadRIFFTags( riffTags, tags );
					success = true;
				}
			}
			BASS_StreamFree( stream );
		}
	}
	return success;
}

bool HandlerBass::SetTags( const std::wstring& filename, const Tags& tags ) const
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

Decoder::Ptr HandlerBass::OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const
{
	DecoderBass* streamBass = nullptr;
	try {
		streamBass = new DecoderBass( filename, context );
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

void HandlerBass::ReadOggTags( const char* oggTags, Tags& tags )
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
					if ( 0 == _stricmp( field.c_str(), "ARTIST" ) ) {
						tags.insert( Tags::value_type( Tag::Artist, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "TITLE" ) ) {
						tags.insert( Tags::value_type( Tag::Title, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "ALBUM" ) ) {
						tags.insert( Tags::value_type( Tag::Album, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "GENRE" ) ) {
						tags.insert( Tags::value_type( Tag::Genre, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "YEAR" ) ) {
						tags.insert( Tags::value_type( Tag::Year, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "DATE" ) ) {
						// Prefer 'date' over 'year' (both map to the same tag type).
						tags[ Tag::Year ] = value;
					} else if ( 0 == _stricmp( field.c_str(), "COMMENT" ) ) {
						tags.insert( Tags::value_type( Tag::Comment, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "TRACK" ) ) {
						tags.insert( Tags::value_type( Tag::Track, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "TRACKNUMBER" ) ) {
						// Prefer 'tracknumber' over 'track' (both map to the same tag type).
						tags[ Tag::Track ] = value;
					} else if ( 0 == _stricmp( field.c_str(), "REPLAYGAIN_TRACK_GAIN" ) ) {
						tags.insert( Tags::value_type( Tag::GainTrack, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "REPLAYGAIN_ALBUM_GAIN" ) ) {
						tags.insert( Tags::value_type( Tag::GainAlbum, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "COMPOSER" ) ) {
						tags.insert( Tags::value_type( Tag::Composer, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "CONDUCTOR" ) ) {
						tags.insert( Tags::value_type( Tag::Conductor, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "LABEL" ) ) {
						tags.insert( Tags::value_type( Tag::Publisher, value ) );
					} else if ( 0 == _stricmp( field.c_str(), "PUBLISHER" ) ) {
						// Prefer 'publisher' over 'label' (both map to the same tag type).
						tags[ Tag::Publisher ] = value;
					}
				}
			}
			currentTag += 1 + tagLength;
		}
	}
}

bool HandlerBass::WriteOggTags( const std::wstring& filename, const Tags& tags )
{
	bool handled = true;

	// Check if there are any supported tags.
	for ( auto iter = tags.begin(); handled && ( iter != tags.end() ); iter++ ) {
		const Tag tag = iter->first;
		switch ( tag ) {
			case Tag::Album:
			case Tag::Artist:
			case Tag::Comment:
			case Tag::GainAlbum:
			case Tag::GainTrack:
			case Tag::Genre:
			case Tag::Title:
			case Tag::Track:
			case Tag::Year:
			case Tag::Composer:
			case Tag::Conductor:
			case Tag::Publisher: {
				handled = false;
				break;
			}
			default: {
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
							const std::string& tagValue = tagIter.second;
							std::string tagField;
							switch ( tag ) {
								case Tag::Album: {
									tagField = "ALBUM";
									break;
								}
								case Tag::Artist: {
									tagField = "ARTIST";
									break;
								}
								case Tag::Comment: {
									tagField = "COMMENT";
									break;
								}
								case Tag::Genre: {
									tagField = "GENRE";
									break;
								}
								case Tag::Title: {
									tagField = "TITLE";
									break;
								}
								case Tag::Track: {
									tagField = "TRACKNUMBER";
									break;
								}
								case Tag::Year: {
									tagField = "DATE";
									break;
								}
								case Tag::GainAlbum: {
									tagField = "REPLAYGAIN_ALBUM_GAIN";
									break;
								}
								case Tag::GainTrack: {
									tagField = "REPLAYGAIN_TRACK_GAIN";
									break;
								}
								case Tag::Composer: {
									tagField = "COMPOSER";
									break;
								}
								case Tag::Conductor: {
									tagField = "CONDUCTOR";
									break;
								}
								case Tag::Publisher: {
									tagField = "PUBLISHER";
									break;
								}
								default: {
									break;
								}
							}
							if ( !tagField.empty() ) {
								for ( int index = 0; index < originalComments->comments; ++index ) {
									const std::string comment = originalComments->user_comments[ index ];
									const size_t delimiter = comment.find( '=' );
									if ( ( std::string::npos != delimiter ) && ( delimiter > 0 ) ) {
										const std::string field = StringToUpper( comment.substr( 0 /*offset*/, delimiter /*count*/ ) );
										if ( tagField == field ) {
											originalCommentsToDrop.insert( index );
										}
									}
								}
								const std::string comment = tagField + '=' + tagValue;
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
				vcedit_clear( state );
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

void HandlerBass::ReadRIFFTags( const char* riffTags, Tags& tags )
{
	if ( nullptr == riffTags )
		return;

	const std::regex kRegexTag( R"(^(.{4})=(.+))" );

	const char* currentTag = riffTags;
	while ( 0 != *currentTag ) {
		const size_t tagLength = strlen( currentTag );
		const std::string tag = CodePageToUTF8( currentTag, 1252 );

		std::smatch match;
		if ( std::regex_match( tag, match, kRegexTag ) && ( 3 == match.size() ) ) {
			const std::string field = match[ 1 ];
			const std::string value = match[ 2 ];
			if ( field == "IART" ) {
				tags.insert( { Tag::Artist, value } );
			} else if ( field == "INAM" ) {
				tags.insert( { Tag::Title, value } );
			} else if ( field == "IGNR" ) {
				tags.insert( { Tag::Genre, value } );
			} else if ( field == "ICMT" ) {
				tags.insert( { Tag::Comment, value } );
			} else if ( field == "IPRD" ) {
				tags.insert( { Tag::Album, value } );
			} else if ( field == "ICRD" ) {
				tags.insert( { Tag::Year, value } );
			} else if ( field == "ITRK" ) {
				tags.insert( { Tag::Track, value } );
			}
		}

		currentTag += 1 + tagLength;
	}
}

bool HandlerBass::ReadDSDTag( const std::wstring& filename, Tags& tags )
{
	if ( std::ifstream dsdFile( filename, std::ios::binary ); dsdFile.good() ) {
		dsdFile.seekg( 0, std::ios::end );
		const uint64_t filesize = dsdFile.tellg();
		dsdFile.seekg( 0 );
		if ( dsdFile.good() ) {
			constexpr uint32_t kDsdHeaderSize = 28;
			std::vector<char> header( kDsdHeaderSize );
			dsdFile.read( header.data(), kDsdHeaderSize );
			if ( dsdFile.good() && ( "DSD " == std::string( header.data(), 4 ) ) && ( kDsdHeaderSize == header[ 4 ] ) ) {
				const uint32_t dsdSize = *reinterpret_cast<uint32_t*>( &header[ 12 ] );
				const uint32_t tagOffset = *reinterpret_cast<uint32_t*>( &header[ 20 ] );
				const uint32_t tagSize = ( tagOffset > 0 && tagOffset < dsdSize && dsdSize <= filesize ) ? ( dsdSize - tagOffset ) : 0;
				constexpr uint32_t kMaximumTagSize = 0x10000000;
				if ( tagSize > 0 && tagSize <= kMaximumTagSize ) {
					std::vector<char> id3v2( tagSize );
					dsdFile.seekg( tagOffset );
					dsdFile.read( id3v2.data(), id3v2.size() );
					if ( dsdFile.good() ) {
						TagReader tagReader( id3v2 );
						if ( const auto fileTags = tagReader.GetTags(); fileTags.has_value() ) {
							tags = *fileTags;
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

std::wstring HandlerBass::GetTemporaryFilename()
{
	std::wstring filename;
	WCHAR pathName[ MAX_PATH ];
	if ( 0 != GetTempPath( MAX_PATH, pathName ) ) {
		filename = pathName + UTF8ToWideString( GenerateGUIDString() );
	}
	return filename;
}

void HandlerBass::SettingsChanged( Settings& settings )
{
	LoadSoundFont( settings );
	BASS_SetConfig( BASS_CONFIG_DSD_FREQ, settings.GetDSDSamplerate() );
}

void HandlerBass::LoadSoundFont( Settings& settings )
{
	if ( 0 != m_BassMidi ) {
		const std::wstring filename = settings.GetSoundFont();
		if ( filename != m_SoundFontFilename ) {
			m_SoundFontFilename = filename;
			if ( 0 != m_BassMidiSoundFont ) {
				BASS_MIDI_FontFree( m_BassMidiSoundFont );
				m_BassMidiSoundFont = 0;
			}
			m_BassMidiSoundFont = BASS_MIDI_FontInit( filename.c_str(), BASS_UNICODE );
			if ( 0 != m_BassMidiSoundFont ) {
				const BASS_MIDI_FONT font = { m_BassMidiSoundFont, -1 /*preset*/, 0 /*bank*/ };
				BASS_MIDI_StreamSetFonts( 0 /*default*/, &font, 1 /*count*/ );
			}
		}
	}
}
