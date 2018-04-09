#pragma once
#include "Handler.h"

#include <string>

#include "Opusfile.h"

// Opus handler
class HandlerOpus :	public Handler
{
public:
	HandlerOpus();

	virtual ~HandlerOpus();

	// Returns a description of the handler.
	virtual std::wstring GetDescription() const;

	// Returns the supported file extensions.
	virtual std::set<std::wstring> GetSupportedFileExtensions() const;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	virtual bool GetTags( const std::wstring& filename, Tags& tags ) const;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	virtual bool SetTags( const std::wstring& filename, const Tags& tags ) const;

	// Returns a decoder for 'filename', or nullptr if a decoder cannot be created.
	virtual Decoder::Ptr OpenDecoder( const std::wstring& filename ) const;
};
