#pragma once

#include <list>
#include <optional>
#include <string>

#include "Tag.h"

// Minimum valid year.
static constexpr long MINYEAR = 1000;

// Maximum valid year.
static constexpr long MAXYEAR = 9999;

// Loudness reference level in LUFS.
static constexpr float LOUDNESS_REFERENCE = -18.0f;

// Media information.
class MediaInfo
{
public:
	// A list of media information.
	using List = std::list<MediaInfo>;

	// Source types.
	enum class Source {
		File,
		CDDA
	};

	// 'filename' - media file name (for file sources).
	MediaInfo( const std::wstring& filename = std::wstring() );

	// 'cddbID' - CDDB ID (for CDDA sources).
	MediaInfo( const long cddbID );

	// Less than operator.
	bool operator<( const MediaInfo& other ) const;

	// Tags cast operator.
	operator Tags() const;

	// Returns the filename (full path).
	const std::wstring& GetFilename() const;

	// Sets the filename (full path).
	void SetFilename( const std::wstring& filename );

	// Returns the last modified timestamp.
	long long GetFiletime() const;

	// Sets the last modified timestamp.
	void SetFiletime( const long long filetime );

	// Returns the filesize, in bytes.
  // 'applyCues' - true to return the estimated track filesize with cues applied (if present), false to return the underlying filesize.
	long long GetFilesize( const bool applyCues = false ) const;

	// Sets the filesize, in bytes.
	void SetFilesize( const long long filesize );

	// Returns the duration, in seconds.
  // 'applyCues' - true to return the track duration with cues applied (if present), false to return the underlying file duration.
	float GetDuration( const bool applyCues = true ) const;

	// Sets the duration, in seconds.
	void SetDuration( const float duration );

	// Returns the sample rate.
	long GetSampleRate() const;

	// Sets the sample rate.
	void SetSampleRate( const long sampleRate );

	// Returns the bits per sample, if relevant.
	std::optional<long> GetBitsPerSample() const;

	// Sets the bits per sample, if relevant.
	void SetBitsPerSample( const std::optional<long> bitsPerSample );

	// Returns the channel count.
	long GetChannels() const;

	// Sets the channel count.
	void SetChannels( const long channels );

	// Returns the artist.
	const std::wstring& GetArtist() const;

	// Sets the artist.
	void SetArtist( const std::wstring& artist );

	// Sets the title.
	void SetTitle( const std::wstring& title );

	// Returns the album.
	const std::wstring& GetAlbum() const;

	// Sets the album.
	void SetAlbum( const std::wstring& album );

	// Returns the genre.
	const std::wstring& GetGenre() const;

	// Sets the genre.
	void SetGenre( const std::wstring& genre );

	// Returns the year.
	long GetYear() const;

	// Sets the year.
	void SetYear( const long year );

	// Returns the comment.
	const std::wstring& GetComment() const;

	// Sets the comment.
	void SetComment( const std::wstring& comment );

	// Returns the track number.
	long GetTrack() const;

	// Sets the track number.
	void SetTrack( const long track );

	// Returns the format version.
	const std::wstring& GetVersion() const;

	// Sets the format version.
	void SetVersion( const std::wstring& version );

	// Returns the track gain, in dB (or nullopt if there is no gain).
	std::optional<float> GetGainTrack() const;

	// Sets the track gain, in dB.
	void SetGainTrack( const std::optional<float> gain );

	// Returns the album gain, in dB (or nullopt if there is no gain).
	std::optional<float> GetGainAlbum() const;

	// Sets the album gain, in dB.
	void SetGainAlbum( const std::optional<float> gain );

	// Returns the title
	// 'filenameAsTitle' - whether to return the filename if there is no title.
	std::wstring GetTitle( const bool filenameAsTitle = false ) const;

	// Returns the filename extension.
	std::wstring GetType() const;

	// Returns the bitrate, in kbps (or nullopt if there is no bitrate).
	// 'calculate' - calculates the bitrate based on the filesize, if there is no bitrate.
	std::optional<float> GetBitrate( const bool calculate = false ) const;

	// Sets the bitrate, in kbps.
	void SetBitrate( const std::optional<float> bitrate );

	// Returns the artwork ID.
	// 'checkFolder' - if true and there is no artwork ID, returns the filename of any artwork in the media file's folder.
	std::wstring GetArtworkID( const bool checkFolder = false ) const;

	// Sets the artwork ID.
	void SetArtworkID( const std::wstring& id );

  // Gets the start position (for tracks from cue files), in frames (where there are 75 frames per second).
  const std::optional<long>& GetCueStart() const;

  // Sets the start position (for tracks from cue files), in frames (where there are 75 frames per second).
  void SetCueStart( const std::optional<long>& frames );

  // Gets the end position (for tracks from cue files), in frames (where there are 75 frames per second).
  const std::optional<long>& GetCueEnd() const;

  // Sets the end position (for tracks from cue files), in frames (where there are 75 frames per second).
  void SetCueEnd( const std::optional<long>& frames );

	// Returns the filename, including cues (if present).
  // 'fullPath' - whether to return the full path, or just the filename component.
  // 'removeExtension' - whether to return just the filename component with no extension.
	std::wstring GetFilenameWithCues( const bool fullPath = false, const bool removeExtension = false ) const;

  // Returns the media source.
	Source GetSource() const;

	// Returns the CDDB ID.
	long GetCDDB() const;

	// Returns whether the 'other' media information is a duplicate of this one.
	bool IsDuplicate( const MediaInfo& other ) const;

	// Gets common media information (restricted to artist, title, album, genre, year, comment, track, artwork).
	// 'mediaList' - the list of media to query.
	// 'commonInfo' - out, common media information.
	// Returns whether any common information was retrieved.
	static bool GetCommonInfo( const List& mediaList, MediaInfo& commonInfo );

  // Returns the string value with embedded cues, if present (used for display and copy & paste operations).
  static std::wstring FormatCues( const std::optional<long>& cueStart, const std::optional<long>& cueEnd, const std::wstring& value );

  // Returns media information from a filepath containing optional embedded cues (used for paste operations).
  static MediaInfo ExtractCues( const std::wstring& filepath );

private:
	std::wstring m_Filename = {};
	long long m_Filetime = 0;
	long long m_Filesize = 0;
	float m_Duration = 0;
	long m_SampleRate = 0;
	long m_Channels = 0;
	std::wstring m_Artist = {};
	std::wstring m_Title = {};
	std::wstring m_Album = {};
	std::wstring m_Genre = {};
	long m_Year = 0;
	std::wstring m_Comment= {};
	long m_Track = 0;
	std::wstring m_Version = {};
	std::wstring m_ArtworkID = {};
	Source m_Source = Source::File;
	long m_CDDB = 0;
	std::optional<long> m_BitsPerSample = std::nullopt;
	std::optional<float> m_Bitrate = std::nullopt;
	std::optional<float> m_GainTrack = std::nullopt;
	std::optional<float> m_GainAlbum = std::nullopt;
  std::optional<long> m_CueStart = std::nullopt;
  std::optional<long> m_CueEnd = std::nullopt;
};

