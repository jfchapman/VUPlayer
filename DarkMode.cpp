#include "DarkMode.h"

#include "Utility.h"
#include "WndListScrollbar.h"

#include <CommCtrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <set>
#include <Uxtheme.h>
#include <vssym32.h>

namespace
{
	// Undocumented uxtheme.dll preferred app mode (Windows 10 1903 and later).
	enum class PreferredAppMode {
		Default,
		AllowDark,
		ForceDark,
		ForceLight,
		Max
	};

	// Undocumented uxtheme.dll functions (imported by ordinal).
	using fnRefreshImmersiveColorPolicyState = void ( WINAPI* )();												// Ordinal 104
	using fnAllowDarkModeForWindow = bool ( WINAPI* )( HWND hwnd, bool allow );						// Ordinal 133
	using fnAllowDarkModeForApp = bool ( WINAPI* )( bool allow );													// Ordinal 135 (Windows 10 1809)
	using fnSetPreferredAppMode = PreferredAppMode ( WINAPI* )( PreferredAppMode appMode );	// Ordinal 135 (Windows 10 1903 and later)
	using fnFlushMenuThemes = void ( WINAPI* )();																					// Ordinal 136

	fnRefreshImmersiveColorPolicyState s_RefreshImmersiveColorPolicyState = nullptr;
	fnAllowDarkModeForWindow s_AllowDarkModeForWindow = nullptr;
	fnAllowDarkModeForApp s_AllowDarkModeForApp = nullptr;
	fnSetPreferredAppMode s_SetPreferredAppMode = nullptr;
	fnFlushMenuThemes s_FlushMenuThemes = nullptr;

	// Undocumented menu bar drawing messages.
	constexpr UINT WM_UAHDRAWMENU = 0x0091;
	constexpr UINT WM_UAHDRAWMENUITEM = 0x0092;

	// Undocumented menu bar drawing structures.
	typedef union tagUAHMENUITEMMETRICS {
		struct {
			DWORD cx;
			DWORD cy;
		} rgsizeBar[ 2 ];
		struct {
			DWORD cx;
			DWORD cy;
		} rgsizePopup[ 4 ];
	} UAHMENUITEMMETRICS;

	typedef struct tagUAHMENUPOPUPMETRICS {
		DWORD rgcx[ 4 ];
		DWORD fUpdateMaxWidths : 2;
	} UAHMENUPOPUPMETRICS;

	typedef struct tagUAHMENU {
		HMENU hmenu;
		HDC hdc;
		DWORD dwFlags;
	} UAHMENU;

	typedef struct tagUAHMENUITEM {
		int iPosition;
		UAHMENUITEMMETRICS umim;
		UAHMENUPOPUPMETRICS umpm;
	} UAHMENUITEM;

	typedef struct tagUAHDRAWMENUITEM {
		DRAWITEMSTRUCT dis;
		UAHMENU um;
		UAHMENUITEM umi;
	} UAHDRAWMENUITEM;

	// DWM window attributes for a dark title bar.
	constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_PRE_20H1 = 19;
	constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_20H1 = 20;

	// Windows build numbers.
	constexpr DWORD s_Build1809 = 17763;
	constexpr DWORD s_Build1903 = 18362;

	// Current dark mode preference.
	DarkMode::Mode s_Mode = DarkMode::Mode::System;

	// Indicates whether dark mode is supported.
	bool s_Supported = false;

	// Indicates whether dark mode is currently active.
	bool s_Active = false;

	// Indicates whether the application level dark mode state has been modified.
	// Until dark mode is first activated, the application is left in its default (non dark mode) state.
	bool s_AppModeModified = false;

	// Indicates whether the main window dark mode state has been modified.
	// Until dark mode is first activated, the main window is left in its default (non dark mode) state.
	bool s_MainWindowModified = false;

	// Menu theme handle, used for drawing menu bar item text.
	HTHEME s_MenuTheme = nullptr;

	// Controls which currently have a dark mode theme applied.
	std::set<HWND> s_ThemedControls;

	// Applies the 'dark' title bar state to the top level window 'hwnd'.
	void ApplyToTitleBar( const HWND hwnd, const bool dark )
	{
		s_AllowDarkModeForWindow( hwnd, dark );
		const BOOL useDarkMode = dark ? TRUE : FALSE;
		if ( FAILED( DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_20H1, &useDarkMode, sizeof( useDarkMode ) ) ) ) {
			DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_PRE_20H1, &useDarkMode, sizeof( useDarkMode ) );
		}
	}

	// Dialog control background colour (for edit controls & list boxes), which matches the 'DarkMode_CFD' themed edit control fill colour.
	constexpr COLORREF s_ControlBackgroundColour = RGB( 0x38 /*red*/, 0x38 /*green*/, 0x38 /*blue*/ );

	// Returns the dialog control background brush.
	HBRUSH GetControlBackgroundBrush();

	// Dialog control border colour, which matches the 'DarkMode_CFD' themed edit control border colour.
	constexpr COLORREF s_ControlBorderColour = RGB( 0x9b /*red*/, 0x9b /*green*/, 0x9b /*blue*/ );

	// Dialog list view gridline colour (slightly lighter than the list background, in the same way that the light mode gridlines are slightly darker).
	constexpr COLORREF s_GridlineColour = RGB( 0x33 /*red*/, 0x33 /*green*/, 0x33 /*blue*/ );

	// Dialog link colour, which matches the dark mode checkbox colour.
	constexpr COLORREF s_LinkColour = RGB( 0x60 /*red*/, 0xcd /*green*/, 0xff /*blue*/ );

	// Tab control colours (the selected tab uses the background colour, so that it merges with the tab control display area).
	constexpr COLORREF s_TabColour = RGB( 0x2b /*red*/, 0x2b /*green*/, 0x2b /*blue*/ );

	// Subclass IDs.
	constexpr UINT_PTR s_DialogSubclassID = 1;
	constexpr UINT_PTR s_GroupBoxSubclassID = 1;
	constexpr UINT_PTR s_LinkSubclassID = 1;
	constexpr UINT_PTR s_TabSubclassID = 1;
	constexpr UINT_PTR s_ListViewSubclassID = 1;
	constexpr UINT_PTR s_EditSubclassID = 1;
	constexpr UINT_PTR s_ProgressBarSubclassID = 1;
	constexpr UINT_PTR s_StaticSubclassID = 1;

	// Dark mode tab control state.
	struct TabState {
		// The tab under the mouse cursor (or -1 if there is none).
		int HotItem = -1;

		// Indicates whether the mouse is being tracked (in order to receive a notification when the mouse leaves the control).
		bool IsTrackingMouse = false;
	};

	// Returns whether focus rectangles should be shown for the control 'hwnd'.
	bool ShowFocusRect( const HWND hwnd )
	{
		return ( GetFocus() == hwnd ) && !( UISF_HIDEFOCUS & SendMessage( hwnd, WM_QUERYUISTATE, 0, 0 ) );
	}

	// Fills the 'rect' on the 'dc' with the 'colour'.
	void FillSolidRect( const HDC dc, const RECT& rect, const COLORREF colour )
	{
		const COLORREF previousColour = SetBkColor( dc, colour );
		ExtTextOut( dc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr );
		SetBkColor( dc, previousColour );
	}

	// Paints the dark mode link control 'hwnd'.
	// Only the painting is handled here, the control itself still handles mouse & keyboard input.
	void PaintLink( const HWND hwnd )
	{
		PAINTSTRUCT ps = {};
		if ( const HDC dc = BeginPaint( hwnd, &ps ); nullptr != dc ) {
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );
			FillRect( dc, &clientRect, DarkMode::GetBackgroundBrush() );

			const HFONT font = reinterpret_cast<HFONT>( SendMessage( hwnd, WM_GETFONT, 0, 0 ) );
			HFONT linkFont = nullptr;
			if ( LOGFONT logFont = {}; ( nullptr != font ) && ( 0 != GetObject( font, sizeof( LOGFONT ), &logFont ) ) ) {
				logFont.lfUnderline = TRUE;
				linkFont = CreateFontIndirect( &logFont );
			}
			const HGDIOBJ previousFont = ( nullptr != font ) ? SelectObject( dc, font ) : nullptr;
			SetBkMode( dc, TRANSPARENT );

			// The window text contains the link markup (e.g. "(<a>Link</a>)"), so draw each section of text in turn.
			WCHAR buffer[ 512 ] = {};
			GetWindowText( hwnd, buffer, static_cast<int>( std::size( buffer ) ) );
			const std::wstring text = buffer;
			const bool enabled = IsWindowEnabled( hwnd );
			int x = clientRect.left;
			RECT focusRect = {};
			size_t position = 0;
			bool isLink = false;
			while ( position < text.size() ) {
				const size_t tagStart = text.find( L'<', position );
				const size_t sectionEnd = ( std::wstring::npos == tagStart ) ? text.size() : tagStart;
				if ( sectionEnd > position ) {
					const std::wstring section = text.substr( position, sectionEnd - position );
					SelectObject( dc, ( isLink && ( nullptr != linkFont ) ) ? linkFont : font );
					SetTextColor( dc, enabled ? ( isLink ? s_LinkColour : DarkMode::GetTextColour() ) : DarkMode::GetDisabledTextColour() );
					SIZE size = {};
					GetTextExtentPoint32( dc, section.c_str(), static_cast<int>( section.size() ), &size );
					TextOut( dc, x, clientRect.top, section.c_str(), static_cast<int>( section.size() ) );
					if ( isLink ) {
						focusRect = { x, clientRect.top, x + size.cx, clientRect.top + size.cy };
					}
					x += size.cx;
				}
				if ( std::wstring::npos == tagStart ) {
					break;
				}
				const size_t tagEnd = text.find( L'>', tagStart );
				if ( std::wstring::npos == tagEnd ) {
					break;
				}
				isLink = ( L'/' != text[ tagStart + 1 ] );
				position = tagEnd + 1;
			}

			if ( ShowFocusRect( hwnd ) && !IsRectEmpty( &focusRect ) ) {
				DrawFocusRect( dc, &focusRect );
			}

			if ( nullptr != previousFont ) {
				SelectObject( dc, previousFont );
			}
			if ( nullptr != linkFont ) {
				DeleteObject( linkFont );
			}
			EndPaint( hwnd, &ps );
		}
	}

	// Subclass procedure for dark mode link controls (whose link colour cannot be set, and is too dark for dark mode).
	LRESULT CALLBACK LinkSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_PAINT: {
				PaintLink( hwnd );
				return 0;
			}
			case WM_ERASEBKGND: {
				return TRUE;
			}
			case WM_SETFOCUS:
			case WM_KILLFOCUS:
			case WM_ENABLE:
			case WM_UPDATEUISTATE: {
				InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
				break;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, LinkSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Dark mode list view state, for list view controls in dialogs.
	struct ListViewState {
		// Dark mode scrollbar overlay.
		std::unique_ptr<WndListScrollbar> Scrollbar;

		// Indicates whether gridlines are drawn by the dark mode custom draw handling (as the gridline colour of the list itself cannot be set, and is too light).
		bool DrawGridlines = false;
	};

	// Returns the dark mode state for the dialog list view control 'hwnd', or nullptr if the control does not have any dark mode state.
	ListViewState* GetListViewState( const HWND hwnd );

	// Subclass procedure for dark mode list view controls in dialogs (the 'refData' is the list's dark mode state, which is owned by the subclass).
	LRESULT CALLBACK ListViewSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData )
	{
		ListViewState* state = reinterpret_cast<ListViewState*>( refData );
		WndListScrollbar* scrollbar = ( nullptr != state ) ? state->Scrollbar.get() : nullptr;
		switch ( message ) {
			case WM_NOTIFY: {
				// The header background is drawn dark by its theme, but the header text colour needs to be set explicitly.
				if ( LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam ); ( nullptr != hdr ) && ( NM_CUSTOMDRAW == hdr->code ) && ( ListView_GetHeader( hwnd ) == hdr->hwndFrom ) ) {
					LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>( lParam );
					if ( CDDS_PREPAINT == nmcd->dwDrawStage ) {
						return CDRF_NOTIFYITEMDRAW;
					} else if ( CDDS_ITEMPREPAINT == nmcd->dwDrawStage ) {
						SetTextColor( nmcd->hdc, DarkMode::GetTextColour() );
						return CDRF_DODEFAULT;
					}
				}
				break;
			}
			case WM_NCPAINT: {
				// Paint over the (light) window border.
				const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
				if ( ( WS_BORDER & GetWindowLongPtr( hwnd, GWL_STYLE ) ) || ( WS_EX_CLIENTEDGE & GetWindowLongPtr( hwnd, GWL_EXSTYLE ) ) ) {
					if ( const HDC dc = GetWindowDC( hwnd ); nullptr != dc ) {
						RECT windowRect = {};
						GetWindowRect( hwnd, &windowRect );
						OffsetRect( &windowRect, -windowRect.left, -windowRect.top );
						FrameRect( dc, &windowRect, DarkMode::GetEdgeBrush() );
						ReleaseDC( hwnd, dc );
					}
				}
				return result;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, ListViewSubclassProc, subclassID );
				delete state;
				return DefSubclassProc( hwnd, message, wParam, lParam );
			}
			default: {
				break;
			}
		}
		const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );

		// Any message might have scrolled the list, or changed its size or contents, so keep the dark mode scrollbars in sync.
		if ( ( nullptr != scrollbar ) && ( WM_DESTROY != message ) ) {
			scrollbar->Update();
		}
		return result;
	}

	ListViewState* GetListViewState( const HWND hwnd )
	{
		DWORD_PTR refData = 0;
		return GetWindowSubclass( hwnd, ListViewSubclassProc, s_ListViewSubclassID, &refData ) ? reinterpret_cast<ListViewState*>( refData ) : nullptr;
	}

	// Draws gridlines for the dark mode list view control 'hwnd' onto the 'dc', in the same positions as the list's own gridlines.
	void DrawListViewGridlines( const HWND hwnd, const HDC dc )
	{
		RECT clientRect = {};
		GetClientRect( hwnd, &clientRect );

		// Gridlines start below the header (if it is shown).
		int top = clientRect.top;
		const HWND header = ListView_GetHeader( hwnd );
		if ( RECT headerRect = {}; ( nullptr != header ) && IsWindowVisible( header ) && GetWindowRect( header, &headerRect ) ) {
			top += headerRect.bottom - headerRect.top;
		}

		// Vertical gridlines, at the right edge of each column.
		if ( const int columnCount = ( nullptr != header ) ? Header_GetItemCount( header ) : 0; columnCount > 0 ) {
			std::vector<int> columnOrder( columnCount );
			if ( ListView_GetColumnOrderArray( hwnd, columnCount, columnOrder.data() ) ) {
				int x = clientRect.left - GetScrollPos( hwnd, SB_HORZ );
				for ( const int column : columnOrder ) {
					x += ListView_GetColumnWidth( hwnd, column );
					FillSolidRect( dc, { x - 1, top, x, clientRect.bottom }, s_GridlineColour );
				}
			}
		}

		// Horizontal gridlines, at the bottom edge of each row (including the empty area below the last item).
		if ( ListView_GetItemCount( hwnd ) > 0 ) {
			if ( RECT itemRect = {}; ListView_GetItemRect( hwnd, ListView_GetTopIndex( hwnd ), &itemRect, LVIR_BOUNDS ) ) {
				if ( const int rowHeight = itemRect.bottom - itemRect.top; rowHeight > 0 ) {
					for ( int y = itemRect.bottom - 1; y < clientRect.bottom; y += rowHeight ) {
						FillSolidRect( dc, { clientRect.left, y, clientRect.right, y + 1 }, s_GridlineColour );
					}
				}
			}
		}
	}

	// Paints the border of the dark mode edit control 'hwnd', so that it matches the 'DarkMode_CFD' themed edit controls.
	void PaintEditBorder( const HWND hwnd )
	{
		RECT windowRect = {};
		GetWindowRect( hwnd, &windowRect );
		RECT clientRect = {};
		GetClientRect( hwnd, &clientRect );
		MapWindowPoints( hwnd, nullptr, reinterpret_cast<POINT*>( &clientRect ), 2 );
		OffsetRect( &clientRect, -windowRect.left, -windowRect.top );
		OffsetRect( &windowRect, -windowRect.left, -windowRect.top );

		// The border area excludes the client area & the scrollbars.
		if ( HRGN borderRegion = CreateRectRgnIndirect( &windowRect ); nullptr != borderRegion ) {
			RECT screenWindowRect = {};
			GetWindowRect( hwnd, &screenWindowRect );
			const auto exclude = [ borderRegion ] ( RECT rect ) {
				if ( HRGN region = CreateRectRgnIndirect( &rect ); nullptr != region ) {
					CombineRgn( borderRegion, borderRegion, region, RGN_DIFF );
					DeleteObject( region );
				}
			};
			exclude( clientRect );
			for ( const LONG objectID : { OBJID_VSCROLL, OBJID_HSCROLL } ) {
				SCROLLBARINFO scrollbarInfo = {};
				scrollbarInfo.cbSize = sizeof( SCROLLBARINFO );
				if ( GetScrollBarInfo( hwnd, objectID, &scrollbarInfo ) && !( STATE_SYSTEM_INVISIBLE & scrollbarInfo.rgstate[ 0 ] ) ) {
					RECT scrollbarRect = scrollbarInfo.rcScrollBar;
					OffsetRect( &scrollbarRect, -screenWindowRect.left, -screenWindowRect.top );
					exclude( scrollbarRect );
				}
			}
			if ( const HDC dc = GetWindowDC( hwnd ); nullptr != dc ) {
				FillRgn( dc, borderRegion, GetControlBackgroundBrush() );
				if ( const HBRUSH borderBrush = CreateSolidBrush( s_ControlBorderColour ); nullptr != borderBrush ) {
					FrameRect( dc, &windowRect, borderBrush );
					DeleteObject( borderBrush );
				}
				ReleaseDC( hwnd, dc );
			}
			DeleteObject( borderRegion );
		}
	}

	// Subclass procedure for dark mode edit controls with scrollbars.
	// These use the 'DarkMode_Explorer' theme, for dark scrollbars, but this theme has a light border, so the border is painted here.
	LRESULT CALLBACK EditSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_NCPAINT: {
				const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
				if ( ( WS_BORDER & GetWindowLongPtr( hwnd, GWL_STYLE ) ) || ( WS_EX_CLIENTEDGE & GetWindowLongPtr( hwnd, GWL_EXSTYLE ) ) ) {
					PaintEditBorder( hwnd );
				}
				return result;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, EditSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Handles custom draw notifications for dark mode list view controls in dialogs, so that the selected items use the accent colour (rather than the light theme selection colours).
	LRESULT OnListViewCustomDraw( LPNMLVCUSTOMDRAW nmlvcd )
	{
		const ListViewState* state = GetListViewState( nmlvcd->nmcd.hdr.hwndFrom );
		const bool drawGridlines = ( nullptr != state ) && state->DrawGridlines;
		switch ( nmlvcd->nmcd.dwDrawStage ) {
			case CDDS_PREPAINT: {
				return CDRF_NOTIFYITEMDRAW | ( drawGridlines ? CDRF_NOTIFYPOSTPAINT : 0 );
			}
			case CDDS_POSTPAINT: {
				if ( drawGridlines ) {
					DrawListViewGridlines( nmlvcd->nmcd.hdr.hwndFrom, nmlvcd->nmcd.hdc );
				}
				return CDRF_DODEFAULT;
			}
			case CDDS_ITEMPREPAINT: {
				const bool selected = ( LVIS_SELECTED == ListView_GetItemState( nmlvcd->nmcd.hdr.hwndFrom, static_cast<int>( nmlvcd->nmcd.dwItemSpec ), LVIS_SELECTED ) );
				if ( selected ) {
					nmlvcd->clrText = DarkMode::GetListBackgroundColour();
					nmlvcd->clrTextBk = DarkMode::GetAccentColour();
				}
				// Mask out the selection & focus states, so that the custom highlight colour takes effect, and a focus rectangle is not drawn.
				nmlvcd->nmcd.uItemState &= ~( CDIS_SELECTED | CDIS_FOCUS );
				return CDRF_DODEFAULT;
			}
			default: {
				return CDRF_DODEFAULT;
			}
		}
	}

	// Handles custom draw notifications for dark mode check boxes & radio buttons in dialogs.
	// The dark mode theme does not define a text colour for radio buttons, nor for check boxes on Windows 10 (only Windows 11 does), so these buttons are drawn here.
	// Returns the custom draw result, or nullopt if the default drawing should be used.
	std::optional<LRESULT> OnCheckBoxOrRadioButtonCustomDraw( LPNMCUSTOMDRAW nmcd )
	{
		const HWND button = nmcd->hdr.hwndFrom;
		const LONG_PTR style = GetWindowLongPtr( button, GWL_STYLE );
		const LONG_PTR buttonType = BS_TYPEMASK & style;
		const bool isRadioButton = ( BS_RADIOBUTTON == buttonType ) || ( BS_AUTORADIOBUTTON == buttonType );
		const bool isCheckBox = !( BS_PUSHLIKE & style ) && ( ( BS_CHECKBOX == buttonType ) || ( BS_AUTOCHECKBOX == buttonType ) || ( BS_3STATE == buttonType ) || ( BS_AUTO3STATE == buttonType ) );
		if ( ( !isRadioButton && !isCheckBox ) || ( CDDS_PREPAINT != nmcd->dwDrawStage ) ) {
			return std::nullopt;
		}
		const HTHEME theme = OpenThemeData( button, VSCLASS_BUTTON );
		if ( nullptr == theme ) {
			return std::nullopt;
		}

		const HDC dc = nmcd->hdc;
		const RECT& rect = nmcd->rc;
		FillRect( dc, &rect, DarkMode::GetBackgroundBrush() );

		// Glyph (the check box & radio button states share the same layout).
		const bool enabled = IsWindowEnabled( button );
		const LRESULT checkState = Button_GetCheck( button );
		int state = RBS_UNCHECKEDNORMAL;
		if ( !enabled ) {
			state = RBS_UNCHECKEDDISABLED;
		} else if ( CDIS_SELECTED & nmcd->uItemState ) {
			state = RBS_UNCHECKEDPRESSED;
		} else if ( CDIS_HOT & nmcd->uItemState ) {
			state = RBS_UNCHECKEDHOT;
		}
		if ( BST_CHECKED == checkState ) {
			state += ( RBS_CHECKEDNORMAL - RBS_UNCHECKEDNORMAL );
		} else if ( isCheckBox && ( BST_INDETERMINATE == checkState ) ) {
			state += ( CBS_MIXEDNORMAL - CBS_UNCHECKEDNORMAL );
		}
		const int part = isCheckBox ? BP_CHECKBOX : BP_RADIOBUTTON;
		SIZE glyphSize = {};
		GetThemePartSize( theme, dc, part, state, nullptr /*rect*/, TS_DRAW, &glyphSize );
		RECT glyphRect = { rect.left, rect.top + ( rect.bottom - rect.top - glyphSize.cy ) / 2, rect.left + glyphSize.cx, 0 };
		glyphRect.bottom = glyphRect.top + glyphSize.cy;
		DrawThemeBackground( theme, dc, part, state, &glyphRect, nullptr /*clipRect*/ );
		CloseThemeData( theme );

		// Label.
		WCHAR text[ 256 ] = {};
		if ( const int length = GetWindowText( button, text, static_cast<int>( std::size( text ) ) ); length > 0 ) {
			const HFONT font = reinterpret_cast<HFONT>( SendMessage( button, WM_GETFONT, 0, 0 ) );
			const HGDIOBJ previousFont = ( nullptr != font ) ? SelectObject( dc, font ) : nullptr;
			const LRESULT uiState = SendMessage( button, WM_QUERYUISTATE, 0, 0 );
			const UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | ( ( UISF_HIDEACCEL & uiState ) ? DT_HIDEPREFIX : 0 );
			RECT textRect = rect;
			textRect.left = glyphRect.right + static_cast<int>( 3 * GetDPIScaling() );
			SetBkMode( dc, TRANSPARENT );
			SetTextColor( dc, enabled ? DarkMode::GetTextColour() : DarkMode::GetDisabledTextColour() );
			DrawText( dc, text, length, &textRect, format );

			// Focus rectangle (around the label).
			if ( ( CDIS_FOCUS & nmcd->uItemState ) && !( UISF_HIDEFOCUS & uiState ) ) {
				RECT focusRect = textRect;
				DrawText( dc, text, length, &focusRect, format | DT_CALCRECT );
				focusRect.top = textRect.top + ( textRect.bottom - textRect.top - ( focusRect.bottom - focusRect.top ) ) / 2;
				focusRect.bottom = focusRect.top + ( focusRect.bottom - focusRect.top );
				InflateRect( &focusRect, 1, 1 );
				DrawFocusRect( dc, &focusRect );
			}

			if ( nullptr != previousFont ) {
				SelectObject( dc, previousFont );
			}
		}
		return CDRF_SKIPDEFAULT;
	}

	// Paints the progress bar 'hwnd'.
	// Progress bars are drawn here in both light & dark modes, so that they behave consistently (the moving highlight, which the themed progress bar draws over the fill, is not drawn in either mode).
	// There is no dark mode progress bar theme, so in dark mode the track is drawn in the same style as the dark mode trackbar channel. The fill always uses the themed fill.
	void PaintProgressBar( const HWND hwnd )
	{
		PAINTSTRUCT ps = {};
		if ( const HDC dc = BeginPaint( hwnd, &ps ); nullptr != dc ) {
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );
			const HTHEME theme = OpenThemeData( hwnd, VSCLASS_PROGRESS );

			// Track.
			RECT fillArea = clientRect;
			if ( s_Active || ( nullptr == theme ) ) {
				DarkMode::DrawTrackbarChannel( dc, clientRect );
				InflateRect( &fillArea, -1, -1 );
			} else {
				DrawThemeBackground( theme, dc, PP_BAR, 0, &clientRect, nullptr /*clipRect*/ );
				GetThemeBackgroundContentRect( theme, dc, PP_BAR, 0, &clientRect, &fillArea );
			}

			// Fill.
			PBRANGE range = {};
			SendMessage( hwnd, PBM_GETRANGE, FALSE, reinterpret_cast<LPARAM>( &range ) );
			const int position = static_cast<int>( SendMessage( hwnd, PBM_GETPOS, 0, 0 ) );
			if ( range.iHigh > range.iLow ) {
				const float proportion = std::clamp( static_cast<float>( position - range.iLow ) / ( range.iHigh - range.iLow ), 0.0f, 1.0f );
				RECT fillRect = fillArea;
				if ( PBS_VERTICAL & GetWindowLongPtr( hwnd, GWL_STYLE ) ) {
					fillRect.top = fillRect.bottom - static_cast<LONG>( proportion * ( fillRect.bottom - fillRect.top ) );
				} else {
					fillRect.right = fillRect.left + static_cast<LONG>( proportion * ( fillRect.right - fillRect.left ) );
				}
				if ( !IsRectEmpty( &fillRect ) ) {
					const bool drawn = ( nullptr != theme ) && SUCCEEDED( DrawThemeBackground( theme, dc, PP_FILL, PBFS_NORMAL, &fillRect, nullptr /*clipRect*/ ) );
					if ( !drawn ) {
						FillSolidRect( dc, fillRect, DarkMode::GetAccentColour() );
					}
				}
			}

			if ( nullptr != theme ) {
				CloseThemeData( theme );
			}
			EndPaint( hwnd, &ps );
		}
	}


	// Subclass procedure for progress bars (in both light & dark modes).
	LRESULT CALLBACK ProgressBarSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_PAINT: {
				// Marquee progress bars are left to draw themselves.
				if ( !( PBS_MARQUEE & GetWindowLongPtr( hwnd, GWL_STYLE ) ) ) {
					PaintProgressBar( hwnd );
					return 0;
				}
				break;
			}
			case WM_ERASEBKGND: {
				return TRUE;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, ProgressBarSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Applies the progress bar drawing to the 'control', if it is a progress bar (used in light mode, where the other dialog controls are left in their default state).
	BOOL CALLBACK ApplyToDialogProgressBar( HWND control, LPARAM /*param*/ )
	{
		WCHAR className[ 64 ] = {};
		GetClassName( control, className, static_cast<int>( std::size( className ) ) );
		if ( 0 == _wcsicmp( className, PROGRESS_CLASS ) ) {
			SetWindowSubclass( control, ProgressBarSubclassProc, s_ProgressBarSubclassID, 0 /*refData*/ );
		}
		return TRUE;
	}

	// Handles custom draw notifications for dark mode trackbar controls in dialogs.
	// The channel & thumb are drawn in the same style as the main window trackbars, but the thumb always uses the default dark mode colours.
	LRESULT OnTrackbarCustomDraw( LPNMCUSTOMDRAW nmcd )
	{
		const HWND trackbar = nmcd->hdr.hwndFrom;
		switch ( nmcd->dwDrawStage ) {
			case CDDS_PREPAINT: {
				return CDRF_NOTIFYITEMDRAW;
			}
			case CDDS_ITEMPREPAINT: {
				if ( TBCD_CHANNEL == nmcd->dwItemSpec ) {
					DarkMode::DrawTrackbarChannel( nmcd->hdc, nmcd->rc );
					return CDRF_SKIPDEFAULT;
				} else if ( TBCD_THUMB == nmcd->dwItemSpec ) {
					COLORREF colour = DarkMode::GetAccentColour();
					if ( !IsWindowEnabled( trackbar ) ) {
						colour = DarkMode::GetTrackbarDisabledThumbColour();
					} else if ( trackbar == GetCapture() ) {
						colour = DarkMode::GetTrackbarChannelColour();
					} else if ( POINT pt = {}; GetCursorPos( &pt ) && MapWindowPoints( nullptr, trackbar, &pt, 1 ) && PtInRect( &nmcd->rc, pt ) ) {
						colour = DarkMode::GetTrackbarHotThumbColour();
					}
					const RECT thumbRect = { nmcd->rc.left, nmcd->rc.top, nmcd->rc.right - 1, nmcd->rc.bottom - 1 };
					FillSolidRect( nmcd->hdc, thumbRect, colour );
					return CDRF_SKIPDEFAULT;
				}
				return CDRF_DODEFAULT;
			}
			default: {
				return CDRF_DODEFAULT;
			}
		}
	}

	// Paints the dark mode tab control 'hwnd', using the tab 'state'.
	void PaintTab( const HWND hwnd, const TabState& state )
	{
		PAINTSTRUCT ps = {};
		if ( const HDC dc = BeginPaint( hwnd, &ps ); nullptr != dc ) {
			const int offset = std::max( 1, static_cast<int>( 2 * GetDPIScaling() ) );
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );

			// Background (excluding the display area, which is covered by the tab pages).
			RECT displayRect = clientRect;
			TabCtrl_AdjustRect( hwnd, FALSE, &displayRect );
			const int savedDC = SaveDC( dc );
			ExcludeClipRect( dc, displayRect.left, displayRect.top, displayRect.right, displayRect.bottom );
			FillRect( dc, &clientRect, DarkMode::GetBackgroundBrush() );

			// Display area frame.
			RECT frameRect = displayRect;
			InflateRect( &frameRect, 1, 1 );
			FrameRect( dc, &frameRect, DarkMode::GetEdgeBrush() );

			const HFONT font = reinterpret_cast<HFONT>( SendMessage( hwnd, WM_GETFONT, 0, 0 ) );
			const HGDIOBJ previousFont = ( nullptr != font ) ? SelectObject( dc, font ) : nullptr;
			SetBkMode( dc, TRANSPARENT );
			SetTextColor( dc, DarkMode::GetTextColour() );

			const auto drawText = [ hwnd, dc ] ( const int item, RECT rect ) {
				WCHAR text[ 128 ] = {};
				TCITEM tabItem = {};
				tabItem.mask = TCIF_TEXT;
				tabItem.pszText = text;
				tabItem.cchTextMax = static_cast<int>( std::size( text ) );
				if ( TabCtrl_GetItem( hwnd, item, &tabItem ) ) {
					DrawText( dc, text, -1 /*count*/, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX );
				}
			};

			// Unselected tabs.
			const int selectedItem = TabCtrl_GetCurSel( hwnd );
			const int itemCount = TabCtrl_GetItemCount( hwnd );
			for ( int item = 0; item < itemCount; item++ ) {
				if ( RECT itemRect = {}; ( item != selectedItem ) && TabCtrl_GetItemRect( hwnd, item, &itemRect ) ) {
					FillSolidRect( dc, itemRect, ( item == state.HotItem ) ? DarkMode::GetHotColour() : s_TabColour );
					FrameRect( dc, &itemRect, DarkMode::GetEdgeBrush() );
					drawText( item, itemRect );
				}
			}

			// Selected tab, which is slightly larger, and merges with the display area.
			if ( RECT itemRect = {}; ( selectedItem >= 0 ) && TabCtrl_GetItemRect( hwnd, selectedItem, &itemRect ) ) {
				InflateRect( &itemRect, offset, 0 );
				itemRect.top -= offset;
				itemRect.bottom += 1;
				FillRect( dc, &itemRect, DarkMode::GetBackgroundBrush() );
				FillSolidRect( dc, { itemRect.left, itemRect.top, itemRect.right, itemRect.top + 1 }, DarkMode::GetEdgeColour() );
				FillSolidRect( dc, { itemRect.left, itemRect.top, itemRect.left + 1, itemRect.bottom }, DarkMode::GetEdgeColour() );
				FillSolidRect( dc, { itemRect.right - 1, itemRect.top, itemRect.right, itemRect.bottom }, DarkMode::GetEdgeColour() );
				itemRect.bottom -= 1;
				drawText( selectedItem, itemRect );
				if ( ShowFocusRect( hwnd ) ) {
					InflateRect( &itemRect, -offset, -offset );
					DrawFocusRect( dc, &itemRect );
				}
			}

			if ( nullptr != previousFont ) {
				SelectObject( dc, previousFont );
			}
			RestoreDC( dc, savedDC );
			EndPaint( hwnd, &ps );
		}
	}

	// Subclass procedure for dark mode tab controls (there is no dark mode tab control theme, so these are painted here).
	LRESULT CALLBACK TabSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData )
	{
		TabState* state = reinterpret_cast<TabState*>( refData );
		switch ( message ) {
			case WM_PAINT: {
				if ( nullptr != state ) {
					PaintTab( hwnd, *state );
					return 0;
				}
				break;
			}
			case WM_ERASEBKGND: {
				return TRUE;
			}
			case WM_MOUSEMOVE: {
				if ( nullptr != state ) {
					TCHITTESTINFO hitTest = {};
					hitTest.pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
					if ( const int hotItem = TabCtrl_HitTest( hwnd, &hitTest ); hotItem != state->HotItem ) {
						state->HotItem = hotItem;
						InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
					}
					if ( !state->IsTrackingMouse ) {
						TRACKMOUSEEVENT trackMouseEvent = {};
						trackMouseEvent.cbSize = sizeof( TRACKMOUSEEVENT );
						trackMouseEvent.dwFlags = TME_LEAVE;
						trackMouseEvent.hwndTrack = hwnd;
						state->IsTrackingMouse = TrackMouseEvent( &trackMouseEvent );
					}
				}
				break;
			}
			case WM_MOUSELEAVE: {
				if ( nullptr != state ) {
					state->IsTrackingMouse = false;
					if ( -1 != state->HotItem ) {
						state->HotItem = -1;
						InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
					}
				}
				break;
			}
			case WM_SETFOCUS:
			case WM_KILLFOCUS:
			case WM_UPDATEUISTATE: {
				InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
				break;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, TabSubclassProc, subclassID );
				delete state;
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Paints the dark mode group box 'hwnd'.
	void PaintGroupBox( const HWND hwnd )
	{
		PAINTSTRUCT ps = {};
		if ( const HDC dc = BeginPaint( hwnd, &ps ); nullptr != dc ) {
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );

			const HFONT font = reinterpret_cast<HFONT>( SendMessage( hwnd, WM_GETFONT, 0, 0 ) );
			const HGDIOBJ previousFont = ( nullptr != font ) ? SelectObject( dc, font ) : nullptr;
			TEXTMETRIC textMetric = {};
			GetTextMetrics( dc, &textMetric );

			// Clear the strip containing the text (which may be left over from previous, longer, text).
			const RECT textStripRect = { clientRect.left, clientRect.top, clientRect.right, clientRect.top + textMetric.tmHeight };
			FillRect( dc, &textStripRect, DarkMode::GetBackgroundBrush() );

			// Frame, which is vertically centred on the text.
			RECT frameRect = clientRect;
			frameRect.top += textMetric.tmHeight / 2;
			FrameRect( dc, &frameRect, DarkMode::GetEdgeBrush() );

			// Text, which is drawn over the top edge of the frame.
			WCHAR text[ 256 ] = {};
			if ( const int length = GetWindowText( hwnd, text, static_cast<int>( std::size( text ) ) ); length > 0 ) {
				const int margin = static_cast<int>( 6 * GetDPIScaling() );
				const int padding = static_cast<int>( 2 * GetDPIScaling() );
				RECT textRect = { 0, 0, 0, 0 };
				DrawText( dc, text, length, &textRect, DT_CALCRECT | DT_SINGLELINE );
				OffsetRect( &textRect, clientRect.left + margin + padding, clientRect.top );
				RECT backgroundRect = { textRect.left - padding, textRect.top, textRect.right + padding, textRect.bottom };
				FillRect( dc, &backgroundRect, DarkMode::GetBackgroundBrush() );
				SetBkMode( dc, TRANSPARENT );
				SetTextColor( dc, IsWindowEnabled( hwnd ) ? DarkMode::GetTextColour() : DarkMode::GetDisabledTextColour() );
				DrawText( dc, text, length, &textRect, DT_SINGLELINE | DT_HIDEPREFIX );
			}

			if ( nullptr != previousFont ) {
				SelectObject( dc, previousFont );
			}
			EndPaint( hwnd, &ps );
		}
	}

	// Paints the disabled dark mode static text control 'hwnd'.
	void PaintDisabledStatic( const HWND hwnd )
	{
		PAINTSTRUCT ps = {};
		if ( const HDC dc = BeginPaint( hwnd, &ps ); nullptr != dc ) {
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );
			const HBRUSH brush = reinterpret_cast<HBRUSH>( SendMessage( GetParent( hwnd ), WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>( dc ), reinterpret_cast<LPARAM>( hwnd ) ) );
			FillRect( dc, &clientRect, ( nullptr != brush ) ? brush : DarkMode::GetBackgroundBrush() );

			WCHAR text[ 256 ] = {};
			if ( const int length = GetWindowText( hwnd, text, static_cast<int>( std::size( text ) ) ); length > 0 ) {
				const LONG_PTR style = GetWindowLongPtr( hwnd, GWL_STYLE );
				const LONG_PTR type = SS_TYPEMASK & style;
				UINT format = DT_EXPANDTABS | ( ( SS_RIGHT == type ) ? DT_RIGHT : ( ( SS_CENTER == type ) ? DT_CENTER : DT_LEFT ) );
				if ( SS_CENTERIMAGE & style ) {
					format |= DT_SINGLELINE | DT_VCENTER;
				} else if ( SS_LEFTNOWORDWRAP != type ) {
					format |= DT_WORDBREAK;
				}
				if ( SS_NOPREFIX & style ) {
					format |= DT_NOPREFIX;
				}
				const HFONT font = reinterpret_cast<HFONT>( SendMessage( hwnd, WM_GETFONT, 0, 0 ) );
				const HGDIOBJ previousFont = ( nullptr != font ) ? SelectObject( dc, font ) : nullptr;
				SetBkMode( dc, TRANSPARENT );
				SetTextColor( dc, DarkMode::GetDisabledTextColour() );
				DrawText( dc, text, length, &clientRect, format );
				if ( nullptr != previousFont ) {
					SelectObject( dc, previousFont );
				}
			}
			EndPaint( hwnd, &ps );
		}
	}

	// Subclass procedure for dark mode static text controls.
	// When disabled, static controls draw embossed text (which is too bright on a dark background), so disabled text is painted here in the disabled text colour.
	LRESULT CALLBACK StaticSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_PAINT: {
				if ( !IsWindowEnabled( hwnd ) ) {
					PaintDisabledStatic( hwnd );
					return 0;
				}
				break;
			}
			case WM_ENABLE: {
				InvalidateRect( hwnd, nullptr /*rect*/, TRUE /*erase*/ );
				break;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, StaticSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Subclass procedure for dark mode group boxes (there is no dark mode group box theme, so these are painted here).
	LRESULT CALLBACK GroupBoxSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_PAINT: {
				PaintGroupBox( hwnd );
				return 0;
			}
			case WM_ENABLE: {
				InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
				break;
			}
			case WM_SETTEXT: {
				// The group box draws its new text immediately (rather than via WM_PAINT), so prevent that, and repaint it here instead.
				// (Note that re-enabling redrawing would also show a hidden window, so this only applies to visible group boxes).
				const bool visible = ( WS_VISIBLE & GetWindowLongPtr( hwnd, GWL_STYLE ) );
				if ( visible ) {
					SendMessage( hwnd, WM_SETREDRAW, FALSE, 0 );
				}
				const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
				if ( visible ) {
					SendMessage( hwnd, WM_SETREDRAW, TRUE, 0 );
					InvalidateRect( hwnd, nullptr /*rect*/, FALSE /*erase*/ );
				}
				return result;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, GroupBoxSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Subclass procedure for dark mode dialogs, which handles the control colour messages.
	LRESULT CALLBACK DialogSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
	{
		switch ( message ) {
			case WM_CTLCOLORDLG: {
				return reinterpret_cast<LRESULT>( DarkMode::GetBackgroundBrush() );
			}
			case WM_CTLCOLORSTATIC: {
				const HDC dc = reinterpret_cast<HDC>( wParam );
				const HWND control = reinterpret_cast<HWND>( lParam );
				SetTextColor( dc, IsWindowEnabled( control ) ? DarkMode::GetTextColour() : DarkMode::GetDisabledTextColour() );
				SetBkColor( dc, DarkMode::GetBackgroundColour() );
				return reinterpret_cast<LRESULT>( DarkMode::GetBackgroundBrush() );
			}
			case WM_CTLCOLOREDIT:
			case WM_CTLCOLORLISTBOX: {
				const HDC dc = reinterpret_cast<HDC>( wParam );
				SetTextColor( dc, DarkMode::GetTextColour() );
				SetBkColor( dc, s_ControlBackgroundColour );
				return reinterpret_cast<LRESULT>( GetControlBackgroundBrush() );
			}
			case WM_CTLCOLORBTN: {
				SetBkColor( reinterpret_cast<HDC>( wParam ), DarkMode::GetBackgroundColour() );
				return reinterpret_cast<LRESULT>( DarkMode::GetBackgroundBrush() );
			}
			case WM_NOTIFY: {
				if ( LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam ); ( nullptr != hdr ) && ( NM_CUSTOMDRAW == hdr->code ) ) {
					WCHAR className[ 64 ] = {};
					GetClassName( hdr->hwndFrom, className, static_cast<int>( std::size( className ) ) );
					if ( 0 == _wcsicmp( className, WC_LISTVIEW ) ) {
						return OnListViewCustomDraw( reinterpret_cast<LPNMLVCUSTOMDRAW>( lParam ) );
					} else if ( 0 == _wcsicmp( className, TRACKBAR_CLASS ) ) {
						return OnTrackbarCustomDraw( reinterpret_cast<LPNMCUSTOMDRAW>( lParam ) );
					} else if ( 0 == _wcsicmp( className, WC_BUTTON ) ) {
						if ( const auto result = OnCheckBoxOrRadioButtonCustomDraw( reinterpret_cast<LPNMCUSTOMDRAW>( lParam ) ); result.has_value() ) {
							return result.value();
						}
					}
				}
				break;
			}
			case WM_NCDESTROY: {
				RemoveWindowSubclass( hwnd, DialogSubclassProc, subclassID );
				break;
			}
			default: {
				break;
			}
		}
		return DefSubclassProc( hwnd, message, wParam, lParam );
	}

	// Applies the current dark mode state to the control 'hwnd', using the 'darkTheme' when dark mode is active, and restoring the 'lightTheme' when deactivated.
	void ApplyControlTheme( const HWND hwnd, LPCWSTR darkTheme, LPCWSTR lightTheme );

	// Returns the tooltip control which belongs to the 'control' (based on its window class), or nullptr if there is no tooltip control.
	HWND GetControlTooltip( const HWND control )
	{
		WCHAR className[ 64 ] = {};
		GetClassName( control, className, static_cast<int>( std::size( className ) ) );
		UINT message = 0;
		if ( 0 == _wcsicmp( className, TOOLBARCLASSNAME ) ) {
			message = TB_GETTOOLTIPS;
		} else if ( 0 == _wcsicmp( className, TRACKBAR_CLASS ) ) {
			message = TBM_GETTOOLTIPS;
		} else if ( 0 == _wcsicmp( className, WC_LISTVIEW ) ) {
			message = LVM_GETTOOLTIPS;
		} else if ( 0 == _wcsicmp( className, WC_TREEVIEW ) ) {
			message = TVM_GETTOOLTIPS;
		} else if ( 0 == _wcsicmp( className, WC_TABCONTROL ) ) {
			message = TCM_GETTOOLTIPS;
		}
		return ( 0 != message ) ? reinterpret_cast<HWND>( SendMessage( control, message, 0, 0 ) ) : nullptr;
	}

	// Applies the current dark mode state to the tooltip control of the 'control' (if it has one).
	BOOL CALLBACK ApplyToControlTooltip( HWND control, LPARAM /*param*/ )
	{
		if ( const HWND tooltip = GetControlTooltip( control ); nullptr != tooltip ) {
			ApplyControlTheme( tooltip, L"DarkMode_Explorer", nullptr /*lightTheme*/ );
		}
		return TRUE;
	}

	// Applies dark mode to the dialog 'control', based on its window class.
	BOOL CALLBACK ApplyToDialogControl( HWND control, LPARAM /*param*/ )
	{
		// Tooltips (dialogs are always created in the current mode, so the tooltips do not need to be returned to their light theme).
		if ( const HWND tooltip = GetControlTooltip( control ); nullptr != tooltip ) {
			s_AllowDarkModeForWindow( tooltip, true );
			SetWindowTheme( tooltip, L"DarkMode_Explorer", nullptr );
		}

		WCHAR className[ 64 ] = {};
		GetClassName( control, className, static_cast<int>( std::size( className ) ) );
		if ( 0 == _wcsicmp( className, WC_BUTTON ) ) {
			if ( BS_GROUPBOX == ( BS_TYPEMASK & GetWindowLongPtr( control, GWL_STYLE ) ) ) {
				SetWindowSubclass( control, GroupBoxSubclassProc, s_GroupBoxSubclassID, 0 /*refData*/ );
			} else {
				s_AllowDarkModeForWindow( control, true );
				SetWindowTheme( control, L"DarkMode_Explorer", nullptr );
			}
		} else if ( 0 == _wcsicmp( className, WC_EDIT ) ) {
			s_AllowDarkModeForWindow( control, true );
			if ( ( WS_VSCROLL | WS_HSCROLL ) & GetWindowLongPtr( control, GWL_STYLE ) ) {
				// The 'DarkMode_CFD' theme has light scrollbars, so use the 'DarkMode_Explorer' theme for edit controls with scrollbars (and paint the border separately).
				SetWindowTheme( control, L"DarkMode_Explorer", nullptr );
				SetWindowSubclass( control, EditSubclassProc, s_EditSubclassID, 0 /*refData*/ );
			} else {
				SetWindowTheme( control, L"DarkMode_CFD", nullptr );
			}
		} else if ( 0 == _wcsicmp( className, WC_COMBOBOX ) ) {
			s_AllowDarkModeForWindow( control, true );
			SetWindowTheme( control, L"DarkMode_CFD", nullptr );
			// The drop down list uses the 'DarkMode_Explorer' theme, for a dark scrollbar.
			if ( COMBOBOXINFO comboBoxInfo = { sizeof( COMBOBOXINFO ) }; GetComboBoxInfo( control, &comboBoxInfo ) ) {
				if ( nullptr != comboBoxInfo.hwndList ) {
					s_AllowDarkModeForWindow( comboBoxInfo.hwndList, true );
					SetWindowTheme( comboBoxInfo.hwndList, L"DarkMode_Explorer", nullptr );
				}
				// Changing the theme selects the text in the edit part of a drop down combo box, so clear the selection (unless the edit control has the focus).
				if ( ( nullptr != comboBoxInfo.hwndItem ) && ( comboBoxInfo.hwndItem != comboBoxInfo.hwndCombo ) && ( GetFocus() != comboBoxInfo.hwndItem ) ) {
					SendMessage( comboBoxInfo.hwndItem, EM_SETSEL, 0, 0 );
				}
			}
		} else if ( 0 == _wcsicmp( className, WC_LISTVIEW ) ) {
			if ( nullptr == GetListViewState( control ) ) {
				ListView_SetBkColor( control, DarkMode::GetListBackgroundColour() );
				ListView_SetTextBkColor( control, DarkMode::GetListBackgroundColour() );
				ListView_SetTextColor( control, DarkMode::GetListTextColour() );
				if ( const HWND header = ListView_GetHeader( control ); nullptr != header ) {
					s_AllowDarkModeForWindow( header, true );
					SetWindowTheme( header, L"ItemsView", nullptr );
				}
				// Note that the list state is owned by the subclass, which deletes it when the control is destroyed.
				ListViewState* state = new ListViewState();
				const HINSTANCE instance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( control, GWLP_HINSTANCE ) );
				state->Scrollbar = std::make_unique<WndListScrollbar>( instance, control );
				if ( LVS_EX_GRIDLINES & ListView_GetExtendedListViewStyle( control ) ) {
					// The list's own gridlines are too light for dark mode (and their colour cannot be set), so these are drawn by the dark mode custom draw handling instead.
					ListView_SetExtendedListViewStyleEx( control, LVS_EX_GRIDLINES, 0 );
					state->DrawGridlines = true;
				}
				SetWindowSubclass( control, ListViewSubclassProc, s_ListViewSubclassID, reinterpret_cast<DWORD_PTR>( state ) );
				state->Scrollbar->SetActive( true );
			}
		} else if ( 0 == _wcsicmp( className, WC_LINK ) ) {
			SetWindowSubclass( control, LinkSubclassProc, s_LinkSubclassID, 0 /*refData*/ );
		} else if ( 0 == _wcsicmp( className, PROGRESS_CLASS ) ) {
			SetWindowSubclass( control, ProgressBarSubclassProc, s_ProgressBarSubclassID, 0 /*refData*/ );
		} else if ( 0 == _wcsicmp( className, WC_STATIC ) ) {
			if ( const LONG_PTR type = SS_TYPEMASK & GetWindowLongPtr( control, GWL_STYLE ); ( SS_LEFT == type ) || ( SS_RIGHT == type ) || ( SS_CENTER == type ) || ( SS_LEFTNOWORDWRAP == type ) ) {
				SetWindowSubclass( control, StaticSubclassProc, s_StaticSubclassID, 0 /*refData*/ );
			}
		} else if ( 0 == _wcsicmp( className, WC_TABCONTROL ) ) {
			// Note that the tab state is owned by the subclass, which deletes it when the control is destroyed.
			if ( DWORD_PTR existingState = 0; !GetWindowSubclass( control, TabSubclassProc, s_TabSubclassID, &existingState ) ) {
				SetWindowSubclass( control, TabSubclassProc, s_TabSubclassID, reinterpret_cast<DWORD_PTR>( new TabState() ) );
			}
		}
		return TRUE;
	}

	// Applies the current dark mode state to the control 'hwnd', using the 'darkTheme' when dark mode is active, and restoring the 'lightTheme' when deactivated.
	// Until dark mode is first activated, the control is left in its existing state.
	void ApplyControlTheme( const HWND hwnd, LPCWSTR darkTheme, LPCWSTR lightTheme )
	{
		if ( !s_Supported || ( nullptr == hwnd ) ) {
			return;
		}
		const auto themedControl = s_ThemedControls.find( hwnd );
		if ( s_Active ) {
			s_AllowDarkModeForWindow( hwnd, true );
			SetWindowTheme( hwnd, darkTheme, nullptr );
			s_ThemedControls.insert( hwnd );
		} else if ( s_ThemedControls.end() != themedControl ) {
			// Return the control to its light theme.
			s_AllowDarkModeForWindow( hwnd, false );
			SetWindowTheme( hwnd, lightTheme, nullptr );
			s_ThemedControls.erase( themedControl );
		}
	}

	// Owns a solid brush for the lifetime of the application.
	class OwnedBrush
	{
	public:
		OwnedBrush( const COLORREF colour ) : m_Brush( CreateSolidBrush( colour ) ) {}
		~OwnedBrush() { if ( nullptr != m_Brush ) { DeleteObject( m_Brush ); } }
		OwnedBrush( const OwnedBrush& ) = delete;
		OwnedBrush& operator=( const OwnedBrush& ) = delete;
		HBRUSH Get() const { return m_Brush; }
	private:
		HBRUSH m_Brush;
	};

	HBRUSH GetControlBackgroundBrush()
	{
		static const OwnedBrush brush( s_ControlBackgroundColour );
		return brush.Get();
	}

	// Returns the Windows build number.
	DWORD GetBuildNumber()
	{
		DWORD buildNumber = 0;
		using fnRtlGetVersion = LONG ( WINAPI* )( PRTL_OSVERSIONINFOW );
		if ( const HMODULE ntdll = GetModuleHandle( L"ntdll.dll" ); nullptr != ntdll ) {
			if ( const auto rtlGetVersion = reinterpret_cast<fnRtlGetVersion>( GetProcAddress( ntdll, "RtlGetVersion" ) ); nullptr != rtlGetVersion ) {
				RTL_OSVERSIONINFOW versionInfo = {};
				versionInfo.dwOSVersionInfoSize = sizeof( versionInfo );
				if ( 0 == rtlGetVersion( &versionInfo ) ) {
					buildNumber = versionInfo.dwBuildNumber;
				}
			}
		}
		return buildNumber;
	}

	// Returns whether the Windows app colour scheme is set to dark.
	bool SystemUsesDarkMode()
	{
		DWORD value = 1;
		DWORD size = sizeof( value );
		const LSTATUS status = RegGetValue( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr /*type*/, &value, &size );
		return ( ERROR_SUCCESS == status ) && ( 0 == value );
	}

	// Applies the current dark mode state at the application level (this affects popup menus & other system drawn elements).
	// When dark mode is not active, the application is returned to the default state (i.e. as if dark mode support did not exist).
	void ApplyAppMode()
	{
		if ( !s_Supported || ( !s_Active && !s_AppModeModified ) ) {
			return;
		}
		if ( nullptr != s_SetPreferredAppMode ) {
			PreferredAppMode appMode = PreferredAppMode::Default;
			if ( s_Active ) {
				appMode = ( DarkMode::Mode::Dark == s_Mode ) ? PreferredAppMode::ForceDark : PreferredAppMode::AllowDark;
			}
			s_SetPreferredAppMode( appMode );
		} else if ( nullptr != s_AllowDarkModeForApp ) {
			s_AllowDarkModeForApp( s_Active );
		}
		s_RefreshImmersiveColorPolicyState();
		s_FlushMenuThemes();
		s_AppModeModified = s_Active;
	}

	// Returns the menu theme handle for the window 'hwnd'.
	HTHEME GetMenuTheme( const HWND hwnd )
	{
		if ( nullptr == s_MenuTheme ) {
			s_MenuTheme = OpenThemeData( hwnd, VSCLASS_MENU );
		}
		return s_MenuTheme;
	}

	// Closes the menu theme handle.
	void CloseMenuTheme()
	{
		if ( nullptr != s_MenuTheme ) {
			CloseThemeData( s_MenuTheme );
			s_MenuTheme = nullptr;
		}
	}

	// Draws the menu bar background for the window 'hwnd'.
	void DrawMenuBarBackground( const HWND hwnd, const UAHMENU& menu )
	{
		MENUBARINFO menuBarInfo = {};
		menuBarInfo.cbSize = sizeof( menuBarInfo );
		if ( GetMenuBarInfo( hwnd, OBJID_MENU, 0 /*item*/, &menuBarInfo ) ) {
			RECT windowRect = {};
			GetWindowRect( hwnd, &windowRect );
			RECT menuBarRect = menuBarInfo.rcBar;
			OffsetRect( &menuBarRect, -windowRect.left, -windowRect.top );
			menuBarRect.top -= 1;
			FillRect( menu.hdc, &menuBarRect, DarkMode::GetBackgroundBrush() );
		}
	}

	// Draws a menu bar item for the window 'hwnd'.
	void DrawMenuBarItem( const HWND hwnd, const UAHDRAWMENUITEM& item )
	{
		WCHAR text[ 256 ] = {};
		MENUITEMINFO menuItemInfo = {};
		menuItemInfo.cbSize = sizeof( menuItemInfo );
		menuItemInfo.fMask = MIIM_STRING;
		menuItemInfo.dwTypeData = text;
		menuItemInfo.cch = static_cast<UINT>( std::size( text ) - 1 );
		GetMenuItemInfo( item.um.hmenu, item.umi.iPosition, TRUE /*byPosition*/, &menuItemInfo );

		const UINT state = item.dis.itemState;
		HBRUSH backgroundBrush = DarkMode::GetBackgroundBrush();
		COLORREF textColour = DarkMode::GetTextColour();
		int themeState = MBI_NORMAL;
		if ( ODS_SELECTED & state ) {
			backgroundBrush = DarkMode::GetPressedBrush();
			themeState = MBI_PUSHED;
		} else if ( ODS_HOTLIGHT & state ) {
			backgroundBrush = DarkMode::GetHotBrush();
			themeState = MBI_HOT;
		}
		if ( ( ODS_GRAYED | ODS_DISABLED ) & state ) {
			textColour = DarkMode::GetDisabledTextColour();
			themeState = MBI_DISABLED;
		}

		DWORD textFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
		if ( ODS_NOACCEL & state ) {
			textFlags |= DT_HIDEPREFIX;
		}

		RECT itemRect = item.dis.rcItem;
		FillRect( item.um.hdc, &itemRect, backgroundBrush );

		if ( const HTHEME theme = GetMenuTheme( hwnd ); nullptr != theme ) {
			DTTOPTS options = {};
			options.dwSize = sizeof( options );
			options.dwFlags = DTT_TEXTCOLOR;
			options.crText = textColour;
			DrawThemeTextEx( theme, item.um.hdc, MENU_BARITEM, themeState, text, -1 /*length*/, textFlags, &itemRect, &options );
		} else {
			SetBkMode( item.um.hdc, TRANSPARENT );
			SetTextColor( item.um.hdc, textColour );
			DrawText( item.um.hdc, text, -1 /*length*/, &itemRect, textFlags );
		}
	}

	// Paints over the light line which Windows draws between the menu bar & the client area of the window 'hwnd'.
	void DrawMenuBarBottomLine( const HWND hwnd )
	{
		MENUBARINFO menuBarInfo = {};
		menuBarInfo.cbSize = sizeof( menuBarInfo );
		if ( !IsIconic( hwnd ) && GetMenuBarInfo( hwnd, OBJID_MENU, 0 /*item*/, &menuBarInfo ) ) {
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );
			MapWindowPoints( hwnd, nullptr, reinterpret_cast<POINT*>( &clientRect ), 2 );
			RECT windowRect = {};
			GetWindowRect( hwnd, &windowRect );
			OffsetRect( &clientRect, -windowRect.left, -windowRect.top );
			RECT lineRect = clientRect;
			lineRect.bottom = lineRect.top;
			lineRect.top -= 1;
			if ( const HDC dc = GetWindowDC( hwnd ); nullptr != dc ) {
				FillRect( dc, &lineRect, DarkMode::GetBackgroundBrush() );
				ReleaseDC( hwnd, dc );
			}
		}
	}
}

void DarkMode::Initialise( const Mode mode )
{
	s_Mode = mode;
	const DWORD buildNumber = GetBuildNumber();
	if ( buildNumber >= s_Build1809 ) {
		HMODULE uxtheme = GetModuleHandle( L"uxtheme.dll" );
		if ( nullptr == uxtheme ) {
			uxtheme = LoadLibraryEx( L"uxtheme.dll", nullptr /*file*/, LOAD_LIBRARY_SEARCH_SYSTEM32 );
		}
		if ( nullptr != uxtheme ) {
			s_RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>( GetProcAddress( uxtheme, MAKEINTRESOURCEA( 104 ) ) );
			s_AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>( GetProcAddress( uxtheme, MAKEINTRESOURCEA( 133 ) ) );
			s_FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>( GetProcAddress( uxtheme, MAKEINTRESOURCEA( 136 ) ) );
			const FARPROC ordinal135 = GetProcAddress( uxtheme, MAKEINTRESOURCEA( 135 ) );
			if ( buildNumber < s_Build1903 ) {
				s_AllowDarkModeForApp = reinterpret_cast<fnAllowDarkModeForApp>( ordinal135 );
			} else {
				s_SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>( ordinal135 );
			}
			s_Supported = ( nullptr != s_RefreshImmersiveColorPolicyState ) && ( nullptr != s_AllowDarkModeForWindow ) && ( nullptr != s_FlushMenuThemes ) && ( nullptr != ordinal135 );
		}
	}
	Refresh();
}

bool DarkMode::IsSupported()
{
	return s_Supported;
}

bool DarkMode::IsActive()
{
	return s_Active;
}

bool DarkMode::UseDarkColours()
{
	if ( s_Active ) {
		return true;
	}
	if ( IsHighContrastActive() ) {
		// Treat the high contrast theme as dark if its window background colour is dark.
		const COLORREF background = GetSysColor( COLOR_WINDOW );
		const int luminance = ( 299 * GetRValue( background ) + 587 * GetGValue( background ) + 114 * GetBValue( background ) ) / 1000;
		return luminance < 128;
	}
	return false;
}

bool DarkMode::SetMode( const Mode mode )
{
	s_Mode = mode;
	return Refresh();
}

bool DarkMode::Refresh()
{
	const bool wasActive = s_Active;
	bool active = false;
	if ( s_Supported && !IsHighContrastActive() ) {
		switch ( s_Mode ) {
			case Mode::System: {
				active = SystemUsesDarkMode();
				break;
			}
			case Mode::Light: {
				active = false;
				break;
			}
			case Mode::Dark: {
				// Forcing dark mode is only possible on Windows 10 1903 and later.
				active = ( nullptr != s_SetPreferredAppMode ) ? true : SystemUsesDarkMode();
				break;
			}
		}
	}
	s_Active = active;
	ApplyAppMode();
	return wasActive != s_Active;
}

void DarkMode::ApplyToMainWindow( const HWND hwnd )
{
	// Leave the main window in its default state, unless dark mode is (or has been) active.
	if ( !s_Supported || ( nullptr == hwnd ) || ( !s_Active && !s_MainWindowModified ) ) {
		return;
	}
	s_MainWindowModified = s_Active;

	ApplyToTitleBar( hwnd, s_Active );

	// Force the frame to be redrawn (Windows 10 doesn't always repaint the title bar until the activation state changes).
	CloseMenuTheme();
	SetWindowPos( hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	if ( IsWindowVisible( hwnd ) && ( GetActiveWindow() == hwnd ) ) {
		SendMessage( hwnd, WM_NCACTIVATE, FALSE, 0 );
		SendMessage( hwnd, WM_NCACTIVATE, TRUE, 0 );
	}
	DrawMenuBar( hwnd );
}

void DarkMode::ApplyToControl( const HWND hwnd, LPCWSTR lightTheme )
{
	ApplyControlTheme( hwnd, L"DarkMode_Explorer", lightTheme );
}

void DarkMode::ApplyToHeader( const HWND hwnd )
{
	ApplyControlTheme( hwnd, L"ItemsView", nullptr /*lightTheme*/ );
}

void DarkMode::ApplyToEdit( const HWND hwnd )
{
	ApplyControlTheme( hwnd, L"DarkMode_CFD", nullptr /*lightTheme*/ );
}

COLORREF DarkMode::GetEditBackgroundColour()
{
	return s_ControlBackgroundColour;
}

HBRUSH DarkMode::GetEditBackgroundBrush()
{
	return GetControlBackgroundBrush();
}

void DarkMode::ApplyToChildTooltips( const HWND hwnd )
{
	if ( nullptr != hwnd ) {
		EnumChildWindows( hwnd, ApplyToControlTooltip, 0 /*param*/ );
	}
}

void DarkMode::ApplyToDialog( const HWND hwnd )
{
	if ( nullptr == hwnd ) {
		return;
	}

	// Dialogs are left in their default state when dark mode is not active,
	// apart from progress bars, which are drawn without the moving highlight (so that they behave consistently in both modes).
	// This is only done when visual styles are in use (i.e. not for high contrast or the classic theme).
	if ( !s_Active ) {
		if ( IsAppThemed() && !IsHighContrastActive() ) {
			EnumChildWindows( hwnd, ApplyToDialogProgressBar, 0 /*param*/ );
		}
		return;
	}
	ApplyToTitleBar( hwnd, true );
	// Property sheet pages draw their background using the (light) tab control theme, so disable this.
	EnableThemeDialogTexture( hwnd, ETDT_DISABLE );
	SetWindowSubclass( hwnd, DialogSubclassProc, s_DialogSubclassID, 0 /*refData*/ );
	EnumChildWindows( hwnd, ApplyToDialogControl, 0 /*param*/ );
}

void DarkMode::DrawScrollbarCorner( const HDC dc, const RECT& rect )
{
	// Use the scrollbar track from the same theme as the dark mode scrollbars, so that the corner matches.
	bool drawn = false;
	if ( const HTHEME theme = OpenThemeData( nullptr, L"DarkMode_Explorer::ScrollBar" ); nullptr != theme ) {
		drawn = SUCCEEDED( DrawThemeBackground( theme, dc, SBP_LOWERTRACKVERT, SCRBS_NORMAL, &rect, nullptr /*clipRect*/ ) );
		CloseThemeData( theme );
	}
	if ( !drawn ) {
		FillRect( dc, &rect, GetBackgroundBrush() );
	}
}

void DarkMode::DrawScrollbarCorner( const HWND hwnd )
{
	SCROLLBARINFO verticalInfo = {};
	verticalInfo.cbSize = sizeof( SCROLLBARINFO );
	SCROLLBARINFO horizontalInfo = {};
	horizontalInfo.cbSize = sizeof( SCROLLBARINFO );
	const auto isVisible = [] ( const SCROLLBARINFO& info ) {
		return !( ( STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN ) & info.rgstate[ 0 ] ) && !IsRectEmpty( &info.rcScrollBar );
	};
	if ( GetScrollBarInfo( hwnd, OBJID_VSCROLL, &verticalInfo ) && GetScrollBarInfo( hwnd, OBJID_HSCROLL, &horizontalInfo ) && isVisible( verticalInfo ) && isVisible( horizontalInfo ) ) {
		RECT windowRect = {};
		GetWindowRect( hwnd, &windowRect );
		RECT cornerRect = { verticalInfo.rcScrollBar.left, horizontalInfo.rcScrollBar.top, verticalInfo.rcScrollBar.right, horizontalInfo.rcScrollBar.bottom };
		OffsetRect( &cornerRect, -windowRect.left, -windowRect.top );
		if ( const HDC dc = GetWindowDC( hwnd ); nullptr != dc ) {
			DrawScrollbarCorner( dc, cornerRect );
			ReleaseDC( hwnd, dc );
		}
	}
}

bool DarkMode::HandleMainWindowMessage( const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam, LRESULT& result )
{
	switch ( message ) {
		case WM_UAHDRAWMENU: {
			if ( s_Active && ( 0 != lParam ) ) {
				DrawMenuBarBackground( hwnd, *reinterpret_cast<const UAHMENU*>( lParam ) );
				result = 0;
				return true;
			}
			break;
		}
		case WM_UAHDRAWMENUITEM: {
			if ( s_Active && ( 0 != lParam ) ) {
				DrawMenuBarItem( hwnd, *reinterpret_cast<const UAHDRAWMENUITEM*>( lParam ) );
				result = 0;
				return true;
			}
			break;
		}
		case WM_NCPAINT:
		case WM_NCACTIVATE: {
			if ( s_Active ) {
				result = DefWindowProc( hwnd, message, wParam, lParam );
				DrawMenuBarBottomLine( hwnd );
				return true;
			}
			break;
		}
		case WM_THEMECHANGED: {
			CloseMenuTheme();
			break;
		}
		default: {
			break;
		}
	}
	return false;
}

bool DarkMode::IsColourSchemeChange( const LPARAM lParam )
{
	const LPCWSTR area = reinterpret_cast<LPCWSTR>( lParam );
	return ( nullptr != area ) && ( CSTR_EQUAL == CompareStringOrdinal( area, -1, L"ImmersiveColorSet", -1, TRUE /*ignoreCase*/ ) );
}

COLORREF DarkMode::GetBackgroundColour()
{
	return RGB( 0x20, 0x20, 0x20 );
}

COLORREF DarkMode::GetHotColour()
{
	return RGB( 0x3d, 0x3d, 0x3d );
}

COLORREF DarkMode::GetPressedColour()
{
	return RGB( 0x4a, 0x4a, 0x4a );
}

COLORREF DarkMode::GetTextColour()
{
	return RGB( 0xf0, 0xf0, 0xf0 );
}

COLORREF DarkMode::GetDisabledTextColour()
{
	return RGB( 0x6e, 0x6e, 0x6e );
}

COLORREF DarkMode::GetEdgeColour()
{
	return RGB( 0x3c, 0x3c, 0x3c );
}

COLORREF DarkMode::GetGripColour()
{
	return RGB( 0x8c, 0x8c, 0x8c );
}

COLORREF DarkMode::GetListTextColour()
{
	return RGB( 0xff, 0xff, 0xff );
}

COLORREF DarkMode::GetListBackgroundColour()
{
	return RGB( 0x27, 0x27, 0x27 );
}

COLORREF DarkMode::GetTrackbarChannelColour()
{
	return RGB( 0x38, 0x38, 0x38 );
}

COLORREF DarkMode::GetTrackbarChannelBorderColour()
{
	return RGB( 0x48, 0x48, 0x48 );
}

COLORREF DarkMode::GetTrackbarHotThumbColour()
{
	return RGB( 0xff, 0xff, 0xff );
}

COLORREF DarkMode::GetTrackbarDisabledThumbColour()
{
	return RGB( 0x48, 0x48, 0x48 );
}

void DarkMode::DrawTrackbarChannel( const HDC dc, const RECT& rect )
{
	// The channel has a border, in the same style as the light themed channel.
	FillSolidRect( dc, rect, GetTrackbarChannelColour() );
	if ( const HBRUSH brush = CreateSolidBrush( GetTrackbarChannelBorderColour() ); nullptr != brush ) {
		FrameRect( dc, &rect, brush );
		DeleteObject( brush );
	}
}

COLORREF DarkMode::GetAccentColour()
{
	// This is a lighter shade of the default blue (between the system highlight colour & the lighter accent shade which Windows uses in dark mode),
	// so that it stands out on dark backgrounds, while still giving good contrast for dark text on the highlight.
	return RGB( 0x39, 0xb0, 0xf5 );
}

HBRUSH DarkMode::GetBackgroundBrush()
{
	static const OwnedBrush brush( GetBackgroundColour() );
	return brush.Get();
}

HBRUSH DarkMode::GetHotBrush()
{
	static const OwnedBrush brush( GetHotColour() );
	return brush.Get();
}

HBRUSH DarkMode::GetPressedBrush()
{
	static const OwnedBrush brush( GetPressedColour() );
	return brush.Get();
}

HBRUSH DarkMode::GetEdgeBrush()
{
	static const OwnedBrush brush( GetEdgeColour() );
	return brush.Get();
}

HBRUSH DarkMode::GetWindowBackgroundBrush()
{
	return s_Active ? GetBackgroundBrush() : reinterpret_cast<HBRUSH>( COLOR_3DFACE + 1 );
}

HBRUSH DarkMode::GetGripBrush()
{
	static const OwnedBrush brush( GetGripColour() );
	return brush.Get();
}
