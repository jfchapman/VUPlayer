#pragma once

#include <memory>
#include <stdexcept>

// Decoder interface.
class Decoder
{
public:
	Decoder();

	virtual ~Decoder();

	// Decoder shared pointer type.
	typedef std::shared_ptr<Decoder> Ptr;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	virtual long Read( float* buffer, const long sampleCount ) = 0;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	virtual float Seek( const float position ) = 0;

	// Returns the duration in seconds.
	float GetDuration() const;

	// Sets the 'duration'.
	void SetDuration( const float duration );

	// Returns the sample rate.
	long GetSampleRate() const;

	// Sets the 'sampleRate'.
	void SetSampleRate( const long sampleRate );

	// Returns the number of channels.
	long GetChannels() const;

	// Sets the number of 'channels'.
	void SetChannels( const long channels );

	// Returns the number of bits per sample.
	long GetBPS() const;

	// Sets the number of 'bitsPerSample'.
	void SetBPS( const long bitsPerSample );

	// Returns the track gain, in dB.
	// 'secondslimit' - number of seconds to devote to calculating an estimate, or 0 to perform a complete calculation.
	virtual float CalculateTrackGain( const float secondsLimit = 0 );

	// Skips any leading silence.
	void SkipSilence();

private:
	// Duration in seconds.
	float m_Duration;

	// Sample rate.
	long m_SampleRate;

	// Number of channels.
	long m_Channels;

	// Bits per sample (if relevant).
	long m_BPS;
};
