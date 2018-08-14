#include "DlgConvert.h"

#include "resource.h"

#include "DlgConvertFilename.h"
#include "Utility.h"

INT_PTR CALLBACK DlgConvert::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgConvert* dialog = reinterpret_cast<DlgConvert*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL : 
				case IDOK : {
					DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnClose( ( IDOK == LOWORD( wParam ) ) );
					}
					EndDialog( hwnd, LOWORD( lParam ) );
					return TRUE;
				}
				case IDC_CONVERT_BROWSE : {
					DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->ChooseFolder();
					}
					break;
				}
				case IDC_CONVERT_FILENAME : {
					DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnFilenameFormat();
					}
					break;
				}
				default : {
					break;
				}
			}
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

int CALLBACK DlgConvert::BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData )
{
	if ( BFFM_INITIALIZED == uMsg ) {
		SendMessage( hwnd, BFFM_SETSELECTION, TRUE, lpData );
	}
	return 0;
}

DlgConvert::DlgConvert( const HINSTANCE instance, const HWND parent, Settings& settings, const MediaInfo::List& tracks ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings ),
	m_Tracks( tracks )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgConvert::~DlgConvert()
{
}

void DlgConvert::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;

	const HWND listWnd = GetDlgItem( m_hWnd, IDC_CONVERT_TRACKLIST );
	if ( nullptr != listWnd ) {
		ListView_SetExtendedListViewStyle( listWnd, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES );
		RECT rect = {};
		GetClientRect( listWnd, &rect );
		LVCOLUMN lvc = {};
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.cx = ( rect.right - rect.left - 40 );
		lvc.fmt = LVCFMT_LEFT;
		lvc.iSubItem = 0;
		const int bufSize = 32;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_TRACK, buffer, bufSize );
		lvc.pszText = buffer;
		ListView_InsertColumn( listWnd, 0, &lvc );

		for ( const auto& track : m_Tracks ) {
			LVITEM item = {};
			item.mask = LVIF_PARAM;
			item.iItem = ListView_GetItemCount( listWnd );
			item.iSubItem = 0;
			item.lParam = static_cast<LPARAM>( track.GetTrack() );
			item.iItem = ListView_InsertItem( listWnd, &item );
			if ( item.iItem >= 0 ) {
				const std::wstring title = track.GetTitle( true /*filenameAsTitle*/ );
				ListView_SetItemText( listWnd, item.iItem, 0, const_cast<LPWSTR>( title.c_str() ) );
				ListView_SetCheckState( listWnd, item.iItem, TRUE );
			}
		}
	}
	UpdateFolderControl();

	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );
	CheckDlgButton( m_hWnd, IDC_CONVERT_ADDTOLIBRARY, extractToLibrary ? BST_CHECKED : BST_UNCHECKED );
}

void DlgConvert::ChooseFolder()
{
	WCHAR title[ MAX_PATH ] = {};
	LoadString( m_hInst, IDS_CONVERTFOLDER, title, MAX_PATH );

	WCHAR buffer[ MAX_PATH ] = {};
	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );
	wcscpy_s( buffer, MAX_PATH, extractFolder.c_str() );

	BROWSEINFO bi = {};
	bi.hwndOwner = m_hWnd;
	bi.lpszTitle = title;
	bi.pszDisplayName = buffer;
	bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	bi.lpfn = &BrowseCallbackProc;
	bi.lParam = reinterpret_cast<LPARAM>( buffer );
	LPITEMIDLIST idlist = SHBrowseForFolder( &bi );
	if ( nullptr != idlist ) {
		const DWORD pathSize = 1024;
		WCHAR path[ pathSize ] = {};
		if ( TRUE == SHGetPathFromIDListEx( idlist, path, pathSize, GPFIDL_DEFAULT ) ) {
			extractFolder = path;
			m_Settings.SetExtractSettings( extractFolder, extractFilename, extractToLibrary );
			UpdateFolderControl();
		}
		CoTaskMemFree( idlist );
	}
}

void DlgConvert::UpdateFolderControl()
{
	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );
	SetDlgItemText( m_hWnd, IDC_CONVERT_FOLDER, extractFolder.c_str() );
}

void DlgConvert::OnFilenameFormat()
{
	const DlgConvertFilename dlgFilename( m_hInst, m_hWnd, m_Settings );
}

const MediaInfo::List& DlgConvert::GetSelectedTracks() const
{
	return m_Tracks;
}

void DlgConvert::OnClose( const bool ok )
{
	if ( ok ) {
		const HWND listWnd = GetDlgItem( m_hWnd, IDC_CONVERT_TRACKLIST );
		if ( nullptr != listWnd ) {
			int index = 0;
			auto iter = m_Tracks.begin();
			while ( m_Tracks.end() != iter ) {
				if ( FALSE == ListView_GetCheckState( listWnd, index++ ) ) {
					iter = m_Tracks.erase( iter );
				} else {
					++iter;
				}
			}
		}
		std::wstring extractFolder;
		std::wstring extractFilename;
		bool extractToLibrary = false;
		m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );
		const UINT checkState = IsDlgButtonChecked( m_hWnd, IDC_CONVERT_ADDTOLIBRARY );
		extractToLibrary = ( BST_CHECKED == checkState ) ? true : false;
		m_Settings.SetExtractSettings( extractFolder, extractFilename, extractToLibrary );
	} else {
		m_Tracks.clear();
	}
}
