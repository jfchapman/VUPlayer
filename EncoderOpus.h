#pragma once

#include "Encoder.h"

#include "opusenc.h"

// FLAC encoder
class EncoderOpus : public Encoder
{
public:
	EncoderOpus();

	virtual ~EncoderOpus();

	// Returns the bitrate from the encoder 'settings'.
	static int GetBitrate( const std::string& settings );

	// Returns the default bitrate;
	static int GetDefaultBitrate();

	// Returns the minimum bitrate;
	static int GetMinimumBitrate();

	// Returns the maximum bitrate;
	static int GetMaximumBitrate();

	// Clamps the bitrate between the allowed limits.
	static void LimitBitrate( int& bitrate );

	// Opens the encoder.
	// 'filename' - (in) output file name without file extension, (out) output file name with file extension.
	// 'sampleRate' - sample rate.
	// 'channels' - channel count.
	// 'bitsPerSample' - bits per sample, if applicable.
	// 'settings' - encoder settings.
	// Returns whether the encoder was opened.
	bool Open( std::wstring& filename, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& settings ) override;

	// Writes sample data.
	// 'samples' - input samples (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to write.
	// Returns whether the samples were written successfully.
	bool Write( float* samples, const long sampleCount ) override;

	// Closes the encoder.
	void Close() override;

private:
	// Opus encoder write callback.
	static int WriteCallback( void *user_data, const unsigned char *ptr, opus_int32 len );

	// Opus encoder close callback.
	static int CloseCallback( void *user_data );

	// Channel count.
	long m_Channels;

	// Opus encoder object.
	OggOpusEnc* m_OpusEncoder;

	// Opus encoder callbacks.
	OpusEncCallbacks m_Callbacks;
};
