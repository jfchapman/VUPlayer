#pragma once

#include "Decoder.h"

#include <string>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;

class DecoderFFmpeg : public Decoder
{
public:
	// 'filename' - file name.
	// 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderFFmpeg( const std::wstring& filename, const Context context );

	~DecoderFFmpeg() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	double Seek( const double position ) override;

private:
	// Deccodes the next chunk of data into the sample buffer, returning whether any data was decoded.
	bool Decode();

	// Converts data from the 'frame' into the sample buffer.
	void ConvertSampleData( const AVFrame* frame );

	// Frees all FFmpeg contexts.
	void FreeContexts();

	// FFmpeg format context.
	AVFormatContext* m_FormatContext = nullptr;

	// FFmpeg decoder context.
	AVCodecContext* m_DecoderContext = nullptr;

	// FFmpeg current packet.
	AVPacket* m_Packet = nullptr;

	// FFmpeg current frame.
	AVFrame* m_Frame = nullptr;

	// FFmpeg resampler context.
	SwrContext* m_ResamplerContext = nullptr;

	// Index of the 'best' stream.
	int m_StreamIndex = 0;

	// Interleaved sample buffer.
	std::vector<float> m_Buffer;

	// Current buffer position.
	size_t m_BufferPosition = 0;
};
