#pragma once
#include "Decoder.h"

#include "FLAC++\all.h"

#include <fstream>
#include <vector>

// FLAC decoder
class DecoderFlac : public Decoder, public FLAC::Decoder::Stream
{
public:
	// 'filename' - file name.
  // 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderFlac( const std::wstring& filename, const Context context );

	~DecoderFlac() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

protected:
	// FLAC callbacks
	FLAC__StreamDecoderReadStatus read_callback( FLAC__byte [], size_t * ) override;
	FLAC__StreamDecoderSeekStatus seek_callback( FLAC__uint64 ) override;
	FLAC__StreamDecoderTellStatus tell_callback( FLAC__uint64 * ) override;
	FLAC__StreamDecoderLengthStatus length_callback( FLAC__uint64 * ) override;
	bool eof_callback() override;
	FLAC__StreamDecoderWriteStatus write_callback( const FLAC__Frame *, const FLAC__int32 *const [] ) override;
	void metadata_callback( const FLAC__StreamMetadata * ) override;
	void error_callback( FLAC__StreamDecoderErrorStatus ) override;

private:
	// Calculates the bitrate of the FLAC stream (returns nullopt if the bitrate was not calculated).
	std::optional<float> CalculateBitrate();

	// Input file stream.
	std::ifstream m_FileStream;

	// Current FLAC frame.
	FLAC__Frame m_FLACFrame;

	// Frame buffer.
	std::vector<float> m_FrameBuffer;

	// Current frame buffer position.
	uint32_t m_FramePos;

	// Indicates whether this is a valid FLAC stream.
	bool m_Valid;
};

