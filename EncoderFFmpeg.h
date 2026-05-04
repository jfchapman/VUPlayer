#pragma once

#include "Encoder.h"

#include <vector>

struct AVCodecContext;
struct AVIOContext;
struct AVStream;
struct AVCodec;
struct AVFormatContext;
struct AVChannelLayout;
struct SwrContext;

class EncoderFFmpeg : public Encoder
{
public:
	EncoderFFmpeg();

	virtual ~EncoderFFmpeg();

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

protected:
	// Returns the file extension used by the encoder.
	virtual std::wstring GetFileExtension() const = 0;

	// Returns the FFmpeg codec used by the encoder.
	virtual const AVCodec* GetCodec() const = 0;

	// Sets up the FFmpeg codec context.
	virtual bool SetupCodecContext( AVCodecContext* codecContext, const AVCodec* codec, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& settings ) const = 0;

	// Returns the FFmpeg channel layout for the number of 'channels'.
	static AVChannelLayout GetChannelLayout( const uint32_t channels );

private:
	// Converts sample data using the FFmpeg resampler.
	// 'inputBuffer' - input samples (interleaved).
	// 'inputSamples' - input sample count.
	// 'outputBuffers' - output samples (planar).
	// 'outputSamples' - output sample count.
	// Returns the number of samples converted.
	uint32_t Resample( const float* const inputBuffer, const uint32_t inputSamples, uint8_t** outputBuffers, const uint32_t outputSamples );

	// Encodes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were encoded successfully.
	bool EncodeSamples( float* inputSamples, const long inputSampleCount );

	// Encodes the next frame from the sample buffer, returning false if there was any error.
	bool EncodeFrame();

	// Frees all FFmpeg contexts.
	void FreeContexts();

	AVFormatContext* m_avformatContext = nullptr;
	AVCodecContext* m_avcodecContext = nullptr;
	SwrContext* m_swrContext = nullptr;
	int64_t m_pts = 0;
	uint32_t m_BytesPerSample = 0;
	int m_InputSampleRate = 0;
	int m_OutputSampleRate = 0;
	int m_OutputChannels = 0;
	std::vector<std::vector<uint8_t>> m_SampleBuffers;
};
