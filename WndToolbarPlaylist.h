#pragma once

#include "WndToolbar.h"

class WndToolbarPlaylist : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	WndToolbarPlaylist( HINSTANCE instance, HWND parent, Settings& settings );

	// Updates the toolbar state.
	// 'output' - output object.
	// 'playlist' - currently displayed playlist.
	// 'selectedItem' - currently focused playlist item.
	void Update( Output& output, const Playlist::Ptr playlist, const Playlist::Item& selectedItem );

	// Returns the tooltip resource ID corresponding to a 'commandID'.
	UINT GetTooltip( const UINT commandID ) const override;

  // Called when application 'settings' have potentially been changed.
  void OnSettingsChanged( Settings& settings );

private:
	// Creates the buttons.
	void CreateButtons();

  // Returns whether the toolbar is displaying a 'Delete File' icon, rather that a 'Remove Track' icon.
  bool IsDeleteFileShown() const;

  // Whether file deletion is allowed (in application settings).
  bool m_AllowFileDeletion = false;
};

