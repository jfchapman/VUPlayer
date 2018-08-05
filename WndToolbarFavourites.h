#pragma once

#include "WndToolbar.h"

class WndToolbarFavourites : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbarFavourites( HINSTANCE instance, HWND parent );

	virtual ~WndToolbarFavourites();

	// Updates the tooolbar state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	virtual void Update( Output& output, const Playlist::Ptr playlist, const Playlist::Item& selectedItem );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const;

private:
	// Creates the buttons.
	void CreateButtons();

	// Creates the image list.
	void CreateImageList();

	// Image list.
	HIMAGELIST m_ImageList;
};

