#pragma once

#include "stdafx.h"

#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <vector>

// Folder monitor for file & folder changes.
class FolderMonitor
{
public:
	// 'hwnd' - window handle for device notifications.
	FolderMonitor( const HWND hwnd );

	virtual ~FolderMonitor();

	// Monitor event type.
	enum class Event {
		FolderRenamed,					// A folder has been renamed.
		FolderCreated,					// A folder has been created.
		FolderDeleted,					// A folder has been deleted.
		FileRenamed,						// A file has been renamed.
		FileCreated,						// A file has been created.
		FileDeleted,						// A file has been deleted.
	};

	// Event callback.
	// 'monitorEvent' - event type.
	// 'oldFilename' - the old file or folder name.
	// 'newFilename' - the new file or folder name (which will be identical to oldFilename for events other than FolderRenamed & FileRenamed).
	typedef std::function< void( const Event monitorEvent, const std::wstring& oldFilename, const std::wstring& newFilename ) > EventCallback;

	// Adds the folder to be monitored.
	// 'folder' - absolute folder file path.
	// 'callback' - event callback.
	// Returns whether monitoring has successfully started.
	bool AddFolder( const std::wstring folder, const EventCallback callback );

	// Removes the 'folder' from being monitored.
	void RemoveFolder( const std::wstring folder );

	// Stops all folders from being monitored.
	void RemoveAllFolders();

	// Called when a file system device 'handle' is being removed.
	void OnDeviceHandleRemoved( const HANDLE handle );

private:
	// Change type.
	enum class ChangeType {
		FolderChange,					// Folder changes.
		FileChange						// File changes.
	};

	// Maps a file time to a file name.
	typedef std::map<long long,std::wstring> AddFileMap;

	// Monitor information.
	struct MonitorInfo {
		std::wstring DirectoryPath;	// Directory path.
		HANDLE DirectoryHandle;			// Directory handle.
		HANDLE MonitorThreadHandle;	// Monitor thread handle.
		HANDLE AddFileThreadHandle;	// Add file thread handle.
		HANDLE CancelHandle;				// Cancel event handle.
		HDEVNOTIFY DevNotifyHandle;	// Device notification handle.
		EventCallback Callback;			// Event callback.
		ChangeType ChangeType;			// Change type.
		AddFileMap AddFileQueue;		// Add file queue.
		std::mutex AddFileMutex;		// Add file mutex.
	};

	// A list of monitors.
	typedef std::list<MonitorInfo*> Monitors;

	// Maps a folder to monitor information.
	typedef std::map<std::wstring,Monitors> FolderMap;

	// Monitor thread procedure.
	static DWORD WINAPI MonitorThreadProc( LPVOID lpParam );

	// Add file thread procedure.
	static DWORD WINAPI AddFileThreadProc( LPVOID lpParam );

	// Creates a monitor.
	// 'folder' - absolute folder file path.
	// 'callback' - event callback.
	// 'changeType' - change type.
	// Returns whether monitoring has successfully started.
	bool AddMonitor( const std::wstring folder, const EventCallback callback, const ChangeType changeType );

	// Deletes and releases all resources used by the 'monitor'.
	void CloseMonitor( MonitorInfo* monitor );

	// Window handle for device notifications.
	const HWND m_hWnd;

	// The information for the monitored folders.
	FolderMap m_Monitors;
};
