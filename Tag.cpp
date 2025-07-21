#include "Tag.h"

#include "Utility.h"

#include "json.hpp"

#include <fstream>
#include <array>
#include <set>

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
