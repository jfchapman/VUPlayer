#pragma once

#include "EncoderFFmpeg.h"

class EncoderALAC : public EncoderFFmpeg
{
public:
	EncoderALAC();

	virtual ~EncoderALAC();

protected:
	// Returns the file extension used by the encoder.
	std::wstring GetFileExtension() const override;

	// Returns the FFmpeg codec used by the encoder.
	const AVCodec* GetCodec() const override;

	// Sets up the FFmpeg codec context.
	bool SetupCodecContext( AVCodecContext* codecContext, const AVCodec* codec, const long sampleRate, const long channels, const std::optional<long> bitsPerSample, const std::string& settings ) const override;
};
