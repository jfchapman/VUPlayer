#pragma once

#include "WndToolbar.h"

class WndToolbarConvert : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbarConvert( HINSTANCE instance, HWND parent );

	virtual ~WndToolbarConvert();

	// Updates the toolbar state.
	// 'playlist' - currently displayed playlist.
	virtual void Update( const Playlist::Ptr playlist );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const;

private:
	// Creates the buttons.
	void CreateButtons();

	// Creates the image list.
	void CreateImageList();

	// Image list.
	HIMAGELIST m_ImageList;

	// The current tooltip resource ID.
	WORD m_TooltipID;
};

