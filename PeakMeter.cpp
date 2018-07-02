#include "PeakMeter.h"

// Render thread millisecond interval.
static const DWORD s_RenderThreadInterval = 15;

// Decay factor.
static const float s_DecayFactor = 0.02f;

// Render thread procedure
static DWORD WINAPI RenderThreadProc( LPVOID lpParam )
{
	PeakMeter* meter = reinterpret_cast<PeakMeter*>( lpParam );
	if ( nullptr != meter ) {
		meter->RenderThreadHandler();
	}
	return 0;
}

PeakMeter::PeakMeter( WndVisual& wndVisual ) :
	Visual( wndVisual ),
	m_RenderThread( NULL ),
	m_RenderStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Colour( nullptr ),
	m_BackgroundColour( nullptr ),
	m_LeftLevel( 0 ),
	m_RightLevel( 0 )
{
}

PeakMeter::~PeakMeter()
{
	StopRenderThread();
	CloseHandle( m_RenderStopEvent );
	CloseHandle( m_RenderThread );
	FreeResources();
}

int PeakMeter::GetHeight( const int width )
{
	return width / 6;
}
void PeakMeter::Show()
{
	StartRenderThread();
}

void PeakMeter::Hide()
{
	StopRenderThread();
}

void PeakMeter::StartRenderThread()
{
	if ( nullptr == m_RenderThread ) {
		ResetEvent( m_RenderStopEvent );
		m_RenderThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, RenderThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

void PeakMeter::StopRenderThread()
{
	if ( nullptr != m_RenderThread ) {
		SetEvent( m_RenderStopEvent );
		WaitForSingleObject( m_RenderThread, INFINITE );
		CloseHandle( m_RenderThread );
		m_RenderThread = nullptr;
	}
}

void PeakMeter::RenderThreadHandler()
{
	DWORD result = 0;
	do {
		DoRender();	
		result = WaitForSingleObject( m_RenderStopEvent, s_RenderThreadInterval );
	} while ( WAIT_OBJECT_0 != result );
}

void PeakMeter::OnPaint()
{
	ID2D1DeviceContext* deviceContext = BeginDrawing();
	if ( nullptr != deviceContext ) {
		LoadResources( deviceContext );
		const D2D1_SIZE_F targetSize = deviceContext->GetSize();
		if ( ( targetSize.height > 0 ) && ( targetSize.height > 0 ) ) {
			if ( nullptr != m_BackgroundColour ) {
				const D2D1_RECT_F rect = D2D1::RectF( 0 /*left*/, 0 /*top*/, targetSize.width, targetSize.height );
				deviceContext->FillRectangle( rect, m_BackgroundColour );
			}
			if ( nullptr != m_Colour ) {
				m_Colour->SetStartPoint( D2D1::Point2F( 0 /*x*/, 0 /*y*/ ) );
				m_Colour->SetEndPoint( D2D1::Point2F( targetSize.width /*x*/, 0 /*y*/ ) );

				GetLevels();

				const int borderSize = 2;
				const int elementWidth = 4;
				const int width = static_cast<int>( targetSize.width );
				const FLOAT elementCornerRadius = static_cast<FLOAT>( elementWidth / 2 );

				const FLOAT litOpacity = 1.0f;
				const FLOAT unlitOpacity = 0.5f;

				FLOAT top = static_cast<FLOAT>( borderSize );
				FLOAT bottom = ( targetSize.height - ( borderSize / 2 ) ) / 2;
				for ( int pos = borderSize; pos < ( width - elementWidth ); pos += elementWidth ) {
					const FLOAT left = static_cast<FLOAT>( pos );
					const FLOAT right = static_cast<FLOAT>( pos + elementWidth - 1 );
					const FLOAT opacity = ( ( m_LeftLevel * targetSize.width ) > left ) ? litOpacity : unlitOpacity;
					m_Colour->SetOpacity( opacity );
					const D2D1_RECT_F rect = D2D1::RectF( left, top, right, bottom );
					const D2D1_ROUNDED_RECT roundedRect = { rect, elementCornerRadius, elementCornerRadius };
					deviceContext->FillRoundedRectangle( roundedRect, m_Colour );			
				}

				top = ( targetSize.height + ( borderSize / 2 ) ) / 2;
				bottom = targetSize.height - borderSize;
				for ( int pos = borderSize; pos < ( width - elementWidth ); pos += elementWidth ) {
					const FLOAT left = static_cast<FLOAT>( pos );
					const FLOAT right = static_cast<FLOAT>( pos + elementWidth - 1 );
					const FLOAT opacity = ( ( m_RightLevel * targetSize.width ) > left ) ? litOpacity : unlitOpacity;
					m_Colour->SetOpacity( opacity );
					const D2D1_RECT_F rect = D2D1::RectF( left, top, right, bottom );
					const D2D1_ROUNDED_RECT roundedRect = { rect, elementCornerRadius, elementCornerRadius };
					deviceContext->FillRoundedRectangle( roundedRect, m_Colour );			
				}
			}
		}
		EndDrawing();
	}
}

void PeakMeter::OnSettingsChanged()
{
	FreeResources();
	DoRender();
}

void PeakMeter::LoadResources( ID2D1DeviceContext* deviceContext )
{
	if ( nullptr != deviceContext ) {
		if ( ( nullptr == m_Colour ) && ( nullptr == m_BackgroundColour ) ) {
			COLORREF base = 0;
			COLORREF peak = 0;
			COLORREF background = 0;
			GetSettings().GetPeakMeterSettings( base, peak, background );

			D2D1_GRADIENT_STOP gradientStop[ 2 ];
			gradientStop[ 0 ].color = D2D1::ColorF( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( base ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( base ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( base ) ) / 0xff,
					0xff ) );
			gradientStop[ 0 ].position = 0.0f;
			gradientStop[ 1 ].color = D2D1::ColorF( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( peak ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( peak ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( peak ) ) / 0xff,
					0xff ) );
			gradientStop[ 1 ].position = 1.0f;

			ID2D1GradientStopCollection* gradientStops = nullptr;
			HRESULT hr = deviceContext->CreateGradientStopCollection( gradientStop, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &gradientStops );
			if ( SUCCEEDED( hr ) ) {
				hr = deviceContext->CreateLinearGradientBrush( 
						D2D1::LinearGradientBrushProperties( D2D1::Point2F() /*start*/, D2D1::Point2F() /*end*/ ), gradientStops, &m_Colour );
				gradientStops->Release();
			}

			deviceContext->CreateSolidColorBrush( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( background ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( background ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( background ) ) / 0xff,
					0xff ), &m_BackgroundColour );
		}
	}
}

void PeakMeter::FreeResources()
{
	if ( nullptr != m_Colour ) {
		m_Colour->Release();
		m_Colour = nullptr;
	}
	if ( nullptr != m_BackgroundColour ) {
		m_BackgroundColour->Release();
		m_BackgroundColour = nullptr;
	}
}

void PeakMeter::GetLevels()
{
	float left = 0;
	float right = 0;
	GetOutput().GetLevels( left, right );

	if ( left > m_LeftLevel ) {
		m_LeftLevel += ( left - m_LeftLevel ) / 2;
		if ( m_LeftLevel > 1.0f ) {
			m_LeftLevel = 1.0f;
		}
	} else {
		m_LeftLevel -= s_DecayFactor;
		if ( m_LeftLevel < left ) {
			m_LeftLevel = left;
		}
	}

	if ( right > m_RightLevel ) {
		m_RightLevel += ( right - m_RightLevel ) / 2;
		if ( m_RightLevel > 1.0f ) {
			m_RightLevel = 1.0f;
		}
	} else {
		m_RightLevel -= s_DecayFactor;
		if ( m_RightLevel < right ) {
			m_RightLevel = right;
		}
	}
}
