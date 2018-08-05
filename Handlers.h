#pragma once

#include "Handler.h"

#include <list>

class Handlers
{
public:
	Handlers();

	virtual ~Handlers();

	// Opens a decoder.
	// 'filename' - file to open.
	// Returns the decoder, or nullptr if the stream could not be opened.
	Decoder::Ptr OpenDecoder( const std::wstring& filename ) const;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	bool GetTags( const std::wstring& filename, Handler::Tags& tags ) const;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	bool SetTags( const std::wstring& filename, const Handler::Tags& tags ) const;

	// Returns all the file extensions supported by the handlers.
	std::set<std::wstring> GetAllSupportedFileExtensions() const;

	// Returns the BASS library version.
	std::wstring GetBassVersion() const;

	// Adds a 'handler'.
	void AddHandler( Handler::Ptr handler );

private:
	// Returns a handler supported by the 'filename' extension, or nullptr of there was no match.
	Handler::Ptr FindHandler( const std::wstring& filename ) const;

	// Available handlers.
	std::list<Handler::Ptr> m_Handlers;
};

