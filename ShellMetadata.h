#pragma once

#include "Handler.h"

#include <Windows.h>
#include <Shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <mfapi.h>

#include <string>

// Shell metadata functionality 
class ShellMetadata
{
public:
	ShellMetadata();
	virtual ~ShellMetadata();

	// Gets metadata.
	// 'filename' - file to get metadata from.
	// 'tags' - out, metadata.
	// Returns true if metadata was returned.
	static bool Get( const std::wstring& filename, Tags& tags );

	// Sets metadata.
	// 'filename' - file to set metadata to.
	// 'tags' - metadata.
	// Returns true if metadata was set.
	static bool Set( const std::wstring& filename, const Tags& tags );

	// Returns the audio sub type description from the 'guid'.
	static std::wstring GetAudioSubType( const std::wstring& guid );

	// Converts a 'propVariant' to a string.
	static std::wstring PropertyToString( const PROPVARIANT& propVariant );
};

