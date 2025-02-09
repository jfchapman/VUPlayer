#include "HandlerFlac.h"

#include "DecoderFlac.h"
#include "EncoderFlac.h"

#include "Utility.h"

// Amount of padding to add when writing out FLAC files that don't contain any padding.
constexpr uint32_t kPaddingSize = 1024;

HandlerFlac::HandlerFlac() :
	Handler()
{
}

HandlerFlac::~HandlerFlac()
{
}

std::wstring HandlerFlac::GetDescription() const
{
	const std::wstring description = L"Free Lossless Audio Codec " + UTF8ToWideString( FLAC__VERSION_STRING );
	return description;
}

std::set<std::wstring> HandlerFlac::GetSupportedFileExtensions() const
{
	return { L"flac" };
}

bool HandlerFlac::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	tags.clear();
	FLAC::Metadata::SimpleIterator iterator;
	if ( iterator.is_valid() && iterator.init( WideStringToUTF8( filename ).c_str(), true /*readOnly*/, true /*preserveFileStats*/ ) ) {
		success = true;
		do {
			const FLAC__MetadataType blockType = iterator.get_block_type();
			if ( FLAC__METADATA_TYPE_VORBIS_COMMENT == blockType ) {
				FLAC::Metadata::Prototype* block = iterator.get_block();
				if ( nullptr != block ) {
					FLAC::Metadata::VorbisComment* vorbisComment = dynamic_cast<FLAC::Metadata::VorbisComment*>( block );
					if ( ( nullptr != vorbisComment ) && ( vorbisComment->is_valid() ) ) {
						const unsigned char* vendor = vorbisComment->get_vendor_string();
						if ( ( nullptr != vendor ) && ( vendor[ 0 ] != 0 ) ) {
							tags.insert( Tags::value_type( Tag::Version, reinterpret_cast<const char*>( vendor ) ) );
						}
						const unsigned int commentCount = vorbisComment->get_num_comments();
						for ( unsigned int commentIndex = 0; commentIndex < commentCount; commentIndex++ ) {
							const FLAC::Metadata::VorbisComment::Entry entry = vorbisComment->get_comment( commentIndex );
							if ( ( entry.is_valid() ) && ( entry.get_field_name_length() != 0 ) && ( entry.get_field_value_length() != 0 ) ) {
								const char* field = entry.get_field_name();
								if ( 0 == _stricmp( field, "ARTIST" ) ) {
									tags.insert( Tags::value_type( Tag::Artist, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "TITLE" ) ) {
									tags.insert( Tags::value_type( Tag::Title, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "ALBUM" ) ) {
									tags.insert( Tags::value_type( Tag::Album, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "GENRE" ) ) {
									tags.insert( Tags::value_type( Tag::Genre, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "YEAR" ) ) {
									tags.insert( Tags::value_type( Tag::Year, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "DATE" ) ) {
									// Prefer 'date' over 'year' (both map to the same tag type).
									tags[ Tag::Year ] = entry.get_field_value();
								} else if ( 0 == _stricmp( field, "COMMENT" ) ) {
									tags.insert( Tags::value_type( Tag::Comment, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "TRACK" ) ) {
									tags.insert( Tags::value_type( Tag::Track, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "TRACKNUMBER" ) ) {
									// Prefer 'tracknumber' over 'track' (both map to the same tag type).
									tags[ Tag::Track ] = entry.get_field_value();
								} else if ( 0 == _stricmp( field, "REPLAYGAIN_TRACK_GAIN" ) ) {
									tags.insert( Tags::value_type( Tag::GainTrack, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "REPLAYGAIN_ALBUM_GAIN" ) ) {
									tags.insert( Tags::value_type( Tag::GainAlbum, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "COMPOSER" ) ) {
									tags.insert( Tags::value_type( Tag::Composer, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "CONDUCTOR" ) ) {
									tags.insert( Tags::value_type( Tag::Conductor, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "LABEL" ) ) {
									tags.insert( Tags::value_type( Tag::Publisher, entry.get_field_value() ) );
								} else if ( 0 == _stricmp( field, "PUBLISHER" ) ) {
									// Prefer 'publisher' over 'label' (both map to the same tag type).
									tags[ Tag::Publisher ] = entry.get_field_value();
								}
							}
						}
					}
					delete block;
					block = nullptr;
				}
			} else if ( FLAC__METADATA_TYPE_PICTURE == blockType ) {
				FLAC::Metadata::Prototype* block = iterator.get_block();
				if ( nullptr != block ) {
					FLAC::Metadata::Picture* picture = dynamic_cast<FLAC::Metadata::Picture*>( block );
					if ( ( nullptr != picture ) && ( picture->is_valid() ) && ( FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER == picture->get_type() ) ) {
						const int dataLength = static_cast<int>( picture->get_data_length() );
						if ( dataLength > 0 ) {
							const std::string encodedImage = Base64Encode( reinterpret_cast<const BYTE*>( picture->get_data() ), dataLength );
							if ( !encodedImage.empty() ) {
								tags.insert( Tags::value_type( Tag::Artwork, encodedImage ) );
							}
						}
					}
					delete block;
					block = nullptr;
				}
			}
		} while ( iterator.next() );
	}
	return success;
}

bool HandlerFlac::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	bool success = false;
	FLAC::Metadata::Chain chain;
	if ( chain.is_valid() && chain.read( WideStringToUTF8( filename ).c_str() ) ) {
		FLAC::Metadata::Iterator iterator;
		if ( iterator.is_valid() ) {
			iterator.init( chain );

			bool writeChain = false;

			// Determine which metadata blocks need to be updated.
			bool updateVorbisComment = false;
			bool updatePicture = false;
			bool clearPicture = false;
			for ( const auto& tagIter : tags ) {
				const Tag tag = tagIter.first;
				switch ( tag ) {
					case Tag::Album:
					case Tag::Artist:
					case Tag::Comment:
					case Tag::Genre:
					case Tag::Title:
					case Tag::Track:
					case Tag::Year:
					case Tag::GainAlbum:
					case Tag::GainTrack:
					case Tag::Composer:
					case Tag::Conductor:
					case Tag::Publisher: {
						updateVorbisComment = true;
						break;
					}
					case Tag::Artwork: {
						updatePicture = true;
						clearPicture = tagIter.second.empty();
						break;
					}
					default: {
						break;
					}
				}
			}

			if ( !updateVorbisComment && !updatePicture ) {
				success = true;
			} else {

				if ( updateVorbisComment ) {
					// Insert a vorbis comment block if necessary.
					bool vorbisCommentExists = false;
					do {
						vorbisCommentExists = ( FLAC__METADATA_TYPE_VORBIS_COMMENT == iterator.get_block_type() );
					} while ( !vorbisCommentExists && iterator.next() );
					if ( !vorbisCommentExists ) {
						FLAC::Metadata::VorbisComment* vorbisComment = new FLAC::Metadata::VorbisComment();
						iterator.insert_block_after( vorbisComment );
						writeChain = true;
					}

					// Update vorbis comments.
					iterator.init( chain );
					do {
						const FLAC__MetadataType blockType = iterator.get_block_type();
						if ( FLAC__METADATA_TYPE_VORBIS_COMMENT == blockType ) {
							FLAC::Metadata::Prototype* block = iterator.get_block();
							if ( nullptr != block ) {
								FLAC::Metadata::VorbisComment* vorbisComment = dynamic_cast<FLAC::Metadata::VorbisComment*>( block );
								if ( nullptr != vorbisComment ) {
									for ( const auto& tagIter : tags ) {
										const Tag tag = tagIter.first;
										const std::string& value = tagIter.second;
										std::string field;
										switch ( tag ) {
											case Tag::Album: {
												field = "ALBUM";
												break;
											}
											case Tag::Artist: {
												field = "ARTIST";
												break;
											}
											case Tag::Comment: {
												field = "COMMENT";
												break;
											}
											case Tag::Genre: {
												field = "GENRE";
												break;
											}
											case Tag::Title: {
												field = "TITLE";
												break;
											}
											case Tag::Track: {
												field = "TRACKNUMBER";
												break;
											}
											case Tag::Year: {
												field = "DATE";
												break;
											}
											case Tag::GainAlbum: {
												field = "REPLAYGAIN_ALBUM_GAIN";
												break;
											}
											case Tag::GainTrack: {
												field = "REPLAYGAIN_TRACK_GAIN";
												break;
											}
											case Tag::Composer: {
												field = "COMPOSER";
												break;
											}
											case Tag::Conductor: {
												field = "CONDUCTOR";
												break;
											}
											case Tag::Publisher: {
												field = "PUBLISHER";
												break;
											}
											default: {
												break;
											}
										}
										if ( !field.empty() ) {
											if ( value.empty() ) {
												vorbisComment->remove_entries_matching( field.c_str() );
												writeChain = true;
											} else {
												FLAC::Metadata::VorbisComment::Entry entry( field.c_str(), value.c_str() );
												vorbisComment->replace_comment( entry, true /*all*/ );
												writeChain = true;
											}
										}
									}
								}
								delete block;
								block = nullptr;
							}
							break;
						}
					} while ( iterator.next() );
				}

				if ( updatePicture ) {
					iterator.init( chain );
					// Delete any existing (front cover) picture blocks.
					do {
						if ( FLAC__METADATA_TYPE_PICTURE == iterator.get_block_type() ) {
							FLAC::Metadata::Prototype* block = iterator.get_block();
							if ( nullptr != block ) {
								FLAC::Metadata::Picture* picture = dynamic_cast<FLAC::Metadata::Picture*>( block );
								if ( ( nullptr != picture ) && ( FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER == picture->get_type() ) ) {
									iterator.delete_block( false /*replaceWithPadding*/ );
									writeChain = true;
								}
								delete block;
								block = nullptr;
							}
						}
					} while ( iterator.next() );

					if ( !clearPicture ) {
						// Insert (front cover) picture block.
						const auto pictureIter = tags.find( Tag::Artwork );
						if ( tags.end() != pictureIter ) {
							const std::string& encodedImage = pictureIter->second;
							const std::vector<BYTE> imageBytes = Base64Decode( encodedImage );
							const size_t imageSize = imageBytes.size();
							if ( imageSize > 0 ) {
								std::string mimeType;
								int width = 0;
								int height = 0;
								int depth = 0;
								int colours = 0;
								GetImageInformation( encodedImage, mimeType, width, height, depth, colours );
								FLAC::Metadata::Picture* picture = new FLAC::Metadata::Picture();
								picture->set_mime_type( mimeType.c_str() );
								picture->set_width( static_cast<FLAC__uint32>( width ) );
								picture->set_height( static_cast<FLAC__uint32>( height ) );
								picture->set_depth( static_cast<FLAC__uint32>( depth ) );
								picture->set_colors( static_cast<FLAC__uint32>( colours ) );
								picture->set_data( imageBytes.data(), static_cast<FLAC__uint32>( imageSize ) );
								picture->set_type( FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER );
								iterator.insert_block_after( picture );
								writeChain = true;
							}
						}
					}
				}

				if ( writeChain ) {
					chain.sort_padding();

					// If there is no padding, add an arbitrary amount before writing out the chain.
					iterator.init( chain );
					bool paddingExists = false;
					do {
						if ( FLAC__METADATA_TYPE_PADDING == iterator.get_block_type() ) {
							FLAC::Metadata::Prototype* block = iterator.get_block();
							if ( nullptr != block ) {
								FLAC::Metadata::Padding* padding = dynamic_cast<FLAC::Metadata::Padding*>( block );
								paddingExists = ( ( nullptr != padding ) && ( padding->get_length() > 0 ) );
								delete block;
								block = nullptr;
							}
						}
					} while ( !paddingExists && iterator.next() );
					if ( !paddingExists ) {
						FLAC::Metadata::Padding* padding = new FLAC::Metadata::Padding( kPaddingSize );
						iterator.insert_block_after( padding );
						chain.sort_padding();
					}

					success = chain.write();
				}
			}
		}
	}
	return success;
}

Decoder::Ptr HandlerFlac::OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const
{
	DecoderFlac* streamFlac = nullptr;
	try {
		streamFlac = new DecoderFlac( filename, context );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( streamFlac );
	return stream;
}

Encoder::Ptr HandlerFlac::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderFlac() );
	return encoder;
}

bool HandlerFlac::IsDecoder() const
{
	return true;
}

bool HandlerFlac::IsEncoder() const
{
	return true;
}

bool HandlerFlac::CanConfigureEncoder() const
{
	return false;
}

bool HandlerFlac::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerFlac::SettingsChanged( Settings& /*settings*/ )
{
}
