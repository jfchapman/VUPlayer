#pragma once

#include "Decoder.h"

#include "mpc/streaminfo.h"
#include "mpc/mpcdec.h"

#include <string>

class DecoderMPC : public Decoder
{
public:
	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderMPC( const std::wstring& filename );

	~DecoderMPC() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

private:
	// File handle.
	FILE* m_file;

	// MPC reader.
	mpc_reader m_reader;

	// MPC demuxer.
	mpc_demux* m_demux;

	// Sample buffer.
	std::vector<float> m_buffer;

	// Sample count in buffer.
	size_t m_buffercount;

	// Sample position in buffer.
	size_t m_bufferpos;

	// Indicates whether the end of stream has been reached.
	bool m_eos;
};
