#include "TagReader.h"

#include "Utility.h"

#include <algorithm>
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
	{ "TDRC", Tag::Year },
	{ "TCOM", Tag::Composer },
	{ "TPE3", Tag::Conductor },
	{ "TPUB", Tag::Publisher },
	{ "COMM", Tag::Comment },
	{ "APIC", Tag::Artwork }
};

// Supported ID3v2 user defined text frame (TXXX) descriptions (in lowercase) and their corresponding tag type.
static const std::map<std::string, Tag> kSupportedID3v2UserTextFrames = {
	{ "replaygain_track_gain", Tag::GainTrack },
	{ "replaygain_album_gain", Tag::GainAlbum }
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

// APE binary items containing front cover and other artwork (in lowercase).
static const std::string kAPEFrontCoverItem = "cover art (front)";
static const std::string kAPEOtherCoverItem = "cover art (other)";

// ISO 8859-1 code page.
constexpr uint32_t kCodePageISO8859_1 = 28591u;

// ID3v2 text encodings.
constexpr char kEncodingISO8859_1 = 0;
constexpr char kEncodingUTF16 = 1;
constexpr char kEncodingUTF16BE = 2;
constexpr char kEncodingUTF8 = 3;

// Returns whether 'encoding' is a valid ID3v2 text encoding.
static bool IsValidID3v2Encoding( const char encoding )
{
	return ( encoding >= kEncodingISO8859_1 ) && ( encoding <= kEncodingUTF8 );
}

// Returns the size in bytes of a null terminated ID3v2 'text' string with the given 'encoding' (excluding the terminator),
// and the size in bytes of the terminator, where 'maxSize' is the maximum number of bytes to scan.
static std::pair<uint32_t, uint32_t> GetID3v2StringSize( const char encoding, const char* text, const uint32_t maxSize )
{
	if ( ( kEncodingUTF16 == encoding ) || ( kEncodingUTF16BE == encoding ) )
		return { 2 * static_cast<uint32_t>( wcsnlen( reinterpret_cast<const wchar_t*>( text ), maxSize / 2 ) ), 2 };
	return { static_cast<uint32_t>( strnlen( text, maxSize ) ), 1 };
}

// Returns the UTF-8 value of the UTF-16 'text' of 'size' bytes (ignoring any trailing odd byte).
static std::string ConvertUTF16ToUTF8( const char* text, const uint32_t size, const bool bigEndian )
{
	const uint32_t textSize = size & ~1u;
	if ( 0 == textSize )
		return {};

	if ( !bigEndian )
		return WideStringToUTF8( std::wstring( reinterpret_cast<const wchar_t*>( text ), textSize / 2 ) );

	std::vector<char> value( text, text + textSize );
	for ( size_t i = 0; i < value.size(); i += 2 ) {
		std::swap( value[ i ], value[ 1 + i ] );
	}
	return WideStringToUTF8( std::wstring( reinterpret_cast<const wchar_t*>( value.data() ), value.size() / 2 ) );
}

TagReader::TagReader( const std::filesystem::path& filepath )
{
	// Returns whether any tags other than ReplayGain values have been read.
	const auto hasMetadata = [ this ] () {
		return std::any_of( m_Tags.begin(), m_Tags.end(), [] ( const auto& tag ) { return ( Tag::GainTrack != tag.first ) && ( Tag::GainAlbum != tag.first ); } );
	};

	ParseID3v2Tag( filepath );
	if ( hasMetadata() ) {
		// Add any ReplayGain values (e.g. as written by MP3Gain) or artwork which are only in an APE tag.
		ParseAPETag( filepath, true /*supplementaryOnly*/ );
	} else if ( !m_UnsupportedID3v2Tag ) {
		// Fall back to other tag types (keeping any ReplayGain values from an ID3v2 tag).
		// If there is an ID3v2 tag which is not supported, don't fall back, so that the caller can use the shell metadata instead.
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
				m_UnsupportedID3v2Tag = IsID3v2TagHeader( tag.data() ) && !IsSupportedID3v2TagHeader( tag.data() );
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

	const uint8_t majorVersion = static_cast<uint8_t>( tag[ 3 ] );

	const auto firstFrame = tag.begin() + kID3v2TagHeaderSize;
	auto frame = firstFrame;

	bool frontCoverFound = false;
	bool commentWithoutDescriptionFound = false;

	while ( kID3v2FrameHeaderSize + std::distance( firstFrame, frame ) <= tagSize ) {
		const std::string frameID( &frame[ 0 ], 4 );

		uint32_t frameSize = 0;
		if ( 4 == majorVersion ) {
			// ID3v2.4 frame sizes are sync-safe integers
			if ( 0 != ( ( frame[ 4 ] | frame[ 5 ] | frame[ 6 ] | frame[ 7 ] ) & 0x80 ) )
				break;
			frameSize = static_cast<uint8_t>( frame[ 4 ] );
			frameSize <<= 7; frameSize |= static_cast<uint8_t>( frame[ 5 ] );
			frameSize <<= 7; frameSize |= static_cast<uint8_t>( frame[ 6 ] );
			frameSize <<= 7; frameSize |= static_cast<uint8_t>( frame[ 7 ] );
		} else {
			frameSize = static_cast<uint8_t>( frame[ 4 ] );
			frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 5 ] );
			frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 6 ] );
			frameSize <<= 8; frameSize |= static_cast<uint8_t>( frame[ 7 ] );
		}

		const std::bitset<8> frameFormatFlags( frame[ 9 ] );

		if ( ( 0 == frameSize ) || ( static_cast<uint64_t>( kID3v2FrameHeaderSize ) + frameSize + std::distance( firstFrame, frame ) > tagSize ) )
			break;

		frame += kID3v2FrameHeaderSize;

		const auto tagID = kSupportedID3v2Frames.find( frameID );
		if ( ( "TXXX" == frameID ) && frameFormatFlags.none() ) {
			// User defined text information frame (only used for ReplayGain values)
			constexpr uint32_t kUserTextDescriptionOffset = 1; // Skips the text encoding
			if ( const auto [ description, value ] = ConvertID3v2DescribedTextFrameToUTF8( &frame[ 0 ], frameSize, kUserTextDescriptionOffset ); !value.empty() ) {
				if ( const auto userTag = kSupportedID3v2UserTextFrames.find( StringToLower( description ) ); kSupportedID3v2UserTextFrames.end() != userTag ) {
					m_Tags.insert( { userTag->second, value } );
				}
			}
		} else if ( ( frameSize > 1 ) && frameFormatFlags.none() && ( kSupportedID3v2Frames.end() != tagID ) ) {
			const auto tagType = tagID->second;
			if ( Tag::Artwork == tagType ) {
				// Picture frame
				if ( const auto [ image, pictureType ] = ConvertID3v2PictureFrameToBase64( &frame[ 0 ], frameSize ); !image.empty() ) {
					constexpr uint8_t kOtherPicture = 0;
					constexpr uint8_t kFrontCover = 3;
					if ( ( kFrontCover == pictureType ) && !frontCoverFound ) {
						// A front cover replaces any other picture already read
						m_Tags.insert_or_assign( tagType, image );
						frontCoverFound = true;
					} else if ( kOtherPicture == pictureType ) {
						m_Tags.insert( { tagType, image } );
					}
				}
			} else if ( Tag::Comment == tagType ) {
				// Comment frame (prefer a comment with no description, and ignore iTunes specific comments)
				constexpr uint32_t kCommentDescriptionOffset = 4; // Skips the text encoding and language
				if ( const auto [ description, value ] = ConvertID3v2DescribedTextFrameToUTF8( &frame[ 0 ], frameSize, kCommentDescriptionOffset ); !value.empty() ) {
					if ( description.empty() ) {
						if ( !commentWithoutDescriptionFound ) {
							m_Tags.insert_or_assign( tagType, value );
							commentWithoutDescriptionFound = true;
						}
					} else if ( !description.starts_with( "iTun" ) ) {
						m_Tags.insert( { tagType, value } );
					}
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

bool TagReader::IsID3v2TagHeader( const char* header )
{
	return ( nullptr != header ) && ( "ID3" == std::string( &header[ 0 ], 3 ) ) &&
		( 0xFF != static_cast<uint8_t>( header[ 3 ] ) ) && ( 0xFF != static_cast<uint8_t>( header[ 4 ] ) ) &&
		( 0 == ( ( header[ 6 ] | header[ 7 ] | header[ 8 ] | header[ 9 ] ) & 0x80 ) );
}

bool TagReader::IsSupportedID3v2TagHeader( const char* header )
{
	if ( !IsID3v2TagHeader( header ) )
		return false;

	// Only accept ID3v2.3 tags with no header flags set, or ID3v2.4 tags with only the footer flag set
	const uint8_t flags = static_cast<uint8_t>( header[ 5 ] );
	switch ( header[ 3 ] ) {
		case 3:
			return 0 == flags;
		case 4: {
			constexpr uint8_t kFooterPresent = 0x10;
			return 0 == ( flags & ~kFooterPresent );
		}
		default:
			return false;
	}
}

uint32_t TagReader::GetID3v2TagSize( const char* header )
{
	if ( !IsSupportedID3v2TagHeader( header ) )
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
		case kEncodingISO8859_1: {
				const std::string value( &frame[ 1 ], frameSize - 1 );
				return CodePageToUTF8( value, kCodePageISO8859_1 );
			}
		case kEncodingUTF16: {
				// UTF-16 with BOM
				if ( frameSize > 4 ) {
					const wchar_t bom = *reinterpret_cast<const wchar_t*>( &frame[ 1 ] );
					if ( 0xFEFF == bom )
						return ConvertUTF16ToUTF8( &frame[ 3 ], frameSize - 3, false /*bigEndian*/ );
					else if ( 0xFFFE == bom )
						return ConvertUTF16ToUTF8( &frame[ 3 ], frameSize - 3, true /*bigEndian*/ );
				}
				return {};
			}
		case kEncodingUTF16BE: {
				// UTF-16 big endian without BOM (ID3v2.4)
				return ConvertUTF16ToUTF8( &frame[ 1 ], frameSize - 1, true /*bigEndian*/ );
			}
		case kEncodingUTF8: {
				// UTF-8 (ID3v2.4)
				return std::string( &frame[ 1 ], strnlen( &frame[ 1 ], frameSize - 1 ) );
			}
		default:
			return {};
	}
}

std::pair<std::string, std::string> TagReader::ConvertID3v2DescribedTextFrameToUTF8( const char* frame, const uint32_t frameSize, const uint32_t descriptionOffset )
{
	if ( ( nullptr == frame ) || ( descriptionOffset < 1 ) || ( frameSize < descriptionOffset + 2 ) )
		return {};

	const char textEncoding = frame[ 0 ];
	if ( !IsValidID3v2Encoding( textEncoding ) )
		return {};

	// Read the short content description string
	const auto [ descriptionLength, terminatorSize ] = GetID3v2StringSize( textEncoding, &frame[ descriptionOffset ], frameSize - descriptionOffset );
	const uint32_t offset = descriptionOffset + descriptionLength + terminatorSize;
	if ( offset >= frameSize )
		return {};

	// Converts encoded 'text' of 'length' bytes, using the frame text encoding
	const auto convertText = [ textEncoding ]( const char* text, const uint32_t length ) {
		std::vector<char> textFrame( 1 + length );
		textFrame[ 0 ] = textEncoding;
		std::copy( text, text + length, 1 + textFrame.begin() );
		return ConvertID3v2TextFrameToUTF8( textFrame.data(), static_cast<uint32_t>( textFrame.size() ) );
	};

	return { convertText( frame + descriptionOffset, descriptionLength ), convertText( frame + offset, frameSize - offset ) };
}

std::pair<std::string, uint8_t> TagReader::ConvertID3v2PictureFrameToBase64( const char* frame, const uint32_t frameSize )
{
	const char textEncoding = frame[ 0 ];
	if ( IsValidID3v2Encoding( textEncoding ) ) {
		// Skip mime type string (always ISO-8859-1)
		uint32_t offset = 1;
		const char* mimeType = &frame[ offset ];
		const uint32_t mimeTypeLength = static_cast<uint32_t>( strnlen( mimeType, frameSize - offset ) );
		offset += 1 + mimeTypeLength;
		if ( offset < frameSize ) {
			const uint8_t pictureType = static_cast<uint8_t>( frame[ offset ] );
			offset += 1;
			if ( offset < frameSize ) {
				// Skip encoded description string
				const auto [ descriptionLength, terminatorSize ] = GetID3v2StringSize( textEncoding, &frame[ offset ], frameSize - offset );
				offset += descriptionLength + terminatorSize;
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

	// ID3v2.4 genres can be a plain numeric genre ID.
	if ( !value.empty() && ( value.size() <= 3 ) && std::all_of( value.begin(), value.end(), []( const char c ) { return ( c >= '0' ) && ( c <= '9' ); } ) ) {
		if ( const auto genreID = std::stoul( value ); genreID < kID3Genres.size() )
			return kID3Genres[ genreID ];
	}

	return value;
}

bool TagReader::ParseAPETag( const std::filesystem::path& filepath, const bool supplementaryOnly )
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

	const size_t initialTagCount = m_Tags.size();

	struct ItemHeader {
		uint32_t value_size = 0;
		uint32_t flags = 0;
	};

	// Whether artwork had already been read (from an ID3v2 tag), and whether a front cover has been read from the APE tag.
	const bool hadArtwork = ( m_Tags.end() != m_Tags.find( Tag::Artwork ) );
	bool frontCoverFound = false;

	auto item = apeTag.begin();
	for ( uint32_t index = 0; ( index < footer.item_count ) && ( std::distance( item, apeTag.end() ) >= sizeof( ItemHeader ) ); index++ ) {
		ItemHeader header;
		std::copy( item, item + sizeof( ItemHeader ), reinterpret_cast<char*>( &header ) );
		item += sizeof( ItemHeader );

		// Note that the item key may be at the end of the tag data (for a malformed tag), so the iterator is not dereferenced.
		const char* itemKey = apeTag.data() + std::distance( apeTag.begin(), item );
		const uint32_t keyLength = static_cast<uint32_t>( strnlen( itemKey, std::distance( item, apeTag.end() ) ) );
		if ( 1ull + keyLength + header.value_size > static_cast<uint64_t>( std::distance( item, apeTag.end() ) ) )
			break;
		const std::string key = ( keyLength > 1 ) ? std::string( itemKey, keyLength ) : std::string();

		item += 1 + keyLength;

		// Items with an empty value are skipped
		if ( header.value_size > 0 ) {
			const std::string lowercaseKey = StringToLower( key );
			if ( const bool frontCover = ( kAPEFrontCoverItem == lowercaseKey ); frontCover || ( kAPEOtherCoverItem == lowercaseKey ) ) {
				// Cover art items consist of a null terminated description (typically a filename), followed by the image data.
				// A front cover is preferred, but artwork which was already read (from an ID3v2 tag) is not replaced.
				if ( !hadArtwork && ( frontCover ? !frontCoverFound : ( m_Tags.end() == m_Tags.find( Tag::Artwork ) ) ) ) {
					const auto valueEnd = item + header.value_size;
					if ( const auto description = std::find( item, valueEnd, 0 ); valueEnd != description ) {
						if ( const std::vector<uint8_t> imageBytes( description + 1, valueEnd ); !imageBytes.empty() ) {
							if ( const std::string image = ConvertImage( imageBytes ); !image.empty() ) {
								m_Tags.insert_or_assign( Tag::Artwork, image );
								frontCoverFound = frontCover;
							}
						}
					}
				}
			} else if ( const auto tag = kSupportedAPETags.find( lowercaseKey ); kSupportedAPETags.end() != tag ) {
				const std::string value( &(*item), header.value_size );
				const auto tagType = tag->second;
				if ( !supplementaryOnly || ( Tag::GainTrack == tagType ) || ( Tag::GainAlbum == tagType ) ) {
					m_Tags.insert( { tagType, value } );
				}
			}
		}

		item += header.value_size;
	}

	return m_Tags.size() > initialTagCount;
}

bool TagReader::ParseID3v1Tag( const std::filesystem::path& filepath )
{
	std::ifstream stream( filepath, std::ios::binary );
	if ( !stream.good() )
		return false;

	const size_t initialTagCount = m_Tags.size();

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
		const std::string year = StripWhitespace( std::string( &tag[ kYearTag.first ], strnlen( &tag[ kYearTag.first ], kYearTag.second ) ) );
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
		if ( 0 != track )
			m_Tags.insert( { Tag::Track, std::to_string( track ) } );

		// Only use the genre if any other fields are present, as a zero filled (blank) tag would otherwise give a genre of 'Blues'.
		if ( !genre.empty() && ( m_Tags.size() > initialTagCount ) )
			m_Tags.insert( { Tag::Genre, genre } );
	}

	return m_Tags.size() > initialTagCount;
}
