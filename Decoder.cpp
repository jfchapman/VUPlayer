#include "Decoder.h"

#include "Settings.h"

#include "ebur128.h"

#include <windows.h>

#include <random>
#include <string>

Decoder::Decoder() :
	m_Duration( 0 ),
	m_SampleRate( 0 ),
	m_Channels( 0 ),
	m_BPS( 0 ),
	m_Bitrate( NAN )
{
}

Decoder::~Decoder()
{
}

float Decoder::GetDuration() const
{
	return m_Duration;
}

void Decoder::SetDuration( const float duration )
{
	m_Duration = duration;
}

long Decoder::GetSampleRate() const
{
	return m_SampleRate;
}

void Decoder::SetSampleRate( const long sampleRate )
{
	m_SampleRate = sampleRate;
}

long Decoder::GetChannels() const
{
	return m_Channels;
}

void Decoder::SetChannels( const long channels )
{
	m_Channels = channels;
}

long Decoder::GetBPS() const
{
	return m_BPS;
}

void Decoder::SetBPS( const long bitsPerSample )
{
	m_BPS = bitsPerSample;
}

float Decoder::GetBitrate() const
{
	return m_Bitrate;
}

void Decoder::SetBitrate( const float bitrate )
{
	m_Bitrate = bitrate;
}

float Decoder::CalculateTrackGain( CanContinue canContinue, const float secondsLimit )
{
	float trackGain = NAN;
	if ( ( m_SampleRate > 0 ) && ( m_Channels > 0 ) ) {

		LARGE_INTEGER perfFreq, perfStart, perfEnd;
		QueryPerformanceFrequency( &perfFreq );
		QueryPerformanceCounter( &perfStart );

		if ( secondsLimit > 0 ) {
			Seek( m_Duration * 0.33f );
		}

		ebur128_state* r128State = ebur128_init( static_cast<unsigned int>( m_Channels ), static_cast<unsigned int>( m_SampleRate ), EBUR128_MODE_I );
		if ( nullptr != r128State ) {
			const long sampleSize = 4096;
			std::vector<float> buffer( sampleSize * m_Channels );
			long samplesRead = Read( buffer.data(), sampleSize );
			int errorState = EBUR128_SUCCESS;
			while ( ( EBUR128_SUCCESS == errorState ) && ( samplesRead > 0 ) && canContinue() ) {
				errorState = ebur128_add_frames_float( r128State, buffer.data(), static_cast<size_t>( samplesRead ) );
				if ( secondsLimit > 0 ) {
					QueryPerformanceCounter( &perfEnd );
					const float seconds = static_cast<float>( perfEnd.QuadPart - perfStart.QuadPart ) / perfFreq.QuadPart;
					if ( seconds >= secondsLimit ) {
						break;
					}
				}
				samplesRead = Read( buffer.data(), sampleSize );
			}

			if ( ( EBUR128_SUCCESS == errorState ) && canContinue() ) {
				double loudness = 0;
				errorState = ebur128_loudness_global( r128State, &loudness );
				if ( EBUR128_SUCCESS == errorState ) {
					trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
				}
			}
			ebur128_destroy( &r128State );
		}
	}
	return trackGain;
}

void Decoder::SkipSilence()
{
	if ( m_Channels > 0 ) {
		std::vector<float> buffer( m_Channels );
		bool silence = true;
		while ( silence && Read( &buffer[ 0 ], 1 /*sampleCount*/ ) > 0 ) {
			for ( auto sample = buffer.begin(); silence && ( sample != buffer.end() ); sample++ ) {
				silence = ( 0 == *sample );
			}
		}
	}
}

bool Decoder::SupportsStreamTitles() const
{
	return false;
}

std::pair<float /*seconds*/, std::wstring /*title*/> Decoder::GetStreamTitle()
{
	return {};
}
