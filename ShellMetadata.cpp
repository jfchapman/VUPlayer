#include "ShellMetadata.h"

#ifndef _CONSOLE // Functionality that is not required by the shelltag console application

#include "Utility.h"

constexpr wchar_t kShellTagExe[] = L"shelltag.exe";

// JPEG quality when converting artwork for the shell property handlers.
constexpr ULONG kArtworkJPEGQuality = 90;

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
	// The shell property handlers (for MP3, M4A & WMA files at least) only store JPEG artwork, and convert any other image format to JPEG themselves, discarding any transparency.
	// So convert non-JPEG artwork here instead, which draws any transparent areas over a white background, and uses a known quality setting.
	Tags shellTags = tags;
	if ( const auto artwork = shellTags.find( Tag::Artwork ); shellTags.end() != artwork ) {
		if ( const std::vector<BYTE> image = Base64Decode( artwork->second ); !image.empty() && !IsJPEGImage( image ) ) {
			if ( const std::vector<BYTE> jpeg = ConvertImageToJPEG( image, kArtworkJPEGQuality ); !jpeg.empty() ) {
				artwork->second = Base64Encode( jpeg.data(), static_cast<int>( jpeg.size() ) );
			} else {
				shellTags.erase( artwork );
			}
		}
	}

	bool success = false;
	std::error_code ec;
	auto tagsFilename = std::filesystem::temp_directory_path( ec ) / UTF8ToWideString( GenerateGUIDString() );
	tagsFilename.replace_extension( L".json" );
	if ( TagsToJSON( shellTags, tagsFilename ) ) {
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

#endif // !_CONSOLE
