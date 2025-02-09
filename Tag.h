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
	Artwork,
	Composer,
	Conductor,
	Publisher
};

// Maps a tag type to the UTF-8 encoded tag content.
using Tags = std::map<Tag, std::string>;

// Gets any APE 'tags' from 'filename', returning true if any tags were retrieved.
bool GetAPETags( const std::wstring& filename, Tags& tags );
