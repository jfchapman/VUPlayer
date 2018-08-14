#include "VUMeter.h"

#include "VUMeterData.h"

#include <fstream>
#include <iomanip>

// Render thread millisecond interval.
static const DWORD s_RenderThreadInterval = 15;

// Rise factor.
static const float s_RiseFactor = 0.2f;

// Rounded corner width.
static const float s_RoundedCornerWidth = 16.0f;

DWORD WINAPI VUMeter::RenderThreadProc( LPVOID lpParam )
{
	VUMeter* vumeter = reinterpret_cast<VUMeter*>( lpParam );
	if ( nullptr != vumeter ) {
		vumeter->RenderThreadHandler();
	}
	return 0;
}

VUMeter::VUMeter( WndVisual& wndVisual, const bool stereo ) :
	Visual( wndVisual ),
	m_RenderThread( NULL ),
	m_RenderStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_MeterImage( new BYTE[ VU_WIDTH * VU_HEIGHT * 4 ] ),
	m_MeterPin( nullptr ),
	m_BitmapLeft( nullptr ),
	m_BitmapRight( nullptr ),
	m_Brush( nullptr ),
	m_LeftLevel( 0 ),
	m_RightLevel( 0 ),
	m_Decay( GetSettings().GetVUMeterDecay() ),
	m_IsStereo( stereo )
{
	memcpy( m_MeterImage, VU_BASE, VU_WIDTH * VU_HEIGHT * 4 );
}

VUMeter::~VUMeter()
{
	StopRenderThread();
	CloseHandle( m_RenderStopEvent );
	CloseHandle( m_RenderThread );
	FreeResources();
	delete [] m_MeterImage;
}

int VUMeter::GetHeight( const int width )
{
	const int height = static_cast<int>( static_cast<float>( width ) * ( m_IsStereo ? 2 : 1 ) / VU_WIDTH * VU_HEIGHT );
	return height;
}

void VUMeter::Show()
{
	StartRenderThread();
}

void VUMeter::Hide()
{
	StopRenderThread();
}

void VUMeter::StartRenderThread()
{
	if ( nullptr == m_RenderThread ) {
		ResetEvent( m_RenderStopEvent );
		m_RenderThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, RenderThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

void VUMeter::StopRenderThread()
{
	if ( nullptr != m_RenderThread ) {
		SetEvent( m_RenderStopEvent );
		WaitForSingleObject( m_RenderThread, INFINITE );
		CloseHandle( m_RenderThread );
		m_RenderThread = nullptr;
		m_LeftLevel = 0;
		m_RightLevel = 0;
	}
}

void VUMeter::RenderThreadHandler()
{
	DWORD result = 0;
	do {
		if ( GetLevels() ) {
			DoRender();
		}
		result = WaitForSingleObject( m_RenderStopEvent, s_RenderThreadInterval );
	} while ( WAIT_OBJECT_0 != result );
}

bool VUMeter::GetLevels()
{
	bool levelsChanged = false;

	const int previousLeftPos = static_cast<int>( m_LeftLevel * VU_PINCOUNT + 0.5f );
	const int previousRightPos = static_cast<int>( m_RightLevel * VU_PINCOUNT + 0.5f );

	float left = 0.0f;
	float right = 0.0f;
	GetOutput().GetLevels( left, right );
	if ( !m_IsStereo ) {
		if ( left > right ) {
			right = left;
		} else {
			left = right;
		}
	}

	if ( left < 0.0f ) {
		left = 0.0f;
	} else if ( left > 1.0f ) {
		left = 1.0f;
	}
	if ( right < 0.0f ) {
		right = 0.0f;
	} else if ( right > 1.0f ) {
		right = 1.0f;
	}

	if ( m_LeftLevel < left ) {
		m_LeftLevel += ( left - m_LeftLevel ) * s_RiseFactor;
	} else {
		m_LeftLevel -= m_Decay;
		if ( m_LeftLevel < left ) {
			m_LeftLevel = left;
		}
	}

	if ( m_RightLevel < right ) {
		m_RightLevel += ( right - m_RightLevel ) * s_RiseFactor;
	} else {
		m_RightLevel -= m_Decay;
		if ( m_RightLevel < right ) {
			m_RightLevel = right;
		}
	}

	const int currentLeftPos = static_cast<int>( m_LeftLevel * VU_PINCOUNT + 0.5f );
	const int currentRightPos = static_cast<int>( m_RightLevel * VU_PINCOUNT + 0.5f );

	if ( ( currentLeftPos != previousLeftPos ) && ( nullptr != m_BitmapLeft ) ) {
		DrawPin( currentLeftPos );
		const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, VU_WIDTH /*right*/, VU_HEIGHT /*bottom*/ );
		const UINT32 pitch = VU_WIDTH * 4;
		m_BitmapLeft->CopyFromMemory( &destRect, m_MeterImage, pitch );
		levelsChanged = true;
	} 

	if ( ( currentRightPos != previousRightPos ) && ( nullptr != m_BitmapRight ) ) {
		DrawPin( currentRightPos );
		const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, VU_WIDTH /*right*/, VU_HEIGHT /*bottom*/ );
		const UINT32 pitch = VU_WIDTH * 4;
		m_BitmapRight->CopyFromMemory( &destRect, m_MeterImage, pitch );
		levelsChanged = true;
	}

	return levelsChanged;
}

void VUMeter::DrawPin( const int position )
{
	int pos = position;
	if ( pos < 0 ) {
		pos = 0;
	} else if ( pos > VU_PINCOUNT ) {
		pos = VU_PINCOUNT;
	}

	const DWORD* pinBuffer = VU_PIN[ pos ];
	if ( m_MeterPin != pinBuffer ) {
		// Blank out the previous pin position.
		memcpy( m_MeterImage, VU_BASE, VU_WIDTH * VU_HEIGHT * 4 );

		// Draw the current pin.
		int index = 0;
		while ( 0 != pinBuffer[ index ] ) {
			m_MeterImage[ pinBuffer[ index ] >> 8 ] = static_cast<BYTE>( pinBuffer[ index ] & 0xff );
			++index;
		}

		m_MeterPin = pinBuffer;
	}
}

void VUMeter::OnPaint()
{
	ID2D1DeviceContext* deviceContext = BeginDrawing();
	if ( nullptr != deviceContext ) {
		LoadResources( deviceContext );
		const D2D1_SIZE_F targetSize = deviceContext->GetSize();
		if ( ( targetSize.height > 0 ) && ( targetSize.height > 0 ) ) {
			const float halfHeight = targetSize.height / ( m_IsStereo ? 2.0f : 1.0f );
			const D2D1_RECT_F leftRect = D2D1::RectF( 0, 0, targetSize.width, halfHeight );
			const D2D1_RECT_F rightRect = D2D1::RectF( 0, leftRect.bottom, targetSize.width, leftRect.bottom + halfHeight );
			const FLOAT opacity = 1.0;
			const D2D1_INTERPOLATION_MODE interpolationMode = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
			if ( nullptr != m_BitmapLeft ) {
				deviceContext->DrawBitmap( m_BitmapLeft, &leftRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
			if ( nullptr != m_BitmapRight ) {
				deviceContext->DrawBitmap( m_BitmapRight, &rightRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
			if ( nullptr != m_Brush ) {
				const float strokeWidth = s_RoundedCornerWidth * targetSize.width / VU_WIDTH;
				D2D1_ROUNDED_RECT roundedRect = {};
				roundedRect.radiusX = strokeWidth * 2;
				roundedRect.radiusY = strokeWidth * 2;
				roundedRect.rect = D2D1::RectF( -strokeWidth /*left*/, -strokeWidth /*top*/, targetSize.width + strokeWidth, targetSize.height + strokeWidth );
				deviceContext->DrawRoundedRectangle( roundedRect, m_Brush, strokeWidth * 2 );
			}
		}
		EndDrawing();
	}
}

void VUMeter::OnSettingsChanged()
{
	m_Decay = GetSettings().GetVUMeterDecay();
}

void VUMeter::LoadResources( ID2D1DeviceContext* deviceContext )
{
	if ( nullptr != deviceContext ) {

		if ( nullptr == m_Brush ) {
			deviceContext->CreateSolidColorBrush( D2D1::ColorF( GetSysColor( COLOR_3DFACE ) ), &m_Brush );
		}

		if ( ( nullptr == m_BitmapLeft ) && ( nullptr == m_BitmapRight ) ) {
			const D2D1_SIZE_U bitmapSize = D2D1::SizeU( VU_WIDTH, VU_HEIGHT );
			D2D1_BITMAP_PROPERTIES bitmapProperties = {};
			bitmapProperties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
			HRESULT hr = deviceContext->CreateBitmap( bitmapSize, bitmapProperties, &m_BitmapLeft );
			if ( FAILED( hr ) ) {
				FreeResources();
			}
			if ( m_IsStereo ) {
				hr = deviceContext->CreateBitmap( bitmapSize, bitmapProperties, &m_BitmapRight );
				if ( FAILED( hr ) ) {
					FreeResources();
				}
			}

			DrawPin( 0 );
			const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, VU_WIDTH /*right*/, VU_HEIGHT /*bottom*/ );
			const UINT32 pitch = VU_WIDTH * 4;
			if ( nullptr != m_BitmapLeft ) {
				m_BitmapLeft->CopyFromMemory( &destRect, m_MeterImage, pitch );
			}
			if ( nullptr != m_BitmapRight ) {
				m_BitmapRight->CopyFromMemory( &destRect, m_MeterImage, pitch );
			}
		}
	}
}

void VUMeter::FreeResources()
{
	if ( nullptr != m_BitmapLeft ) {
		m_BitmapLeft->Release();
		m_BitmapLeft = nullptr;
	}
	if ( nullptr != m_BitmapRight ) {
		m_BitmapRight->Release();
		m_BitmapRight = nullptr;
	}
	if ( nullptr != m_Brush ) {
		m_Brush->Release();
		m_Brush = nullptr;
	}
}
