#include "DecoderFlac.h"

DecoderFlac::DecoderFlac( const std::wstring& filename ) :
	Decoder(),
	FLAC::Decoder::Stream(),
	m_FileStream(),
	m_FLACFrame(),
	m_FrameBuffer(),
	m_FramePos( 0 ),
	m_Valid( false )
{
	m_FileStream.open( filename, std::ios::binary | std::ios::in );
	if ( m_FileStream.is_open() ) {
		if ( init() == FLAC__STREAM_DECODER_INIT_STATUS_OK )	{
			process_until_end_of_metadata();
		}
	}

	if ( m_Valid ) {
		SetBitrate( CalculateBitrate() );
	} else {
		finish();
		m_FileStream.close();
		throw std::runtime_error( "DecoderFlac could not load file" );
	}
}

DecoderFlac::~DecoderFlac()
{
	finish();
	m_FileStream.close();
}

long DecoderFlac::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	while ( samplesRead < sampleCount ) {
    if ( m_FramePos < m_FLACFrame.header.blocksize ) {
      const uint32_t samplesToCopy = std::min( m_FLACFrame.header.blocksize - m_FramePos, static_cast<uint32_t>( sampleCount - samplesRead ) );
      const auto first = m_FrameBuffer.begin() + m_FramePos * m_FLACFrame.header.channels;
      const auto last = first + samplesToCopy * m_FLACFrame.header.channels;
      const auto dest = buffer + samplesRead * m_FLACFrame.header.channels;
      std::copy( first, last, dest );
      m_FramePos += samplesToCopy;
      samplesRead += samplesToCopy;
    } else {
      m_FramePos = 0;
      m_FLACFrame = {};
      if ( !process_single() || ( 0 == m_FLACFrame.header.blocksize ) ) {
        break;
      }
    }
	}
	return samplesRead;
}

float DecoderFlac::Seek( const float position )
{
	float seekPosition = 0;
	m_FramePos = 0;
	m_FLACFrame = {};
	if ( ( GetSampleRate() > 0 ) && seek_absolute( static_cast<FLAC__uint64>( position * GetSampleRate() ) ) ) {
		seekPosition = static_cast<float>( m_FLACFrame.header.number.sample_number ) / GetSampleRate();
	}	else {
		reset();
    process_until_end_of_metadata();
	}
	return seekPosition;
}

std::optional<float> DecoderFlac::CalculateBitrate()
{
	std::optional<float> bitrate;
	if ( const float duration = GetDuration(); ( duration > 0 ) && m_FileStream.good() ) {
		const auto initial = m_FileStream.tellg();

		m_FileStream.seekg( 0, std::ios_base::end );
		const long long filesize = m_FileStream.tellg();
		m_FileStream.seekg( 0, std::ios_base::beg );

		std::vector<char> block( 4 );
		m_FileStream.read( block.data(), 4 );
		if ( ( 'f' == block[ 0 ] ) && ( 'L' == block[ 1 ] ) && ( 'a' ==  block[ 2 ] ) && ('C' == block[ 3 ] ) ) {
			m_FileStream.read( block.data(), 4 );
			while ( m_FileStream.good() ) {
				const long long currentPos = m_FileStream.tellg();
				const unsigned long blockSize = ( static_cast<unsigned char>( block[ 1 ] ) << 16 ) | ( static_cast<unsigned char>( block[ 2 ] ) << 8 ) | static_cast<unsigned char>( block[ 3 ] );
				if ( ( currentPos + blockSize ) < filesize ) {
					const bool lastBlock = block[ 0 ] & 0x80;
					if ( lastBlock ) {
						const long long streamsize = filesize - currentPos - blockSize;
						bitrate = ( streamsize * 8 ) / ( duration * 1000 );
						break;
					}
				} else {
					break;
				}
				m_FileStream.seekg( blockSize, std::ios_base::cur );
				m_FileStream.read( block.data(), 4 );
			};
		}

		if ( !m_FileStream.good() ) {
			m_FileStream.clear();
		}
		m_FileStream.seekg( initial );
	}
	return bitrate;
}

FLAC__StreamDecoderReadStatus DecoderFlac::read_callback( FLAC__byte buf[], size_t * size )
{
	FLAC__StreamDecoderReadStatus status = FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	if ( m_FileStream.eof() ) {
		*size = 0;
		status = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	} else {
		m_FileStream.read( reinterpret_cast<char*>( buf ), *size );
		*size = static_cast<size_t>( m_FileStream.gcount() );
		if ( *size > 0 ) {
			status = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	}
	return status;
}

FLAC__StreamDecoderSeekStatus DecoderFlac::seek_callback( FLAC__uint64 pos )
{
	if ( !m_FileStream.good() ) {
		m_FileStream.clear();
	}
	m_FileStream.seekg( pos, std::ios_base::beg );
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus DecoderFlac::tell_callback( FLAC__uint64 * pos )
{
	if ( !m_FileStream.good() ) {
		m_FileStream.clear();
	}
	*pos = m_FileStream.tellg();
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus DecoderFlac::length_callback( FLAC__uint64 * pos )
{
	if ( !m_FileStream.good() ) {
		m_FileStream.clear();
	}
	const std::streampos currentPos = m_FileStream.tellg();
	m_FileStream.seekg( 0, std::ios_base::end );
	*pos = m_FileStream.tellg();
	m_FileStream.seekg( currentPos );
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool DecoderFlac::eof_callback()
{
	const bool eof = m_FileStream.eof();
	return eof;
}

FLAC__StreamDecoderWriteStatus DecoderFlac::write_callback( const FLAC__Frame * frame, const FLAC__int32 *const buffer[] )
{
  m_FLACFrame = *frame;
  m_FrameBuffer.resize( m_FLACFrame.header.blocksize * m_FLACFrame.header.channels );
  uint32_t offset = 0;
  for ( uint32_t sample = 0; sample < m_FLACFrame.header.blocksize; sample++ ) {
	  for ( uint32_t channel = 0; channel < m_FLACFrame.header.channels; channel++ ) {
	  	m_FrameBuffer[ offset++ ] = static_cast<float>( buffer[ channel ][ sample ] ) / ( 1ul << ( m_FLACFrame.header.bits_per_sample - 1ul ) );
		}
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void DecoderFlac::metadata_callback( const FLAC__StreamMetadata * metadata )
{
	if ( metadata->type == FLAC__METADATA_TYPE_STREAMINFO ) {
		SetBPS( metadata->data.stream_info.bits_per_sample );
		SetChannels( metadata->data.stream_info.channels );
		SetSampleRate( metadata->data.stream_info.sample_rate );
		if ( GetSampleRate() > 0 ) {
			SetDuration( static_cast<float>( metadata->data.stream_info.total_samples ) / GetSampleRate() );
		}
		m_Valid = true;
	}
}

void DecoderFlac::error_callback( FLAC__StreamDecoderErrorStatus )
{
}
