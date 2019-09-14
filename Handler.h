#pragma once
#include "stdafx.h"

#include "Decoder.h"
#include "Encoder.h"
#include "Tag.h"

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

class Settings;

// Audio format handler interface.
class Handler
{
public:
	Handler()
	{
	}

	virtual ~Handler()
	{
	}

	// Handler shared pointer type.
	typedef std::shared_ptr<Handler> Ptr;

	// A list of handlers.
	typedef std::list<Ptr> List;

	// Returns a description of the handler.
	virtual std::wstring GetDescription() const = 0;

	// Returns the supported file extensions, as a set of lowercase strings.
	virtual std::set<std::wstring> GetSupportedFileExtensions() const = 0;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	virtual bool GetTags( const std::wstring& filename, Tags& tags ) const = 0;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	virtual bool SetTags( const std::wstring& filename, const Tags& tags ) const = 0;

	// Returns a decoder for 'filename', or nullptr if a decoder cannot be created.
	virtual Decoder::Ptr OpenDecoder( const std::wstring& filename ) const = 0;

	// Returns an encoder, or nullptr if an encoder cannot be created.
	virtual Encoder::Ptr OpenEncoder() const = 0;

	// Returns whether the handler can decode.
	virtual bool IsDecoder() const = 0;

	// Returns whether the handler can encode.
	virtual bool IsEncoder() const = 0;

	// Returns whether the handler supports encoder configuration.
	virtual bool CanConfigureEncoder() const = 0;

	// Displays an encoder configuration dialog for the handler.
	// 'instance' - application instance handle.
	// 'parent' - application window handle.
	// 'settings' - in/out, encoder settings.
	// Returns whether the encoder has been configured.
	virtual bool ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const = 0;

	// Called when the application 'settings' have changed.
	virtual void SettingsChanged( Settings& settings ) = 0;
};
