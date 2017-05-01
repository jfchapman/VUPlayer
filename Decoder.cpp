#include "Decoder.h"

#include "Settings.h"

#include "replaygain_analysis.h"

#include <windows.h>

#include <random>
#include <string>

Decoder::Decoder() :
	m_Duration( 0 ),
	m_SampleRate( 0 ),
	m_Channels( 0 ),
	m_BPS( 0 )
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

float Decoder::CalculateTrackGain( const float secondsLimit )
{
	float trackGain = REPLAYGAIN_NOVALUE;
	if ( ( m_SampleRate > 0 ) && ( m_Channels > 0 ) ) {

		LARGE_INTEGER perfFreq, perfStart, perfEnd;
		QueryPerformanceFrequency( &perfFreq );
		QueryPerformanceCounter( &perfStart );

		if ( secondsLimit > 0 ) {
			Seek( m_Duration * 0.33f );
		}

		replaygain_analysis analysis;
		analysis.InitGainAnalysis( m_SampleRate );
		const long scale = 1 << 15;
		const long bufferSize = 4096;
		float* buffer = new float[ bufferSize * m_Channels ];
		const int numChannels = ( 1 == m_Channels ) ? 1 : 2;
		float* leftSamples = new float[ bufferSize ];
		float* rightSamples = ( 1 == numChannels ) ? nullptr : new float[ bufferSize ];

		long totalSamplesRead = 0;
		long samplesRead = Read( buffer, bufferSize );
		while ( samplesRead > 0 ) {
			totalSamplesRead += samplesRead;
			for ( long index = 0; index < samplesRead; index++ ) {
				leftSamples[ index ] = buffer[ index * m_Channels ] * scale;
			}
			if ( nullptr != rightSamples ) {
				for ( long index = 0; index < samplesRead; index++ ) {
					rightSamples[ index ] = buffer[ index * m_Channels + 1 ] * scale;
				}
			}
			analysis.AnalyzeSamples( leftSamples, rightSamples, samplesRead, numChannels );
			if ( secondsLimit > 0 ) {
				QueryPerformanceCounter( &perfEnd );
				const float seconds = static_cast<float>( perfEnd.QuadPart - perfStart.QuadPart ) / perfFreq.QuadPart;
				if ( seconds >= secondsLimit ) {
					break;
				}
			}
			samplesRead = Read( buffer, bufferSize );
		}

		delete [] leftSamples;
		delete [] rightSamples;
		delete [] buffer;

		trackGain = analysis.GetTitleGain();
		if ( GAIN_NOT_ENOUGH_SAMPLES == trackGain ) {
			trackGain = REPLAYGAIN_NOVALUE;
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