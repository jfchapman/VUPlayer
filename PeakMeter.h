#pragma once

#include "Visual.h"

class PeakMeter : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	PeakMeter( WndVisual& wndVisual );

	virtual ~PeakMeter();

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

	// Gets the current levels from the output object.
	void GetLevels();

	// Rendering thread handle.
	HANDLE m_RenderThread;

	// Rendering thread stop event handle.
	HANDLE m_RenderStopEvent;

	// Meter element colour.
	ID2D1LinearGradientBrush* m_Colour;

	// Background colour.
	ID2D1SolidColorBrush* m_BackgroundColour;

	// Left level.
	float m_LeftLevel;

	// Right level.
	float m_RightLevel;
};

