#pragma once

#include "Tag.h"

#include <array>

// ID3 and APE tag reader
class TagReader
{
public:
	// 'filepath' - file to scan for tags (in preferential order of ID3v2 > APE > ID3v1, with any ReplayGain values or artwork in an APE tag added to ID3v2 tags).
	TagReader( const std::filesystem::path& filepath );

	// 'id3v2Tag' - ID3v2.3 or ID3v2.4 tag.
	TagReader( const std::vector<char>& id3v2Tag );

	// Returns file tags, or nullopt if no tags were read.
	std::optional<Tags> GetTags() const;

private:
	// Returns whether 'header' is a valid ID3v2 tag header (of any version).
	static bool IsID3v2TagHeader( const char* header );

	// Returns whether 'header' is a supported ID3v2.3 or ID3v2.4 tag header.
	static bool IsSupportedID3v2TagHeader( const char* header );

	// Returns the size represented by an ID3v2.3 or ID3v2.4 tag 'header'.
	static uint32_t GetID3v2TagSize( const char* header );

	// Returns the UTF-8 value of an ID3v2 encoded text frame.
	static std::string ConvertID3v2TextFrameToUTF8( const char* frame, const uint32_t frameSize );

	// Returns a pair containing the UTF-8 short content description and value of an ID3v2 frame consisting of a text encoding,
	// followed by a description string at 'descriptionOffset', followed by a value string (e.g. comment and user defined text frames).
	static std::pair<std::string, std::string> ConvertID3v2DescribedTextFrameToUTF8( const char* frame, const uint32_t frameSize, const uint32_t descriptionOffset );

	// Returns a pair containing the base64 encoded image and picture type of an ID3v2 picture frame.
	static std::pair<std::string, uint8_t> ConvertID3v2PictureFrameToBase64( const char* frame, const uint32_t frameSize );

	// Returns the genre of an ID3v2 content type value.
	static std::string GetID3v2Genre( const std::string& contentType );

	// Parses an ID3v2.3 or ID3v2.4 tag, returning whether any tags were read.
	// https://id3.org/id3v2.3.0
	// https://id3.org/id3v2.4.0-structure
	// https://id3.org/id3v2.4.0-frames
	bool ParseID3v2Tag( const std::filesystem::path& filepath );
	bool ParseID3v2Tag( const std::vector<char>& id3v2Tag );

	// Parses an APE tag, returning whether any tags were added.
	// 'supplementaryOnly' - whether to only read ReplayGain values and artwork (existing values are not replaced).
	// https://wiki.hydrogenaudio.org/index.php?title=APEv2_specification
	bool ParseAPETag( const std::filesystem::path& filepath, const bool supplementaryOnly = false );

	// Parses an ID3v1 tag, returning whether any tags were added (existing values are not replaced).
	bool ParseID3v1Tag( const std::filesystem::path& filepath );

	// Parsed tags.
	Tags m_Tags;

	// Whether the file contains an ID3v2 tag of an unsupported version, or with unsupported header flags.
	bool m_UnsupportedID3v2Tag = false;
};
