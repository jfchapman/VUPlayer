#pragma once

#include "Decoder.h"

#include "CDDAMedia.h"

#include <string>

class DecoderCDDA : public Decoder
{
public:
	// 'cddaMedia' - CD audio disc information.
	// 'track' - track number.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderCDDA( const CDDAMedia& cddaMedia, const long track );

	virtual ~DecoderCDDA();

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	virtual long Read( float* buffer, const long sampleCount );

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	virtual float Seek( const float position );

	// Returns the track gain, in dB.
	// 'secondslimit' - number of seconds to devote to calculating an estimate, or 0 to perform a complete calculation.
	virtual float CalculateTrackGain( const float secondsLimit = 0 );

private:
	// CD audio disc information.
	const CDDAMedia m_CDDAMedia;

	// Start sector.
	const long m_SectorStart;

	// End sector.
	const long m_SectorEnd;

	// CD audio handle.
	const HANDLE m_Handle;

	// Data buffer.
	CDDAMedia::Data m_Buffer;

	// Current sector.
	long m_CurrentSector;

	// Current buffer position.
	size_t m_CurrentBufPos;
};
