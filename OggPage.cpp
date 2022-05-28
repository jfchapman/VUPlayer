#include "OggPage.h"

#include <algorithm>

// CRC lookup table for Ogg checksums.
std::vector<uint32_t> OggPage::s_CRCTable = OggPage::GenerateCRC32LookupTable( 0x04c11db7 );

// Maximum comment header size.
static const uint32_t s_MaxCommentSize = 125829120;

// Maximum page content size.
uint32_t OggPage::MaximumContentSize = 255 * 255;

OggPage::OggPage( std::fstream& stream ) :
	m_Header(),
	m_Content()
{
	bool ok = false;
	if ( stream.good() ) {
		// Read the page header and do some basic checks.
		m_Header.resize( 27 + 255 );
		stream.read( reinterpret_cast<char*>( &m_Header[ 0 ] ), 27 );
		if ( stream.good() && ( 0 == memcmp( &m_Header[ 0 ], "OggS", 4 ) ) && ( 0 == m_Header[ 4 ] ) ) {
			const uint8_t segmentCount = m_Header[ 26 ];
			if ( segmentCount > 0 ) {
				stream.read( reinterpret_cast<char*>( &m_Header[ 27 ] ), segmentCount );
			}
			if ( stream.good() ) {
				// Append the segment table to the header.
				m_Header.resize( 27 + segmentCount );
				uint32_t contentSize = 0;
				auto segment = m_Header.begin() + 27;
				while ( m_Header.end() != segment ) {
					contentSize += *segment++;
				}
				if ( contentSize > 0 ) {
					// Read the packet data.
					m_Content.resize( contentSize );
					stream.read( reinterpret_cast<char*>( &m_Content[ 0 ] ), contentSize );
				}
				if ( stream.good() ) {
					ok = CheckCRC();
				}
			}
		}
	}
	if ( !ok ) {
		throw std::runtime_error( "Could not construct Ogg page from stream." );
	}
}

OggPage::OggPage( const bool isContinued, const uint32_t serial, const uint32_t sequence, std::vector<uint8_t>& content ) :
	m_Header(),
	m_Content()
{
	if ( ( 0 != serial ) && ( 0 != sequence ) && ( content.size() <= s_MaxCommentSize ) ) {
		const uint32_t contentSize = std::min<uint32_t>( MaximumContentSize, static_cast<uint32_t>( content.size() ) );

		uint8_t segmentCount = static_cast<uint8_t>( contentSize / 255 );
		if ( MaximumContentSize != contentSize ) {
			++segmentCount;
		}

		m_Header.resize( 27 + segmentCount, 0 );
		memcpy( &m_Header[ 0 ], "OggS", 4 );
		SetContinued( isContinued );
		SetGranulePosition( ( MaximumContentSize == contentSize ) ? -1 : 0 );
		SetSerialNumber( serial );
		SetSequenceNumber( sequence );
		m_Header[ 26 ] = segmentCount;

		uint32_t remainingContent = contentSize;
		auto segment = m_Header.begin() + 27;
		while ( m_Header.end() != segment ) {
			const uint8_t segmentSize = static_cast<uint8_t>( std::min<uint32_t>( static_cast<uint32_t>( 255 ), remainingContent ) );
			remainingContent -= segmentSize;
			*segment++ = segmentSize;
		}

		m_Content.reserve( contentSize );
		m_Content.insert( m_Content.end(), content.begin(), content.begin() + contentSize );
		CalculateCRC();

		if ( MaximumContentSize == contentSize ) {
			content.erase( content.begin(), content.begin() + contentSize );
		}
	} else {
		throw std::runtime_error( "Could not construct Ogg page from content." );
	}
}

bool OggPage::IsContinued() const
{
	const bool continued = m_Header[ 5 ] & 0x01;
	return continued;
}

void OggPage::SetContinued( const bool continued )
{
	if ( IsContinued() != continued ) {
		m_Header[ 5 ] |= 0x01;
		CalculateCRC();
	}
}

bool OggPage::IsBOS() const
{
	const bool bos = m_Header[ 5 ] & 0x02;
	return bos;
}

void OggPage::SetBOS( const bool bos )
{
	if ( IsBOS() != bos ) {
		m_Header[ 5 ] |= 0x02;
		CalculateCRC();
	}
}

bool OggPage::IsEOS() const
{
	const bool eos = m_Header[ 5 ] & 0x04;
	return eos;
}

void OggPage::SetEOS( const bool eos )
{
	if ( IsEOS() != eos ) {
		m_Header[ 5 ] |= 0x04;
		CalculateCRC();
	}
}

uint64_t OggPage::GetGranulePosition() const
{
	const uint64_t granule = 
		( static_cast<uint64_t>( m_Header[ 13 ] ) << 56 ) |
		( static_cast<uint64_t>( m_Header[ 12 ] ) << 48 ) |
		( static_cast<uint64_t>( m_Header[ 11 ] ) << 40 ) |
		( static_cast<uint64_t>( m_Header[ 10 ] ) << 32 ) |
		( static_cast<uint64_t>( m_Header[ 9 ] ) << 24 ) |
		( static_cast<uint64_t>( m_Header[ 8 ] ) << 16 ) |
		( static_cast<uint64_t>( m_Header[ 7 ] ) << 8 ) |
		static_cast<uint64_t>( m_Header[ 6 ] );
	return granule;
}

void OggPage::SetGranulePosition( const uint64_t granule )
{
	if ( GetGranulePosition() != granule ) {
		m_Header[ 6 ] = granule & 0xff;
		m_Header[ 7 ] = ( granule >> 8 ) & 0xff;
		m_Header[ 8 ] = ( granule >> 16 ) & 0xff;
		m_Header[ 9 ] = ( granule >> 24 ) & 0xff;
		m_Header[ 10 ] = ( granule >> 32 ) & 0xff;
		m_Header[ 11 ] = ( granule >> 40 ) & 0xff;
		m_Header[ 12 ] = ( granule >> 48 ) & 0xff;
		m_Header[ 13 ] = ( granule >> 56 ) & 0xff;
		CalculateCRC();
	}
}

uint32_t OggPage::GetSerialNumber() const
{
	const uint32_t serial = ( m_Header[ 17 ] << 24 ) | ( m_Header[ 16 ] << 16 ) | ( m_Header[ 15 ] << 8 ) | ( m_Header[ 14 ] );
	return serial;
}

void OggPage::SetSerialNumber( const uint32_t serial )
{
	if ( GetSerialNumber() != serial ) {
		m_Header[ 14 ] = serial & 0xff;
		m_Header[ 15 ] = ( serial >> 8 ) & 0xff;
		m_Header[ 16 ] = ( serial >> 16 ) & 0xff;
		m_Header[ 17 ] = ( serial >> 24 ) & 0xff;
		CalculateCRC();
	}
}

uint32_t OggPage::GetSequenceNumber() const
{
	const uint32_t sequence = ( m_Header[ 21 ] << 24 ) | ( m_Header[ 20 ] << 16 ) | ( m_Header[ 19 ] << 8 ) | ( m_Header[ 18 ] );
	return sequence;
}

void OggPage::SetSequenceNumber( const uint32_t sequence )
{
	if ( GetSequenceNumber() != sequence ) {
		m_Header[ 18 ] = sequence & 0xff;
		m_Header[ 19 ] = ( sequence >> 8 ) & 0xff;
		m_Header[ 20 ] = ( sequence >> 16 ) & 0xff;
		m_Header[ 21 ] = ( sequence >> 24 ) & 0xff;
		CalculateCRC();
	}
}

uint32_t OggPage::GetCRC() const
{
	const uint32_t crc = ( m_Header[ 25 ] << 24 ) | ( m_Header[ 24 ] << 16 ) | ( m_Header[ 23 ] << 8 ) | ( m_Header[ 22 ] );
	return crc;
}

std::vector<uint32_t> OggPage::GenerateCRC32LookupTable( const uint32_t polynomial )
{
	std::vector<uint32_t> table( 256, 0 );
	for ( uint32_t i = 0; i < 256; i++ ) {
		uint32_t crc = i << 24;
		for ( uint32_t j = 8; j > 0; j-- ) {
			if ( crc & 0x80000000 ) {
				crc = ( crc << 1 ) ^ polynomial;
			} else {
				crc = crc << 1;
			}
		}
		table[ i ] = crc;
	}
	return table;
}

bool OggPage::CheckCRC()
{
	const uint32_t crc = GetCRC();
	CalculateCRC();
	const bool valid = ( GetCRC() == crc );
	return valid;
}

void OggPage::CalculateCRC()
{
	// Blank out the CRC field before computing the checksum.
	m_Header[ 22 ] = 0;
	m_Header[ 23 ] = 0;
	m_Header[ 24 ] = 0;
	m_Header[ 25 ] = 0;

	uint32_t crc = 0;
	for ( const auto& i : m_Header ) {
		crc = ( crc << 8 ) ^ s_CRCTable[ ( ( crc >> 24 ) & 0xff ) ^ i ];
	}
	for ( const auto& i : m_Content ) {
		crc = ( crc << 8 ) ^ s_CRCTable[ ( ( crc >> 24 ) & 0xff ) ^ i ];
	}

	m_Header[ 22 ] = crc & 0xff;
	m_Header[ 23 ] = ( crc >> 8 ) & 0xff;
	m_Header[ 24 ] = ( crc >> 16 ) & 0xff;
	m_Header[ 25 ] = ( crc >> 24 ) & 0xff;
}

bool OggPage::IsComplete() const
{
	bool isComplete = true;
	const uint8_t segmentCount = m_Header[ 26 ];
	if ( segmentCount > 0 ) {
		const uint8_t lastSegmentSize = m_Header[ 26 + segmentCount ];
		isComplete = ( 0xff != lastSegmentSize );
	}
	return isComplete;
}

bool OggPage::IsMaximumSize() const
{
	const bool isMaximumSize = ( MaximumContentSize == GetContent().size() );
	return isMaximumSize;
}

const std::vector<uint8_t>& OggPage::GetHeader() const
{
	return m_Header;
}

const std::vector<uint8_t>& OggPage::GetContent() const
{
	return m_Content;
}

const uint32_t OggPage::GetSize() const
{
	const uint32_t size = static_cast<uint32_t>( m_Header.size() + m_Content.size() );
	return size;
}

bool OggPage::Write( std::ofstream& stream ) const
{
	stream.write( reinterpret_cast<const char*>( GetHeader().data() ), GetHeader().size() );
	if ( !GetContent().empty() ) {
		stream.write( reinterpret_cast<const char*>( GetContent().data() ), GetContent().size() );
	}
	const bool success = stream.good();
	return success;
}
