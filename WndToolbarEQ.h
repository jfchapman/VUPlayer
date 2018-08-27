#pragma once

#include "WndToolbar.h"

class WndToolbarEQ : public WndToolbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	WndToolbarEQ( HINSTANCE instance, HWND parent );

	virtual ~WndToolbarEQ();

	// Updates the tooolbar state.
	// 'eqVisible' - indicates whether the EQ dialog is visible.
	virtual void Update( const bool eqVisible );

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

