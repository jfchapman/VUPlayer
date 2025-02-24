#include "ShellMetadata.h"
#ifndef _CONSOLE

#include "Utility.h"

constexpr wchar_t kShellTagExe[] = L"shelltag.exe";

std::optional<Tags> GetShellMetadata( const std::wstring& audioFilename )
{
	std::optional<Tags> tags;
	std::error_code ec;
	auto tagsFilename = std::filesystem::temp_directory_path( ec ) / UTF8ToWideString( GenerateGUIDString() );
	tagsFilename.replace_extension( L".json" );
	std::wstring commandLine = std::wstring( kShellTagExe ) + L" get \"" + audioFilename + L"\" \"" + tagsFilename.native() + L"\"";
	STARTUPINFO startupInfo = {};
	startupInfo.cb = sizeof( STARTUPINFO );
	PROCESS_INFORMATION processInfo = {};
	if ( CreateProcessW( nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startupInfo, &processInfo ) ) {
		WaitForSingleObject( processInfo.hProcess, INFINITE );
		if ( DWORD exitCode = 0; GetExitCodeProcess( processInfo.hProcess, &exitCode ) && ( kShellTagExitCodeOK == exitCode ) ) {
			tags = TagsFromJSON( tagsFilename );
		}
		CloseHandle( processInfo.hProcess );
		CloseHandle( processInfo.hThread );
	}
	std::filesystem::remove( tagsFilename, ec );
	return tags;
}

bool SetShellMetadata( const std::wstring& audioFilename, const Tags& tags )
{
	bool success = false;
	std::error_code ec;
	auto tagsFilename = std::filesystem::temp_directory_path( ec ) / UTF8ToWideString( GenerateGUIDString() );
	tagsFilename.replace_extension( L".json" );
	if ( TagsToJSON( tags, tagsFilename ) ) {
		constexpr int kMaxRetries = 5;
		for ( int retryCount = 0; retryCount < kMaxRetries; retryCount++ ) {
			std::wstring commandLine = std::wstring( kShellTagExe ) + L" set \"" + audioFilename + L"\" \"" + tagsFilename.native() + L"\"";
			STARTUPINFO startupInfo = {};
			startupInfo.cb = sizeof( STARTUPINFO );
			PROCESS_INFORMATION processInfo = {};
			if ( CreateProcessW( nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startupInfo, &processInfo ) ) {
				WaitForSingleObject( processInfo.hProcess, INFINITE );
				DWORD exitCode = 0;
				const bool gotExitCode = GetExitCodeProcess( processInfo.hProcess, &exitCode );
				CloseHandle( processInfo.hProcess );
				CloseHandle( processInfo.hThread );
				// Try again if the shelltag process fails, or if there was a sharing violation when writing to the file, otherwise we are done.
				if ( gotExitCode && ( ( kShellTagExitCodeOK == exitCode ) || ( kShellTagExitCodeFail == exitCode ) ) ) {
					success = ( kShellTagExitCodeOK == exitCode );
					break;
				}
			} else {
				break;
			}
		}
	}
	std::filesystem::remove( tagsFilename, ec );
	return success;
}
#endif
