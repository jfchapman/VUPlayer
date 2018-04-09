#include "DecoderOpus.h"

#include "Utility.h"

DecoderOpus::DecoderOpus( const std::wstring& filename ) :
	Decoder(),
	m_OpusFile( nullptr )
{
	int error = 0;
	const std::string filepath = WideStringToAnsiCodePage( filename );
	m_OpusFile = op_open_file( filepath.c_str(), &error );
	if ( nullptr != m_OpusFile ) {
		const OpusHead* head = op_head( m_OpusFile, -1 /*link*/ );
		if ( nullptr != head ) {
			SetSampleRate( 48000 );
			SetChannels( head->channel_count );
			const ogg_int64_t pcmTotal = op_pcm_total( m_OpusFile, -1 /*link*/ );
			SetDuration( static_cast<float>( pcmTotal ) / 48000 );
		}
	} else {
		throw std::runtime_error( "DecoderOpus could not load file" );
	}
}

DecoderOpus::~DecoderOpus()
{
	if ( nullptr != m_OpusFile ) {
		op_free( m_OpusFile );
	}
}

long DecoderOpus::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	const long channels = GetChannels();
	if ( ( channels > 0 ) && ( sampleCount > 0 ) ) {
		while ( samplesRead < sampleCount ) {
			const int samplesToRead = sampleCount - samplesRead;
			const int bufSize = samplesToRead * channels;
			const int result = op_read_float( m_OpusFile, buffer + samplesRead * channels, bufSize, nullptr /*link*/ );
			if ( result > 0 ) {
				samplesRead += result;
			} else {
				break;
			}
		}
	}
	return samplesRead;
}

float DecoderOpus::Seek( const float position )
{
	const ogg_int64_t offset = static_cast<ogg_int64_t>( position * GetSampleRate() );
	const float seekPosition = ( 0 == op_pcm_seek( m_OpusFile, offset ) ) ? position : 0;
	return seekPosition;
}
