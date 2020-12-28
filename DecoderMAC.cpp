#include "DecoderMAC.h"

#include "Utility.h"

DecoderMAC::DecoderMAC( const std::wstring& filename ) :
	Decoder(),
	m_decompress( CreateIAPEDecompress( filename.c_str() )  )
{
	if ( m_decompress ) {
		const auto bps =  m_decompress->GetInfo( APE::APE_INFO_BITS_PER_SAMPLE );
		const auto channels = m_decompress->GetInfo( APE::APE_INFO_CHANNELS );
		const auto blockAlign = m_decompress->GetInfo( APE::APE_INFO_BLOCK_ALIGN );
		const auto bitrate = m_decompress->GetInfo( APE::APE_DECOMPRESS_AVERAGE_BITRATE );
		if ( ( channels > 0 ) && ( ( 8 == bps ) || ( 16 == bps ) || ( 24 == bps ) || ( 32 == bps ) ) && ( blockAlign == bps * channels / 8 ) ) {
			SetBPS( static_cast<long>( bps ) );
			SetChannels( static_cast<long>( channels ) );
			SetSampleRate( static_cast<long>( m_decompress->GetInfo( APE::APE_INFO_SAMPLE_RATE ) ) );
			SetDuration( static_cast<float>( m_decompress->GetInfo( APE::APE_DECOMPRESS_LENGTH_MS ) ) / 1000 );
			SetBitrate( static_cast<float>( bitrate ) );
		} else {
			m_decompress.reset();
		}
	}

	if ( !m_decompress ) {
		throw std::runtime_error( "DecoderMAC could not load file" );
	}
}

long DecoderMAC::Read( float* destBuffer, const long sampleCount )
{
	long samplesRead = 0;
	const long blockAlign = static_cast<long>( m_decompress->GetInfo( APE::APE_INFO_BLOCK_ALIGN ) );
	if ( blockAlign > 0 ) {
		std::vector<char> srcBuffer( sampleCount * blockAlign );
		long long blocksRead = 0;
		m_decompress->GetData( srcBuffer.data(), sampleCount, &blocksRead );
		if ( blocksRead > 0 ) {
			samplesRead = static_cast<long>( blocksRead );
			const long channels = GetChannels();
			const long bps = GetBPS();
			switch ( bps ) {
				case 8 : {
					unsigned char* uBuf = reinterpret_cast<unsigned char*>( srcBuffer.data() );
					for ( long long block = 0; block < blocksRead * channels; block++ ) {
						*destBuffer++ = static_cast<float>( static_cast<int>( uBuf[ block ] ) - 128 ) / 128;
					}
					break;
				}
				case 16 : {
					short* sBuf = reinterpret_cast<short*>( srcBuffer.data() );
					for ( long long block = 0; block < blocksRead * channels; block++ ) {
						*destBuffer++ = static_cast<float>( sBuf[ block ] ) / 32768;
					}
					break;
				}
				case 24 : {
					unsigned char* uBuf = reinterpret_cast<unsigned char*>( srcBuffer.data() );
					for ( long long block = 0; block < blocksRead * channels; block++ ) {
						*destBuffer++ = static_cast<float>( ( uBuf[ block * 3 + 2 ] << 24 ) | ( uBuf[ block * 3 + 1 ] << 16 ) | ( uBuf[ block * 3 ] << 8 ) ) / 2147483648ul;
					}
					break;
				}
				case 32 : {
					int32_t* iBuf = reinterpret_cast<int32_t*>( srcBuffer.data() );
					for ( long long block = 0; block < blocksRead * channels; block++ ) {
						*destBuffer++ = static_cast<float>( iBuf[ block ] ) / 2147483648ul;
					}
					break;
				}
				default : {
					samplesRead = 0;
					break;
				}
			}
		}
	}
	return samplesRead;
}

float DecoderMAC::Seek( const float position )
{
	const long long blockOffset = static_cast<long long>( GetSampleRate() * position );
	m_decompress->Seek( blockOffset );
	return static_cast<float>( m_decompress->GetInfo( APE::APE_DECOMPRESS_CURRENT_MS ) ) / 1000;
}
