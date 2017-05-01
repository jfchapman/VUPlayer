#pragma once

#include "Handler.h"

// Bass handler
class HandlerBass : public Handler
{
public:
	HandlerBass();

	virtual ~HandlerBass();

	// Returns a description of the handler.
	virtual std::wstring GetDescription() const;

	// Returns the supported file extensions.
	virtual std::set<std::wstring> GetSupportedFileExtensions() const;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	virtual bool GetTags( const std::wstring& filename, Tags& tags ) const;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	virtual bool SetTags( const std::wstring& filename, const Handler::Tags& tags ) const;

	// Returns a decoder for 'filename', or nullptr if a decoder cannot be created.
	virtual Decoder::Ptr OpenDecoder( const std::wstring& filename ) const;

private:
	// Reads Ogg tags.
	// 'oggTags' - series of null-terminated UTF-8 strings, ending with a double null.
	// 'tags' - out, tag information.
	void ReadOggTags( const char* oggTags, Tags& tags ) const;

	// Writes Ogg 'tags' to 'filename', returning true if the tags were written.
	// 'tags' - out, tag information.
	bool WriteOggTags( const std::wstring& filename, const Tags& tags ) const;

	// Returns a temporary file name.
	std::wstring GetTemporaryFilename() const;
};