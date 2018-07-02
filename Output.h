#pragma once

#include "bass.h"
#include "Handlers.h"
#include "Playlist.h"
#include "Settings.h"

#include <atomic>

// Message ID for signalling that playback needs to be restarted from a playlist item ID (wParam).
static const UINT MSG_RESTARTPLAYBACK = WM_APP + 191;

// Audio output
class Output
{
public:
	// 'hwnd' - main window handle.
	// 'handlers' - the available handlers.
	// 'settings' - application settings.
	// 'initialVolume' - initial volume level.
	Output( const HWND hwnd, const Handlers& handlers, Settings& settings, const float initialVolume );

	virtual ~Output();

	// Output states
	enum class State {
		Stopped,
		Playing,
		Paused
	};

	// Output item information.
	struct Item {
		Playlist::Item PlaylistItem;	// Playlist item.
		float Position;								// Position in seconds.
		float InitialSeek;						// Initial seek time for the item.
	};

	// Maps a device ID to its description.
	typedef std::map<int,std::wstring> Devices;

	// Starts playback.
	// 'startID' - Playlist ID at which to start playback (or zero to start playback at the first playlist item).
	// 'seek' - Initial start position in seconds (negative value to seek relative to the end of the track).
	// Returns true if playback was started.
	bool Play( const long startID = 0, const float seek = 0.0f );

	// Stops playback.
	void Stop();

	// Pauses playback.
	void Pause();

	// Plays the previous track.
	// 'forcePrevious' - True to always play the previous track, false to restart the current track if not already near the start.
	// 'seek' - Start position in seconds (negative value to seek relative to the end of the track).
	void Previous( const bool forcePrevious = false, const float seek = 0.0f );
	
	// Plays the next track.
	void Next();
	
	// Sets the current 'playlist', setting the current state to stopped if necessary.
	void SetPlaylist( const Playlist::Ptr& playlist );

	// Gets the current playlist.
	Playlist::Ptr GetPlaylist();

	// Returns the current output state.
	State GetState() const;

	// Returns the currently playing item.
	Item GetCurrentPlaying();

	// Reads sample data from the current decoder.
	// 'buffer' - sample buffer.
	// 'byteCount' - number of bytes to read.
	// 'handle' - stream handle.
	// Returns the number of bytes read.
	DWORD ReadSampleData( float* buffer, const DWORD byteCount, HSTREAM handle );

	// Returns the volume level in the range 0.0 (silent) to 1.0 (full volume).
	float GetVolume() const;

	// Sets the volume level in the range 0.0 (silent) to 1.0 (full volume).
	void SetVolume( const float volume );

	// Returns the pitch adjustment factor, with 1.0 representing no adjustment.
	float GetPitch() const;

	// Sets the pitch adjustment factor, with 1.0 representing no adjustment.
	void SetPitch( const float pitch );

	// Gets the channel levels for visualisation.
	// 'left' - out, left channel level in the range 0.0 to 1.0.
	// 'right' - out, right channel level in the range 0.0 to 1.0.
	void GetLevels( float& left, float& right );

	// Gets sample data for visualisation.
	// 'sampleCount' - number of samples per channel to get.
	// 'samples' - out, sample data in the range +/-1.0 (an empty vector if nothing is playing).
	// 'channels' - out, number of channels (or zero if nothing is playing).
	void GetSampleData( const long sampleCount, std::vector<float>& samples, long& channels );

	// Gets FFT data for visualisation.
	// 'fft' - out, FFT data.
	void GetFFTData( std::vector<float>& fft );

	// Signals that playback should be restarted.
	void RestartPlayback();

	// Gets whether random play is enabled.
	bool GetRandomPlay() const;

	// Sets whether random play is enabled. 
	void SetRandomPlay( const bool enabled );

	// Gets whether repeat track is enabled.
	bool GetRepeatTrack() const;

	// Sets whether repeat track is enabled. 
	void SetRepeatTrack( const bool enabled );

	// Gets whether repeat playlist is enabled.
	bool GetRepeatPlaylist() const;

	// Sets whether repeat playlist is enabled. 
	void SetRepeatPlaylist( const bool enabled );

	// Gets whether crossfade is enabled.
	bool GetCrossfade() const;

	// Sets whether crossfade is enabled.
	void SetCrossfade( const bool enabled );

	// Called when 'mediaInfo' is updated.
	// Returns whether the currently playing item has changed.
	bool OnUpdatedMedia( const MediaInfo& mediaInfo );

	// Returns the currently selected playlist item (this can be different from the currently playing item).
	Playlist::Item GetCurrentSelectedPlaylistItem();

	// Sets the currently selected playlist item (this can be different from the currently playing item).
	void SetCurrentSelectedPlaylistItem( const Playlist::Item& item );

	// Returns all the file extensions supported by the handlers.
	std::set<std::wstring> GetAllSupportedFileExtensions() const;

	// Returns the available output devices.
	Devices GetDevices() const;

	// Returns the name of the current output device.
	std::wstring GetCurrentDevice() const;

	// Called when output settings are changed.
	void SettingsChanged();

	// Toggles whether to stop playback at the end of the current track.
	void ToggleStopAtTrackEnd();

	// Returns whether playback is to be stopped at the end of the current track.
	bool GetStopAtTrackEnd() const;

	// Toggles whether output is muted. 
	void ToggleMuted();

	// Returns whether output is muted.
	bool GetMuted() const;

	// Toggles whether to fade out the current track.
	void ToggleFadeOut();

	// Returns whether the current track is being faded out.
	bool GetFadeOut() const;

	// Toggles whether to fade into the next track.
	void ToggleFadeToNext();

	// Returns whether we are fading into the next track.
	bool GetFadeToNext() const;

	// Background thread handler for calculating the crossfade point for the current track.
	void OnCalculateCrossfadeHandler();

	// Returns the maximum pitch adjustment factor.
	float GetPitchRange() const;

	// Easter egg.
	void Bling( const int blingID );

private:
	// Output queue.
	typedef std::vector<Item> Queue;

	// Maps a playlist item ID to a ReplayGain estimate.
	typedef std::map<long,float> ReplayGainEstimateMap;

	// Maps an ID to a stream handle.
	typedef std::map<int,HSTREAM> StreamMap;

	// Initialises the BASS system;
	void InitialiseBass();

	// Estimates the ReplayGain for a playlist 'item' if necessary.
	void EstimateReplayGain( Playlist::Item& item );

	// Calculates the crossfade point for 'filename'.
	// 'seekOffset' - indicates the initial seek position of 'filename', in seconds.
	void CalculateCrossfadePoint( const std::wstring& filename, const float seekOffset = 0.0f );

	// Terminates the crossfade calculation thread.
	void StopCrossfadeThread();

	// Returns the crossfade position for the current track, in seconds.
	float GetCrossfadePosition() const;

	// Sets the crossfade 'position' for the current track, in seconds.
	void SetCrossfadePosition( const float position );

	// Applies ReplayGain to an to an output 'buffer' of 'bufferSize' in bytes, using 'item' information.
	void ApplyReplayGain( float* buffer, const long bufferSize, const Playlist::Item& item );

	// Gets the output queue.
	Queue GetOutputQueue();

	// Sets the output 'queue'.
	void SetOutputQueue( const Queue& queue );

	// Parent window handle.
	HWND m_Parent;

	// The available handlers.
	const Handlers& m_Handlers;

	// Application settings.
	Settings& m_Settings;

	// The current playlist.
	Playlist::Ptr m_Playlist;

	// The currently decoding playlist item.
	Playlist::Item m_CurrentItemDecoding;

	// The currently decoding stream.
	Decoder::Ptr m_DecoderStream;

	// The current BASS output stream.
	HSTREAM m_OutputStream;

	// Playlist mutex.
	std::mutex m_PlaylistMutex;

	// Output queue mutex.
	std::mutex m_QueueMutex;

	// Volume level in the range 0.0 (silent) to 1.0 (full volume).
	float m_Volume;

	// Pitch adjustment factor, with the default being 1.0 (no adjustment).
	float m_Pitch;

	// The list of output items with their start times in the output stream.
	Queue m_OutputQueue;

	// Playlist item to restart playback from, if stream playback has ended.
	Playlist::Item m_RestartItem;

	// Indicates whether random play is enabled.
	bool m_RandomPlay;

	// Indicates whether track repeat is enabled.
	bool m_RepeatTrack;

	// Indicates whether playlist repeat is enabled.
	bool m_RepeatPlaylist;

	// Indicates whether crossfade is enabled.
	bool m_Crossfade;

	// The currently selected playlist item (this can be different from the currently playing item).
	Playlist::Item m_CurrentSelectedPlaylistItem;

	// Indicates whether the default output device is being used.
	bool m_UsingDefaultDevice;

	// ReplayGain mode.
	Settings::ReplaygainMode m_ReplaygainMode;

	// ReplayGain preamp in dB.
	float m_ReplaygainPreamp;

	// ReplayGain hard limit flag.
	bool m_ReplaygainHardlimit;

	// Indicates whether to stop playback at the end of the current track.
	bool m_StopAtTrackEnd;

	// Indicates whether the output is muted.
	bool m_Muted;

	// Indicates whether the current track is being faded out.
	bool m_FadeOut;

	// Indicates whether we are fading into the next track.
	bool m_FadeToNext;

	// Indicates whether to switch to the next track when we are fading out.
	bool m_SwitchToNext;

	// Position in the output stream when fade out was started, in seconds.
	float m_FadeOutStartPosition;

	// Position of the last transition in the output stream, in seconds.
	float m_LastTransitionPosition;

	// Crossfade position for the current track, in seconds.
	std::atomic<float> m_CrossfadePosition;

	// Filename of the track for which to calculate the crossfade position.
	std::wstring m_CrossfadeFilename;

	// The thread for calculating crossfade position.
	HANDLE m_CrossfadeThread;

	// Event handle for terminating the crossfade calculation thread.
	HANDLE m_CrossfadeStopEvent;

	// The decoding stream that is being faded out during a crossfade.
	Decoder::Ptr m_CrossfadingStream;

	// The item that is being faded out during a crossfade.
	Playlist::Item m_CurrentItemCrossfading;

	// Indicates an offset to subtract from the crossfade calculation, in seconds.
	float m_CrossfadeSeekOffset;

	// ReplayGain estimates.
	ReplayGainEstimateMap m_ReplayGainEstimateMap;

	// Bling map.
	StreamMap m_BlingMap;
};
