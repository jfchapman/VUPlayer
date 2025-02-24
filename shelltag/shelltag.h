#pragma once

#include "Tag.h"

#include <Windows.h>
#include <Shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <mfapi.h>

#include <mutex>
#include <string>

class ShellTag
{
public:
	// Gets metadata.
	// 'filename' - file to get metadata from.
	// 'tags' - out, metadata.
	// Returns kShellTagExitCodeOK if any tags were read, or kShellTagExitCodeFail otherwise.
	static DWORD Get( const std::wstring& filename, Tags& tags );

	// Sets metadata.
	// 'filename' - file to set metadata to.
	// 'tags' - metadata.
	// Returns kShellTagExitCodeOK if any tags were written or none of the tags are supported.
	// Returns kShellTagExitCodeSharingViolation if tags could not be written due to a file sharing violation.
	// Returns kShellTagExitCodeFail otherwise.
	static DWORD Set( const std::wstring& filename, const Tags& tags );

	// Returns the audio sub type description from the 'guid'.
	static std::wstring GetAudioSubType( const std::wstring& guid );

	// Converts a 'propVariant' to a string.
	static std::wstring PropertyToString( const PROPVARIANT& propVariant );

private:
	// Maps a audio subtype GUID string to an audio format description.
	using AudioFormatMap = std::map<std::wstring, std::wstring>;

	// Audio format descriptions
	static AudioFormatMap s_AudioFormatDescriptions;
};
