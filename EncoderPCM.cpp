#include "EncoderPCM.h"

#include "Utility.h"

#include <mmreg.h>

#include <vector>

// Maximum number of bytes that can be written.
static const long long s_MaxBytes = UINT_MAX;

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
		m_header.nBlockAlign = m_header.nChannels * m_header.wBitsPerSample / 8;
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
	const long long filePosition = _ftelli64( m_file );
	if ( 8 == m_header.wBitsPerSample ) {
		std::vector<unsigned char> outputBuffer( outputBufferSize );
		for ( long sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
			outputBuffer[ sampleIndex ] = FloatToUnsigned8( samples[ sampleIndex ] );
		}

		size_t elementCount = static_cast<size_t>( outputBufferSize );
		if ( ( filePosition + outputBufferSize ) > s_MaxBytes ) {
			if ( filePosition < s_MaxBytes ) {
				elementCount = static_cast<size_t>( s_MaxBytes - filePosition );
				elementCount -= ( elementCount % m_header.nBlockAlign );
			} else {
				elementCount = 0;
			}
		}

		success = ( static_cast<size_t>( outputBufferSize ) == fwrite( &outputBuffer[ 0 ], 1 /*elementSize*/, elementCount, m_file ) );
	} else if ( 16 == m_header.wBitsPerSample ) {
		std::vector<short> outputBuffer( outputBufferSize );
		for ( long sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
			outputBuffer[ sampleIndex ] = FloatTo16( samples[ sampleIndex ] );
		}

		size_t elementCount = static_cast<size_t>( outputBufferSize );
		if ( ( filePosition + ( outputBufferSize * 2 ) ) > s_MaxBytes ) {
			if ( filePosition < s_MaxBytes ) {
				elementCount = static_cast<size_t>( s_MaxBytes - filePosition );
				elementCount -= ( elementCount % m_header.nBlockAlign );
				elementCount /= 2;
			} else {
				elementCount = 0;
			}
		}

		success = ( static_cast<size_t>( outputBufferSize ) == fwrite( &outputBuffer[ 0 ], 2 /*elementSize*/, elementCount, m_file ) );
	}
	return success;
}

void EncoderPCM::Close()
{
	if ( nullptr != m_file ) {
		const long long bytesWritten = _ftelli64( m_file );
		m_header.dwTotalLength = static_cast<DWORD>( bytesWritten - 8 );
		m_header.nDataLength = static_cast<DWORD>( bytesWritten - sizeof( WaveFileHeader ) );

		if ( 0 == fseek( m_file, 0, SEEK_SET ) ) {
			fwrite( &m_header, sizeof( WaveFileHeader ), 1, m_file );
		}

		fclose( m_file );
		m_file = nullptr;
	}
}
