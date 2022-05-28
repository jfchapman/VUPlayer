#pragma once

#include "Encoder.h"

#include "bass.h"
#include "lame.h"

#include <vector>

// LAME MP3 encoder
class EncoderMP3 : public Encoder
{
public:
	EncoderMP3();

	virtual ~EncoderMP3();

	// Returns the VBR quality from the encoder 'settings'.
	static int GetVBRQuality( const std::string& settings );

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
	// LAME flags.
	lame_global_flags* m_flags = nullptr;

	// Output file.
	FILE* m_file = nullptr;

	// Output buffer.
	std::vector<unsigned char> m_outputBuffer;

	// Mix buffer, used when downsampling to stereo.
	std::vector<float> m_mixBuffer;

	// Input channel count.
	long m_inputChannels = 0;

	// Mixer stream, used when downsampling to stereo.
	HSTREAM m_mixerStream = 0;

	// Decoder stream, used when downsampling to stereo.
	HSTREAM m_decoderStream = 0;
};
