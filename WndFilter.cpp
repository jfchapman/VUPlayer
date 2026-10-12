#include "WndFilter.h"

#include "resource.h"

#include "DarkMode.h"
#include "Utility.h"

#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <vector>

// Filter bar window class name.
static constexpr wchar_t s_FilterClass[] = L"VUFilterClass";

// Edit control ID.
static constexpr int s_EditID = 1001;

// Edit control subclass ID.
static constexpr UINT_PTR s_EditSubclassID = 1;

// Timer ID for applying the filter text while it is being typed.
static constexpr UINT_PTR s_FilterTimerID = 1;

// Delay before applying the filter text while it is being typed, in milliseconds.
static constexpr UINT s_FilterTimerDelay = 250;

// Scaling applied to the Windows message font (which is 9pt by default), so that the filter text is 10pt by default.
static constexpr float s_FontScaling = 10.0f / 9.0f;

// Additional height of the edit control (above the font height), in DIPs.
static constexpr int s_EditPadding = 8;

// Inset of the clear button highlight from the edit control client area, in DIPs.
static constexpr int s_ClearButtonInset = 2;

// Size of the clear button cross, in DIPs (before the font scaling is applied, so that the cross is in proportion with the filter text).
static constexpr float s_CrossSize = 8.0f;

// Dark mode cue banner (placeholder) text colour, which is lighter than the themed cue banner colour (3.8:1 contrast against the edit control background).
static constexpr COLORREF s_DarkModeCueBannerColour = RGB( 0x80 /*red*/, 0x80 /*green*/, 0x80 /*blue*/ );

// Dark mode border colour, when the edit control does not have the focus (which is more subdued than the themed border colour).
static constexpr COLORREF s_DarkModeBorderColour = RGB( 0x5c /*red*/, 0x5c /*green*/, 0x5c /*blue*/ );

// Returns the dark mode edit control background colour, which matches the default dark mode list background (rather than the other dark mode edit controls).
static COLORREF GetDarkModeBackgroundColour()
{
	return DarkMode::GetListBackgroundColour();
}

// Scales a value in DIPs to pixels.
static int Scale( const int value )
{
	return static_cast<int>( std::lround( value * GetDPIScaling() ) );
}

// Returns the margin below & to the right of the edit control. This matches the splitter width, so that the edit control appears to have the same border as the other main window panes.
// There is no left margin, so that the edit control aligns with the playlist, and no top margin, so that the edit control sits directly below the toolbar.
static int GetMargin()
{
	return GetSystemMetrics( SM_CXSIZEFRAME );
}

// Returns a blend of the 'foreground' colour into the 'background' colour, by the 'amount' (0 to 1).
static COLORREF Blend( const COLORREF background, const COLORREF foreground, const float amount )
{
	const auto component = [ amount ] ( const BYTE backgroundComponent, const BYTE foregroundComponent ) {
		return static_cast<BYTE>( backgroundComponent + ( foregroundComponent - backgroundComponent ) * amount );
	};
	return RGB( component( GetRValue( background ), GetRValue( foreground ) ),
		component( GetGValue( background ), GetGValue( foreground ) ),
		component( GetBValue( background ), GetBValue( foreground ) ) );
}

LRESULT CALLBACK WndFilter::FilterProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WndFilter* wndFilter = reinterpret_cast<WndFilter*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
	if ( nullptr != wndFilter ) {
		switch ( message ) {
			case WM_ERASEBKGND: {
				return TRUE;
			}
			case WM_PAINT: {
				PAINTSTRUCT ps = {};
				BeginPaint( hwnd, &ps );
				FillRect( ps.hdc, &ps.rcPaint, DarkMode::GetWindowBackgroundBrush() );
				EndPaint( hwnd, &ps );
				return 0;
			}
			case WM_SIZE: {
				wndFilter->Layout();
				break;
			}
			case WM_COMMAND: {
				if ( s_EditID == LOWORD( wParam ) ) {
					if ( EN_CHANGE == HIWORD( wParam ) ) {
						wndFilter->OnEditChanged();
					} else if ( EN_KILLFOCUS == HIWORD( wParam ) ) {
						// Apply any pending filter text straight away, in case the focus is moving to another playlist.
						wndFilter->ApplyFilter();
					}
				}
				break;
			}
			case WM_TIMER: {
				if ( s_FilterTimerID == wParam ) {
					wndFilter->ApplyFilter();
					return 0;
				}
				break;
			}
			case WM_CTLCOLOREDIT: {
				if ( const HBRUSH brush = wndFilter->OnCtlColorEdit( reinterpret_cast<HDC>( wParam ) ); nullptr != brush ) {
					return reinterpret_cast<LRESULT>( brush );
				}
				break;
			}
			default: {
				break;
			}
		}
	}
	return DefWindowProc( hwnd, message, wParam, lParam );
}

LRESULT CALLBACK WndFilter::EditProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData )
{
	WndFilter* wndFilter = reinterpret_cast<WndFilter*>( refData );
	switch ( message ) {
		case WM_GETDLGCODE: {
			// The main window treats Enter & Escape as dialog commands, so ask for these keys to be passed to the edit control instead.
			LRESULT code = DefSubclassProc( hwnd, message, wParam, lParam );
			if ( const MSG* msg = reinterpret_cast<const MSG*>( lParam ); ( nullptr != msg ) && ( ( WM_KEYDOWN == msg->message ) || ( WM_CHAR == msg->message ) ) && ( ( VK_RETURN == msg->wParam ) || ( VK_ESCAPE == msg->wParam ) ) ) {
				code |= DLGC_WANTMESSAGE;
			}
			return code;
		}
		case WM_KEYDOWN: {
			switch ( wParam ) {
				case VK_ESCAPE: {
					wndFilter->ClearFilter( true /*focusList*/ );
					return 0;
				}
				case VK_RETURN:
				case VK_DOWN: {
					wndFilter->ApplyFilter();
					wndFilter->FocusList();
					return 0;
				}
				default: {
					break;
				}
			}
			break;
		}
		case WM_CHAR: {
			switch ( wParam ) {
				case VK_RETURN:
				case VK_ESCAPE: {
					// Prevent the edit control from beeping.
					return 0;
				}
				case 0x7f: {
					// Ctrl+Backspace (which the edit control would otherwise insert as a character).
					wndFilter->DeletePreviousWord();
					return 0;
				}
				default: {
					break;
				}
			}
			break;
		}
		case WM_PAINT: {
			// The clear button (and in dark mode, the cue banner) is drawn after the edit control has painted itself.
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			wndFilter->DrawDarkModeCueBanner();
			wndFilter->DrawClearButton();
			return result;
		}
		case WM_NCPAINT: {
			// In dark mode, the themed border includes an inner edge drawn in the theme's own background colour, so paint over it with the edit control background colour.
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			wndFilter->DrawDarkModeInnerBorder();
			return result;
		}
		case WM_SETCURSOR: {
			if ( ( hwnd == reinterpret_cast<HWND>( wParam ) ) && ( HTCLIENT == LOWORD( lParam ) ) ) {
				POINT point = {};
				GetCursorPos( &point );
				ScreenToClient( hwnd, &point );
				if ( wndFilter->IsOverClearButton( point ) ) {
					SetCursor( LoadCursor( nullptr, IDC_ARROW ) );
					return TRUE;
				}
			}
			break;
		}
		case WM_MOUSEMOVE:
		case WM_MOUSELEAVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_CAPTURECHANGED: {
			if ( wndFilter->OnEditMouseMessage( message, lParam ) ) {
				return 0;
			}
			// In dark mode, the themed edit control redraws itself when the mouse enters or leaves it (to show a hover state), which causes flicker.
			// The hover state is not used (to match light mode), so the edit control is not passed mouse movement unless a mouse button is held (e.g. to select text).
			if ( DarkMode::IsActive() && ( ( WM_MOUSELEAVE == message ) || ( ( WM_MOUSEMOVE == message ) && !( MK_LBUTTON & wParam ) && ( GetCapture() != hwnd ) ) ) ) {
				return 0;
			}
			break;
		}
		case WM_NCMOUSEMOVE:
		case WM_NCMOUSELEAVE: {
			// See above (the themed edit control also tracks the mouse over its border).
			if ( DarkMode::IsActive() ) {
				return 0;
			}
			break;
		}
		case WM_NCDESTROY: {
			RemoveWindowSubclass( hwnd, EditProc, subclassID );
			break;
		}
		default: {
			break;
		}
	}
	return DefSubclassProc( hwnd, message, wParam, lParam );
}

WndFilter::WndFilter( HINSTANCE instance, HWND parent, FilterChangedCallback onFilterChanged, FocusListCallback onFocusList, VisibilityChangedCallback onVisibilityChanged ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_hEdit( nullptr ),
	m_Font( nullptr ),
	m_DarkModeBackgroundBrush( nullptr ),
	m_EditHeight( 0 ),
	m_CueBanner(),
	m_WasEmpty( true ),
	m_OnFilterChanged( onFilterChanged ),
	m_OnFocusList( onFocusList ),
	m_OnVisibilityChanged( onVisibilityChanged ),
	m_AppliedFilter(),
	m_IgnoreChange( false ),
	m_ClearButtonHot( false ),
	m_ClearButtonPressed( false ),
	m_ClearButtonCapture( false )
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof( WNDCLASSEX );
	wc.hCursor = LoadCursor( 0, IDC_ARROW );
	wc.hInstance = instance;
	wc.lpfnWndProc = FilterProc;
	wc.lpszClassName = s_FilterClass;
	RegisterClassEx( &wc );

	// The filter bar is initially hidden.
	const DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	const DWORD exStyle = WS_EX_CONTROLPARENT;
	m_hWnd = CreateWindowEx( exStyle, s_FilterClass, NULL, style, 0, 0, 0, 0, parent, NULL, instance, NULL );
	SetWindowLongPtr( m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

	NONCLIENTMETRICS metrics = {};
	metrics.cbSize = sizeof( NONCLIENTMETRICS );
	if ( SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof( NONCLIENTMETRICS ), &metrics, 0 ) ) {
		metrics.lfMessageFont.lfHeight = static_cast<LONG>( std::lround( metrics.lfMessageFont.lfHeight * s_FontScaling ) );
		m_Font = CreateFontIndirect( &metrics.lfMessageFont );
	}

	m_hEdit = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>( static_cast<INT_PTR>( s_EditID ) ), instance, NULL );
	SetWindowSubclass( m_hEdit, EditProc, s_EditSubclassID, reinterpret_cast<DWORD_PTR>( this ) );
	if ( nullptr != m_Font ) {
		SetWindowFont( m_hEdit, m_Font, FALSE /*redraw*/ );
	}
	WCHAR cueBanner[ 64 ] = {};
	LoadString( m_hInst, IDS_FILTER_CUEBANNER, cueBanner, static_cast<int>( std::size( cueBanner ) ) );
	m_CueBanner = cueBanner;

	int fontHeight = 0;
	if ( const HDC dc = GetDC( m_hEdit ); nullptr != dc ) {
		const HGDIOBJ previousFont = SelectObject( dc, ( nullptr != m_Font ) ? m_Font : GetStockObject( DEFAULT_GUI_FONT ) );
		TEXTMETRIC textMetric = {};
		if ( GetTextMetrics( dc, &textMetric ) ) {
			fontHeight = textMetric.tmHeight;
		}
		SelectObject( dc, previousFont );
		ReleaseDC( m_hEdit, dc );
	}
	m_EditHeight = fontHeight + Scale( s_EditPadding );

	// Only the height of the filter bar is significant here, the position & width are set by the parent window layout.
	SetWindowPos( m_hWnd, nullptr, 0, 0, 0, m_EditHeight + GetMargin(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );

	ApplyDarkMode();
}

WndFilter::~WndFilter()
{
	if ( nullptr != m_Font ) {
		DeleteObject( m_Font );
	}
	if ( nullptr != m_DarkModeBackgroundBrush ) {
		DeleteObject( m_DarkModeBackgroundBrush );
	}
}

HWND WndFilter::GetWindowHandle() const
{
	return m_hWnd;
}

HWND WndFilter::GetEditHandle() const
{
	return m_hEdit;
}

bool WndFilter::IsShown() const
{
	// Note that the style is checked, rather than using IsWindowVisible, so that the result does not depend on whether the main window is visible.
	return ( WS_VISIBLE & GetWindowLongPtr( m_hWnd, GWL_STYLE ) );
}

bool WndFilter::HasFocus() const
{
	return ( nullptr != m_hEdit ) && ( GetFocus() == m_hEdit );
}

void WndFilter::Activate()
{
	SetShown( true );
	SetFocus( m_hEdit );
	Edit_SetSel( m_hEdit, 0, -1 );
}

void WndFilter::SetShown( const bool shown )
{
	if ( shown != IsShown() ) {
		if ( !shown ) {
			// Move the focus before hiding the filter bar, so that the focus is not left on a hidden window.
			if ( const HWND focus = GetFocus(); ( nullptr != focus ) && ( ( focus == m_hWnd ) || IsChild( m_hWnd, focus ) ) ) {
				FocusList();
			}
		}
		ShowWindow( m_hWnd, shown ? SW_SHOWNA : SW_HIDE );
		if ( m_OnVisibilityChanged ) {
			m_OnVisibilityChanged();
		}
	}
}

void WndFilter::ClearFilter( const bool focusList )
{
	m_IgnoreChange = true;
	SetWindowText( m_hEdit, L"" );
	m_IgnoreChange = false;
	ApplyFilter();
	if ( focusList ) {
		FocusList();
	} else {
		SetFocus( m_hEdit );
	}
}

void WndFilter::SetFilter( const std::wstring& filter )
{
	KillTimer( m_hWnd, s_FilterTimerID );
	m_IgnoreChange = true;
	SetWindowText( m_hEdit, filter.c_str() );
	m_IgnoreChange = false;
	m_AppliedFilter = filter;
}

void WndFilter::ApplyDarkMode()
{
	DarkMode::ApplyToEdit( m_hEdit );
	UpdateCueBanner();
	// Changing the theme can reset the edit control margins.
	SetEditMargins();
	RedrawWindow( m_hWnd, nullptr /*rect*/, nullptr /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
}

void WndFilter::OnSysColorChange()
{
	RedrawWindow( m_hWnd, nullptr /*rect*/, nullptr /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN );
}

void WndFilter::Layout()
{
	RECT clientRect = {};
	GetClientRect( m_hWnd, &clientRect );
	const int margin = GetMargin();
	const int editWidth = std::max( 0, static_cast<int>( clientRect.right ) - margin );
	MoveWindow( m_hEdit, 0 /*x*/, 0 /*y*/, editWidth, m_EditHeight, TRUE /*repaint*/ );
	SetEditMargins();
}

void WndFilter::SetEditMargins()
{
	const RECT buttonRect = GetClearButtonRect();
	const int rightMargin = static_cast<int>( buttonRect.right - buttonRect.left );
	SendMessage( m_hEdit, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELPARAM( 0, rightMargin ) );
}

void WndFilter::OnEditChanged()
{
	// The edit control draws typed text directly, so make sure that the clear button is shown or hidden as the text changes.
	const RECT buttonRect = GetClearButtonRect();
	InvalidateRect( m_hEdit, &buttonRect, TRUE /*erase*/ );

	// In dark mode, the cue banner is drawn separately, so make sure that it is shown or hidden when the edit control becomes empty or non-empty.
	const bool isEmpty = ( 0 == GetWindowTextLength( m_hEdit ) );
	if ( ( isEmpty != m_WasEmpty ) && DarkMode::IsActive() ) {
		InvalidateRect( m_hEdit, nullptr /*rect*/, TRUE /*erase*/ );
	}
	m_WasEmpty = isEmpty;

	if ( !m_IgnoreChange ) {
		SetTimer( m_hWnd, s_FilterTimerID, s_FilterTimerDelay, nullptr /*timerProc*/ );
	}
}

void WndFilter::ApplyFilter()
{
	KillTimer( m_hWnd, s_FilterTimerID );
	const int length = GetWindowTextLength( m_hEdit );
	std::vector<WCHAR> buffer( 1 + length, 0 );
	GetWindowText( m_hEdit, buffer.data(), static_cast<int>( buffer.size() ) );
	const std::wstring filter = buffer.data();
	if ( filter != m_AppliedFilter ) {
		m_AppliedFilter = filter;
		if ( m_OnFilterChanged ) {
			m_OnFilterChanged( filter );
		}
	}
}

void WndFilter::FocusList()
{
	if ( m_OnFocusList ) {
		m_OnFocusList();
	}
}

void WndFilter::DeletePreviousWord()
{
	DWORD selectionStart = 0;
	DWORD selectionEnd = 0;
	SendMessage( m_hEdit, EM_GETSEL, reinterpret_cast<WPARAM>( &selectionStart ), reinterpret_cast<LPARAM>( &selectionEnd ) );
	if ( selectionStart == selectionEnd ) {
		const int length = GetWindowTextLength( m_hEdit );
		std::vector<WCHAR> buffer( 1 + length, 0 );
		GetWindowText( m_hEdit, buffer.data(), static_cast<int>( buffer.size() ) );
		DWORD wordStart = std::min( selectionStart, static_cast<DWORD>( length ) );
		while ( ( wordStart > 0 ) && iswspace( buffer[ wordStart - 1 ] ) ) {
			--wordStart;
		}
		while ( ( wordStart > 0 ) && !iswspace( buffer[ wordStart - 1 ] ) ) {
			--wordStart;
		}
		Edit_SetSel( m_hEdit, wordStart, selectionEnd );
	}
	Edit_ReplaceSel( m_hEdit, L"" );
}

HBRUSH WndFilter::OnCtlColorEdit( const HDC dc )
{
	if ( DarkMode::IsActive() ) {
		if ( nullptr == m_DarkModeBackgroundBrush ) {
			m_DarkModeBackgroundBrush = CreateSolidBrush( GetDarkModeBackgroundColour() );
		}
		SetTextColor( dc, DarkMode::GetTextColour() );
		SetBkColor( dc, GetDarkModeBackgroundColour() );
		return m_DarkModeBackgroundBrush;
	}
	return nullptr;
}

void WndFilter::DrawDarkModeInnerBorder()
{
	if ( !DarkMode::IsActive() ) {
		return;
	}

	const HDC dc = GetWindowDC( m_hEdit );
	if ( nullptr == dc ) {
		return;
	}

	// Fill the non-client area inside the outer border line (i.e. between the border line and the client area), excluding the client area itself.
	RECT windowRect = {};
	GetWindowRect( m_hEdit, &windowRect );
	RECT clientRect = {};
	GetClientRect( m_hEdit, &clientRect );
	MapWindowPoints( m_hEdit, nullptr, reinterpret_cast<LPPOINT>( &clientRect ), 2 );
	OffsetRect( &clientRect, -windowRect.left, -windowRect.top );
	OffsetRect( &windowRect, -windowRect.left, -windowRect.top );

	RECT innerRect = windowRect;
	InflateRect( &innerRect, -1, -1 );
	ExcludeClipRect( dc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom );
	if ( const HBRUSH brush = CreateSolidBrush( GetDarkModeBackgroundColour() ); nullptr != brush ) {
		FillRect( dc, &innerRect, brush );
		DeleteObject( brush );
	}

	// The themed border is too bright against the main window, so it is subdued unless the edit control has the focus (in which case the themed border is kept).
	// The border is not highlighted when the mouse is over the edit control, to match the light mode behaviour.
	if ( GetFocus() != m_hEdit ) {
		if ( const HBRUSH brush = CreateSolidBrush( s_DarkModeBorderColour ); nullptr != brush ) {
			FrameRect( dc, &windowRect, brush );
			DeleteObject( brush );
		}
	}

	ReleaseDC( m_hEdit, dc );
}

void WndFilter::UpdateCueBanner()
{
	LPCWSTR cueBanner = DarkMode::IsActive() ? L"" : m_CueBanner.c_str();
	SendMessage( m_hEdit, EM_SETCUEBANNER, TRUE /*drawFocused*/, reinterpret_cast<LPARAM>( cueBanner ) );
}

void WndFilter::DrawDarkModeCueBanner()
{
	if ( !DarkMode::IsActive() || m_CueBanner.empty() || ( GetWindowTextLength( m_hEdit ) > 0 ) ) {
		return;
	}

	const HDC dc = GetDC( m_hEdit );
	if ( nullptr == dc ) {
		return;
	}

	// The cue banner is drawn in the same position as the edit control text (i.e. at the top left of the formatting rectangle).
	// Note that the caret is hidden while drawing, as it is drawn by inverting the screen contents.
	HideCaret( m_hEdit );
	RECT formatRect = {};
	SendMessage( m_hEdit, EM_GETRECT, 0, reinterpret_cast<LPARAM>( &formatRect ) );
	const HGDIOBJ previousFont = SelectObject( dc, GetWindowFont( m_hEdit ) );
	SetTextColor( dc, s_DarkModeCueBannerColour );
	SetBkMode( dc, TRANSPARENT );
	DrawText( dc, m_CueBanner.c_str(), static_cast<int>( m_CueBanner.size() ), &formatRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS );
	SelectObject( dc, previousFont );
	ShowCaret( m_hEdit );

	ReleaseDC( m_hEdit, dc );
}

RECT WndFilter::GetClearButtonRect() const
{
	// The clear button is a square at the right of the edit control client area.
	RECT clientRect = {};
	GetClientRect( m_hEdit, &clientRect );
	const LONG size = std::min( clientRect.bottom - clientRect.top, clientRect.right - clientRect.left );
	return { clientRect.right - size, clientRect.top, clientRect.right, clientRect.bottom };
}

bool WndFilter::IsClearButtonShown() const
{
	return GetWindowTextLength( m_hEdit ) > 0;
}

bool WndFilter::IsOverClearButton( const POINT& point ) const
{
	const RECT buttonRect = GetClearButtonRect();
	return IsClearButtonShown() && PtInRect( &buttonRect, point );
}

void WndFilter::DrawClearButton()
{
	if ( !IsClearButtonShown() ) {
		return;
	}

	const HDC dc = GetDC( m_hEdit );
	if ( nullptr == dc ) {
		return;
	}

	const bool darkMode = DarkMode::IsActive();
	const bool highContrast = IsHighContrastActive();
	const COLORREF backgroundColour = darkMode ? GetDarkModeBackgroundColour() : GetSysColor( COLOR_WINDOW );
	const COLORREF textColour = darkMode ? DarkMode::GetTextColour() : GetSysColor( COLOR_WINDOWTEXT );

	const RECT buttonRect = GetClearButtonRect();
	if ( const HBRUSH brush = CreateSolidBrush( backgroundColour ); nullptr != brush ) {
		FillRect( dc, &buttonRect, brush );
		DeleteObject( brush );
	}

	// The cross is drawn in a subdued colour, unless the mouse is over the button (in which case the button background is also highlighted).
	COLORREF crossColour = highContrast ? textColour : Blend( backgroundColour, textColour, 0.6f );
	if ( m_ClearButtonHot || m_ClearButtonPressed ) {
		RECT highlightRect = buttonRect;
		const int inset = Scale( s_ClearButtonInset );
		InflateRect( &highlightRect, -inset, -inset );
		COLORREF highlightColour = {};
		if ( highContrast ) {
			highlightColour = GetSysColor( COLOR_HIGHLIGHT );
			crossColour = GetSysColor( COLOR_HIGHLIGHTTEXT );
		} else {
			// The highlight is a blend of the text colour into the background colour, in the same way as the toolbar buttons.
			const float amount = darkMode ? ( m_ClearButtonPressed ? 0.3f : 0.15f ) : ( m_ClearButtonPressed ? 0.2f : 0.1f );
			highlightColour = Blend( backgroundColour, textColour, amount );
			crossColour = textColour;
		}
		if ( const HBRUSH brush = CreateSolidBrush( highlightColour ); nullptr != brush ) {
			FillRect( dc, &highlightRect, brush );
			DeleteObject( brush );
		}
	}

	{
		const float dpiScaling = GetDPIScaling();
		const float crossSize = s_CrossSize * s_FontScaling * dpiScaling;
		const float centreX = ( buttonRect.left + buttonRect.right ) / 2.0f;
		const float centreY = ( buttonRect.top + buttonRect.bottom ) / 2.0f;
		Gdiplus::Graphics graphics( dc );
		graphics.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );
		graphics.SetPixelOffsetMode( Gdiplus::PixelOffsetModeHalf );
		Gdiplus::Color colour;
		colour.SetFromCOLORREF( crossColour );
		const Gdiplus::Pen pen( colour, std::max( 1.0f, dpiScaling ) );
		graphics.DrawLine( &pen, centreX - crossSize / 2, centreY - crossSize / 2, centreX + crossSize / 2, centreY + crossSize / 2 );
		graphics.DrawLine( &pen, centreX - crossSize / 2, centreY + crossSize / 2, centreX + crossSize / 2, centreY - crossSize / 2 );
	}

	ReleaseDC( m_hEdit, dc );
}

void WndFilter::SetClearButtonState( const bool hot, const bool pressed )
{
	if ( ( hot != m_ClearButtonHot ) || ( pressed != m_ClearButtonPressed ) ) {
		m_ClearButtonHot = hot;
		m_ClearButtonPressed = pressed;
		const RECT buttonRect = GetClearButtonRect();
		InvalidateRect( m_hEdit, &buttonRect, TRUE /*erase*/ );
	}
}

bool WndFilter::OnEditMouseMessage( const UINT message, const LPARAM lParam )
{
	const POINT point = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
	switch ( message ) {
		case WM_MOUSEMOVE: {
			const bool over = IsOverClearButton( point );
			if ( m_ClearButtonCapture ) {
				// The button is shown as pressed only while the mouse is over it.
				SetClearButtonState( over, over );
				return true;
			}
			if ( over && !m_ClearButtonHot ) {
				TRACKMOUSEEVENT tme = {};
				tme.cbSize = sizeof( TRACKMOUSEEVENT );
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = m_hEdit;
				TrackMouseEvent( &tme );
			}
			SetClearButtonState( over, false );
			break;
		}
		case WM_MOUSELEAVE: {
			if ( !m_ClearButtonCapture ) {
				SetClearButtonState( false, false );
			}
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK: {
			if ( IsOverClearButton( point ) ) {
				m_ClearButtonCapture = true;
				SetCapture( m_hEdit );
				SetClearButtonState( true, true );
				return true;
			}
			break;
		}
		case WM_LBUTTONUP: {
			if ( m_ClearButtonCapture ) {
				const bool over = IsOverClearButton( point );
				ReleaseCapture();
				SetClearButtonState( false, false );
				if ( over ) {
					ClearFilter( false /*focusList*/ );
				}
				return true;
			}
			break;
		}
		case WM_CAPTURECHANGED: {
			if ( m_ClearButtonCapture ) {
				m_ClearButtonCapture = false;
				SetClearButtonState( m_ClearButtonHot, false );
				return true;
			}
			break;
		}
		default: {
			break;
		}
	}
	return false;
}
