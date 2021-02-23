#pragma once

#include "Visual.h"

class Artwork :	public Visual
{
public:
	// 'wndVisual' - Visual container window.
	Artwork( WndVisual& wndVisual );

	virtual ~Artwork();

	// Returns the required visual height, based on a given width.
	virtual int GetHeight( const int width );

	// Shows the visual.
	virtual void Show();

	// Hides the visual.
	virtual void Hide();

	// Called when the visual needs repainting.
	virtual void OnPaint();

	// Called when the visual settings have changed.
	virtual void OnSettingsChanged();

private:
	// Loads the artwork resource from 'mediaInfo', using the 'deviceContext'.
	void LoadArtwork( const MediaInfo& mediaInfo, ID2D1DeviceContext* deviceContext );

	// Frees the artwork resource.
	void FreeArtwork();

	// Returns the artwork bitmap from 'mediaInfo', or null if a bitmap could not be loaded.
	std::unique_ptr<Gdiplus::Bitmap> GetArtworkBitmap( const MediaInfo& mediaInfo );

	// Background brush.
	ID2D1SolidColorBrush* m_Brush;

	// Currently displayed bitmap.
	ID2D1Bitmap* m_Bitmap;

	// Currently displayed artwork ID.
	std::wstring m_ArtworkID;
};

