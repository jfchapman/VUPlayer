#include "DecoderMPC.h"

#include "Utility.h"

DecoderMPC::DecoderMPC( const std::wstring& filename, const Context context ) :
	Decoder( context ),
	m_file( nullptr ),
	m_reader(),
	m_demux(),
	m_buffer( MPC_DECODER_BUFFER_LENGTH ),
	m_buffercount( 0 ),
	m_bufferpos( 0 ),
	m_eos( false )
{
	if ( 0 == _wfopen_s( &m_file, filename.c_str(), L"rb" ) && ( nullptr != m_file ) ) {
		if ( MPC_STATUS_OK == mpc_reader_init_stdio_stream( &m_reader, m_file ) ) {
			m_demux = mpc_demux_init( &m_reader );
			if ( nullptr != m_demux ) {
				mpc_streaminfo info = {};
				mpc_demux_get_info( m_demux, &info );
				SetChannels( static_cast<long>( info.channels ) );
				SetSampleRate( static_cast<long>( info.sample_freq ) );
				SetBitrate( static_cast<float>( info.average_bitrate ) / 1000 );
				SetDuration( static_cast<float>( mpc_streaminfo_get_length( &info ) ) );
				if ( ( 0 == GetChannels() ) || ( 0 == GetSampleRate() ) || ( GetDuration() <= 0 ) ) {
					mpc_demux_exit( m_demux );
					m_demux = nullptr;
				}
			}
			if ( nullptr == m_demux ) {
				mpc_reader_exit_stdio( &m_reader );
			}
		}
	}

	if ( nullptr == m_demux ) {
		if ( nullptr != m_file ) {
			fclose( m_file );
		}
		throw std::runtime_error( "DecoderMPC could not load file" );
	}
}

DecoderMPC::~DecoderMPC()
{
	mpc_demux_exit( m_demux );
	mpc_reader_exit_stdio( &m_reader );
	fclose( m_file );
}

long DecoderMPC::Read( float* destBuffer, const long sampleCount )
{
	long samplesRead = 0;
	const long channels = GetChannels();
	while ( !m_eos && ( samplesRead < sampleCount ) ) {
		if ( m_bufferpos < m_buffercount ) {
			for ( long channel = 0; channel < channels; channel++ ) {
				*destBuffer++ = m_buffer[ m_bufferpos++ ];
			}
			++samplesRead;
		} else {
			m_bufferpos = 0;
			m_buffercount = 0;
			mpc_frame_info frame = {};
			frame.buffer = m_buffer.data();
			if ( ( MPC_STATUS_OK == mpc_demux_decode( m_demux, &frame ) ) && ( -1 != frame.bits ) ) {
				m_buffercount = frame.samples * channels;
			}
			m_eos = ( 0 == m_buffercount );
		}
	}
	return samplesRead;
}

float DecoderMPC::Seek( const float position )
{
	m_bufferpos = 0;
	m_buffercount = 0;
	if ( MPC_STATUS_OK == mpc_demux_seek_second( m_demux, position ) ) {
		mpc_frame_info frame = {};
		frame.buffer = m_buffer.data();
		do {
			if ( MPC_STATUS_OK != mpc_demux_decode( m_demux, &frame ) ) {
				break;
			}
		} while ( 0 == frame.samples );
		m_buffercount = frame.samples * GetChannels();
	}
	return position;
}
