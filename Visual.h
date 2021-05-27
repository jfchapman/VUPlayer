#pragma once

#include "WndVisual.h"

class Visual
{
public:
	// 'wndVisual' - Visual container window.
	Visual( WndVisual& wndVisual );
	
	virtual ~Visual();

	// Returns the required visual height, based on a given width.
	virtual int GetHeight( const int width ) = 0;

	// Shows the visual.
	virtual void Show() = 0;

	// Hides the visual.
	virtual void Hide() = 0;

	// Called when the visual needs repainting.
	virtual void OnPaint() = 0;

	// Called when the visual settings have changed.
	virtual void OnSettingsChange() = 0;

	// Called when the system colours have changed.
	virtual void OnSysColorChange() = 0;

	// Returns the output object.
	Output& GetOutput();

	// Returns the media library.
	Library& GetLibrary();

	// Returns the application settings.
	Settings& GetSettings();

	// Begins drawing, and returns, the Direct2D device context.
	ID2D1DeviceContext* BeginDrawing();

	// Ends drawing to the Direct2D device context.
	void EndDrawing();

	// Requests a re-render.
	void DoRender();

	// Returns whether hardware acceleration is enabled.
	bool IsHardwareAccelerationEnabled() const;

private:
	// Visual container window.
	WndVisual& m_WndVisual;
};

