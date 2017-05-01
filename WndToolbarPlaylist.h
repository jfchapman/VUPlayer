#pragma once

#include "WndToolbar.h"

class WndToolbarPlaylist : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbarPlaylist( HINSTANCE instance, HWND parent );

	virtual ~WndToolbarPlaylist();

	// Updates the tooolbar state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	virtual void Update( Output& output, const Playlist::Ptr& playlist, const Playlist::Item& selectedItem );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	virtual UINT GetTooltip( const UINT commandID ) const;

private:
	// Creates the buttons.
	void CreateButtons();

	// Creates the image list.
	void CreateImageList();

	// Image list.
	HIMAGELIST m_ImageList;

	// Maps an image list index to a command ID.
	std::map<int,UINT> m_ImageMap;
};

