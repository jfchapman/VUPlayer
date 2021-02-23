#include "OptionsArtwork.h"

#include "resource.h"
#include "Utility.h"
#include "VUPlayer.h"

#include <filesystem>

// Initial folder setting for choosing artwork.
static const std::string s_ArtworkFolderSetting = "Artwork";

OptionsArtwork::OptionsArtwork( HINSTANCE instance, Settings& settings, Output& output ) :
	Options( instance, settings, output )
{
}

void OptionsArtwork::OnInit( const HWND hwnd )
{
	const auto artworkPath = GetSettings().GetDefaultArtwork();
	SetDlgItemText( hwnd, IDC_OPTIONS_ARTWORK_PATH, artworkPath.c_str() );

	if ( artworkPath.empty() ) {
		CheckDlgButton( hwnd, IDC_OPTIONS_ARTWORK_DEFAULT, BST_CHECKED );
	} else {
		CheckDlgButton( hwnd, IDC_OPTIONS_ARTWORK_CUSTOM, BST_CHECKED );
	}

	VUPlayer* vuplayer = VUPlayer::Get();
	if ( nullptr != vuplayer ) {
		m_Artwork = vuplayer->GetPlaceholderImage();
	}
}

void OptionsArtwork::OnSave( const HWND hwnd )
{
	const int pathSize = MAX_PATH;
	WCHAR path[ pathSize ] = {};
	if ( IsDlgButtonChecked( hwnd, IDC_OPTIONS_ARTWORK_CUSTOM ) ) {
		GetDlgItemText( hwnd, IDC_OPTIONS_ARTWORK_PATH, path, pathSize );
	}
	GetSettings().SetDefaultArtwork( path );
}

void OptionsArtwork::OnCommand( const HWND hwnd, const WPARAM wParam, const LPARAM /*lParam*/ )
{
	const WORD notificationCode = HIWORD( wParam );
	const WORD controlID = LOWORD( wParam );
	if ( BN_CLICKED == notificationCode ) {
		switch ( controlID ) {
			case IDC_OPTIONS_ARTWORK_BROWSE : {
				OnChooseArtwork( hwnd );
				break;
			}
			case IDC_OPTIONS_ARTWORK_DEFAULT : 
			case IDC_OPTIONS_ARTWORK_CUSTOM : {
				LoadArtwork( hwnd );
				break;
			}
		}
	}
}

void OptionsArtwork::OnDrawItem( const HWND, const WPARAM wParam, const LPARAM lParam )
{
	if ( DRAWITEMSTRUCT* drawitem = reinterpret_cast<DRAWITEMSTRUCT*>( lParam ); ( nullptr != drawitem ) && ( IDC_OPTIONS_ARTWORK_IMAGE == wParam ) ) {
		OnDrawArtwork( drawitem );
	}
}

void OptionsArtwork::OnChooseArtwork( const HWND hwnd )
{
	if ( const auto filename = ChooseArtwork( GetInstanceHandle(), hwnd, GetSettings().GetLastFolder( s_ArtworkFolderSetting ) ); !filename.empty() ) {
		const std::filesystem::path path( filename );
		GetSettings().SetLastFolder( s_ArtworkFolderSetting, path.parent_path() );
		SetDlgItemText( hwnd, IDC_OPTIONS_ARTWORK_PATH, filename.c_str() );
		if ( !IsDlgButtonChecked( hwnd, IDC_OPTIONS_ARTWORK_CUSTOM ) ) {
			CheckDlgButton( hwnd, IDC_OPTIONS_ARTWORK_DEFAULT, BST_UNCHECKED );
			CheckDlgButton( hwnd, IDC_OPTIONS_ARTWORK_CUSTOM, BST_CHECKED );
		}
		LoadArtwork( hwnd );
	}
}

void OptionsArtwork::LoadArtwork( const HWND hwnd )
{
	m_Artwork.reset();
	if ( IsDlgButtonChecked( hwnd, IDC_OPTIONS_ARTWORK_CUSTOM ) ) {
		const int pathSize = MAX_PATH;
		WCHAR path[ pathSize ] = {};
		GetDlgItemText( hwnd, IDC_OPTIONS_ARTWORK_PATH, path, pathSize );
		if ( std::filesystem::exists( path ) ) {
			try {
				m_Artwork = std::make_unique<Gdiplus::Bitmap>( path );
			} catch ( ... ) {}
		}
	}

	if ( !m_Artwork || ( m_Artwork->GetWidth() == 0 ) || ( m_Artwork->GetHeight() == 0 ) ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			m_Artwork = vuplayer->GetPlaceholderImage( false /*useSetting*/ );
		}	
	}

	InvalidateRect( GetDlgItem( hwnd, IDC_OPTIONS_ARTWORK_IMAGE ), nullptr /*rect*/, FALSE /*erase*/ );
}

void OptionsArtwork::OnDrawArtwork( DRAWITEMSTRUCT* drawItemStruct )
{
	if ( ( nullptr != drawItemStruct ) && ( IDC_OPTIONS_ARTWORK_IMAGE == drawItemStruct->CtlID ) ) {
		const int clientWidth = drawItemStruct->rcItem.right - drawItemStruct->rcItem.left;
		const int clientHeight = drawItemStruct->rcItem.bottom - drawItemStruct->rcItem.top;
		Gdiplus::Rect clientRect( 0, 0, clientWidth, clientHeight );
		Gdiplus::Graphics graphics( drawItemStruct->hDC );
		graphics.Clear( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ) );
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
