#pragma once

#include "Handler.h"

#include "CDDAManager.h"

class HandlerCDDA : public Handler
{
public:
	// 'instance' - module instance handle.
	// 'cddaManager' - CD audio manager.
	HandlerCDDA( const HINSTANCE instance, CDDAManager& cddaManager );

	virtual ~HandlerCDDA();

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

private:
	// Module instance handle.
	HINSTANCE m_hInst;

	// CD audio manager
	CDDAManager& m_CDDAManager;
};

