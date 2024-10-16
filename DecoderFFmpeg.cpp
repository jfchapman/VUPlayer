#include "DecoderFFmpeg.h"

#include "Utility.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

DecoderFFmpeg::DecoderFFmpeg( const std::wstring& filename, const Context context ) :
	Decoder( context )
{
	const std::string utf8Filename = WideStringToUTF8( filename );

	m_FormatContext = avformat_alloc_context();
	if ( nullptr != m_FormatContext ) {
		if ( 0 == avformat_open_input( &m_FormatContext, utf8Filename.data(), nullptr, nullptr ) ) {
			if ( avformat_find_stream_info( m_FormatContext, nullptr ) >= 0 ) {
				const AVCodec* codec = nullptr;
				m_StreamIndex = av_find_best_stream( m_FormatContext, AVMediaType::AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0 );
				if ( ( m_StreamIndex >= 0 ) && ( nullptr != codec ) ) {
					if ( AVStream* stream = m_FormatContext->streams[ m_StreamIndex ]; nullptr != stream ) {
						if ( AVCodecParameters* codecParams = stream->codecpar; nullptr != codecParams ) {
							if ( codecParams->ch_layout.nb_channels > 0 ) {
								SetChannels( codecParams->ch_layout.nb_channels );
								SetSampleRate( codecParams->sample_rate );
								if ( codecParams->bits_per_coded_sample > 0 ) {
									SetBPS( codecParams->bits_per_coded_sample );
								}
								if ( codecParams->bit_rate > 0 ) {
									SetBitrate( codecParams->bit_rate / 1000.0f );
								}
								if ( stream->duration > 0 ) {
									SetDuration( static_cast<float>( stream->duration * av_q2d( stream->time_base ) ) );
								} else {
									SetDuration( static_cast<float>( m_FormatContext->duration ) / AV_TIME_BASE );
								}

								m_DecoderContext = avcodec_alloc_context3( codec );
								if ( nullptr != m_DecoderContext ) {
									int result = avcodec_parameters_to_context( m_DecoderContext, codecParams );
									if ( result >= 0 ) {
										result = avcodec_open2( m_DecoderContext, codec, nullptr );
									}
									if ( result >= 0 ) {
										m_Packet = av_packet_alloc();
										m_Frame = av_frame_alloc();

                    const auto srcLayout = m_DecoderContext->ch_layout;
                    const auto srcFormat = m_DecoderContext->sample_fmt;
                    const AVChannelLayout dstLayout = srcLayout;
                    const AVSampleFormat dstFormat = AV_SAMPLE_FMT_FLT;
                    result = swr_alloc_set_opts2( &m_ResamplerContext, &dstLayout, dstFormat, codecParams->sample_rate, &srcLayout, srcFormat, codecParams->sample_rate, 0, nullptr );
                    if ( result >= 0 ) {
                      if ( swr_init( m_ResamplerContext ) < 0 ) {
                        swr_free( &m_ResamplerContext );
                        m_ResamplerContext = nullptr;
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

	if ( ( nullptr == m_FormatContext ) || ( nullptr == m_DecoderContext ) || ( nullptr == m_Packet ) || ( nullptr == m_Frame ) || ( nullptr == m_ResamplerContext ) ) {
		if ( nullptr != m_Packet ) {
			av_packet_free( &m_Packet );
		}
		if ( nullptr != m_Frame ) {
			av_frame_free( &m_Frame );
		}
		if ( nullptr != m_DecoderContext ) {
			avcodec_free_context( &m_DecoderContext );
		}
		if ( nullptr != m_FormatContext ) {
			avformat_close_input( &m_FormatContext );
		}
		if ( nullptr != m_ResamplerContext ) {
			swr_free( &m_ResamplerContext );
		}
		throw std::runtime_error( "DecoderFFmpeg could not load file" );
	}
}

DecoderFFmpeg::~DecoderFFmpeg()
{
	if ( nullptr != m_Packet ) {
		av_packet_free( &m_Packet );
	}
	av_frame_free( &m_Frame );
	avcodec_free_context( &m_DecoderContext );
	avformat_close_input( &m_FormatContext );
  swr_free( &m_ResamplerContext );
}

void DecoderFFmpeg::ConvertSampleData( const AVFrame* frame )
{
  if ( nullptr == frame )
    return;

  const size_t previousBufferSize = m_Buffer.size();
  m_Buffer.resize( previousBufferSize + frame->nb_samples * GetChannels() );
  uint8_t* buffer = reinterpret_cast<uint8_t*>( m_Buffer.data() + previousBufferSize );
  const int samples = swr_convert( m_ResamplerContext, &buffer, frame->nb_samples, frame->data, frame->nb_samples );
  if ( samples > 0 ) {
    m_Buffer.resize( previousBufferSize + samples * GetChannels() );
  } else {
    m_Buffer.resize( previousBufferSize );
  }
}

bool DecoderFFmpeg::Decode()
{
	m_Buffer.clear();
	m_BufferPosition = 0;
	while ( ( nullptr != m_Packet ) && m_Buffer.empty() ) {
		if ( av_read_frame( m_FormatContext, m_Packet ) < 0 ) {
			// Flush the decoder.
			av_packet_free( &m_Packet );
			m_Packet = nullptr;
		}
		if ( ( nullptr == m_Packet ) || ( m_StreamIndex == m_Packet->stream_index ) ) {
			int result = avcodec_send_packet( m_DecoderContext, m_Packet );
			while ( result >= 0 ) {
				result = avcodec_receive_frame( m_DecoderContext, m_Frame );
				if ( result >= 0 ) {
					ConvertSampleData( m_Frame );
					av_frame_unref( m_Frame );
				}
			}
		}
		if ( nullptr != m_Packet ) {
			av_packet_unref( m_Packet );
		}
	}
	return !m_Buffer.empty();
}

long DecoderFFmpeg::Read( float* output, const long sampleCount )
{
	long samplesRead = 0;
	while ( samplesRead < sampleCount ) {
		if ( m_BufferPosition < m_Buffer.size() ) {
			const long bufferSamples = GetChannels() * std::min<long>( static_cast<long>( m_Buffer.size() - m_BufferPosition ) / GetChannels(), sampleCount - samplesRead );
			std::copy( m_Buffer.begin() + m_BufferPosition, m_Buffer.begin() + m_BufferPosition + bufferSamples, output );
			m_BufferPosition += bufferSamples;
			output += bufferSamples;
			samplesRead += bufferSamples / GetChannels();
		} else if ( !Decode() ) {
			break;
		}
	}
	return samplesRead;
}

double DecoderFFmpeg::Seek( const double position )
{
	m_Buffer.clear();
	m_BufferPosition = 0;
	const int64_t ts( position * AV_TIME_BASE );
	return ( avformat_seek_file( m_FormatContext, -1, INT64_MIN, ts, ts, 0 ) < 0 ) ? 0 : position;
}
