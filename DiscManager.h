#pragma once

#include "stdafx.h"

#include "CDDAMedia.h"
#include "Handler.h"

#include <map>
#include <optional>
#include <set>
#include <string>

// Optical disc manager
class DiscManager
{
public:
	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'library' - media library.
	// 'handlers' - available handlers.
	// 'musicbrainz' - MusicBrainz manager.
	DiscManager( const HINSTANCE instance, const HWND hwnd, Library& library, Handlers& handlers, MusicBrainz& musicbrainz );

	virtual ~DiscManager();

	// Device change type.
	enum class ChangeType { Added, Removed };

	// CD audio media information, mapped by drive letter.
	using CDDAMediaMap = std::map<wchar_t, CDDAMedia>;

	// Data media name, mapped by drive letter.
	using DataMediaMap = std::map<wchar_t, std::wstring>;

	// Returns the CD-ROM drive letters.
	static std::set<wchar_t> GetCDROMDrives();

	// Returns the available drives which contain CD audio media.
	CDDAMediaMap GetCDDADrives() const;

	// Returns the available drives which contain data media.
	DataMediaMap GetDataDrives() const;

	// Called when a piece of media has been added or removed.
	// 'drive' - drive letter of the device.
	// 'change' - change type.
	void OnDeviceChange( const wchar_t drive, const ChangeType change );

private:
	// Device changes, mapped by drive letter.
	using ChangeMap = std::map<wchar_t, ChangeType>;

	// Media update thread start procedure.
	// 'lParam' - thread parameter.
	static DWORD WINAPI UpdateThreadProc( LPVOID lParam );

	// Starts the media update thread.
	void StartUpdateThread();

	// Stops the media update thread.
	void StopUpdateThread();

	// Media update thread handler.
	void UpdateThreadHandler();

	// Returns the CD audio media for the 'drive' letter, or null if there is no media.
	std::optional<CDDAMedia> GetCDDAMedia( const wchar_t drive ) const;

	// Returns the data media name for the 'drive' letter, or null if there is no media.
	std::optional<std::wstring> GetDataMedia( const wchar_t drive ) const;

	// Media library.
	Library& m_Library;

	// MusicBrainz manager.
	MusicBrainz& m_MusicBrainz;

	// Available CD audio media.
	CDDAMediaMap m_CDDAMedia;

	// Available data media.
	DataMediaMap m_DataMedia;

	// Device changes.
	ChangeMap m_ChangeMap;

	// Media mutex.
	mutable std::mutex m_MediaMutex;

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
