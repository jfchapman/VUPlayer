#include "Tag.h"

#include "Utility.h"

#include "json.hpp"

#include <fstream>
#include <array>
#include <set>

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

// Maps a tag type to the corresponding tag name (for JSON storage).
static const std::map<Tag, std::string> kTagNames = {
	{ Tag::Artist,              "artist" },
	{ Tag::Title,               "title" },
	{ Tag::Album,               "album" },
	{ Tag::Genre,               "genre" },
	{ Tag::Year,                "year" },
	{ Tag::Comment,             "comment" },
	{ Tag::Track,               "track" },
	{ Tag::Version,             "version" },
	{ Tag::GainTrack,           "gainTrack" },
	{ Tag::GainAlbum,           "gainAlbum" },
	{ Tag::Artwork,             "artwork" },
	{ Tag::Composer,            "composer" },
	{ Tag::Conductor,           "conductor" },
	{ Tag::Publisher,           "publisher" }
};

// Preferred APE tag names (from the supported types).
static const std::set<std::string> kPreferredAPETags = { "album", "artist", "comment", "replaygain_album_gain", "replaygain_track_gain", "genre", "title", "track", "year", "composer", "conductor", "publisher" };

bool GetAPETags( const std::wstring& filename, Tags& tags )
{
	std::ifstream stream( filename, std::ios::binary );

	struct Footer {
		std::array<char, 8>   magic = {};
		uint32_t              version = 0;
		uint32_t              tag_size = 0;
		uint32_t              item_count = 0;
		uint32_t              flags = 0;
		std::array<char, 8>   reserved = {};
	};

	constexpr uint32_t kFlagContainsFooter = 1 << 30;

	stream.seekg( 0ll - sizeof( Footer ), std::ios::end );
	Footer f;
	stream.read( reinterpret_cast<char*>( &f ), sizeof( Footer ) );
	const auto filesize = stream.tellg();
	bool validFooter = ( stream.gcount() == sizeof( Footer ) ) &&
		( 0 == std::strncmp( f.magic.data(), "APETAGEX", 8 ) ) &&
		( ( 1000 == f.version ) || ( 2000 == f.version ) ) &&
		( f.flags & kFlagContainsFooter ) &&
		( f.tag_size < filesize ) &&
		( f.tag_size > sizeof( Footer ) ) &&
		( f.item_count > 0 );

	if ( !validFooter )
		return false;

	struct Header {
		uint32_t value_size = 0;
		uint32_t flags = 0;
	};

	constexpr uint32_t kFlagsNonUTF8 = 6;
	constexpr uint32_t kMaxValueSize = 0x100000;

	tags.clear();

	stream.seekg( 0ll - f.tag_size, std::ios::end );
	for ( uint32_t index = 0; stream.good() && ( index < f.item_count ); index++ ) {
		Header header;
		stream.read( reinterpret_cast<char*>( &header ), sizeof( Header ) );

		std::string key;
		char c = 0;
		stream.get( c );
		while ( c && stream.good() ) {
			key.push_back( c );
			stream.get( c );
		}

		const auto tagInfo = kSupportedAPETags.find( StringToLower( key ) );
		if ( ( header.flags & kFlagsNonUTF8 ) || ( header.value_size > kMaxValueSize ) || ( kSupportedAPETags.end() == tagInfo ) ) {
			stream.seekg( header.value_size, std::ios::cur );
		} else {
			std::string value( 1 + header.value_size, 0 );
			stream.read( value.data(), header.value_size );
			if ( stream.good() ) {
				const auto& tagName = tagInfo->first;
				const auto tagType = tagInfo->second;
				if ( const auto preferredTag = kPreferredAPETags.find( tagName ); kPreferredAPETags.end() != preferredTag ) {
					tags[ tagType ] = value;
				} else {
					tags.insert( { tagType, value } );
				}
			}
		}
	}

	return !tags.empty();
}

bool TagsToJSON( const Tags& tags, const std::filesystem::path& filename )
{
	if ( tags.empty() )
		return false;

	try {
		nlohmann::json doc;
		for ( const auto& [tagType, tagValue] : tags ) {
			if ( const auto tagName = kTagNames.find( tagType ); kTagNames.end() != tagName ) {
				doc[ tagName->second ] = tagValue;
			}
		}
		if ( !doc.empty() ) {
			std::ofstream stream( filename );
			stream << doc;
			return true;
		}
	} catch ( const nlohmann::json::exception& ) {}
	return false;
}

std::optional<Tags> TagsFromJSON( const std::filesystem::path& filename )
{
	try {
		std::ifstream stream( filename );
		const nlohmann::json doc = nlohmann::json::parse( stream );
		Tags tags;
		for ( const auto& [tagType, tagName] : kTagNames ) {
			if ( const auto tag = doc.find( tagName ); ( doc.end() != tag ) && tag->is_string() ) {
				tags.insert( { tagType, *tag } );
			}
		}
		if ( !tags.empty() ) {
			return tags;
		}
	} catch ( const nlohmann::json::exception& ) {}
	return std::nullopt;
}
