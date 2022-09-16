#include "EncoderMP3.h"

#include "bassmix.h"

#include <stdexcept>

void null_report_function( const char* /*format*/, va_list /*ap*/ )
{
}

EncoderMP3::EncoderMP3() :
	Encoder()
{
}

EncoderMP3::~EncoderMP3()
{
}

bool EncoderMP3::Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> /*bitsPerSample*/, const long long /*totalSamples*/, const std::string& settings, const Tags& /*tags*/ )
{
	bool success = false;

	m_flags = lame_init();
	if ( nullptr != m_flags ) {
		lame_set_errorf( m_flags, null_report_function );
		lame_set_debugf( m_flags, null_report_function );
		lame_set_msgf( m_flags, null_report_function );

		lame_set_VBR( m_flags, vbr_default );
		lame_set_VBR_quality( m_flags, static_cast<float>( GetVBRQuality( settings ) ) );

		lame_set_bWriteVbrTag( m_flags, 1 );

		m_inputChannels = channels;

    // Convert 6.1 files to 7.1, so that they are downmixed correctly.
    m_convert61to71 = ( 7 == m_inputChannels );
    if ( m_convert61to71 ) {
      m_inputChannels = 8;
    }

		const int outputChannels = std::min<int>( m_inputChannels, 2 );

		success = ( 0 == lame_set_num_channels( m_flags, outputChannels ) );
			( 0 == lame_set_in_samplerate( m_flags, static_cast<int>( sampleRate ) ) ) &&
			( 0 == lame_init_params( m_flags ) );

		if ( success && ( outputChannels < m_inputChannels ) ) {
			m_decoderStream = BASS_StreamCreate( sampleRate, m_inputChannels, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE, STREAMPROC_PUSH, this );
			m_mixerStream = BASS_Mixer_StreamCreate( sampleRate, outputChannels, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE );
			success = ( 0 != m_decoderStream ) && ( 0 != m_mixerStream ) && ( 0 != BASS_Mixer_StreamAddChannel( m_mixerStream, m_decoderStream, BASS_MIXER_CHAN_DOWNMIX ) );
		}

		if ( success ) {
			filename += L".mp3";
			m_file = _wfsopen( filename.c_str(), L"w+b", _SH_DENYRW );
			success = ( nullptr != m_file );
		}
		
		if ( !success ) {
			lame_close( m_flags );
			m_flags = nullptr;

			if ( 0 != m_mixerStream ) {
				BASS_StreamFree( m_mixerStream );
				m_mixerStream = 0;
			}
			if ( 0 != m_decoderStream ) {
				BASS_StreamFree( m_decoderStream );
				m_decoderStream = 0;
			}
		}
	}
	return success;
}

bool EncoderMP3::Write( float* samples, const long sampleCount )
{
	const int outputChannels = lame_get_num_channels( m_flags );

	if ( outputChannels < m_inputChannels ) {
    if ( m_convert61to71 ) {
      // Copy the back centre channel to the back left & back right channels.
      m_convertedBuffer.resize( sampleCount * 8 );
      for ( long sample = sampleCount - 1; sample >= 0; sample-- ) {
        const long srcOffset = sample * 7;
        const long destOffset = sample * 8;
        m_convertedBuffer[ destOffset + 7 ] = samples[ srcOffset + 6 ];
        m_convertedBuffer[ destOffset + 6 ] = samples[ srcOffset + 5 ];
        m_convertedBuffer[ destOffset + 5 ] = samples[ srcOffset + 4 ];
        m_convertedBuffer[ destOffset + 4 ] = samples[ srcOffset + 4 ];
        m_convertedBuffer[ destOffset + 3 ] = samples[ srcOffset + 3 ];
        m_convertedBuffer[ destOffset + 2 ] = samples[ srcOffset + 2 ];
        m_convertedBuffer[ destOffset + 1 ] = samples[ srcOffset + 1 ];
        m_convertedBuffer[ destOffset + 0 ] = samples[ srcOffset + 0 ];
      }
      samples = m_convertedBuffer.data();
    }

		const DWORD decodeBufferSize = sampleCount * m_inputChannels * 4;
		const DWORD mixBufferSize = outputChannels * sampleCount;
		m_mixBuffer.resize( outputChannels * sampleCount );
		if ( ( BASS_StreamPutData( m_decoderStream, samples, decodeBufferSize ) == decodeBufferSize ) && ( BASS_ChannelGetData( m_mixerStream, m_mixBuffer.data(), mixBufferSize * 4 ) == ( mixBufferSize * 4 ) ) ) {
			samples = m_mixBuffer.data();
		} else {
			samples = nullptr;
		}
	}

	bool success = ( nullptr != samples );
	if ( success ) {
		const int outputBufferSize = sampleCount * outputChannels;
		m_outputBuffer.resize( outputBufferSize );
		const int bytesEncoded = ( 1 == outputChannels ) ? 
			lame_encode_buffer_ieee_float( m_flags, samples, nullptr, sampleCount, m_outputBuffer.data(), outputBufferSize ) : 
			lame_encode_buffer_interleaved_ieee_float( m_flags, samples, sampleCount, m_outputBuffer.data(), outputBufferSize );
		success = ( bytesEncoded > 0 ) && ( nullptr != m_file ) && ( static_cast<size_t>( bytesEncoded ) == fwrite( m_outputBuffer.data(), 1 /*elementSize*/, bytesEncoded, m_file ) );
	}
	return success;
}

void EncoderMP3::Close()
{
	if ( nullptr != m_flags ) {
		const int outputBufferSize = 65536;
		m_outputBuffer.resize( outputBufferSize );
		const int bytesEncoded = lame_encode_flush( m_flags, m_outputBuffer.data(), outputBufferSize );
		if ( ( bytesEncoded > 0 ) && ( nullptr != m_file ) ) {
			fwrite( m_outputBuffer.data(), 1 /*elementSize*/, bytesEncoded, m_file );
		}

		lame_mp3_tags_fid( m_flags, m_file );

		lame_close( m_flags );
		m_flags = nullptr;
	}

	if ( nullptr != m_file ) {
		fclose( m_file );
		m_file = nullptr;
	}

	if ( 0 != m_mixerStream ) {
		BASS_StreamFree( m_mixerStream );
		m_mixerStream = 0;
	}

	if ( 0 != m_decoderStream ) {
		BASS_StreamFree( m_decoderStream );
		m_decoderStream = 0;
	}
}

int EncoderMP3::GetVBRQuality( const std::string& settings )
{
	int quality = 2;
	try {
		quality = std::stoi( settings );
		if ( ( quality < 0 ) || ( quality > 9 ) ) {
			quality = 2;
		}
	} catch ( const std::logic_error& ) {
	}
	return quality;
}
