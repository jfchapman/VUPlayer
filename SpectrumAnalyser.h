#pragma once

#include "Visual.h"

class SpectrumAnalyser : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	SpectrumAnalyser( WndVisual& wndVisual );

	virtual ~SpectrumAnalyser();

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

	// Render thread handler.
	void RenderThreadHandler();

private:
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

	// Analyser colour.
	ID2D1LinearGradientBrush* m_Colour;

	// Background colour.
	ID2D1SolidColorBrush* m_BackgroundColour;

	// Spectrum values.
	std::vector<float> m_Values;
};

