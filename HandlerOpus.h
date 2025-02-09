#pragma once
#include "Handler.h"

#include <string>
#include <vector>

// Opus handler
class HandlerOpus : public Handler
{
public:
	HandlerOpus();

	virtual ~HandlerOpus();

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

private:
	// Configuration dialog initialisation information.
	struct ConfigurationInfo {
		// Configuration settings.
		std::string& m_Settings;

		// Handler object.
		const HandlerOpus* m_Handler;

		// Application instance handle.
		HINSTANCE m_hInst;
	};

	// Encoder configuration dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Converts the gain value to an appropriate R128 string (returns an empty string if the value could not be converted).
	static std::string GainToR128( const std::string& gain );

	// Converts the R128 value to an appropriate gain string (returns an empty string if the value could not be converted).
	static std::string R128ToGain( const std::string& gain );

	// Called when the encoder configuration dialog is initialised.
	// 'hwnd' - dialog window handle.
	// 'settings' - configuration settings.
	void OnConfigureInit( const HWND hwnd, const std::string& settings ) const;

	// Called when the default button is pressed on the encoder configuration dialog.
	// 'hwnd' - dialog window handle.
	// 'settings' - out, configuration settings.
	void OnConfigureDefault( const HWND hwnd, std::string& settings ) const;

	// Called when the encoder configuration dialog is closed.
	// 'hwnd' - dialog window handle.
	// 'settings' - out, configuration settings.
	void OnConfigureClose( const HWND hwnd, std::string& settings ) const;

	// Returns the current bitrate control value.
	int GetBitrate( const HWND control ) const;

	// Sets the current bitrate control value.
	void SetBitrate( const HWND control, const int bitrate ) const;
};
