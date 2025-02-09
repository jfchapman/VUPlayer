#include "OpusComment.h"

#include "Utility.h"

#include <fstream>

// https://tools.ietf.org/html/rfc7845.html

// Maximum comment header size.
static const uint32_t s_MaxCommentSize = 125829120;

OpusComment::OpusComment( const std::wstring& filename, const bool readonly ) :
	m_Filename( filename ),
	m_Stream( filename, ( readonly ? ( std::ios::in | std::ios::binary ) : ( std::ios::in | std::ios::out | std::ios::binary ) ), _SH_DENYWR ),
	m_OriginalPages(),
	m_Vendor(),
	m_Comments(),
	m_BinaryData()
{
	bool readComments = false;
	try {
		const OggPage header( m_Stream );
		if ( IsOpusHeader( header ) ) {
			const uint32_t serial = header.GetSerialNumber();
			const uint32_t sequence = header.GetSequenceNumber();
			uint32_t nextSequence = sequence;
			std::vector<uint8_t> vorbisComment;
			bool valid = true;
			while ( valid && !readComments ) {
				const long long streamPos = m_Stream.tellg();
				const OggPage page( m_Stream );
				if ( page.GetSerialNumber() == serial ) {
					if ( page.GetSequenceNumber() != ++nextSequence ) {
						// Treat a gap in page sequence numbers as an error.
						valid = false;
					} else {
						if ( vorbisComment.empty() && !IsCommentHeader( page ) ) {
							// A comment header should directly follow the Opus header.
							valid = false;
						} else if ( !vorbisComment.empty() && !page.IsContinued() ) {
							// Subsequent comment header pages should be corrently flagged.
							valid = false;
						} else {
							const auto& content = page.GetContent();
							if ( ( content.size() + vorbisComment.size() ) > s_MaxCommentSize ) {
								valid = false;
							} else {
								if ( !content.empty() ) {
									vorbisComment.insert( vorbisComment.end(), content.begin(), content.end() );
								}
								readComments = page.IsComplete();
								m_OriginalPages.insert( std::map<long long, OggPage>::value_type( streamPos, page ) );
							}
						}
					}
				}
			}

			if ( readComments ) {
				// Skip the comment signature.
				auto offset = vorbisComment.begin() + 8;

				// Vendor.
				const uint32_t vendorLength = ToUint32LE( vorbisComment, static_cast<uint32_t>( std::distance( vorbisComment.begin(), offset ) ) );
				offset += 4;
				if ( static_cast<long long>( vendorLength ) > std::distance( offset, vorbisComment.end() ) ) {
					valid = false;
				} else if ( vendorLength > 0 ) {
					m_Vendor = std::string( reinterpret_cast<const char*>( &offset[ 0 ] ), vendorLength );
					offset += vendorLength;
				}

				// User comments.
				if ( valid ) {
					const uint32_t commentCount = ToUint32LE( vorbisComment, static_cast<uint32_t>( std::distance( vorbisComment.begin(), offset ) ) );
					offset += 4;
					valid = ( ( 4 * static_cast<long long>( commentCount ) ) <= static_cast<long long>( std::distance( offset, vorbisComment.end() ) ) );
					if ( valid ) {
						m_Comments.reserve( commentCount );
						uint32_t commentIndex = 0;
						while ( valid && ( commentIndex++ < commentCount ) ) {
							valid = ( std::distance( offset, vorbisComment.end() ) >= 4 );
							if ( valid ) {
								const uint32_t commentLength = ToUint32LE( vorbisComment, static_cast<uint32_t>( std::distance( vorbisComment.begin(), offset ) ) );
								offset += 4;
								valid = ( static_cast<long long>( commentLength ) <= std::distance( offset, vorbisComment.end() ) );
								if ( valid ) {
									const std::string tag( reinterpret_cast<const char*>( &offset[ 0 ] ), commentLength );
									const size_t delimiter = tag.find( '=' );
									if ( std::string::npos != delimiter ) {
										m_Comments.push_back( std::make_pair( tag.substr( 0, delimiter ), tag.substr( 1 + delimiter ) ) );
									}
									offset += commentLength;
								}
							}
						}
					}
				}

				// Trailing data.
				if ( valid ) {
					const size_t paddingSize = std::distance( offset, vorbisComment.end() );
					if ( ( paddingSize > 0 ) && ( offset[ 0 ] & 0x01 ) ) {
						m_BinaryData.resize( paddingSize );
						std::copy( offset, vorbisComment.end(), m_BinaryData.begin() );
					}
				}
			}
		}
	} catch ( const std::runtime_error& ) {
	}

	if ( !readComments ) {
		throw std::runtime_error( "Could not read Opus comments." );
	}
}

OpusComment::~OpusComment()
{
}

bool OpusComment::IsOpusHeader( const OggPage& page )
{
	bool isHeader = false;
	if ( page.IsBOS() && page.IsComplete() && ( 0 == page.GetSequenceNumber() ) ) {
		const std::vector<uint8_t>& content = page.GetContent();
		isHeader = ( content.size() >= 19 ) && ( 0 == memcmp( &content[ 0 ], "OpusHead", 8 ) );
	}
	return isHeader;
}

bool OpusComment::IsCommentHeader( const OggPage& page )
{
	const std::vector<uint8_t>& content = page.GetContent();
	const bool isComment = !page.IsContinued() && ( content.size() >= 16 ) && ( 0 == memcmp( &content[ 0 ], "OpusTags", 8 ) );
	return isComment;
}

uint32_t OpusComment::ToUint32LE( const std::vector<uint8_t>& data, const uint32_t offset )
{
	const uint32_t value = ( ( offset + 4 ) <= data.size() ) ?
		( data[ offset + 3 ] << 24 ) | ( data[ offset + 2 ] << 16 ) | ( data[ offset + 1 ] << 8 ) | ( data[ offset ] ) : 0;
	return value;
}

uint32_t OpusComment::ToUint32BE( const std::vector<uint8_t>& data, const uint32_t offset )
{
	const uint32_t value = ( ( offset + 4 ) <= data.size() ) ?
		( data[ offset ] << 24 ) | ( data[ offset + 1 ] << 16 ) | ( data[ offset + 2 ] << 8 ) | ( data[ offset + 3 ] ) : 0;
	return value;
}

void OpusComment::ToBytesLE( const uint32_t value, std::vector<uint8_t>& data, const uint32_t offset )
{
	if ( ( offset + 4 ) <= data.size() ) {
		data[ offset ] = value & 0xff;
		data[ offset + 1 ] = ( value >> 8 ) & 0xff;
		data[ offset + 2 ] = ( value >> 16 ) & 0xff;
		data[ offset + 3 ] = ( value >> 24 ) & 0xff;
	}
}

void OpusComment::ToBytesBE( const uint32_t value, std::vector<uint8_t>& data, const uint32_t offset )
{
	if ( ( offset + 4 ) <= data.size() ) {
		data[ offset + 3 ] = value & 0xff;
		data[ offset + 2 ] = ( value >> 8 ) & 0xff;
		data[ offset + 1 ] = ( value >> 16 ) & 0xff;
		data[ offset ] = ( value >> 24 ) & 0xff;
	}
}

const std::string& OpusComment::GetVendor() const
{
	return m_Vendor;
}

const std::vector<std::pair<std::string, std::string>>& OpusComment::GetUserComments() const
{
	return m_Comments;
}

void OpusComment::AddUserComment( const std::string& field, const std::string& value )
{
	m_Comments.push_back( std::make_pair( field, value ) );
}

void OpusComment::RemoveUserComment( const std::string& field )
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

bool OpusComment::GetPicture( const uint32_t pictureType, std::string& mimeType, std::string& description,
	uint32_t& width, uint32_t& height, uint32_t& depth, uint32_t& colours, std::vector<uint8_t>& picture ) const
{
	bool success = false;
	mimeType.clear();
	description.clear();
	width = 0;
	height = 0;
	depth = 0;
	colours = 0;
	picture.clear();
	auto comment = m_Comments.begin();
	while ( m_Comments.end() != comment ) {
		if ( 0 == _stricmp( "METADATA_BLOCK_PICTURE", comment->first.c_str() ) ) {
			const auto metadata = Base64Decode( comment->second.c_str() );
			if ( metadata.size() >= 32 ) {
				uint32_t offset = 0;
				const uint32_t value = ToUint32BE( metadata, offset );
				if ( value == pictureType ) {
					offset += 4;
					const uint32_t mimeSize = ToUint32BE( metadata, offset );
					offset += 4;
					if ( mimeSize > 0 ) {
						mimeType = std::string( reinterpret_cast<const char*>( &metadata[ offset ] ), mimeSize );
						offset += mimeSize;
					}
					const uint32_t descriptionSize = ToUint32BE( metadata, offset );
					offset += 4;
					if ( descriptionSize > 0 ) {
						description = std::string( reinterpret_cast<const char*>( &metadata[ offset ] ), descriptionSize );
						offset += descriptionSize;
					}
					width = ToUint32BE( metadata, offset );
					offset += 4;
					height = ToUint32BE( metadata, offset );
					offset += 4;
					depth = ToUint32BE( metadata, offset );
					offset += 4;
					colours = ToUint32BE( metadata, offset );
					offset += 4;
					const uint32_t pictureSize = ToUint32BE( metadata, offset );
					offset += 4;
					if ( ( pictureSize > 0 ) && ( ( offset + pictureSize ) <= metadata.size() ) ) {
						picture.resize( pictureSize );
						std::copy( metadata.begin() + offset, metadata.begin() + offset + pictureSize, picture.begin() );
						success = true;
						break;
					}
				}
			}
		}
		++comment;
	}
	return success;
}

void OpusComment::AddPicture( const uint32_t pictureType, const std::string& mimeType, const std::string& description,
	const uint32_t width, const uint32_t height, const uint32_t depth, const uint32_t colours, const std::vector<uint8_t>& picture )
{
	if ( ( picture.size() > 0 ) && ( picture.size() < s_MaxCommentSize ) && ( width > 0 ) && ( height > 0 ) && ( depth > 0 ) && ( pictureType >= 0 ) && ( pictureType <= 20 ) ) {
		if ( ( 1 == pictureType ) || ( 2 == pictureType ) ) {
			RemovePicture( pictureType );
		}

		const uint32_t mimeSize = static_cast<uint32_t>( mimeType.size() );
		const uint32_t descriptionSize = static_cast<uint32_t>( description.size() );
		const uint32_t pictureSize = static_cast<uint32_t>( picture.size() );
		const uint32_t dataSize = 4 /*pictureType*/ + 4 /*mimeType*/ + mimeSize + 4 /*description*/ + descriptionSize +
			4 /*width*/ + 4 /*height*/ + 4 /*depth*/ + 4 /*colours*/ + 4 /*pictureSize*/ + pictureSize;
		std::vector<uint8_t> metadata( dataSize );
		uint32_t offset = 0;

		ToBytesBE( pictureType, metadata, offset );
		offset += 4;
		ToBytesBE( mimeSize, metadata, offset );
		offset += 4;
		if ( mimeSize > 0 ) {
			std::copy( mimeType.begin(), mimeType.end(), &metadata[ offset ] );
			offset += mimeSize;
		}
		ToBytesBE( descriptionSize, metadata, offset );
		offset += 4;
		if ( descriptionSize > 0 ) {
			std::copy( description.begin(), description.end(), &metadata[ offset ] );
			offset += descriptionSize;
		}
		ToBytesBE( width, metadata, offset );
		offset += 4;
		ToBytesBE( height, metadata, offset );
		offset += 4;
		ToBytesBE( depth, metadata, offset );
		offset += 4;
		ToBytesBE( colours, metadata, offset );
		offset += 4;
		ToBytesBE( pictureSize, metadata, offset );
		offset += 4;
		if ( pictureSize > 0 ) {
			std::copy( picture.begin(), picture.end(), &metadata[ offset ] );
			offset += pictureSize;
		}

		const std::string encodedPicture = Base64Encode( &metadata[ 0 ], static_cast<int>( metadata.size() ) );
		if ( !encodedPicture.empty() ) {
			AddUserComment( "METADATA_BLOCK_PICTURE", encodedPicture );
		}
	}
}

void OpusComment::RemovePicture( const uint32_t type )
{
	auto comment = m_Comments.begin();
	while ( m_Comments.end() != comment ) {
		bool eraseComment = false;
		if ( 0 == _stricmp( "METADATA_BLOCK_PICTURE", comment->first.c_str() ) ) {
			const auto picture = Base64Decode( comment->second.c_str() );
			if ( picture.size() >= 4 ) {
				const uint32_t value = ToUint32BE( picture, 0 /*offset*/ );
				eraseComment = ( value == type );
			}
		}
		if ( eraseComment ) {
			comment = m_Comments.erase( comment );
		} else {
			++comment;
		}
	}
}

bool OpusComment::WriteComments()
{
	bool wroteComments = false;

	// Calculate the required size of the comment header content (excluding padding & Ogg page headers).
	size_t modifiedContentSize = 8;                    // Comment signature
	modifiedContentSize += 4;                          // Vendor length
	modifiedContentSize += m_Vendor.size();            // Vendor string
	modifiedContentSize += 4;                          // User comments count
	for ( const auto& comment : m_Comments ) {
		modifiedContentSize += 4;                      // User comment length
		modifiedContentSize += comment.first.size();   // User comment field
		modifiedContentSize += 1;                      // User comment delimiter
		modifiedContentSize += comment.second.size();  // User comment value
	}
	modifiedContentSize += m_BinaryData.size();        // Trailing binary data

	// Determine if we can modify the comment in-place.
	size_t originalContentSize = 0;
	for ( const auto& page : m_OriginalPages ) {
		originalContentSize += page.second.GetContent().size();
	}

	// If there's binary data to preserve, we can't use padding.
	bool modifyInPlace = m_BinaryData.empty() ?
		( modifiedContentSize <= originalContentSize ) : ( modifiedContentSize == originalContentSize );

	if ( modifyInPlace ) {
		// We also require all the original comment header pages (except for the last) to be the maximum possible size.
		auto page = m_OriginalPages.rbegin();
		while ( m_OriginalPages.rend() != ++page ) {
			modifyInPlace &= page->second.IsMaximumSize();
		}
	}

	if ( modifyInPlace ) {
		// Reclaim some space if there is too much padding.
		modifyInPlace = ( ( originalContentSize - modifiedContentSize ) < OggPage::MaximumContentSize );
	}

	if ( modifyInPlace ) {
		modifiedContentSize = originalContentSize;
	} else if ( m_BinaryData.empty() ) {
		// If there is enough room on the final comment header page, add some padding.
		const size_t lastPageContentSize = modifiedContentSize % OggPage::MaximumContentSize;
		if ( lastPageContentSize < OggPage::MaximumContentSize ) {
			const size_t padding = 512;
			modifiedContentSize += std::min<size_t>( OggPage::MaximumContentSize - lastPageContentSize - 1, padding );
		}
	}

	// Generate the new comment header content.
	std::vector<uint8_t> modifiedContent( modifiedContentSize, 0 );
	uint32_t offset = 0;

	memcpy( &modifiedContent[ offset ], "OpusTags", 8 );
	offset += 8;

	const uint32_t vendorLength = static_cast<uint32_t>( m_Vendor.size() );
	ToBytesLE( vendorLength, modifiedContent, offset );
	offset += 4;
	std::copy( m_Vendor.begin(), m_Vendor.end(), &modifiedContent[ offset ] );
	offset += vendorLength;

	const uint32_t userCommentsCount = static_cast<uint32_t>( m_Comments.size() );
	ToBytesLE( userCommentsCount, modifiedContent, offset );
	offset += 4;

	for ( const auto& comment : m_Comments ) {
		const uint32_t commentSize = static_cast<uint32_t>( comment.first.size() + 1 + comment.second.size() );
		ToBytesLE( commentSize, modifiedContent, offset );
		offset += 4;
		std::copy( comment.first.begin(), comment.first.end(), &modifiedContent[ offset ] );
		offset += static_cast<uint32_t>( comment.first.size() );
		memcpy( &modifiedContent[ offset ], "=", 1 );
		offset += 1;
		std::copy( comment.second.begin(), comment.second.end(), &modifiedContent[ offset ] );
		offset += static_cast<uint32_t>( comment.second.size() );
	}

	if ( !m_BinaryData.empty() ) {
		std::copy( m_BinaryData.begin(), m_BinaryData.end(), &modifiedContent[ offset ] );
	}

	const uint32_t serial = m_OriginalPages.begin()->second.GetSerialNumber();

	// Generate the new comment header pages, keyed by sequence number.
	std::map<uint32_t, OggPage> modifiedPages;
	try {
		uint32_t sequence = 1;
		bool pagesRemaining = true;
		do {
			const bool isContinued = ( sequence > 1 );
			const OggPage page( isContinued, serial, sequence, modifiedContent );
			pagesRemaining = !page.IsComplete();
			modifiedPages.insert( std::map<uint32_t, OggPage>::value_type( sequence, page ) );
			++sequence;
		} while ( pagesRemaining );
	} catch ( const std::runtime_error& ) {
		modifiedPages.clear();
	}

	if ( !modifiedPages.empty() ) {
		if ( modifyInPlace ) {
			// Double check that our modified pages are consistent with the original pages.
			auto originalPage = m_OriginalPages.begin();
			auto modifiedPage = modifiedPages.begin();
			modifyInPlace = ( m_OriginalPages.size() == modifiedPages.size() );
			while ( modifyInPlace && ( m_OriginalPages.end() != originalPage ) && ( modifiedPages.end() != modifiedPage ) ) {
				modifyInPlace =
					( originalPage->second.GetSize() == modifiedPage->second.GetSize() ) &&
					( originalPage->second.GetSequenceNumber() == modifiedPage->first );
				++originalPage;
				++modifiedPage;
			}
		}

		if ( modifyInPlace ) {
			// Attempt to write out the comments in-place.
			auto originalPage = m_OriginalPages.begin();
			auto modifiedPage = modifiedPages.begin();
			bool ok = true;
			while ( ok && ( m_OriginalPages.end() != originalPage ) && ( modifiedPages.end() != modifiedPage ) ) {
				m_Stream.seekp( originalPage->first );
				const char* header = reinterpret_cast<const char*>( modifiedPage->second.GetHeader().data() );
				const char* content = reinterpret_cast<const char*>( modifiedPage->second.GetContent().data() );
				m_Stream.write( header, modifiedPage->second.GetHeader().size() );
				m_Stream.write( content, modifiedPage->second.GetContent().size() );
				ok = m_Stream.good();
				++originalPage;
				++modifiedPage;
			}
			wroteComments = ok;
		}

		if ( !wroteComments ) {
			m_Stream.clear();
			m_Stream.seekg( 0, std::ios::end );
			const std::streampos sourceStreamSize = m_Stream.tellg();
			m_Stream.seekg( 0 );
			bool ok = m_Stream.good();
			if ( ok ) {
				// Attempt to copy the original stream to a temporary file with the modified comment header.
				const std::wstring tempFilename = m_Filename + L".TmpOpusComment";
				try {
					// If the number of comment header pages has changed, we will need to adjust the sequence numbers of all subsequent pages.
					const int sequenceAdjustment = static_cast<int>( modifiedPages.size() ) - static_cast<int>( m_OriginalPages.size() );
					const uint32_t lastOriginalSequenceNumber = m_OriginalPages.rbegin()->second.GetSequenceNumber();

					std::ofstream outStream( tempFilename, std::ios::out | std::ios::binary, _SH_DENYRW );
					ok = ( m_Stream.good() && outStream.good() );
					while ( ok && ( m_Stream.tellg() < sourceStreamSize ) ) {
						OggPage page( m_Stream );
						if ( page.GetSerialNumber() == serial ) {
							const uint32_t sequence = page.GetSequenceNumber();
							if ( sequence <= lastOriginalSequenceNumber ) {
								if ( ( modifiedPages.end() != modifiedPages.find( sequence ) ) ) {
									// Write out the modified page.
									ok = modifiedPages[ sequence ].Write( outStream );
									modifiedPages.erase( sequence );
								} else if ( 0 == sequence ) {
									// Write out the original header.
									ok = page.Write( outStream );
								}
							} else {
								// We have reached the source audio data, so flush out any remaining modified pages.
								if ( !modifiedPages.empty() ) {
									auto modifiedPage = modifiedPages.begin();
									while ( ok && ( modifiedPages.end() != modifiedPage ) ) {
										ok = modifiedPage->second.Write( outStream );
										++modifiedPage;
									}
									modifiedPages.clear();
								}

								if ( 0 != sequenceAdjustment ) {
									// Alter the sequence number before writing out the original page.
									const uint32_t alteredSequence = sequence + sequenceAdjustment;
									page.SetSequenceNumber( alteredSequence );
								}

								// Write out the original page.
								ok = page.Write( outStream );
							}
						} else {
							// Just pass through pages from other logical bitstreams.
							ok = page.Write( outStream );
						}
					}
				} catch ( const std::runtime_error& ) {
					ok = false;
				}

				if ( ok ) {
					// Replace the original file with the modified copy.
					m_Stream.close();
					ok = ( 0 == _wunlink( m_Filename.c_str() ) );
					if ( ok ) {
						ok = ( 0 == _wrename( tempFilename.c_str(), m_Filename.c_str() ) );
					}
				}

				if ( !ok ) {
					_wunlink( tempFilename.c_str() );
				}

				wroteComments = ok;
			}
		}
	}
	return wroteComments;
}
