#pragma once

#include "Visual.h"

class Artwork :	public Visual
{
public:
	// 'wndVisual' - Visual container window.
	Artwork( WndVisual& wndVisual );

	~Artwork() override;

	// Returns the required visual height, based on a given width.
	int GetHeight( const int width ) override;

	// Shows the visual.
	void Show() override;

	// Hides the visual.
	void Hide() override;

	// Called when the visual needs repainting.
	void OnPaint() override;

	// Called when the visual settings have changed.
	void OnSettingsChange() override;

	// Called when the system colours have changed.
	void OnSysColorChange() override;

  // Called when the visual should free any resources.
	void FreeResources() override;

private:
	// Loads the artwork resource from 'mediaInfo', using the 'deviceContext'.
	void LoadArtwork( const MediaInfo& mediaInfo, ID2D1DeviceContext* deviceContext );

	// Returns the artwork bitmap from 'mediaInfo', or null if a bitmap could not be loaded.
	std::unique_ptr<Gdiplus::Bitmap> GetArtworkBitmap( const MediaInfo& mediaInfo );

	// Currently displayed bitmap.
  Microsoft::WRL::ComPtr<ID2D1Bitmap>	m_Bitmap;

	// Currently displayed artwork ID.
	std::wstring m_ArtworkID;

	// The previous drawan size.
	D2D1_SIZE_F m_PreviousSize;
};

