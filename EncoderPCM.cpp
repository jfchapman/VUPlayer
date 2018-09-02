#include "EncoderPCM.h"

#include "Utility.h"

#include <mmreg.h>

#include <vector>

EncoderPCM::EncoderPCM() :
	Encoder(),
	m_header( {} ),
	m_file( nullptr )
{
}

EncoderPCM::~EncoderPCM()
{
}

bool EncoderPCM::Open( std::wstring& filename, const long sampleRate, const long channels, const long bitsPerSample, const std::string& /*settings*/ )
{
	bool success = false;
	if ( ( 1 == channels ) || ( 2 == channels ) ) {
		memcpy( m_header.hdrRIFF, "RIFF" ,4 );
		memcpy( m_header.hdrFMT, "fmt ", 4 );
		memcpy( m_header.hdrDATA, "data", 4 );
		memcpy( m_header.hdrWAVE, "WAVE", 4 );
		m_header.nFmtLength = 16;

		m_header.wFormatTag = WAVE_FORMAT_PCM;
		m_header.nChannels = static_cast<WORD>( channels );
		m_header.nSamplesPerSec = static_cast<DWORD>( sampleRate );
		m_header.wBitsPerSample = ( 8 == bitsPerSample ) ? 8 : 16;
		m_header.nBlockAlign = m_header.nChannels * m_header.wBitsPerSample;
		m_header.nAvgBytesPerSec = m_header.nSamplesPerSec * m_header.nBlockAlign;
		filename += L".wav";
		if ( 0 == _wfopen_s( &m_file, filename.c_str(), L"wb" ) ) {
			const int headerSize = sizeof( WaveFileHeader );
			success = ( 1 == fwrite( &m_header, sizeof( WaveFileHeader ), 1, m_file ) );
			if ( !success ) {
				fclose( m_file );
				m_file = nullptr;
			}
		}
	}
	return success;
}

bool EncoderPCM::Write( float* samples, const long sampleCount )
{
	bool success = false;
	const long outputBufferSize = m_header.nChannels * sampleCount;
	if ( 8 == m_header.wBitsPerSample ) {
		std::vector<char> outputBuffer( outputBufferSize );
		for ( long sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
			outputBuffer[ sampleIndex ] = FloatTo8( samples[ sampleIndex ] );
		}
		success = ( static_cast<size_t>( outputBufferSize ) == fwrite( &outputBuffer[ 0 ], 1 /*elementSize*/, outputBufferSize, m_file ) );
	} else if ( 16 == m_header.wBitsPerSample ) {
		std::vector<short> outputBuffer( outputBufferSize );
		for ( long sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
			outputBuffer[ sampleIndex ] = FloatTo16( samples[ sampleIndex ] );
		}
		success = ( static_cast<size_t>( outputBufferSize ) == fwrite( &outputBuffer[ 0 ], 2 /*elementSize*/, outputBufferSize, m_file ) );
	}
	return success;
}

void EncoderPCM::Close()
{
	if ( nullptr != m_file ) {
		const long bytesWritten = ftell( m_file );
		m_header.dwTotalLength = bytesWritten - 8;
		m_header.nDataLength = bytesWritten - sizeof( WaveFileHeader );

		if ( 0 == fseek( m_file, 0, SEEK_SET ) ) {
			fwrite( &m_header, sizeof( WaveFileHeader ), 1, m_file );
		}

		fclose( m_file );
		m_file = nullptr;
	}
}
