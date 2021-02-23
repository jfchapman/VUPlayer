#pragma once

#include "stdafx.h"

#include "CDDAMedia.h"
#include "Handler.h"

#include <map>
#include <set>
#include <string>

// CD audio manager.
class CDDAManager
{
public:
	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'library' - media library.
	// 'handlers' - available handlers.
	// 'musicbrainz' - MusicBrainz manager.
	CDDAManager( const HINSTANCE instance, const HWND hwnd, Library& library, Handlers& handlers, MusicBrainz& musicbrainz );

	virtual ~CDDAManager();

	// CD audio media information, mapped by drive path.
	typedef std::map<wchar_t,CDDAMedia> CDDAMediaMap;

	// Returns the CD-ROM drive letters.
	static std::set<wchar_t> GetCDROMDrives();

	// Returns the available CD audio media.
	CDDAMediaMap GetCDDADrives();

	// Called when a piece of media has been added or removed from the system.
	void OnDeviceChange();

private:
	// Media update thread start procedure.
	// 'lParam' - thread parameter.
	static DWORD WINAPI UpdateThreadProc( LPVOID lParam );

	// Starts the media update thread.
	void StartUpdateThread();

	// Stops the media update thread.
	void StopUpdateThread();

	// Media update thread handler.
	void UpdateThreadHandler();

	// Media library.
	Library& m_Library;

	// MusicBrainz manager.
	MusicBrainz& m_MusicBrainz;

	// Available CD audio media.
	CDDAMediaMap m_Media;

	// Media mutex.
	std::mutex m_MediaMutex;

	// Media update thread handle.
	HANDLE m_UpdateThread;

	// Event handle with which to stop the media update thread.
	HANDLE m_StopEvent;

	// Event handle with which to wake the media update thread.
	HANDLE m_WakeEvent;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle to notify when the available CD audio media has changed.
	HWND m_hWnd;
};
