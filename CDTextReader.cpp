#include "CDTextReader.h"

#include "Utility.h"

#include <array>

// Size of a pack, in bytes.
static constexpr size_t kPackSize = 18;

// Offset and size of the text in a pack, in bytes.
static constexpr size_t kTextOffset = 4;
static constexpr size_t kTextSize = 12;

// Pack types.
static constexpr unsigned char kPackTitle = 0x80;
static constexpr unsigned char kPackPerformer = 0x81;
static constexpr unsigned char kPackSongwriter = 0x82;
static constexpr unsigned char kPackComposer = 0x83;
static constexpr unsigned char kPackMessage = 0x85;
static constexpr unsigned char kPackGenre = 0x87;
static constexpr unsigned char kPackSizeInformation = 0x8f;

// Pack types which consist of a string for each track, and the corresponding track information field.
static const std::map<unsigned char, std::wstring CDTextReader::Track::*> kTrackStringPacks = {
	{ kPackTitle, &CDTextReader::Track::Title },
	{ kPackPerformer, &CDTextReader::Track::Performer },
	{ kPackSongwriter, &CDTextReader::Track::Songwriter },
	{ kPackComposer, &CDTextReader::Track::Composer },
	{ kPackMessage, &CDTextReader::Track::Message }
};

// Character codes (from the first size information pack of a block).
static constexpr unsigned char kCharacterCodeKorean = 0x81;
static constexpr unsigned char kCharacterCodeMandarin = 0x82;

// Maximum track number.
static constexpr long kMaxTrack = 99;

CDTextReader::CDTextReader( const std::vector<unsigned char>& packs ) :
	m_Blocks()
{
	// The text for each pack type and block, which (other than for the genre) forms a sequence of null terminated strings for consecutive tracks.
	struct TextSequence {
		long FirstTrack = 0;
		bool DoubleByte = false;
		std::string Text;
	};
	std::map<std::pair<unsigned char /*packType*/, long /*block*/>, TextSequence> sequences;
	std::map<long /*block*/, unsigned char /*characterCode*/> characterCodes;

	for ( size_t offset = 0; ( offset + kPackSize ) <= packs.size(); offset += kPackSize ) {
		const unsigned char* pack = packs.data() + offset;
		const unsigned char packType = pack[ 0 ];
		const bool extension = ( 0 != ( pack[ 1 ] & 0x80 ) );
		const long track = pack[ 1 ] & 0x7f;
		const bool doubleByte = ( 0 != ( pack[ 3 ] & 0x80 ) );
		const long block = ( pack[ 3 ] >> 4 ) & 0x07;
		if ( extension ) {
			continue;
		}

		if ( kPackSizeInformation == packType ) {
			// The first size information pack of a block starts with the character code.
			if ( 0 == track ) {
				characterCodes.insert( { block, pack[ kTextOffset ] } );
			}
		} else if ( ( kPackGenre == packType ) || kTrackStringPacks.contains( packType ) ) {
			const auto [ sequence, inserted ] = sequences.try_emplace( { packType, block } );
			if ( inserted ) {
				sequence->second.FirstTrack = track;
				sequence->second.DoubleByte = doubleByte;
			}
			sequence->second.Text.append( reinterpret_cast<const char*>( pack + kTextOffset ), kTextSize );
		}
	}

	for ( const auto& [ key, sequence ] : sequences ) {
		const auto& [ packType, block ] = key;
		const auto characterCode = characterCodes.find( block );
		const UINT codePage = GetCodePage( ( characterCodes.end() != characterCode ) ? std::make_optional( characterCode->second ) : std::nullopt, sequence.DoubleByte );

		if ( kPackGenre == packType ) {
			// The disc genre consists of a two byte genre code, followed by an optional (null terminated) description, which is preferred if present.
			if ( sequence.Text.size() >= 2 ) {
				const unsigned int code = ( static_cast<unsigned char>( sequence.Text[ 0 ] ) << 8 ) | static_cast<unsigned char>( sequence.Text[ 1 ] );
				const std::string description = sequence.Text.substr( 2, sequence.Text.find( '\0', 2 ) - 2 );
				std::wstring genre = description.empty() ? std::wstring() : CodePageToWideString( description, codePage );
				if ( genre.empty() ) {
					genre = GetGenre( code );
				}
				if ( !genre.empty() ) {
					m_Blocks[ block ][ 0 ].Genre = genre;
				}
			}
			continue;
		}

		// Strings are terminated by a null character (two null bytes for double byte text), and a tab character indicates the same string as the previous track.
		const auto field = kTrackStringPacks.at( packType );
		const size_t characterSize = sequence.DoubleByte ? 2 : 1;
		const std::string terminator( characterSize, '\0' );
		const std::string tab( characterSize, '\t' );
		long track = sequence.FirstTrack;
		std::wstring previousString;
		size_t start = 0;
		for ( size_t position = 0; ( ( position + characterSize ) <= sequence.Text.size() ) && ( track <= kMaxTrack ); position += characterSize ) {
			if ( 0 == sequence.Text.compare( position, characterSize, terminator ) ) {
				const std::string text = sequence.Text.substr( start, position - start );
				const std::wstring value = ( tab == text ) ? previousString : CodePageToWideString( text, codePage );
				if ( !value.empty() ) {
					m_Blocks[ block ][ track ].*field = value;
				}
				previousString = value;
				++track;
				start = position + characterSize;
			}
		}
	}
}

const CDTextReader::Blocks& CDTextReader::GetBlocks() const
{
	return m_Blocks;
}

UINT CDTextReader::GetCodePage( const std::optional<unsigned char>& characterCode, const bool doubleByte )
{
	if ( doubleByte ) {
		if ( kCharacterCodeKorean == characterCode ) {
			return 949;
		}
		if ( kCharacterCodeMandarin == characterCode ) {
			return 936;
		}
		// MS-JIS (Shift-JIS).
		return 932;
	}
	// ISO 8859-1 or ASCII, using Windows-1252 (a superset of the printable ISO 8859-1 characters).
	return 1252;
}

std::wstring CDTextReader::GetGenre( const unsigned int code )
{
	// Genre codes 0 (not used) and 1 (not defined) have no name.
	static const std::array<const wchar_t*, 28> kGenres = {
		L"", L"", L"Adult Contemporary", L"Alternative Rock", L"Children's Music", L"Classical", L"Contemporary Christian", L"Country", L"Dance",
		L"Easy Listening", L"Erotic", L"Folk", L"Gospel", L"Hip Hop", L"Jazz", L"Latin", L"Musical", L"New Age", L"Opera", L"Operetta",
		L"Pop Music", L"Rap", L"Reggae", L"Rock Music", L"Rhythm & Blues", L"Sound Effects", L"Spoken Word", L"World Music"
	};
	return ( code < kGenres.size() ) ? kGenres[ code ] : std::wstring();
}
