#include "EncoderFlac.h"

#include "Utility.h"

#include <vector>

EncoderFlac::EncoderFlac() :
	Encoder(),
  FLAC::Encoder::File()
{
}

EncoderFlac::~EncoderFlac()
{
}

bool EncoderFlac::Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const long long totalSamples, const std::string& /*settings*/, const Tags& tags )
{
	bool success = false;
	filename += L".flac";
	FILE* f = _wfsopen( filename.c_str(), L"w+b", _SH_DENYRW );
	if ( nullptr != f ) {
		set_sample_rate( sampleRate );
		set_channels( channels );
		set_compression_level( 5 );
		set_verify( true );
		set_total_samples_estimate( ( totalSamples > 0 ) ? static_cast<FLAC__uint64>( totalSamples ) : 0 );

		const long bps = bitsPerSample.value_or( 0 );
		switch ( bps ) {
			case 8 :
			case 16 :
			case 24 : {
				set_bits_per_sample( bps );
				break;
			}
			default : {
				if ( bitsPerSample > 24 ) {
					set_bits_per_sample( 24 );
				} else {
					set_bits_per_sample( 16 );
				}
				break;
			}
		}

		constexpr uint32_t kSeekTableSecondsSpacing = 10;
		constexpr uint32_t kPaddingSize = 4096;

		std::vector<FLAC::Metadata::Prototype*> metadata;

		if ( ( totalSamples > 0 ) && ( sampleRate > 0 ) ) {
			const uint32_t sampleSpacing = static_cast<uint32_t>( sampleRate * kSeekTableSecondsSpacing );
			m_seekTable = std::make_unique<FLAC::Metadata::SeekTable>();
			if ( m_seekTable->template_append_spaced_points_by_samples( sampleSpacing, static_cast<FLAC__uint64>( totalSamples ) ) && m_seekTable->template_sort( false ) ) {
				metadata.push_back( m_seekTable.get() );
			}
		}

		m_vorbisComment = CreateVorbisComment( tags );
		if ( m_vorbisComment ) {
			metadata.push_back( m_vorbisComment.get() );
		}

		m_picture = CreatePicture( tags );
		if ( m_picture ) {
			metadata.push_back( m_picture.get() );
		}

		m_padding = std::make_unique<FLAC::Metadata::Padding>( kPaddingSize );
		metadata.push_back( m_padding.get() );

		set_metadata( metadata.data(), static_cast<uint32_t>( metadata.size() ) );

		success = ( FLAC__STREAM_ENCODER_INIT_STATUS_OK == init( f ) );
		if ( !success ) {
			fclose( f );
		}
	}
	return success;
}

bool EncoderFlac::Write( float* samples, const long sampleCount )
{
	const long bps = get_bits_per_sample();
	const long bufferSize = sampleCount * get_channels();
	std::vector<FLAC__int32> buffer( bufferSize );
	for ( long sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++ ) {
		buffer[ sampleIndex ] = static_cast<FLAC__int32>( 
			( 16 == bps ) ? FloatTo16( samples[ sampleIndex ] ) : ( ( 24 == bps ) ? FloatTo24( samples[ sampleIndex ] ) : FloatToSigned8( samples[ sampleIndex ] ) ) );
	}
	const bool success = process_interleaved( buffer.data(), sampleCount );
	return success;
}

void EncoderFlac::Close()
{
	finish();
}

std::unique_ptr<FLAC::Metadata::VorbisComment> EncoderFlac::CreateVorbisComment( const Tags& tags )
{
	std::unique_ptr<FLAC::Metadata::VorbisComment> vorbisComment = std::make_unique<FLAC::Metadata::VorbisComment>();
	for ( const auto& [ tag, value ] : tags ) {
		if ( !value.empty() ) {
			std::string field;
			switch ( tag ) {
				case Tag::Album : {
					field = "ALBUM";
					break;
				}
				case Tag::Artist : {
					field = "ARTIST";
					break;
				}
				case Tag::Comment : {
					field = "COMMENT";
					break;								
				}
				case Tag::Genre : {
					field = "GENRE";
					break;
				}
				case Tag::Title : {
					field = "TITLE";
					break;
				}
				case Tag::Track : {
					field = "TRACKNUMBER";
					break;
				}
				case Tag::Year : {
					field = "DATE";
					break;
				}
				case Tag::GainAlbum : {
					field = "REPLAYGAIN_ALBUM_GAIN";
					break;
				}
				case Tag::GainTrack : {
					field = "REPLAYGAIN_TRACK_GAIN";
					break;
				}
				default : {
					break;
				}
			}
			if ( !field.empty() ) {
				FLAC::Metadata::VorbisComment::Entry entry( field.c_str(), value.c_str() );
				vorbisComment->append_comment( entry );
			}
		}
	}
	return vorbisComment;
}

std::unique_ptr<FLAC::Metadata::Picture> EncoderFlac::CreatePicture( const Tags& tags )
{
	std::unique_ptr<FLAC::Metadata::Picture> picture;
	if ( const auto pictureIter = tags.find( Tag::Artwork ); tags.end() != pictureIter ) {
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
			picture = std::make_unique<FLAC::Metadata::Picture>();
			picture->set_mime_type( mimeType.c_str() );
			picture->set_width( static_cast<FLAC__uint32>( width ) );
			picture->set_height( static_cast<FLAC__uint32>( height ) );
			picture->set_depth( static_cast<FLAC__uint32>( depth ) );
			picture->set_colors( static_cast<FLAC__uint32>( colours ) );
			picture->set_data( imageBytes.data(), static_cast<FLAC__uint32>( imageSize ) );
			picture->set_type( FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER );
		}
	}
	return picture;
}
