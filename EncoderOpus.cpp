#include "EncoderOpus.h"

#include "Handler.h"
#include "Utility.h"

#include <vector>

// Minimum/maximum/default bit rates.
constexpr int kMinimumOpusBitrate = 8;
constexpr int kMaximumOpusBitrate = 256;
constexpr int kDefaultOpusBitrate = 128;

EncoderOpus::EncoderOpus() :
	Encoder(),
	m_Channels( 0 ),
	m_OpusEncoder( nullptr ),
	m_Callbacks( {} )
{
}

EncoderOpus::~EncoderOpus()
{
}

int EncoderOpus::WriteCallback( void *user_data, const unsigned char *ptr, opus_int32 len )
{
	FILE* f = reinterpret_cast<FILE*>( user_data );
	if ( ( nullptr != f ) && ( nullptr != ptr ) && ( len > 0 ) ) {
		fwrite( ptr, 1, len, f );
	}
	return 0;
}

int EncoderOpus::CloseCallback( void *user_data )
{
	FILE* f = reinterpret_cast<FILE*>( user_data );
	if ( nullptr != f ) {
		fclose( f );
	}
	return 0;
}


bool EncoderOpus::Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> /*bitsPerSample*/, const long long /*totalSamples*/, const std::string& settings, const Tags& /*tags*/ )
{
	m_Channels = channels;
	OggOpusComments* opusComments = ope_comments_create();
	if ( nullptr != opusComments ) {
		filename += L".opus";

		FILE* f = _wfsopen( filename.c_str(), L"wb", _SH_DENYRW );
		if ( nullptr != f ) {
			m_Callbacks.write = WriteCallback;
			m_Callbacks.close = CloseCallback;
			const int family = ( m_Channels > 8 ) ? 255 : ( ( m_Channels > 2 ) ? 1 : 0 );
			m_OpusEncoder = ope_encoder_create_callbacks( &m_Callbacks, f /*userData*/, opusComments, sampleRate, channels, family, nullptr /*error*/ );
			if ( nullptr != m_OpusEncoder ) {
				const int bitrate = 1000 * GetBitrate( settings );
				ope_encoder_ctl( m_OpusEncoder, OPUS_SET_BITRATE( bitrate ) );
			} else {
				fclose( f );
			}
		}
		ope_comments_destroy( opusComments );
	}
	const bool success = ( nullptr != m_OpusEncoder );
	return success;
}

bool EncoderOpus::Write( float* samples, const long sampleCount )
{
	// For multi-channel streams, change from BASS to Opus channel ordering.
	switch ( m_Channels ) {
		case 3: {
			// (left, right, center) ->
			// (left, center, right)
			long offset = 0;
			for ( long n = 0; n < sampleCount; n++, offset += m_Channels ) {
				std::swap( samples[ offset + 1 ], samples[ offset + 2 ] );
			}
			break;
		}
		case 5: {
			// (front left, front right, front center, rear left, rear right) ->
			// (front left, front center, front right, rear left, rear right)
			long offset = 0;
			for ( long n = 0; n < sampleCount; n++, offset += m_Channels ) {
				std::swap( samples[ offset + 1 ], samples[ offset + 2 ] );
			}
			break;
		}
		case 6: {
			// (front left, front right, front center, LFE, rear left, rear right) ->
			// (front left, front center, front right, rear left, rear right, LFE)
			long offset = 0;
			for ( long n = 0; n < sampleCount; n++, offset += m_Channels ) {
				std::swap( samples[ offset + 1 ], samples[ offset + 2 ] );
				const float lfe = samples[ offset + 3 ];
				const float rearL = samples[ offset + 4 ];
				const float rearR = samples[ offset + 5 ];
				samples[ offset + 3 ] = rearL;
				samples[ offset + 4 ] = rearR;
				samples[ offset + 5 ] = lfe;
			}
			break;
		}
		case 7: {
			// (front left, front right, front center, LFE, rear center, side left, side right) ->
			// (front left, front center, front right, side left, side right, rear center, LFE)
			long offset = 0;
			for ( long n = 0; n < sampleCount; n++, offset += m_Channels ) {
				std::swap( samples[ offset + 1 ], samples[ offset + 2 ] );
				const float lfe = samples[ offset + 3 ];
				const float rearC = samples[ offset + 4 ];
				const float sideL = samples[ offset + 5 ];
				const float sideR = samples[ offset + 6 ];
				samples[ offset + 3 ] = sideL;
				samples[ offset + 4 ] = sideR;
				samples[ offset + 5 ] = rearC;
				samples[ offset + 6 ] = lfe;
			}
			break;
		}
		case 8: {
			// (front left, front right, front center, LFE, rear left, rear right, side left, side right) ->
			// (front left, front center, front right, side left, side right, rear left, rear right, LFE)
			long offset = 0;
			for ( long n = 0; n < sampleCount; n++, offset += m_Channels ) {
				std::swap( samples[ offset + 1 ], samples[ offset + 2 ] );
				const float lfe = samples[ offset + 3 ];
				const float rearL = samples[ offset + 4 ];
				const float rearR = samples[ offset + 5 ];
				const float sideL = samples[ offset + 6 ];
				const float sideR = samples[ offset + 7 ];
				samples[ offset + 3 ] = sideL;
				samples[ offset + 4 ] = sideR;
				samples[ offset + 5 ] = rearL;
				samples[ offset + 6 ] = rearR;
				samples[ offset + 7 ] = lfe;
			}
			break;
		}
		default: {
			break;
		}
	}
	const bool success = ( OPE_OK == ope_encoder_write_float( m_OpusEncoder, samples, sampleCount ) );
	return success;
}

void EncoderOpus::Close()
{
	if ( nullptr != m_OpusEncoder ) {
		ope_encoder_drain( m_OpusEncoder );
		ope_encoder_destroy( m_OpusEncoder );
	}
}

int EncoderOpus::GetBitrate( const std::string& settings )
{
	int bitrate = kDefaultOpusBitrate;
	try {
		bitrate = std::stoi( settings );
	} catch ( const std::logic_error& ) {
	}
	LimitBitrate( bitrate );
	return bitrate;
}

int EncoderOpus::GetDefaultBitrate()
{
	return kDefaultOpusBitrate;
}

int EncoderOpus::GetMinimumBitrate()
{
	return kMinimumOpusBitrate;
}

int EncoderOpus::GetMaximumBitrate()
{
	return kMaximumOpusBitrate;
}

void EncoderOpus::LimitBitrate( int& bitrate )
{
	if ( bitrate < kMinimumOpusBitrate ) {
		bitrate = kMinimumOpusBitrate;
	} else if ( bitrate > kMaximumOpusBitrate ) {
		bitrate = kMaximumOpusBitrate;
	}
}
