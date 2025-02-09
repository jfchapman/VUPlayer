#pragma once

#include "Decoder.h"

#include <string>
#include <fstream>
#include <vector>

// Decoder for raw audio data from bin/cue files (44100Hz, 16-bit, stereo).
class DecoderBin : public Decoder
{
public:
	// 'filename' - file name.
	// 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderBin( const std::wstring& filename, const Context context );

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	double Seek( const double position ) override;

private:
	// File stream.
	std::ifstream m_Stream;

	// Scratch buffer.
	std::vector<char> m_Buffer;
};
