#pragma once

#include "Decoder.h"

#include "libopenmpt/libopenmpt.hpp"

#include <fstream>

class DecoderOpenMPT : public Decoder
{
public:
	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderOpenMPT( const std::wstring& filename );

	~DecoderOpenMPT() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

private:
  // File stream.
  std::ifstream m_stream;

  // OpenMPT module.
  openmpt::module m_module;
};
