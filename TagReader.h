#pragma once

#include "Tag.h"

#include <array>

// ID3 and APE tag reader
class TagReader
{
public:
	// 'filepath' - file to scan for tags (in preferential order of ID3v2 > APE > ID3v1).
	TagReader( const std::filesystem::path& filepath );

	// 'id3v2Tag' - ID3v2.3 tag.
	TagReader( const std::vector<char>& id3v2Tag );

	// Returns file tags, or nullopt if no tags were read.
	std::optional<Tags> GetTags() const;

private:
	// Returns the size represented by an ID3v2.3 tag 'header'.
	static uint32_t GetID3v2TagSize( const char* header );

	// Returns the UTF-8 value of an ID3v2.3 encoded text frame. 
	static std::string ConvertID3v2TextFrameToUTF8( const char* frame, const uint32_t frameSize );

	// Returns the UTF-8 value of an ID3v2.3 comment frame. 
	static std::string ConvertID3v2CommentFrameToUTF8( const char* frame, const uint32_t frameSize );

	// Returns a pair containing the base64 encoded image and picture type of an ID3v2.3 picture frame. 
	static std::pair<std::string, uint8_t> ConvertID3v2PictureFrameToBase64( const char* frame, const uint32_t frameSize );

	// Returns the genre of an ID3v2.3 content type value.
	static std::string GetID3v2Genre( const std::string& contentType );

	// Parses an ID3v2.3 tag, returning whether any tags were read.
	// https://id3.org/id3v2.3.0
	bool ParseID3v2Tag( const std::filesystem::path& filepath );
	bool ParseID3v2Tag( const std::vector<char>& id3v2Tag );

	// Parses an APE tag, returning whether any tags were read.
	// https://wiki.hydrogenaudio.org/index.php?title=APEv2_specification
	bool ParseAPETag( const std::filesystem::path& filepath );

	// Parses an ID3v1 tag, returning whether any tags were read.
	bool ParseID3v1Tag( const std::filesystem::path& filepath );

	// Parsed tags.
	Tags m_Tags;
};
