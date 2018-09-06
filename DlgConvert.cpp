#include "DlgConvert.h"

#include "resource.h"

#include "windowsx.h"

#include "DlgConvertFilename.h"
#include "Utility.h"

// All tracks entry ID.
static const long s_AllTracksID = 0;

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
					EndDialog( hwnd, 0 );
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
				case IDC_CONVERT_CONFIGURE : {
					DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnConfigure();
					}
					break;
				}
				case IDC_CONVERT_FORMAT : {
					if ( CBN_SELCHANGE == HIWORD( wParam ) ) {
						DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						if ( nullptr != dialog ) {
							dialog->UpdateSelectedEncoder();
						}					
					}
					break;
				}
				default : {
					break;
				}
			}
			break;
		}
		case WM_NOTIFY : {
			LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
			if ( ( nullptr != nmhdr ) && ( nmhdr->code == LVN_ITEMCHANGED ) ) {
				LPNMLISTVIEW nmListView = reinterpret_cast<LPNMLISTVIEW>( lParam );
				DlgConvert* dialog = reinterpret_cast<DlgConvert*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
				if ( nullptr != dialog ) {
					dialog->UpdateCheckedItems( nmListView->iItem );
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

DlgConvert::DlgConvert( const HINSTANCE instance, const HWND parent, Settings& settings, Handlers& handlers, const Playlist::ItemList& tracks, Playlist::ItemList& selectedTracks ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings ),
	m_Handlers( handlers ),
	m_Tracks( tracks ),
	m_SelectedTracks( selectedTracks ),
	m_CheckedItems(),
	m_Initialised( false ),
	m_SelectedEncoder(),
	m_Encoders()
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgConvert::~DlgConvert()
{
}

void DlgConvert::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );

	// Adjust dialog title if necessary.
	if ( !m_Tracks.empty() && ( MediaInfo::Source::CDDA == m_Tracks.begin()->Info.GetSource() ) ) {
		const int bufSize = 32;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_EXTRACT_TRACKS, buffer, bufSize );
		SetWindowText( m_hWnd, buffer );
	}

	// Initialise track list.
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

		for ( const auto& track : m_SelectedTracks ) {
			m_CheckedItems.insert( track.ID );
		}

		// Add all tracks.
		if ( !m_Tracks.empty() ) {
			LVITEM item = {};
			item.mask = LVIF_PARAM;
			item.iItem = ListView_GetItemCount( listWnd );
			item.iSubItem = 0;
			item.lParam = s_AllTracksID;
			item.iItem = ListView_InsertItem( listWnd, &item );
			if ( item.iItem >= 0 ) {
				LoadString( m_hInst, IDS_ALLTRACKS, buffer, bufSize );
				ListView_SetItemText( listWnd, item.iItem, 0, buffer );

				const BOOL state = ( m_CheckedItems.size() == m_Tracks.size() ) ? TRUE : FALSE;
				ListView_SetCheckState( listWnd, item.iItem, state );
				if ( state ) {
					m_CheckedItems.insert( s_AllTracksID );
				}
			}		
		}

		// Add track list.
		for ( const auto& track : m_Tracks ) {
			LVITEM item = {};
			item.mask = LVIF_PARAM;
			item.iItem = ListView_GetItemCount( listWnd );
			item.iSubItem = 0;
			item.lParam = static_cast<LPARAM>( track.ID );
			item.iItem = ListView_InsertItem( listWnd, &item );
			if ( item.iItem >= 0 ) {
				const std::wstring title = track.Info.GetTitle( true /*filenameAsTitle*/ );
				ListView_SetItemText( listWnd, item.iItem, 0, const_cast<LPWSTR>( title.c_str() ) );

				const BOOL state = ( m_CheckedItems.end() != m_CheckedItems.find( track.ID ) ) ? TRUE : FALSE;
				ListView_SetCheckState( listWnd, item.iItem, state );
			}
		}
	}

	// Initialise output format list.
	const HWND wndEncoders = GetDlgItem( m_hWnd, IDC_CONVERT_FORMAT );
	const Handler::List encoders = m_Handlers.GetEncoders();
	for ( const auto& encoder : encoders ) {
		if ( encoder ) {
			const std::wstring& description = encoder->GetDescription();
			ComboBox_AddString( wndEncoders, description.c_str() );
			m_Encoders.insert( EncoderMap::value_type( description, encoder ) );
		}
	}
	const std::wstring encoder = m_Settings.GetEncoder();
	if ( CB_ERR == ComboBox_SelectString( wndEncoders, -1, encoder.c_str() ) ) {
		ComboBox_SetCurSel( wndEncoders, 0 );
	}
	
	UpdateSelectedEncoder();

	UpdateFolderControl();


	if ( MediaInfo::Source::CDDA == m_Tracks.begin()->Info.GetSource() ) {
		const HWND addToLibraryWnd = GetDlgItem( m_hWnd, IDC_CONVERT_ADDTOLIBRARY );
		if ( nullptr != addToLibraryWnd ) {
			const int bufSize = 128;
			WCHAR buffer[ bufSize ] = {};
			LoadString( m_hInst, IDS_EXTRACT_ADDTOLIBRARY, buffer, bufSize );
			SetWindowText( addToLibraryWnd, buffer );
		}
	}

	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );
	CheckDlgButton( m_hWnd, IDC_CONVERT_ADDTOLIBRARY, extractToLibrary ? BST_CHECKED : BST_UNCHECKED );

	const HWND okWnd = GetDlgItem( m_hWnd, IDOK );
	EnableWindow( okWnd, m_SelectedTracks.empty() ? FALSE : TRUE );

	m_Initialised = true;
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

void DlgConvert::UpdateSelectedEncoder()
{
	const HWND wndEncoders = GetDlgItem( m_hWnd, IDC_CONVERT_FORMAT );
	if ( nullptr != wndEncoders ) {
		WCHAR selectedEncoder[ 256 ] = {};
		GetWindowText( wndEncoders, selectedEncoder, 256 );
		const auto encoderIter = m_Encoders.find( selectedEncoder );
		if ( m_Encoders.end() != encoderIter ) {
			m_SelectedEncoder = encoderIter->second;
		} else {
			m_SelectedEncoder.reset();
		}

		const BOOL enableConfigure = ( m_SelectedEncoder && m_SelectedEncoder->CanConfigureEncoder() ) ? TRUE : FALSE;
		const HWND hwndConfigure = GetDlgItem( m_hWnd, IDC_CONVERT_CONFIGURE );
		if ( nullptr != hwndConfigure ) {
			EnableWindow( hwndConfigure, enableConfigure );
		}
	}
}

void DlgConvert::UpdateCheckedItems( const int itemIndex )
{
	if ( m_Initialised ) {
		const HWND listWnd = GetDlgItem( m_hWnd, IDC_CONVERT_TRACKLIST );
		if ( nullptr != listWnd ) {
			const bool isChecked = ( FALSE != ListView_GetCheckState( listWnd, itemIndex ) );

			LVITEM item = {};
			item.iItem = itemIndex;
			item.mask = LVIF_PARAM;
			const long itemID = ( TRUE == ListView_GetItem( listWnd, &item ) ) ? static_cast<long>( item.lParam ) : 0;
			const bool wasChecked = ( m_CheckedItems.end() != m_CheckedItems.find( itemID ) );

			if ( isChecked != wasChecked ) {
				if ( s_AllTracksID == itemID ) {
					// Check or uncheck all tracks, as necessary
					if ( isChecked ) {
						m_CheckedItems.insert( s_AllTracksID );
						for ( const auto& track : m_Tracks ) {
							m_CheckedItems.insert( track.ID );
						}
					} else {
						m_CheckedItems.clear();
					}
					
					const int itemCount = ListView_GetItemCount( listWnd );
					for ( int index = 0; index < itemCount; index++ ) {
						ListView_SetCheckState( listWnd, index, isChecked );
					}

				} else {
					if ( isChecked ) {
						m_CheckedItems.insert( itemID );
						if ( m_CheckedItems.size() == m_Tracks.size() ) {
							m_CheckedItems.insert( s_AllTracksID );
							ListView_SetCheckState( listWnd, s_AllTracksID, TRUE );
						}
					} else {
						m_CheckedItems.erase( itemID );
						if ( m_CheckedItems.erase( s_AllTracksID ) > 0 ) {
							ListView_SetCheckState( listWnd, s_AllTracksID, FALSE );
						}
					}
				}

				const HWND okWnd = GetDlgItem( m_hWnd, IDOK );
				const BOOL wasOKEnabled = IsWindowEnabled( okWnd );
				const BOOL isOKEnabled = m_CheckedItems.empty() ? FALSE : TRUE;
				if ( wasOKEnabled != isOKEnabled ) {
					EnableWindow( okWnd, isOKEnabled );
				}
			}
		}
	}
}

void DlgConvert::OnFilenameFormat()
{
	const DlgConvertFilename dlgFilename( m_hInst, m_hWnd, m_Settings );
}

void DlgConvert::OnClose( const bool ok )
{
	m_SelectedTracks.clear();
	if ( ok ) {
		const HWND listWnd = GetDlgItem( m_hWnd, IDC_CONVERT_TRACKLIST );
		if ( nullptr != listWnd ) {
			int index = 0;
			auto iter = m_Tracks.begin();
			for ( const auto& track : m_Tracks ) {
				if ( FALSE != ListView_GetCheckState( listWnd, ++index ) ) {
					m_SelectedTracks.push_back( track );
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
	}
}

const Handler::Ptr DlgConvert::GetSelectedHandler() const
{
	return m_SelectedEncoder;
}

void DlgConvert::OnConfigure()
{
	if ( m_SelectedEncoder && m_SelectedEncoder->CanConfigureEncoder() ) {
		std::string settings = m_Settings.GetEncoderSettings( m_SelectedEncoder->GetDescription() );
		if ( m_SelectedEncoder->ConfigureEncoder( m_hInst, m_hWnd, settings ) ) {
			m_Settings.SetEncoderSettings( m_SelectedEncoder->GetDescription(), settings );
		}
	}
}
