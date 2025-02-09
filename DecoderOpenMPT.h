#pragma once

// Due to GitHub file size limits, exclude libopenmpt from debug builds.
#ifndef _DEBUG

#include "Decoder.h"

#include "libopenmpt.hpp"

#include <fstream>
#include <thread>

class DecoderOpenMPT : public Decoder
{
public:
	// 'filename' - file name.
	// 'context' - context for which the decoder is to be used.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderOpenMPT( const std::wstring& filename, const Context context );

	~DecoderOpenMPT() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	double Seek( const double position ) override;

private:
	// Starts the loop detection thread.
	void StartLoopDetectionThread();

	// Stops the loop detection thread.
	void StopLoopDetectionThread();

	// Calculates whether the song is looped.
	void CalculateIsLooped();

	// Loop detection thread.
	std::thread m_threadLoopDetection;

	// Indicates whether the loop detection thread should be stopped.
	std::atomic_bool m_stopLoopDetection = false;

	// Indicates if the song is looped.
	std::atomic_bool m_looped = false;

	// File name.
	const std::wstring m_filename;

	// File stream.
	std::ifstream m_stream;

	// OpenMPT module.
	openmpt::module m_module;

	// Indicates whether to fade out looped songs.
	bool m_fadeout = false;
};

#endif // _NDEBUG