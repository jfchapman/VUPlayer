#pragma once

#include "Encoder.h"

#include <vector>
#include <array>

struct AVCodecContext;
struct AVIOContext;
struct AVStream;
struct AVCodec;
struct AVFormatContext;
struct SwrContext;

// FFmpeg (AAC) encoder
class EncoderFFmpeg : public Encoder
{
public:
	EncoderFFmpeg();

	virtual ~EncoderFFmpeg();

	// Returns the bitrate from the encoder 'settings'.
	static int GetBitrate( const std::string& settings );

	// Returns the default bitrate.
	static int GetDefaultBitrate();

	// Returns the minimum bitrate.
	static int GetMinimumBitrate();

	// Returns the maximum bitrate.
	static int GetMaximumBitrate();

	// Clamps the bitrate between the allowed limits.
	static void LimitBitrate( int& bitrate );

	// Opens the encoder.
	// 'filename' - (in) output file name without file extension, (out) output file name with file extension.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample, if applicable.
	// 'totalSamples' - approximate total number of samples to encode, if known.
	// 'settings' - encoder settings.
	// 'tags' - metadata tags.
	// Returns whether the encoder was opened.
	bool Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const long long totalSamples, const std::string& settings, const Tags& tags ) override;

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	bool Write( float* samples, const long sampleCount ) override;

	// Closes the encoder.
	void Close() override;

private:
	// Converts sample data using the FFmpeg resampler.
	// 'inputBuffer' - input samples (interleaved).
	// 'inputSamples' - input sample count.
	// 'outputBuffers' - output samples (planar).
	// 'outputSamples' - output sample count.
	// Returns the number of samples converted.
	uint32_t Resample( const float* const inputBuffer, const uint32_t inputSamples, uint8_t** outputBuffers, const uint32_t outputSamples );

	// Encodes the next frame from the sample buffer, returning false if there was any error.
	bool EncodeFrame();

	// Frees all FFmpeg contexts.
	void FreeContexts();

	AVFormatContext* m_avformatContext = nullptr;
	AVCodecContext* m_avcodecContext = nullptr;
	SwrContext* m_swrContext = nullptr;
	int64_t m_pts = 0;
	int m_InputSampleRate = 0;
	int m_OutputSampleRate = 0;
	int m_OutputChannels = 0;
	std::array<std::vector<float>, 8> m_SampleBuffers;
};
