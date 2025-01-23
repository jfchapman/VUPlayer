#include "Decoder.h"

#include "Settings.h"

#include "ebur128.h"

#include <windows.h>

#include <string>

Decoder::Decoder( const Context context ) :
	m_Duration( 0 ),
	m_SampleRate( 0 ),
	m_Channels( 0 ),
	m_BPS(),
	m_Bitrate(),
	m_Context( context )
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

std::optional<long> Decoder::GetBPS() const
{
	return m_BPS;
}

void Decoder::SetBPS( const std::optional<long> bitsPerSample )
{
	m_BPS = bitsPerSample;
}

std::optional<float> Decoder::GetBitrate() const
{
	return m_Bitrate;
}

void Decoder::SetBitrate( const std::optional<float> bitrate )
{
	m_Bitrate = bitrate;
}

std::optional<float> Decoder::CalculateTrackGain( CanContinue canContinue, const float secondsLimit )
{
	std::optional<float> trackGain;
	if ( ( m_SampleRate > 0 ) && ( m_Channels > 0 ) ) {

		LARGE_INTEGER perfFreq, perfStart, perfEnd;
		QueryPerformanceFrequency( &perfFreq );
		QueryPerformanceCounter( &perfStart );

		if ( ( secondsLimit > 0 ) && !m_SampleStart ) {
			Seek( m_Duration * 0.33f );
		}

		ebur128_state* r128State = ebur128_init( static_cast<unsigned int>( m_Channels ), static_cast<unsigned int>( m_SampleRate ), EBUR128_MODE_I );
		if ( nullptr != r128State ) {
			const long sampleSize = 4096;
			std::vector<float> buffer( sampleSize * m_Channels );
			long samplesRead = ReadSamples( buffer.data(), sampleSize );
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
				samplesRead = ReadSamples( buffer.data(), sampleSize );
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
		while ( silence && ReadSamples( buffer.data(), 1 /*sampleCount*/ ) > 0 ) {
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

long Decoder::ReadSamples( float* buffer, const long requestedSamples )
{
	const long samplesToRead = m_SamplesRemaining ? static_cast<long>( std::min<int64_t>( requestedSamples, *m_SamplesRemaining ) ) : requestedSamples;
	const long samplesRead = Read( buffer, samplesToRead );
	if ( m_SamplesRemaining ) {
		*m_SamplesRemaining -= samplesRead;
	}
	return samplesRead;
}

double Decoder::SetPosition( const double position )
{
	double offset = 0;
	if ( m_SampleStart && ( GetSampleRate() > 0 ) ) {
		m_SamplesRemaining.reset();
		offset = static_cast<double>( *m_SampleStart ) / GetSampleRate();
		if ( m_SampleEnd && ( *m_SampleEnd >= *m_SampleStart ) ) {
			if ( const int64_t sampleCount = *m_SampleEnd - *m_SampleStart - static_cast<int64_t>( position * GetSampleRate() ); sampleCount > 0 ) {
				m_SamplesRemaining = sampleCount;
			}
		}
	}
	return Seek( position + offset ) - offset;
}

void Decoder::SetCues( const std::optional<long>& cueStart, const std::optional<long>& cueEnd )
{
	m_SampleStart = cueStart ? std::make_optional( static_cast<int64_t>( *cueStart ) * GetSampleRate() / 75 ) : std::nullopt;
	m_SampleEnd = cueEnd ? std::make_optional( static_cast<int64_t>( *cueEnd ) * GetSampleRate() / 75 ) : std::nullopt;
	if ( m_SampleStart && m_SampleEnd && ( *m_SampleEnd >= *m_SampleStart ) ) {
		m_SamplesRemaining = *m_SampleEnd - *m_SampleStart;
	}
	if ( m_SampleStart ) {
		SetPosition( 0 );
	}
}
