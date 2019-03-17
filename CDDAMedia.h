#pragma once

#include "stdafx.h"

#include "Gracenote.h"
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
	// 'gracenote' - Gracenote handler.
	// Throws a std::runtime_error exception if there are no audio tracks available.
	CDDAMedia( const wchar_t drive, Library& library, Gracenote& gracenote );

	virtual ~CDDAMedia();

	// CD audio data.
	typedef std::vector<short> Data;

	// A map of CD Audio data, keyed by sector index.
	typedef std::map<long,Data> DataMap;

	// Returns the formatted media 'filepath' corresponding to 'drive' & 'track'.
	static std::wstring ToMediaFilepath( const wchar_t drive, const long track );

	// Converts the formatted media 'filepath' to the corresponding 'drive' & 'track', returning whether the conversion was successful.
	static bool FromMediaFilepath( const std::wstring& filepath, wchar_t& drive, long& track );

	// Returns whether the table of contents for a CD-ROM 'drive' letter contains any data tracks.
	static bool	ContainsData( const wchar_t drive );

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

	// Returns the sector count of the CD audio 'track'.
	long GetSectorCount( const long track ) const;

	// Returns the 'track' length, in bytes.
	long GetTrackLength( const long track ) const;

	// Returns the table of contents in string form.
	std::string GetGracenoteTOC() const;

private:
	// Data cache.
	class Cache
	{
		public:
			Cache() :
				m_Cache(),
				m_Mutex()
			{
			}

			// Gets CD audio 'data' for the 'sector' index, returning whether the sector data was retrieved.
			bool GetData( const long sector, Data& data )
			{
				std::lock_guard<std::mutex> lock( m_Mutex );
				const auto iter = m_Cache.find( sector );
				const bool success = ( m_Cache.end() != iter );
				if ( success ) {
					data = iter->second;
				}
				return success;
			}

			// Caches the CD audio 'data' for the 'sector' index.
			void SetData( const long sector, const Data& data )
			{
				std::lock_guard<std::mutex> lock( m_Mutex );
				m_Cache.insert( std::map<long,Data>::value_type( sector, data ) );
			}

		private:
			// CD audio data, mapped by sector.
			std::map<long,Data> m_Cache;
			
			// Cache mutex.
			std::mutex m_Mutex;
	};

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

	// Gracenote handler.
	Gracenote& m_Gracenote;

	// Disk geometry.
	DISK_GEOMETRY m_DiskGeometry;

	// Table of contents.
	CDROM_TOC m_TOC;

	// CDDB ID.
	long m_CDDB;

	// Playlist.
	Playlist::Ptr m_Playlist;

	// CD audio data cache.
	std::shared_ptr<Cache> m_Cache;
};
