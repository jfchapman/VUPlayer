#include "SpectrumAnalyser.h"

// Render thread millisecond interval.
static const DWORD s_RenderThreadInterval = 15;

// Decay factor.
static const int s_DecayFactor = 40;

DWORD WINAPI SpectrumAnalyser::RenderThreadProc( LPVOID lpParam )
{
	SpectrumAnalyser* analyser = reinterpret_cast<SpectrumAnalyser*>( lpParam );
	if ( nullptr != analyser ) {
		analyser->RenderThreadHandler();
	}
	return 0;
}

SpectrumAnalyser::SpectrumAnalyser( WndVisual& wndVisual ) :
	Visual( wndVisual ),
	m_RenderThread( NULL ),
	m_RenderStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Colour( nullptr ),
	m_BackgroundColour( nullptr ),
	m_Values()
{
}

SpectrumAnalyser::~SpectrumAnalyser()
{
	StopRenderThread();
	CloseHandle( m_RenderStopEvent );
	CloseHandle( m_RenderThread );
	FreeResources();
}

int SpectrumAnalyser::GetHeight( const int width )
{
	return width / 2;
}
void SpectrumAnalyser::Show()
{
	StartRenderThread();
}

void SpectrumAnalyser::Hide()
{
	StopRenderThread();
}

void SpectrumAnalyser::StartRenderThread()
{
	if ( nullptr == m_RenderThread ) {
		ResetEvent( m_RenderStopEvent );
		m_RenderThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, RenderThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

void SpectrumAnalyser::StopRenderThread()
{
	if ( nullptr != m_RenderThread ) {
		SetEvent( m_RenderStopEvent );
		WaitForSingleObject( m_RenderThread, INFINITE );
		CloseHandle( m_RenderThread );
		m_RenderThread = nullptr;
	}
}

void SpectrumAnalyser::RenderThreadHandler()
{
	DWORD result = 0;
	do {
		DoRender();	
		result = WaitForSingleObject( m_RenderStopEvent, s_RenderThreadInterval );
	} while ( WAIT_OBJECT_0 != result );
}

void SpectrumAnalyser::OnPaint()
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
				m_Colour->SetStartPoint( D2D1::Point2F( 0, 0 ) );
				m_Colour->SetEndPoint( D2D1::Point2F( 0, targetSize.height ) );

				std::vector<float> fft;
				GetOutput().GetFFTData( fft );
				const size_t fftSize = fft.size();
				if ( fftSize > 0 ) {
					const long width = static_cast<long>( targetSize.width );

					if ( m_Values.size() != static_cast<size_t>( width ) ) {
						m_Values.resize( width, targetSize.height );
					}

					const float decay = targetSize.height / s_DecayFactor;

					for ( long pos = 1; ( pos < width ); pos += 3 ) {
						const size_t bin = static_cast<size_t>( pow( fftSize - 1, pow( ( static_cast<float>( pos ) / width ), 0.4 ) ) );
						const float value = fft.at( bin );
						float y = ( -targetSize.height / 4.0f ) * ( ( value < 0.0001 ) ? -4.0f : log10f( value ) );

						if ( y < m_Values.at( pos ) ) {
							m_Values.at( pos ) = y;
						} else {
							m_Values.at( pos ) += decay;
							if ( m_Values.at( pos ) > y ) {
								m_Values.at( pos ) = y;
							}
							if ( m_Values.at( pos ) > targetSize.height ) {
								m_Values.at( pos ) = targetSize.height;
							}
							y = m_Values.at( pos );
						}

						const D2D1_RECT_F rect = D2D1::RectF( static_cast<FLOAT>( pos - 1 ) /*left*/, y /*top*/, static_cast<FLOAT>( pos + 1 ) /*right*/, targetSize.height );
						deviceContext->FillRectangle( rect, m_Colour );
					}
				}
			}
		}
		EndDrawing();
	}
}

void SpectrumAnalyser::OnSettingsChanged()
{
	FreeResources();
	DoRender();
}

void SpectrumAnalyser::LoadResources( ID2D1DeviceContext* deviceContext )
{
	if ( nullptr != deviceContext ) {
		if ( ( nullptr == m_Colour ) && ( nullptr == m_BackgroundColour ) ) {
			COLORREF base = 0;
			COLORREF peak = 0;
			COLORREF background = 0;
			GetSettings().GetSpectrumAnalyserSettings( base, peak, background );

			D2D1_GRADIENT_STOP gradientStop[ 2 ];
			gradientStop[ 0 ].color = D2D1::ColorF( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( peak ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( peak ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( peak ) ) / 0xff,
					0xff ) );
			gradientStop[ 0 ].position = 0.5f;
			gradientStop[ 1 ].color = D2D1::ColorF( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( base ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( base ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( base ) ) / 0xff,
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

void SpectrumAnalyser::FreeResources()
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
