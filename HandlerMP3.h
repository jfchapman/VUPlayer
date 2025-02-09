#pragma once
#include "Handler.h"

#include <string>

// MP3 encoder handler
class HandlerMP3 : public Handler
{
public:
	HandlerMP3();

	virtual ~HandlerMP3();

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
		const HandlerMP3* m_Handler;

		// Application instance handle.
		HINSTANCE m_hInst;
	};

	// Encoder configuration dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

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

	// Returns the current quality setting of the slider control.
	int GetQuality( const HWND slider ) const;

	// Sets the current quality setting on the slider control.
	void SetQuality( const HWND slider, const int quality ) const;

	// Returns the tooltip for the slider control.
	std::wstring GetTooltip( const HINSTANCE instance, const HWND slider ) const;
};
