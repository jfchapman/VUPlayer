#pragma once

#include "Encoder.h"

#include "FLAC++/all.h"

// FLAC encoder
class EncoderFlac : public Encoder, public FLAC::Encoder::File
{
public:
	EncoderFlac();

	virtual ~EncoderFlac();

	// Opens the encoder.
	// 'filename' - output file name.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample if applicable, zero if not.
	// Returns whether the encoder was opened.
	virtual bool Open( const std::wstring& filename, const long sampleRate, const long channels, const long bitsPerSample );

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	virtual bool Write( float* samples, const long sampleCount );

	// Closes the encoder.
	virtual void Close();
};
