#pragma once

#include "Encoder.h"

#include "FLAC++/all.h"

// FLAC encoder
class EncoderFlac : public Encoder, public FLAC::Encoder::File
{
public:
	EncoderFlac();

	virtual ~EncoderFlac();

	// Returns the compression level from the encoder 'settings'.
	static int GetCompressionLevel( const std::string& settings );

	// Returns the default compression level.
	static int GetDefaultCompressionLevel();

	// Returns the minimum compression level.
	static int GetMinimumCompressionLevel();

	// Returns the maximum compression level.
	static int GetMaximumCompressionLevel();

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
	// Creates a vorbis comment metadata object.
	// 'tags' - metadata tags.
	// Returns the metadata object, or null if none of the supplied tags are supported.
	static std::unique_ptr<FLAC::Metadata::VorbisComment> CreateVorbisComment( const Tags& tags );

	// Creates a picture metadata object.
	// 'tags' - metadata tags.
	// Returns the metadata object, or null if tags does not contain a supported picture type.
	static std::unique_ptr<FLAC::Metadata::Picture> CreatePicture( const Tags& tags );

	// Seek table metadata object.
	std::unique_ptr<FLAC::Metadata::SeekTable> m_seekTable;

	// Padding metadata object.
	std::unique_ptr<FLAC::Metadata::Padding> m_padding;

	// Vorbis comment metadata object.
	std::unique_ptr<FLAC::Metadata::VorbisComment> m_vorbisComment;

	// Picture metadata object.
	std::unique_ptr<FLAC::Metadata::Picture> m_picture;
};
