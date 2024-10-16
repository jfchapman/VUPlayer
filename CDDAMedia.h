#pragma once

#include "stdafx.h"

#include "MusicBrainz.h"
#include "Playlist.h"

#include <winioctl.h>
#include <ntddcdrm.h>

#include <string>
#include <tuple>
#include <set>

class CDDACache;

// Audio CD information.
class CDDAMedia
{
public:
	// 'drive' - CD-ROM drive letter.
	// 'library' - media library.
	// 'musicbrainz' - MusicBrainz manager.
	// Throws a std::runtime_error exception if there are no audio tracks available.
	CDDAMedia( const wchar_t drive, Library& library, MusicBrainz& musicbrainz );

	virtual ~CDDAMedia();

	// CD audio data.
	using Data = std::vector<short>;

	// A map of CD Audio data, keyed by sector index.
	using DataMap = std::map<long, Data>;

	// Returns the formatted media 'filepath' corresponding to 'drive' & 'track'.
	static std::wstring ToMediaFilepath( const wchar_t drive, const long track );

	// Converts the formatted media 'filepath' to the corresponding 'drive' & 'track', returning whether the conversion was successful.
	static bool FromMediaFilepath( const std::wstring& filepath, wchar_t& drive, long& track );

	// Returns whether the table of contents for a CD-ROM 'drive' letter contains any data tracks.
	static bool	ContainsData( const wchar_t drive );

	// Returns whether the table of contents for a CD-ROM 'drive' letter contains any CD Audio tracks.
	static bool	ContainsCDAudio( const wchar_t drive );

	// Returns the CDDB ID.
	long GetCDDB() const;

	// Returns the playlist.
	Playlist::Ptr GetPlaylist() const;

	// Opens the CD for subsequent reading.
	// Returns a handle to the CD, or nullptr if the CD could not be opened.
	HANDLE Open() const;

	// Closes the CD 'handle'.
	void Close( const HANDLE handle ) const;

	// Reads a CD audio sector.
	// 'handle' - CD handle.
	// 'sector' - sector index.
	// 'useCache' - whether to cache the read sector.
	// 'data' - out, the CD audio data.
	// Returns whether the sector was read successfully.
	bool Read( const HANDLE handle, const long sector, const bool useCache, Data& data ) const;

	// Reads CD audio sectors.
	// 'handle' - CD handle.
	// 'sectorStart' - start sector index.
	// 'sectorCount' - the number of sectors to read.
	// 'dataMap' - out, the CD audio data.
	// Returns whether any sectors were read successfully.
	bool Read( const HANDLE handle, const long sectorStart, const long sectorCount, DataMap& data ) const;

	// Returns the start sector of the CD audio 'track'.
	long GetStartSector( const long track ) const;

	// Returns the sector count of the CD audio 'track' (returns zero for data tracks).
	long GetSectorCount( const long track ) const;

	// Returns the 'track' length, in bytes (returns zero for data tracks).
	long GetTrackLength( const long track ) const;

	// Returns the MusicBrainz ID for disc queries.
	std::pair<std::string /*discid*/, std::string /*toc*/> GetMusicBrainzDiscID() const;

  // Returns the MusicBrainz ID ('discid'/'toc'), 'startCues' & the 'backingFile' for a 'playlist' (if it is suitable for a query).
  static std::optional<std::tuple<std::string /*discid*/, std::string /*toc*/, std::set<long> /*startCues*/, std::wstring /*backingFile*/>> GetMusicBrainzPlaylistID( Playlist* const playlist );

private:
	// A string pair.
	using StringPair = std::pair<std::wstring, std::wstring>;

	// Maps a track number to an artist/title pair.
	using StringPairMap = std::map<long, StringPair>;

	// Maps a block number to a string map.
	using BlockMap = std::map<long, StringPairMap>;

	// Maps a track number to a string.
	using StringMap = std::map<long, std::wstring>;

	// Reads the table of contents, returning whether there are any audio tracks available.
	bool ReadTOC();

	// Calculates the CDDB ID from the table of contents.
	bool CalculateCDDBID();

	// Returns the CDDB sum for 'sectorCount'.
	unsigned long CDDBSum( unsigned long sectorCount ) const;

	// Generates a playlist for the CD 'drive', returning whether the playlist was generated successfully.
	bool GeneratePlaylist( const wchar_t drive );

	// Reads CD-Text into 'blocks', returning whether any information was read.
	bool ReadCDText( BlockMap& blocks ) const;

	// Sets the 'text' and any 'extraTitles' using the CD-Text 'data' at the 'descriptorIndex'.
	void GetDataBlockText( const CDROM_TOC_CD_TEXT_DATA& data, const int descriptorIndex, std::wstring& text, StringMap& extraTitles ) const;

	// Returns the MusicBrainz ID for the 'toc'.
	static std::pair<std::string /*discid*/, std::string /*toc*/> GetMusicBrainzID( const CDROM_TOC& toc );

	// Returns the start sector of the CD audio 'track' from the 'toc'.
	static long GetStartSector( const CDROM_TOC& toc, const long track );

  // CD-ROM drive path.
	const std::wstring m_DrivePath;

	// Media library.
	Library& m_Library;

	// MusicBrainz manager.
	MusicBrainz& m_MusicBrainz;

	// Disk geometry.
	DISK_GEOMETRY m_DiskGeometry;

	// Table of contents.
	CDROM_TOC m_TOC;

	// CDDB ID.
	long m_CDDB;

	// Playlist.
	Playlist::Ptr m_Playlist;

	// CD audio data cache.
	std::shared_ptr<CDDACache> m_Cache;
};
