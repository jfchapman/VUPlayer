#pragma once

#include "stdafx.h"

#include "Output.h"
#include "WndRebarItem.h"

class WndTrackbar : public WndRebarItem
{
public:
	// Trackbar type
	enum class Type {
		Seek,
		Volume,
		Pitch,
		Balance
	};

	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'output' - output object.
	// 'settings' - application settings.
	// 'minValue' - minimum trackbar value.
	// 'maxValue' - maximum trackbar value.
	// 'type' - trackbar type.
	WndTrackbar( HINSTANCE instance, HWND parent, Output& output, Settings& settings, const int minValue, const int maxValue, const Type type );

	virtual ~WndTrackbar();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Returns whether the trackbar is enabled.
	bool GetEnabled() const;

	// Sets whether the trackbar is enabled.
	void SetEnabled( const bool enabled );

	// Sets whether the trackbar is visible.
	void SetVisible( const bool visible );

	// Gets the trackbar type.
	Type GetType() const;

	// Sets the trackbar type.
	virtual void SetType( const Type type );

	// Returns the tooltip text for the control.
	virtual const std::wstring& GetTooltipText() = 0;

	// Called when the trackbar 'position' has changed.
	virtual void OnPositionChanged( const int position ) = 0;

	// Called when the user drags the trackbar thumb to a 'position'.
	virtual void OnDrag( const int position ) = 0;

	// Returns the rebar item ID.
	int GetID() const override;

	// Returns the rebar item window handle.
	HWND GetWindowHandle() const override;

	// Gets the rebar item fixed width.
	std::optional<int> GetWidth() const override;

	// Gets the rebar item fixed height.
	std::optional<int> GetHeight() const override;

	// Returns whether the rebar item has a vertical divider.
	bool HasDivider() const override;

	// Returns whether the rebar item can be hidden.
	bool CanHide() const override;

	// Called when the rebar item 'settings' have been changed.
	void OnChangeRebarItemSettings( Settings& settings ) override;

	// Rebar item custom draw callback, using the 'nmcd' structure.
	// Return the appropriate value for the draw stage, or nullopt if the rebar item does not handle custom draw.
	std::optional<LRESULT> OnCustomDraw( LPNMCUSTOMDRAW nmcd ) override;

	// Displays the context menu for the rebar item at the specified 'position', in screen coordinates.
	// Return true if the rebar item handled the context menu, or false to display the rebar context menu.
	bool ShowContextMenu( const POINT& position ) override;

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	// 'isClassicTheme' - indicates whether the Windows classic theme is active.
	void OnSysColorChange( const bool isHighContrast, const bool isClassicTheme ) override;

protected:
	// Returns the output object.
	Output& GetOutput();

	// Returns the output object.
	const Output& GetOutput() const;

	// Returns the application settings.
	Settings& GetSettings();

	// Returns the module instance handle.
	HINSTANCE GetInstanceHandle() const;

	// Returns the trackbar thumb position.
	int GetPosition() const;

	// Sets the trackbar thumb position.
	void SetPosition( const int position );

	// Returns the trackbar range.
	int GetRange() const;

private:
	// Window procedure for the window that hosts the trackbar.
	static LRESULT CALLBACK TrackbarHostProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Window procedure for the trackbar.
	static LRESULT CALLBACK TrackbarProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Next available child window ID.
	static UINT_PTR s_WndTrackbarID;

	// Called when the host window is resized.
	void OnSize();

	// Called when the window needs painting with the 'ps'.
	void OnPaint( const PAINTSTRUCT& ps );

	// Called when the trackbar window background needs erasing.
	void OnEraseTrackbarBackground( const HDC dc );

	// Returns the background colour to use for painting/erasing.
	COLORREF GetBackgroundColour() const;

	// Returns the colour to use to draw the thumb.
	// 'thumbRect' - thumb location, in client coordinates.
	COLORREF GetThumbColour( const RECT& thumbRect ) const;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Host window handle.
	HWND m_hHostWnd;

	// Trackbar window handle.
	HWND m_hTrackbarWnd;

	// Default trackbar window procedure.
	WNDPROC m_DefaultWndProc;

	// Output object.
	Output& m_Output;

	// Application settings.
	Settings& m_Settings;

	// Trackbar type.
	Type m_Type;

	// Thumb colour.
	COLORREF m_ThumbColour;

	// Background colour.
	COLORREF m_BackgroundColour;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;

	// Indicates whether the Windows classic theme is active.
	bool m_IsClassicTheme;

	// Indicates whether the OS is Windows 10 (or later).
	const bool m_IsWindows10;
};
