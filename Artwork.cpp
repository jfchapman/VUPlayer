#include "Artwork.h"

#include "VUPlayer.h"
#include "Utility.h"

// Minimum visual height.
static const int s_MinHeight = static_cast<int>( 100 * GetDPIScaling() );

Artwork::Artwork( WndVisual& wndVisual ) :
	Visual( wndVisual ),
	m_ArtworkID( L"Init" ),
	m_PreviousSize( {} )
{
}

Artwork::~Artwork()
{
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

		if ( m_Bitmap ) {
			const D2D1_SIZE_F targetSize = deviceContext->GetSize();
			const D2D1_SIZE_F bitmapSize = m_Bitmap->GetSize();
			if ( ( bitmapSize.width > 0 ) && ( bitmapSize.height > 0 ) ) {
				const D2D1_RECT_F srcRect = D2D1::RectF( 0.0f /*left*/, 0.0f /*top*/, bitmapSize.width /*right*/, bitmapSize.height /*bottom*/ );
				const D2D1_RECT_F destRect = { 0, 0, targetSize.width, targetSize.height };
				const FLOAT opacity = 1.0;

				const D2D1_INTERPOLATION_MODE interpolationMode = ( ( m_PreviousSize.width == targetSize.width ) && ( m_PreviousSize.height == targetSize.height ) ) ?
					D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC : D2D1_INTERPOLATION_MODE_LINEAR;
				m_PreviousSize = targetSize;

				deviceContext->DrawBitmap( m_Bitmap.Get(), destRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
		}
		EndDrawing();
	}
}

void Artwork::OnSettingsChange()
{
	FreeResources();
}

void Artwork::OnSysColorChange()
{
}

void Artwork::LoadArtwork( const MediaInfo& mediaInfo, ID2D1DeviceContext* deviceContext )
{
	const std::wstring artworkID = mediaInfo.GetArtworkID( true /*checkFolder*/ );
	if ( ( artworkID != m_ArtworkID ) && ( nullptr != deviceContext ) ) {
		FreeResources();
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

void Artwork::FreeResources()
{
	m_Bitmap.Reset();
	m_ArtworkID = L"Init";
}

std::unique_ptr<Gdiplus::Bitmap> Artwork::GetArtworkBitmap( const MediaInfo& mediaInfo )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	Library& library = GetLibrary();
	const std::vector<BYTE> imageBytes = library.GetMediaArtwork( mediaInfo );
	if ( !imageBytes.empty() ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( &imageBytes[ 0 ], static_cast<ULONG>( imageBytes.size() ), NULL /*bytesWritten*/ ) ) ) {
				try {
					bitmap = std::make_unique<Gdiplus::Bitmap>( stream );
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}

	if ( !bitmap || ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) {
		const std::wstring artworkID = mediaInfo.GetArtworkID( true /*checkFolder*/ );
		if ( !artworkID.empty() && ( artworkID != mediaInfo.GetArtworkID( false /*checkFolder*/ ) ) ) {
			try {
				bitmap = std::make_unique<Gdiplus::Bitmap>( artworkID.c_str() );
			} catch ( ... ) {
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
