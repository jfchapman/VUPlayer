#include "DlgTrackInfo.h"

#include "Utility.h"
#include "VUPlayer.h"

#include "resource.h"
#include "windowsx.h"

#include <fstream>

// ID3 genre count
static const int s_GenreCount = 148;

// ID3 genre list.
static const wchar_t* s_GenreList[ s_GenreCount ] = {
L"Blues",L"Classic Rock",L"Country",L"Dance",L"Disco",L"Funk",L"Grunge",
L"Hip-Hop",L"Jazz",L"Metal",L"New Age",L"Oldies",L"Other",L"Pop",L"R&B",L"Rap",L"Reggae",L"Rock",L"Techno",
L"Industrial",L"Alternative",L"Ska",L"Death Metal",L"Pranks",L"Soundtrack",L"Euro-Techno",L"Ambient",L"Trip-Hop",
L"Vocal",L"Jazz Funk",L"Fusion",L"Trance",L"Classical",L"Instrumental",L"Acid",L"House",L"Game",L"Sound Clip",
L"Gospel",L"Noise",L"Alternative Rock",L"Bass",L"Soul",L"Punk",L"Space",L"Meditative",L"Instrumental Pop",
L"Instrumental Rock",L"Ethnic",L"Gothic",L"Darkwave",L"Techno-Industrial",L"Electronic",L"Pop-Folk",L"Eurodance",
L"Dream",L"Southern Rock",L"Comedy",L"Cult",L"Gangsta",L"Top 40",L"Christian Rap",L"Pop-Funk",L"Jungle",
L"Native American",L"Cabaret",L"New Wave",L"Psychedelic",L"Rave",L"Showtunes",L"Trailer",L"Lo-Fi",L"Tribal",
L"Acid Punk",L"Acid Jazz",L"Polka",L"Retro",L"Musical",L"Rock & Roll",L"Hard Rock",L"Folk",L"Folk Rock",
L"National Folk",L"Swing",L"Fast Fusion",L"Bebop",L"Latin",L"Revival",L"Celtic",L"Bluegrass",L"Avantgarde",
L"Gothic Rock",L"Progressive Rock",L"Psychedelic Rock",L"Symphonic Rock",L"Slow Rock",L"Big Band",L"Chorus",
L"Easy Listening",L"Acoustic",L"Humour",L"Speech",L"Chanson",L"Opera",L"Chamber Music",L"Sonata",L"Symphony",
L"Booty Bass",L"Primus",L"Porn Groove",L"Satire",L"Slow Jam",L"Club",L"Tango",L"Samba",L"Folklore",L"Ballad",
L"Power Ballad",L"Rhythmic Soul",L"Freestyle",L"Duet",L"Punk Rock",L"Drum Solo",L"A cappella",L"Euro-House",
L"Dance Hall",L"Goa",L"Drum & Bass",L"Club-House",L"Hardcore",L"Terror",L"Indie",L"Britpop",L"Negerpunk",
L"Polsk Punk",L"Beat",L"Christian Gangsta",L"Heavy Metal",L"Black Metal",L"Crossover",
L"Contemporary Christian",L"Christian Rock",L"Merengue",L"Salsa",L"Thrash Metal",L"Anime",L"JPop",L"Synthpop"
};

// Maximum size when creating a PNG from pasted images.
static const int s_MaxPastedImageBytes = 0x1000000;

// Initial folder setting for choosing artwork.
static const std::string s_ArtworkFolderSetting = "Artwork";

// Message ID for updating the progress bar when saving information.
// 'wParam' - percent progress.
static const UINT MSG_UPDATEPROGRESS = WM_APP + 65;

// Message ID for indicating that the saving of media information has been completed.
static const UINT MSG_SAVECOMPLETED = WM_APP + 66;

INT_PTR CALLBACK DlgTrackInfo::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
				return TRUE;
			}
			break;
		}
		case WM_DESTROY: {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_COMMAND: {
			switch ( LOWORD( wParam ) ) {
				case IDOK: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnSave( hwnd );
					}
					return TRUE;
				}
				case IDCANCEL: {
					HWND hwndClose = GetDlgItem( hwnd, IDCANCEL );
					if ( IsWindowVisible( hwndClose ) ) {
						EndDialog( hwnd, IDCANCEL );
					} else {
						DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
						if ( nullptr != dialog ) {
							dialog->OnCancelSave();
						}
					}
					return TRUE;
				}
				case IDC_TRACKINFO_CANCELSAVE: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnCancelSave();
					}
					break;
				}
				case IDC_TRACKINFO_CHOOSEARTWORK: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnChooseArtwork( hwnd );
					}
					break;
				}
				case IDC_TRACKINFO_EXPORTARTWORK: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnExportArtwork( hwnd );
					}
					break;
				}
				case IDC_TRACKINFO_CLEARARTWORK: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnClearArtwork( hwnd );
					}
					break;
				}
				case ID_TRACKINFOMENU_CUT: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnCutCopyArtwork( hwnd, true /*cut*/ );
					}
					break;
				}
				case ID_TRACKINFOMENU_COPY: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnCutCopyArtwork( hwnd, false /*cut*/ );
					}
					break;
				}
				case ID_TRACKINFOMENU_PASTE: {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnPasteArtwork( hwnd );
					}
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		case WM_DRAWITEM: {
			DRAWITEMSTRUCT* drawitem = reinterpret_cast<DRAWITEMSTRUCT*>( lParam );
			DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( ( nullptr != drawitem ) && ( nullptr != dialog ) ) {
				dialog->OnOwnerDraw( drawitem );
			}
			break;
		}
		case WM_LBUTTONDBLCLK: {
			POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
			ClientToScreen( hwnd, &pt );
			const HWND artworkWnd = GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK );
			if ( nullptr != artworkWnd && ( TRUE == IsWindowEnabled( artworkWnd ) ) ) {
				RECT rect = {};
				GetWindowRect( artworkWnd, &rect );
				if ( TRUE == PtInRect( &rect, pt ) ) {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnChooseArtwork( hwnd );
					}
				}
			}
			break;
		}
		case WM_CONTEXTMENU: {
			POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
			const HWND artworkWnd = GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK );
			if ( nullptr != artworkWnd && ( TRUE == IsWindowEnabled( artworkWnd ) ) ) {
				RECT rect = {};
				GetWindowRect( artworkWnd, &rect );
				if ( TRUE == PtInRect( &rect, pt ) ) {
					DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->OnArtworkContextMenu( hwnd, pt );
					}
				}
			}
			break;
		}
		case MSG_UPDATEPROGRESS: {
			HWND hwndProgress = GetDlgItem( hwnd, IDC_TRACKINFO_PROGRESS );
			SendMessage( hwndProgress, PBM_SETPOS, wParam, lParam );
			break;
		}
		case MSG_SAVECOMPLETED: {
			EndDialog( hwnd, IDOK );
			return TRUE;
		}
		default: {
			break;
		}
	}
	return FALSE;
}

DWORD WINAPI DlgTrackInfo::SaveThreadProc( LPVOID lpParam )
{
	HWND hwnd = static_cast<HWND>( lpParam );
	DlgTrackInfo* dialog = reinterpret_cast<DlgTrackInfo*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
	if ( nullptr != dialog ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		dialog->SaveThreadHandler( hwnd );
		CoUninitialize();
	}
	return 0;
}

DlgTrackInfo::DlgTrackInfo( HINSTANCE instance, HWND parent, Library& library, Settings& settings, Playlist::Items& items ) :
	m_hInst( instance ),
	m_Library( library ),
	m_Settings( settings ),
	m_Items( items ),
	m_Bitmap(),
	m_InitialInfo(),
	m_ClosingInfo(),
	m_ChosenArtworkImage(),
	m_UpdateInfo(),
	m_CancelSaveEvent( nullptr )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_TRACKINFORMATION ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

DlgTrackInfo::~DlgTrackInfo()
{
	if ( nullptr != m_SaveThread ) {
		CloseHandle( m_SaveThread );
	}
	if ( nullptr != m_CancelSaveEvent ) {
		CloseHandle( m_CancelSaveEvent );
	}
}

void DlgTrackInfo::OnInitDialog( HWND hwnd )
{
	CentreDialog( hwnd );
	HWND genreWnd = GetDlgItem( hwnd, IDC_TRACKINFO_GENRE );
	if ( nullptr != genreWnd ) {
		for ( const auto& iter : s_GenreList ) {
			ComboBox_AddString( genreWnd, iter );
		}
	}

	// Ensure item information is up to date before editing.
	for ( auto& item : m_Items ) {
		if ( MediaInfo mediaInfo( item.Info ); m_Library.GetMediaInfo( mediaInfo ) ) {
			item.Info = mediaInfo;
		}
	}

	if ( ( m_Items.size() > 1 ) || ( !m_Items.empty() && !m_Items.front().Duplicates.empty() ) ) {
		const int bufSize = 64;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_MULTIPLE_SOURCES, buffer, bufSize );
		SetDlgItemText( hwnd, IDC_TRACKINFO_FILENAME, buffer );

		auto iter = m_Items.begin();
		bool sameArtist = true;
		bool sameTitle = true;
		bool sameAlbum = true;
		bool samePublisher = true;
		bool sameComposer = true;
		bool sameConductor = true;
		bool sameGenre = true;
		bool sameYear = true;
		bool sameTrack = true;
		bool sameComment = true;
		bool sameArtworkID = true;
		std::wstring artist = iter->Info.GetArtist();
		std::wstring title = iter->Info.GetTitle();
		std::wstring album = iter->Info.GetAlbum();
		std::wstring publisher = iter->Info.GetPublisher();
		std::wstring composer = iter->Info.GetComposer();
		std::wstring conductor = iter->Info.GetConductor();
		std::wstring genre = iter->Info.GetGenre();
		long year = iter->Info.GetYear();
		long track = iter->Info.GetTrack();
		std::wstring comment = iter->Info.GetComment();
		std::wstring artworkID = iter->Info.GetArtworkID();
		while ( ++iter != m_Items.end() ) {
			if ( sameArtist && ( iter->Info.GetArtist() != artist ) ) {
				sameArtist = false;
				artist.clear();
			}
			if ( sameTitle && ( iter->Info.GetTitle() != title ) ) {
				sameTitle = false;
				title.clear();
			}
			if ( sameAlbum && ( iter->Info.GetAlbum() != album ) ) {
				sameAlbum = false;
				album.clear();
			}
			if ( samePublisher && ( iter->Info.GetPublisher() != publisher ) ) {
				samePublisher = false;
				publisher.clear();
			}
			if ( sameComposer && ( iter->Info.GetComposer() != composer ) ) {
				sameComposer = false;
				composer.clear();
			}
			if ( sameConductor && ( iter->Info.GetConductor() != conductor ) ) {
				sameConductor = false;
				conductor.clear();
			}
			if ( sameGenre && ( iter->Info.GetGenre() != genre ) ) {
				sameGenre = false;
				genre.clear();
			}
			if ( sameYear && ( iter->Info.GetYear() != year ) ) {
				sameYear = false;
				year = 0;
			}
			if ( sameTrack && ( iter->Info.GetTrack() != track ) ) {
				sameTrack = false;
				track = 0;
			}
			if ( sameComment && ( iter->Info.GetComment() != comment ) ) {
				sameComment = false;
				comment.clear();
			}
			if ( sameArtworkID && ( iter->Info.GetArtworkID() != artworkID ) ) {
				sameArtworkID = false;
				artworkID.clear();
			}
		}
		m_InitialInfo.SetArtist( artist );
		m_InitialInfo.SetTitle( title );
		m_InitialInfo.SetAlbum( album );
		m_InitialInfo.SetPublisher( publisher );
		m_InitialInfo.SetComposer( composer );
		m_InitialInfo.SetConductor( conductor );
		m_InitialInfo.SetGenre( genre );
		m_InitialInfo.SetYear( year );
		m_InitialInfo.SetTrack( track );
		m_InitialInfo.SetComment( comment );
		m_InitialInfo.SetArtworkID( artworkID );
	} else if ( !m_Items.empty() ) {
		m_InitialInfo = m_Items.begin()->Info;
		SetDlgItemText( hwnd, IDC_TRACKINFO_FILENAME, m_InitialInfo.GetFilenameWithCues( true /*fullPath*/ ).c_str() );
	}

	SetDlgItemText( hwnd, IDC_TRACKINFO_ARTIST, m_InitialInfo.GetArtist().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_TITLE, m_InitialInfo.GetTitle().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_ALBUM, m_InitialInfo.GetAlbum().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_PUBLISHER, m_InitialInfo.GetPublisher().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_COMPOSER, m_InitialInfo.GetComposer().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_CONDUCTOR, m_InitialInfo.GetConductor().c_str() );
	SetDlgItemText( hwnd, IDC_TRACKINFO_GENRE, m_InitialInfo.GetGenre().c_str() );
	if ( m_InitialInfo.GetYear() > 0 ) {
		SetDlgItemInt( hwnd, IDC_TRACKINFO_YEAR, static_cast<UINT>( m_InitialInfo.GetYear() ), FALSE /*signed*/ );
	}
	if ( m_InitialInfo.GetTrack() > 0 ) {
		SetDlgItemInt( hwnd, IDC_TRACKINFO_TRACKNUMBER, static_cast<UINT>( m_InitialInfo.GetTrack() ), FALSE /*signed*/ );
	}

	std::wstring comment = m_InitialInfo.GetComment();
	WideStringReplace( comment, L"\r\n", L"\n" );
	WideStringReplace( comment, L"\r", L"\r\n" );
	WideStringReplace( comment, L"\r\n", L"\n" );
	WideStringReplace( comment, L"\n", L"\r\n" );
	m_InitialInfo.SetComment( comment );

	SetDlgItemText( hwnd, IDC_TRACKINFO_COMMENT, m_InitialInfo.GetComment().c_str() );
	m_Bitmap = GetArtwork( m_InitialInfo );
	m_ClosingInfo.SetArtworkID( m_InitialInfo.GetArtworkID() );
	EnableControls( hwnd );

	HWND hwndTrack = GetDlgItem( hwnd, IDC_TRACKINFO_TRACKNUMBER );
	if ( nullptr != hwndTrack ) {
		const BOOL enable = m_Items.empty() ? FALSE : ( MediaInfo::Source::File == m_Items.front().Info.GetSource() );
		EnableWindow( hwndTrack, enable );
	}
}

std::unique_ptr<Gdiplus::Bitmap> DlgTrackInfo::GetArtwork( const MediaInfo& mediaInfo )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	const std::wstring& artworkID = mediaInfo.GetArtworkID();
	if ( !artworkID.empty() ) {
		std::vector<BYTE> artwork = m_Library.GetMediaArtwork( mediaInfo );
		if ( !artwork.empty() ) {
			IStream* stream = nullptr;
			if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
				if ( SUCCEEDED( stream->Write( &artwork[ 0 ], static_cast<ULONG>( artwork.size() ), NULL /*bytesWritten*/ ) ) ) {
					try {
						bitmap = std::make_unique<Gdiplus::Bitmap>( stream );
					} catch ( ... ) {
					}
				}
				stream->Release();
			}
		}
	}
	if ( !bitmap || ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			bitmap = vuplayer->GetPlaceholderImage();
		}
	}
	return bitmap;
}

std::unique_ptr<Gdiplus::Bitmap> DlgTrackInfo::GetArtwork( const std::vector<BYTE>& image )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	const ULONG imageSize = static_cast<ULONG>( image.size() );
	if ( imageSize > 0 ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( &image[ 0 ], imageSize, NULL /*bytesWritten*/ ) ) ) {
				try {
					bitmap = std::make_unique<Gdiplus::Bitmap>( stream );
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}
	if ( !bitmap || ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			bitmap = vuplayer->GetPlaceholderImage();
		}
	}
	return bitmap;
}

void DlgTrackInfo::OnOwnerDraw( DRAWITEMSTRUCT* drawItemStruct )
{
	if ( ( nullptr != drawItemStruct ) && ( IDC_TRACKINFO_ARTWORK == drawItemStruct->CtlID ) ) {
		const int clientWidth = drawItemStruct->rcItem.right - drawItemStruct->rcItem.left;
		const int clientHeight = drawItemStruct->rcItem.bottom - drawItemStruct->rcItem.top;
		Gdiplus::Rect clientRect( 0, 0, clientWidth, clientHeight );
		Gdiplus::Graphics graphics( drawItemStruct->hDC );
		graphics.Clear( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ) );
		if ( m_Bitmap ) {
			const UINT bitmapWidth = m_Bitmap->GetWidth();
			const UINT bitmapHeight = m_Bitmap->GetHeight();
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
			graphics.DrawImage( m_Bitmap.get(), clientRect );
		}
		Gdiplus::Pen pen( static_cast<Gdiplus::ARGB>( Gdiplus::Color::Gray ) );
		graphics.DrawRectangle( &pen, 0, 0, clientWidth - 1, clientHeight - 1 );
	}
}

void DlgTrackInfo::OnChooseArtwork( HWND hwnd )
{
	if ( const auto filename = ChooseArtwork( m_hInst, hwnd, m_Settings.GetLastFolder( s_ArtworkFolderSetting ) ); !filename.empty() ) {
		std::ifstream fileStream;
		fileStream.open( filename, std::ios::binary | std::ios::in );
		if ( fileStream.is_open() ) {
			fileStream.seekg( 0, fileStream.end );
			const int streamSize = static_cast<int>( fileStream.tellg() );
			fileStream.seekg( 0, fileStream.beg );
			if ( streamSize > 0 ) {
				std::vector<BYTE> imageBytes( streamSize );
				char* imageBuffer = reinterpret_cast<char*>( imageBytes.data() );
				fileStream.read( imageBuffer, streamSize );
				const std::string encodedImage = ConvertImage( imageBytes );
				const std::vector<BYTE> image = Base64Decode( encodedImage );
				if ( !image.empty() ) {
					m_ChosenArtworkImage = image;
					m_ClosingInfo.SetArtworkID( std::wstring() );
				}
			}
			fileStream.close();
		}

		const std::filesystem::path path( filename );
		m_Settings.SetLastFolder( s_ArtworkFolderSetting, path.parent_path() );

		m_Bitmap = GetArtwork( m_ChosenArtworkImage );
		InvalidateRect( GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK ), NULL /*rect*/, TRUE /*erase*/ );
		EnableControls( hwnd );
	}
}

void DlgTrackInfo::OnExportArtwork( HWND hwnd )
{
	const std::vector<BYTE> imageBytes = m_Library.GetMediaArtwork( m_ClosingInfo );
	const int imageSize = static_cast<int>( imageBytes.size() );
	if ( imageSize > 0 ) {
		const std::string encodedImage = Base64Encode( &imageBytes[ 0 ], imageSize );
		std::string mimeType;
		int width = 0;
		int height = 0;
		int depth = 0;
		int colours = 0;
		GetImageInformation( encodedImage, mimeType, width, height, depth, colours );
		if ( ( width > 0 ) && ( height > 0 ) ) {
			std::wstring fileExt;
			if ( "image/bmp" == mimeType ) {
				fileExt = L"bmp";
			} else if ( "image/x-emf" == mimeType ) {
				fileExt = L"emf";
			} else if ( "image/x-wmf" == mimeType ) {
				fileExt = L"wmf";
			} else if ( "image/jpeg" == mimeType ) {
				fileExt = L"jpg";
			} else if ( "image/png" == mimeType ) {
				fileExt = L"png";
			} else if ( "image/gif" == mimeType ) {
				fileExt = L"gif";
			} else if ( "image/tiff" == mimeType ) {
				fileExt = L"tiff";
			}

			WCHAR title[ MAX_PATH ] = {};
			LoadString( m_hInst, IDS_EXPORTARTWORK_TITLE, title, MAX_PATH );

			WCHAR buffer[ MAX_PATH ] = {};
			GetDlgItemText( hwnd, IDC_TRACKINFO_ALBUM, buffer, MAX_PATH );
			if ( wcslen( buffer ) == 0 ) {
				GetDlgItemText( hwnd, IDC_TRACKINFO_TITLE, buffer, MAX_PATH );
				if ( wcslen( buffer ) == 0 ) {
					LoadString( m_hInst, IDS_ARTWORK, buffer, MAX_PATH );
				}
			}
			std::set<WCHAR> invalidCharacters = { '\\', '/', ':', '*', '?', '"', '<', '>', '|' };
			const size_t length = wcslen( buffer );
			for ( size_t index = 0; index < length; index++ ) {
				if ( invalidCharacters.end() != invalidCharacters.find( buffer[ index ] ) ) {
					buffer[ index ] = '_';
				}
			}

			const std::wstring initialFolder = m_Settings.GetLastFolder( s_ArtworkFolderSetting );

			OPENFILENAME ofn = {};
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.hwndOwner = hwnd;
			ofn.lpstrTitle = title;
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
			ofn.lpstrDefExt = fileExt.c_str();
			ofn.lpstrFile = buffer;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
			if ( FALSE != GetSaveFileName( &ofn ) ) {
				const std::wstring filename = ofn.lpstrFile;
				std::ofstream fileStream;
				fileStream.open( filename, std::ios::binary | std::ios::out | std::ios::trunc );
				if ( fileStream.is_open() ) {
					const char* image = reinterpret_cast<const char*>( &imageBytes[ 0 ] );
					fileStream.write( image, imageSize );
					fileStream.close();
				}
				m_Settings.SetLastFolder( s_ArtworkFolderSetting, filename.substr( 0, ofn.nFileOffset ) );
			}
		}
	}
}

void DlgTrackInfo::OnClearArtwork( HWND hwnd )
{
	MediaInfo info;
	m_Bitmap = GetArtwork( info );
	InvalidateRect( GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK ), NULL /*rect*/, TRUE /*erase*/ );
	m_ChosenArtworkImage.clear();
	m_ClosingInfo.SetArtworkID( std::wstring() );
	EnableControls( hwnd );
}

void DlgTrackInfo::EnableControls( HWND hwnd )
{
	HWND hwndExport = GetDlgItem( hwnd, IDC_TRACKINFO_EXPORTARTWORK );
	if ( nullptr != hwndExport ) {
		const BOOL enable = m_ClosingInfo.GetArtworkID().empty() ? FALSE : TRUE;
		EnableWindow( hwndExport, enable );
	}
	HWND hwndClear = GetDlgItem( hwnd, IDC_TRACKINFO_CLEARARTWORK );
	if ( nullptr != hwndClear ) {
		const BOOL enable = ( m_ClosingInfo.GetArtworkID().empty() && m_ChosenArtworkImage.empty() ) ? FALSE : TRUE;
		EnableWindow( hwndClear, enable );
	}
}

void DlgTrackInfo::OnArtworkContextMenu( HWND hwnd, const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_TRACKINFO ) );
	if ( NULL != menu ) {
		HMENU artworkmenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != artworkmenu ) {

			const UINT enableCutCopy = ( m_ClosingInfo.GetArtworkID().empty() && m_ChosenArtworkImage.empty() ) ? MF_DISABLED : MF_ENABLED;
			const UINT enablePaste = IsClipboardFormatAvailable( CF_BITMAP ) ? MF_ENABLED : MF_DISABLED;

			EnableMenuItem( artworkmenu, ID_TRACKINFOMENU_CUT, MF_BYCOMMAND | enableCutCopy );
			EnableMenuItem( artworkmenu, ID_TRACKINFOMENU_COPY, MF_BYCOMMAND | enableCutCopy );
			EnableMenuItem( artworkmenu, ID_TRACKINFOMENU_PASTE, MF_BYCOMMAND | enablePaste );

			TrackPopupMenu( artworkmenu, TPM_RIGHTBUTTON, position.x, position.y, 0 /*reserved*/, hwnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}

void DlgTrackInfo::OnCutCopyArtwork( HWND hwnd, const bool cut )
{
	if ( m_Bitmap ) {
		HBITMAP hBitmap = nullptr;
		if ( Gdiplus::Ok == m_Bitmap->GetHBITMAP( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ), &hBitmap ) ) {
			DIBSECTION dibSection = {};
			if ( 0 != GetObject( hBitmap, sizeof( DIBSECTION ), &dibSection ) ) {
				HDC dc = GetDC( hwnd );
				if ( nullptr != dc ) {
					BITMAPINFOHEADER bitmapInfoHeader = dibSection.dsBmih;
					const void* bits = dibSection.dsBm.bmBits;
					BITMAPINFO* bitmapInfo = reinterpret_cast<BITMAPINFO*>( &dibSection.dsBmih );
					HBITMAP dibBitmap = CreateDIBitmap( dc, &bitmapInfoHeader, CBM_INIT, bits, bitmapInfo, DIB_RGB_COLORS );
					if ( nullptr != dibBitmap ) {
						if ( OpenClipboard( hwnd ) ) {
							EmptyClipboard();
							if ( ( nullptr != SetClipboardData( CF_BITMAP, dibBitmap ) ) && cut ) {
								OnClearArtwork( hwnd );
							}
							CloseClipboard();
						}
						DeleteObject( dibBitmap );
					}
					ReleaseDC( hwnd, dc );
				}
			}
			DeleteObject( hBitmap );
		}
	}
}

void DlgTrackInfo::OnPasteArtwork( HWND hwnd )
{
	if ( OpenClipboard( hwnd ) ) {
		HANDLE handle = GetClipboardData( CF_BITMAP );
		if ( nullptr != handle ) {
			HBITMAP hBitmap = reinterpret_cast<HBITMAP>( handle );
			try {
				Gdiplus::Bitmap bitmap( hBitmap, nullptr /*palette*/ );
				if ( ( bitmap.GetWidth() > 0 ) && ( bitmap.GetHeight() > 0 ) ) {
					// Convert pasted bitmap to PNG format.
					CLSID encoderClsid = {};
					UINT numEncoders = 0;
					UINT bufferSize = 0;
					if ( ( Gdiplus::Ok == Gdiplus::GetImageEncodersSize( &numEncoders, &bufferSize ) ) && ( bufferSize > 0 ) ) {
						std::vector<char> buffer( bufferSize, 0 );
						Gdiplus::ImageCodecInfo* imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>( buffer.data() );
						if ( Gdiplus::Ok == Gdiplus::GetImageEncoders( numEncoders, bufferSize, imageCodecInfo ) ) {
							for ( UINT index = 0; index < numEncoders; index++ ) {
								if ( Gdiplus::ImageFormatPNG == imageCodecInfo[ index ].FormatID ) {
									encoderClsid = imageCodecInfo[ index ].Clsid;
									break;
								}
							}
						}
					}

					IStream* stream = nullptr;
					if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
						if ( Gdiplus::Ok == bitmap.Save( stream, &encoderClsid ) ) {
							STATSTG stats = {};
							if ( SUCCEEDED( stream->Stat( &stats, STATFLAG_NONAME ) ) && ( stats.cbSize.QuadPart > 0 ) && ( stats.cbSize.QuadPart < s_MaxPastedImageBytes ) ) {
								if ( SUCCEEDED( stream->Seek( { 0 }, STREAM_SEEK_SET, NULL /*newPosition*/ ) ) ) {
									const ULONG pngBufferSize = static_cast<ULONG>( stats.cbSize.QuadPart );
									std::vector<BYTE> pngBuffer( pngBufferSize );
									ULONG bytesRead = 0;
									if ( SUCCEEDED( stream->Read( &pngBuffer[ 0 ], pngBufferSize, &bytesRead ) ) && ( bytesRead == pngBufferSize ) ) {
										m_ChosenArtworkImage = pngBuffer;
										m_Bitmap = GetArtwork( m_ChosenArtworkImage );
										InvalidateRect( GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK ), NULL /*rect*/, TRUE /*erase*/ );
										EnableControls( hwnd );
									}
								}
							}
						}
						stream->Release();
					}
				}
			} catch ( ... ) {
			}
		}
		CloseClipboard();
	}
}

void DlgTrackInfo::OnSave( HWND hwnd )
{
	// Get dialog information.
	const int bufSize = 4096;
	WCHAR buffer[ bufSize ];
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_ARTIST, buffer, bufSize ) ) {
		m_ClosingInfo.SetArtist( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_TITLE, buffer, bufSize ) ) {
		m_ClosingInfo.SetTitle( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_ALBUM, buffer, bufSize ) ) {
		m_ClosingInfo.SetAlbum( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_PUBLISHER, buffer, bufSize ) ) {
		m_ClosingInfo.SetPublisher( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_COMPOSER, buffer, bufSize ) ) {
		m_ClosingInfo.SetComposer( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_CONDUCTOR, buffer, bufSize ) ) {
		m_ClosingInfo.SetConductor( buffer );
	}
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_GENRE, buffer, bufSize ) ) {
		m_ClosingInfo.SetGenre( buffer );
	}
	m_ClosingInfo.SetYear( GetDlgItemInt( hwnd, IDC_TRACKINFO_YEAR, NULL, FALSE /*signed*/ ) );
	m_ClosingInfo.SetTrack( GetDlgItemInt( hwnd, IDC_TRACKINFO_TRACKNUMBER, NULL, FALSE /*signed*/ ) );
	if ( 0 != GetDlgItemText( hwnd, IDC_TRACKINFO_COMMENT, buffer, bufSize ) ) {
		m_ClosingInfo.SetComment( buffer );
	}
	if ( !m_ChosenArtworkImage.empty() ) {
		const std::wstring artworkID = m_Library.AddArtwork( m_ChosenArtworkImage );
		m_ClosingInfo.SetArtworkID( artworkID );
	}

	// Determine which items need their information updating.
	for ( const auto& iter : m_Items ) {
		Playlist::Item item( iter );
		Playlist::Item previousItem( item );
		MediaInfo& info = item.Info;
		bool updateItem = false;
		if ( m_ClosingInfo.GetArtist() != m_InitialInfo.GetArtist() ) {
			info.SetArtist( m_ClosingInfo.GetArtist() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetTitle() != m_InitialInfo.GetTitle() ) {
			info.SetTitle( m_ClosingInfo.GetTitle() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetAlbum() != m_InitialInfo.GetAlbum() ) {
			info.SetAlbum( m_ClosingInfo.GetAlbum() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetPublisher() != m_InitialInfo.GetPublisher() ) {
			info.SetPublisher( m_ClosingInfo.GetPublisher() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetComposer() != m_InitialInfo.GetComposer() ) {
			info.SetComposer( m_ClosingInfo.GetComposer() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetConductor() != m_InitialInfo.GetConductor() ) {
			info.SetConductor( m_ClosingInfo.GetConductor() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetGenre() != m_InitialInfo.GetGenre() ) {
			info.SetGenre( m_ClosingInfo.GetGenre() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetYear() != m_InitialInfo.GetYear() ) {
			info.SetYear( m_ClosingInfo.GetYear() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetTrack() != m_InitialInfo.GetTrack() ) {
			info.SetTrack( m_ClosingInfo.GetTrack() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetComment() != m_InitialInfo.GetComment() ) {
			info.SetComment( m_ClosingInfo.GetComment() );
			updateItem = true;
		}
		if ( m_ClosingInfo.GetArtworkID() != m_InitialInfo.GetArtworkID() ) {
			info.SetArtworkID( m_ClosingInfo.GetArtworkID() );
			updateItem = true;
		}
		if ( updateItem ) {
			m_UpdateInfo.push_back( std::make_pair( previousItem, item ) );
		}
	}

	if ( !m_UpdateInfo.empty() ) {
		// Disable controls.
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_ARTIST ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_TITLE ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_ALBUM ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_PUBLISHER ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_COMPOSER ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_CONDUCTOR ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_GENRE ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_YEAR ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_TRACKNUMBER ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_COMMENT ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_ARTWORK ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_CHOOSEARTWORK ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_EXPORTARTWORK ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDC_TRACKINFO_CLEARARTWORK ), FALSE );
		EnableWindow( GetDlgItem( hwnd, IDOK ), FALSE );

		if ( m_UpdateInfo.size() > 1 ) {
			ShowProgress( hwnd );
		}

		// Launch save thread.
		m_CancelSaveEvent = CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ );
		m_SaveThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, SaveThreadProc, hwnd, 0 /*flags*/, NULL /*threadId*/ );

	} else {
		EndDialog( hwnd, IDOK );
	}
}

void DlgTrackInfo::ShowProgress( HWND hwnd )
{
	const HWND hwndChooseArtwork = GetDlgItem( hwnd, IDC_TRACKINFO_CHOOSEARTWORK );
	const HWND hwndExportArtwork = GetDlgItem( hwnd, IDC_TRACKINFO_EXPORTARTWORK );
	const HWND hwndClearArtwork = GetDlgItem( hwnd, IDC_TRACKINFO_CLEARARTWORK );
	const HWND hwndClose = GetDlgItem( hwnd, IDCANCEL );

	RECT leftRect = {};
	GetWindowRect( hwndChooseArtwork, &leftRect );
	RECT rightRect = {};
	GetWindowRect( hwndClearArtwork, &rightRect );
	RECT cancelRect = {};
	GetWindowRect( hwndClose, &cancelRect );
	MapWindowRect( NULL /*hwndFrom*/, hwnd, &leftRect );
	MapWindowRect( NULL /*hwndFrom*/, hwnd, &rightRect );
	MapWindowRect( NULL /*hwndFrom*/, hwnd, &cancelRect );

	ShowWindow( hwndChooseArtwork, SW_HIDE );
	ShowWindow( hwndExportArtwork, SW_HIDE );
	ShowWindow( hwndClearArtwork, SW_HIDE );
	ShowWindow( hwndClose, SW_HIDE );

	const HWND hwndProgress = GetDlgItem( hwnd, IDC_TRACKINFO_PROGRESS );
	int x = static_cast<int>( leftRect.left ) + 1;
	int y = static_cast<int>( leftRect.top ) + 1;
	int height = static_cast<int>( leftRect.bottom - leftRect.top ) - 2;
	int width = static_cast<int>( rightRect.right - leftRect.left ) - 2;
	MoveWindow( hwndProgress, x, y, width, height, TRUE /*repaint*/ );
	ShowWindow( hwndProgress, SW_SHOW );

	const HWND hwndCancel = GetDlgItem( hwnd, IDC_TRACKINFO_CANCELSAVE );
	x = static_cast<int>( cancelRect.left );
	y = static_cast<int>( cancelRect.top );
	height = static_cast<int>( cancelRect.bottom - cancelRect.top );
	width = static_cast<int>( cancelRect.right - cancelRect.left );
	MoveWindow( hwndCancel, x, y, width, height, TRUE /*repaint*/ );
	EnableWindow( hwndCancel, TRUE );
	ShowWindow( hwndCancel, SW_SHOW );
}

void DlgTrackInfo::SaveThreadHandler( HWND hwnd )
{
	const int totalItems = static_cast<int>( m_UpdateInfo.size() );
	int savedItems = 0;
	for ( const auto& iter : m_UpdateInfo ) {
		MediaInfo previousMediaInfo = iter.first.Info;
		MediaInfo updatedMediaInfo = iter.second.Info;
		m_Library.UpdateMediaTags( previousMediaInfo, updatedMediaInfo );
		for ( const auto& duplicate : iter.first.Duplicates ) {
			previousMediaInfo.SetFilename( duplicate );
			updatedMediaInfo.SetFilename( duplicate );
			m_Library.UpdateMediaTags( previousMediaInfo, updatedMediaInfo );
		}
		const int percentDone = ++savedItems * 100 / totalItems;
		PostMessage( hwnd, MSG_UPDATEPROGRESS, percentDone, 0 /*lParam*/ );
		if ( WAIT_OBJECT_0 == WaitForSingleObject( m_CancelSaveEvent, 0 ) ) {
			break;
		}
	}
	PostMessage( hwnd, MSG_SAVECOMPLETED, 0 /*wParam*/, 0 /*lParam*/ );
}

void DlgTrackInfo::OnCancelSave()
{
	SetEvent( m_CancelSaveEvent );
	HCURSOR oldCursor = SetCursor( LoadCursor( NULL /*instance*/, IDC_WAIT ) );
	WaitForSingleObject( m_SaveThread, INFINITE );
	SetCursor( oldCursor );
}
