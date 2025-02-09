#pragma once

#include "Handler.h"

#include <list>
#include <mutex>

class Library;
class MediaInfo;

// Audio format handlers
class Handlers
{
public:
	Handlers();

	virtual ~Handlers();

	// Opens a decoder.
	// 'filename' - file to open.
	// 'context' - context for which the decoder is to be used.
	// 'applyCues' - whether to apply any cues (if present) when opening the decoder.
	// Returns the decoder, or nullptr if the stream could not be opened.
	Decoder::Ptr OpenDecoder( const MediaInfo& mediaInfo, const Decoder::Context context, const bool applyCues = true ) const;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	bool GetTags( const std::wstring& filename, Tags& tags ) const;

	// Writes tags for 'mediaInfo' using the media 'library', returning true if the tags were written.
	bool SetTags( const MediaInfo& mediaInfo, Library& library ) const;

	// Returns all the file extensions supported by the decoders, as a set of lowercase strings.
	std::set<std::wstring> GetAllSupportedFileExtensions() const;

	// Returns the BASS library version.
	std::wstring GetBassVersion() const;

	// Returns the OpenMPT library version.
	std::wstring GetOpenMPTVersion() const;

	// Adds a 'handler'.
	void AddHandler( Handler::Ptr handler );

	// Returns the available encoders.
	Handler::List GetEncoders() const;

	// Opens a encoder matching the 'description'.
	// Returns the encoder, or nullptr if an encoder could not be opened.
	Encoder::Ptr OpenEncoder( const std::wstring& description ) const;

	// Called when the application 'settings' have changed.
	void SettingsChanged( Settings& settings );

	// Initialises the handlers with the application 'settings'.
	void Init( Settings& settings );

private:
	// Returns a decoder handler supported by the 'filename' extension, or nullptr of there was no match.
	Handler::Ptr FindDecoderHandler( const std::wstring& filename ) const;

	// BASS handler.
	Handler::Ptr m_HandlerBASS;

	// OpenMPT handler.
	Handler::Ptr m_HandlerOpenMPT;

	// FFmpeg handler.
	Handler::Ptr m_HandlerFFmpeg;

	// Available handlers.
	Handler::List m_Handlers;

	// Available decoders.
	Handler::List m_Decoders;

	// Available encoders.
	Handler::List m_Encoders;

	// Whether to allow writing of metadata tags to file.
	mutable bool m_WriteTags = false;

	// Whether to preserve the last modified time when writing metadata tags to file.
	mutable bool m_PreserveLastModifiedTime = false;

	// Decoders mutex (used when swapping decoder order).
	mutable std::mutex m_MutexDecoders;
};
