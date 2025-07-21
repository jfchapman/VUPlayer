#include "TagReader.h"

#include "Utility.h"

#include <bitset>
#include <fstream>
#include <functional>
#include <regex>

// Supported ID3v2 frame IDs and their corresponding tag type.
static const std::map<std::string, Tag> kSupportedID3v2Frames = {
	{ "TALB", Tag::Album },
	{ "TPE1", Tag::Artist },
	{ "TCON", Tag::Genre },
	{ "TIT2", Tag::Title },
	{ "TRCK", Tag::Track },
	{ "TYER", Tag::Year },
	{ "TCOM", Tag::Composer },
	{ "TPE3", Tag::Conductor },
	{ "TPUB", Tag::Publisher },
	{ "COMM", Tag::Comment },
	{ "APIC", Tag::Artwork }
};

// Size of an ID3v2.3 tag header.
constexpr uint32_t kID3v2TagHeaderSize = 10;

// Size of an ID3v2.3 frame header.
constexpr uint32_t kID3v2FrameHeaderSize = 10;

// Size of an ID3v1 tag.
constexpr uint32_t kID3v1TagSize = 128;

// All supported APE tag names (note that some names map to same tag type).
static const std::map<std::string, Tag> kSupportedAPETags = {
	{ "album",                  Tag::Album },
	{ "artist",                 Tag::Artist },
	{ "comment",                Tag::Comment },
	{ "replaygain_album_gain",  Tag::GainAlbum },
	{ "replay gain (album)",    Tag::GainAlbum },
	{ "replaygain_track_gain",  Tag::GainTrack },
	{ "replay gain (radio)",    Tag::GainTrack },
	{ "genre",                  Tag::Genre },
	{ "title",                  Tag::Title },
	{ "tracknumber",            Tag::Track },
	{ "track",                  Tag::Track },
	{ "date",                   Tag::Year },
	{ "year",                   Tag::Year },
	{ "composer",               Tag::Composer },
	{ "conductor",              Tag::Conductor },
	{ "publisher",              Tag::Publisher },
	{ "label",                  Tag::Publisher }
};

// ISO 8859-1 code page.
constexpr uint32_t kCodePageISO8859_1 = 28591u;

TagReader::TagReader( const std::filesystem::path& filepath )
{
	if ( !ParseID3v2Tag( filepath ) ) {
		if ( !ParseAPETag( filepath ) ) {
			ParseID3v1Tag( filepath );
		}
	}
}

TagReader::TagReader( const std::vector<char>& id3v2Tag )
{
	ParseID3v2Tag( id3v2Tag );
}

std::optional<Tags> TagReader::GetTags() const
{
	if ( m_Tags.empty() )
		return std::nullopt;

	return std::make_optional( m_Tags );
}

bool TagReader::ParseID3v2Tag( const std::filesystem::path& filepath )
{
	if ( std::ifstream stream( filepath, std::ios::binary ); stream.good() ) {
		stream.seekg( 0, std::ios::end );
		const uint64_t filesize = stream.tellg();
		stream.seekg( 0 );
		if ( stream.good() ) {
			std::vector<char> tag( kID3v2TagHeaderSize );
			stream.read( tag.data(), kID3v2TagHeaderSize );
			if ( stream.good() ) {
				const auto tagSize = GetID3v2TagSize( tag.data() );
				constexpr uint64_t kMaximumID3v2TagSize = 0x10000000;
				if ( ( tagSize > kID3v2FrameHeaderSize ) && ( kID3v2TagHeaderSize + tagSize <= std::min( filesize, kMaximumID3v2TagSize ) ) ) {
					tag.resize( kID3v2TagHeaderSize + tagSize );
					stream.read( tag.data() + kID3v2TagHeaderSize, tagSize );
					if ( stream.good() ) {
						return ParseID3v2Tag( tag );
					}
				}
			}
		}
	}
	return false;
}

bool TagReader::ParseID3v2Tag( const std::vector<char>& tag )
{
	if ( tag.size() < kID3v2TagHeaderSize )
		return false;

	const auto tagSize = GetID3v2TagSize( tag.data() );
	if ( ( tagSize <= kID3v2FrameHeaderSize ) || ( kID3v2TagHeaderSize + tagSize > tag.size() ) )
		return false;

	const auto firstFrame = tag.begin() + kID3v2TagHeaderSize;
	auto frame = firstFrame;

	while ( kID3v2FrameHeaderSize + std::distance( firstFrame, frame ) <= tagSize ) {
		const std::string frameID( &frame[ 0 ], 4 );

		uint32_t frameSize = static_cast<uint8_t>( frame[ 4 ] );
		frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 5 ] );
		frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 6 ] );
		frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 7 ] );

		const std::bitset<8> frameFormatFlags( frame[ 9 ] );

		if ( ( 0 == frameSize ) || ( kID3v2FrameHeaderSize + frameSize + std::distance( firstFrame, frame ) > tagSize ) )
			break;

		frame += kID3v2FrameHeaderSize;

		const auto tagID = kSupportedID3v2Frames.find( frameID );
		if ( ( frameSize > 1 ) && frameFormatFlags.none() && ( kSupportedID3v2Frames.end() != tagID ) ) {
			const auto tagType = tagID->second;
			if ( Tag::Artwork == tagType ) {
				// Picture frame
				if ( const auto [ image, pictureType ] = ConvertID3v2PictureFrameToBase64( &frame[ 0 ], frameSize ); !image.empty() ) {
					constexpr uint8_t kOtherPicture = 0;
					constexpr uint8_t kFrontCover = 3;
					if ( kFrontCover == pictureType )
						m_Tags.insert( { tagType, image } );
					else if ( ( kOtherPicture == pictureType ) && ( m_Tags.end() == m_Tags.find( Tag::Artwork ) ) )
						m_Tags.insert( { tagType, image } );
				}
			} else if ( Tag::Comment == tagType ) {
				// Comment frame
				if ( const auto value = ConvertID3v2CommentFrameToUTF8( &frame[ 0 ], frameSize ); !value.empty() ) {
					m_Tags.insert( { tagType, value } );
				}
			} else {
				// Text information frame
				if ( const auto value = ConvertID3v2TextFrameToUTF8( &frame[ 0 ], frameSize ); !value.empty() ) {
					if ( Tag::Genre == tagType ) {
						if ( const auto genre = GetID3v2Genre( value ); !genre.empty() ) {
							m_Tags.insert( { tagType, genre } );
						}
					} else {
						m_Tags.insert( { tagType, value } );
					}
				}
			}
		}
		frame += frameSize;
	}

	return !m_Tags.empty();
}

uint32_t TagReader::GetID3v2TagSize( const char* header )
{
	// Only accept ID3v2.3 tags with no header flags set
	if ( ( nullptr == header ) || ( "ID3" != std::string( &header[ 0 ], 3 ) ) || ( 3 != header[ 3 ] ) || ( 0 != header[ 4 ] ) )
		return 0;

	const std::bitset<8> flags( header[ 5 ] );
	if ( flags.any() )
		return 0;

	uint32_t tagSize = static_cast<uint8_t>( header[ 6 ] );
	tagSize <<= 7; tagSize |= static_cast<uint8_t>( header[ 7 ] & 0x7F );
	tagSize <<= 7; tagSize |= static_cast<uint8_t>( header[ 8 ] & 0x7F );
	tagSize <<= 7; tagSize |= static_cast<uint8_t>( header[ 9 ] & 0x7F );
	return tagSize;
}

std::string TagReader::ConvertID3v2TextFrameToUTF8( const char* frame, const uint32_t frameSize )
{
	if ( nullptr == frame || frameSize < 2 )
		return {};

	const char textEncoding = frame[ 0 ];
	switch ( textEncoding ) {
		case 0: {
				// ISO-8859-1
				const std::string value( &frame[ 1 ], frameSize - 1 );
				return CodePageToUTF8( value, kCodePageISO8859_1 );
			}
			break;
		case 1: {
				// Unicode with BOM
				if ( ( frameSize > 3 ) && ( 1 == frameSize % 2 ) ) {
					const wchar_t bom = *reinterpret_cast<const wchar_t*>( &frame[ 1 ] );
					if ( 0xFEFF == bom ) {
						const std::wstring wValue( reinterpret_cast<const wchar_t*>( &frame[ 3 ] ), ( frameSize - 3 ) / 2 );
						return WideStringToUTF8( wValue );
					} else if ( 0xFFFE == bom ) {
						std::vector<char> value( frameSize - 3 );
						std::copy( frame + 3, frame + frameSize, value.begin() );
						for ( size_t i = 0; i < value.size(); i += 2 ) {
							std::swap( value[ i ], value[ 1 + i ] );
						}
						const std::wstring wValue( reinterpret_cast<const wchar_t*>( &value[ 0 ] ), value.size() / 2 );
						return WideStringToUTF8( wValue );
					}
				}
			}
			break;
		default:
			break;
	}

	return {};
}

std::string TagReader::ConvertID3v2CommentFrameToUTF8( const char* frame, const uint32_t frameSize )
{
	if ( nullptr == frame || frameSize < 6 )
		return {};

	const char textEncoding = frame[ 0 ];
	if ( !( 0 == textEncoding || 1 == textEncoding ) )
		return {};

	// Ignore language and short content description strings
	uint32_t offset = 4;
	if ( 0 == textEncoding ) {
		const char* description = &frame[ offset ];
		const uint32_t descriptionLength = static_cast<uint32_t>( strnlen( description, frameSize - offset ) );
		offset += 1 + descriptionLength;
	} else {
		const wchar_t* description = reinterpret_cast<const wchar_t*>( &frame[ offset ] );
		const uint32_t descriptionLength = 2 * static_cast<uint32_t>( wcsnlen( description, ( frameSize - offset ) / 2 ) );
		offset += 2 + descriptionLength;
	}
	if ( offset >= frameSize )
		return {};

	std::vector<char> modifiedFrame( 1 + frameSize - offset );
	modifiedFrame[ 0 ] = textEncoding;
	std::copy( frame + offset, frame + frameSize, 1 + modifiedFrame.begin() );
	return ConvertID3v2TextFrameToUTF8( modifiedFrame.data(), static_cast<uint32_t>( modifiedFrame.size() ) );
}

std::pair<std::string, uint8_t> TagReader::ConvertID3v2PictureFrameToBase64( const char* frame, const uint32_t frameSize )
{
	const char textEncoding = frame[ 0 ];
	if ( 0 == textEncoding || 1 == textEncoding ) {
		// Skip mime type string
		uint32_t offset = 1;
		const char* mimeType = &frame[ offset ];
		const uint32_t mimeTypeLength = static_cast<uint32_t>( strnlen( mimeType, frameSize - offset ) );
		offset += 1 + mimeTypeLength;
		if ( offset < frameSize ) {
			const uint8_t pictureType = static_cast<uint8_t>( frame[ offset ] );
			offset += 1;
			if ( offset < frameSize ) {
				// Skip encoded description string
				if ( 0 == textEncoding ) {
					// ISO-8859-1
					const char* description = &frame[ offset ];
					const uint32_t descriptionLength = static_cast<uint32_t>( strnlen( description, frameSize - offset ) );
					offset += 1 + descriptionLength;
				} else {
					// Unicode with BOM
					const wchar_t* description = reinterpret_cast<const wchar_t*>( &frame[ offset ] );
					const uint32_t descriptionLength = 2 * static_cast<uint32_t>( wcsnlen( description, ( frameSize - offset ) / 2 ) );
					offset += 2 + descriptionLength;
				}
				if ( offset < frameSize ) {
					// Convert picture data to base64
					std::vector<uint8_t> imageBytes( frameSize - offset );
					std::copy( &frame[ offset ], &frame[ frameSize ], imageBytes.begin() );
					return std::make_pair( ConvertImage( imageBytes ), pictureType );
				}
			}
		}
	}
	return {};
}

std::string TagReader::GetID3v2Genre( const std::string& contentType )
{
	// Strip out any parenthesised term at the start of the content.
	const std::regex kRegexGenre( R"(^\(([^\)]*)\)(.*))" );

	std::string value = contentType;
	std::smatch match;
	bool matched = std::regex_match( value, match, kRegexGenre );
	while ( matched && ( 3 == match.size() ) ) {
		try {
			if ( const auto genreID = std::stoul( match.str( 1 ) ); genreID < kID3Genres.size() )
				return kID3Genres[ genreID ];
		} catch ( const std::logic_error& ) {
		}
		value = match.str( 2 );
		matched = std::regex_match( value, match, kRegexGenre );
	}

	return value;
}

bool TagReader::ParseAPETag( const std::filesystem::path& filepath )
{
	struct Footer {
		std::array<char, 8>   magic = {};
		uint32_t              version = 0;
		uint32_t              tag_size = 0;
		uint32_t              item_count = 0;
		uint32_t              flags = 0;
		std::array<char, 8>   reserved = {};
	};

	std::function<bool(const Footer& footer)> isValidFooter = [](const Footer& footer) {
		return ( 0 == std::strncmp( footer.magic.data(), "APETAGEX", 8 ) ) &&
			( ( 1000 == footer.version ) || ( 2000 == footer.version ) ) &&
			( footer.tag_size > sizeof( Footer ) ) &&
			( footer.item_count > 0 );		
	};

	std::function<bool(std::ifstream& stream, Footer& footer, std::vector<char>& tag)> readAPETag = [isValidFooter](std::ifstream& stream, Footer& footer, std::vector<char>& tag) {
		if ( !stream.good() )
			return false;
		
		constexpr uint64_t kMaximumApeTagSize = 0x10000000;
		stream.read( reinterpret_cast<char*>( &footer ), sizeof( Footer ) );
		if ( stream.good() && isValidFooter( footer ) && ( footer.tag_size <= std::min( static_cast<uint64_t>( stream.tellg() ), kMaximumApeTagSize ) ) ) {
			tag.resize( footer.tag_size - sizeof( Footer ) );
			stream.seekg( 0ll - footer.tag_size, std::ios::cur );
			stream.read( tag.data(), tag.size() );
			return stream.good();
		}

		return false;
	};

	std::ifstream stream( filepath, std::ios::binary );
	if ( !stream.good() )
		return false;

	Footer footer;
	std::vector<char> apeTag;

	// Search for a footer at the end of the file.
	stream.seekg( 0ll - sizeof( Footer ), std::ios::end );
	bool validApeTag = readAPETag( stream, footer, apeTag );
	if ( !validApeTag ) {
		// Search for a footer immediately preceding an ID3v1 tag.
		std::array<char, 3> id3TagHeader = {};
		stream.seekg( 0ll - kID3v1TagSize, std::ios::end );
		stream.read( id3TagHeader.data(), id3TagHeader.size() );
		if ( "TAG" == std::string( id3TagHeader.data(), 3 ) ) {
			stream.seekg( 0ll - kID3v1TagSize - static_cast<int32_t>( sizeof( Footer ) ), std::ios::end );
			validApeTag = readAPETag( stream, footer, apeTag );
		}
	}

	if ( !validApeTag )
		return false;
	
	struct ItemHeader {
		uint32_t value_size = 0;
		uint32_t flags = 0;
	};

	auto item = apeTag.begin();
	for ( uint32_t index = 0; ( index < footer.item_count ) && ( std::distance( item, apeTag.end() ) >= sizeof( ItemHeader ) ); index++ ) {
		ItemHeader header;
		std::copy( item, item + sizeof( ItemHeader ), reinterpret_cast<char*>( &header ) );
		if ( 0 == header.value_size )
			break;

		item += sizeof( ItemHeader );

		const uint32_t keyLength = static_cast<uint32_t>( strnlen( &(*item), std::distance( item, apeTag.end() ) ) );
		if ( 1 + keyLength + header.value_size > static_cast<uint64_t>( std::distance( item, apeTag.end() ) ) )
			break;
		const std::string key = ( keyLength > 1 ) ? std::string( &(*item), keyLength ) : std::string();

		item += 1 + keyLength;

		if ( const auto tag = kSupportedAPETags.find( StringToLower( key ) ); kSupportedAPETags.end() != tag ) {
			const std::string value( &(*item), header.value_size );
			const auto tagType = tag->second;
			m_Tags.insert( { tagType, value } );	
		}

		item += header.value_size;
	}

	return !m_Tags.empty();
}

bool TagReader::ParseID3v1Tag( const std::filesystem::path& filepath )
{
	std::ifstream stream( filepath, std::ios::binary );
	if ( !stream.good() )
		return false;

	std::vector<char> tag( kID3v1TagSize );
	stream.seekg( 0ll - kID3v1TagSize, std::ios::end );
	stream.read( tag.data(), tag.size() );
	if ( stream.good() && ( "TAG" == std::string( tag.data(), 3 ) ) ) {
		constexpr std::pair<uint32_t, uint32_t> kTitleTag = { 3, 30 };
		constexpr std::pair<uint32_t, uint32_t> kArtistTag = { kTitleTag.first + kTitleTag.second, 30 };
		constexpr std::pair<uint32_t, uint32_t> kAlbumTag = { kArtistTag.first + kArtistTag.second, 30 };
		constexpr std::pair<uint32_t, uint32_t> kYearTag = { kAlbumTag.first + kAlbumTag.second, 4 };
		constexpr std::pair<uint32_t, uint32_t> kCommentTag = { kYearTag.first + kYearTag.second, 30 };

		const std::string title = StripWhitespace( CodePageToUTF8( std::string( &tag[ kTitleTag.first ], kTitleTag.second ), kCodePageISO8859_1 ) );
		const std::string artist = StripWhitespace( CodePageToUTF8( std::string( &tag[ kArtistTag.first ], kArtistTag.second ), kCodePageISO8859_1 ) );
		const std::string album = StripWhitespace( CodePageToUTF8( std::string( &tag[ kAlbumTag.first ], kAlbumTag.second ), kCodePageISO8859_1 ) );
		const std::string year( &tag[ kYearTag.first ], kYearTag.second );
		const std::string comment = StripWhitespace( CodePageToUTF8( std::string( &tag[ kCommentTag.first ], kCommentTag.second ), kCodePageISO8859_1 ) );

		const uint8_t genreID = static_cast<uint8_t>( tag[ kID3v1TagSize - 1 ] );
		const std::string genre = ( genreID < kID3Genres.size() ) ? kID3Genres[ genreID ] : std::string();

		// For ID3v1.1, check if the last byte of the comment field represents a track number.
		uint8_t track = 0;
		if ( 0 == tag[ kCommentTag.first + kCommentTag.second - 2 ] ) {
			constexpr uint8_t kMaxTrack = 99;
			if ( const uint8_t value = static_cast<uint8_t>( tag[ kCommentTag.first + kCommentTag.second - 1 ] ); value <= kMaxTrack ) {
				track = value;
			}
		}

		if ( !title.empty() )
			m_Tags.insert( { Tag::Title, title } );
		if ( !artist.empty() )
			m_Tags.insert( { Tag::Artist, artist } );
		if ( !album.empty() )
			m_Tags.insert( { Tag::Album, album } );
		if ( !year.empty() )
			m_Tags.insert( { Tag::Year, year } );
		if ( !comment.empty() )
			m_Tags.insert( { Tag::Comment, comment } );
		if ( !genre.empty() )
			m_Tags.insert( { Tag::Genre, genre } );
		if ( 0 != track )
			m_Tags.insert( { Tag::Track, std::to_string( track ) } );
	}

	return !m_Tags.empty();
}
