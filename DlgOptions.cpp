#include "DlgOptions.h"

#include "OptionsGeneral.h"
#include "OptionsHotkeys.h"
#include "OptionsMod.h"
#include "OptionsLoudness.h"

#include "resource.h"
#include "Utility.h"
#include "windowsx.h"

bool DlgOptions::s_IsCentred = false;

// Property sheet procedure
INT_PTR CALLBACK DlgOptions::OptionsProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			PROPSHEETPAGE* page = reinterpret_cast<PROPSHEETPAGE*>( lParam );
			if ( nullptr != page ) {
				Options* options = reinterpret_cast<Options*>( page->lParam );
				if ( nullptr != options ) {
					SetWindowLongPtr( hwnd, DWLP_USER, page->lParam );
					options->OnInit( hwnd );
				}
			}
			if ( !s_IsCentred ) {
				s_IsCentred = true;
				CentreDialog( GetParent( hwnd ) );
			}
			break;
		}
		case WM_DESTROY : {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_NOTIFY : {
			LPPSHNOTIFY pshNotify = reinterpret_cast<LPPSHNOTIFY>( lParam );
			if ( nullptr != pshNotify ) {
				Options* options = reinterpret_cast<Options*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
				if ( nullptr != options ) {
					if ( PSN_APPLY == pshNotify->hdr.code ) {
						options->OnSave( hwnd );
					} else {
						options->OnNotify( hwnd, wParam, lParam );
					}
				}
			}
			break;
		}
		case WM_COMMAND : {
			Options* options = reinterpret_cast<Options*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != options ) {
				options->OnCommand( hwnd, wParam, lParam );
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
	const UINT pageCount = 4;
	std::vector<HPROPSHEETPAGE> pages( pageCount );

	OptionsGeneral optionsGeneral( instance, settings, output );
	OptionsHotkeys optionsHotkeys( instance, settings, output );
	OptionsMod optionsMod( instance, settings, output );
	OptionsLoudness optionsLoudness( instance, settings, output );

	// General property page
	PROPSHEETPAGE propPageGeneral = {};
	propPageGeneral.dwSize = sizeof( PROPSHEETPAGE );
	propPageGeneral.dwFlags = PSP_DEFAULT;
	propPageGeneral.hInstance = instance;
	propPageGeneral.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_GENERAL );
	propPageGeneral.pfnDlgProc = OptionsProc;
	propPageGeneral.lParam = reinterpret_cast<LPARAM>( &optionsGeneral );
	pages.at( 0 ) = CreatePropertySheetPage( &propPageGeneral );
	
	// Hotkeys property page
	PROPSHEETPAGE propPageHotkeys = {};
	propPageHotkeys.dwSize = sizeof( PROPSHEETPAGE );
	propPageHotkeys.dwFlags = PSP_DEFAULT;
	propPageHotkeys.hInstance = instance;
	propPageHotkeys.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_HOTKEYS );
	propPageHotkeys.pfnDlgProc = OptionsProc;
	propPageHotkeys.lParam = reinterpret_cast<LPARAM>( &optionsHotkeys );
	pages.at( 1 ) = CreatePropertySheetPage( &propPageHotkeys );

	// MOD Music property page
	PROPSHEETPAGE propPageMODMusic = {};
	propPageMODMusic.dwSize = sizeof( PROPSHEETPAGE );
	propPageMODMusic.dwFlags = PSP_DEFAULT;
	propPageMODMusic.hInstance = instance;
	propPageMODMusic.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_MOD );
	propPageMODMusic.pfnDlgProc = OptionsProc;
	propPageMODMusic.lParam = reinterpret_cast<LPARAM>( &optionsMod );
	pages.at( 2 ) = CreatePropertySheetPage( &propPageMODMusic );

	// Loudness property page
	PROPSHEETPAGE propPageLoudness = {};
	propPageLoudness.dwSize = sizeof( PROPSHEETPAGE );
	propPageLoudness.dwFlags = PSP_DEFAULT;
	propPageLoudness.hInstance = instance;
	propPageLoudness.pszTemplate = MAKEINTRESOURCE( IDD_OPTIONS_GAIN );
	propPageLoudness.pfnDlgProc = OptionsProc;
	propPageLoudness.lParam = reinterpret_cast<LPARAM>( &optionsLoudness );
	pages.at( 3 ) = CreatePropertySheetPage( &propPageLoudness );

	PROPSHEETHEADER propSheetHeader = {};
	propSheetHeader.dwSize = sizeof( PROPSHEETHEADER );
	propSheetHeader.dwFlags = PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	propSheetHeader.hInstance = instance;
	propSheetHeader.hwndParent = parent;
	propSheetHeader.pszCaption = MAKEINTRESOURCE( IDS_OPTIONS_TITLE );
	propSheetHeader.nPages = pageCount;
	propSheetHeader.nStartPage = 0;
	propSheetHeader.phpage = &pages[ 0 ];

	if ( PropertySheet( &propSheetHeader ) > 0 ) {
		m_Output.SettingsChanged();
	}
}

DlgOptions::~DlgOptions()
{
	s_IsCentred = false;
}
