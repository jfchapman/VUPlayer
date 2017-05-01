#pragma once

#include "WndTrackbar.h"

class WndTrackbarVolume :	public WndTrackbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	WndTrackbarVolume( HINSTANCE instance, HWND parent, Output& output );

	virtual ~WndTrackbarVolume();

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText();

	// Called when the trackbar position has finished changing.
	virtual void OnPositionChanged( const int position );

	// Called when the user drags the trackbar thumb.
	virtual void OnDrag( const int position );

	// Updates the trackbar to reflect the current output volume.
	void Update();

private:
	// Updates the output volume when the trackbar 'position' changes.
	void UpdateVolume( const int position );

	// Tooltip text.
	std::wstring m_Tooltip;
};
