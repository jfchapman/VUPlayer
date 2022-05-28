#include "DecoderFFmpeg.h"

#include "Utility.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

DecoderFFmpeg::DecoderFFmpeg( const std::wstring& filename ) :
	Decoder()
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
							if ( codecParams->channels > 0 ) {
								SetChannels( codecParams->channels );
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
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ( ( nullptr == m_FormatContext ) || ( nullptr == m_DecoderContext ) || ( nullptr == m_Packet ) || ( nullptr == m_Frame ) ) {
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
}

void DecoderFFmpeg::ConvertSampleData( const AVFrame* frame, std::vector<float>& buffer )
{
	if ( nullptr != frame ) {
		switch ( frame->format ) {
			// Non-planar sample formats
			case AV_SAMPLE_FMT_U8 : {
				uint8_t* data = frame->data[ 0 ];
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( Unsigned8ToFloat( data[ pos ] ) );
				}
				break;
			}
			case AV_SAMPLE_FMT_S16 : {
				int16_t* data = reinterpret_cast<int16_t*>( frame->data[ 0 ] );
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( Signed16ToFloat( data[ pos ] ) );
				}
				break;
			}
			case AV_SAMPLE_FMT_S32 : {
				int32_t* data = reinterpret_cast<int32_t*>( frame->data[ 0 ] );
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( Signed32ToFloat( data[ pos ] ) );
				}
				break;
			}
			case AV_SAMPLE_FMT_S64 : {
				int64_t* data = reinterpret_cast<int64_t*>( frame->data[ 0 ] );
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( Signed64ToFloat( data[ pos ] ) );
				}
				break;
			}
			case AV_SAMPLE_FMT_FLT : {
				float* data = reinterpret_cast<float*>( frame->data[ 0 ] );
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( data[ pos ] );
				}
				break;
			}
			case AV_SAMPLE_FMT_DBL : {
				double* data = reinterpret_cast<double*>( frame->data[ 0 ] );
				for ( int pos = 0; pos < frame->channels * frame->nb_samples; pos++ ) {
					buffer.push_back( static_cast<float>( data[ pos ] ) );
				}
				break;
			}

			// Planar sample formats
			case AV_SAMPLE_FMT_U8P : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						uint8_t* data = frame->data[ channel ];
						buffer.push_back( Unsigned8ToFloat( data[ samplePos ] ) );
					}					
				}
				break;
			}
			case AV_SAMPLE_FMT_S16P : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						int16_t* data = reinterpret_cast<int16_t*>( frame->data[ channel ] );
						buffer.push_back( Signed16ToFloat( data[ samplePos ] ) );
					}					
				}
				break;
			}
			case AV_SAMPLE_FMT_S32P : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						int32_t* data = reinterpret_cast<int32_t*>( frame->data[ channel ] );
						buffer.push_back( Signed32ToFloat( data[ samplePos ] ) );
					}					
				}
				break;
			}
			case AV_SAMPLE_FMT_S64P : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						int64_t* data = reinterpret_cast<int64_t*>( frame->data[ channel ] );
						buffer.push_back( Signed64ToFloat( data[ samplePos ] ) );
					}					
				}
				break;
			}
			case AV_SAMPLE_FMT_FLTP : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						float* data = reinterpret_cast<float*>( frame->data[ channel ] );
						buffer.push_back( data[ samplePos ] );
					}					
				}
				break;
			}
			case AV_SAMPLE_FMT_DBLP : {
				for ( int samplePos = 0; samplePos < frame->nb_samples; samplePos++ ) {
					for ( int channel = 0; channel < frame->channels; channel++ ) {
						double* data = reinterpret_cast<double*>( frame->data[ channel ] );
						buffer.push_back( static_cast<float>( data[ samplePos ] ) );
					}					
				}
				break;
			}
		}
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
					ConvertSampleData( m_Frame, m_Buffer );
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

float DecoderFFmpeg::Seek( const float position )
{
	m_Buffer.clear();
	m_BufferPosition = 0;
	const int64_t ts( position * AV_TIME_BASE );
	return ( avformat_seek_file( m_FormatContext, -1, INT64_MIN, ts, ts, 0 ) < 0 ) ? 0 : position;
}
