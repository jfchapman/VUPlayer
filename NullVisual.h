#pragma once

#include "Visual.h"

class NullVisual : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	NullVisual( WndVisual& wndVisual );

	virtual ~NullVisual();

	// Returns the required visual height, based on a given width.
	virtual int GetHeight( const int width );

	// Shows the visual.
	virtual void Show();

	// Hides the visual.
	virtual void Hide();

	// Called when the visual needs repainting.
	virtual void OnPaint();

	// Called when the visual settings have changed.
	virtual void OnSettingsChanged();
};

