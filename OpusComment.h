#pragma once

#include "OggPage.h"

#include <map>
#include <string>
#include <vector>

// Opus comment handler
class OpusComment
{
public:
	// 'filename' - Opus filename.
	// 'readonly' - true to open the file read only, false to allow for modification of comments.
	// Throws a std::runtime_error exception if the file could not be identified as an Opus file.
	// On successful construction, the comments from the first logical Opus bitstream will be read.
	OpusComment( const std::wstring& filename, const bool readonly = true );

	virtual ~OpusComment();

	// Returns the vendor string (UTF-8 encoded).
	const std::string& GetVendor() const;

	// Returns the user comment field/value pairs (UTF-8 encoded).
	const std::vector<std::pair<std::string, std::string>>& GetUserComments() const;

	// Adds a user comment field/value pair (UTF-8 encoded).
	void AddUserComment( const std::string& field, const std::string& value );

	// Removes all user comments matching the field (UTF-8 encoded).
	void RemoveUserComment( const std::string& field );

	// Gets a picture.
	// 'pictureType' - picture type (https://xiph.org/flac/format.html#metadata_block_picture).
	// 'mimeType' - out, MIME type.
	// 'description' - out, picture description (UTF-8 encoded).
	// 'width' - out, width in pixels.
	// 'height' - out, height in pixels.
	// 'depth' - out, colour depth in bits per pixel.
	// 'colours' - out, number of colours used for indexed colour pictures (GIF), or zero for non-indexed colour pictures.
	// 'picture' - out, picture data.
	// Returns whether the picture was got. 
	// Note that if there are multiple pictures of the same type, the first one is returned.
	bool GetPicture( const uint32_t pictureType, std::string& mimeType, std::string& description,
		uint32_t& width, uint32_t& height, uint32_t& depth, uint32_t& colours, std::vector<uint8_t>& picture ) const;

	// Adds a picture.
	// 'pictureType' - picture type.
	// 'mimeType' - MIME type.
	// 'description' - picture description (UTF-8 encoded).
	// 'width' - width in pixels.
	// 'height' - height in pixels.
	// 'depth' - colour depth in bits per pixel.
	// 'colours' - number of colours used for indexed colour pictures (GIF), or zero for non-indexed colour pictures.
	// 'picture' - picture data.
	void AddPicture( const uint32_t pictureType, const std::string& mimeType, const std::string& description,
		const uint32_t width, const uint32_t height, const uint32_t depth, const uint32_t colours, const std::vector<uint8_t>& picture );

	// Removes all pictures matching the picture type.
	void RemovePicture( const uint32_t type );

	// Writes modified comments out to file, returning whether the comments were successfully written.
	bool WriteComments();

private:
	// Returns whether the ogg page is a valid Opus header.
	static bool IsOpusHeader( const OggPage& page );

	// Returns whether the ogg page is the first page of an Opus comment.
	static bool IsCommentHeader( const OggPage& page );

	// Converts the 4-byte value at an offset in the (little-endian) data to a unsigned 32 bit value.
	static uint32_t ToUint32LE( const std::vector<uint8_t>& data, const uint32_t offset );

	// Converts the 4-byte value at an offset in the (big-endian) data to a unsigned 32 bit value.
	static uint32_t ToUint32BE( const std::vector<uint8_t>& data, const uint32_t offset );

	// Converts an unsigned 32 bit value to 4-byte value at an offset in the (little-endian) data.
	static void ToBytesLE( const uint32_t value, std::vector<uint8_t>& data, const uint32_t offset );

	// Converts an unsigned 32 bit value to 4-byte value at an offset in the (big-endian) data.
	static void ToBytesBE( const uint32_t value, std::vector<uint8_t>& data, const uint32_t offset );

	// Opus file name.
	std::wstring m_Filename;

	// Opus stream.
	std::fstream m_Stream;

	// Original Opus comment page(s), keyed by the file offset to the start of the page.
	std::map<long long, OggPage> m_OriginalPages;

	// Vendor string (UTF-8 encoded).
	std::string m_Vendor;

	// User comment name/value pairs (UTF-8 encoded).
	std::vector<std::pair<std::string, std::string>> m_Comments;

	// Trailing binary data.
	std::vector<uint8_t> m_BinaryData;
};
