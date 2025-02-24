#include "EncoderFFmpeg.h"

#include "Utility.h"
#include <stdexcept>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include <libswresample/swresample.h>
}

EncoderFFmpeg::EncoderFFmpeg() : Encoder()
{
}

EncoderFFmpeg::~EncoderFFmpeg()
{
	FreeContexts();
}

// Minimum/maximum/default bit rates.
constexpr int kMinimumFFmpegBitrate = 64;
constexpr int kMaximumFFmpegBitrate = 320;
constexpr int kDefaultFFmpegBitrate = 128;

int EncoderFFmpeg::GetBitrate( const std::string& settings )
{
	int bitrate = kDefaultFFmpegBitrate;
	if ( !settings.empty() ) {
		try {
			bitrate = std::stoi( settings );
			LimitBitrate( bitrate );
		} catch ( const std::logic_error& ) {
		}
	}
	return bitrate;
}

int EncoderFFmpeg::GetDefaultBitrate()
{
	return kDefaultFFmpegBitrate;
}

int EncoderFFmpeg::GetMinimumBitrate()
{
	return kMinimumFFmpegBitrate;
}

int EncoderFFmpeg::GetMaximumBitrate()
{
	return kMaximumFFmpegBitrate;
}

void EncoderFFmpeg::LimitBitrate( int& bitrate )
{
	if ( bitrate < kMinimumFFmpegBitrate ) {
		bitrate = kMinimumFFmpegBitrate;
	} else if ( bitrate > kMaximumFFmpegBitrate ) {
		bitrate = kMaximumFFmpegBitrate;
	}
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

bool EncoderFFmpeg::Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> /*bitsPerSample*/, const long long /*totalSamples*/, const std::string& settings, const Tags& /*tags*/ )
{
	filename += L".m4a";
	const auto u8Filename = WideStringToUTF8( filename );
	if ( AVIOContext* ioContext = nullptr; avio_open( &ioContext, u8Filename.c_str(), AVIO_FLAG_WRITE ) >= 0 ) {
		if ( m_avformatContext = avformat_alloc_context(); nullptr != m_avformatContext ) {
			m_avformatContext->pb = ioContext;
			if ( m_avformatContext->oformat = av_guess_format( nullptr, u8Filename.c_str(), nullptr ); nullptr != m_avformatContext->oformat ) {
				if ( m_avformatContext->url = av_strdup( u8Filename.c_str() ); nullptr != m_avformatContext->url ) {
					if ( const AVCodec* avCodec = avcodec_find_encoder( AV_CODEC_ID_AAC ); nullptr != avCodec ) {
						if ( AVStream* avStream = avformat_new_stream( m_avformatContext, nullptr ); nullptr != avStream ) {
							if ( m_avcodecContext = avcodec_alloc_context3( avCodec ); nullptr != m_avcodecContext ) {
								m_InputSampleRate = static_cast<int>( sampleRate );

								m_OutputSampleRate = 0;
								int numSupportedSampleRates = 0;
								const int* pSupportedSampleRates = nullptr;
								avcodec_get_supported_config( m_avcodecContext, avCodec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, reinterpret_cast<const void**>( &pSupportedSampleRates ), &numSupportedSampleRates );
								std::set<int> supportedSampleRates;
								supportedSampleRates.insert( pSupportedSampleRates, pSupportedSampleRates + numSupportedSampleRates );
								for ( const auto supportedSampleRate : supportedSampleRates ) {
									if ( supportedSampleRate >= m_InputSampleRate ) {
										m_OutputSampleRate = supportedSampleRate;
										break;
									}
								}
								if ( ( 0 == m_OutputSampleRate ) && !supportedSampleRates.empty() ) {
									m_OutputSampleRate = *supportedSampleRates.rbegin();
								}

								if ( m_OutputSampleRate > 0 ) {
									m_OutputChannels = std::min( static_cast<int>( m_SampleBuffers.size() ), static_cast<int>( channels ) );
									m_avcodecContext->ch_layout = GetChannelLayout( m_OutputChannels );
									m_avcodecContext->sample_rate = static_cast<int>( m_OutputSampleRate );
									m_avcodecContext->bit_rate = 1000 * GetBitrate( settings );
									m_avcodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;

									m_pts = 0;
									avStream->time_base.den = m_avcodecContext->sample_rate;
									avStream->time_base.num = 1;

									if ( m_avformatContext->oformat->flags & AVFMT_GLOBALHEADER ) {
										m_avformatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
									}

									if ( avcodec_open2( m_avcodecContext, avCodec, nullptr ) >= 0 ) {
										if ( avcodec_parameters_from_context( avStream->codecpar, m_avcodecContext ) >= 0 ) {
											if ( avformat_write_header( m_avformatContext, nullptr ) >= 0 ) {
												const AVChannelLayout srcLayout = GetChannelLayout( channels );
												const AVChannelLayout dstLayout = m_avcodecContext->ch_layout;
												constexpr AVSampleFormat srcSampleFormat = AV_SAMPLE_FMT_FLT;
												const AVSampleFormat dstSampleFormat = m_avcodecContext->sample_fmt;
												if ( swr_alloc_set_opts2( &m_swrContext, &dstLayout, dstSampleFormat, m_avcodecContext->sample_rate, &srcLayout, srcSampleFormat, m_InputSampleRate, 0, nullptr ) >= 0 ) {
													av_opt_set( m_swrContext, "filter_type", "kaiser", 0 );
													if ( swr_init( m_swrContext ) < 0 ) {
														swr_free( &m_swrContext );
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if ( ( nullptr == m_avformatContext ) || ( nullptr == m_avcodecContext ) || ( nullptr == m_swrContext ) ) {
		FreeContexts();
		return false;
	}
	return true;
}

void EncoderFFmpeg::FreeContexts()
{
	if ( nullptr != m_avcodecContext ) {
		avcodec_free_context( &m_avcodecContext );
	}
	if ( nullptr != m_avformatContext ) {
		if ( nullptr != m_avformatContext->pb ) {
			avio_closep( &m_avformatContext->pb );
		}
		avformat_free_context( m_avformatContext );
		m_avformatContext = nullptr;
	}
	if ( nullptr != m_swrContext ) {
		swr_free( &m_swrContext );
	}
}

bool EncoderFFmpeg::Write( float* inputSamples, const long inputSampleCount )
{
	// Resample audio input.
	const uint32_t outputSampleCount = static_cast<uint32_t>( static_cast<int64_t>( inputSampleCount ) * m_OutputSampleRate / m_InputSampleRate );
	uint64_t bufferSamples = m_SampleBuffers[ 0 ].size();
	std::vector<uint8_t*> outputBuffers( m_OutputChannels );
	if ( outputSampleCount > 0 ) {
		for ( int channel = 0; channel < m_OutputChannels; channel++ ) {
			m_SampleBuffers[ channel ].resize( bufferSamples + outputSampleCount );
			outputBuffers[ channel ] = reinterpret_cast<uint8_t*>( m_SampleBuffers[ channel ].data() + bufferSamples );
		}
	}
	if ( const uint32_t samplesConverted = Resample( inputSamples, inputSampleCount, outputBuffers.data(), outputSampleCount ); samplesConverted > 0 ) {
		for ( int channel = 0; channel < m_OutputChannels; channel++ ) {
			m_SampleBuffers[ channel ].resize( bufferSamples + samplesConverted );
		}
		bufferSamples = m_SampleBuffers[ 0 ].size();
	}

	// Encode audio frames.
	bool success = true;
	while ( success && ( bufferSamples >= m_avcodecContext->frame_size ) ) {
		success = EncodeFrame();
		bufferSamples = m_SampleBuffers[ 0 ].size();
	}
	return success;
}

void EncoderFFmpeg::Close()
{
	// Flush the resampler.
	if ( const int sampleCount = swr_get_out_samples( m_swrContext, 0 ); sampleCount > 0 ) {
		uint64_t bufferSamples = m_SampleBuffers[ 0 ].size();
		std::vector<uint8_t*> outputBuffers( m_OutputChannels );
		for ( int channel = 0; channel < m_OutputChannels; channel++ ) {
			m_SampleBuffers[ channel ].resize( bufferSamples + sampleCount );
			outputBuffers[ channel ] = reinterpret_cast<uint8_t*>( m_SampleBuffers[ channel ].data() + bufferSamples );
		}
		if ( const uint32_t samplesConverted = Resample( nullptr, 0, outputBuffers.data(), sampleCount ); samplesConverted > 0 ) {
			for ( int channel = 0; channel < m_OutputChannels; channel++ ) {
				m_SampleBuffers[ channel ].resize( bufferSamples + samplesConverted );
			}
		}
	}

	// Encode any remaining audio frames.
	uint64_t bufferSamples = m_SampleBuffers[ 0 ].size();
	bool success = true;
	while ( success && ( bufferSamples > 0 ) ) {
		success = EncodeFrame();
		bufferSamples = m_SampleBuffers[ 0 ].size();
	}

	// Flush the encoder and close the output file.
	avcodec_send_frame( m_avcodecContext, nullptr );
	av_write_trailer( m_avformatContext );

	FreeContexts();
}

bool EncoderFFmpeg::EncodeFrame()
{
	bool success = false;
	if ( AVFrame* frame = av_frame_alloc(); nullptr != frame ) {
		frame->nb_samples = std::min( m_avcodecContext->frame_size, static_cast<int>( m_SampleBuffers[ 0 ].size() ) );
		frame->ch_layout = m_avcodecContext->ch_layout;
		frame->format = m_avcodecContext->sample_fmt;
		frame->sample_rate = m_avcodecContext->sample_rate;
		if ( av_frame_get_buffer( frame, 0 ) >= 0 ) {
			for ( int channel = 0; channel < m_OutputChannels; channel++ ) {
				std::copy( m_SampleBuffers[ channel ].begin(), m_SampleBuffers[ channel ].begin() + frame->nb_samples, reinterpret_cast<float*>( frame->data[ channel ] ) );
				m_SampleBuffers[ channel ].erase( m_SampleBuffers[ channel ].begin(), m_SampleBuffers[ channel ].begin() + frame->nb_samples );
			}
			if ( AVPacket* packet = av_packet_alloc(); nullptr != packet ) {
				frame->pts = m_pts;
				m_pts += frame->nb_samples;
				if ( avcodec_send_frame( m_avcodecContext, frame ) >= 0 ) {
					if ( avcodec_receive_packet( m_avcodecContext, packet ) >= 0 ) {
						av_write_frame( m_avformatContext, packet );
					}
					success = true;
				}
				av_packet_free( &packet );
			}
		}
		av_frame_free( &frame );
	}
	return success;
}

uint32_t EncoderFFmpeg::Resample( const float* const inputBuffer, const uint32_t inputSamples, uint8_t** outputBuffers, const uint32_t outputSamples )
{
	const uint8_t* const inBuffer = reinterpret_cast<const uint8_t* const>( inputBuffer );
	const int samplesConverted = swr_convert( m_swrContext, outputBuffers, static_cast<int>( outputSamples ), &inBuffer, static_cast<int>( inputSamples ) );
	return ( samplesConverted < 0 ) ? 0 : static_cast<uint32_t>( samplesConverted );
}
