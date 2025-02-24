// A console wrapper to isolate the main application from Windows Shell shenanigans.
// Usage: shelltag.exe get|set audio_filename tags_filename

#include "ShellTag.h"
#include "ShellMetadata.h"
#include "Utility.h"

int wmain( int argc, wchar_t *argv[] )
{
	int exitCode = kShellTagExitCodeFail;
	SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX );
	if ( SUCCEEDED( CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED ) ) ) {
		if ( ( 4 == argc ) && ( nullptr != argv[ 1 ] ) && ( nullptr != argv[ 2 ] ) && ( nullptr != argv[ 3 ] ) ) {
			if ( const auto command = WideStringToLower( argv[ 1 ] ); ( L"get" == command ) || ( L"set" == command ) ) {
				const std::filesystem::path audioFilename( argv[ 2 ] );
				const std::filesystem::path tagsFilename( argv[ 3 ] );
				std::error_code ec;
				if ( std::filesystem::exists( audioFilename, ec ) ) {
					if ( L"set" == command ) {
						if ( std::filesystem::exists( tagsFilename, ec ) ) {
							const auto tags = TagsFromJSON( tagsFilename );
							if ( tags ) {
								exitCode = ShellTag::Set( audioFilename, *tags );
							}
						}
					} else if ( !tagsFilename.empty() ) {
						Tags tags;
						if ( kShellTagExitCodeOK == ShellTag::Get( audioFilename, tags ) ) {
							exitCode = TagsToJSON( tags, tagsFilename ) ? kShellTagExitCodeOK : kShellTagExitCodeFail;
						}
					}
				}
			}
		}
		CoUninitialize();
	}
	return exitCode;
}
