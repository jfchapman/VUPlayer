#pragma once

#include "stdafx.h"

#include "CDDAManager.h"
#include "Handlers.h"
#include "Settings.h"

#include <atomic>

// CD audio extractor.
class CDDAExtract
{
public:
	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'library' - media library.
	// 'settings' - application settings.
	// 'handlers' - audio format handlers.
	// 'cddaManager' - CD audio manager.
	// 'tracks' - tracks to extract.
	CDDAExtract( const HINSTANCE instance, const HWND hwnd, Library& library, Settings& settings, Handlers& handlers, CDDAManager& cddaManager, const MediaInfo::List& tracks );

	virtual ~CDDAExtract();

private:
	// CD audio data.
	typedef std::shared_ptr<CDDAMedia::Data> DataPtr;

	// Maps media information to CD audio data.
	typedef std::map<MediaInfo,DataPtr> MediaData;

	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Read thread procedure.
	static DWORD WINAPI ReadThreadProc( LPVOID lpParam );

	// Encode thread procedure.
	static DWORD WINAPI EncodeThreadProc( LPVOID lpParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Closes the dialog.
	void Close();

	// Read thread handler.
	void ReadHandler();

	// Encode thread handler.
	void EncodeHandler();

	// Returns whether extraction has been cancelled.
	bool Cancelled() const;

	// Returns the output filename for 'mediaInfo'.
	std::wstring GetOutputFilename( const MediaInfo& mediaInfo ) const;

	// Updates the status of the progress bars.
	void UpdateStatus();

	// Called if there was an error during the extraction process.
	void Error();

	// Writes track specific tags to 'filename' based on the 'mediaInfo'.
	void WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Writes album specific tags to 'filename' based on the 'mediaInfo'.
	void WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle to notify when the available CD audio media has changed.
	HWND m_hWnd;

	// Media library.
	Library& m_Library;

	// Application settings.
	Settings& m_Settings;

	// Audio format handlers.
	Handlers& m_Handlers;

	// CD audio manager.
	CDDAManager& m_CDDAManager;

	// Tracks to extract.
	const MediaInfo::List m_Tracks;

	// Cancel event handle.
	HANDLE m_CancelEvent;

	// Pending encode event handle.
	HANDLE m_PendingEncodeEvent;

	// Read thread handle.
	HANDLE m_ReadThread;

	// Encode thread handle.
	HANDLE m_EncodeThread;

	// Pending tracks to encode.
	MediaData m_PendingEncode;

	// Pending tracks mutex.
	std::mutex m_PendingEncodeMutex;

	// Read track status.
	std::atomic<long> m_StatusTrack;

	// Read pass status.
	std::atomic<long> m_StatusPass;

	// Read progress, in the range 0.0 to 1.0.
	std::atomic<float> m_ProgressRead;

	// Encode progress, in the range 0.0 to 1.0.
	std::atomic<float> m_ProgressEncode;

	// Progress bar range.
	long m_ProgressRange;

	// The currently displayed read track status.
	long m_DisplayedTrack;

	// The currently displayed read pass status.
	long m_DisplayedPass;

	// The total number of samples to extract.
	long m_TotalSamples;
};
