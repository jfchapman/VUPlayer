#include "DecoderOpus.h"

#include "Utility.h"

DecoderOpus::DecoderOpus( const std::wstring& filename ) :
	Decoder(),
	m_OpusFile( nullptr )
{
	int error = 0;
	const std::string filepath = WideStringToUTF8( filename );
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
				// For multi-channel streams, change from Opus to BASS channel ordering.
				switch ( channels ) {
					case 3 : {
						// (left, center, right) ->
						// (left, right, center)
						int offset = samplesRead * channels;
						for ( int n = 0; n < result; n++, offset += channels ) {
							std::swap( buffer[ offset + 1 ], buffer[ offset + 2 ] );
						}
						break;
					}
					case 5 : {
						// (front left, front center, front right, rear left, rear right) ->
						// (front left, front right, front center, rear left, rear right)
						int offset = samplesRead * channels;
						for ( int n = 0; n < result; n++, offset += channels ) {
							std::swap( buffer[ offset + 1 ], buffer[ offset + 2 ] );
						}
						break;
					}
					case 6 : {
						// (front left, front center, front right, rear left, rear right, LFE) ->
						// (front left, front right, front center, LFE, rear left, rear right)
						int offset = samplesRead * channels;
						for ( int n = 0; n < result; n++, offset += channels ) {
							std::swap( buffer[ offset + 1 ], buffer[ offset + 2 ] );
							const float rearL = buffer[ offset + 3 ];
							const float rearR = buffer[ offset + 4 ];
							const float lfe = buffer[ offset + 5 ];
							buffer[ offset + 3 ] = lfe;
							buffer[ offset + 4 ] = rearL;
							buffer[ offset + 5 ] = rearR;
						}
						break;
					}
					case 7 : {
						// (front left, front center, front right, side left, side right, rear center, LFE) ->
						// (front left, front right, front center, LFE, rear center, side left, side right)
						int offset = samplesRead * channels;
						for ( int n = 0; n < result; n++, offset += channels ) {
							std::swap( buffer[ offset + 1 ], buffer[ offset + 2 ] );
							const float sideL = buffer[ offset + 3 ];
							const float sideR = buffer[ offset + 4 ];
							const float rearC = buffer[ offset + 5 ];
							const float lfe = buffer[ offset + 6 ];
							buffer[ offset + 3 ] = lfe;
							buffer[ offset + 4 ] = rearC;
							buffer[ offset + 5 ] = sideL;
							buffer[ offset + 6 ] = sideR;
						}
						break;
					}
					case 8 : {
						// (front left, front center, front right, side left, side right, rear left, rear right, LFE) ->
						// (front left, front right, front center, LFE, rear left, rear right, side left, side right)
						int offset = samplesRead * channels;
						for ( int n = 0; n < result; n++, offset += channels ) {
							std::swap( buffer[ offset + 1 ], buffer[ offset + 2 ] );
							const float sideL = buffer[ offset + 3 ];
							const float sideR = buffer[ offset + 4 ];
							const float rearL = buffer[ offset + 5 ];
							const float rearR = buffer[ offset + 6 ];
							const float lfe = buffer[ offset + 7 ];
							buffer[ offset + 3 ] = lfe;
							buffer[ offset + 4 ] = rearL;
							buffer[ offset + 5 ] = rearR;
							buffer[ offset + 6 ] = sideL;
							buffer[ offset + 7 ] = sideR;
						}
						break;
					}
					default: {
						break;
					}
				}
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
