#pragma once

#include "Handler.h"

#include <memory>
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
	// 'filename' - output file name.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample if applicable, zero if not.
	// Returns whether the encoder was opened.
	virtual bool Open( const std::wstring& filename, const long sampleRate, const long channels, const long bitsPerSample ) = 0;

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	virtual bool Write( float* samples, const long sampleCount ) = 0;

	// Closes the encoder.
	virtual void Close() = 0;
};
