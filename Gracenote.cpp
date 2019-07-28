#include "Gracenote.h"

#include "Utility.h"
#include "VUPlayer.h"

// Match dialog icon size.
const int s_MatchDialogIconSize = 100;

Gracenote::Gracenote( const HINSTANCE instance, const HWND hwnd, Settings& settings, const bool disable ) :
	m_hInst( instance ),
	m_hWnd( hwnd ),
	m_Settings( settings ),
	m_Handle( disable ? nullptr : LoadLibrary( L"gnsdk_vuplayer.dll" ) ),
	m_Init( nullptr ),
	m_Close( nullptr ),
	m_Query( nullptr ),
	m_FreeResult( nullptr ),
	m_Initialised( false ),
	m_PendingQueries(),
	m_PendingQueriesMutex(),
	m_QueryThread( nullptr ),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_ActiveQuery( false )
{
	if ( nullptr != m_Handle ) {
		m_Init = reinterpret_cast<gn_init>( GetProcAddress( m_Handle, "init" ) );
		m_Close = reinterpret_cast<gn_close>( GetProcAddress( m_Handle, "close" ) );
		m_Query = reinterpret_cast<gn_query>( GetProcAddress( m_Handle, "query" ) );
		m_FreeResult = reinterpret_cast<gn_free_result>( GetProcAddress( m_Handle, "free_result" ) );

		if ( ( nullptr == m_Init ) || ( nullptr == m_Close ) || ( nullptr == m_Query ) || ( nullptr == m_FreeResult ) ) {
			FreeLibrary( m_Handle );
			m_Handle = nullptr;
		} else {
			m_QueryThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, QueryThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
		}
	}
}

Gracenote::~Gracenote()
{
	if ( nullptr != m_QueryThread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_QueryThread, INFINITE );
		m_QueryThread = nullptr;
	}
	CloseHandle( m_StopEvent );
	CloseHandle( m_WakeEvent );

	Close();

	if ( nullptr != m_Handle ) {
		FreeLibrary( m_Handle );
		m_Handle = nullptr;
	}
}

DWORD WINAPI Gracenote::QueryThreadProc( LPVOID lParam )
{
	Gracenote* gracenote = static_cast<Gracenote*>( lParam );
	if ( nullptr != gracenote ) {
		gracenote->QueryHandler();
	}
	return 0;
}

INT_PTR CALLBACK Gracenote::MatchDialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( lParam );
			Gracenote* gracenote = ( nullptr != matchInfo ) ? matchInfo->m_Gracenote : nullptr;
			if ( nullptr != gracenote ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				gracenote->OnInitMatchDialog( hwnd, matchInfo->m_Result );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDOK :
				case IDCANCEL : {
					int selection = -1;
					if ( IDOK == LOWORD( wParam ) ) {
						MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						if ( nullptr != matchInfo ) {
							const HWND hwndList = GetDlgItem( hwnd, IDC_GRACENOTE_MATCHES );
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
				const HWND hwndList = GetDlgItem( hwnd, IDC_GRACENOTE_MATCHES );
				if ( hwndList == nmhdr->hwndFrom ) {
					if ( NM_DBLCLK == nmhdr->code ) {
						LPNMITEMACTIVATE nmItemActivate = reinterpret_cast<LPNMITEMACTIVATE>( lParam );
						EndDialog( hwnd, nmItemActivate->iItem );
					} else if ( LVN_ITEMCHANGED == nmhdr->code ) {
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
					} else if ( LVN_GETINFOTIP == nmhdr->code ) {
						LPNMLVGETINFOTIP nmInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>( lParam );
						MatchInfo* matchInfo = reinterpret_cast<MatchInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						if ( ( nullptr != nmInfoTip ) && ( nullptr != matchInfo ) && ( static_cast<size_t>( nmInfoTip->iItem ) < matchInfo->m_Result.Albums.size() ) ) {
							std::wstring tooltip;
							const Album::TrackMap& tracks = matchInfo->m_Result.Albums.at( nmInfoTip->iItem ).Tracks;
							for ( const auto& track : tracks ) {
								if ( !track.second.Title.empty() ) {
									if ( !tooltip.empty() ) {
										tooltip += L"\r\n";
									}
									tooltip += std::to_wstring( track.first );
									tooltip += '\t';
									tooltip += track.second.Title;
								}
							}
							if ( !tooltip.empty() ) {
								if ( tooltip.size() >= static_cast<size_t>( nmInfoTip->cchTextMax ) ) {
									tooltip = tooltip.substr( 0 /*offset*/, nmInfoTip->cchTextMax - 4 );
									tooltip += L"...";
								}
								wcscpy_s( nmInfoTip->pszText, nmInfoTip->cchTextMax, tooltip.c_str() );
							}
						}
					}
				}
			}
			break;
		}
		case WM_DESTROY : {
			const HWND hwndList = GetDlgItem( hwnd, IDC_GRACENOTE_MATCHES );
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
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

bool Gracenote::IsAvailable() const
{
	const bool loaded = ( nullptr != m_Handle );
	return loaded;
}

void Gracenote::Close()
{
	if ( ( nullptr != m_Handle ) && m_Initialised ) {
		m_Close( m_UserID.c_str() );
		m_UserID.clear();
		m_Initialised = false;
	}
}

void Gracenote::Query( const std::string& toc, const bool forceDialog )
{
	std::lock_guard<std::mutex> lock( m_PendingQueriesMutex );
	bool addPending = true;
	auto pendingQuery = m_PendingQueries.begin();
	while ( addPending && ( m_PendingQueries.end() != pendingQuery ) ) {
		addPending = ( pendingQuery->first != toc );
		++pendingQuery;
	}
	if ( addPending ) {
		m_PendingQueries.push_back( std::make_pair( toc, forceDialog ) );
		SetEvent( m_WakeEvent );
	}
}

void Gracenote::QueryHandler()
{
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {

		PendingQuery pendingQuery;
		m_PendingQueriesMutex.lock();
		if ( !m_PendingQueries.empty() ) {
			pendingQuery = m_PendingQueries.front();
			m_PendingQueries.pop_front();
		}
		m_PendingQueriesMutex.unlock();

		const std::string& toc = pendingQuery.first;
		const bool forceDialog = pendingQuery.second;

		if ( !toc.empty() ) {
			Result* result = new Result();
			result->TOC = toc;

			if ( nullptr != m_Handle ) {
				std::string userID;
				bool enable = true;
				bool enableLog = true;
				m_Settings.GetGracenoteSettings( userID, enable, enableLog );
				if ( enable ) {
					m_ActiveQuery = true;
					if ( !m_Initialised ) {
						const char* initResult = m_Init( userID.c_str(), nullptr /*language*/, WideStringToUTF8( VUPlayer::DocumentsFolder() ).c_str() );
						if ( nullptr != initResult ) {
							m_UserID = initResult;
							m_Initialised = true;
						}
					}
					if ( m_Initialised ) {

						if ( m_UserID != userID ) {
							m_Settings.SetGracenoteSettings( m_UserID, enable, enableLog );
						}

						const gn_result* queryResult = m_Query( m_UserID.c_str(), toc.c_str() );
						if ( nullptr != queryResult ) {
							result->ExactMatch = queryResult->exact_match;
							const size_t albumCount = queryResult->album_count;
							result->Albums.reserve( albumCount );
							for ( size_t albumIndex = 0; albumIndex < albumCount; albumIndex++ ) {
								const gn_album* albumResult = queryResult->albums + albumIndex;
								if ( nullptr != albumResult ) {
									Album album;

									// Copy across album information.
									if ( nullptr != albumResult->artwork_data ) {
										album.Artwork.resize( albumResult->artwork_data_size );
										std::copy( albumResult->artwork_data, albumResult->artwork_data + albumResult->artwork_data_size, album.Artwork.begin() );
									}
									const gn_track& albumInfo = albumResult->tracks[ 0 ];
									album.Title = ( nullptr != albumInfo.title ) ? UTF8ToWideString( albumInfo.title ) : std::wstring();
									album.Artist = ( nullptr != albumInfo.artist ) ? UTF8ToWideString( albumInfo.artist ) : std::wstring();
									album.Genre = ( nullptr != albumInfo.genre ) ? UTF8ToWideString( albumInfo.genre ) : std::wstring();
									album.Year = 0;
									if ( nullptr != albumInfo.year ) {
										try {
											album.Year = std::stol( albumInfo.year );
										} catch ( ... ) {
										}
									}

									// Copy across track information.
									for ( size_t trackIndex = 1; trackIndex < 100; trackIndex++ ) {
										const gn_track& trackInfo = albumResult->tracks[ trackIndex ];
										const char* title = trackInfo.title;
										const char* artist = trackInfo.artist;
										const char* genre = trackInfo.genre;
										const char* year = trackInfo.year;
										if ( ( nullptr != title ) || ( nullptr != artist ) || ( nullptr != genre ) || ( nullptr != year ) ) {
											Track track;
											track.Title = ( nullptr != title ) ? UTF8ToWideString( title ) : std::wstring();
											track.Artist = ( nullptr != artist ) ? UTF8ToWideString( artist ) : std::wstring();
											track.Genre = ( nullptr != genre ) ? UTF8ToWideString( genre ) : std::wstring();
											track.Year = 0;
											if ( nullptr != year ) {
												try {
													track.Year = std::stol( year );
												} catch ( ... ) {
												}
											}
											album.Tracks.insert( Album::TrackMap::value_type( static_cast<long>( trackIndex ), track ) );
										}
									}

									result->Albums.push_back( album );
								}
							}
							m_FreeResult( queryResult );
						}
					}
					m_ActiveQuery = false;
				}
			}

			if ( result->Albums.empty() ) {
				delete result;
				result = nullptr;
			} else {
				PostMessage( m_hWnd, MSG_GRACENOTEQUERYRESULT, reinterpret_cast<WPARAM>( result ), forceDialog );
			}
		}

		m_PendingQueriesMutex.lock();
		auto pendingIter = m_PendingQueries.begin();
		while ( m_PendingQueries.end() != pendingIter ) {
			if ( pendingIter->first == toc ) {
				pendingIter = m_PendingQueries.erase( pendingIter );
			} else {
				++pendingIter;
			}
		}
		if ( m_PendingQueries.empty() ) {
			ResetEvent( m_WakeEvent );
		}
		m_PendingQueriesMutex.unlock();
	}
}

int Gracenote::ShowMatchesDialog( const Result& result )
{
	MatchInfo* matchInfo = new MatchInfo( { result, this } );
	const int selection = static_cast<int>( DialogBoxParam( m_hInst, MAKEINTRESOURCE( IDD_GRACENOTE_MATCHES ), m_hWnd, MatchDialogProc, reinterpret_cast<LPARAM>( matchInfo ) ) );
	delete matchInfo;
	return selection;
}

void Gracenote::OnInitMatchDialog( const HWND hwnd, const Result& result )
{
	CentreDialog( hwnd );

	VUPlayer* vuplayer = VUPlayer::Get();
	if ( nullptr != vuplayer ) {
		const HWND wndLogo = GetDlgItem( hwnd, IDC_GRACENOTE_LOGO );
		if ( nullptr != wndLogo ) {
			RECT clientRect = {};
			GetClientRect( wndLogo, &clientRect );
			const HBITMAP hBitmap = vuplayer->GetGracenoteLogo( clientRect );
			if ( nullptr != hBitmap ) {
				SendDlgItemMessage( hwnd, IDC_GRACENOTE_LOGO, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>( hBitmap ) );
				DeleteObject( hBitmap );
			}
		}
	}

	const HWND hwndList = GetDlgItem( hwnd, IDC_GRACENOTE_MATCHES );
	if ( nullptr != hwndList ) {
		ListView_SetExtendedListViewStyleEx( hwndList, LVS_EX_INFOTIP, LVS_EX_INFOTIP );

		// Create the image list.
		const int iconSize = static_cast<int>( s_MatchDialogIconSize * GetDPIScaling() );
		HIMAGELIST imageList = ImageList_Create( iconSize, iconSize, ILC_COLOR32, 0 /*initial*/, 1 /*grow*/ );

		const Gdiplus::Color white( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ) );
		Gdiplus::Bitmap bitmap( iconSize, iconSize, PixelFormat32bppARGB );
		Gdiplus::Graphics graphics( &bitmap );
		const Gdiplus::Rect rect( 0, 0, iconSize, iconSize );

		// Maps an album index to an image index.
		typedef std::map<int,int> ImageIndexMap;
		ImageIndexMap imageIndexMap;

		int albumIndex = 0;
		for ( const auto& album : result.Albums ) {
			std::shared_ptr<Gdiplus::Bitmap> artwork;

			if ( !album.Artwork.empty() ) {
				IStream* stream = nullptr;
				if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
					if ( SUCCEEDED( stream->Write( &album.Artwork[ 0 ], static_cast<ULONG>( album.Artwork.size() ), NULL /*bytesWritten*/ ) ) ) {
						try {
							artwork = std::shared_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap( stream ) );
						} catch ( ... ) {
						}
					}
					stream->Release();
				}
			}

			if ( !artwork || ( artwork->GetWidth() == 0 ) || ( artwork->GetHeight() == 0 ) ) {
				if ( nullptr != vuplayer ) {
					artwork = vuplayer->GetPlaceholderImage();
				}	
			}

			if ( artwork && ( artwork->GetWidth() != 0 ) && ( artwork->GetHeight() != 0 ) ) {
				HBITMAP hBitmap = nullptr;
				if ( Gdiplus::Ok == graphics.DrawImage( artwork.get(), rect ) ) {
					if ( Gdiplus::Ok == bitmap.GetHBITMAP( white, &hBitmap ) ) {
						const int imageIndex = ImageList_Add( imageList, hBitmap, nullptr /*mask*/ );
						imageIndexMap.insert( ImageIndexMap::value_type( albumIndex, imageIndex ) );
					}
				}
			}
			++albumIndex;
		}
		ListView_SetImageList( hwndList, imageList, LVSIL_NORMAL );

		// Add the items to the list.
		LVITEM item = {};
		item.mask = LVIF_IMAGE | LVIF_TEXT;
		item.iItem = 0;
		item.iImage = 0;
		for ( const auto& album : result.Albums ) {
			const auto imageIter = imageIndexMap.find( item.iItem );
			if ( imageIndexMap.end() != imageIter ) {
				item.iImage = imageIter->second;
				item.mask = LVIF_IMAGE | LVIF_TEXT;
			} else {
				item.mask = LVIF_TEXT;
			}
			item.pszText = const_cast<LPWSTR>( album.Title.c_str() );
			ListView_InsertItem( hwndList, &item );
			item.iItem++;
		}
	}
}

bool Gracenote::IsActive() const
{
	const bool active = m_ActiveQuery.load();
	return active;
}