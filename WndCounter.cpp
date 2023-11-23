#include "WndCounter.h"

#include "resource.h"

#include "Utility.h"
#include "VUPlayer.h"

// Counter control ID
static const UINT_PTR s_WndCounterID = 1300;

// Counter window class name
static const wchar_t s_CounterClass[] = L"VUCounterClass";

// Counter widths
static const std::map<Settings::ToolbarSize, int> s_CounterWidths = {
	{ Settings::ToolbarSize::Small, 135 },
	{ Settings::ToolbarSize::Medium, 150 },
	{ Settings::ToolbarSize::Large, 165 }
};

LRESULT CALLBACK WndCounter::CounterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndCounter* wndCounter = reinterpret_cast<WndCounter*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndCounter ) {
		switch ( message ) {
			case WM_PAINT : {
				PAINTSTRUCT ps = {};
				BeginPaint( hwnd, &ps );
				wndCounter->OnPaint( ps );
				EndPaint( hwnd, &ps );
				break;
			}
			case WM_ERASEBKGND : {
				return TRUE;
			}
			case WM_SIZE : {
				wndCounter->CentreFont();
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
			case WM_DESTROY : {
				SetWindowLongPtr( hwnd, DWLP_USER, 0 );
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

WndCounter::WndCounter( HINSTANCE instance, HWND parent, Settings& settings, Output& output ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings ),
	m_Output( output ),
	m_Text(),
	m_Font(),
	m_FontColour( DEFAULT_ICONCOLOUR ),
	m_BackgroundColour( GetSysColor( COLOR_WINDOW ) ),
	m_ShowRemaining( false ),
	m_FontMidpoint( 0 ),
	m_Width(),
	m_IsHighContrast( IsHighContrastActive() ),
	m_IsClassicTheme( IsClassicThemeActive() )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hCursor = LoadCursor( 0, IDC_ARROW );
	wc.hInstance = instance;
	wc.lpfnWndProc = CounterProc;
	wc.lpszClassName = s_CounterClass;
	RegisterClassEx( &wc );
	SetWidth( settings );

	const DWORD style = WS_CHILD | WS_VISIBLE;
	const int x = 0;
	const int y = 0;
	const int width = GetWidth().value_or( 0 );

	LPVOID param = NULL;
	m_hWnd = CreateWindowEx( 0, s_CounterClass, NULL, style, x, y, width, 0 /*height*/, parent, reinterpret_cast<HMENU>( s_WndCounterID ), instance, param );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

	LOGFONT font;
	NONCLIENTMETRICS metrics = {};
	metrics.cbSize = sizeof( NONCLIENTMETRICS );
	if ( FALSE != SystemParametersInfo( SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0 /*winIni*/ ) ) {
		font = metrics.lfStatusFont;
		font.lfHeight *= 2;
		font.lfWeight = FW_BOLD;
	}
	m_Settings.GetCounterSettings( font, m_FontColour, m_ShowRemaining );
	COLORREF unusedColour = {};
	m_Settings.GetToolbarColours( unusedColour, m_BackgroundColour );
	SetCounterFont( font );
}

WndCounter::~WndCounter()
{
	SaveSettings();
}

HWND WndCounter::GetWindowHandle()
{
	return m_hWnd;
}

void WndCounter::Refresh()
{
	const Output::Item item = m_Output.GetCurrentPlaying();
	double position = item.Position;
	if ( m_ShowRemaining ) {
		const double duration = item.PlaylistItem.Info.GetDuration();
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
	InvalidateRect( m_hWnd, nullptr, FALSE );
}

void WndCounter::OnPaint( const PAINTSTRUCT& ps )
{
	RECT clientRect = {};
	GetClientRect( m_hWnd, &clientRect );

	Gdiplus::Font font( ps.hdc, &m_Font );
	Gdiplus::Bitmap bitmap( clientRect.right - clientRect.left, clientRect.bottom, PixelFormat32bppARGB );
	Gdiplus::Graphics bitmapGraphics( &bitmap );

	Gdiplus::Color backgroundColour;
	backgroundColour.SetFromCOLORREF( m_IsHighContrast ? GetSysColor( COLOR_WINDOW ) : ( m_IsClassicTheme ? GetSysColor( COLOR_3DFACE ) : m_BackgroundColour ) );
	bitmapGraphics.Clear( backgroundColour );

	Gdiplus::Color textColour;
	textColour.SetFromCOLORREF( m_IsHighContrast ? GetSysColor( COLOR_HIGHLIGHT ) : m_FontColour );
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

	Gdiplus::Graphics displayGraphics( ps.hdc );
	displayGraphics.DrawImage( &bitmap, 0.0f /*x*/, 0.0f /*y*/ );
}

void WndCounter::SetCounterFont( const LOGFONT& logfont )
{
	m_Font = logfont;
	CentreFont();
}

void WndCounter::CentreFont()
{
	RECT clientRect = {};
	GetClientRect( m_hWnd, &clientRect );
	if ( clientRect.right - clientRect.left > 0 ) {
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
}

bool WndCounter::ShowContextMenu( const POINT& position )
{
	HMENU menu = LoadMenu( m_hInst, MAKEINTRESOURCE( IDR_MENU_COUNTER ) );
	if ( NULL != menu ) {
		HMENU countermenu = GetSubMenu( menu, 0 /*pos*/ );
		if ( NULL != countermenu ) {
			CheckMenuItem( countermenu, m_ShowRemaining ? ID_VIEW_COUNTER_TRACKREMAINING : ID_VIEW_COUNTER_TRACKELAPSED, MF_BYCOMMAND | MF_CHECKED );

			UINT enableColourChoice = m_IsHighContrast ? MF_DISABLED : MF_ENABLED;
			EnableMenuItem( menu, ID_VIEW_COUNTER_FONTCOLOUR, MF_BYCOMMAND | enableColourChoice );

			enableColourChoice = ( m_IsHighContrast || m_IsClassicTheme ) ? MF_DISABLED : MF_ENABLED;
			EnableMenuItem( menu, ID_TOOLBAR_COLOUR_BACKGROUND, MF_BYCOMMAND | enableColourChoice );

			TrackPopupMenu( countermenu, TPM_RIGHTBUTTON, position.x, position.y, 0 /*reserved*/, m_hWnd, NULL /*rect*/ );
		}
		DestroyMenu( menu );
	}
	return true;
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
		case ID_TOOLBAR_COLOUR_BACKGROUND : {
			SendMessage( GetParent( m_hWnd ), WM_COMMAND, command, 0 );
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
	chooseFont.nSizeMax = 36;
	chooseFont.lpLogFont = &logFont;
	if ( ( TRUE == ChooseFont( &chooseFont ) ) && ( nullptr != chooseFont.lpLogFont ) ) {
		SetCounterFont( *chooseFont.lpLogFont );
	}
}

void WndCounter::OnSelectColour()
{
	VUPlayer* vuplayer = VUPlayer::Get();
	COLORREF* customColours = ( nullptr != vuplayer ) ? vuplayer->GetCustomColours() : nullptr;
	if ( const auto colour = ChooseColour( m_hWnd, m_FontColour, customColours ); colour.has_value() ) {
		m_FontColour = colour.value();
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

void WndCounter::SaveSettings()
{
	m_Settings.SetCounterSettings( m_Font, m_FontColour, m_ShowRemaining );
}

int WndCounter::GetID() const
{
	return 0;
}

HWND WndCounter::GetWindowHandle() const
{
	return m_hWnd;
}

void WndCounter::SetWidth( Settings& settings )
{
	m_Width = std::nullopt;
	if ( const auto widthIter = s_CounterWidths.find( settings.GetToolbarSize() ); s_CounterWidths.end() != widthIter ) {
		m_Width = static_cast<int>( widthIter->second * GetDPIScaling() );
	}
}

std::optional<int> WndCounter::GetWidth() const
{
	return m_Width;
}

std::optional<int> WndCounter::GetHeight() const
{
	return std::nullopt;
}

bool WndCounter::HasDivider() const
{
	return true;
}

bool WndCounter::CanHide() const
{
	return false;
}

void WndCounter::OnChangeRebarItemSettings( Settings& settings )
{
	SetWidth( settings );
	COLORREF unusedColour = 0;
	settings.GetToolbarColours( unusedColour, m_BackgroundColour );
}

std::optional<LRESULT> WndCounter::OnCustomDraw( LPNMCUSTOMDRAW /*nmcd*/ )
{
	return std::nullopt;
}

void WndCounter::OnSysColorChange( const bool isHighContrast, const bool isClassicTheme )
{
	m_IsHighContrast = isHighContrast;
	m_IsClassicTheme = isClassicTheme;
	Redraw();
}
