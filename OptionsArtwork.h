#pragma once

#include "Options.h"

class OptionsArtwork : public Options
{
public:
	// 'instance' - module instance handle.
	// 'settings' - application settings.
	// 'output' - output object.
	OptionsArtwork( HINSTANCE instance, Settings& settings, Output& output );

	// Called when the options page should be initialised.
	// 'hwnd' - dialog window handle.
	void OnInit( const HWND hwnd ) override;

	// Called when the options page should be saved.
	// 'hwnd' - dialog window handle.
	void OnSave( const HWND hwnd ) override;

	// Called when a command on the options page should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - command parameter.
	// 'lParam' - command parameter.
	void OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM lParam ) override;

	// Called when a draw item message should be handled.
	// 'hwnd' - dialog window handle.
	// 'wParam' - draw item parameter.
	// 'lParam' - draw item parameter.
	void OnDrawItem( const HWND, const WPARAM wParam, const LPARAM lParam ) override;

private:
	// Loads the current placeholder artwork.
	// 'hwnd' - dialog window handle.
	void LoadArtwork( const HWND hwnd );

	// Called when painting is required for the artwork control.
	void OnDrawArtwork( DRAWITEMSTRUCT* drawItemStruct );

	// Launches a file chooser to select an artwork image.
	// 'hwnd' - dialog window handle.
	void OnChooseArtwork( const HWND hwnd );

	// Current artwork.
	std::unique_ptr<Gdiplus::Bitmap> m_Artwork = nullptr;
};
