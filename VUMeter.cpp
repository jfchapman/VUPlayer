#include "VUMeter.h"

// Render thread millisecond interval.
constexpr DWORD kRenderThreadInterval = 15;

// Rise factor.
constexpr float kRiseFactor = 0.2f;

// Rounded corner width.
constexpr float kRoundedCornerWidth = 16.0f;

// Whether the meter resource has been loaded successfully.
bool VUMeter::s_MeterLoaded = false;

// Base meter image width.
DWORD VUMeter::s_MeterWidth = 0;

// Base meter image height.
DWORD VUMeter::s_MeterHeight = 0;

// Base meter image.
const BYTE* VUMeter::s_MeterBase = nullptr;

// Meter pin delta arrays (null terminated).
std::vector<const DWORD*> VUMeter::s_MeterPins = {};

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
	m_MeterImage(),
	m_MeterPin( nullptr ),
	m_OutputLevel( {} ),
	m_LeftDisplayLevel( 0 ),
	m_RightDisplayLevel( 0 ),
	m_MeterPosition( {} ),
	m_Decay( GetSettings().GetVUMeterDecay() ),
	m_IsStereo( stereo )
{
	if ( !s_MeterLoaded ) {
		// Load resources.
		if ( HRSRC hResource = FindResource( nullptr, MAKEINTRESOURCE( IDR_VUMETER ), RT_RCDATA ); nullptr != hResource ) {
			constexpr DWORD kHeaderBytes = 8;
			if ( const DWORD resourceSize = SizeofResource( nullptr, hResource ); resourceSize > kHeaderBytes ) {
				if ( HGLOBAL hGlobal = LoadResource( nullptr, hResource ); nullptr != hGlobal ) {
					if ( const BYTE* resourceData = static_cast<const BYTE*>( LockResource( hGlobal ) ); nullptr != resourceData ) {
						// Base image.
						const DWORD* header = reinterpret_cast<const DWORD*>( resourceData );
						s_MeterWidth = header[ 0 ];
						s_MeterHeight = header[ 1 ];
						if ( ( 0 != s_MeterWidth ) && ( 0 != s_MeterHeight ) ) {
							if ( const DWORD imageBytes = s_MeterWidth * s_MeterHeight * 4; imageBytes < ( resourceSize - kHeaderBytes ) ) {
								s_MeterBase = resourceData + kHeaderBytes;

								// Meter pin deltas.
								const DWORD* data = reinterpret_cast<const DWORD*>( resourceData + kHeaderBytes + imageBytes );
								const DWORD dataSize = ( resourceSize - imageBytes - kHeaderBytes ) / 4;
								if ( ( dataSize > 0 ) && ( 0 == data[ dataSize - 1 ] ) ) {
									s_MeterPins.clear();
									s_MeterPins.push_back( data );
									DWORD offset = 0;
									bool pinsOK = true;
									do {
										do {
											pinsOK = ( data[ offset ] >> 8 ) < imageBytes;
										} while ( 0 != data[ offset++ ] );

										if ( offset < dataSize ) {
											s_MeterPins.push_back( data + offset );
										}
									} while ( pinsOK && ( offset < dataSize ) );

									s_MeterLoaded = pinsOK;
								}
							}
						}
					}
				}
			}
		}
	}

	if ( s_MeterLoaded ) {
		m_MeterImage.resize( s_MeterWidth * s_MeterHeight * 4 );
	}
}

VUMeter::~VUMeter()
{
	StopRenderThread();
	CloseHandle( m_RenderStopEvent );
	CloseHandle( m_RenderThread );
	FreeResources();
}

int VUMeter::GetHeight( const int width )
{
	const int height = s_MeterLoaded ? ( static_cast<int>( static_cast<float>( width ) * ( m_IsStereo ? 2 : 1 ) / s_MeterWidth * s_MeterHeight ) ) : 0;
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
		m_OutputLevel = {};
		m_LeftDisplayLevel = 0;
		m_RightDisplayLevel = 0;
		m_MeterPosition = {};
	}
}

void VUMeter::RenderThreadHandler()
{
	DWORD result = 0;
	do {
		if ( GetLevels() ) {
			DoRender();
		}
		result = WaitForSingleObject( m_RenderStopEvent, kRenderThreadInterval );
	} while ( WAIT_OBJECT_0 != result );
}

bool VUMeter::GetLevels()
{
	auto& [leftOutput, rightOutput] = m_OutputLevel;
	GetOutput().GetLevels( leftOutput, rightOutput );
	if ( !m_IsStereo ) {
		if ( leftOutput > rightOutput ) {
			rightOutput = leftOutput;
		} else {
			leftOutput = rightOutput;
		}
	}
	leftOutput = std::clamp( leftOutput, 0.0f, 1.0f );
	rightOutput = std::clamp( rightOutput, 0.0f, 1.0f );

	float leftDisplay = m_LeftDisplayLevel;
	float rightDisplay = m_RightDisplayLevel;

	if ( leftDisplay < leftOutput ) {
		leftDisplay += ( leftOutput - leftDisplay ) * kRiseFactor;
	} else {
		leftDisplay -= m_Decay;
		if ( leftDisplay < leftOutput ) {
			leftDisplay = leftOutput;
		}
	}

	if ( rightDisplay < rightOutput ) {
		rightDisplay += ( rightOutput - rightDisplay ) * kRiseFactor;
	} else {
		rightDisplay -= m_Decay;
		if ( rightDisplay < rightOutput ) {
			rightDisplay = rightOutput;
		}
	}

	const bool levelsChanged = ( leftDisplay != m_LeftDisplayLevel ) || ( rightDisplay != m_RightDisplayLevel );
	if ( levelsChanged ) {
		m_LeftDisplayLevel = leftDisplay;
		m_RightDisplayLevel = rightDisplay;
	}

	return levelsChanged;
}

void VUMeter::DrawPin( const int position )
{
	if ( s_MeterLoaded ) {
		const int pos = std::clamp( position, 0, static_cast<int>( s_MeterPins.size() - 1 ) );
		const DWORD* pinBuffer = s_MeterPins[ pos ];
		if ( m_MeterPin != pinBuffer ) {
			// Blank out the previous pin position.
			std::copy( s_MeterBase, s_MeterBase + s_MeterWidth * s_MeterHeight * 4, m_MeterImage.data() );

			// Draw the current pin.
			int index = 0;
			while ( 0 != pinBuffer[ index ] ) {
				m_MeterImage[ pinBuffer[ index ] >> 8 ] = static_cast<BYTE>( pinBuffer[ index ] & 0xff );
				++index;
			}

			m_MeterPin = pinBuffer;
		}
	}
}

void VUMeter::UpdateBitmaps( const float leftLevel, const float rightLevel )
{
	if ( s_MeterLoaded ) {
		auto& [leftMeter, rightMeter] = m_MeterPosition;

		const int leftPosition = GetPinPosition( leftLevel );
		const int rightPosition = GetPinPosition( rightLevel );

		if ( ( leftMeter != leftPosition ) && m_BitmapLeft ) {
			leftMeter = leftPosition;
			DrawPin( leftMeter );
			const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, s_MeterWidth /*right*/, s_MeterHeight /*bottom*/ );
			const UINT32 pitch = s_MeterWidth * 4;
			m_BitmapLeft->CopyFromMemory( &destRect, m_MeterImage.data(), pitch );
		}

		if ( ( rightMeter != rightPosition ) && m_BitmapRight ) {
			rightMeter = rightPosition;
			DrawPin( rightMeter );
			const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, s_MeterWidth /*right*/, s_MeterHeight /*bottom*/ );
			const UINT32 pitch = s_MeterWidth * 4;
			m_BitmapRight->CopyFromMemory( &destRect, m_MeterImage.data(), pitch );
		}
	}
}

void VUMeter::OnPaint()
{
	ID2D1DeviceContext* deviceContext = BeginDrawing();
	if ( nullptr != deviceContext ) {
		LoadResources( deviceContext );
		const D2D1_SIZE_F targetSize = deviceContext->GetSize();
		if ( s_MeterLoaded && ( targetSize.width > 0 ) && ( targetSize.height > 0 ) ) {
			UpdateBitmaps( m_LeftDisplayLevel, m_RightDisplayLevel );
			const float halfHeight = targetSize.height / ( m_IsStereo ? 2.0f : 1.0f );
			const D2D1_RECT_F leftRect = D2D1::RectF( 0, 0, targetSize.width, halfHeight );
			const D2D1_RECT_F rightRect = D2D1::RectF( 0, leftRect.bottom, targetSize.width, leftRect.bottom + halfHeight );
			const FLOAT opacity = 1.0;
			const D2D1_INTERPOLATION_MODE interpolationMode = IsHardwareAccelerationEnabled() ? D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC : D2D1_INTERPOLATION_MODE_LINEAR;
			if ( m_BitmapLeft ) {
				deviceContext->DrawBitmap( m_BitmapLeft.Get(), &leftRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
			if ( m_BitmapRight ) {
				deviceContext->DrawBitmap( m_BitmapRight.Get(), &rightRect, opacity, interpolationMode, NULL /*srcRect*/, NULL /*transform*/ );
			}
			if ( m_Brush ) {
				const float strokeWidth = kRoundedCornerWidth * targetSize.width / s_MeterWidth;
				D2D1_ROUNDED_RECT roundedRect = {};
				roundedRect.radiusX = strokeWidth * 2;
				roundedRect.radiusY = strokeWidth * 2;
				roundedRect.rect = D2D1::RectF( -strokeWidth /*left*/, -strokeWidth /*top*/, targetSize.width + strokeWidth, targetSize.height + strokeWidth );
				deviceContext->DrawRoundedRectangle( roundedRect, m_Brush.Get(), strokeWidth * 2 );
			}
		}
		EndDrawing();
	}
}

void VUMeter::OnSettingsChange()
{
	m_Decay = GetSettings().GetVUMeterDecay();
	FreeResources();
}

void VUMeter::OnSysColorChange()
{
	FreeResources();
}

void VUMeter::LoadResources( ID2D1DeviceContext* deviceContext )
{
	if ( nullptr != deviceContext ) {

		if ( !m_Brush ) {
			deviceContext->CreateSolidColorBrush( D2D1::ColorF( GetSysColor( COLOR_3DFACE ) ), &m_Brush );
		}

		if ( s_MeterLoaded && !m_BitmapLeft && !m_BitmapRight ) {
			const D2D1_SIZE_U bitmapSize = D2D1::SizeU( s_MeterWidth, s_MeterHeight );
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
			const D2D1_RECT_U destRect = D2D1::RectU( 0 /*left*/, 0 /*top*/, s_MeterWidth /*right*/, s_MeterHeight /*bottom*/ );
			const UINT32 pitch = s_MeterWidth * 4;
			if ( m_BitmapLeft ) {
				m_BitmapLeft->CopyFromMemory( &destRect, m_MeterImage.data(), pitch );
			}
			if ( m_BitmapRight ) {
				m_BitmapRight->CopyFromMemory( &destRect, m_MeterImage.data(), pitch );
			}
		}
	}
}

void VUMeter::FreeResources()
{
	m_BitmapLeft.Reset();
	m_BitmapRight.Reset();
	m_Brush.Reset();
}

int VUMeter::GetPinPosition( const float level )
{
	return static_cast<int>( level * ( s_MeterPins.size() - 1 ) + 0.5f );
}
