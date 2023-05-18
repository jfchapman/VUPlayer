#pragma once

#include "stdafx.h"

#include "Handlers.h"
#include "Settings.h"

#include <atomic>

class WndTaskbar;

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
	Converter( const HINSTANCE instance, const HWND hwnd, Library& library, Settings& settings, Handlers& handlers, const Playlist::Items& tracks, const Handler::Ptr encoderHandler, const std::wstring& joinFilename, WndTaskbar& taskbar );

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

	// Returns whether conversion has been cancelled.
	bool Cancelled() const;

	// Returns the output filename for 'mediaInfo'.
	std::wstring GetOutputFilename( const MediaInfo& mediaInfo ) const;

	// Updates the status of the progress bars.
	void UpdateStatus();

	// Writes track tags to 'filename' based on the 'mediaInfo'.
	void WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Writes album tags to 'filename' based on the 'mediaInfo'.
	void WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Returns a decoder for the 'item', or nullptr if a decoder could not be opened.
	Decoder::Ptr OpenDecoder( const Playlist::Item& item ) const;

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
	const Playlist::Items m_Tracks;

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

	// The total number of tracks to convert.
	long m_TrackCount;

	// The encoder handler to use.
	Handler::Ptr m_EncoderHandler;

	// The encoder to use.
	Encoder::Ptr m_Encoder;

	// The encoder settings to use.
	std::string m_EncoderSettings;

	// The output filename, when joining tracks into a single file.
	std::wstring m_JoinFilename;
};
