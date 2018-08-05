#pragma once

#include "WndTrackbar.h"

class WndTrackbarSeek : public WndTrackbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	WndTrackbarSeek( HINSTANCE instance, HWND parent, Output& output );

	virtual ~WndTrackbarSeek();

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText();

	// Called when the trackbar position has changed.
	virtual void OnPositionChanged( const int position );

	// Called when the user drags the trackbar thumb.
	virtual void OnDrag( const int position );

	// Updates the seek control state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	void Update( Output& output, const Playlist::Ptr playlist, const Playlist::Item& selectedItem );

private:
	// Returns whether the trackbar thumb is being dragged.
	bool IsDragging() const;

	// Tooltip text.
	std::wstring m_Tooltip;

	// Indicates that the trackbar thumb is being dragged.
	bool m_IsDragging;

	// Current output item represented by the trackbar.
	Output::Item m_OutputItem;

	// Current playlist represented by the trackbar.
	Playlist::Ptr m_Playlist;
};

