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
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderFlac( const std::wstring& filename );

	virtual ~DecoderFlac();

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	virtual long Read( float* buffer, const long sampleCount );

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	virtual float Seek( const float position );

protected:
	// FLAC callbacks
	virtual FLAC__StreamDecoderReadStatus read_callback( FLAC__byte [], size_t * );
	virtual FLAC__StreamDecoderSeekStatus seek_callback( FLAC__uint64 );
	virtual FLAC__StreamDecoderTellStatus tell_callback( FLAC__uint64 * );
	virtual FLAC__StreamDecoderLengthStatus length_callback( FLAC__uint64 * );
	virtual bool eof_callback();
	virtual FLAC__StreamDecoderWriteStatus write_callback( const FLAC__Frame *, const FLAC__int32 *const [] );
	virtual void metadata_callback( const FLAC__StreamMetadata * );
	virtual void error_callback( FLAC__StreamDecoderErrorStatus );

private:
	// Input file stream.
	std::ifstream m_FileStream;

	// Current FLAC frame.
	FLAC__Frame m_FLACFrame;

	// Current FLAC buffer.
	FLAC__int32** m_FLACBuffer;

	// Current FLAC frame position.
	unsigned int m_FLACFramePos;

	// Indicates whether this is a valid FLAC stream.
	bool m_Valid;
};

