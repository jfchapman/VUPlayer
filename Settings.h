#pragma once

#include <list>

#include "Database.h"
#include "Library.h"
#include "Playlist.h"

// MOD music fadeout flag.
static const DWORD VUPLAYER_MUSIC_FADEOUT = 0x80000000;

// Application settings
class Settings
{
public:
	// 'database' - application database.
	// 'library' - media library.
	// 'settings' - initial settings, used when running in 'portable' mode. 
	Settings( Database& database, Library& library, const std::string& settings = std::string() );

	virtual ~Settings();

	// Playlist column information.
	struct PlaylistColumn {
		int ID;			// Column ID.
		int Width;	// Column width.
	};

	// Hotkey information.
	struct Hotkey {
		int ID;						// ID.
		int Key;					// Key code.
		bool Alt;					// Alt key modifier.
		bool Ctrl;				// Control key modifier.
		bool Shift;				// Shift key modifier.
	};

	// Gain mode.
	enum class GainMode {
		Disabled,
		Track,
		Album
	};

	// Limiter mode.
	enum class LimitMode {
		None,
		Hard,
		Soft
	};

	// Notification area icon click commands.
	enum class SystrayCommand {
		None = 0,
		Play,
		Stop,
		Previous,
		Next,
		ShowHide
	};

	// Pitch range options.
	enum class PitchRange {
		Small = 0,
		Medium,
		Large
	};

	// EQ settings.
	struct EQ {
		// Maps a centre frequency, in Hz, to a gain value.
		typedef std::map<int,float> GainMap;

		// Indicates that the EQ window is centred.
		static const int Centred = INT_MIN;

		// Maximum gain in dB.
		static const int MaxGain = 9;

		// Minimum gain in dB.
		static const int MinGain = -9;

		EQ() :
			Visible( false ),
			X( Centred ),
			Y( Centred ),
			Enabled( false ),
			Preamp( 0 ),
			Gains( { { 80, 0.f }, { 140, 0.f }, { 250, 0.f }, { 500, 0.f }, { 1000, 0.f }, { 2000, 0.f }, { 4000, 0.f }, { 8000, 0.f }, { 14000, 0.f } } ),
			Bandwidth( 12 )
		{
		}

		bool Visible;						// Indicates whether the EQ window is shown.
		int X;									// EQ window X position.
		int Y;									// EQ window Y position.
		bool Enabled;						// Indicates whether EQ is enabled.
		float Preamp;						// Preamp in the range -15 to +15dB.
		GainMap Gains;					// Gains in the range -15 to +15dB.
		float Bandwidth;				// Bandwidth in semitones.
	};

	// A list of playlist columns.
	typedef std::list<PlaylistColumn> PlaylistColumns;

	// A list of hotkeys.
	typedef std::list<Hotkey> HotkeyList;

	// Maps a pitch range option to a pitch adjustment factor.
	typedef std::map<PitchRange,float> PitchRangeMap;

	// Returns the playlist control settings.
	// 'columns' - out, column settings.
	// 'font' - out, list control font.
	// 'fontColour' - out, list control font colour.
	// 'backgroundColour' - out, list control background colour.
	// 'highlightColour' - out, list control highlight colour.
	void GetPlaylistSettings( PlaylistColumns& columns, LOGFONT& font,
			COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour );

	// Sets the playlist control settings.
	// 'columns' - column settings.
	// 'font' - list control font.
	// 'fontColour' - list control font colour.
	// 'backgroundColour' - list control background colour.
	// 'highlightColour' - list control highlight colour.
	void SetPlaylistSettings( const PlaylistColumns& columns, const LOGFONT& font,
			const COLORREF& fontColour, const COLORREF& backgroundColour, const COLORREF& highlightColour );

	// Returns the tree control settings.
	// 'font' - out, tree control font.
	// 'fontColour' - out, tree control font colour.
	// 'backgroundColour' - out, tree control background colour.
	// 'highlightColour' - out, tree control highlight colour.
	// 'showFavourites' - out, whether Favourites is shown.
	// 'showAllTracks' - out, whether All Tracks is shown.
	// 'showArtists' - out, whether Artists are shown.
	// 'showAlbums' - out, whether Albums are shown.
	// 'showGenres' - out, whether Genres are shown.
	// 'showYears' - out, whether Years are shown.
	void GetTreeSettings( LOGFONT& font, COLORREF& fontColour, COLORREF& backgroundColour, COLORREF& highlightColour,
			bool& showFavourites, bool& showAllTracks, bool& showArtists, bool& showAlbums, bool& showGenres, bool& showYears );

	// Sets the tree control settings.
	// 'font' - tree control font.
	// 'fontColour' - tree control font colour.
	// 'backgroundColour' - tree control background colour.
	// 'highlightColour' - tree control highlight colour.
	// 'showFavourites' - whether Favourites is shown.
	// 'showAllTracks' - whether All Tracks is shown.
	// 'showArtists' - whether Artists are shown.
	// 'showAlbums' - whether Albums are shown.
	// 'showGenres' - whether Genres are shown.
	// 'showYears' - whether Years are shown.
	void SetTreeSettings( const LOGFONT& font, const COLORREF& fontColour, const COLORREF& backgroundColour, const COLORREF& highlightColour,
			const bool showFavourites, const bool showAllTracks, const bool showArtists, const bool showAlbums, const bool showGenres, const bool showYears );

	// Gets the playlists.
	Playlists GetPlaylists();

	// Gets the Favourites playlist.
	Playlist::Ptr GetFavourites();

	// Removes a playlist from the database.
	void RemovePlaylist( const Playlist& playlist );

	// Saves a playlist to the database.
	void SavePlaylist( Playlist& playlist );

	// Gets the oscilloscope colour.
	COLORREF GetOscilloscopeColour();

	// Sets the oscilloscope colour.
	void SetOscilloscopeColour( const COLORREF colour );

	// Gets the oscilloscope background colour.
	COLORREF GetOscilloscopeBackground();

	// Sets the oscilloscope background colour.
	void SetOscilloscopeBackground( const COLORREF colour );

	// Gets the oscilloscope weight.
	float GetOscilloscopeWeight();

	// Sets the oscilloscope weight.
	void SetOscilloscopeWeight( const float weight );

	// Gets the spectrum analyser settings.
	// 'base' - out, base colour.
	// 'peak' - out, peak colour.
	// 'background - out, background colour.
	void GetSpectrumAnalyserSettings( COLORREF& base, COLORREF& peak, COLORREF& background );

	// Sets the spectrum analyser settings.
	// 'base' - base colour.
	// 'peak' - peak colour.
	// 'background - background colour.
	void SetSpectrumAnalyserSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background );

	// Gets the peak meter settings.
	// 'base' - out, base colour.
	// 'peak' - out, peak colour.
	// 'background - out, background colour.
	void GetPeakMeterSettings( COLORREF& base, COLORREF& peak, COLORREF& background );

	// Sets the peak meter settings.
	// 'base' - base colour.
	// 'peak' - peak colour.
	// 'background - background colour.
	void SetPeakMeterSettings( const COLORREF& base, const COLORREF& peak, const COLORREF& background );

	// Gets the VUMeter decay setting.
	float GetVUMeterDecay();

	// Sets the VUMeter decay settings.
	void SetVUMeterDecay( const float decay );

	// Gets the application startup position settings.
	// 'x' - out, desktop X position.
	// 'y' - out, desktop Y position.
	// 'width' - out, main window width.
	// 'height' - out, main window height.
	// 'maximised' - out, whether the main window is maximised.
	// 'minimsed' - out, whether the main window is minimised.
	void GetStartupPosition( int& x, int& y, int& width, int& height, bool& maximised, bool& minimised );

	// Gets the application startup position settings.
	// 'x' - out, desktop X position.
	// 'y' - out, desktop Y position.
	// 'width' - out, main window width.
	// 'height' - out, main window height.
	// 'maximised' - out, whether the main window is maximised.
	// 'minimsed' - out, whether the main window is minimised.
	void SetStartupPosition( const int x, const int y, const int width, const int height, const bool maximised, const bool minimised );

	// Returns the startup visual ID.
	int GetVisualID();

	// Sets the startup visual ID.
	void SetVisualID( const int visualID );

	// Returns the startup split width.
	int GetSplitWidth();

	// Sets the startup split width.
	void SetSplitWidth( const int width );

	// Returns the startup volume level.
	float GetVolume();

	// Sets the startup volume level.
	void SetVolume( const float volume );

	// Gets the startup playlist.
	std::wstring GetStartupPlaylist();

	// Sets the startup 'playlist'.
	void SetStartupPlaylist( const std::wstring& playlist );

	// Gets the startup filename.
	std::wstring GetStartupFilename();

	// Sets the startup 'filename'.
	void SetStartupFilename( const std::wstring& filename );

	// Gets the counter settings.
	// 'font' - out, counter font.
	// 'fontColour' - out, counter colour.
	// 'showRemaining' - out, true to display remaining time, false for elapsed time.
	void GetCounterSettings( LOGFONT& font,	COLORREF& colour, bool& showRemaining );

	// Sets the counter settings.
	// 'font' - counter font.
	// 'fontColour' - counter colour.
	// 'showRemaining' - true to display remaining time, false for elapsed time.
	void SetCounterSettings( const LOGFONT& font, const COLORREF colour, const bool showRemaining );

	// Returns the output device name (or an empty string for the default device).
	std::wstring GetOutputDevice();

	// Sets the output device 'name' (or an empty string for the default device).
	void SetOutputDevice( const std::wstring& name );

	// Gets default MOD music settings.
	// 'mod' - out, MOD settings.
	// 'mtm' - out, MTM settings.
	// 's3m' - out, S3M settings.
	// 'xm' - out, XM settings.
	// 'it' - out, IT settings.
	void GetDefaultMODSettings( long long& mod, long long& mtm, long long& s3m, long long& xm, long long& it );

	// Gets MOD music settings.
	// 'mod' - out, MOD settings.
	// 'mtm' - out, MTM settings.
	// 's3m' - out, S3M settings.
	// 'xm' - out, XM settings.
	// 'it' - out, IT settings.
	void GetMODSettings( long long& mod, long long& mtm, long long& s3m, long long& xm, long long& it );

	// Sets MOD music settings.
	// 'mod' - MOD settings.
	// 'mtm' - MTM settings.
	// 's3m' - S3M settings.
	// 'xm' - XM settings.
	// 'it' - IT settings.
	void SetMODSettings( const long long mod, const long long mtm, const long long s3m, const long long xm, const long long it );

	// Gets default gain settings.
	// 'gainMode' - out, gain mode.
	// 'limitMode' - out, limiter mode.
	// 'preamp' - out, preamp in dB.
	void GetDefaultGainSettings( GainMode& gainMode, LimitMode& limitMode, float& preamp );

	// Gets gain settings.
	// 'gainMode' - out, gain mode.
	// 'limitMode' - out, limiter mode.
	// 'preamp' - out, preamp in dB.
	void GetGainSettings( GainMode& gainMode, LimitMode& limitMode, float& preamp );

	// Sets gain settings.
	// 'gainMode' - gain mode.
	// 'limitMode' - limiter mode.
	// 'preamp' - preamp in dB.
	void SetGainSettings( const GainMode gainMode, const LimitMode limitMode, const float preamp );

	// Gets notification area settings.
	// 'enable' - out, whether the notification area icon is shown.
	// 'singleClick' - out, single click action.
	// 'doubleClick' - out, double click action.
	void GetSystraySettings( bool& enable, SystrayCommand& singleClick, SystrayCommand& doubleClick );

	// Sets notification area settings.
	// 'enable' - whether the notification area icon is shown.
	// 'singleClick' - single click action.
	// 'doubleClick' - double click action.
	void SetSystraySettings( const bool enable, const SystrayCommand singleClick, const SystrayCommand doubleClick );

	// Gets playback settings.
	// 'randomPlay' - whether random playback is enabled.
	// 'repeatTrack' - whether repeat track is enabled.
	// 'repeatPlaylist' - whether repeat playlist is enabled.
	// 'crossfade' - whether crossfade is enabled.
	void GetPlaybackSettings( bool& randomPlay, bool& repeatTrack, bool& repeatPlaylist, bool& crossfade );

	// Sets playback settings.
	// 'randomPlay' - whether random playback is enabled.
	// 'repeatTrack' - whether repeat track is enabled.
	// 'repeatPlaylist' - whether repeat playlist is enabled.
	// 'crossfade' - whether crossfade is enabled.
	void SetPlaybackSettings( const bool randomPlay, const bool repeatTrack, const bool repeatPlaylist, const bool crossfade );

	// Gets hotkey settings.
	// 'enable' - out, whether hotkeys are enabled.
	// 'hotkeys' - out, hotkeys.
	void GetHotkeySettings( bool& enable, HotkeyList& hotkeys );

	// Sets hotkey settings.
	// 'enable' - whether hotkeys are enabled.
	// 'hotkeys' - hotkeys.
	void SetHotkeySettings( const bool enable, const HotkeyList& hotkeys );

	// Gets the pitch range.
	PitchRange GetPitchRange();

	// Sets the pitch range.
	void SetPitchRange( const PitchRange range );

	// Returns the available pitch range options.
	PitchRangeMap GetPitchRangeOptions() const;

	// Gets the output control type (volume, pitch, etc).
	int GetOutputControlType();

	// Sets the output control type (volume, pitch, etc).
	void SetOutputControlType( const int type );

	// Returns the track conversion/extraction settings.
	void GetExtractSettings( std::wstring& folder, std::wstring& filename, bool& addToLibrary, bool& joinTracks );

	// Sets the track conversion/extraction settings.
	void SetExtractSettings( const std::wstring& folder, const std::wstring& filename, const bool addToLibrary, const bool joinTracks );

	// Gets Gracenote settings.
	// 'userID' - Gracenote user ID string.
	// 'enable' - whether to enable Gracenote functionality.
	// 'enableLog' - whether to enable logging of Gracenote functionality.
	void GetGracenoteSettings( std::string& userID, bool& enable, bool& enableLog );

	// Sets Gracenote settings.
	// 'userID' - Gracenote user ID string.
	// 'enable' - whether to enable Gracenote functionality.
	// 'enableLog' - whether to enable logging of Gracenote functionality.
	void SetGracenoteSettings( const std::string& userID, const bool enable, const bool enableLog );

	// Gets EQ settings.
	EQ GetEQSettings();

	// Sets EQ settings.
	void SetEQSettings( const EQ& eq );

	// Gets the name of the encoder to use for conversion.
	std::wstring GetEncoder();

	// Sets the name of the 'encoder' to use for conversion.
	void SetEncoder( const std::wstring& encoder );

	// Gets the settings for the 'encoder' name.
	std::string GetEncoderSettings( const std::wstring& encoder );

	// Sets the 'settings' for the 'encoder' name.
	void SetEncoderSettings( const std::wstring& encoder, const std::string& settings );

	// Gets the soundfont file name to use for MIDI playback.
	std::wstring GetSoundFont();

	// Sets the soundfont file name to use for MIDI playback.
	void SetSoundFont( const std::wstring& filename );

	// Returns whether the 'toolbarID' is enabled.
	bool GetToolbarEnabled( const int toolbarID );

	// Sets whether the 'toolbarID' is 'enabled'.
	void SetToolbarEnabled( const int toolbarID, const bool enabled );

	// Returns whether duplicate tracks are merged (for Artist/Album/Genre/Year playlists).
	bool GetMergeDuplicates();

	// Sets whether duplicate tracks are merged (for Artist/Album/Genre/Year playlists).
	void SetMergeDuplicates( const bool mergeDuplicates );

	// Returns the last user selected folder for the 'folderType'.
	std::wstring GetLastFolder( const std::string& folderType );

	// Sets the last user selected 'folder' for the 'folderType'.
	void SetLastFolder( const std::string& folderType, const std::wstring& folder );

	// Returns whether scrobbling is enabled.
	bool GetScrobblerEnabled();

	// Sets whether scrobbling is enabled.
	void SetScrobblerEnabled( const bool enabled );

	// Returns the scrobbler session key.
	std::string GetScrobblerKey();

	// Sets the scrobbler session 'key'.
	void SetScrobblerKey( const std::string& key );

	// Exports the settings in JSON format to 'output'.
	void ExportSettings( std::string& output );

private:
	// Updates the database to the current version if necessary.
	void UpdateDatabase();

	// Updates the settings table if necessary.
	void UpdateSettingsTable();

	// Updates the playlist columns table if necessary.
	void UpdatePlaylistColumnsTable();

	// Updates the playlist sources table if necessary.
	void UpdatePlaylistsTable();

	// Updates the hotkeys table if necessary.
	void UpdateHotkeysTable();

	// Updates the playlist table if necessary.
	void UpdatePlaylistTable( const std::string& table );

	// Sets the playlist files from the database.
	void ReadPlaylistFiles( Playlist& playlist );

	// Imports the JSON format settings from 'input'.
	void ImportSettings( const std::string& input );

	// Returns whether a GUID string is valid.
	static bool IsValidGUID( const std::string& guid );

	// Database.
	Database& m_Database;

	// Media library.
	Library& m_Library;
};
