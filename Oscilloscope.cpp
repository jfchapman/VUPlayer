#include "Oscilloscope.h"

// Render thread millisecond interval.
static const DWORD s_RenderThreadInterval = 15;

// Render thread procedure
static DWORD WINAPI RenderThreadProc( LPVOID lpParam )
{
	Oscilloscope* oscilloscope = reinterpret_cast<Oscilloscope*>( lpParam );
	if ( nullptr != oscilloscope ) {
		oscilloscope->RenderThreadHandler();
	}
	return 0;
}

Oscilloscope::Oscilloscope( WndVisual& wndVisual ) :
	Visual( wndVisual ),
	m_RenderThread( NULL ),
	m_RenderStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Colour( nullptr ),
	m_BackgroundColour( nullptr ),
	m_Weight( 2.0f )
{
}

Oscilloscope::~Oscilloscope()
{
	StopRenderThread();
	CloseHandle( m_RenderStopEvent );
	CloseHandle( m_RenderThread );
	FreeResources();
}

int Oscilloscope::GetHeight( const int width )
{
	return width;
}
void Oscilloscope::Show()
{
	StartRenderThread();
}

void Oscilloscope::Hide()
{
	StopRenderThread();
}

void Oscilloscope::StartRenderThread()
{
	if ( nullptr == m_RenderThread ) {
		ResetEvent( m_RenderStopEvent );
		m_RenderThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, RenderThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

void Oscilloscope::StopRenderThread()
{
	if ( nullptr != m_RenderThread ) {
		SetEvent( m_RenderStopEvent );
		WaitForSingleObject( m_RenderThread, INFINITE );
		CloseHandle( m_RenderThread );
		m_RenderThread = nullptr;
	}
}

void Oscilloscope::RenderThreadHandler()
{
	DWORD result = 0;
	do {
		DoRender();	
		result = WaitForSingleObject( m_RenderStopEvent, s_RenderThreadInterval );
	} while ( WAIT_OBJECT_0 != result );
}

void Oscilloscope::OnPaint()
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
				std::vector<float> sampleData;
				long channelCount = 0;
				const long scale = 4;
				const long width = static_cast<long>( scale * targetSize.width );
				GetOutput().GetSampleData( width, sampleData, channelCount );
				if ( channelCount > 0 ) {
					const FLOAT meterHeight = targetSize.height / channelCount;
					const long sampleDataWidth = static_cast<long>( sampleData.size() / channelCount );
					for ( long channelIndex = 0; channelIndex < channelCount; channelIndex++ ) {
						const FLOAT halfHeight = meterHeight / 2;
						const FLOAT centrePoint = channelIndex * meterHeight + halfHeight;
						if ( width <= sampleDataWidth ) {
							ID2D1Factory* factory = nullptr;
							deviceContext->GetFactory( &factory );
							if ( nullptr != factory ) {
								ID2D1PathGeometry* pathGeometry = nullptr;
								if ( SUCCEEDED( factory->CreatePathGeometry( &pathGeometry ) ) ) {
									ID2D1GeometrySink* sink = nullptr;
									if ( SUCCEEDED( pathGeometry->Open( &sink ) ) ) {
										sink->SetFillMode( D2D1_FILL_MODE_ALTERNATE );
										D2D1_POINT_2F point = D2D1::Point2F( 0 /*x*/, centrePoint - halfHeight * sampleData.at( channelIndex ) /*y*/ );
										sink->BeginFigure( point, D2D1_FIGURE_BEGIN_HOLLOW );
										for ( long pos = 1; ( pos < width / scale ); pos++ ) {
											point = D2D1::Point2F( static_cast<FLOAT>( pos ) /*x*/,
													centrePoint - halfHeight * sampleData.at( scale * pos * channelCount + channelIndex ) /*y*/ );
											sink->AddLine( point );
										}
										sink->EndFigure( D2D1_FIGURE_END_OPEN );
										sink->Close();
										sink->Release();
										deviceContext->DrawGeometry( pathGeometry, m_Colour, m_Weight );
									}
									pathGeometry->Release();
								}
								factory->Release();
							}
						} else {
							const D2D1_POINT_2F startPoint = D2D1::Point2F( 0 /*x*/, centrePoint );
							const D2D1_POINT_2F endPoint = D2D1::Point2F( static_cast<FLOAT>( width ) /*x*/, centrePoint );
							deviceContext->DrawLine( startPoint, endPoint, m_Colour, m_Weight );
						}
					}
				}
			}
		}
		EndDrawing();
	}
}

void Oscilloscope::OnSettingsChanged()
{
	FreeResources();
	DoRender();
}

void Oscilloscope::LoadResources( ID2D1DeviceContext* deviceContext )
{
	if ( nullptr != deviceContext ) {
		if ( ( nullptr == m_Colour ) && ( nullptr == m_BackgroundColour ) ) {
			const COLORREF colour = GetSettings().GetOscilloscopeColour();
			deviceContext->CreateSolidColorBrush( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( colour ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( colour ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( colour ) ) / 0xff,
					0xff ), &m_Colour );
			const COLORREF background = GetSettings().GetOscilloscopeBackground();
			deviceContext->CreateSolidColorBrush( D2D1::ColorF(
					static_cast<FLOAT>( GetRValue( background ) ) / 0xff,
					static_cast<FLOAT>( GetGValue( background ) ) / 0xff,
					static_cast<FLOAT>( GetBValue( background ) ) / 0xff,
					0xff ), &m_BackgroundColour );
			m_Weight = GetSettings().GetOscilloscopeWeight();
		}
	}
}

void Oscilloscope::FreeResources()
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
