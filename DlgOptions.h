#pragma once

#include "Output.h"
#include "Settings.h"

class DlgOptions
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	DlgOptions( HINSTANCE instance, HWND parent, Settings& settings, Output& output );

	virtual ~DlgOptions();

private:
	// Options dialog procedure.
	static INT_PTR CALLBACK OptionsProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Applies the page font to the property sheet frame (tab control & buttons), and adjusts the sheet layout to suit.
	// 'sheet' - property sheet window handle.
	// 'page' - initial property page window handle.
	static void LayoutPropertySheet( const HWND sheet, const HWND page );

	// Moves a property 'page' to the top of the display area of the 'sheet' tab control.
	static void MovePropertySheetPage( const HWND sheet, const HWND page );

	// Indicates whether the dialog has been centred.
	static bool s_IsCentred;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;
};
