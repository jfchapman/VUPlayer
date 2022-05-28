#pragma once

#include "stdafx.h"

#include "Encoder.h"

#include <mmreg.h>

#include <vector>

// PCM encoder
class EncoderPCM : public Encoder
{
public:
	EncoderPCM();

	virtual ~EncoderPCM();

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
	// Wave format.
	std::optional<WAVEFORMATEX> m_wavFormat = std::nullopt;

	// Extensible wave format.
	std::optional<WAVEFORMATEXTENSIBLE> m_wavFormatExtensible = std::nullopt;

	// Scratch buffer for 8-bit & 24-bit samples.
	std::vector<uint8_t> m_buffer8;

	// Scratch buffer for 16-bit samples.
	std::vector<int16_t> m_buffer16;

	// Output file.
	FILE* m_file = nullptr;

	// Sample rate.
	long m_sampleRate = 0;

	// Channel count.
	long m_channels = 0;

	// Bits per sample.
	long m_bitsPerSample = 0;

	// Number of data bytes written.
	uint64_t m_dataBytesWritten = 0;

	// File position of the data chunk size.
	long long m_dataChunkSizeOffset = 0;

	// File position of the junk chunk.
	long long m_junkChunkOffset = 0;
};
