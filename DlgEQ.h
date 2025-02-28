#pragma once

#include "stdafx.h"

#include "Output.h"
#include "Settings.h"

class DlgEQ
{
public:
	// 'instance' - module instance handle.
	// 'wndFocusOnHide' - the window handle which gains focus when the dialog is hidden.
	// 'settings' - application settings.
	// 'output' - output object.
	DlgEQ( const HINSTANCE instance, const HWND wndFocusOnHide, Settings& settings, Output& output );

	virtual ~DlgEQ();

	// Returns whether the dialog is visible.
	bool IsVisible() const;

	// Shows or hides the dialog.
	void ToggleVisibility();

	// Initialises the dialog using the 'parent' window handle.
	void Init( const HWND parent );

	// Saves the current EQ settings to application settings.
	void SaveSettings();

	// Returns the modeless dialog window handle.
	HWND GetWindowHandle() const;

private:
	// Maps a slider control handle to a value.
	typedef std::map<HWND, int> SliderValueMap;

	// Dialog box procedure.
	static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Called when the dialog is initialised.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( const HWND hwnd );

	// Called when the position of the 'slider' control changes.
	void OnSliderChange( const HWND slider );

	// Returns the gain corresponding to the slider control position.
	float GetGainFromPosition( const int position ) const;

	// Returns the slider control position corresponding to the gain.
	int GetPositionFromGain( const float gain ) const;

	// Called when the enable button is pressed.
	void OnEnable();

	// Called when the reset button is pressed.
	void OnReset();

	// Returns the tooltip for the slider control.
	std::wstring GetTooltip( const HWND slider ) const;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Dialog window handle.
	HWND m_hWnd;

	// The window handle to gain focus when the dialog is hidden.
	HWND m_hWndFocusOnHide;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Slider control positions.
	SliderValueMap m_SliderPositions;

	// Slider control frequencies.
	SliderValueMap m_SliderFrequencies;

	// Current EQ settings.
	Settings::EQ m_CurrentEQ;
};
