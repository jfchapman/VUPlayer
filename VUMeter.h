#pragma once

#include "Visual.h"

// Minimum decay factor.
static const float VUMeterDecayMinimum = 0.0075f;

// Normal decay factor.
static const float VUMeterDecayNormal = 0.01f;

// Maximum decay factor.
static const float VUMeterDecayMaximum = 0.015f;

class VUMeter :	public Visual
{
public:
	// 'wndVisual' - Visual container window.
	// 'stereo' - true for stereo meter, false for mono.
	VUMeter( WndVisual& wndVisual, const bool stereo = true );

	virtual ~VUMeter();

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

	// Gets the current levels, returning true if the levels have changed since last time around.
	bool GetLevels();

	// Draws a pin position onto the base meter image.
	void DrawPin( const int position );

	// Loads the resources using the 'deviceContext'.
	void LoadResources( ID2D1DeviceContext* deviceContext );

	// Frees the resources.
	void FreeResources();

	// Rendering thread handle.
	HANDLE m_RenderThread;

	// Rendering thread stop event handle.
	HANDLE m_RenderStopEvent;

	// Meter image.
	BYTE* m_MeterImage;

	// Current pin position buffer drawn on the meter image.
	const DWORD* m_MeterPin;

	// Left meter bitmap.
	ID2D1Bitmap* m_BitmapLeft;

	// Right meter bitmap.
	ID2D1Bitmap* m_BitmapRight;

	// Background brush.
	ID2D1SolidColorBrush* m_Brush;

	// Current left channel level.
	float m_LeftLevel;

	// Current right channel level.
	float m_RightLevel;

	// Decay rate.
	float m_Decay;

	// Whether this is a stereo meter.
	bool m_IsStereo;
};
