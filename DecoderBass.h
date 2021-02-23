#pragma once

#include "Decoder.h"

#include "bass.h"

#include <mutex>
#include <string>

// Bass decoder
class DecoderBass : public Decoder
{
public:
	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderBass( const std::wstring& filename );

	~DecoderBass() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

	// Returns the track gain, in dB.
	// 'canContinue' - callback which returns whether the calculation can continue.
	// 'secondslimit' - number of seconds to devote to calculating an estimate, or 0 to perform a complete calculation.
	float CalculateTrackGain( CanContinue canContinue, const float secondsLimit = 0 ) override;

	// Returns whether stream titles are supported.
	bool SupportsStreamTitles() const override;

	// Returns the current stream title, and the position (in seconds) at which the title last changed.
	std::pair<float /*seconds*/, std::wstring /*title*/> GetStreamTitle() override;

private:
	// URL stream metadata callback.
	static void CALLBACK MetadataSyncProc( HSYNC handle, DWORD channel, DWORD data, void *user );

	// Called when metadata is received for a 'channel'.
	void OnMetadata( const DWORD channel );

	// Stream handle
	DWORD m_Handle;

	// Indicates when to start fading out MOD music (or zero if not applicable).
	QWORD m_FadeStartPosition;

	// Indicates when to end fading out MOD music (or zero if not applicable).
	QWORD m_FadeEndPosition;

	// Current decoding position.
	QWORD m_CurrentPosition;

	// Indicates whether the current stream is a URL.
	bool m_IsURL;

	// Current number of silence samples that have been returned for URL streams.
	long m_CurrentSilenceSamples;

	// Current stream title, and the position (in seconds) at which the title last changed.
	std::pair<float /*seconds*/, std::wstring /*title*/> m_StreamTitle;

	// Stream title mutex.
	std::mutex m_StreamTitleMutex;
};

