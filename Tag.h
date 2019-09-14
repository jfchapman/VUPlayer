#pragma once

#include <map>
#include <string>

// Tag types.
enum class Tag {
	Artist,
	Title,
	Album,
	Genre,
	Year,
	Comment,
	Track,
	Version,
	GainTrack,
	GainAlbum,
	Artwork
};

// Maps a tag type to the UTF-8 encoded tag content.
typedef std::map<Tag,std::string> Tags;
