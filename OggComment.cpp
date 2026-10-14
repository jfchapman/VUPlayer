#include "OggComment.h"

#include "Utility.h"

#include <algorithm>
#include <cstring>
#include <iterator>

// https://xiph.org/vorbis/doc/Vorbis_I_spec.html
// https://tools.ietf.org/html/rfc7845.html

// Maximum page content size.
static constexpr size_t s_MaxPageContentSize = 255 * 255;

// Front cover picture type.
static constexpr uint32_t s_FrontCover = 3;

OggComment::OggComment( const std::wstring& filename, const Type type, const bool readonly ) :
	m_Filename( filename ),
	m_Type( type ),
	m_Readonly( readonly ),
	m_Stream( filename, ( readonly ? ( std::ios::in | std::ios::binary ) : ( std::ios::in | std::ios::out | std::ios::binary ) ), _SH_DENYWR ),
	m_Serial( 0 ),
	m_HeaderPackets(),
	m_HeaderPages(),
	m_Writable( false ),
	m_Vendor(),
	m_Comments(),
	m_RawComments(),
	m_BinaryData()
{
	bool readComments = false;
	if ( m_Stream.is_open() ) {
		PageReader reader( m_Stream );
		ogg_page page = {};
		long long offset = 0;

		// Find the first logical bitstream of the codec type (the initial pages of all logical bitstreams are grouped together at the start).
		bool found = false;
		while ( !found && reader.Read( page, offset ) && ( 0 != ogg_page_bos( &page ) ) ) {
			found = HasHeaderSignature( 0, page.body, static_cast<size_t>( page.body_len ) );
		}

		if ( found ) {
			m_Serial = ogg_page_serialno( &page );
			StreamState stream( m_Serial );
			const size_t headerCount = GetHeaderCount();
			size_t headerSize = 0;
			bool valid = true;
			do {
				if ( ogg_page_serialno( &page ) == m_Serial ) {
					headerSize += static_cast<size_t>( page.body_len );
					valid = ( headerSize <= MaximumCommentSize ) && ( 0 == ogg_stream_pagein( &stream.m_State, &page ) );
					if ( valid ) {
						m_HeaderPages.push_back( { offset, page.header_len + page.body_len, ogg_page_pageno( &page ), 0 != ogg_page_eos( &page ) } );
						ogg_packet packet = {};
						int result = 0;
						while ( valid && ( m_HeaderPackets.size() < headerCount ) && ( 0 != ( result = ogg_stream_packetout( &stream.m_State, &packet ) ) ) ) {
							valid = ( result > 0 ) && HasHeaderSignature( m_HeaderPackets.size(), packet.packet, static_cast<size_t>( packet.bytes ) );
							if ( valid ) {
								m_HeaderPackets.emplace_back( packet.packet, packet.packet + packet.bytes );
							}
						}
					}
				}
			} while ( valid && ( m_HeaderPackets.size() < headerCount ) && reader.Read( page, offset ) );

			if ( valid && ( headerCount == m_HeaderPackets.size() ) ) {
				readComments = ParseCommentPacket( m_HeaderPackets[ 1 ] );
				if ( readComments ) {
					// To be able to write the comments, the header packets must end on a page boundary, so that the audio data begins on a fresh page.
					// Note that 'page' is the page that completed the header packets.
					ogg_packet packet = {};
					const int segmentCount = page.header[ 26 ];
					m_Writable = ( 0 == ogg_stream_packetpeek( &stream.m_State, &packet ) ) && ( segmentCount > 0 ) && ( 255 != page.header[ 26 + segmentCount ] );

					// There should also be nothing other than valid pages up to the end of the header pages.
					m_Writable = m_Writable && reader.IsValid();

					// The header page sequence numbers should also start from zero.
					for ( size_t index = 0; m_Writable && ( index < m_HeaderPages.size() ); index++ ) {
						m_Writable = ( static_cast<long>( index ) == m_HeaderPages[ index ].Sequence );
					}
				}
			}
		}
	}

	if ( !readComments ) {
		throw std::runtime_error( "Could not read Ogg comments." );
	}
}

OggComment::~OggComment()
{
}

OggComment::PageReader::PageReader( std::istream& stream ) :
	m_Stream( stream ),
	m_Sync(),
	m_Offset( 0 ),
	m_BytesRead( 0 ),
	m_Valid( true )
{
	ogg_sync_init( &m_Sync );
}

OggComment::PageReader::~PageReader()
{
	ogg_sync_clear( &m_Sync );
}

bool OggComment::PageReader::Read( ogg_page& page, long long& offset )
{
	constexpr long kBufferSize = 65536;
	bool readPage = false;
	bool moreData = true;
	while ( !readPage && moreData ) {
		const long result = ogg_sync_pageseek( &m_Sync, &page );
		if ( result > 0 ) {
			offset = m_Offset;
			m_Offset += result;
			readPage = true;
		} else if ( result < 0 ) {
			// Skip over data which is not part of a valid page (e.g. junk, or a page with an invalid checksum).
			m_Offset -= result;
			m_Valid = false;
		} else {
			char* buffer = ogg_sync_buffer( &m_Sync, kBufferSize );
			moreData = ( nullptr != buffer );
			if ( moreData ) {
				m_Stream.read( buffer, kBufferSize );
				const std::streamsize bytesRead = m_Stream.gcount();
				moreData = ( bytesRead > 0 );
				if ( moreData ) {
					ogg_sync_wrote( &m_Sync, static_cast<long>( bytesRead ) );
					m_BytesRead += bytesRead;
				} else if ( !m_Stream.eof() || ( m_Offset != m_BytesRead ) ) {
					// Either a read error, or the stream ended with a partial page (or other trailing data).
					m_Valid = false;
				}
			} else {
				m_Valid = false;
			}
		}
	}
	return readPage;
}

bool OggComment::PageReader::IsValid() const
{
	return m_Valid;
}

OggComment::StreamState::StreamState( const int serial ) :
	m_State()
{
	ogg_stream_init( &m_State, serial );
}

OggComment::StreamState::~StreamState()
{
	ogg_stream_clear( &m_State );
}

size_t OggComment::GetHeaderCount() const
{
	return ( Type::Vorbis == m_Type ) ? 3 : 2;
}

bool OggComment::HasHeaderSignature( const size_t index, const uint8_t* data, const size_t size ) const
{
	bool hasSignature = false;
	if ( nullptr != data ) {
		if ( Type::Vorbis == m_Type ) {
			// Identification, comment & setup headers, each with a packet type followed by 'vorbis'.
			constexpr uint8_t kPacketTypes[] = { 0x01, 0x03, 0x05 };
			constexpr size_t kMinimumSizes[] = { 30, 15, 7 };
			if ( index < std::size( kPacketTypes ) ) {
				hasSignature = ( size >= kMinimumSizes[ index ] ) && ( kPacketTypes[ index ] == data[ 0 ] ) && ( 0 == memcmp( data + 1, "vorbis", 6 ) );
			}
		} else {
			// Identification & comment headers.
			constexpr const char* kSignatures[] = { "OpusHead", "OpusTags" };
			constexpr size_t kMinimumSizes[] = { 19, 16 };
			if ( index < std::size( kSignatures ) ) {
				hasSignature = ( size >= kMinimumSizes[ index ] ) && ( 0 == memcmp( data, kSignatures[ index ], 8 ) );
			}
		}
	}
	return hasSignature;
}

std::string OggComment::GetCommentSignature() const
{
	return ( Type::Vorbis == m_Type ) ? std::string( "\x03" "vorbis", 7 ) : std::string( "OpusTags", 8 );
}

bool OggComment::ParseCommentPacket( const std::vector<uint8_t>& packet )
{
	// Sizes are compared against the remaining data, so that malformed sizes cannot cause an overrun.
	const std::string signature = GetCommentSignature();
	size_t offset = signature.size();
	bool valid = ( packet.size() >= ( offset + 4 ) ) && ( 0 == memcmp( packet.data(), signature.data(), signature.size() ) );

	// Vendor.
	if ( valid ) {
		const uint32_t vendorLength = ToUint32LE( packet, offset );
		offset += 4;
		valid = ( vendorLength <= ( packet.size() - offset ) );
		if ( valid ) {
			m_Vendor.assign( reinterpret_cast<const char*>( packet.data() + offset ), vendorLength );
			offset += vendorLength;
		}
	}

	// User comments.
	if ( valid ) {
		valid = ( ( packet.size() - offset ) >= 4 );
		const uint32_t commentCount = valid ? ToUint32LE( packet, offset ) : 0;
		if ( valid ) {
			offset += 4;
			valid = ( ( 4 * static_cast<uint64_t>( commentCount ) ) <= ( packet.size() - offset ) );
		}
		if ( valid ) {
			m_Comments.reserve( commentCount );
			uint32_t commentIndex = 0;
			while ( valid && ( commentIndex++ < commentCount ) ) {
				valid = ( ( packet.size() - offset ) >= 4 );
				if ( valid ) {
					const uint32_t commentLength = ToUint32LE( packet, offset );
					offset += 4;
					valid = ( commentLength <= ( packet.size() - offset ) );
					if ( valid ) {
						const std::string tag( reinterpret_cast<const char*>( packet.data() + offset ), commentLength );
						const size_t delimiter = tag.find( '=' );
						if ( std::string::npos != delimiter ) {
							m_Comments.push_back( std::make_pair( tag.substr( 0, delimiter ), tag.substr( 1 + delimiter ) ) );
						} else {
							// Preserve malformed comments, so that they are not lost when writing.
							m_RawComments.push_back( tag );
						}
						offset += commentLength;
					}
				}
			}
		}
	}

	// Trailing data.
	// For Opus, data with the least significant bit of the first byte set should be preserved, otherwise it is padding.
	// For Vorbis, the trailing data is the framing bit, which is regenerated when writing.
	if ( valid && ( Type::Opus == m_Type ) && ( offset < packet.size() ) && ( packet[ offset ] & 0x01 ) ) {
		m_BinaryData.assign( packet.begin() + offset, packet.end() );
	}

	return valid;
}

std::vector<uint8_t> OggComment::GenerateCommentPacket() const
{
	std::vector<uint8_t> packet;

	const std::string signature = GetCommentSignature();
	packet.insert( packet.end(), signature.begin(), signature.end() );

	AppendLE( static_cast<uint32_t>( m_Vendor.size() ), packet );
	packet.insert( packet.end(), m_Vendor.begin(), m_Vendor.end() );

	AppendLE( static_cast<uint32_t>( m_Comments.size() + m_RawComments.size() ), packet );
	for ( const auto& [ field, value ] : m_Comments ) {
		AppendLE( static_cast<uint32_t>( field.size() + 1 + value.size() ), packet );
		packet.insert( packet.end(), field.begin(), field.end() );
		packet.push_back( '=' );
		packet.insert( packet.end(), value.begin(), value.end() );
	}
	for ( const auto& comment : m_RawComments ) {
		AppendLE( static_cast<uint32_t>( comment.size() ), packet );
		packet.insert( packet.end(), comment.begin(), comment.end() );
	}

	if ( Type::Vorbis == m_Type ) {
		// Framing bit.
		packet.push_back( 0x01 );
	} else {
		packet.insert( packet.end(), m_BinaryData.begin(), m_BinaryData.end() );
	}

	return packet;
}

std::vector<std::vector<uint8_t>> OggComment::GenerateHeaderPages( const std::vector<uint8_t>& commentPacket ) const
{
	std::vector<std::vector<uint8_t>> pages;
	StreamState stream( m_Serial );
	ogg_page page = {};
	bool ok = true;
	const size_t headerCount = m_HeaderPackets.size();
	for ( size_t index = 0; ok && ( index < headerCount ); index++ ) {
		const std::vector<uint8_t>& data = ( 1 == index ) ? commentPacket : m_HeaderPackets[ index ];
		const bool isLastPacket = ( ( index + 1 ) == headerCount );
		ogg_packet packet = {};
		packet.packet = const_cast<unsigned char*>( data.data() );
		packet.bytes = static_cast<long>( data.size() );
		packet.b_o_s = ( 0 == index ) ? 1 : 0;
		packet.e_o_s = ( isLastPacket && m_HeaderPages.back().EOS ) ? 1 : 0;
		packet.granulepos = 0;
		packet.packetno = static_cast<ogg_int64_t>( index );
		ok = ( 0 == ogg_stream_packetin( &stream.m_State, &packet ) );

		// The identification header is on its own page, and the remaining header packets end on a page boundary.
		if ( ok && ( ( 0 == index ) || isLastPacket ) ) {
			while ( 0 != ogg_stream_flush( &stream.m_State, &page ) ) {
				std::vector<uint8_t>& generatedPage = pages.emplace_back( page.header, page.header + page.header_len );
				generatedPage.insert( generatedPage.end(), page.body, page.body + page.body_len );
			}
		}
	}
	if ( !ok ) {
		pages.clear();
	}
	return pages;
}

bool OggComment::CanWriteInPlace( const std::vector<std::vector<uint8_t>>& pages ) const
{
	bool canWrite = ( pages.size() == m_HeaderPages.size() );
	for ( size_t index = 0; canWrite && ( index < pages.size() ); index++ ) {
		canWrite = ( static_cast<long>( pages[ index ].size() ) == m_HeaderPages[ index ].Size );
	}
	return canWrite;
}

bool OggComment::WriteInPlace( const std::vector<std::vector<uint8_t>>& pages )
{
	m_Stream.clear();
	bool ok = true;
	for ( size_t index = 0; ok && ( index < pages.size() ); index++ ) {
		m_Stream.seekp( m_HeaderPages[ index ].Offset );
		m_Stream.write( reinterpret_cast<const char*>( pages[ index ].data() ), pages[ index ].size() );
		ok = m_Stream.good();
	}
	if ( ok ) {
		m_Stream.flush();
		ok = m_Stream.good();
	}
	return ok;
}

bool OggComment::Rewrite( const std::vector<std::vector<uint8_t>>& pages )
{
	m_Stream.clear();
	m_Stream.seekg( 0 );
	bool ok = m_Stream.good();
	if ( ok ) {
		// Copy the original stream to a temporary file, replacing the header pages.
		const std::wstring tempFilename = m_Filename + L".TmpOggComment";
		std::ofstream outStream( tempFilename, std::ios::out | std::ios::binary | std::ios::trunc, _SH_DENYRW );
		ok = outStream.good();

		// Writes out the generated header pages, from 'first' up to (but not including) 'last'.
		auto writePages = [ &pages, &outStream ] ( const size_t first, const size_t last )
		{
			for ( size_t index = first; index < last; index++ ) {
				outStream.write( reinterpret_cast<const char*>( pages[ index ].data() ), pages[ index ].size() );
			}
			return outStream.good();
		};

		// The identification header page is written in its original position (as the initial pages of all logical bitstreams must precede any other pages),
		// with the remaining header pages written in place of the next original header page.
		const long lastHeaderSequence = m_HeaderPages.back().Sequence;
		const long remainingPagesSequence = std::min<long>( 1, lastHeaderSequence );

		// If the number of header pages has changed, we will need to adjust the sequence numbers of all subsequent pages.
		const long sequenceAdjustment = static_cast<long>( pages.size() ) - static_cast<long>( m_HeaderPages.size() );

		PageReader reader( m_Stream );
		ogg_page page = {};
		long long offset = 0;
		size_t pagesWritten = 0;
		bool streamEnded = false;
		while ( ok && reader.Read( page, offset ) ) {
			bool writeOriginalPage = true;
			if ( !streamEnded && ( ogg_page_serialno( &page ) == m_Serial ) ) {
				// Any subsequent pages with the same serial number belong to a chained bitstream, and are passed through unchanged.
				streamEnded = ( 0 != ogg_page_eos( &page ) );
				const long sequence = ogg_page_pageno( &page );
				if ( sequence <= lastHeaderSequence ) {
					// The original header pages should be exactly where they were found when the comments were read.
					writeOriginalPage = false;
					ok = ( sequence >= 0 ) && ( offset == m_HeaderPages[ sequence ].Offset );
					if ( ok && ( 0 == sequence ) ) {
						ok = writePages( 0, 1 );
						pagesWritten = 1;
					}
					if ( ok && ( remainingPagesSequence == sequence ) ) {
						ok = writePages( pagesWritten, pages.size() );
						pagesWritten = pages.size();
					}
				} else if ( 0 != sequenceAdjustment ) {
					const uint32_t alteredSequence = static_cast<uint32_t>( sequence + sequenceAdjustment );
					page.header[ 18 ] = alteredSequence & 0xff;
					page.header[ 19 ] = ( alteredSequence >> 8 ) & 0xff;
					page.header[ 20 ] = ( alteredSequence >> 16 ) & 0xff;
					page.header[ 21 ] = ( alteredSequence >> 24 ) & 0xff;
					ogg_page_checksum_set( &page );
				}
			}
			if ( ok && writeOriginalPage ) {
				outStream.write( reinterpret_cast<const char*>( page.header ), page.header_len );
				outStream.write( reinterpret_cast<const char*>( page.body ), page.body_len );
				ok = outStream.good();
			}
		}
		// Fail if the original stream contained anything other than valid pages, so that nothing other than the comments is ever changed.
		ok = ok && reader.IsValid() && ( pages.size() == pagesWritten );

		// Explicitly close the output stream, so that any failure to write buffered data is detected.
		outStream.close();
		ok = ok && !outStream.fail();

		if ( ok ) {
			// Replace the original file with the modified copy.
			// The original file is never removed before the replacement succeeds, so that a failure cannot lose both copies.
			m_Stream.close();
			ok = ( FALSE != ReplaceFileW( m_Filename.c_str(), tempFilename.c_str(), nullptr /*backupFilename*/, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr ) );
			if ( !ok ) {
				ok = ( FALSE != MoveFileExW( tempFilename.c_str(), m_Filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) );
			}
		}

		if ( !ok && ( INVALID_FILE_ATTRIBUTES != GetFileAttributesW( m_Filename.c_str() ) ) ) {
			// Only remove the temporary file if the original file is still present.
			DeleteFileW( tempFilename.c_str() );
		}
	}
	return ok;
}

bool OggComment::WriteComments()
{
	bool wroteComments = false;
	if ( !m_Readonly && m_Writable ) {
		std::vector<uint8_t> commentPacket = GenerateCommentPacket();
		if ( ( Type::Opus == m_Type ) && m_BinaryData.empty() ) {
			// Pad out to the original size, so that the comments can be written in-place, unless there is a lot of space to reclaim.
			// Otherwise, add some padding to allow for future modifications, as long as this does not require an extra page.
			const size_t originalSize = m_HeaderPackets[ 1 ].size();
			if ( ( commentPacket.size() <= originalSize ) && ( ( originalSize - commentPacket.size() ) < s_MaxPageContentSize ) ) {
				commentPacket.resize( originalSize, 0 );
			} else {
				const size_t lastPageContentSize = commentPacket.size() % s_MaxPageContentSize;
				const size_t padding = 512;
				commentPacket.resize( commentPacket.size() + std::min<size_t>( s_MaxPageContentSize - lastPageContentSize - 1, padding ), 0 );
			}
		}

		if ( commentPacket.size() <= MaximumCommentSize ) {
			const auto pages = GenerateHeaderPages( commentPacket );
			if ( !pages.empty() ) {
				wroteComments = CanWriteInPlace( pages ) ? WriteInPlace( pages ) : Rewrite( pages );
			}
		}
	}
	return wroteComments;
}

const std::string& OggComment::GetVendor() const
{
	return m_Vendor;
}

const std::vector<std::pair<std::string, std::string>>& OggComment::GetUserComments() const
{
	return m_Comments;
}

void OggComment::AddUserComment( const std::string& field, const std::string& value )
{
	m_Comments.push_back( std::make_pair( field, value ) );
}

void OggComment::RemoveUserComment( const std::string& field )
{
	auto comment = m_Comments.begin();
	while ( m_Comments.end() != comment ) {
		if ( 0 == _stricmp( field.c_str(), comment->first.c_str() ) ) {
			comment = m_Comments.erase( comment );
		} else {
			++comment;
		}
	}
}

std::string OggComment::GetArtwork() const
{
	std::string encodedImage;
	std::vector<uint8_t> picture;
	if ( GetPicture( s_FrontCover, picture ) ) {
		encodedImage = Base64Encode( picture.data(), static_cast<int>( picture.size() ) );
	}
	return encodedImage;
}

void OggComment::SetArtwork( const std::string& encodedImage )
{
	RemovePicture( s_FrontCover );
	if ( !encodedImage.empty() ) {
		const std::vector<uint8_t> imageBytes = Base64Decode( encodedImage );
		if ( !imageBytes.empty() ) {
			std::string mimeType;
			int width = 0;
			int height = 0;
			int depth = 0;
			int colours = 0;
			GetImageInformation( encodedImage, mimeType, width, height, depth, colours );
			AddPicture( s_FrontCover, mimeType, std::string() /*description*/, width, height, depth, colours, imageBytes );
		}
	}
}

bool OggComment::GetPicture( const uint32_t pictureType, std::vector<uint8_t>& picture ) const
{
	bool success = false;
	picture.clear();
	auto comment = m_Comments.begin();
	while ( !success && ( m_Comments.end() != comment ) ) {
		if ( 0 == _stricmp( "METADATA_BLOCK_PICTURE", comment->first.c_str() ) ) {
			// Sizes are compared against the remaining data, so that malformed sizes cannot cause an overrun.
			const auto metadata = Base64Decode( comment->second.c_str() );
			if ( ( metadata.size() >= 32 ) && ( ToUint32BE( metadata, 0 ) == pictureType ) ) {
				size_t offset = 4;
				const uint32_t mimeSize = ToUint32BE( metadata, offset );
				offset += 4;
				bool valid = ( mimeSize <= ( metadata.size() - offset ) );
				if ( valid ) {
					offset += mimeSize;
					valid = ( ( metadata.size() - offset ) >= 4 );
				}
				if ( valid ) {
					const uint32_t descriptionSize = ToUint32BE( metadata, offset );
					offset += 4;
					valid = ( descriptionSize <= ( metadata.size() - offset ) );
					if ( valid ) {
						offset += descriptionSize;
					}
				}
				// Skip over the width, height, depth & colours fields.
				valid = valid && ( ( metadata.size() - offset ) >= 20 );
				if ( valid ) {
					offset += 16;
					const uint32_t pictureSize = ToUint32BE( metadata, offset );
					offset += 4;
					if ( ( pictureSize > 0 ) && ( pictureSize <= ( metadata.size() - offset ) ) ) {
						picture.assign( metadata.begin() + offset, metadata.begin() + offset + pictureSize );
						success = true;
					}
				}
			}
		}
		++comment;
	}
	return success;
}

void OggComment::AddPicture( const uint32_t pictureType, const std::string& mimeType, const std::string& description,
	const uint32_t width, const uint32_t height, const uint32_t depth, const uint32_t colours, const std::vector<uint8_t>& picture )
{
	if ( ( picture.size() > 0 ) && ( picture.size() < MaximumCommentSize ) && ( width > 0 ) && ( height > 0 ) && ( depth > 0 ) && ( pictureType <= 20 ) ) {
		// Only one picture of the file icon types is allowed.
		if ( ( 1 == pictureType ) || ( 2 == pictureType ) ) {
			RemovePicture( pictureType );
		}

		std::vector<uint8_t> metadata;
		metadata.reserve( 32 + mimeType.size() + description.size() + picture.size() );
		AppendBE( pictureType, metadata );
		AppendBE( static_cast<uint32_t>( mimeType.size() ), metadata );
		metadata.insert( metadata.end(), mimeType.begin(), mimeType.end() );
		AppendBE( static_cast<uint32_t>( description.size() ), metadata );
		metadata.insert( metadata.end(), description.begin(), description.end() );
		AppendBE( width, metadata );
		AppendBE( height, metadata );
		AppendBE( depth, metadata );
		AppendBE( colours, metadata );
		AppendBE( static_cast<uint32_t>( picture.size() ), metadata );
		metadata.insert( metadata.end(), picture.begin(), picture.end() );

		const std::string encodedPicture = Base64Encode( metadata.data(), static_cast<int>( metadata.size() ) );
		if ( !encodedPicture.empty() ) {
			AddUserComment( "METADATA_BLOCK_PICTURE", encodedPicture );
		}
	}
}

void OggComment::RemovePicture( const uint32_t pictureType )
{
	auto comment = m_Comments.begin();
	while ( m_Comments.end() != comment ) {
		bool eraseComment = false;
		if ( 0 == _stricmp( "METADATA_BLOCK_PICTURE", comment->first.c_str() ) ) {
			const auto picture = Base64Decode( comment->second.c_str() );
			eraseComment = ( picture.size() >= 4 ) && ( ToUint32BE( picture, 0 ) == pictureType );
		}
		if ( eraseComment ) {
			comment = m_Comments.erase( comment );
		} else {
			++comment;
		}
	}
}

uint32_t OggComment::ToUint32LE( const std::vector<uint8_t>& data, const size_t offset )
{
	const uint32_t value = ( ( offset + 4 ) <= data.size() ) ?
		( data[ offset + 3 ] << 24 ) | ( data[ offset + 2 ] << 16 ) | ( data[ offset + 1 ] << 8 ) | ( data[ offset ] ) : 0;
	return value;
}

uint32_t OggComment::ToUint32BE( const std::vector<uint8_t>& data, const size_t offset )
{
	const uint32_t value = ( ( offset + 4 ) <= data.size() ) ?
		( data[ offset ] << 24 ) | ( data[ offset + 1 ] << 16 ) | ( data[ offset + 2 ] << 8 ) | ( data[ offset + 3 ] ) : 0;
	return value;
}

void OggComment::AppendLE( const uint32_t value, std::vector<uint8_t>& data )
{
	data.push_back( value & 0xff );
	data.push_back( ( value >> 8 ) & 0xff );
	data.push_back( ( value >> 16 ) & 0xff );
	data.push_back( ( value >> 24 ) & 0xff );
}

void OggComment::AppendBE( const uint32_t value, std::vector<uint8_t>& data )
{
	data.push_back( ( value >> 24 ) & 0xff );
	data.push_back( ( value >> 16 ) & 0xff );
	data.push_back( ( value >> 8 ) & 0xff );
	data.push_back( value & 0xff );
}
