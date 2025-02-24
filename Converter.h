#pragma once

#include "stdafx.h"

#include "Handlers.h"
#include "Settings.h"

#include <atomic>

class WndTaskbar;

// Maximum number of encoder threads.
constexpr size_t kEncoderThreadLimit = 32;

// Audio file converter.
class Converter
{
public:
	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'library' - media library.
	// 'settings' - application settings.
	// 'handlers' - audio format handlers.
	// 'tracks' - tracks to convert.
	// 'encoderHandler' - encoder handler to use.
	// 'joinFilename' - output filename, when joining tracks into a single file.
	// 'taskbar' - taskbar control.
	Converter( const HINSTANCE instance, const HWND hwnd, Library& library, Settings& settings, Handlers& handlers, Playlist::Items& tracks, const Handler::Ptr encoderHandler, const std::wstring& joinFilename, WndTaskbar& taskbar );

	virtual ~Converter();

private:
	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Encode thread procedure.
	static DWORD WINAPI EncodeThreadProc( LPVOID lpParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Closes the dialog.
	void Close();

	// Called when a conversion error has occurred.
	void Error();

	// Encode thread handler.
	void EncodeHandler();

	// Handler which converts tracks into a single file.
	// 'addToLibrary' - whether to add the encoded track to the media library.
	// Returns true if conversion completed OK, false otherwise
	bool JoinTracksHandler( const bool addToLibrary );

	// Handler which converts individual tracks.
	// 'addToLibrary' - whether to add the encoded track to the media library.
	// Returns true if conversion completed OK, false otherwise
	bool EncodeTracksHandler( const bool addToLibrary );

	// Returns whether conversion has been cancelled.
	bool Cancelled() const;

	// Updates the status of the progress bars.
	void UpdateStatus();

	// Writes track tags to 'filename' based on the 'mediaInfo'.
	void WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Writes album tags to 'filename' based on the 'mediaInfo'.
	void WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Returns a decoder for the 'item', or nullptr if a decoder could not be opened.
	Decoder::Ptr OpenDecoder( const Playlist::Item& item ) const;

	// Returns a unique output filename for the encoded 'track', using (and updating) the set of 'usedFilenames'. Returns nullopt if a filename could not be created.
	std::optional<std::wstring> GetOutputFilename( const Playlist::Item& track, std::set<std::wstring>& usedFilenames );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// Media library.
	Library& m_Library;

	// Application settings.
	Settings& m_Settings;

	// Audio format handlers.
	Handlers& m_Handlers;

	// Taskbar control.
	WndTaskbar& m_Taskbar;

	// Tracks to convert.
	Playlist::Items& m_Tracks;

	// Cancel event handle.
	HANDLE m_CancelEvent;

	// Encode thread handle.
	HANDLE m_EncodeThread;

	// The current track being converted.
	std::atomic<long> m_StatusTrack;

	// Track progress, in the range 0.0 to 1.0.
	std::atomic<float> m_ProgressTrack;

	// Total progress, in the range 0.0 to 1.0.
	std::atomic<float> m_ProgressTotal;

	// Progress bar range.
	long m_ProgressRange;

	// The currently displayed track status.
	long m_DisplayedTrack;

	// The encoder handler to use.
	Handler::Ptr m_EncoderHandler;

	// The encoder settings to use.
	std::string m_EncoderSettings;

	// The output filename, when joining tracks into a single file.
	std::wstring m_JoinFilename;

	// The (maximum) number of encoding threads to use.
	const uint32_t m_EncoderThreadCount;

	// Encoder progress bars, and the encoder progress in the range 0.0 to 1.0, when using multiple encoding threads.
	std::array<std::pair<HWND, std::atomic<float>>, kEncoderThreadLimit> m_EncoderProgressBars;

	// Encoder progress bar range, when using multiple encoding threads.
	long m_EncoderProgressRange = 0;
};
