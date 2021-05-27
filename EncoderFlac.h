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
	// 'filename' - (in) output file name without file extension, (out) output file name with file extension.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample, if applicable.
	// 'settings' - encoder settings.
	// Returns whether the encoder was opened.
	bool Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& settings ) override;

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	bool Write( float* samples, const long sampleCount ) override;

	// Closes the encoder.
	void Close() override;
};
