#pragma once

#include "Decoder.h"

#include "wavpack.h"

#include <string>

class DecoderWavpack : public Decoder
{
public:
	// 'filename' - file name.
	// 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderWavpack( const std::wstring& filename, const Context context );

	~DecoderWavpack() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	double Seek( const double position ) override;

private:
	// WavPack context.
	WavpackContext* m_Context;
};
