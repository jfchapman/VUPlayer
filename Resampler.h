#pragma once

#include "OutputDecoder.h"

struct SwrContext;

// An output decoder that provides resampled data.
class Resampler : public OutputDecoder
{
public:
	// 'decoder' - underlying decoder.
	// 'id' - playlist item ID.
	// 'outputRate' - output sample rate.
	// 'outputChannels' - output channels.
	Resampler( Decoder::Ptr decoder, const long id, const uint32_t outputRate, const uint32_t outputChannels );
	virtual ~Resampler();

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Returns the output sample rate.
	long GetOutputSampleRate() const override;

	// Returns the output channels.
	long GetOutputChannels() const override;

private:
	// Converts sample data using the FFmpeg resampler.
	// 'inputBuffer' - input samples from the decoder.
	// 'inputSamples' - input sample count.
	// 'outputBuffer' - output samples from the resampler.
	// 'outputSamples' - output sample count.
	// Returns the number of samples converted.
	uint32_t Convert( const float* const inputBuffer, const uint32_t inputSamples, float* outputBuffer, const uint32_t outputSamples );

	// Returns an additional factor by which to scale the buffer size when pre-buffering.
	float GetPreBufferSizeFactor() const;

	// FFmpeg resampler context.
	SwrContext* m_Context = nullptr;

	// Decoded sample data to feed the FFmpeg resampler.
	std::vector<float> m_InputBuffer;

	const uint32_t m_InputSampleRate;
	const uint32_t m_InputChannels;
	const uint32_t m_OutputSampleRate;
	const uint32_t m_OutputChannels;
};
