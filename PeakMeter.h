#pragma once

#include "Visual.h"

class PeakMeter : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	PeakMeter( WndVisual& wndVisual );

	~PeakMeter() override;

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

	// Gets the current levels from the output object.
	void GetLevels();

	// Rendering thread handle.
	HANDLE m_RenderThread;

	// Rendering thread stop event handle.
	HANDLE m_RenderStopEvent;

	// Meter element colour.
	Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_Colour;

	// Background colour.
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_BackgroundColour;

	// Left level.
	float m_LeftLevel;

	// Right level.
	float m_RightLevel;
};

