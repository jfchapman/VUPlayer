#pragma once
#include "Handler.h"

#include "FLAC++\all.h"

// FLAC handler
class HandlerFlac :	public Handler
{
public:
	HandlerFlac();

	virtual ~HandlerFlac();

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
