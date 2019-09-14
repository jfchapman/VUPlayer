#include "FolderMonitor.h"

#include "dbt.h"

DWORD WINAPI FolderMonitor::MonitorThreadProc( LPVOID lpParam )
{
	MonitorInfo* monitorInfo = reinterpret_cast<MonitorInfo*>( lpParam );
	if ( nullptr != monitorInfo ) {
		const DWORD bufferSize = 32768;
		std::vector<unsigned char> buffer( bufferSize );
		const BOOL watchSubtree = TRUE;
		const DWORD notifyFilter = ( ChangeType::FileChange == monitorInfo->ChangeType ) ? FILE_NOTIFY_CHANGE_FILE_NAME : FILE_NOTIFY_CHANGE_DIR_NAME;
		DWORD bytesReturned = 0;

		OVERLAPPED overlapped = {};
		overlapped.hEvent = CreateEvent( nullptr /*securityAttributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ );
		if ( nullptr != overlapped.hEvent ) {
			const HANDLE waitHandles[ 2 ] = { overlapped.hEvent, monitorInfo->CancelHandle };
			bool cancelled = false;

			while ( !cancelled ) {
				if ( ReadDirectoryChangesW( monitorInfo->DirectoryHandle, &buffer[ 0 ], bufferSize, watchSubtree, notifyFilter, &bytesReturned, &overlapped, nullptr /*completionRoutine*/ ) ) {
					if ( WAIT_OBJECT_0 == WaitForMultipleObjects( 2 /*count*/, waitHandles, FALSE /*waitAll*/, INFINITE ) ) {
						if ( GetOverlappedResult( monitorInfo->DirectoryHandle, &overlapped, &bytesReturned, FALSE /*wait*/ ) ) {
							unsigned char* pBuffer = &buffer[ 0 ];
							FILE_NOTIFY_INFORMATION* notifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>( pBuffer );
							while ( nullptr != notifyInfo ) {
								const std::wstring filename = monitorInfo->DirectoryPath + std::wstring( notifyInfo->FileName, notifyInfo->FileNameLength / 2 );

								switch ( notifyInfo->Action ) {
									case FILE_ACTION_ADDED : {
										const DWORD attributes = GetFileAttributes( filename.c_str() );
										if ( ( INVALID_FILE_ATTRIBUTES != attributes ) && !( FILE_ATTRIBUTE_HIDDEN & attributes ) && !( FILE_ATTRIBUTE_SYSTEM & attributes ) ) {
											if ( ChangeType::FileChange == monitorInfo->ChangeType ) {
												// Delay the callback until the newly created file can be read.
												FILETIME fileTime = {};
												GetSystemTimeAsFileTime( &fileTime );
												const long long addedTime = ( static_cast<long long>( fileTime.dwHighDateTime ) << 32 ) + fileTime.dwLowDateTime;
												std::lock_guard<std::mutex> lock( monitorInfo->AddFileMutex );
												monitorInfo->AddFileQueue.insert( AddFileMap::value_type( addedTime, filename ) );
											} else {
												monitorInfo->Callback( FolderMonitor::Event::FolderCreated, filename, filename );
											}
										}
										break;
									}
									case FILE_ACTION_REMOVED : {
										monitorInfo->Callback( ( ChangeType::FileChange == monitorInfo->ChangeType ) ? FolderMonitor::Event::FileDeleted : FolderMonitor::Event::FolderDeleted, filename, filename );
										break;
									}
									case FILE_ACTION_RENAMED_OLD_NAME : {
										if ( 0 != notifyInfo->NextEntryOffset ) {
											pBuffer += notifyInfo->NextEntryOffset;
											notifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>( pBuffer );
											if ( FILE_ACTION_RENAMED_NEW_NAME == notifyInfo->Action ) {
												const std::wstring newFilename = monitorInfo->DirectoryPath + std::wstring( notifyInfo->FileName, notifyInfo->FileNameLength / 2 );
												const DWORD attributes = GetFileAttributes( newFilename.c_str() );
												if ( ( INVALID_FILE_ATTRIBUTES != attributes ) && !( FILE_ATTRIBUTE_HIDDEN & attributes ) && !( FILE_ATTRIBUTE_SYSTEM & attributes ) ) {
													monitorInfo->Callback( ( ChangeType::FileChange == monitorInfo->ChangeType ) ? FolderMonitor::Event::FileRenamed : FolderMonitor::Event::FolderRenamed, filename, newFilename );
												}
											}
										}
										break;
									}
									default : {
										break;
									}
								}

								if ( 0 != notifyInfo->NextEntryOffset ) {
									pBuffer += notifyInfo->NextEntryOffset;
									notifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>( pBuffer );
								} else {
									notifyInfo = nullptr;
								}
							}
						}
					} else {
						cancelled = true;
					}
				} else if ( ERROR_NOTIFY_ENUM_DIR != GetLastError() ) {
					cancelled = true;
				}
			}

			CancelIoEx( monitorInfo->DirectoryHandle, &overlapped );
			GetOverlappedResult( monitorInfo->DirectoryHandle, &overlapped, &bytesReturned, TRUE /*wait*/ );

			CloseHandle( overlapped.hEvent );
		}
	}
	return 0;
}

DWORD WINAPI FolderMonitor::AddFileThreadProc( LPVOID lpParam )
{
	MonitorInfo* monitorInfo = reinterpret_cast<MonitorInfo*>( lpParam );
	if ( nullptr != monitorInfo ) {

		const DWORD retryInterval = 1000 /*msec*/;
		const long long maximumTimeInQueue = 5ll * 60ll /*sec*/ * 10000000ll /*100-nanosecond intervals*/;

		while ( WAIT_OBJECT_0 != WaitForSingleObject( monitorInfo->CancelHandle, retryInterval ) ) {
			FILETIME fileTime = {};
			GetSystemTimeAsFileTime( &fileTime );
			const long long currentTime = ( static_cast<long long>( fileTime.dwHighDateTime ) << 32 ) + fileTime.dwLowDateTime;

			std::lock_guard<std::mutex> lock( monitorInfo->AddFileMutex );
			auto fileIter = monitorInfo->AddFileQueue.begin();
			while ( monitorInfo->AddFileQueue.end() != fileIter ) {
				bool removeFromQueue = true;

				const std::wstring& filename = fileIter->second;
				const DWORD attributes = GetFileAttributes( filename.c_str() );
				if ( INVALID_FILE_ATTRIBUTES != attributes ) {
					const DWORD desiredAccess = GENERIC_READ;
					const DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
					const DWORD creationDisposition = OPEN_EXISTING;
					const DWORD flags = 0;
					const HANDLE fileHandle = CreateFile( filename.c_str(), desiredAccess, shareMode, nullptr /*securtyAttributes*/, creationDisposition, flags, nullptr /*template*/ );
					if ( INVALID_HANDLE_VALUE == fileHandle ) {
						const long long addedTime = fileIter->first;
						if ( ( currentTime - addedTime ) < maximumTimeInQueue ) {
							removeFromQueue = false;
						}
					} else {
						// File can now be read, so fire the callback.
						CloseHandle( fileHandle );
						monitorInfo->Callback( FolderMonitor::Event::FileCreated, filename, filename );
					}
				}

				if ( removeFromQueue ) {
					fileIter = monitorInfo->AddFileQueue.erase( fileIter );
				} else {
					++fileIter;
				}
			}
		}
	}
	return 0;
}

FolderMonitor::FolderMonitor( const HWND hwnd ) :
	m_hWnd( hwnd ),
	m_Monitors()
{
}

FolderMonitor::~FolderMonitor()
{
	RemoveAllFolders();
}

bool FolderMonitor::AddMonitor( const std::wstring folder, const EventCallback callback, const ChangeType changeType )
{
	bool success = false;
	if ( nullptr != callback ) {
		const DWORD desiredAccess = GENERIC_READ;
		const DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		const DWORD creationDisposition = OPEN_EXISTING;
		const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED;

		MonitorInfo* monitorInfo = new MonitorInfo();
		monitorInfo->ChangeType = changeType;
		monitorInfo->Callback = callback;
		monitorInfo->DirectoryPath = folder;
		if ( !monitorInfo->DirectoryPath.empty() && ( monitorInfo->DirectoryPath.back() != '\\' ) && ( monitorInfo->DirectoryPath.back() != '/' ) ) {
			monitorInfo->DirectoryPath += '\\';
		}
		monitorInfo->DirectoryHandle = CreateFile( monitorInfo->DirectoryPath.c_str(), desiredAccess, shareMode, nullptr /*securityAttributes*/, creationDisposition, flags, nullptr /*template*/ );
		if ( INVALID_HANDLE_VALUE != monitorInfo->DirectoryHandle ) {

			DEV_BROADCAST_HANDLE dev = {};
			dev.dbch_size = sizeof( DEV_BROADCAST_HANDLE );
			dev.dbch_devicetype = DBT_DEVTYP_HANDLE;
			dev.dbch_handle = monitorInfo->DirectoryHandle;
			monitorInfo->DevNotifyHandle = RegisterDeviceNotification( m_hWnd, &dev, DEVICE_NOTIFY_WINDOW_HANDLE );

			monitorInfo->CancelHandle = CreateEvent( nullptr /*securityAttributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ );
			if ( nullptr != monitorInfo->CancelHandle ) {
				monitorInfo->MonitorThreadHandle = CreateThread( nullptr /*attributes*/, 0 /*stackSize*/, &MonitorThreadProc, reinterpret_cast<LPVOID>( monitorInfo ) /*param*/, 0 /*flags*/, nullptr /*threadId*/ );
				monitorInfo->AddFileThreadHandle = CreateThread( nullptr /*attributes*/, 0 /*stackSize*/, &AddFileThreadProc, reinterpret_cast<LPVOID>( monitorInfo ) /*param*/, 0 /*flags*/, nullptr /*threadId*/ );
				if ( ( nullptr != monitorInfo->MonitorThreadHandle ) && ( nullptr != monitorInfo->AddFileThreadHandle ) ) {
					auto moniterIter = m_Monitors.insert( FolderMap::value_type( folder, Monitors() ) ).first;
					if ( m_Monitors.end() != moniterIter ) {
						moniterIter->second.push_back( monitorInfo );
						success = true;
					}
				}
			}
		}
		if ( !success ) {
			CloseMonitor( monitorInfo );
		}
	}
	return success;
}

void FolderMonitor::CloseMonitor( MonitorInfo* monitor )
{
	if ( nullptr != monitor ) {
		if ( nullptr != monitor->CancelHandle ) {
			SetEvent( monitor->CancelHandle );
		}

		if ( nullptr != monitor->MonitorThreadHandle ) {
			WaitForSingleObject( monitor->MonitorThreadHandle, INFINITE );
			CloseHandle( monitor->MonitorThreadHandle );
		}

		if ( nullptr != monitor->AddFileThreadHandle ) {
			WaitForSingleObject( monitor->AddFileThreadHandle, INFINITE );
			CloseHandle( monitor->AddFileThreadHandle );
		}

		if ( nullptr != monitor->DevNotifyHandle ) {
			UnregisterDeviceNotification( monitor->DevNotifyHandle );
		}

		if ( nullptr != monitor->CancelHandle ) {
			CloseHandle( monitor->CancelHandle );
		}

		if ( INVALID_HANDLE_VALUE != monitor->DirectoryHandle ) {
			CloseHandle( monitor->DirectoryHandle );
		}

		delete monitor;
	}
}

bool FolderMonitor::AddFolder( const std::wstring folder, const EventCallback callback )
{
	RemoveFolder( folder );
	const bool success = AddMonitor( folder, callback, ChangeType::FolderChange ) && AddMonitor( folder, callback, ChangeType::FileChange );
	return success;
}

void FolderMonitor::RemoveFolder( const std::wstring folder )
{
	auto monitorIter = m_Monitors.find( folder );
	if ( m_Monitors.end() != monitorIter ) {
		const Monitors& monitors = monitorIter->second;
		for ( const auto& monitor : monitors ) {
			CloseMonitor( monitor );
		}
		m_Monitors.erase( folder );
	}
}

void FolderMonitor::RemoveAllFolders()
{
	auto monitor = m_Monitors.begin();
	while ( m_Monitors.end() != monitor ) {
		RemoveFolder( monitor->first );
		monitor = m_Monitors.begin();
	}
}

void FolderMonitor::OnDeviceHandleRemoved( const HANDLE handle )
{
	auto folderIter = m_Monitors.begin();
	while ( m_Monitors.end() != folderIter ) {
		Monitors& monitors = folderIter->second;
		auto monitorIter = monitors.begin();
		while ( monitorIter != monitors.end() ) {
			MonitorInfo* monitorInfo = *monitorIter;
			if ( ( nullptr != monitorInfo ) && ( monitorInfo->DirectoryHandle == handle ) ) {
				CloseMonitor( monitorInfo );
				monitorIter = monitors.erase( monitorIter );
			} else {
				++monitorIter;
			}
		}
		if ( monitors.empty() ) {
			folderIter = m_Monitors.erase( folderIter );
		} else {
			++folderIter;
		}
	}
}
