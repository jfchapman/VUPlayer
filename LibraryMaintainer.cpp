#include "LibraryMaintainer.h"

#include "Utility.h"
#include "VUPlayer.h"

DWORD WINAPI LibraryMaintainer::MaintainerThreadProc( LPVOID lpParam )
{
	LibraryMaintainer* maintainer = static_cast<LibraryMaintainer*>( lpParam );
	if ( nullptr != maintainer ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		maintainer->Handler();
		CoUninitialize();
	}
	return 0;
}

LibraryMaintainer::LibraryMaintainer( const HINSTANCE instance, Library& library, Handlers& handlers ) :
	m_Library( library ),
	m_SupportedFileExtensions(),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Thread( nullptr ),
	m_Status(),
	m_StatusMutex(),
	m_StatusScanningComputer(),
	m_StatusUpdatingLibrary(),
	m_FileAddedCallback( nullptr ),
	m_FinishedCallback( nullptr )
{
	const int bufSize = 64;
	WCHAR buf[ bufSize ] = {};
	LoadString( instance, IDS_MAINTAINER_SCANNINGCOMPUTER, buf, bufSize );
	m_StatusScanningComputer = buf;
	LoadString( instance, IDS_MAINTAINER_UPDATINGLIBRARY, buf, bufSize );
	m_StatusUpdatingLibrary = buf;

	const auto filetypes = handlers.GetAllSupportedFileExtensions();
	for ( const auto& filetype : filetypes ) {
		m_SupportedFileExtensions.insert( WideStringToLower( filetype ) );
	}
}

LibraryMaintainer::~LibraryMaintainer()
{
	Stop();
	CloseHandle( m_StopEvent );
}

void LibraryMaintainer::Start( FileAddedCallback fileAddedCallback, FinishedCallback finishedCallback )
{
	Stop();
	m_FileAddedCallback = fileAddedCallback;
	m_FinishedCallback = finishedCallback;
	m_Thread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, MaintainerThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	if ( nullptr != m_Thread ) {
		SetThreadPriority( m_Thread, THREAD_MODE_BACKGROUND_BEGIN );
	}
}

void LibraryMaintainer::Stop()
{
	if ( nullptr != m_Thread ) {
		SetThreadPriority( m_Thread, THREAD_MODE_BACKGROUND_END );
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_Thread, INFINITE );
		CloseHandle( m_Thread );
		m_Thread = nullptr;
	}
	ResetEvent( m_StopEvent );
	SetStatus( {} );
	m_FileAddedCallback = nullptr;
	m_FinishedCallback = nullptr;
}

bool LibraryMaintainer::IsActive() const
{
	return ( nullptr != m_Thread ) && ( WAIT_OBJECT_0 != WaitForSingleObject( m_Thread, 0 ) );
}

std::wstring LibraryMaintainer::GetStatus() const
{
	std::lock_guard<std::mutex> lock( m_StatusMutex );
	return m_Status;
}

void LibraryMaintainer::SetStatus( const std::wstring& status )
{
	std::lock_guard<std::mutex> lock( m_StatusMutex );
	m_Status = status;
}

void LibraryMaintainer::Handler()
{
	std::set<std::filesystem::path> allFiles;

	// Scan all drives for supported file types.
	std::wstring initialStatus = m_StatusScanningComputer;
	WideStringReplace( initialStatus, L"%", std::to_wstring( 0 ) );
	SetStatus( initialStatus );
	const auto drives = GetRootDrives();
	for ( const auto& drive : drives ) {
		ScanFolder( drive, allFiles );
	}

	if ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) {
		// Make a note of existing library files (excluding streams), and merge in any that were not found in the folder scan.
		std::set<std::filesystem::path> existingFiles;
		const auto allMedia = m_Library.GetAllMedia();
		for ( const auto& mediaInfo : allMedia ) {
			if ( const auto& filename = mediaInfo.GetFilename(); !IsURL( filename ) ) {
				existingFiles.insert( mediaInfo.GetFilename() );
				allFiles.insert( filename );
			}
		}

		// Make a note of all files that are removed from the library.
		MediaInfo::List removedFiles;

		// Refresh library information for all the files.
		if ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) {
			size_t current = 0;
			size_t total = allFiles.size();
			for ( auto path = allFiles.begin(); ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) && ( allFiles.end() != path ); ++path ) {
				std::wstring status = m_StatusUpdatingLibrary;
				WideStringReplace( status, L"%1", std::to_wstring( ++current ) );
				WideStringReplace( status, L"%2", std::to_wstring( total ) );
				status += L" - " + TruncatePath( *path );
				SetStatus( status );

				MediaInfo mediaInfo( path->c_str() );
				if ( m_Library.GetMediaInfo( mediaInfo, true /*scanMedia*/, true /*sendNotification*/, true /*removeMissing*/ ) ) {
					if ( ( nullptr != m_FileAddedCallback ) && ( existingFiles.end() == existingFiles.find( *path ) ) ) {
						m_FileAddedCallback( *path );
					}
				} else {
					removedFiles.push_back( mediaInfo );
				}
			}
			if ( nullptr != m_FinishedCallback ) {
				m_FinishedCallback( removedFiles );
			}
		}
	}

	SetStatus( {} );
}

std::set<std::wstring> LibraryMaintainer::GetRootDrives()
{
	std::set<std::wstring> drives;
	const DWORD bufferLength = GetLogicalDriveStrings( 0 /*bufferLength*/, nullptr /*buffer*/ );
	if ( bufferLength > 0 ) {
		std::vector<WCHAR> buffer( static_cast<size_t>( bufferLength ) );
		if ( GetLogicalDriveStrings( bufferLength, buffer.data() ) <= bufferLength ) {
			LPWSTR driveString = buffer.data();
			size_t stringLength = wcslen( driveString );
			while ( stringLength > 0 ) {
				const UINT driveType = GetDriveType( driveString );
				if ( ( DRIVE_FIXED == driveType ) || ( DRIVE_REMOVABLE == driveType ) || ( DRIVE_REMOTE == driveType ) ) {
					drives.insert( driveString );
				}
				driveString += stringLength + 1;
				stringLength = wcslen( driveString );
			}
		}
	}
	return drives;
}

void LibraryMaintainer::ScanFolder( const std::filesystem::path& folder, std::set<std::filesystem::path>& mediaFiles )
{
	const FINDEX_INFO_LEVELS levels = FindExInfoBasic;
	const FINDEX_SEARCH_OPS searchOp = FindExSearchNameMatch;
	const DWORD flags = FIND_FIRST_EX_LARGE_FETCH;
	WIN32_FIND_DATA findData = {};
	std::filesystem::path path = folder / L"*.*";
	const HANDLE handle = FindFirstFileEx( path.c_str(), levels, &findData, searchOp, nullptr /*filter*/, flags );
	if ( INVALID_HANDLE_VALUE != handle ) {
		BOOL found = TRUE;
		while ( found && ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) ) ) {
			if ( !( ( findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) || ( findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ) ) ) {
				if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
					if ( ( findData.cFileName[ 0 ] != '.' ) ) {
						path = folder / findData.cFileName;
						ScanFolder( path, mediaFiles );
					}
				} else if ( IsSupportedFileType( findData.cFileName ) ) {
					path = folder / findData.cFileName;
					mediaFiles.insert( path );

					std::wstring status = m_StatusScanningComputer;
					WideStringReplace( status, L"%", std::to_wstring( mediaFiles.size() ) );
					status += L" - " + TruncatePath( path );
					SetStatus( status );
				}
			}
			found = FindNextFile( handle, &findData );
		}
		FindClose( handle );
	}
}

bool LibraryMaintainer::IsSupportedFileType( const std::wstring& filename ) const
{
	return m_SupportedFileExtensions.end() != m_SupportedFileExtensions.find( GetFileExtension( filename ) );
}

std::wstring LibraryMaintainer::TruncatePath( const std::filesystem::path& path )
{
	static constexpr size_t maxPathLength = 50;
	std::wstring result = path;
	if ( result.size() > maxPathLength ) {
		std::vector<size_t> delimiterPositions = {};
		size_t pos = result.find( '\\' );
		while ( std::wstring::npos != pos ) {
			delimiterPositions.push_back( pos );
			pos = result.find( '\\', 1 + pos );
		}
		for ( const auto& delimiterPos : delimiterPositions ) {
			const size_t truncatedSize = result.size() - delimiterPos;
			if ( ( truncatedSize <= maxPathLength ) || ( delimiterPos == delimiterPositions.back() ) ) {
				result = result.substr( 0, 1 + delimiterPositions[ 0 ] ) + L"..." + result.substr( delimiterPos );
				break;
			}
		}
	}
	return result;
}
