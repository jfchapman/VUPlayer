#pragma once

#include <atomic>
#include <filesystem>

#include "Library.h"

// Library maintainer.
class LibraryMaintainer
{
public:
	// 'instance' - module instance handle.
	// 'library' - media library.
	// 'handlers' - available handlers.
	LibraryMaintainer( const HINSTANCE instance, Library& library, Handlers& handlers );

	virtual ~LibraryMaintainer();

	// A callback for when a new 'file' is added to the library. 
	using FileAddedCallback = std::function<void( const std::filesystem::path& file )>;

	// A callback for when library maintenance has finished.
	// 'removedFiles' - all files that have been removed from the library.
	using FinishedCallback = std::function<void( const MediaInfo::List& removedFiles )>;

	// Starts library maintenance.
	// 'fileAddedCallback' - callback for when a new file is added to the library.
	// 'finishedCallback' - callback for when library maintenance has finished.
	void Start( FileAddedCallback fileAddedCallback, FinishedCallback finishedCallback );

	// Stops library maintenance.
	void Stop();

	// Returns whether the maintainer is currently active.
	bool IsActive() const;

	// Returns the current status.
	std::wstring GetStatus() const;

private:
	// Thread procedure.
	static DWORD WINAPI MaintainerThreadProc( LPVOID lpParam );

	// Truncates the 'path' for display purposes.
	static std::wstring TruncatePath( const std::filesystem::path& path );

	// Maintenance thread handler.
	void Handler();

	// Returns the root drive names.
	std::set<std::wstring> GetRootDrives();

	// Recursively scans the 'folder' and adds any supported file types to 'mediaFiles'.
	void ScanFolder( const std::filesystem::path& folder, std::set<std::filesystem::path>& mediaFiles );

	// Returns whether the 'filename' is a supported media file type.
	bool IsSupportedFileType( const std::wstring& filename ) const;

	// Sets the current 'status'.
	void SetStatus( const std::wstring& status );

	// Media library.
	Library& m_Library;

	// Stop event handle.
	HANDLE m_StopEvent;

	// Thread handle.
	HANDLE m_Thread;

	// Current status.
	std::wstring m_Status;

	// Status mutex.
	mutable std::mutex m_StatusMutex;

	// Supported media file types.
	std::set<std::wstring> m_SupportedFileExtensions;

	// Status text for scanning the computer.
	std::wstring m_StatusScanningComputer;

	// Status text for updating the library.
	std::wstring m_StatusUpdatingLibrary;

	// A callback for when a new file is added to the library. 
	FileAddedCallback m_FileAddedCallback;

	// A callback for when library maintenance has finished.
	FinishedCallback m_FinishedCallback;
};
