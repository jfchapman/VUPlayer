#pragma once

#include <fstream>
#include <vector>

// Encapsulates an Ogg page.
class OggPage
{
public:
	// 'stream' - binary mode input stream.
	// Throws a std::runtime_error exception if a valid page could not be constructed from the stream.
	// The stream, on input, is required to be positioned on a page boundary.
	// On successful construction, the stream will be positioned at the next page boundary.
	OggPage( std::fstream& stream );

	// 'isContinued' - indicates whether the page is continued from a previous page.
	// 'serial' - serial number (should be non-zero).
	// 'sequence' - sequence number (should be non-zero).
	// 'content' - in/out, page content.
	// Constructs a page suitable for a comment header.
	// Throws a std::runtime_error exception if any of the parameters are invalid.
	// Note that if the content is larger than the maximum page size, it will contain the excess data after construction.
	OggPage( const bool isContinued, const uint32_t serial, const uint32_t sequence, std::vector<uint8_t>& content );

	OggPage() = default;
	OggPage( const OggPage& ) = default;
	OggPage& operator=( const OggPage& ) = default;

	// Returns the maximum size of the page content.
	static uint32_t MaximumContentSize;

	// Returns whether this is a continued packet.
	bool IsContinued() const;

	// Sets whether this is a continued packet.
	void SetContinued( const bool continued );

	// Returns whether this is the first page of a logical bitstream (BOS).
	bool IsBOS() const;

	// Sets whether this is the first page of a logical bitstream (BOS).
	void SetBOS( const bool bos );

	// Returns whether this is the last page of a logical bitstream (EOS).
	bool IsEOS() const;

	// Sets whether this is the last page of a logical bitstream (EOS).
	void SetEOS( const bool eos );

	// Gets the granule position.
	uint64_t GetGranulePosition() const;

	// Sets the granule position.
	void SetGranulePosition( const uint64_t granule );

	// Gets the serial number.
	uint32_t GetSerialNumber() const;

	// Sets the serial number.
	void SetSerialNumber( const uint32_t serial );

	// Gets the sequence number.
	uint32_t GetSequenceNumber() const;

	// Sets the sequence number.
	void SetSequenceNumber( const uint32_t sequence );

	// Gets the CRC checksum.
	uint32_t GetCRC() const;

	// Returns whether this page completes a packet.
	bool IsComplete() const;

	// Returns whether this page is the maximum possible size.
	bool IsMaximumSize() const;

	// Returns the page header.
	const std::vector<uint8_t>& GetHeader() const;

	// Returns the page content.
	const std::vector<uint8_t>& GetContent() const;

	// Gets the size of the page, including header and content.
	const uint32_t GetSize() const;

	// Writes the page to the stream, returning whether the page was successfully written.
	bool Write( std::ofstream& stream ) const;

private:
	// Calculates and stores the checksum.
	void CalculateCRC();

	// Returns whether the checksum is correct.
	bool CheckCRC();

	// Generates a CRC lookup table for the polynomial.
	static std::vector<uint32_t> GenerateCRC32LookupTable( const uint32_t polynomial );

	// CRC lookup table.
	static std::vector<uint32_t> s_CRCTable;

	// Page header.
	std::vector<uint8_t> m_Header;
	
	// Page contents.
	std::vector<uint8_t> m_Content;
};
