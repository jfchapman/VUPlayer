#pragma once

#include "Visual.h"

class NullVisual : public Visual
{
public:
	// 'wndVisual' - Visual container window.
	NullVisual( WndVisual& wndVisual );

	~NullVisual() override;

	// Returns the required visual height, based on a given width.
	int GetHeight( const int width ) override;

	// Shows the visual.
	void Show() override;

	// Hides the visual.
	void Hide() override;

	// Called when the visual needs repainting.
	void OnPaint() override;

	// Called when the visual settings have changed.
	void OnSettingsChange() override;

	// Called when the system colours have changed.
	void OnSysColorChange() override;
};

