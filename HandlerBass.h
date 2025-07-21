#pragma once

#include "Handler.h"

#include "bass.h"
#include "bassdsd.h"
#include "bassmidi.h"

// Bass handler
class HandlerBass : public Handler
{
public:
	HandlerBass();

	virtual ~HandlerBass();

	// Returns a description of the handler.
	std::wstring GetDescription() const override;

	// Returns the supported file extensions, as a set of lowercase strings.
	std::set<std::wstring> GetSupportedFileExtensions() const override;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	bool GetTags( const std::wstring& filename, Tags& tags ) const override;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	bool SetTags( const std::wstring& filename, const Tags& tags ) const override;

	// Returns a decoder for 'filename' in the specified 'context', or nullptr if a decoder cannot be created.
	Decoder::Ptr OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const override;

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

	// Called when the application 'settings' have changed.
	void SettingsChanged( Settings& settings ) override;

	// Loads the soundfont specified by the application 'settings'.
	void LoadSoundFont( Settings& settings );

private:
	// Reads Ogg tags.
	// 'oggTags' - series of null-terminated UTF-8 strings, ending with a double null.
	// 'tags' - out, tag information.
	static void ReadOggTags( const char* oggTags, Tags& tags );

	// Writes Ogg 'tags' to 'filename', returning true if the tags were written.
	// 'tags' - out, tag information.
	static bool WriteOggTags( const std::wstring& filename, const Tags& tags );

	// Reads RIFF tags.
	// 'riffTags' - series of null-terminated strings (with assumed Windows-1252 encoding), ending with a double null.
	// 'tags' - out, tag information.
	static void ReadRIFFTags( const char* riffTags, Tags& tags );

	// Reads the ID3 tag from a Dsf file.
	// 'filename' - Dsf filename.
	// 'tags' - out, tag information.
	// Returns whether any tags were read.
	static bool ReadDSDTag( const std::wstring& filename, Tags& tags );

	// Returns a temporary file name.
	static std::wstring GetTemporaryFilename();

	// BASS midi plugin.
	HPLUGIN m_BassMidi;

	// BASS DSD plugin.
	HPLUGIN m_BassDSD;

	// BASS HLS plugin.
	HPLUGIN m_BassHLS;

	// BASS midi soundfont.
	HSOUNDFONT m_BassMidiSoundFont;

	// Currently loaded soundfont file name.
	std::wstring m_SoundFontFilename;
};
