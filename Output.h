#pragma once

#include "stdafx.h"

#include "bass.h"
#include "Handlers.h"
#include "Resampler.h"
#include "Playlist.h"
#include "Settings.h"

#include <atomic>
#include <functional>
#include <optional>

// Message ID for signalling that playback needs to be restarted from a playlist item ID (wParam).
static const UINT MSG_RESTARTPLAYBACK = WM_APP + 191;

// Audio output
class Output
{
public:
	// 'instance' - module instance handle.
	// 'hwnd' - main window handle.
	// 'handlers' - the available handlers.
	// 'settings' - application settings.
	Output( const HINSTANCE instance, const HWND hwnd, Handlers& handlers, Settings& settings );

	virtual ~Output();

	// Output states
	enum class State {
		Stopped,
		Playing,
		Paused
	};

	// Output item information.
	struct Item {
		Playlist::Item PlaylistItem;  // Playlist item.
		double Position = 0;          // Position in seconds.
		double InitialSeek = 0;       // Initial seek time for the item.
		std::wstring StreamTitle;     // Current stream title.
	};

	// Maps a device ID to its description.
	using Devices = std::map<int, std::wstring>;

	// Callback function for when the output 'playlist' changes.
	using PlaylistChangeCallback = std::function<void( Playlist::Ptr playlist )>;

	// Callback function for when the 'itemID' should be selected (when using 'follow track selection' mode).
	using SelectFollowedTrackCallback = std::function<void( const long itemID )>;

	// Starts playback.
	// 'startID' - Playlist ID at which to start playback (or zero to start playback at the first playlist item).
	// 'seek' - Initial start position in seconds (negative value to seek relative to the end of the track).
	void Play( const long startID = 0, const double seek = 0 );

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

	// Sets the current 'playlist', and starts playback.
	// 'startID' - Playlist ID at which to start playback (or zero to start playback at the first playlist item).
	// 'seek' - Initial start position in seconds (negative value to seek relative to the end of the track).
	// 'pausedOnStartup' - Whether the application has been started in a paused state.
	void Play( const Playlist::Ptr playlist, const long startID = 0, const float seek = 0.0f );

	// Called on application startup to resume playback.
	// 'playlist' - Startup playlist.
	// 'startID' - Playlist ID with which to resume playback.
	// 'seek' - Initial start position in seconds.
	// 'paused' - Whether to start in a paused state.
	void Startup( const Playlist::Ptr playlist, const long startID, const float seek, const bool paused );

	// Gets the current playlist.
	Playlist::Ptr GetPlaylist();

	// Returns the current output state.
	State GetState();

	// Returns the currently playing item.
	Item GetCurrentPlaying();

	// Returns the volume level in the range 0.0 (silent) to 1.0 (full volume).
	float GetVolume() const;

	// Sets the volume level in the range 0.0 (silent) to 1.0 (full volume).
	void SetVolume( const float volume );

	// Returns the pitch adjustment factor, with 1.0 representing no adjustment.
	float GetPitch() const;

	// Sets the pitch adjustment factor, with 1.0 representing no adjustment.
	void SetPitch( const float pitch );

	// Returns the balance, with -1.0 representing full left, 0 representing central, +1.0 representing full right.
	float GetBalance() const;

	// Sets the balance, with -1.0 representing full left, 0 representing central, +1.0 representing full right.
	void SetBalance( const float balance );

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

	// Gets whether loudness normalisation is enabled.
	bool GetLoudnessNormalisation() const;

	// Sets whether loudness normalisation is enabled.
	void SetLoudnessNormalisation( const bool enabled );

	// Called when 'mediaInfo' is updated.
	// Returns whether the currently playing item has changed.
	bool OnUpdatedMedia( const MediaInfo& mediaInfo );

	// Returns the currently selected playlist item (this can be different from the currently playing item).
	Playlist::Item GetCurrentSelectedPlaylistItem();

	// Sets the currently selected playlist item (this can be different from the currently playing item).
	void SetCurrentSelectedPlaylistItem( const Playlist::Item& item );

	// Returns all the file extensions supported by the handlers, as a set of lowercase strings.
	std::set<std::wstring> GetAllSupportedFileExtensions() const;

	// Returns the available output devices.
	// Note that if there are any detected devices, an unnamed default device (with ID -1) will also be included.
	// 'mode' - output mode.
	Devices GetDevices( const Settings::OutputMode mode ) const;

	// Called when output settings are changed.
	void SettingsChanged();

	// Toggles whether to stop playback at the end of the current track.
	void ToggleStopAtTrackEnd();

	// Returns whether playback is to be stopped at the end of the current track.
	bool GetStopAtTrackEnd() const;

	// Toggles whether playback is to follow track selection.
	void ToggleFollowTrackSelection();

	// Returns whether playback is to follow track selection.
	bool GetFollowTrackSelection() const;

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

	// Returns the maximum pitch adjustment factor.
	float GetPitchRange() const;

	// Updates the EQ settings.
	void UpdateEQ( const Settings::EQ& eq );

	// Returns the available handlers.
	const Handlers& GetHandlers() const;

	// Sets the 'callback' function for when the output playlist changes.
	void SetPlaylistChangeCallback( PlaylistChangeCallback callback );

	// Sets the 'callback' function for when the selected playlist item changes (when using 'follow track selection' mode).
	void SetSelectFollowedTrackCallback( SelectFollowedTrackCallback callback );

	// Sets the current 'playlist' and 'selectedItems' to follow (used for 'follow track selection' mode).
	void SetPlaylistInformationToFollow( Playlist::Ptr playlist, const Playlist::Items& selectedItems );

private:
	// Output queue.
	using Queue = std::vector<Item>;

	// Maps a playlist item ID to a gain estimate.
	using GainEstimateMap = std::map<long, std::optional<float>>;

	// A list of FX.
	using FXList = std::list<HFX>;

	// Buffered output decoder shared pointer.
	using OutputDecoderPtr = std::shared_ptr<OutputDecoder>;

	// Preloaded decoder information.
	struct PreloadedDecoder {
		Playlist::Item itemToPreload = {};  // Item to preload.
		Playlist::Item item = {};           // Preloaded item.
		OutputDecoderPtr decoder = {};      // Preloaded decoder.
	};

	// BASS stream callback.
	static DWORD CALLBACK StreamProc( HSTREAM handle, void *buf, DWORD len, void *user );

	// BASS WASAPI callback.
	static DWORD CALLBACK WasapiProc( void *buffer, DWORD length, void *user );

	// BASS WASAPI notification callback.
	static void CALLBACK WasapiNotifyProc( DWORD notify, DWORD device, void *user );

	// BASS ASIO notification callback.
	static void CALLBACK AsioNotifyProc( DWORD notify, void *user );

	// BASS sync, called when playback has ended.
	static void CALLBACK SyncEnd( HSYNC handle, DWORD channel, DWORD data, void *user );

	// BASS sync, called when manually stopping playback.
	static void CALLBACK SyncSlideStop( HSYNC handle, DWORD channel, DWORD data, void *user );

	// Crossfade calculation thread procedure.
	static DWORD WINAPI CrossfadeThreadProc( LPVOID lpParam );

	// Loudness precalculation thread procedure.
	static DWORD WINAPI LoudnessPrecalcThreadProc( LPVOID lpParam );

	// Preload decoder thread procedure.
	static DWORD WINAPI PreloadDecoderProc( LPVOID lpParam );

	// Gets the current tick count.
	static LONGLONG GetTick();

	// Gets the interval between the start and end tick count, in seconds.
	static float GetInterval( const LONGLONG startTick, const LONGLONG endTick );

	// Returns whether a background thread can proceed, after an initial delay.
	// 'stopEvent' - the event reponsible for stopping the background thread.
	static bool CanBackgroundThreadProceed( const HANDLE stopEvent );

	// Reads sample data from the current decoder.
	// 'buffer' - sample buffer.
	// 'byteCount' - number of bytes to read.
	// 'handle' - stream handle.
	// Returns the number of bytes read.
	DWORD ReadSampleData( float* buffer, const DWORD byteCount, HSTREAM handle );

	// Called when playback has ended.
	void OnSyncEnd();

	// Background thread handler for calculating the crossfade point for the current track.
	void CalculateCrossfadeHandler();

	// Background thread handler for precalculating loudness values for tracks in the current playlist.
	void LoudnessPrecalcHandler();

	// Background thread handler for preloading the next decoder.
	void PreloadDecoderHandler();

	// Initialises the BASS system & resampler.
	void InitialiseOutput();

	// Estimates the gain for a playlist 'item' if necessary.
	void EstimateGain( Playlist::Item& item );

	// Starts the thread to calculation the crossfade point for the 'item'.
	// 'seekOffset' - indicates the initial seek position of 'item', in seconds.
	void StartCrossfadeCalculationThread( const Playlist::Item& item, const double seekOffset = 0 );

	// Terminates the crossfade calculation thread.
	void StopCrossfadeCalculationThread();

	// Calculates the crossfade position.
	double CalculateCrossfadePosition( Decoder::Ptr decoder, const double seekPosition, Decoder::CanContinue canContinue ) const;

	// Returns the crossfade position for the current track, in seconds.
	double GetCrossfadePosition() const;

	// Sets the crossfade 'position' for the current track, in seconds.
	void SetCrossfadePosition( const double position );

	// Applies gain (and EQ preamp) to an output 'buffer' containing 'sampleCount' samples, using 'item' information and 'softClipState'.
	void ApplyGain( float* buffer, const long sampleCount, const Playlist::Item& item, std::vector<float>& softClipState );

	// Gets the output queue.
	Queue GetOutputQueue();

	// Sets the output 'queue'.
	void SetOutputQueue( const Queue& queue );

	// Returns a decoder for the 'item' in the specified 'context' (and updates the item if necessary), or nullptr if a decoder could not be opened.
	Decoder::Ptr OpenDecoder( Playlist::Item& item, const Decoder::Context context );

	// Returns an output decoder for the 'item'.
	// 'usePreloadedDecoder' - whether to use the preloaded decoder (when available).
	OutputDecoderPtr OpenOutputDecoder( Playlist::Item& item, const bool usePreloadedDecoder = false );

	// Starts the output and returns the output state.
	State StartOutput();

	// Gets the current decoding position, in seconds.
	float GetDecodePosition() const;

	// Gets the current output position, in seconds.
	float GetOutputPosition() const;

	// Creates the BASS output stream (and mixer stream, if necessary) based on the 'mediaInfo' and the current output mode/device.
	// Returns whether the stream(s) were created successfully.
	bool CreateOutputStream( const MediaInfo& mediaInfo );

	// Updates the volume of the BASS output stream (or mixer stream, if necessary).
	void UpdateOutputVolume();

	// Initialises BASS ASIO output (if necessary), returning whether initialisation was successful.
	bool InitASIO();

	// Applies lead-in silence when starting playback.
	// 'buffer' - sample buffer.
	// 'byteCount' - sample buffer size, in bytes.
	// 'handle' - stream handle.
	// Returns the number of bytes that have been fed in to the start of the sample buffer.
	DWORD ApplyLeadIn( float* buffer, const DWORD byteCount, HSTREAM handle ) const;

	// Returns the fade out duration, in seconds.
	float GetFadeOutDuration() const;

	// Returns the fade to next duration, in seconds.
	float GetFadeToNextDuration() const;

	// Starts the loudness precalculation thread.
	void StartLoudnessPrecalcThread();

	// Stops the loudness precalculation thread.
	void StopLoudnessPrecalcThread();

	// Sets whether the output stream has finished.
	void SetOutputStreamFinished( const bool finished );

	// Returns the next decoder on from the 'item', or nullptr if a decoder could not be opened.
	// Updates 'item' with the next playlist item on success, resets 'item' on failure.
	OutputDecoderPtr GetNextDecoder( Playlist::Item& item );

	// Starts the preload decoder thread.
	void StartPreloadDecoderThread();

	// Stops the preload decoder thread.
	void StopPreloadDecoderThread();

	// Preloads the next decoder on from the current 'item'.
	void PreloadNextDecoder( const Playlist::Item& item );

	// Gets the stream title queue.
	std::vector<std::pair<float /*seconds*/, std::wstring /*title*/>> GetStreamTitleQueue();

	// Sets the stream title 'queue'.
	void SetStreamTitleQueue( const std::vector<std::pair<float /*seconds*/, std::wstring /*title*/>>& queue );

	// Sets the synchronizer which is called when the current output 'stream' ends.
	void SetEndSync( const HSTREAM stream );

	// Gets the current 'playlist' and 'selectedItems' to follow (used for 'follow track selection' mode).
	std::pair<Playlist::Ptr, Playlist::Items> GetPlaylistInformationToFollow();

	// Gets the next track to play, when the 'follow track selection' mode is active.
	// 'currentItem' - currently playing item.
	// 'previous' - whether to select the previous playlist item (if the currently playing item is already selected).
	// If the currently playing item is already selected, returns the next playlist item.
	// If the currently playing item is not selected, returns the next selected playlist item.
	// If no playlist items are selected, returns the playlist item following/preceding the currently playing item, or nullopt (if at the end of the playlist).
	// Returns the playlist & next track to play in the playlist, and whether the next track should be selected.
	std::optional<std::tuple<Playlist::Ptr, Playlist::Item, bool /*SelectTrack*/>> GetTrackToFollow( const Playlist::Item& currentItem, const bool previous = false );

	// Changes the current output 'playlist', returning whether the playist was changed.
	bool ChangePlaylist( const Playlist::Ptr& playlist );

	// Applies the latest WASAPI settings.
	void GetWASAPISettings();

	// Applies the latest ASIO settings.
	void GetASIOSettings();

	// Module instance handle.
	const HINSTANCE m_hInst;

	// Parent window handle.
	const HWND m_Parent;

	// The available handlers.
	Handlers& m_Handlers;

	// Application settings.
	Settings& m_Settings;

	// The current playlist.
	Playlist::Ptr m_Playlist;

	// The currently decoding playlist item.
	Playlist::Item m_CurrentItemDecoding;

	// The soft-clip state for the currently decoding item.
	std::vector<float> m_SoftClipStateDecoding;

	// The currently decoding stream.
	OutputDecoderPtr m_DecoderStream;

	// The sample rate of the currently decoding stream.
	long m_DecoderSampleRate;

	// The current BASS output stream (or source stream when using the mixer).
	HSTREAM m_OutputStream;

	// The current BASS mixer stream.
	HSTREAM m_MixerStream;

	// Playlist mutex.
	std::mutex m_PlaylistMutex;

	// Output queue mutex.
	std::mutex m_QueueMutex;

	// Volume level in the range 0.0 (silent) to 1.0 (full volume).
	float m_Volume;

	// Pitch adjustment factor, with the default being 1.0 (no adjustment).
	float m_Pitch;

	// Balance, with -1.0 representing full left, 0 representing central, +1.0 representing full right.
	float m_Balance;

	// The queue of output items, with their start times, in the output stream.
	Queue m_OutputQueue;

	// Playlist item ID to restart playback from, if stream playback has ended.
	long m_RestartItemID;

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

	// Gain mode.
	Settings::GainMode m_GainMode;

	// Limiter mode.
	Settings::LimitMode m_LimitMode;

	// Gain preamp in dB.
	float m_GainPreamp;

	// Indicates whether loudness normalisation is enabled.
	bool m_LoudnessNormalisation;

	// Indicates whether the 'stop at track end' setting should be reset when playback ends.
	bool m_RetainStopAtTrackEnd;

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
	double m_CrossfadePosition;

	// Item for which to calculate the crossfade position.
	Playlist::Item m_CrossfadeItem;

	// The thread for calculating crossfade position.
	HANDLE m_CrossfadeThread;

	// Event handle for terminating the crossfade calculation thread.
	HANDLE m_CrossfadeStopEvent;

	// The thread for loudness precalculation.
	HANDLE m_LoudnessPrecalcThread;

	// Event handle for terminating the loudness precalculation thread.
	HANDLE m_LoudnessPrecalcStopEvent;

	// The thread for preloading the next decoder.
	HANDLE m_PreloadDecoderThread;

	// Event handle for terminating the preload decoder thread.
	HANDLE m_PreloadDecoderStopEvent;

	// Event handle for waking the preload decoder thread.
	HANDLE m_PreloadDecoderWakeEvent;

	// The decoding stream that is being faded out during a crossfade.
	OutputDecoderPtr m_CrossfadingStream;

	// Crossfading stream mutex.
	std::mutex m_CrossfadingStreamMutex;

	// The item that is being faded out during a crossfade.
	Playlist::Item m_CurrentItemCrossfading;

	// The ID of the playlist item that is being faded out during a crossfade.
	std::atomic<long> m_CrossfadingItemID;

	// The soft-clip state for the currently crossfading item.
	std::vector<float> m_SoftClipStateCrossfading;

	// Indicates an offset to subtract from the crossfade calculation, in seconds.
	double m_CrossfadeSeekOffset;

	// Gain estimates.
	GainEstimateMap m_GainEstimateMap;

	// Current EQ settings.
	Settings::EQ m_CurrentEQ;

	// EQ FX handles.
	FXList m_FX;

	// Indicates whether EQ is enabled.
	bool m_EQEnabled;

	// EQ preamp in dB.
	float m_EQPreamp;

	// Current output mode.
	Settings::OutputMode m_OutputMode;

	// Current output device.
	std::wstring m_OutputDevice;

	// Indicates whether the current WASAPI device has failed or been disabled.
	std::atomic<bool> m_WASAPIFailed;

	// Indicates whether the current WASAPI device is paused.
	std::atomic<bool> m_WASAPIPaused;

	// Indicates whether ASIO should be reinitialised the next time playback is started.
	std::atomic<bool> m_ResetASIO;

	// Indicates whether the output stream has finished.
	std::atomic<bool> m_OutputStreamFinished;

	// Indicates whether an end synchronizer has been set on a mixer stream.
	std::atomic<bool> m_MixerStreamHasEndSync;

	// Whether playback should continue with the currently selected track, when the currently playing track has ended.
	std::atomic<bool> m_FollowTrackSelection;

	// WASAPI settings.
	std::atomic<bool> m_WASAPIUseMixFormat;
	std::atomic<int> m_WASAPIBufferMilliseconds;
	std::atomic<int> m_WASAPILeadInMilliseconds;

	// ASIO settings.
	std::atomic<bool> m_ASIOUseDefaultSamplerate;
	std::atomic<int> m_ASIODefaultSamplerate;
	std::atomic<int> m_ASIOLeadInMilliseconds;

	// Current playlist with selected items to follow (used for 'follow track selection' mode).
	std::pair<Playlist::Ptr, Playlist::Items> m_FollowPlaylistInformation;

	// Mutex for the current playlist with selected items to follow (used for 'follow track selection' mode).
	std::mutex m_FollowPlaylistInformationMutex;

	// When starting playback in non-standard output mode, the lead-in length before passing through actual sample data.
	float m_LeadInSeconds;

	// Indicates whether the application has been started in a paused state.
	bool m_PausedOnStartup;

	// A preloaded decoder, which can be used to minimize the delay when switching streams.
	PreloadedDecoder m_PreloadedDecoder;

	// A mutex for the preloaded decoder.
	std::mutex m_PreloadedDecoderMutex;

	// The queue of stream titles, associated with their start times.
	std::vector<std::pair<float /*seconds*/, std::wstring /*title*/>> m_StreamTitleQueue;

	// Stream title queue mutex.
	std::mutex m_StreamTitleMutex;

	// Callback function for when the output playlist changes.
	PlaylistChangeCallback m_OnPlaylistChangeCallback;

	// Callback function for when the selected playlist item changes (when using 'follow track selection' mode).
	SelectFollowedTrackCallback m_OnSelectFollowedTrackCallback;

	// Indicates whether the output stream should be resampled, and the samplerate & channels to use if so.
	std::optional<std::pair<uint32_t /*samplerate*/, uint32_t /*channels*/>> m_Resample;

	// Callback function for when the output decoder has finished pre-buffering.
	const OutputDecoder::PreBufferFinishedCallback m_OnPreBufferFinishedCallback;
};
