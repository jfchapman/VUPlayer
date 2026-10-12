#include "DlgOptions.h"

#include "OptionsArtwork.h"
#include "OptionsGeneral.h"
#include "OptionsHotkeys.h"
#include "OptionsMixing.h"
#include "OptionsTaskbar.h"

#include "resource.h"
#include "DarkMode.h"
#include "Utility.h"
#include "windowsx.h"

bool DlgOptions::s_IsCentred = false;

// Property sheet procedure
INT_PTR CALLBACK DlgOptions::OptionsProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			PROPSHEETPAGE* page = reinterpret_cast<PROPSHEETPAGE*>( lParam );
			if ( nullptr != page ) {
				Options* options = reinterpret_cast<Options*>( page->lParam );
				if ( nullptr != options ) {
					SetWindowLongPtr( hwnd, DWLP_USER, page->lParam );
					options->OnInit( hwnd );
				}
			}
			DarkMode::ApplyToDialog( hwnd );
			LayoutDialogTextControls( hwnd );
			if ( !s_IsCentred ) {
				s_IsCentred = true;
				// Apply dark mode to the property sheet itself (title bar, tabs & buttons) when the first page is initialised.
				DarkMode::ApplyToDialog( GetParent( hwnd ) );
				LayoutPropertySheet( GetParent( hwnd ), hwnd );
				CentreDialog( GetParent( hwnd ) );
			} else {
				// Pages which are subsequently created need to be positioned below the (enlarged) tabs.
				MovePropertySheetPage( GetParent( hwnd ), hwnd );
			}
			return TRUE;
		}
		case WM_DESTROY: {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_NOTIFY: {
			LPPSHNOTIFY pshNotify = reinterpret_cast<LPPSHNOTIFY>( lParam );
			if ( nullptr != pshNotify ) {
				Options* options = reinterpret_cast<Options*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
				if ( nullptr != options ) {
					if ( PSN_APPLY == pshNotify->hdr.code ) {
						options->OnSave( hwnd );
					} else {
						if ( PSN_SETACTIVE == pshNotify->hdr.code ) {
							const int pageIndex = PropSheet_HwndToIndex( pshNotify->hdr.hwndFrom, hwnd );
							options->GetSettings().SetLastOptionsPage( pageIndex );
						}
						options->OnNotify( hwnd, wParam, lParam );
					}
				}
			}
			break;
		}
		case WM_COMMAND: {
			Options* options = reinterpret_cast<Options*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != options ) {
				options->OnCommand( hwnd, wParam, lParam );
			}
			break;
		}
		case WM_DRAWITEM: {
			Options* options = reinterpret_cast<Options*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != options ) {
				options->OnDrawItem( hwnd, wParam, lParam );
			}
			break;
		}
	}
	return FALSE;
}

DlgOptions::DlgOptions( HINSTANCE instance, HWND parent, Settings& settings, Output& output ) :
	m_hInst( instance ),
	m_Settings( settings ),
	m_Output( output )
{
	const UINT pageCount = 5;
	std::vector<HPROPSHEETPAGE> pages( pageCount );

	OptionsGeneral optionsGeneral( instance, settings, output );
	OptionsTaskbar optionsTaskbar( instance, settings, output );
	OptionsHotkeys optionsHotkeys( instance, settings, output );
	OptionsMixing optionsMixing( instance, settings, output );
	OptionsArtwork optionsArtwork( instance, settings, output );

	// General property page
	PROPSHEETPAGE propPageGeneral = {};
	propPageGeneral.dwSize = sizeof( PROPSHEETPAGE );
	propPageGeneral.dwFlags = PSP_DEFAULT;
	propPageGeneral.hInstance = instance;
	propPageGeneral.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_GENERAL );
	propPageGeneral.pfnDlgProc = OptionsProc;
	propPageGeneral.lParam = reinterpret_cast<LPARAM>( &optionsGeneral );
	pages.at( 0 ) = CreatePropertySheetPage( &propPageGeneral );

	// Taskbar property page
	PROPSHEETPAGE propPageTaskbar = {};
	propPageTaskbar.dwSize = sizeof( PROPSHEETPAGE );
	propPageTaskbar.dwFlags = PSP_DEFAULT;
	propPageTaskbar.hInstance = instance;
	propPageTaskbar.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_TASKBAR );
	propPageTaskbar.pfnDlgProc = OptionsProc;
	propPageTaskbar.lParam = reinterpret_cast<LPARAM>( &optionsTaskbar );
	pages.at( 1 ) = CreatePropertySheetPage( &propPageTaskbar );

	// Hotkeys property page
	PROPSHEETPAGE propPageHotkeys = {};
	propPageHotkeys.dwSize = sizeof( PROPSHEETPAGE );
	propPageHotkeys.dwFlags = PSP_DEFAULT;
	propPageHotkeys.hInstance = instance;
	propPageHotkeys.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_HOTKEYS );
	propPageHotkeys.pfnDlgProc = OptionsProc;
	propPageHotkeys.lParam = reinterpret_cast<LPARAM>( &optionsHotkeys );
	pages.at( 2 ) = CreatePropertySheetPage( &propPageHotkeys );

	// Mixing property page
	PROPSHEETPAGE propPageMixing = {};
	propPageMixing.dwSize = sizeof( PROPSHEETPAGE );
	propPageMixing.dwFlags = PSP_DEFAULT;
	propPageMixing.hInstance = instance;
	propPageMixing.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_MIXING );
	propPageMixing.pfnDlgProc = OptionsProc;
	propPageMixing.lParam = reinterpret_cast<LPARAM>( &optionsMixing );
	pages.at( 3 ) = CreatePropertySheetPage( &propPageMixing );

	// Artwork property page
	PROPSHEETPAGE propPageArtwork = {};
	propPageArtwork.dwSize = sizeof( PROPSHEETPAGE );
	propPageArtwork.dwFlags = PSP_DEFAULT;
	propPageArtwork.hInstance = instance;
	propPageArtwork.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_ARTWORK );
	propPageArtwork.pfnDlgProc = OptionsProc;
	propPageArtwork.lParam = reinterpret_cast<LPARAM>( &optionsArtwork );
	pages.at( 4 ) = CreatePropertySheetPage( &propPageArtwork );

	PROPSHEETHEADER propSheetHeader = {};
	propSheetHeader.dwSize = sizeof( PROPSHEETHEADER );
	propSheetHeader.dwFlags = PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	propSheetHeader.hInstance = instance;
	propSheetHeader.hwndParent = parent;
	propSheetHeader.pszCaption = MAKEINTRESOURCE( IDS_OPTIONS_TITLE );
	propSheetHeader.nPages = pageCount;
	propSheetHeader.nStartPage = std::clamp( static_cast<UINT>( m_Settings.GetLastOptionsPage() ), 0u, pageCount - 1u );
	propSheetHeader.phpage = pages.data();

	if ( PropertySheet( &propSheetHeader ) > 0 ) {
		m_Output.SettingsChanged();
	}
}

DlgOptions::~DlgOptions()
{
	s_IsCentred = false;
}

void DlgOptions::LayoutPropertySheet( const HWND sheet, const HWND page )
{
	// The property sheet frame (tab control & buttons) is created from a system dialog template, so it does not use the font of the pages.
	// Apply the page font to the frame controls, give the buttons the standard (14 dialog unit) height, and enlarge the sheet to suit.
	const HFONT font = reinterpret_cast<HFONT>( SendMessage( page, WM_GETFONT, 0, 0 ) );
	const HWND tabWnd = PropSheet_GetTabControl( sheet );
	const HWND okWnd = GetDlgItem( sheet, IDOK );
	const HWND cancelWnd = GetDlgItem( sheet, IDCANCEL );
	if ( ( nullptr == font ) || ( nullptr == tabWnd ) || ( nullptr == okWnd ) || ( nullptr == cancelWnd ) ) {
		return;
	}

	RECT itemRect = {};
	TabCtrl_GetItemRect( tabWnd, 0, &itemRect );
	const int previousTabHeight = itemRect.bottom - itemRect.top;
	SendMessage( tabWnd, WM_SETFONT, reinterpret_cast<WPARAM>( font ), TRUE /*redraw*/ );
	TabCtrl_GetItemRect( tabWnd, 0, &itemRect );
	const int tabDelta = ( itemRect.bottom - itemRect.top ) - previousTabHeight;

	// Buttons have the same height as a combo box, and the standard (6 dialog unit) spacing.
	const int buttonHeight = GetDialogButtonHeight( page );
	RECT buttonRect = {};
	GetWindowRect( okWnd, &buttonRect );
	const int buttonDelta = buttonHeight - ( buttonRect.bottom - buttonRect.top );
	RECT gapRect = { 0, 0, 6, 0 };
	MapDialogRect( page, &gapRect );
	const int buttonGap = gapRect.right;

	// Grow the tab control (so that the page area is unchanged), and move the pages down.
	RECT rect = {};
	GetWindowRect( tabWnd, &rect );
	MapWindowRect( NULL /*hwndFrom*/, sheet, &rect );
	SetWindowPos( tabWnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top + tabDelta, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
	MovePropertySheetPage( sheet, page );

	// Move the buttons down, resize them, and space the OK button from the Cancel button.
	RECT cancelRect = {};
	GetWindowRect( cancelWnd, &cancelRect );
	MapWindowRect( NULL /*hwndFrom*/, sheet, &cancelRect );
	for ( const HWND button : { okWnd, cancelWnd } ) {
		SendMessage( button, WM_SETFONT, reinterpret_cast<WPARAM>( font ), TRUE /*redraw*/ );
		GetWindowRect( button, &rect );
		MapWindowRect( NULL /*hwndFrom*/, sheet, &rect );
		const int width = rect.right - rect.left;
		const int left = ( okWnd == button ) ? ( cancelRect.left - buttonGap - width ) : rect.left;
		SetWindowPos( button, NULL, left, rect.top + tabDelta, width, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE );
	}

	GetWindowRect( sheet, &rect );
	SetWindowPos( sheet, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top + tabDelta + buttonDelta, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
}

void DlgOptions::MovePropertySheetPage( const HWND sheet, const HWND page )
{
	// Position the page within the display area of the tab control.
	const HWND tabWnd = PropSheet_GetTabControl( sheet );
	if ( ( nullptr != tabWnd ) && ( nullptr != page ) ) {
		RECT rect = {};
		GetWindowRect( tabWnd, &rect );
		MapWindowRect( NULL /*hwndFrom*/, sheet, &rect );
		TabCtrl_AdjustRect( tabWnd, FALSE /*larger*/, &rect );
		RECT pageRect = {};
		GetWindowRect( page, &pageRect );
		MapWindowRect( NULL /*hwndFrom*/, sheet, &pageRect );
		SetWindowPos( page, NULL, pageRect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	}
}
