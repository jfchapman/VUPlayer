#pragma once

#include "Tag.h"

#include <memory>
#include <optional>
#include <string>

// Encoder interface.
class Encoder
{
public:
	Encoder()
	{
	}

	virtual ~Encoder()
	{
	}

	// Encoder shared pointer type.
	typedef std::shared_ptr<Encoder> Ptr;

	// Opens the encoder.
	// 'filename' - (in) output file name without file extension, (out) output file name with file extension.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample, if applicable.
	// 'totalSamples' - approximate total number of samples to encode, if known.
	// 'settings' - encoder settings.
	// 'tags' - metadata tags.
	// Returns whether the encoder was opened.
	virtual bool Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const long long totalSamples, const std::string& settings, const Tags& tags ) = 0;

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	virtual bool Write( float* samples, const long sampleCount ) = 0;

	// Closes the encoder.
	virtual void Close() = 0;
};
