#pragma once

#include "WndTrackbar.h"

class WndTrackbarVolume :	public WndTrackbar
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	// 'settings' - application settings.
	WndTrackbarVolume( HINSTANCE instance, HWND parent, Output& output, Settings& settings );

	virtual ~WndTrackbarVolume();

	// Returns the tooltip text for the control.
	const std::wstring& GetTooltipText() override;

	// Called when the trackbar position has finished changing.
	void OnPositionChanged( const int position ) override;

	// Called when the user drags the trackbar thumb.
	void OnDrag( const int position ) override;

	// Sets the trackbar type.
	void SetType( const Type type ) override;

	// Updates the trackbar to reflect the current output state.
	void Update();

	// Displays the context menu for the rebar item at the specified 'position', in screen coordinates.
	// Return true if the rebar item handled the context menu, or false to display the rebar context menu.
	bool ShowContextMenu( const POINT& position ) override;

	// Gets the rebar item fixed width.
	std::optional<int> GetWidth() const override;

	// Called when the rebar item 'settings' have been changed.
	void OnChangeRebarItemSettings( Settings& settings ) override;

private:
	// Returns the control type specified by 'settings'.
	static Type GetControlType( Settings& settings );

	// Updates the output volume when the trackbar 'position' changes.
	void UpdateVolume( const int position );

	// Returns the volume corresponding to the trackbar position.
	float GetVolumeFromPosition( const int position ) const;

	// Returns the trackbar position corresponding to the volume.
	int GetPositionFromVolume( const float volume ) const;

	// Updates the output pitch adjustment when the trackbar 'position' changes.
	void UpdatePitch( const int position );

	// Returns the pitch adjustment corresponding to the trackbar position.
	float GetPitchFromPosition( const int position ) const;

	// Returns the trackbar position corresponding to the pitch adjustment.
	int GetPositionFromPitch( const float pitch ) const;

	// Updates the balance adjustment when the trackbar 'position' changes.
	void UpdateBalance( const int position );

	// Returns the balance adjustment corresponding to the trackbar position.
	float GetBalanceFromPosition( const int position ) const;

	// Returns the trackbar position corresponding to the balance adjustment.
	int GetPositionFromBalance( const float balance ) const;

	// Sets the control width, based on the 'settings'.
	void SetWidth( Settings& settings );

	// Tooltip text.
	std::wstring m_Tooltip;

	// Control width.
	std::optional<int> m_Width;
};
