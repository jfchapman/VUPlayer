#include "EncoderAAC.h"

#include <set>

extern "C"
{
#include "libavcodec/avcodec.h"
}

EncoderAAC::EncoderAAC() : EncoderFFmpeg()
{
}

EncoderAAC::~EncoderAAC()
{
}

// Minimum/maximum/default bit rates.
constexpr int kMinimumAACBitrate = 64;
constexpr int kMaximumAACBitrate = 320;
constexpr int kDefaultAACBitrate = 128;

int EncoderAAC::GetBitrate( const std::string& settings )
{
	int bitrate = kDefaultAACBitrate;
	if ( !settings.empty() ) {
		try {
			bitrate = std::stoi( settings );
			LimitBitrate( bitrate );
		} catch ( const std::logic_error& ) {
		}
	}
	return bitrate;
}

int EncoderAAC::GetDefaultBitrate()
{
	return kDefaultAACBitrate;
}

int EncoderAAC::GetMinimumBitrate()
{
	return kMinimumAACBitrate;
}

int EncoderAAC::GetMaximumBitrate()
{
	return kMaximumAACBitrate;
}

void EncoderAAC::LimitBitrate( int& bitrate )
{
	if ( bitrate < kMinimumAACBitrate ) {
		bitrate = kMinimumAACBitrate;
	} else if ( bitrate > kMaximumAACBitrate ) {
		bitrate = kMaximumAACBitrate;
	}
}

std::wstring EncoderAAC::GetFileExtension() const
{
	return L".m4a";
}

const AVCodec* EncoderAAC::GetCodec() const
{
	return avcodec_find_encoder( AV_CODEC_ID_AAC );
}

bool EncoderAAC::SetupCodecContext( AVCodecContext* codecContext, const AVCodec* codec, const long sampleRate, const long channels, const std::optional<long> /*bitsPerSample*/, const std::string& settings ) const
{
	codecContext->sample_rate = 0;
	int numSupportedSampleRates = 0;
	const int* pSupportedSampleRates = nullptr;
	if ( avcodec_get_supported_config( codecContext, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, reinterpret_cast<const void**>( &pSupportedSampleRates ), &numSupportedSampleRates ) >= 0 ) {
		std::set<int> supportedSampleRates;
		supportedSampleRates.insert( pSupportedSampleRates, pSupportedSampleRates + numSupportedSampleRates );
		for ( const auto supportedSampleRate : supportedSampleRates ) {
			if ( supportedSampleRate >= sampleRate ) {
				codecContext->sample_rate = supportedSampleRate;
				break;
			}
		}
		if ( 0 == codecContext->sample_rate ) {
			codecContext->sample_rate = *supportedSampleRates.rbegin();
		}
	}
	if ( codecContext->sample_rate > 0 ) {
		codecContext->ch_layout = GetChannelLayout( channels );
		codecContext->bit_rate = 1000 * GetBitrate( settings );
		codecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
		return true;
	}
	return false;
}
