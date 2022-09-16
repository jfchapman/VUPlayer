#pragma once

#include "Visual.h"

// Minimum decay factor.
constexpr float VUMeterDecayMinimum = 0.0075f;

// Normal decay factor.
constexpr float VUMeterDecayNormal = 0.01f;

// Maximum decay factor.
constexpr float VUMeterDecayMaximum = 0.015f;

class VUMeter :	public Visual
{
public:
	// 'wndVisual' - Visual container window.
	// 'stereo' - true for stereo meter, false for mono.
	VUMeter( WndVisual& wndVisual, const bool stereo = true );

	~VUMeter() override;

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

	// Returns the pin position corresponding to the 'level'.
	int GetPinPosition( const float level );

	// Render thread handler.
	void RenderThreadHandler();

	// Starts the rendering thread.
	void StartRenderThread();

	// Stops the rendering thread.
	void StopRenderThread();

	// Gets the output & display levels, returning true if the display levels have changed since last time around.
	bool GetLevels();

	// Draws a pin position onto the base meter image.
	void DrawPin( const int position );

	// Updates the meter bitmaps based on the current 'left' & 'right' display levels.
	void UpdateBitmaps( const float left, const float right );

	// Loads the resources using the 'deviceContext'.
	void LoadResources( ID2D1DeviceContext* deviceContext );

	// Frees the resources.
	void FreeResources();

  // Whether the meter resource has been loaded successfully.
  static bool s_MeterLoaded;

  // Base meter image width.
  static DWORD s_MeterWidth;

  // Base meter image height.
  static DWORD s_MeterHeight;

  // Base meter image bytes.
  static const BYTE* s_MeterBase;

  // Meter pin delta arrays (null terminated).
  static std::vector<const DWORD*> s_MeterPins;

	// Rendering thread handle.
	HANDLE m_RenderThread;

	// Rendering thread stop event handle.
	HANDLE m_RenderStopEvent;

  // Composed meter image.
	std::vector<BYTE> m_MeterImage;

	// Current pin position buffer drawn on the meter image.
	const DWORD* m_MeterPin;

	// Left meter bitmap.
	ID2D1Bitmap* m_BitmapLeft;

	// Right meter bitmap.
	ID2D1Bitmap* m_BitmapRight;

	// Background brush.
	ID2D1SolidColorBrush* m_Brush;

	// Current left/right channel output level.
	std::pair<float, float> m_OutputLevel;

	// Current left channel display level.
	std::atomic<float> m_LeftDisplayLevel;

	// Current right channel display level.
	std::atomic<float> m_RightDisplayLevel;

	// Current left/right meter position.
	std::pair<int, int> m_MeterPosition;

	// Decay rate.
	float m_Decay;

	// Whether this is a stereo meter.
	bool m_IsStereo;
};
