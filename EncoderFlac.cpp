#include "EncoderFlac.h"

#include <vector>

EncoderFlac::EncoderFlac() :
	Encoder()
{
}

EncoderFlac::~EncoderFlac()
{
}

bool EncoderFlac::Open( const std::wstring& filename, const long sampleRate, const long channels, const long bitsPerSample )
{
	bool success = false;
	FILE* f = nullptr;
	if ( ( 0 == _wfopen_s( &f, filename.c_str(), L"wb" ) ) && ( nullptr != f ) ) {
		set_sample_rate( sampleRate );
		set_channels( channels );
		set_bits_per_sample( ( 0 == bitsPerSample ) ? 16 : bitsPerSample );
		set_compression_level( 5 );
		set_verify( true );
		set_total_samples_estimate( 0 );

		success = ( FLAC__STREAM_ENCODER_INIT_STATUS_OK == init( f ) );
		if ( !success ) {
			fclose( f );
		}
	}
	return success;
}

bool EncoderFlac::Write( float* samples, const long sampleCount )
{
	const long channels = get_channels();
	const long bps = get_bits_per_sample();
	const long scale = 1 << ( bps - 1 );
	const long bufferSize = sampleCount * get_channels();
	std::vector<FLAC__int32> buffer( bufferSize );
	for ( long sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++ ) {
		buffer[ sampleIndex ] = static_cast<FLAC__int32>( samples[ sampleIndex ] * scale );
	}
	const bool success = process_interleaved( &buffer[ 0 ], sampleCount );
	return success;
}

void EncoderFlac::Close()
{
	finish();
}
