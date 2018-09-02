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
	std::wstring GetDescription() const override;

	// Returns the supported file extensions.
	std::set<std::wstring> GetSupportedFileExtensions() const override;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	bool GetTags( const std::wstring& filename, Tags& tags ) const override;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	bool SetTags( const std::wstring& filename, const Handler::Tags& tags ) const override;

	// Returns a decoder for 'filename', or nullptr if a decoder cannot be created.
	Decoder::Ptr OpenDecoder( const std::wstring& filename ) const override;

	// Returns an encoder, or nullptr if an encoder cannot be created.
	Encoder::Ptr OpenEncoder() const override;

	// Returns whether the handler can decode.
	bool IsDecoder() const override;

	// Returns whether the handler can encode.
	bool IsEncoder() const override;

	// Returns whether the handler supports encoder configuration.
	bool CanConfigureEncoder() const override;

	// Displays an encoder configuration dialog for the handler.
	// 'instance' - application instance handle.
	// 'parent' - application window handle.
	// 'settings' - in/out, encoder settings.
	// Returns whether the encoder has been configured.
	bool ConfigureEncoder( const HINSTANCE instance, const HWND parent, std::string& settings ) const override;
};
