#pragma once

#include "Decoder.h"

#include "All.h"
#include "maclib.h"
#include "APETag.h"

#include <string>

class DecoderMAC : public Decoder
{
public:
	// 'filename' - file name.
  // 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderMAC( const std::wstring& filename, const Context context );

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

private:
	// APE decompressor.
	std::unique_ptr<APE::IAPEDecompress> m_decompress;
};
