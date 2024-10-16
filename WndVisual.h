#pragma once

#include "Library.h"
#include "Output.h"
#include "resource.h"
#include "Settings.h"

#include <memory>

#include <wrl.h>

#include <D2d1_1.h>
#include <d3d11.h>
#include <DXGI1_2.h>

class Visual;

class WndVisual
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'rebarWnd' - rebar window handle.
	// 'statusWnd' - status bar window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	// 'library' - media library.
	WndVisual( HINSTANCE instance, HWND parent, HWND rebarWnd, HWND statusWnd, Settings& settings, Output& output, Library& library );

	virtual ~WndVisual();

	// Gets the visual window handle.
	HWND GetWindowHandle();

	// Called when the host window needs painting with the 'ps'.
	void OnPaint( const PAINTSTRUCT& ps );

	// Called when the control is resized.
	// 'windowPos' - in/out, window position, which can be modified.
	void Resize( WINDOWPOS* windowPos );

	// Returns the output object.
	Output& GetOutput();

	// Returns the media library.
	Library& GetLibrary();

	// Returns the application settings.
	Settings& GetSettings();

	// Begins drawing, and returns, the Direct2D device context.
	ID2D1DeviceContext* BeginDrawing();

	// Ends drawing to the Direct2D device.
	void EndDrawing();

	// Displays the context menu at the specified 'position', in screen coordinates.
	void OnContextMenu( const POINT& position );

	// Selects a visual.
	void SelectVisual( const long id );

	// Returns the ID of the currently displayed visual.
	long GetCurrentVisualID() const;

	// Returns the IDs of all the visuals.
	std::set<UINT> GetVisualIDs() const;

	// Called by a visual when a render is required.
	void DoRender();

	// Called when the oscilloscope colour command is selected.
	void OnOscilloscopeColour();

	// Called when the oscilloscope background colour command is selected.
	void OnOscilloscopeBackground();

	// Called when an oscilloscope weight command is selected.
	void OnOscilloscopeWeight( const UINT commandID );

	// Called when a spectrum analyser colour command is selected.
	void OnSpectrumAnalyserColour( const UINT commandID );

	// Called when a peak meter colour command is selected.
	void OnPeakMeterColour( const UINT commandID );

	// Called when a VUMeter decay rate command is selected.
	void OnVUMeterDecay( const UINT commandID );

	// Returns the command ID mappings for oscilloscope weights.
	std::map<UINT,float> GetOscilloscopeWeights() const;

	// Returns the command ID mappings for VUMeter decay rates.
	std::map<UINT,float> GetVUMeterDecayRates() const;

	// Called when the placeholder artwork has changed.
	void OnPlaceholderArtworkChanged();

	// Toggles hardware acceleration on/off.
	void ToggleHardwareAccelerationEnabled();

	// Returns whether hardware acceleration is enabled.
	bool IsHardwareAccelerationEnabled() const;

	// Called on a system colour change event.
	void OnSysColorChange();

  // Called when the display resolution has changed.
  void OnDisplayChange();

  // Returns the current DPI scaling factor.
  float GetDPIScalingFactor() const;

private:
	// Window procedure
	static LRESULT CALLBACK VisualProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Maps an ID to a visual.
	typedef std::map<UINT, std::shared_ptr<Visual>> Visuals;

	// Initialises Direct2D resources.
	void InitD2D();

	// Creates, if necessary, the Direct2D device context.
	void CreateD2DDeviceContext();

	// Resizes the Direct2D swap chain to the 'width & 'height', if necessary.
	HRESULT ResizeD2DSwapChain( const UINT width, const UINT height );

	// Initialises the Direct2D device context.
	HRESULT InitialiseDeviceContext();

	// Frees Direct2D resources.
	void ReleaseD2D();

	// Gets the current visual.
	Visual* GetCurrentVisual();

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Parent window handle.
	HWND m_hWndParent;

	// Rebar window handle.
	HWND m_hWndRebar;
	
	// Status window handle.
	HWND m_hWndStatus;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Media library.
	Library& m_Library;

	// Direct2D factory.
	Microsoft::WRL::ComPtr<ID2D1Factory1> m_D2DFactory;

	// Direct2D device context.
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_D2DDeviceContext;

	// Direct2D swap chain.
	Microsoft::WRL::ComPtr<IDXGISwapChain1> m_D2DSwapChain;

	// Visuals.
	Visuals m_Visuals;

	// Current visual ID.
	long m_CurrentVisual;

	// Whether hardware acceleration is enabled in the application settings.
	bool m_HardwareAccelerationEnabled;

  // Current DPI scaling factor.
  float m_DPIScaling = 1.0f;
};

