#include "MusicBrainz.h"

#include "Utility.h"
#include "VUPlayer.h"

#include "json.hpp"

#include <filesystem>
#include <sstream>

// Match dialog icon size.
const int s_MatchDialogIconSize = 100;

// MusicBrainz API server.
static constexpr char s_MusicBrainzServer[]= "musicbrainz.org";

// MusicBrainz API for disc ID lookups.
static constexpr char s_MusicBrainzAPI[] = "/ws/2/discid/";

// Cover Art Archive server.
static constexpr char s_CoverArtArchiveServer[]= "coverartarchive.org";

// Cover Art Archive API for MusicBrainz release lookup.
static constexpr char s_CoverArtArchiveAPI[]= "/release/";

MusicBrainz::MusicBrainz( const HINSTANCE instance, const HWND hwnd, Settings& settings ) :
	m_hInst( instance ),
	m_hWnd( hwnd ),
	m_Settings( settings ),
	m_PendingQueries(),
	m_PendingQueriesMutex(),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_QueryThread( CreateThread( NULL /*attributes*/, 0 /*stackSize*/, QueryThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ ) ),
	m_ActiveQuery( false )
{
	const int bufSize = 32;
	char agent[ bufSize ] = {};
	LoadStringA( m_hInst, IDS_USERAGENT, agent, bufSize );
	m_InternetSession = InternetOpenA( agent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr /*proxy*/, nullptr /*proxyBypass*/, 0 /*flags*/ );
	if ( nullptr != m_InternetSession ) {
		m_InternetConnectionMusicBrainz = InternetConnectA( m_InternetSession, s_MusicBrainzServer, INTERNET_DEFAULT_HTTPS_PORT, nullptr /*username*/, nullptr /*password*/, INTERNET_SERVICE_HTTP, 0 /*flags*/, 0 /*context*/ );
		m_InternetConnectionCoverArtArchive = InternetConnectA( m_InternetSession, s_CoverArtArchiveServer, INTERNET_DEFAULT_HTTPS_PORT, nullptr /*username*/, nullptr /*password*/, INTERNET_SERVICE_HTTP, 0 /*flags*/, 0 /*context*/ );
	}
}

MusicBrainz::~MusicBrainz()
{
	if ( nullptr != m_QueryThread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_QueryThread, INFINITE );
		CloseHandle( m_QueryThread );
	}
	if ( nullptr != m_StopEvent ) {
		CloseHandle( m_StopEvent );
	}
	if ( nullptr != m_WakeEvent ) {
		CloseHandle( m_WakeEvent );
	}
	if ( nullptr != m_InternetConnectionMusicBrainz ) {
		InternetCloseHandle( m_InternetConnectionMusicBrainz );
	}
	if ( nullptr != m_InternetConnectionCoverArtArchive ) {
		InternetCloseHandle( m_InternetConnectionCoverArtArchive );
	}
	if ( nullptr != m_InternetSession ) {
		InternetCloseHandle( m_InternetSession );
	}
}

DWORD WINAPI MusicBrainz::QueryThreadProc( LPVOID lParam )
{
	MusicBrainz* musicBrainz = static_cast<MusicBrainz*>( lParam );
	if ( nullptr != musicBrainz ) {
		musicBrainz->QueryHandler();
	}
	return 0;
}

INT_PTR CALLBACK MusicBrainz::MatchDialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( lParam );
			MusicBrainz* musicBrainz = ( nullptr != matchInfo ) ? matchInfo->m_MusicBrainz : nullptr;
			if ( nullptr != musicBrainz ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				musicBrainz->OnInitMatchDialog( hwnd, matchInfo->m_Result );
			}
			return TRUE;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDOK :
				case IDCANCEL : {
					int selection = -1;
					if ( IDOK == LOWORD( wParam ) ) {
						MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						if ( nullptr != matchInfo ) {
							const HWND hwndList = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES );
							if ( nullptr != hwndList ) {
								LVITEMINDEX itemIndex = {};
								itemIndex.iItem = -1;
								if ( FALSE != ListView_GetNextItemIndex( hwndList, &itemIndex, LVNI_ALL | LVNI_SELECTED ) ) {
									selection = itemIndex.iItem;
								}
							}
						}
					}
					EndDialog( hwnd, selection );
					return TRUE;
				}
				default : {
					break;
				}
			}
			break;
		}
		case WM_NOTIFY : {
			LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
			if ( nullptr != nmhdr ) {
				const HWND hwndList = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES );
				if ( hwndList == nmhdr->hwndFrom ) {
					if ( NM_DBLCLK == nmhdr->code ) {
						LPNMITEMACTIVATE nmItemActivate = reinterpret_cast<LPNMITEMACTIVATE>( lParam );
						EndDialog( hwnd, nmItemActivate->iItem );
					} else if ( LVN_ITEMCHANGED == nmhdr->code ) {
						MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						MusicBrainz* musicBrainz = ( nullptr != matchInfo ) ? matchInfo->m_MusicBrainz : nullptr;
						if ( nullptr != musicBrainz ) {
							musicBrainz->UpdateDialogTrackList( hwnd, matchInfo->m_Result );
							musicBrainz->UpdateDialogArtwork( hwnd, matchInfo->m_Result );
						}

						const HWND hwndOK = GetDlgItem( hwnd, IDOK );
						if ( nullptr != hwndOK ) {
							const BOOL okEnabled = IsWindowEnabled( hwndOK );
							const int selectedCount = ListView_GetSelectedCount( hwndList );
							if ( selectedCount > 0 ) {
								if ( FALSE == okEnabled ) {
									EnableWindow( hwndOK, TRUE );
								}
							} else {
								if ( TRUE == okEnabled ) {
									EnableWindow( hwndOK, FALSE );
								}
							}
						}
					}
				}
			}
			break;
		}
		case WM_DESTROY : {
			const HWND hwndList = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES );
			if ( nullptr != hwndList ) {
				HIMAGELIST imageList = ListView_GetImageList( hwndList, LVSIL_NORMAL );
				if ( nullptr != imageList ) {
					const int imageCount = ImageList_GetImageCount( imageList );
					for ( int imageIndex = 0; imageIndex < imageCount; imageIndex++ ) {
						IMAGEINFO imageInfo = {};
						ImageList_GetImageInfo( imageList, imageIndex, &imageInfo );
						if ( nullptr != imageInfo.hbmImage ) {
							DeleteObject( imageInfo.hbmImage );
						}
					}
				}
			}
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_DRAWITEM : {
			MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			MusicBrainz* musicBrainz = ( nullptr != matchInfo ) ? matchInfo->m_MusicBrainz : nullptr;
			DRAWITEMSTRUCT* drawitem = reinterpret_cast<DRAWITEMSTRUCT*>( lParam );
			if ( ( nullptr != drawitem ) && ( nullptr != musicBrainz ) ) {
				musicBrainz->OnDrawArtwork( drawitem );
			}
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

void MusicBrainz::Query( const std::string& discID, const std::string& toc, const bool forceDialog, const std::string& playlistID )
{
	std::lock_guard<std::mutex> lock( m_PendingQueriesMutex );
	bool addPending = true;
	auto pendingQuery = m_PendingQueries.begin();
	while ( addPending && ( m_PendingQueries.end() != pendingQuery ) ) {
		const auto& [ pendingDiscID, pendingTOC, _ ] = pendingQuery->first;
		addPending = ( discID != pendingDiscID );
		++pendingQuery;
	}
	if ( addPending ) {
		m_PendingQueries.push_back( { { discID, toc, playlistID }, forceDialog } );
		SetEvent( m_WakeEvent );
	}
}

void MusicBrainz::QueryHandler()
{
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };

	CanContinue canContinue( [ handle = m_StopEvent ] ()
	{
		return ( WAIT_OBJECT_0 != WaitForSingleObject( handle, 0 ) );
	} );

	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		PendingQuery pendingQuery;
		{
			std::lock_guard<std::mutex> lock( m_PendingQueriesMutex );
			if ( !m_PendingQueries.empty() ) {
				pendingQuery = m_PendingQueries.front();
				m_PendingQueries.pop_front();
			}
		}

		const auto& [ discID, toc, playlistID ] = pendingQuery.first;
		const bool forceDialog = pendingQuery.second;

		if ( !discID.empty() && !toc.empty() ) {
			Result* result = new Result();
			result->DiscID = discID;
      result->PlaylistID = playlistID;

			if ( m_Settings.GetMusicBrainzEnabled() ) {
				m_ActiveQuery = true;
				const auto response = LookupDisc( discID, toc );
				ParseDiscResponse( response, *result, canContinue );
				m_ActiveQuery = false;
			}

			if ( result->Albums.empty() || !canContinue() ) {
				delete result;
				result = nullptr;
			} else {
				PostMessage( m_hWnd, MSG_MUSICBRAINZQUERYRESULT, reinterpret_cast<WPARAM>( result ), forceDialog );
			}
		}

		std::lock_guard<std::mutex> lock( m_PendingQueriesMutex );
		auto pendingIter = m_PendingQueries.begin();
		while ( m_PendingQueries.end() != pendingIter ) {
			const auto& [ pendingDiscID, pendingTOC, _ ] = pendingIter->first;
			if ( pendingDiscID == toc ) {
				pendingIter = m_PendingQueries.erase( pendingIter );
			} else {
				++pendingIter;
			}
		}
		if ( m_PendingQueries.empty() ) {
			ResetEvent( m_WakeEvent );
		}
	}
}

int MusicBrainz::ShowMatchesDialog( const Result& result )
{
	MatchInfo* matchInfo = new MatchInfo( { result, this } );
	const int selection = static_cast<int>( DialogBoxParam( m_hInst, MAKEINTRESOURCE( IDD_MUSICBRAINZ_MATCHES ), m_hWnd, MatchDialogProc, reinterpret_cast<LPARAM>( matchInfo ) ) );
	delete matchInfo;
	return selection;
}

void MusicBrainz::OnInitMatchDialog( const HWND hwnd, const Result& result )
{
	CentreDialog( hwnd );

	const HWND hwndAlbums = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES );
	const HWND hwndTracks = GetDlgItem( hwnd, IDC_MUSICBRAINZ_TRACKS );
	if ( ( nullptr != hwndAlbums ) && ( nullptr != hwndTracks ) ) {
		// Set up the matches list control.
		{
			ListView_SetExtendedListViewStyle( hwndAlbums, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );
			ListView_SetExtendedListViewStyleEx( hwndAlbums, LVS_EX_INFOTIP, LVS_EX_INFOTIP );

			RECT rect = {};
			GetClientRect( hwndAlbums, &rect );
			LVCOLUMN lvc = {};
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;

			const int bufSize = 32;
			WCHAR buffer[ bufSize ] = {};
			lvc.pszText = buffer;

			int totalWidth = rect.right - rect.left - static_cast<int>( 20 * GetDPIScaling() /*scrollbar*/ );
			const int yearWidth = static_cast<int>( 48 * GetDPIScaling() );
			totalWidth -= yearWidth;
			const int titleWidth = totalWidth / 2;
			const int artistWidth = titleWidth;

			lvc.iSubItem = 0;
			lvc.cx = titleWidth;
			LoadString( m_hInst, IDS_COLUMN_ALBUM, buffer, bufSize );
			ListView_InsertColumn( hwndAlbums, 0, &lvc );

			lvc.iSubItem = 1;
			lvc.cx = artistWidth;
			LoadString( m_hInst, IDS_COLUMN_ARTIST, buffer, bufSize );
			ListView_InsertColumn( hwndAlbums, 1, &lvc );

			lvc.iSubItem = 2;
			lvc.cx = yearWidth;
			lvc.fmt = LVCFMT_CENTER;
			LoadString( m_hInst, IDS_COLUMN_YEAR, buffer, bufSize );
			ListView_InsertColumn( hwndAlbums, 2, &lvc );
		}

		// Set up the track list control.
		{
			ListView_SetExtendedListViewStyle( hwndTracks, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );
			ListView_SetExtendedListViewStyleEx( hwndTracks, LVS_EX_INFOTIP, LVS_EX_INFOTIP );

			RECT rect = {};
			GetClientRect( hwndTracks, &rect );
			LVCOLUMN lvc = {};
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;

			const int bufSize = 32;
			WCHAR buffer[ bufSize ] = {};
			lvc.pszText = buffer;

			int totalWidth = rect.right - rect.left - static_cast<int>( 20 * GetDPIScaling() /*scrollbar*/ );
			const int trackWidth = static_cast<int>( 24 * GetDPIScaling() );
			totalWidth -= trackWidth;
			const int yearWidth = static_cast<int>( 48 * GetDPIScaling() );
			totalWidth -= yearWidth;
			const int titleWidth = totalWidth / 2;
			const int artistWidth = titleWidth;

			lvc.iSubItem = 0;
			lvc.cx = trackWidth;
			ListView_InsertColumn( hwndTracks, 0, &lvc );

			lvc.mask |= LVCF_TEXT;
			lvc.iSubItem = 1;
			lvc.cx = titleWidth;
			LoadString( m_hInst, IDS_COLUMN_TITLE, buffer, bufSize );
			ListView_InsertColumn( hwndTracks, 1, &lvc );

			lvc.iSubItem = 2;
			lvc.cx = artistWidth;
			LoadString( m_hInst, IDS_COLUMN_ARTIST, buffer, bufSize );
			ListView_InsertColumn( hwndTracks, 2, &lvc );

			lvc.iSubItem = 3;
			lvc.cx = yearWidth;
			lvc.fmt = LVCFMT_CENTER;
			LoadString( m_hInst, IDS_COLUMN_YEAR, buffer, bufSize );
			ListView_InsertColumn( hwndTracks, 3, &lvc );
		}

		// Populate the matches list.
		for ( const auto& album : result.Albums ) {
			LVITEM item = {};
			item.mask = LVIF_TEXT;
			item.pszText = const_cast<LPWSTR>( album.Title.c_str() );
			item.iItem = ListView_GetItemCount( hwndAlbums );
			item.iSubItem = 0;
			if ( item.iItem = ListView_InsertItem( hwndAlbums, &item ); item.iItem >= 0 ) {
				ListView_SetItemText( hwndAlbums, item.iItem, 1, const_cast<LPWSTR>( album.Artist.c_str() ) );
				const std::wstring yearString = std::to_wstring( album.Year );
				ListView_SetItemText( hwndAlbums, item.iItem, 2, const_cast<LPWSTR>( yearString.c_str() ) );
			}
		}

		if ( ListView_GetItemCount( hwndAlbums ) > 0 ) {
			ListView_SetItemState( hwndAlbums, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		}
	}
}

void MusicBrainz::UpdateDialogTrackList( const HWND hwnd, const Result& result )
{
	const HWND hwndAlbums = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES );
	const HWND hwndTracks = GetDlgItem( hwnd, IDC_MUSICBRAINZ_TRACKS );
	if ( ( nullptr != hwndAlbums ) && ( nullptr != hwndTracks ) ) {
		ListView_DeleteAllItems( hwndTracks );
		if ( const size_t selectedItem = static_cast<size_t>( ListView_GetNextItem( hwndAlbums, -1, LVNI_SELECTED ) ); selectedItem < result.Albums.size() ) {
			const auto& album = result.Albums[ selectedItem ];
			for ( const auto& track : album.Tracks ) {
				const std::wstring tracknumber = std::to_wstring( track.first );
				const auto& [ title, artist, year ] = track.second;
				LVITEM item = {};
				item.mask = LVIF_TEXT;
				item.pszText = const_cast<LPWSTR>( tracknumber.c_str() );
				item.iItem = ListView_GetItemCount( hwndTracks );
				item.iSubItem = 0;
				if ( item.iItem = ListView_InsertItem( hwndTracks, &item ); item.iItem >= 0 ) {
					ListView_SetItemText( hwndTracks, item.iItem, 1, const_cast<LPWSTR>( title.c_str() ) );
					ListView_SetItemText( hwndTracks, item.iItem, 2, const_cast<LPWSTR>( artist.c_str() ) );
					const std::wstring yearString = std::to_wstring( year );
					ListView_SetItemText( hwndTracks, item.iItem, 3, const_cast<LPWSTR>( yearString.c_str() ) );
				}
			}
		}
	}
}

void MusicBrainz::UpdateDialogArtwork( const HWND hwnd, const Result& result )
{
	// Load artwork.
	m_Artwork.reset();
	if ( const HWND hwndAlbums = GetDlgItem( hwnd, IDC_MUSICBRAINZ_MATCHES ); nullptr != hwndAlbums ) {
		if ( const size_t selectedItem = static_cast<size_t>( ListView_GetNextItem( hwndAlbums, -1, LVNI_SELECTED ) ); selectedItem < result.Albums.size() ) {
			const auto& artwork = result.Albums[ selectedItem ].Artwork;
			if ( !artwork.empty() ) {
				IStream* stream = nullptr;
				if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
					if ( SUCCEEDED( stream->Write( &artwork[ 0 ], static_cast<ULONG>( artwork.size() ), NULL /*bytesWritten*/ ) ) ) {
						try {
							m_Artwork = std::make_unique<Gdiplus::Bitmap>( stream );
						} catch ( ... ) {
						}
					}
					stream->Release();
				}
			}
		}
	}

	// Update artwork border.
	const int bufSize = 32;
	WCHAR buffer[ bufSize ] = {};
	LoadString( m_hInst, IDS_ARTWORK, buffer, bufSize );
	std::wstring borderText = buffer;
	borderText += L" - ";
	UINT width = 0;
	UINT height = 0;
	if ( m_Artwork ) {
		width = m_Artwork->GetWidth();
		height = m_Artwork->GetHeight();
	}
	if ( ( width > 0 ) && ( height > 0 ) ) {
		borderText += std::to_wstring( width );
		borderText += L" x ";
		borderText += std::to_wstring( height );
		LoadString( m_hInst, IDS_UNITS_PX, buffer, bufSize );
		borderText += L' ';
		borderText += buffer;
	} else {
		LoadString( m_hInst, IDS_ARTWORK_NONE, buffer, bufSize );
		borderText += buffer;
	}
	SetDlgItemText( hwnd, IDC_MUSICBRAINZ_ARTWORK_BORDER, borderText.c_str() );

	// Redraw artwork.
	InvalidateRect( GetDlgItem( hwnd, IDC_MUSICBRAINZ_ARTWORK ), nullptr /*rect*/, FALSE /*erase*/ );
}

void MusicBrainz::OnDrawArtwork( DRAWITEMSTRUCT* drawItemStruct )
{
	if ( ( nullptr != drawItemStruct ) && ( IDC_MUSICBRAINZ_ARTWORK == drawItemStruct->CtlID ) ) {
		const int clientWidth = drawItemStruct->rcItem.right - drawItemStruct->rcItem.left;
		const int clientHeight = drawItemStruct->rcItem.bottom - drawItemStruct->rcItem.top;
		Gdiplus::Rect clientRect( 0, 0, clientWidth, clientHeight );
		Gdiplus::Graphics graphics( drawItemStruct->hDC );
		const COLORREF colour = GetBkColor( drawItemStruct->hDC );
		graphics.Clear( { GetRValue( colour ), GetGValue( colour ), GetBValue( colour ) } );
		if ( m_Artwork ) {
			const UINT bitmapWidth = m_Artwork->GetWidth();
			const UINT bitmapHeight = m_Artwork->GetHeight();
			if ( ( clientWidth > 0 ) && ( clientHeight > 0 ) && ( bitmapHeight > 0 ) && ( bitmapWidth > 0 ) ) {
				const float clientAspect = static_cast<float>( clientWidth ) / clientHeight;
				const float bitmapAspect = static_cast<float>( bitmapWidth ) / bitmapHeight;
				if ( bitmapAspect < clientAspect ) {
					clientRect.Width = static_cast<INT>( clientHeight * bitmapAspect );
					clientRect.X = static_cast<INT>( ( clientWidth - clientRect.Width ) / 2 );
				} else {
					clientRect.Height = static_cast<INT>( clientWidth / bitmapAspect );
					clientRect.Y = static_cast<INT>( ( clientHeight - clientRect.Height ) / 2 );
				}
			}
			graphics.SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBilinear );
			graphics.DrawImage( m_Artwork.get(), clientRect );
		}
	}
}

bool MusicBrainz::IsActive() const
{
	return m_ActiveQuery.load();
}

bool MusicBrainz::IsAvailable() const
{
	return ( nullptr != m_QueryThread );
}

std::string MusicBrainz::LookupDisc( const std::string& discID, const std::string& toc ) const
{
	std::string response;
	if ( ( nullptr != m_InternetConnectionMusicBrainz ) && !discID.empty() && !toc.empty() ) {
		static const bool stubs = false;
		const char* acceptTypes[] = { "application/json", nullptr };		
		const DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
		std::stringstream objectName;
		objectName << s_MusicBrainzAPI;
		objectName << discID;
		objectName << "?toc=";
		objectName << toc;
		objectName << "&inc=recordings+artist-credits";
		objectName << "&fmt=json";
		if ( !stubs ) {
			objectName << "&cdstubs=no";
		}

		const HINTERNET hRequest = HttpOpenRequestA( m_InternetConnectionMusicBrainz, nullptr /*verb*/, objectName.str().c_str(), nullptr /*version*/, nullptr /*referrer*/, acceptTypes, flags, 0 /*context*/ );
		if ( nullptr != hRequest ) {
			if ( HttpSendRequestA( hRequest, nullptr /*headers*/, 0 /*headersLength*/, nullptr /*optional*/, 0 /*optionalLength*/ ) ) {
				DWORD bufferLength = 8;
				std::vector<char> statusCode( bufferLength );
				DWORD headerIndex = 0;
				if ( HttpQueryInfoA( hRequest, HTTP_QUERY_STATUS_CODE, statusCode.data(), &bufferLength, &headerIndex ) && ( std::to_string( HTTP_STATUS_OK ) == statusCode.data() ) ) {
					DWORD bytesAvailable = 0;
					while ( InternetQueryDataAvailable( hRequest, &bytesAvailable, 0 /*flags*/, 0 /*context*/ ) && ( bytesAvailable > 0 ) ) {
						DWORD bytesRead = 0;
						std::vector<char> buffer( bytesAvailable );
						if ( InternetReadFile( hRequest, buffer.data(), bytesAvailable, &bytesRead ) && ( bytesRead > 0 ) ) {
							response += std::string( buffer.data(), bytesRead );
						}
					}
				}
			}
			InternetCloseHandle( hRequest );
		}
	}
	return response;
}

std::vector<unsigned char> MusicBrainz::LookupCoverArt( const std::string& releaseID ) const
{
	std::vector<unsigned char> response;
	if ( ( nullptr != m_InternetConnectionCoverArtArchive ) && !releaseID.empty() ) {
		static const bool stubs = false;
		const char* acceptTypes[] = { "image/jpeg", "image/png", "image/bmp", "image/gif", "image/tiff", nullptr };		
		const DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
		std::stringstream objectName;
		objectName << s_CoverArtArchiveAPI;
		objectName << releaseID;
		objectName << "/front";

		const HINTERNET hRequest = HttpOpenRequestA( m_InternetConnectionCoverArtArchive, nullptr /*verb*/, objectName.str().c_str(), nullptr /*version*/, nullptr /*referrer*/, acceptTypes, flags, 0 /*context*/ );
		if ( nullptr != hRequest ) {
			if ( HttpSendRequestA( hRequest, nullptr /*headers*/, 0 /*headersLength*/, nullptr /*optional*/, 0 /*optionalLength*/ ) ) {
				DWORD bufferLength = 8;
				std::vector<char> statusCode( bufferLength );
				DWORD headerIndex = 0;
				if ( HttpQueryInfoA( hRequest, HTTP_QUERY_STATUS_CODE, statusCode.data(), &bufferLength, &headerIndex ) && ( std::to_string( HTTP_STATUS_OK ) == statusCode.data() ) ) {
					DWORD bytesAvailable = 0;
					while ( InternetQueryDataAvailable( hRequest, &bytesAvailable, 0 /*flags*/, 0 /*context*/ ) && ( bytesAvailable > 0 ) ) {
						DWORD bytesRead = 0;
						std::vector<char> buffer( bytesAvailable );
						if ( InternetReadFile( hRequest, buffer.data(), bytesAvailable, &bytesRead ) && ( bytesRead > 0 ) ) {
							response.insert( response.end(), buffer.data(), buffer.data() + bytesRead );
						}
					}
				}
			}
			InternetCloseHandle( hRequest );
		}
	}
	return response;
}

static long ParseYear( const std::string& date )
{
	long year = 0;
	if ( const size_t pos = date.find_first_not_of( "0123456789" ); ( 4 == pos ) || ( ( std::string::npos == pos ) && ( 4 == date.size() ) ) ) {
		try {
			year = std::stol( date.substr( 0, pos ) );
		} catch ( const std::logic_error& ) {}
	}
	return year;
}

static std::wstring ParseArtistCredit( const nlohmann::json& artistCredit )
{
	std::wstring result;
	if ( artistCredit.is_array() ) {
		for ( const auto& artist : artistCredit ) {
			if ( const auto name = artist.find( "name" ); ( artist.end() != name ) && name->is_string() ) {
				result += UTF8ToWideString( *name );
			}
			if ( const auto joinphrase = artist.find( "joinphrase" ); ( artist.end() != joinphrase ) && joinphrase->is_string() ) {
				result += UTF8ToWideString( *joinphrase );
			}
		}
	}
	return result;
}

bool MusicBrainz::ParseDiscResponse( const std::string& response, Result& result, CanContinue canContinue ) const
{
	result.Albums.clear();
	if ( !response.empty() ) {
		try {
			const nlohmann::json document = nlohmann::json::parse( response );
			const auto releases = document.find( "releases" );
			if ( ( document.end() != releases ) && releases->is_array() ) {

				auto albumCompare = []( const Album& a, const Album& b )
				{
					return std::tie( a.Artist, a.Title, a.Year, a.Tracks, a.Artwork ) < std::tie( b.Artist, b.Title, b.Year, b.Tracks, b.Artwork );
				};
				std::set<Album, decltype( albumCompare )> albums( albumCompare );

				// Get release information.
				for ( const auto& release : *releases ) {
					std::vector<Album> releaseAlbums;
					std::wstring releaseTitle;
					std::wstring releaseArtist;
					long releaseYear = 0;
					
					if ( const auto title = release.find( "title" ); ( release.end() != title ) && title->is_string() ) {
						releaseTitle = UTF8ToWideString( *title );
					}
					
					if ( const auto date = release.find( "date" ); ( release.end() != date ) && date->is_string() ) {
						releaseYear = ParseYear( *date );
					}
					
					if ( const auto artistCredit = release.find( "artist-credit" ); ( release.end() != artistCredit ) && artistCredit->is_array() ) {
						releaseArtist = ParseArtistCredit( *artistCredit );
					}

					if ( const auto media = release.find( "media" ); ( release.end() != media ) && media->is_array() ) {
						for ( const auto& medium : *media ) {
							// If this is an exact match, we want to ignore all other media in this release.
							bool exactMatch = false;
							if ( const auto discs = medium.find( "discs" ); ( medium.end() != discs ) && discs->is_array() ) {
								for ( const auto& disc : *discs ) {
									if ( const auto discID = disc.find( "id" ); ( disc.end() != discID ) && discID->is_string() ) {
										exactMatch = ( result.DiscID == std::string( *discID ) );
										if ( exactMatch ) {
											releaseAlbums.clear();
										}
									}
								}
							}

							if ( const auto tracks = medium.find( "tracks" ); ( medium.end() != tracks ) && tracks->is_array() ) {
								Album album = {};
								album.Title = releaseTitle;
								album.Artist = releaseArtist;
								album.Year = releaseYear;
								for ( const auto& track : *tracks ) {
									if ( const auto trackposition = track.find( "position" ); ( track.end() != trackposition ) && trackposition->is_number() ) {
										if ( const auto title = track.find( "title" ); ( track.end() != title ) && title->is_string() ) {
											const std::wstring trackTitle = UTF8ToWideString( *title );

											std::wstring trackArtist;
											if ( const auto artistCredit = track.find( "artist-credit" ); ( track.end() != artistCredit ) && artistCredit->is_array() ) {
												trackArtist = ParseArtistCredit( *artistCredit );
											}

											long trackYear = 0;
											if ( const auto recording = track.find( "recording" ); ( track.end() != recording ) && recording->is_object() ) {
												if ( const auto date = recording->find( "first-release-date" ); ( recording->end() != date ) && date->is_string() ) {
													trackYear = ParseYear( *date );
												}
											}

											album.Tracks.emplace( *trackposition, std::make_tuple( trackTitle, trackArtist, trackYear ) );
										}
									}
								}
								releaseAlbums.emplace_back( album );
							}

							if ( exactMatch ) {
								break;
							}
						}
					}

					// Lookup artwork.
					if ( const auto id = release.find( "id" ); ( release.end() != id ) && id->is_string() && ( ( nullptr == canContinue ) || canContinue() ) ) {
						const auto artwork = LookupCoverArt( *id );
						for ( auto& album : releaseAlbums ) {
							album.Artwork = artwork;
						}
					}

					albums.insert( releaseAlbums.begin(), releaseAlbums.end() );
				}

				result.Albums.insert( result.Albums.end(), albums.begin(), albums.end() );
			}
		} catch ( const nlohmann::json::exception& ) {

		}
	}
	return !result.Albums.empty();
}
