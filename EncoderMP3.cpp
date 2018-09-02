#include "EncoderMP3.h"

#include <vector>

void null_report_function( const char* /*format*/, va_list /*ap*/ )
{
	return;
}

EncoderMP3::EncoderMP3() :
	Encoder(),
	m_flags( nullptr ),
	m_file( nullptr )
{
}

EncoderMP3::~EncoderMP3()
{
}

bool EncoderMP3::Open( std::wstring& filename, const long sampleRate, const long channels, const long /*bitsPerSample*/, const std::string& settings )
{
	bool success = false;

	m_flags = lame_init();
	if ( nullptr != m_flags ) {
		lame_set_errorf( m_flags, null_report_function );
		lame_set_debugf( m_flags, null_report_function );
		lame_set_msgf( m_flags, null_report_function );

		lame_set_VBR( m_flags, vbr_default );
		lame_set_VBR_quality( m_flags, static_cast<float>( GetVBRQuality( settings ) ) );

		lame_set_bWriteVbrTag( m_flags, 1 );

		success = ( 0 == lame_set_num_channels( m_flags, static_cast<int>( channels ) ) );
			( 0 == lame_set_in_samplerate( m_flags, static_cast<int>( sampleRate ) ) ) &&
			( 0 == lame_init_params( m_flags ) );

		if ( success ) {
			filename += L".mp3";
			success = ( 0 == _wfopen_s( &m_file, filename.c_str(), L"w+b" ) );
		} else {
			lame_close( m_flags );
			m_flags = nullptr;
		}
	}
	return success;
}

bool EncoderMP3::Write( float* samples, const long sampleCount )
{
	const int outputBufferSize = sampleCount * 2;
	std::vector<unsigned char> outputBuffer( outputBufferSize );
	const int bytesEncoded = lame_encode_buffer_interleaved_ieee_float( m_flags, samples, sampleCount, &outputBuffer[ 0 ], outputBufferSize );
	const bool success = ( bytesEncoded > 0 ) && ( nullptr != m_file ) && ( static_cast<size_t>( bytesEncoded ) == fwrite( &outputBuffer[ 0 ], 1 /*elementSize*/, bytesEncoded, m_file ) );
	return success;
}

void EncoderMP3::Close()
{
	if ( nullptr != m_flags ) {
		const int outputBufferSize = 65536;
		std::vector<unsigned char> outputBuffer( outputBufferSize );
		const int bytesEncoded = lame_encode_flush( m_flags, &outputBuffer[ 0 ], outputBufferSize );
		if ( ( bytesEncoded > 0 ) && ( nullptr != m_file ) ) {
			fwrite( &outputBuffer[ 0 ], 1 /*elementSize*/, bytesEncoded, m_file );
		}

		lame_mp3_tags_fid( m_flags, m_file );

		lame_close( m_flags );
		m_flags = nullptr;
	}
	if ( nullptr != m_file ) {
		fclose( m_file );
		m_file = nullptr;
	}
}

int EncoderMP3::GetVBRQuality( const std::string& settings )
{
	int quality = 2;
	try {
		quality = std::stoi( settings );
		if ( ( quality < 0 ) || ( quality > 9 ) ) {
			quality = 2;
		}
	} catch ( ... ) {

	}
	return quality;
}
