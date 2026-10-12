#pragma once

#include "stdafx.h"

#include <functional>
#include <string>

// Playlist filter bar, which is shown above the playlist.
class WndFilter
{
public:
	// Called when the filter text has changed, with the new 'filter' text.
	using FilterChangedCallback = std::function<void( const std::wstring& filter )>;

	// Called when the focus should move to the playlist.
	using FocusListCallback = std::function<void()>;

	// Called when the filter bar has been shown or hidden.
	using VisibilityChangedCallback = std::function<void()>;

	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'onFilterChanged' - called when the filter text has changed (after a short delay, while the text is being typed).
	// 'onFocusList' - called when the focus should move to the playlist.
	// 'onVisibilityChanged' - called when the filter bar has been shown or hidden (so that the parent window layout can be updated).
	WndFilter( HINSTANCE instance, HWND parent, FilterChangedCallback onFilterChanged, FocusListCallback onFocusList, VisibilityChangedCallback onVisibilityChanged );

	virtual ~WndFilter();

	// Gets the filter bar window handle.
	HWND GetWindowHandle() const;

	// Gets the filter edit control window handle.
	HWND GetEditHandle() const;

	// Returns whether the filter bar is shown.
	bool IsShown() const;

	// Returns whether the filter edit control has the focus.
	bool HasFocus() const;

	// Shows the filter bar (if it is hidden), and moves the focus to the filter text, selecting it.
	void Activate();

	// Shows or hides the filter bar (moving the focus to the playlist, if the filter bar had the focus and is being hidden).
	void SetShown( const bool shown );

	// Clears the filter text (calling the filter changed callback, if the filter has changed).
	// 'focusList' - true to move the focus to the playlist, false to move the focus to the filter text.
	void ClearFilter( const bool focusList );

	// Sets the 'filter' text, without calling the filter changed callback.
	void SetFilter( const std::wstring& filter );

	// Applies the current dark mode state.
	void ApplyDarkMode();

	// Called when the system colours (or high contrast state) change.
	void OnSysColorChange();

private:
	// Window procedure.
	static LRESULT CALLBACK FilterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Edit control subclass procedure.
	static LRESULT CALLBACK EditProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData );

	// Positions the edit control.
	void Layout();

	// Called when the filter text is changed.
	void OnEditChanged();

	// Applies the current filter text (if it has changed since it was last applied).
	void ApplyFilter();

	// Moves the focus to the playlist.
	void FocusList();

	// Deletes the word before the caret (in response to Ctrl+Backspace).
	void DeletePreviousWord();

	// Handles WM_CTLCOLOREDIT for the filter edit control, returning the background brush, or nullptr to use the default colours.
	HBRUSH OnCtlColorEdit( const HDC dc );

	// Reserves space for the clear button at the right of the edit control, so that the text does not run underneath it.
	void SetEditMargins();

	// Sets the edit control cue banner (placeholder text) for the current dark mode state.
	// In dark mode, the cue banner is drawn by DrawDarkModeCueBanner instead, as the themed cue banner colour is too dark to read easily.
	void UpdateCueBanner();

	// Draws the cue banner onto the edit control, when dark mode is active and there is no filter text.
	void DrawDarkModeCueBanner();

	// Draws the inner edge of the edit control border in the edit control background colour, when dark mode is active.
	void DrawDarkModeInnerBorder();

	// Returns the clear button area, in edit control client coordinates.
	RECT GetClearButtonRect() const;

	// Returns whether the clear button is shown (i.e. whether there is any filter text).
	bool IsClearButtonShown() const;

	// Returns whether the edit control client 'point' is over the clear button (when shown).
	bool IsOverClearButton( const POINT& point ) const;

	// Draws the clear button (when shown) onto the edit control.
	void DrawClearButton();

	// Sets whether the clear button is 'hot' (the mouse is over it) and 'pressed', redrawing it if the state has changed.
	void SetClearButtonState( const bool hot, const bool pressed );

	// Handles edit control mouse messages for the clear button, returning true if the 'message' was handled.
	bool OnEditMouseMessage( const UINT message, const LPARAM lParam );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Filter bar window handle.
	HWND m_hWnd;

	// Filter edit control window handle.
	HWND m_hEdit;

	// Filter text font.
	HFONT m_Font;

	// Dark mode edit control background brush (created when first needed).
	HBRUSH m_DarkModeBackgroundBrush;

	// Height of the edit control.
	int m_EditHeight;

	// Cue banner (placeholder) text.
	std::wstring m_CueBanner;

	// Indicates whether the edit control was empty when the text last changed.
	bool m_WasEmpty;

	// Called when the filter text has changed.
	FilterChangedCallback m_OnFilterChanged;

	// Called when the focus should move to the playlist.
	FocusListCallback m_OnFocusList;

	// Called when the filter bar has been shown or hidden.
	VisibilityChangedCallback m_OnVisibilityChanged;

	// The filter text which was last applied.
	std::wstring m_AppliedFilter;

	// Indicates whether filter text changes should be ignored (because the text is being set programmatically).
	bool m_IgnoreChange;

	// Indicates whether the mouse is over the clear button.
	bool m_ClearButtonHot;

	// Indicates whether the clear button is being pressed.
	bool m_ClearButtonPressed;

	// Indicates whether the mouse has been captured by the clear button (i.e. the left mouse button was pressed over the clear button).
	bool m_ClearButtonCapture;
};
