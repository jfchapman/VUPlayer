#include "Resampler.h"

#include <map>
#include <stdexcept>

extern "C"
{
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

static AVChannelLayout GetChannelLayout( const uint32_t channels )
{
	const std::map<uint32_t, AVChannelLayout> kChannelLayouts =
	{
		{ 1, AV_CHANNEL_LAYOUT_MONO },
		{ 2, AV_CHANNEL_LAYOUT_STEREO },
		{ 3, AV_CHANNEL_LAYOUT_SURROUND },
		{ 4, AV_CHANNEL_LAYOUT_QUAD },
		{ 5, AV_CHANNEL_LAYOUT_5POINT0 },
		{ 6, AV_CHANNEL_LAYOUT_5POINT1 },
		{ 7, AV_CHANNEL_LAYOUT_6POINT1 },
		{ 8, AV_CHANNEL_LAYOUT_7POINT1 },
		{ 10, AV_CHANNEL_LAYOUT_5POINT1POINT4_BACK },
		{ 12, AV_CHANNEL_LAYOUT_7POINT1POINT4_BACK },
		{ 14, AV_CHANNEL_LAYOUT_9POINT1POINT4_BACK },
	};
	if ( const auto layout = kChannelLayouts.find( channels ); kChannelLayouts.end() != layout )
		return layout->second;
	return {};
}

Resampler::Resampler( Decoder::Ptr decoder, const long id, const uint32_t outputRate, const uint32_t outputChannels ) :
	OutputDecoder( decoder, id ),
	m_InputSampleRate( static_cast<uint32_t>( decoder->GetSampleRate() ) ),
	m_InputChannels( GetDecoderChannels( decoder ) ),
	m_OutputSampleRate( outputRate ),
	m_OutputChannels( outputChannels )
{
	const AVChannelLayout srcLayout = GetChannelLayout( m_InputChannels );
	const AVChannelLayout dstLayout = GetChannelLayout( m_OutputChannels );
	constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_FLT;
	int result = swr_alloc_set_opts2( &m_Context, &dstLayout, kSampleFormat, static_cast<int>( m_OutputSampleRate ), &srcLayout, kSampleFormat, static_cast<int>( m_InputSampleRate ), 0, nullptr );
	if ( 0 == result ) {
		result = av_opt_set( m_Context, "filter_type", "kaiser", 0 );
		result = swr_init( m_Context );
		if ( result < 0 )
			swr_free( &m_Context );
	}
	if ( nullptr == m_Context ) {
		throw std::runtime_error( "Resampler could not be initialised" );
	}
}

Resampler::~Resampler()
{
	if ( nullptr != m_Context ) {
		swr_free( &m_Context );
	}
}

long Resampler::Read( float* outputBuffer, const long outputSampleCount )
{
	long inputSampleCount = std::max( 1l, static_cast<long>( outputSampleCount * static_cast<float>( m_InputSampleRate ) / m_OutputSampleRate ) );
	m_InputBuffer.resize( inputSampleCount * m_InputChannels );

	long samplesDecoded = OutputDecoder::Read( m_InputBuffer.data(), inputSampleCount );
	long samplesConverted = static_cast<long>( Convert( m_InputBuffer.data(), static_cast<uint32_t>( samplesDecoded ), outputBuffer, static_cast<uint32_t>( outputSampleCount ) ) );

	// The resampler has an internal buffer, so keep feeding it data until it provides some output.
	constexpr long kBufferSizeLimit = 65536;
	while ( ( samplesConverted <= 0 ) && ( samplesDecoded > 0 ) && ( inputSampleCount <= kBufferSizeLimit ) ) {
		inputSampleCount *= 2;
		m_InputBuffer.resize( inputSampleCount * m_InputChannels );
		samplesDecoded = OutputDecoder::Read( m_InputBuffer.data(), inputSampleCount );
		samplesConverted = static_cast<long>( Convert( m_InputBuffer.data(), static_cast<uint32_t>( samplesDecoded ), outputBuffer, static_cast<uint32_t>( outputSampleCount ) ) );
	}
	return samplesConverted;
}

long Resampler::GetOutputSampleRate() const
{
	return static_cast<long>( m_OutputSampleRate );
}

long Resampler::GetOutputChannels() const
{
	return static_cast<long>( m_OutputChannels );
}

float Resampler::GetPreBufferSizeFactor() const
{
	return std::clamp( static_cast<float>( m_InputSampleRate ) / m_OutputSampleRate, 1.0f, 8.0f );
}

uint32_t Resampler::Convert( const float* const inputBuffer, const uint32_t inputSamples, float* outputBuffer, const uint32_t outputSamples )
{
	const uint8_t* const inBuffer = reinterpret_cast<const uint8_t* const>( inputBuffer );
	uint8_t* const outBuffer = reinterpret_cast<uint8_t* const>( outputBuffer );
	const int samplesConverted = swr_convert( m_Context, &outBuffer, static_cast<int>( outputSamples ), &inBuffer, static_cast<int>( inputSamples ) );
	return ( samplesConverted < 0 ) ? 0 : static_cast<uint32_t>( samplesConverted );
}
