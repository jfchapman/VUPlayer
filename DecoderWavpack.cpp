#include "DecoderWavpack.h"

#include "Utility.h"

DecoderWavpack::DecoderWavpack( const std::wstring& filename ) :
	Decoder(),
	m_Context( nullptr )
{
	char* error = nullptr;
	const int flags = OPEN_WVC | OPEN_NORMALIZE | OPEN_DSD_AS_PCM;
	const int offset = 0;
	m_Context = WavpackOpenFileInput( WideStringToAnsiCodePage( filename ).c_str(), error, flags, offset );
	if ( nullptr != m_Context ) {
		SetBPS( static_cast<long>( WavpackGetBitsPerSample( m_Context ) ) );
		SetChannels( static_cast<long>( WavpackGetNumChannels( m_Context ) ) );
		SetSampleRate( static_cast<long>( WavpackGetSampleRate( m_Context ) ) );
		if ( GetSampleRate() > 0 ) {
			SetDuration( static_cast<float>( WavpackGetNumSamples64( m_Context ) ) / GetSampleRate() );
		}	
	} else {
		throw std::runtime_error( "DecoderWavpack could not load file" );
	}
}

DecoderWavpack::~DecoderWavpack()
{
	WavpackCloseFile( m_Context );
}

long DecoderWavpack::Read( float* buffer, const long sampleCount )
{
	const long samplesRead = ( sampleCount > 0 ) ? static_cast<long>( WavpackUnpackSamples( m_Context, reinterpret_cast<int32_t*>( buffer ), sampleCount ) ) : 0;
	if ( !( WavpackGetMode( m_Context ) & MODE_FLOAT ) ) {
		int32_t* nativeBuffer = reinterpret_cast<int32_t*>( buffer );
		const long bufferSize = samplesRead * GetChannels();
		const int bitsPerSample = WavpackGetBytesPerSample( m_Context ) * 8;
		for ( long index = 0; index < bufferSize; index++ ) {
			buffer[ index ] = static_cast<float>( nativeBuffer[ index ] ) / ( 1 << ( bitsPerSample - 1 ) );
		}
	}
	return samplesRead;
}

float DecoderWavpack::Seek( const float position )
{
	float seekPosition = position;
	const int64_t samplePosition = static_cast<int64_t>( position * GetSampleRate() );
	if ( 0 == WavpackSeekSample64( m_Context, samplePosition ) ) {
		seekPosition = 0;
	}
	return seekPosition;
}
