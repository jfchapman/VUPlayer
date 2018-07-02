#pragma once

#include "WndTrackbar.h"

class WndTrackbarVolume :	public WndTrackbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	// 'settings' - application settings.
	WndTrackbarVolume( HINSTANCE instance, HWND parent, Output& output, Settings& settings );

	virtual ~WndTrackbarVolume();

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText();

	// Called when the trackbar position has finished changing.
	virtual void OnPositionChanged( const int position );

	// Called when the user drags the trackbar thumb.
	virtual void OnDrag( const int position );

	// Sets the trackbar type.
	virtual void SetType( const Type type );

	// Displays the context menu at the specified 'position', in screen coordinates.
	virtual void OnContextMenu( const POINT& position );

	// Updates the trackbar to reflect the current output state.
	void Update();

private:
	// Returns the control type specified by 'settings'.
	static Type GetControlType( Settings& settings );

	// Updates the output volume when the trackbar 'position' changes.
	void UpdateVolume( const int position );

	// Returns the volume corresponding to the trackbar position.
	float GetVolumeFromPosition( const int position ) const;

	// Returns the trackbar position corresponding to the volume.
	int GetPositionFromVolume( const float volume ) const;

	// Updates the output pitch adjustment when the trackbar 'position' changes.
	void UpdatePitch( const int position );

	// Returns the pitch adjustment corresponding to the trackbar position.
	float GetPitchFromPosition( const int position ) const;

	// Returns the trackbar position corresponding to the pitch adjustment.
	int GetPositionFromPitch( const float pitch ) const;

	// Tooltip text.
	std::wstring m_Tooltip;

	// Application settings.
	Settings& m_Settings;
};
