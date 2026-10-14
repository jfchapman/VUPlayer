#pragma once

#include "ogg/ogg.h"

#include <fstream>
#include <string>
#include <vector>

// Ogg Vorbis & Opus comment handler.
class OggComment
{
public:
	// Codec of the logical bitstream containing the comments.
	enum class Type {
		Vorbis,
		Opus
	};

	// 'filename' - Ogg filename.
	// 'type' - codec of the logical bitstream containing the comments.
	// 'readonly' - true to open the file read only, false to allow for modification of comments.
	// Throws a std::runtime_error exception if the comments could not be read from the first logical bitstream of the codec type.
	OggComment( const std::wstring& filename, const Type type, const bool readonly = true );

	virtual ~OggComment();

	// Returns the vendor string (UTF-8 encoded).
	const std::string& GetVendor() const;

	// Returns the user comment field/value pairs (UTF-8 encoded).
	const std::vector<std::pair<std::string, std::string>>& GetUserComments() const;

	// Adds a user comment field/value pair (UTF-8 encoded).
	void AddUserComment( const std::string& field, const std::string& value );

	// Removes all user comments matching the field (UTF-8 encoded).
	void RemoveUserComment( const std::string& field );

	// Returns the front cover artwork as a base64 encoded image, or an empty string if there is no front cover.
	std::string GetArtwork() const;

	// Sets the front cover artwork.
	// 'encodedImage' - base64 encoded image, or an empty string to remove the front cover.
	void SetArtwork( const std::string& encodedImage );

	// Writes modified comments out to file, returning whether the comments were successfully written.
	bool WriteComments();

	// Maximum comment header size.
	static constexpr uint32_t MaximumCommentSize = 125829120;

private:
	// Original header page information.
	struct PageInfo {
		// File offset to the start of the page.
		long long Offset;

		// Page size, including header and body.
		long Size;

		// Page sequence number.
		long Sequence;

		// Whether this is the last page of the logical bitstream.
		bool EOS;
	};

	// Reads pages from a binary mode input stream, positioned at the start of the file.
	class PageReader
	{
	public:
		PageReader( std::istream& stream );
		~PageReader();

		PageReader( const PageReader& ) = delete;
		PageReader& operator=( const PageReader& ) = delete;

		// Reads the next 'page', returning false if there are no more pages.
		// 'offset' - out, file offset to the start of the page.
		// Note that the page data is only valid until the next call.
		bool Read( ogg_page& page, long long& offset );

		// Returns whether the stream so far has consisted only of valid pages.
		// Once all pages have been read, this also indicates that the stream ended cleanly on a page boundary.
		bool IsValid() const;

	private:
		// Input stream.
		std::istream& m_Stream;

		// Sync state.
		ogg_sync_state m_Sync;

		// File offset to the next unread page data.
		long long m_Offset;

		// Total number of bytes read from the stream.
		long long m_BytesRead;

		// Whether the stream so far has consisted only of valid pages.
		bool m_Valid;
	};

	// Encapsulates the stream state for a logical bitstream.
	class StreamState
	{
	public:
		StreamState( const int serial );
		~StreamState();

		StreamState( const StreamState& ) = delete;
		StreamState& operator=( const StreamState& ) = delete;

		// Stream state.
		ogg_stream_state m_State;
	};

	// Returns the number of header packets for the codec.
	size_t GetHeaderCount() const;

	// Returns whether 'data' contains the header packet signature for the codec.
	// 'index' - header packet index.
	bool HasHeaderSignature( const size_t index, const uint8_t* data, const size_t size ) const;

	// Returns the comment header packet signature for the codec.
	std::string GetCommentSignature() const;

	// Parses the comment header packet, returning whether the packet was valid.
	bool ParseCommentPacket( const std::vector<uint8_t>& packet );

	// Generates a comment header packet from the current comments (without any padding).
	std::vector<uint8_t> GenerateCommentPacket() const;

	// Generates the header pages, with the comment header packet replaced by 'commentPacket'.
	// Returns the pages (header & body), or an empty vector if the pages could not be generated.
	std::vector<std::vector<uint8_t>> GenerateHeaderPages( const std::vector<uint8_t>& commentPacket ) const;

	// Returns whether the header 'pages' exactly match the layout of the original header pages, so that they can be written in-place.
	bool CanWriteInPlace( const std::vector<std::vector<uint8_t>>& pages ) const;

	// Writes out header 'pages' in-place, returning whether the pages were written.
	bool WriteInPlace( const std::vector<std::vector<uint8_t>>& pages );

	// Attempts to rewrite the whole file with the header 'pages', returning whether the file was written.
	bool Rewrite( const std::vector<std::vector<uint8_t>>& pages );

	// Gets a picture.
	// 'pictureType' - picture type (https://xiph.org/flac/format.html#metadata_block_picture).
	// 'picture' - out, picture data.
	// Returns whether the picture was got.
	// Note that if there are multiple pictures of the same type, the first one is returned.
	bool GetPicture( const uint32_t pictureType, std::vector<uint8_t>& picture ) const;

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
	void RemovePicture( const uint32_t pictureType );

	// Converts the 4-byte value at an offset in the (little-endian) data to a unsigned 32 bit value.
	static uint32_t ToUint32LE( const std::vector<uint8_t>& data, const size_t offset );

	// Converts the 4-byte value at an offset in the (big-endian) data to a unsigned 32 bit value.
	static uint32_t ToUint32BE( const std::vector<uint8_t>& data, const size_t offset );

	// Appends an unsigned 32 bit value to the data, as 4 little-endian bytes.
	static void AppendLE( const uint32_t value, std::vector<uint8_t>& data );

	// Appends an unsigned 32 bit value to the data, as 4 big-endian bytes.
	static void AppendBE( const uint32_t value, std::vector<uint8_t>& data );

	// Ogg file name.
	const std::wstring m_Filename;

	// Codec type.
	const Type m_Type;

	// Whether the file was opened read only.
	const bool m_Readonly;

	// Ogg stream.
	std::fstream m_Stream;

	// Serial number of the logical bitstream containing the comments.
	int m_Serial;

	// Original header packets.
	std::vector<std::vector<uint8_t>> m_HeaderPackets;

	// Original header pages.
	std::vector<PageInfo> m_HeaderPages;

	// Whether the original header layout allows the comments to be written.
	bool m_Writable;

	// Vendor string (UTF-8 encoded).
	std::string m_Vendor;

	// User comment field/value pairs (UTF-8 encoded).
	std::vector<std::pair<std::string, std::string>> m_Comments;

	// User comments without a field/value delimiter, preserved as-is (UTF-8 encoded).
	std::vector<std::string> m_RawComments;

	// Trailing binary data (Opus only).
	std::vector<uint8_t> m_BinaryData;
};
