#pragma once

#include "Visual.h"

class Oscilloscope : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	Oscilloscope( WndVisual& wndVisual );
	
	~Oscilloscope() override;

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

private:
	// Render thread procedure.
	static DWORD WINAPI RenderThreadProc( LPVOID lpParam );

	// Render thread handler.
	void RenderThreadHandler();

	// Starts the rendering thread.
	void StartRenderThread();

	// Stops the rendering thread.
	void StopRenderThread();

	// Loads the resources using the 'deviceContext'.
	void LoadResources( ID2D1DeviceContext* deviceContext );

	// Frees the resources.
	void FreeResources();

	// Rendering thread handle.
	HANDLE m_RenderThread;

	// Rendering thread stop event handle.
	HANDLE m_RenderStopEvent;

	// Oscilloscope colour.
	ID2D1SolidColorBrush* m_Colour;

	// Background colour.
	ID2D1SolidColorBrush* m_BackgroundColour;

	// Oscilloscope stroke width.
	FLOAT m_Weight;
};

