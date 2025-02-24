#pragma once

#include <map>
#include <string>
#include <filesystem>
#include <optional>

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

// Writes tags to a JSON formatted file, returning true if the tags were written.
bool TagsToJSON( const Tags& tags, const std::filesystem::path& filename );

// Converts tags from a JSON formatted file, returning the tags if successful, nullopt if not.
std::optional<Tags> TagsFromJSON( const std::filesystem::path& filename );