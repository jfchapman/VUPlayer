#include "HandlerBass.h"

#include "DecoderBass.h"
#include "Settings.h"
#include "Utility.h"

#include "OggComment.h"
#include "TagReader.h"

#include <list>
#include <sstream>
#include <regex>
#include <fstream>

// Supported tags and their Vorbis comment field names.
static const std::map<Tag, std::string> s_SupportedOggTags = {
	{ Tag::Album,      "ALBUM" },
	{ Tag::Artist,     "ARTIST" },
	{ Tag::Comment,    "COMMENT" },
	{ Tag::GainAlbum,  "REPLAYGAIN_ALBUM_GAIN" },
	{ Tag::GainTrack,  "REPLAYGAIN_TRACK_GAIN" },
	{ Tag::Genre,      "GENRE" },
	{ Tag::Title,      "TITLE" },
	{ Tag::Track,      "TRACKNUMBER" },
	{ Tag::Year,       "DATE" },
	{ Tag::Composer,   "COMPOSER" },
	{ Tag::Conductor,  "CONDUCTOR" },
	{ Tag::Publisher,  "PUBLISHER" }
};

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
	return { L"mod", L"s3m", L"xm", L"mtm", L"mo3", L"umx", L"mp3", L"ogg", L"wav", L"mid", L"midi", L"dsd", L"dsf", L"dff" };
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
				success = ReadOggComment( filename, tags );
				if ( !success ) {
					// Fall back to the tags read by BASS.
					const char* oggTags = BASS_ChannelGetTags( stream, BASS_TAG_OGG );
					if ( nullptr != oggTags ) {
						ReadOggTags( oggTags, tags );
						success = true;
					}
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
	if ( const HMUSIC music = BASS_MusicLoad( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, BASS_UNICODE | BASS_MUSIC_NOSAMPLE, 0 /*freq*/ ); 0 != music ) {
		// MOD music tagging is not supported, so return true to indicate this has been handled (and prevent other handlers from trying to write tags).
		BASS_MusicFree( music );
		return true;
	}

	const DWORD flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
	const HSTREAM handle = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
	if ( 0 != handle ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( handle, &info );
		BASS_StreamFree( handle );
		if ( BASS_CTYPE_STREAM_OGG == info.ctype ) {
			return WriteOggTags( filename, tags );
		}
	}
	return false;
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
				AddOggTag( tag.substr( 0 /*offset*/, pos ), tag.substr( 1 + pos /*offset*/ ), tags );
			}
			currentTag += 1 + tagLength;
		}
	}
}

bool HandlerBass::ReadOggComment( const std::wstring& filename, Tags& tags )
{
	bool success = false;
	try {
		const OggComment oggComment( filename, OggComment::Type::Vorbis );
		for ( const auto& [ field, value ] : oggComment.GetUserComments() ) {
			AddOggTag( field, value, tags );
		}
		if ( const std::string artwork = oggComment.GetArtwork(); !artwork.empty() ) {
			tags.insert( Tags::value_type( Tag::Artwork, artwork ) );
		}
		success = true;
	} catch ( const std::runtime_error& ) {
	}
	return success;
}

void HandlerBass::AddOggTag( const std::string& field, const std::string& value, Tags& tags )
{
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

bool HandlerBass::WriteOggTags( const std::wstring& filename, const Tags& tags )
{
	// Only modify the file if there are any supported tags.
	const bool hasSupportedTags = std::any_of( tags.begin(), tags.end(), [] ( const auto& tag ) {
		return ( Tag::Artwork == tag.first ) || ( s_SupportedOggTags.end() != s_SupportedOggTags.find( tag.first ) );
	} );
	bool success = !hasSupportedTags;
	if ( hasSupportedTags ) {
		try {
			OggComment oggComment( filename, OggComment::Type::Vorbis, false /*readonly*/ );
			for ( const auto& [ tag, value ] : tags ) {
				if ( const auto match = s_SupportedOggTags.find( tag ); s_SupportedOggTags.end() != match ) {
					const std::string& field = match->second;
					oggComment.RemoveUserComment( field );
					if ( !value.empty() ) {
						oggComment.AddUserComment( field, value );
					}
				} else if ( Tag::Artwork == tag ) {
					oggComment.SetArtwork( value );
				}
			}
			success = oggComment.WriteComments();
		} catch ( const std::runtime_error& ) {
		}
	}
	return success;
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
