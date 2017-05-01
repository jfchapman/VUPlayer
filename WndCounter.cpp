#include "WndCounter.h"

#include "resource.h"

#include "Utility.h"
#include "VUPlayer.h"

// Counter control ID
static const UINT_PTR s_WndCounterID = 1300;

// Counter window class name
static const wchar_t s_CounterClass[] = L"VUCounterClass";

// Counter width
static const int s_CounterWidth = 130;

// Window procedure
static LRESULT CALLBACK WndCounterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndCounter* wndCounter = reinterpret_cast<WndCounter*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndCounter ) {
		switch ( message ) {
			case WM_PAINT : {
				PAINTSTRUCT ps;
				BeginPaint( hwnd, &ps );
				wndCounter->OnPaint( ps );
				EndPaint( hwnd, &ps );
				break;
			}
			case WM_ERASEBKGND : {
				return TRUE;
			}
			case WM_CONTEXTMENU : {
				POINT pt = {};
				pt.x = LOWORD( lParam );
				pt.y = HIWORD( lParam );
				wndCounter->OnContextMenu( pt );
				break;
			}
			case WM_COMMAND : {
				const UINT commandID = LOWORD( wParam );
				wndCounter->OnCommand( commandID );
				break;
			}
			case WM_LBUTTONDOWN : {
				wndCounter->Toggle();
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

WndCounter::WndCounter( HINSTANCE instance, HWND parent, Settings& settings, Output& output, const int height ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings ),
	m_Output( output ),
	m_Text(),
	m_Font(),
	m_Colour( RGB( 0 /*red*/, 122 /*green*/, 217 /*blue*/ ) ),
	m_ShowRemaining( false ),
	m_FontMidpoint( 0 )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hInstance = instance;
	wc.lpfnWndProc = WndCounterProc;
	wc.lpszClassName = s_CounterClass;
	RegisterClassEx( &wc );

	const DWORD style = WS_CHILD | WS_VISIBLE;
	const int x = 0;
	const int y = 0;
	const int width = static_cast<int>( s_CounterWidth * GetDPIScaling() );
	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( 0, s_CounterClass, NULL, style, x, y, width, height, parent, reinterpret_cast<HMENU>( s_WndCounterID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

	LOGFONT font;
	NONCLIENTMETRICS metrics = {};
	metrics.cbSize = sizeof( NONCLIENTMETRICS );
	if ( FALSE != SystemParametersInfo( SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0 /*winIni*/ ) ) {
		font = metrics.lfStatusFont;
		font.lfHeight *= 2;
		font.lfWeight = FW_HEAVY;
	}
	m_Settings.GetCounterSettings( font, m_Colour, m_ShowRemaining );
	SetCounterFont( font );
}

WndCounter::~WndCounter()
{
	SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( m_DefaultWndProc ) );
	m_Settings.SetCounterSettings( m_Font, m_Colour, m_ShowRemaining );
}

HWND WndCounter::GetWindowHandle()
{
	return m_hWnd;
}

WNDPROC WndCounter::GetDefaultWndProc()
{
	return m_DefaultWndProc;
}

void WndCounter::Refresh()
{
	const Output::Item item = m_Output.GetCurrentPlaying();
	float position = item.Position;
	if ( m_ShowRemaining ) {
		const float duration = item.PlaylistItem.Info.GetDuration();
		if ( duration > position ) {
			position = duration - position;
		} else {
			position = 0;
		}
	}
	std::wstring text = DurationToString( m_hInst, position, true /*colonDelimited*/ );
	if ( m_ShowRemaining ) {
		text = L"-" + text;
	}
	if ( text != m_Text ) {
		m_Text = text;
		Redraw();
	}
}

void WndCounter::Redraw()
{
	const HWND parentWnd = GetParent( m_hWnd );
	RECT rect = {};
	GetClientRect( m_hWnd, &rect );
	MapWindowPoints( m_hWnd, parentWnd, reinterpret_cast<LPPOINT>( &rect ), 2 /*numPoints*/ );
	InvalidateRect( parentWnd, &rect, TRUE /*erase*/ );
}

void WndCounter::OnPaint( const PAINTSTRUCT& ps )
{
	RECT clientRect = {};
	GetClientRect( m_hWnd, &clientRect );

	// Draw text to an offscreen bitmap.
	Gdiplus::Font font( ps.hdc, &m_Font );
	Gdiplus::Bitmap bitmap( clientRect.right - clientRect.left, clientRect.bottom, PixelFormat32bppARGB );
	Gdiplus::Graphics bitmapGraphics( &bitmap );
	Gdiplus::Color textColour;
	textColour.SetFromCOLORREF( m_Colour );
	const Gdiplus::SolidBrush textBrush( textColour );
	const Gdiplus::RectF rect( 0, 0, static_cast<Gdiplus::REAL>( bitmap.GetWidth() ), static_cast<Gdiplus::REAL>( bitmap.GetHeight() ) );
	Gdiplus::StringFormat format( Gdiplus::StringFormat::GenericTypographic() );
	format.SetAlignment( Gdiplus::StringAlignment::StringAlignmentCenter );
	format.SetLineAlignment( Gdiplus::StringAlignment::StringAlignmentNear );
	bitmapGraphics.SetTextRenderingHint( Gdiplus::TextRenderingHint::TextRenderingHintAntiAlias );
	const Gdiplus::REAL x = static_cast<Gdiplus::REAL>( clientRect.right - clientRect.left ) / 2;
	const Gdiplus::REAL y = static_cast<Gdiplus::REAL>( clientRect.bottom ) / 2 - m_FontMidpoint;
	const Gdiplus::PointF origin( x, y );
	bitmapGraphics.DrawString( m_Text.c_str(), -1 /*length*/, &font, origin, &format, &textBrush );

	// Draw the counter from the bitmap (to avoid flicker).
	Gdiplus::Graphics graphics( ps.hdc );
	graphics.DrawImage( &bitmap, 0.0f /*x*/, 0.0f /*y*/ );
}

void WndCounter::SetCounterFont( const LOGFONT& logfont )
{
	m_Font = logfont;
	// Calculate the vertical extents of the font so that text can be effectively centered when drawing the counter.
	RECT clientRect = {};
	GetClientRect( m_hWnd, &clientRect );
	Gdiplus::Bitmap bitmap( clientRect.right - clientRect.left, clientRect.right - clientRect.left, PixelFormat32bppARGB );
	Gdiplus::Graphics graphics( &bitmap );
	const Gdiplus::SolidBrush brush( static_cast<Gdiplus::ARGB>( Gdiplus::Color::Black ) );
	HDC dc = GetDC( m_hWnd );
	const Gdiplus::Font font( dc, &m_Font );
	ReleaseDC( m_hWnd, dc );
	Gdiplus::StringFormat format( Gdiplus::StringFormat::GenericTypographic() );
	format.SetAlignment( Gdiplus::StringAlignment::StringAlignmentNear );
	format.SetLineAlignment( Gdiplus::StringAlignment::StringAlignmentNear );
	graphics.SetTextRenderingHint( Gdiplus::TextRenderingHint::TextRenderingHintAntiAlias );
	graphics.Clear( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ) );
	Gdiplus::PointF origin( 0, 0 );
	graphics.DrawString( L":-0123456789", -1 /*length*/, &font, origin, &format, &brush );
	Gdiplus::Rect rect( 0, 0, bitmap.GetWidth(), bitmap.GetHeight() );
	Gdiplus::BitmapData bitmapData;
	if ( Gdiplus::Ok == bitmap.LockBits( &rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData ) ) {
		Gdiplus::ARGB* pixels = static_cast<Gdiplus::ARGB*>( bitmapData.Scan0 );
		UINT minY = bitmap.GetHeight();
		UINT maxY = 0;
		for ( UINT row = 0; row < bitmapData.Height; row++ ) {
			for ( UINT column = 0; column < bitmapData.Width; column++ ) {
				if ( 0xffffffff != pixels[ row * bitmapData.Stride / 4 + column ] ) {
					if ( row < minY ) {
						minY = row;
					}
					if ( row > maxY ) {
						maxY = row;
					}
					break;
				}
			}
		}
		bitmap.UnlockBits( &bitmapData );
		m_FontMidpoint = static_cast<float>( minY + maxY ) / 2;
	}
	Redraw();
}

void WndCounter::OnContextMenu( const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_COUNTER ) );
	if ( NULL != menu ) {
		HMENU countermenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != countermenu ) {
			CheckMenuItem( countermenu, m_ShowRemaining ? ID_VIEW_COUNTER_TRACKREMAINING : ID_VIEW_COUNTER_TRACKELAPSED, MF_BYCOMMAND | MF_CHECKED );
			TrackPopupMenu( countermenu, TPM_RIGHTBUTTON, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
}

void WndCounter::OnCommand( const UINT command )
{
	switch ( command ) {
		case ID_VIEW_COUNTER_TRACKELAPSED :
		case ID_VIEW_COUNTER_TRACKREMAINING : {
			SetTrackRemaining( ID_VIEW_COUNTER_TRACKREMAINING == command );
			break;
		}
		case ID_VIEW_COUNTER_FONTSTYLE : {
			OnSelectFont();
			break;
		}
		case ID_VIEW_COUNTER_FONTCOLOUR : {
			OnSelectColour();
			break;
		}
		default : {
			break;
		}
	}
}

void WndCounter::OnSelectFont()
{
	LOGFONT logFont = m_Font;
	CHOOSEFONT chooseFont = {};
	chooseFont.lStructSize = sizeof( CHOOSEFONT );
	chooseFont.hwndOwner = m_hWnd;
	chooseFont.Flags = CF_FORCEFONTEXIST | CF_NOVERTFONTS | CF_LIMITSIZE | CF_INITTOLOGFONTSTRUCT;
	chooseFont.nSizeMax = 32;
	chooseFont.lpLogFont = &logFont;
	if ( ( TRUE == ChooseFont( &chooseFont ) ) && ( nullptr != chooseFont.lpLogFont ) ) {
		SetCounterFont( *chooseFont.lpLogFont );
	}
}

void WndCounter::OnSelectColour()
{
	CHOOSECOLOR chooseColor = {};
	chooseColor.lStructSize = sizeof( CHOOSECOLOR );
	chooseColor.hwndOwner = m_hWnd;
	VUPlayer* vuplayer = VUPlayer::Get();
	if ( nullptr != vuplayer ) {
		chooseColor.lpCustColors = vuplayer->GetCustomColours();
	}
	chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
	chooseColor.rgbResult = m_Colour;
	if ( TRUE == ChooseColor( &chooseColor ) ) {
		m_Colour = chooseColor.rgbResult;
		Redraw();
	}
}

bool WndCounter::GetTrackRemaining() const
{
	return m_ShowRemaining;
}

void WndCounter::SetTrackRemaining( const bool showRemaining )
{
	if ( showRemaining != m_ShowRemaining ) {
		m_ShowRemaining = showRemaining;
		Refresh();
	}
}

void WndCounter::Toggle()
{
	m_ShowRemaining = !m_ShowRemaining;
	Refresh();
}
