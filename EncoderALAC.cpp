#include "EncoderALAC.h"

extern "C"
{
#include "libavcodec/avcodec.h"
}

EncoderALAC::EncoderALAC() : EncoderFFmpeg()
{
}

EncoderALAC::~EncoderALAC()
{
}

std::wstring EncoderALAC::GetFileExtension() const
{
	return L".m4a";
}

const AVCodec* EncoderALAC::GetCodec() const
{
	return avcodec_find_encoder( AV_CODEC_ID_ALAC );
}

bool EncoderALAC::SetupCodecContext( AVCodecContext* codecContext, const AVCodec* codec, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& /*settings*/ ) const
{
	codecContext->ch_layout = GetChannelLayout( 2 ); // Default to stereo output
	int numSupportedChannelLayouts = 0;
	const AVChannelLayout* pSupportedChannelLayouts = nullptr;
	if ( avcodec_get_supported_config( codecContext, codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, reinterpret_cast<const void**>( &pSupportedChannelLayouts ), &numSupportedChannelLayouts ) >= 0 ) {
		for ( int i = 0; i < numSupportedChannelLayouts; i++ ) {
			if ( pSupportedChannelLayouts[ i ].nb_channels == channels ) {
				codecContext->ch_layout = pSupportedChannelLayouts[ i ];
				break;
			}
		}
	}
	codecContext->sample_rate = static_cast<int>( sampleRate );
	codecContext->sample_fmt = ( bitsPerSample.value_or( 0 ) > 16 ) ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S16P;
	codecContext->compression_level = 2;
	return true;
}
