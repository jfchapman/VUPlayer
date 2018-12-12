#include "Artwork.h"

#include "VUPlayer.h"
#include "Utility.h"

// Minimum visual height.
static const int s_MinHeight = static_cast<int>( 100 * GetDPIScaling() );

Artwork::Artwork( WndVisual& wndVisual ) :
	Visual( wndVisual ),
	m_Brush( nullptr ),
	m_Bitmap( nullptr ),
	m_ArtworkID( L"Init" )
{
}

Artwork::~Artwork()
{
	if ( nullptr != m_Brush ) {
		m_Brush->Release();
	}
	if ( nullptr != m_Bitmap ) {
		m_Bitmap->Release();
	}
}

int Artwork::GetHeight( const int width )
{
	int height = width;
	const MediaInfo mediaInfo = ( Output::State::Stopped == GetOutput().GetState() ) ?
			GetOutput().GetCurrentSelectedPlaylistItem().Info : GetOutput().GetCurrentPlaying().PlaylistItem.Info;
	std::shared_ptr<Gdiplus::Bitmap> bitmap = GetArtworkBitmap( mediaInfo );
	if ( bitmap ) {
		const D2D1_SIZE_F bitmapSize = D2D1::SizeF( static_cast<FLOAT>( bitmap->GetWidth() ), static_cast<FLOAT>( bitmap->GetHeight() ) );
		if ( ( bitmapSize.height > 0 ) && ( bitmapSize.width > 0 ) ) {
			const FLOAT aspect = bitmapSize.width / bitmapSize.height;
			height = static_cast<int>( width / aspect );
			if ( height < s_MinHeight ) {
				height = s_MinHeight;
			}
		}
	}
	return height;
}

void Artwork::Show()
{
}

void Artwork::Hide()
{
}

void Artwork::OnPaint()
{
	ID2D1DeviceContext* deviceContext = BeginDrawing();
	if ( nullptr != deviceContext ) {
		const MediaInfo mediaInfo = ( Output::State::Stopped == GetOutput().GetState() ) ?
				GetOutput().GetCurrentSelectedPlaylistItem().Info : GetOutput().GetCurrentPlaying().PlaylistItem.Info;

		LoadArtwork( mediaInfo, deviceContext );

		const D2D1_SIZE_F targetSize = deviceContext->GetSize();

		if ( nullptr == m_Brush ) {
			deviceContext->CreateSolidColorBrush( D2D1::ColorF( GetSysColor( COLOR_3DFACE ) ), &m_Brush );
		}
		if ( nullptr != m_Brush ) {
			const D2D1_RECT_F rect = D2D1::RectF( 0 /*left*/, 0 /*top*/, targetSize.width, targetSize.height );
			deviceContext->FillRectangle( rect, m_Brush );
		}

		if ( nullptr != m_Bitmap ) {
			const D2D1_SIZE_F bitmapSize = m_Bitmap->GetSize();
			if ( ( bitmapSize.height > 0 ) && ( bitmapSize.width > 0 ) ) {
				const D2D1_RECT_F srcRect = D2D1::RectF( 0.0f /*left*/, 0.0f /*top*/, bitmapSize.width /*right*/, bitmapSize.height /*bottom*/ );

				const D2D1_RECT_F destRect = { 0, 0, targetSize.width, targetSize.height };
				const FLOAT opacity = 1.0;
				const D2D1_INTERPOLATION_MODE interpolationMode = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
				deviceContext->DrawBitmap( m_Bitmap, destRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
		}
		EndDrawing();
	}
}

void Artwork::OnSettingsChanged()
{
}

void Artwork::LoadArtwork( const MediaInfo& mediaInfo, ID2D1DeviceContext* deviceContext )
{
	const std::wstring artworkID = mediaInfo.GetArtworkID();
	if ( ( artworkID != m_ArtworkID ) && ( nullptr != deviceContext ) ) {
		FreeArtwork();
		std::shared_ptr<Gdiplus::Bitmap> bitmap = GetArtworkBitmap( mediaInfo );
		if ( bitmap ) {
			Gdiplus::Rect rect( 0 /*x*/, 0 /*y*/, bitmap->GetWidth(), bitmap->GetHeight() );
			Gdiplus::BitmapData bitmapData = {};
			if ( Gdiplus::Ok == bitmap->LockBits( &rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData ) ) {			
				const D2D1_SIZE_U bitmapSize = D2D1::SizeU( bitmap->GetWidth(), bitmap->GetHeight() );
				D2D1_BITMAP_PROPERTIES bitmapProperties = {};
				bitmapProperties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
				const UINT pitch = bitmapSize.width * 4;
				deviceContext->CreateBitmap( bitmapSize, bitmapData.Scan0, pitch, bitmapProperties, &m_Bitmap );
				bitmap->UnlockBits( &bitmapData );
				m_ArtworkID = artworkID;
			}
		}
	}
}

void Artwork::FreeArtwork()
{
	if ( nullptr != m_Bitmap ) {
		m_Bitmap->Release();
		m_Bitmap = nullptr;
	}
	m_ArtworkID = std::wstring();
}

std::shared_ptr<Gdiplus::Bitmap> Artwork::GetArtworkBitmap( const MediaInfo& mediaInfo )
{
	std::shared_ptr<Gdiplus::Bitmap> bitmapPtr;
	Library& library = GetLibrary();
	const std::vector<BYTE> imageBytes = library.GetMediaArtwork( mediaInfo );
	if ( !imageBytes.empty() ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( &imageBytes[ 0 ], static_cast<ULONG>( imageBytes.size() ), NULL /*bytesWritten*/ ) ) ) {
				try {
					bitmapPtr = std::shared_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap( stream ) );
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}
  if ( !bitmapPtr || ( bitmapPtr->GetWidth() == 0 ) || ( bitmapPtr->GetHeight() == 0 ) ) {
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			bitmapPtr = vuplayer->GetPlaceholderImage();
		}
	}
	return bitmapPtr;
}
