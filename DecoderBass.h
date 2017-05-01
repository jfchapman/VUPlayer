#pragma once

#include "Decoder.h"

#include "bass.h"

#include <string>
#include <stdexcept>

// Bass decoder
class DecoderBass : public Decoder
{
public:
	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderBass( const std::wstring& filename );

	virtual ~DecoderBass();

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	virtual long Read( float* buffer, const long sampleCount );

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	virtual float Seek( const float position );

private:
	// Stream handle
	DWORD m_Handle;

	// Indicates when to start fading out MOD music (or zero if not applicable).
	QWORD m_FadeStartPosition;

	// Indicates when to end fading out MOD music (or zero if not applicable).
	QWORD m_FadeEndPosition;

	// Current decoding position.
	QWORD m_CurrentPosition;
};

