#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

// Decoder interface.
class Decoder
{
public:
  // Contexts for which a decoder can be used.
  enum class Context {
    Output,     // Decoding to an output device 
    Input,      // Decoding as an input for conversion
    Temporary   // Any other usage (information gathering, loudness calculation, etc.) 
  };

	// Decoder shared pointer type.
	using Ptr = std::shared_ptr<Decoder>;

	// A callback which returns true to continue.
	using CanContinue = std::function<bool()>;

	// Returns the duration in seconds.
	float GetDuration() const;

	// Returns the sample rate.
	long GetSampleRate() const;

	// Returns the number of channels.
	long GetChannels() const;

	// Returns the number of bits per sample (if relevant).
	std::optional<long> GetBPS() const;

	// Returns the bitrate in kbps (if relevant).
	std::optional<float> GetBitrate() const;

	// Returns the track gain, in dB, or nullopt if the calculation failed.
	// 'canContinue' - callback which returns whether the calculation can continue.
	// 'secondslimit' - number of seconds to devote to calculating an estimate, or 0 to perform a complete calculation.
	virtual std::optional<float> CalculateTrackGain( CanContinue canContinue, const float secondsLimit = 0 );

	// Skips any leading silence.
	void SkipSilence();

	// Returns whether stream titles are supported.
	virtual bool SupportsStreamTitles() const;

	// Returns the current stream title, and the position (in seconds) at which the title last changed.
	virtual std::pair<float /*seconds*/, std::wstring /*title*/> GetStreamTitle();

	// Reads sample data (taking notice of any cues).
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long ReadSamples( float* buffer, const long sampleCount );

	// Seeks to a 'position' in the stream, in seconds (taking notice of any cues).
	// Returns the new position in seconds.
	double SetPosition( const double position );

  // Sets the start & end positions (for tracks from cue files), in frames (where there are 75 frames per second).
  void SetCues( const std::optional<long>& cueStart, const std::optional<long>& cueEnd );

protected:
  // 'context' - context for which the decoder is to be used.
  Decoder( const Context context );

	virtual ~Decoder();

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	virtual long Read( float* buffer, const long sampleCount ) = 0;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	virtual double Seek( const double position ) = 0;

  // Returns the context for which the decoder is being used.
  Context GetContext() const { return m_Context; }

  // Sets the 'duration'.
	void SetDuration( const float duration );

	// Sets the 'sampleRate'.
	void SetSampleRate( const long sampleRate );

	// Sets the number of 'channels'.
	void SetChannels( const long channels );

	// Sets the number of 'bitsPerSample'.
	void SetBPS( const std::optional<long> bitsPerSample );

	// Sets the 'bitrate' in kbps.
	void SetBitrate( const std::optional<float> bitrate );

private:
  // Duration in seconds.
	float m_Duration;

	// Sample rate.
	long m_SampleRate;

	// Number of channels.
	long m_Channels;

	// Bits per sample (if relevant).
	std::optional<long> m_BPS;

	// Bitrate in kbps (if relevant).
	std::optional<float> m_Bitrate;

  // Start sample position (for tracks from cue files).
  std::optional<int64_t> m_SampleStart;

  // End sample position (for tracks from cue files).
  std::optional<int64_t> m_SampleEnd;

  // Remaining number of samples to read (for tracks from cue files).
  std::optional<int64_t> m_SamplesRemaining;

  // Decoder context.
  const Context m_Context;
};
