#pragma once

#include "WndToolbar.h"

class WndToolbarPlayback : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbarPlayback( HINSTANCE instance, HWND parent );

	virtual ~WndToolbarPlayback();

	// Updates the toolbar state.
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

	// Sets whether the 'paused' button is shown.
	void SetPaused( const bool paused ) const;

	// Returns whether the pause button is shown.
	bool IsPauseShown() const;

	// Image list.
	HIMAGELIST m_ImageList;

	// Maps an image list index to a command ID.
	std::map<int,UINT> m_ImageMap;
};

