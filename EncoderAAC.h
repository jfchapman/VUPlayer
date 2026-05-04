#pragma once

#include "EncoderFFmpeg.h"

class EncoderAAC : public EncoderFFmpeg
{
public:
	EncoderAAC();

	virtual ~EncoderAAC();

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

protected:
	// Returns the file extension used by the encoder.
	std::wstring GetFileExtension() const override;

	// Returns the FFmpeg codec used by the encoder.
	const AVCodec* GetCodec() const override;

	// Sets up the FFmpeg codec context.
	bool SetupCodecContext( AVCodecContext* codecContext, const AVCodec* codec, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& settings ) const override;
};
