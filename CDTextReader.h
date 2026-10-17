#pragma once

#include "stdafx.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

// CD-Text reader, for the data returned by IOCTL_CDROM_READ_TOC_EX using the CDROM_READ_TOC_EX_FORMAT_CDTEXT format.
// https://www.gnu.org/software/libcdio/cd-text-format.html
class CDTextReader
{
public:
	// CD-Text information for a track (or for the whole disc).
	struct Track {
		std::wstring Title;
		std::wstring Performer;
		std::wstring Songwriter;
		std::wstring Composer;
		std::wstring Message;
		std::wstring Genre;	// Only applies to the whole disc.
	};

	// Maps a track number to its information (where track zero represents the whole disc).
	using Tracks = std::map<long, Track>;

	// Maps a block number (each block represents a language) to the track information.
	using Blocks = std::map<long, Tracks>;

	// 'packs' - CD-Text packs, excluding the data header (any incomplete trailing pack is ignored).
	CDTextReader( const std::vector<unsigned char>& packs );

	// Returns the track information for each block.
	const Blocks& GetBlocks() const;

private:
	// Returns the code page to use for a 'characterCode' (from a size information pack), and whether the text is 'doubleByte'.
	static UINT GetCodePage( const std::optional<unsigned char>& characterCode, const bool doubleByte );

	// Returns the name of a genre 'code', or an empty string if the code is not defined.
	static std::wstring GetGenre( const unsigned int code );

	// Track information for each block.
	Blocks m_Blocks;
};
