#include "EncoderPCM.h"

#include "Utility.h"

#include <assert.h>

#include <array>
#include <vector>

// Size of the 'ds64' chunk (and placeholder 'JUNK' chunk).
constexpr DWORD kChunkSizeDS64 = 28;

EncoderPCM::EncoderPCM() :
	Encoder()
{
}

EncoderPCM::~EncoderPCM()
{
}

bool EncoderPCM::Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const long long /*totalSamples*/, const std::string& /*settings*/, const Tags& /*tags*/ )
{
	bool success = false;

	m_sampleRate = sampleRate;
	m_channels = channels;
	m_bitsPerSample = bitsPerSample.value_or( 16 );
	if ( m_bitsPerSample <= 8 ) {
		m_bitsPerSample = 8;
	} else if ( m_bitsPerSample > 16 ) {
		m_bitsPerSample = 24;
	} else {
		m_bitsPerSample = 16;
	}

	m_dataBytesWritten = 0;
	m_dataChunkSizeOffset = 0;
	m_junkChunkOffset = 0;
	m_wavFormat.reset();
	m_wavFormatExtensible.reset();

	if ( ( m_sampleRate > 0 ) && ( m_channels > 0 ) ) {
		constexpr DWORD kSizeExtensible = 22;

		const bool isExtensible = ( m_channels > 2 ) || ( m_bitsPerSample > 16 );
		if ( isExtensible ) {
			m_wavFormatExtensible = std::make_optional<WAVEFORMATEXTENSIBLE>( {} );
			m_wavFormatExtensible->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			m_wavFormatExtensible->Format.nChannels = static_cast<WORD>( m_channels );
			m_wavFormatExtensible->Format.cbSize = static_cast<WORD>( kSizeExtensible );
			m_wavFormatExtensible->Format.nSamplesPerSec = static_cast<DWORD>( m_sampleRate );
			m_wavFormatExtensible->Format.wBitsPerSample = static_cast<WORD>( m_bitsPerSample );
			m_wavFormatExtensible->Format.nBlockAlign = m_wavFormatExtensible->Format.nChannels * m_wavFormatExtensible->Format.wBitsPerSample / 8;
			m_wavFormatExtensible->Format.nAvgBytesPerSec = m_wavFormatExtensible->Format.nSamplesPerSec * m_wavFormatExtensible->Format.nBlockAlign;

			m_wavFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			m_wavFormatExtensible->Samples.wValidBitsPerSample = 24;

			switch ( m_channels ) {
				case 1 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_CENTER;
					break;
				}
				case 2 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
					break;
				}
				case 3 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER;
					break;
				}
				case 4 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
					break;
				}
				case 5 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
					break;
				}
				case 6 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
					break;
				}
				case 7 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
					break;
				}
				case 8 : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
					break;
				}
				default : {
					m_wavFormatExtensible->dwChannelMask = SPEAKER_ALL;
					break;
				}
			}
		} else {
			m_wavFormat = std::make_optional<WAVEFORMATEX>( {} );
			m_wavFormat->wFormatTag = WAVE_FORMAT_PCM;
			m_wavFormat->nChannels = static_cast<WORD>( m_channels );
			m_wavFormat->cbSize = 0;
			m_wavFormat->nSamplesPerSec = static_cast<DWORD>( m_sampleRate );
			m_wavFormat->wBitsPerSample = static_cast<WORD>( m_bitsPerSample );
			m_wavFormat->nBlockAlign = m_wavFormat->nChannels * m_wavFormat->wBitsPerSample / 8;
			m_wavFormat->nAvgBytesPerSec = m_wavFormat->nSamplesPerSec * m_wavFormat->nBlockAlign;
		}

		filename += L".wav";

		m_file = _wfsopen( filename.c_str(), L"wb", _SH_DENYRW );
		if ( nullptr != m_file ) {
			// 'RIFF' chunk
			const DWORD riffSize = 0;
			success = ( 4 == fwrite( "RIFF", 1, 4, m_file ) ) && ( 4 == fwrite( &riffSize, 1, 4, m_file ) ) && ( 4 == fwrite( "WAVE", 1, 4, m_file ) );

			// 'JUNK' chunk (to be replaced with a 'ds64' chunk on close, if required)
			if ( success ) {
				constexpr std::array<unsigned char, kChunkSizeDS64> junkChunk = {};
				m_junkChunkOffset = _ftelli64( m_file );
				success = ( 4 == fwrite( "JUNK", 1, 4, m_file ) ) && ( 4 == fwrite( &kChunkSizeDS64, 1, 4, m_file ) ) && ( 1 == fwrite( junkChunk.data(), kChunkSizeDS64, 1, m_file ) );
			}

			// 'fmt ' chunk
			if ( success ) {
				const DWORD fmtSize = m_wavFormatExtensible ? sizeof( WAVEFORMATEXTENSIBLE ) : sizeof( WAVEFORMATEX );
				success = ( 4 == fwrite( "fmt ", 1, 4, m_file ) ) && ( 4 == fwrite( &fmtSize, 1, 4, m_file ) );
				if ( success ) {
					success = m_wavFormatExtensible ? ( 1 == fwrite( &m_wavFormatExtensible.value(), fmtSize, 1, m_file ) ) : ( 1 == fwrite( &m_wavFormat.value(), fmtSize, 1, m_file ) );
				}
			}

			// 'data' chunk
			if ( success ) {
				const DWORD dataSize = 0;
				success = ( 4 == fwrite( "data", 1, 4, m_file ) ) && ( 4 == fwrite( &dataSize, 1, 4, m_file ) );
				if ( success ) {
					m_dataChunkSizeOffset = _ftelli64( m_file ) - 4;
				}
			}

			if ( !success ) {
				fclose( m_file );
				m_file = nullptr;
				_wremove( filename.c_str() );
			}
		}
	}
	return success;
}

bool EncoderPCM::Write( float* samples, const long sampleCount )
{
	bool success = false;
	const size_t outputBufferSize = static_cast<size_t>( m_channels * sampleCount );
	switch ( m_bitsPerSample ) {
		case 8 : {		
			m_buffer8.resize( outputBufferSize );
			for ( size_t sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
				m_buffer8[ sampleIndex ] = FloatToUnsigned8( samples[ sampleIndex ] );
			}
			success = ( outputBufferSize == fwrite( m_buffer8.data(), 1, outputBufferSize, m_file ) );
			if ( success ) {
				m_dataBytesWritten += outputBufferSize;
			}
			break;
		}
		case 16 : {
			m_buffer16.resize( outputBufferSize );
			for ( size_t sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
				m_buffer16[ sampleIndex ] = FloatTo16( samples[ sampleIndex ] );
			}
			success = ( outputBufferSize == fwrite( m_buffer16.data(), 2, outputBufferSize, m_file ) );
			if ( success ) {
				m_dataBytesWritten += 2 * outputBufferSize;
			}
			break;
		}
		case 24 : {
			m_buffer8.resize( 3 * outputBufferSize );
			for ( size_t sampleIndex = 0; sampleIndex < outputBufferSize; sampleIndex++ ) {
				const int32_t value = FloatTo24( samples[ sampleIndex ] );
				m_buffer8[ sampleIndex * 3 ] = value & 0xff;
				m_buffer8[ sampleIndex * 3 + 1 ] = ( value >> 8 ) & 0xff;
				m_buffer8[ sampleIndex * 3 + 2 ] = ( value >> 16 ) & 0xff;
			}
			success = ( outputBufferSize == fwrite( m_buffer8.data(), 3, outputBufferSize, m_file ) );
			if ( success ) {
				m_dataBytesWritten += 3 * outputBufferSize;
			}
			break;
		}
	}
	return success;
}

void EncoderPCM::Close()
{
	if ( nullptr != m_file ) {
		const long long riffSize = _ftelli64( m_file ) - 8;
		if ( riffSize < UINT32_MAX ) {
			// For file sizes less than 4GiB, use a regular wav file.
			if ( 0 == _fseeki64( m_file, 4, SEEK_SET ) ) {
				const DWORD riffSize32 = static_cast<DWORD>( riffSize );
				fwrite( &riffSize32, 4, 1, m_file );
			}
			if ( 0 == _fseeki64( m_file, m_dataChunkSizeOffset, SEEK_SET ) ) {
				const DWORD dataSize32 = static_cast<DWORD>( m_dataBytesWritten );
				fwrite( &dataSize32, 4, 1, m_file );
			}
		} else {
			// For file sizes greater than 4GiB, convert to RF64 format.
			if ( 0 == _fseeki64( m_file, 0, SEEK_SET ) ) {
				fwrite( "RF64", 1, 4, m_file );
				const DWORD riffSize32 = 0xffffffff;
				fwrite( &riffSize32, 4, 1, m_file );
			}
			if ( 0 == _fseeki64( m_file, m_dataChunkSizeOffset, SEEK_SET ) ) {
				const DWORD dataSize32 = 0xffffffff;
				fwrite( &dataSize32, 4, 1, m_file );
			}
			if ( 0 == _fseeki64( m_file, m_junkChunkOffset, SEEK_SET ) ) {	
				fwrite( "ds64", 1, 4, m_file );
				fwrite( &kChunkSizeDS64, 1, 4, m_file );

				struct ds64chunk {
					uint32_t riffSizeLow;
					uint32_t riffSizeHigh;
					uint32_t dataSizeLow;
					uint32_t dataSizeHigh;
					uint32_t sampleCountLow;
					uint32_t sampleCountHigh;
					uint32_t tableLength;
				};

				ds64chunk ds64 = {}; 
				static_assert( sizeof( ds64chunk ) == kChunkSizeDS64 );

				ds64.riffSizeLow = riffSize & 0xffffffff;
				ds64.riffSizeHigh = ( riffSize >> 32 ) & 0xffffffff;
				ds64.dataSizeLow = m_dataBytesWritten & 0xffffffff;
				ds64.dataSizeHigh = ( m_dataBytesWritten >> 32 ) & 0xffffffff;
				const long long sampleCount = m_dataBytesWritten / m_channels / ( m_bitsPerSample / 8 );
				ds64.sampleCountLow = sampleCount & 0xffffffff;
				ds64.sampleCountHigh = ( sampleCount >> 32 ) & 0xffffffff;
				ds64.tableLength = 0;

				fwrite( &ds64, kChunkSizeDS64, 1, m_file );
			}
		}
		fclose( m_file );
		m_file = nullptr;
	}
}
