#pragma once

#include "stdafx.h"

#include "Decoder.h"
#include "Playlist.h"

#include <array>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

// Buffered output decoder wrapper.
class OutputDecoder
{
public:
	// 'decoder' - underlying decoder.
	// 'id' - playlist item ID.
	OutputDecoder( Decoder::Ptr decoder, const long id );

	virtual ~OutputDecoder();

	// Callback function for when the output decoder has finished pre-buffering the playlist item ID.
	using PreBufferFinishedCallback = std::function<void( const long /*ID*/ )>;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount );

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position );

	// Returns the sample rate.
	long GetSampleRate() const;

	// Returns the number of channels.
	long GetChannels() const;

	// Returns the number of bits per sample (if relevant).
	std::optional<long> GetBPS() const;

	// Returns the bitrate in kbps (if relevant).
	std::optional<float> GetBitrate() const;

	// Skips any leading silence.
	void SkipSilence();

	// Returns whether stream titles are supported.
	bool SupportsStreamTitles() const;

	// Returns the current stream title, and the position (in seconds) at which the title last changed.
	std::pair<float /*seconds*/, std::wstring /*title*/> GetStreamTitle();

	// Starts pre-buffering sample data - all subsequent reads will be pre-buffered.
	// 'callback' - called when the output decoder has finished pre-buffering.
	void PreBuffer( PreBufferFinishedCallback callback );

private:
	// Starts the pre-buffering thread.
	void StartPreBufferThread();

	// Stops the pre-buffering thread.
	void StopPreBufferThread();

	// Decoder.
	const Decoder::Ptr m_Decoder;

	// Decoder channels.
	const long m_Channels;

	// Playlist item ID.
	const long m_ID;

	// Indicates whether to use pre-buffering.
	bool m_UsePreBuffer = false;

	// Pre-buffer output semaphore.
	HANDLE m_SemaphoreOutput = nullptr;

	// Pre-buffer input semaphore.
	HANDLE m_SemaphoreInput = nullptr;

	// Pre-buffer thread.
	std::thread m_BufferThread;

	// Pre-buffer mutex.
	std::mutex m_BufferMutex;

	// Indicates whether the pre-buffering thread should stop.
	std::atomic_bool m_StopPreBuffering = false;

	// Indicates whether decoding has finished.
	bool m_DecoderFinished = false;

	// Callback function for when the output decoder has finished pre-buffering.
	PreBufferFinishedCallback m_PreBufferFinishedCallback = nullptr;

	// Pre-buffered sample data.
	std::vector<std::vector<float>> m_BufferSlots;

	// Read buffer queue.
	std::queue<std::vector<float>*> m_ReadQueue;

	// Current read buffer.
	std::vector<float>* m_ReadBuffer = nullptr;

	// Current read buffer offset.
	long m_ReadBufferOffset = 0;
};
