#include "EncoderFlac.h"

#include "Utility.h"

#include <vector>

EncoderFlac::EncoderFlac() :
	Encoder()
{
}

EncoderFlac::~EncoderFlac()
{
}

bool EncoderFlac::Open( std::wstring& filename, const long sampleRate, const long channels, const long bitsPerSample, const std::string& /*settings*/ )
{
	bool success = false;
	filename += L".flac";
	FILE* f = _wfsopen( filename.c_str(), L"wb", _SH_DENYRW );;
	if ( nullptr != f ) {
		set_sample_rate( sampleRate );
		set_channels( channels );
		set_compression_level( 5 );
		set_verify( true );
		set_total_samples_estimate( 0 );

		switch ( bitsPerSample ) {
			case 8 :
			case 16 :
			case 24 : {
				set_bits_per_sample( bitsPerSample );
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
	const long bufferSize = sampleCount * get_channels();
	std::vector<FLAC__int32> buffer( bufferSize );
	for ( long sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++ ) {
		buffer[ sampleIndex ] = static_cast<FLAC__int32>( 
			( 16 == bps ) ? FloatTo16( samples[ sampleIndex ] ) : ( ( 24 == bps ) ? FloatTo24( samples[ sampleIndex ] ) : FloatToSigned8( samples[ sampleIndex ] ) ) );
	}
	const bool success = process_interleaved( &buffer[ 0 ], sampleCount );
	return success;
}

void EncoderFlac::Close()
{
	finish();
}
