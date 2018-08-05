#pragma once

#include "stdafx.h"

#include "Playlist.h"

#include <winioctl.h>
#include <ntddcdrm.h>

#include <string>

// Audio CD information.
class CDDAMedia
{
public:
	// 'drive' - CD-ROM drive letter.
	// 'library' - media library.
	// Throws a std::runtime_error exception if there are no audio tracks available.
	CDDAMedia( const wchar_t drive, Library& library );

	virtual ~CDDAMedia();

	// CD audio data.
	typedef std::vector<short> Data;

	// Returns the formatted media 'filepath' corresponding to 'drive' & 'track'.
	static std::wstring ToMediaFilepath( const wchar_t drive, const long track );

	// Converts the formatted media 'filepath' to the corresponding 'drive' & 'track', returning whether the conversion was successful.
	static bool FromMediaFilepath( const std::wstring& filepath, wchar_t& drive, long& track );

	// Returns the CDDB ID.
	long GetCDDB() const;

	// Returns the playlist.
	Playlist::Ptr GetPlaylist() const;

	// Opens the CD for subsequent reading.
	// Returns a handle to the CD, or nullptr if the CD could not be opened.
	HANDLE Open() const;

	// Closes the CD 'handle'.
	void Close( const HANDLE handle ) const;

	// Reads the CD 'sector' into 'data', using the opened 'handle'.
	// Returns whether the sector was read successfully.
	bool Read( const HANDLE handle, const long sector, Data& data ) const;

	// Returns the start sector of the CD audio 'track'.
	long GetStartSector( const long track ) const;

	// Returns the sector count of the CD audio 'track'.
	long GetSectorCount( const long track ) const;

	// Returns the 'track' length, in bytes.
	long GetTrackLength( const long track ) const;

private:
	// A string pair.
	typedef std::pair<std::wstring,std::wstring> StringPair;

	// Maps a track number to an artist/title pair.
	typedef std::map<long,StringPair> StringPairMap;

	// Maps a block number to a string map.
	typedef std::map<long,StringPairMap> BlockMap;

	// Maps a track number to a string.
	typedef std::map<long,std::wstring> StringMap;

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

	// CD-ROM drive path.
	const std::wstring m_DrivePath;

	// Media library.
	Library& m_Library;

	// Disk geometry.
	DISK_GEOMETRY m_DiskGeometry;

	// Table of contents.
	CDROM_TOC m_TOC;

	// CDDB ID.
	long m_CDDB;

	// Playlist.
	Playlist::Ptr m_Playlist;
};
