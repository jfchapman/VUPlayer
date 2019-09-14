#include "DecoderCDDA.h"

#include "Utility.h"

DecoderCDDA::DecoderCDDA( const CDDAMedia& cddaMedia, const long track ) :
	Decoder(),
	m_CDDAMedia( cddaMedia ),
	m_SectorStart( m_CDDAMedia.GetStartSector( track ) ),
	m_SectorEnd( m_CDDAMedia.GetStartSector( track + 1 ) ),
	m_Handle( ( m_SectorEnd > m_SectorStart ) ? m_CDDAMedia.Open() : nullptr ),
	m_Buffer(),
	m_CurrentSector( m_SectorStart ),
	m_CurrentBufPos( 0 )
{
	if ( nullptr == m_Handle ) {
		throw std::runtime_error( "DecoderCDDA could not open track" );
	} else {
		SetSampleRate( 44100 );
		SetChannels( 2 );
		SetBPS( 16 );
		SetDuration( static_cast<float>( m_CDDAMedia.GetTrackLength( track ) ) / ( 44100 * 4 ) );
	}
}

DecoderCDDA::~DecoderCDDA()
{
	m_CDDAMedia.Close( m_Handle );
}

long DecoderCDDA::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	long outputBufPos = 0;
	while ( samplesRead < sampleCount ) {
		if ( m_CurrentBufPos < m_Buffer.size() ) {
			buffer[ outputBufPos++ ] = static_cast<float>( m_Buffer[ m_CurrentBufPos++ ] ) / 32768.0f;
			buffer[ outputBufPos++ ] = static_cast<float>( m_Buffer[ m_CurrentBufPos++ ] ) / 32768.0f;
			++samplesRead;
		} else {
			if ( ( m_CurrentSector < m_SectorEnd ) && ( m_CDDAMedia.Read( m_Handle, m_CurrentSector++, true /*useCache*/, m_Buffer ) ) ) {
				m_CurrentBufPos = 0;
			} else {
				break;
			}
		}
	}
	return samplesRead;
}

float DecoderCDDA::Seek( const float position )
{
	float seekPosition = 0;
	const float duration = GetDuration();
	if ( duration > 0 ) {
		const long seekSector = m_SectorStart + static_cast<long>( ( m_SectorEnd - m_SectorStart ) * ( position / duration ) );
		if ( ( seekSector >= m_SectorStart ) && ( seekSector < m_SectorEnd ) ) {
			m_CurrentSector = seekSector;
			m_CurrentBufPos = 0;
			seekPosition = position;
		}
	}
	return seekPosition;
}

float DecoderCDDA::CalculateTrackGain( const float /*secondsLimit*/ )
{
	// Reading a CD is too slow to provide an estimate.
	return NAN;
}
