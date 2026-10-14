#include "Utility.h"

#include "resource.h"

#include "Gdiplusimaging.h"
#include "windowsx.h"

#include <initguid.h> 
#include <Uxtheme.h>
#include <VersionHelpers.h>
#include "objbase.h"
#include "uiautomation.h"
#include <shellapi.h>
#include <wrl/client.h>

#ifndef _CONSOLE
#include "webp/decode.h"
#include "webp/demux.h"
#endif

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

// Maximum size when converting images.
static const int sMaxConvertImageBytes = 0x1000000;

// Maximum size when loading image files.
static const uintmax_t sMaxImageFileBytes = 0x10000000;

// Random number engine.
static std::default_random_engine sRandomEngine{ std::random_device {} ( ) };

std::wstring UTF8ToWideString( const std::string& text )
{
	std::wstring result;
	if ( !text.empty() ) {
		const int bufferSize = MultiByteToWideChar( CP_UTF8, 0, text.c_str(), -1 /*strLen*/, nullptr /*buffer*/, 0 /*bufferSize*/ );
		if ( bufferSize > 0 ) {
			std::vector<WCHAR> buffer( bufferSize );
			if ( 0 != MultiByteToWideChar( CP_UTF8, 0, text.c_str(), -1 /*strLen*/, buffer.data(), bufferSize ) ) {
				result = buffer.data();
			}
		}
	}
	return result;
}

std::string WideStringToUTF8( const std::wstring& text )
{
	std::string result;
	if ( !text.empty() ) {
		const int bufferSize = WideCharToMultiByte( CP_UTF8, 0, text.c_str(), -1 /*strLen*/, nullptr /*buffer*/, 0 /*bufferSize*/, NULL /*defaultChar*/, NULL /*usedDefaultChar*/ );
		if ( bufferSize > 0 ) {
			std::vector<char> buffer( bufferSize );
			if ( 0 != WideCharToMultiByte( CP_UTF8, 0, text.c_str(), -1 /*strLen*/, buffer.data(), bufferSize, NULL /*defaultChar*/, NULL /*usedDefaultChar*/ ) ) {
				result = buffer.data();
			}
		}
	}
	return result;
}

std::wstring AnsiCodePageToWideString( const std::string& text )
{
	std::wstring result;
	if ( !text.empty() ) {
		const int bufferSize = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, text.c_str(), -1 /*strLen*/, nullptr /*buffer*/, 0 /*bufferSize*/ );
		if ( bufferSize > 0 ) {
			std::vector<WCHAR> buffer( bufferSize );
			if ( 0 != MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, text.c_str(), -1 /*strLen*/, buffer.data(), bufferSize ) ) {
				result = buffer.data();
			}
		}
	}
	return result;
}

std::string WideStringToAnsiCodePage( const std::wstring& text )
{
	std::string result;
	if ( !text.empty() ) {
		const int bufferSize = WideCharToMultiByte( CP_ACP, 0 /*flags*/, text.c_str(), -1 /*strLen*/, nullptr /*buffer*/, 0 /*bufferSize*/, NULL /*defaultChar*/, NULL /*usedDefaultChar*/ );
		if ( bufferSize > 0 ) {
			std::vector<char> buffer( bufferSize );
			if ( 0 != WideCharToMultiByte( CP_ACP, 0 /*flags*/, text.c_str(), -1 /*strLen*/, buffer.data(), bufferSize, NULL /*defaultChar*/, NULL /*usedDefaultChar*/ ) ) {
				result = buffer.data();
			}
		}
	}
	return result;
}

std::wstring CodePageToWideString( const std::string& text, const UINT codePage )
{
	std::wstring result;
	if ( !text.empty() ) {
		const int bufferSize = MultiByteToWideChar( codePage, MB_PRECOMPOSED, text.c_str(), -1 /*strLen*/, nullptr /*buffer*/, 0 /*bufferSize*/ );
		if ( bufferSize > 0 ) {
			std::vector<WCHAR> buffer( bufferSize );
			if ( 0 != MultiByteToWideChar( codePage, MB_PRECOMPOSED, text.c_str(), -1 /*strLen*/, buffer.data(), bufferSize ) ) {
				result = buffer.data();
			}
		}
	}
	return result;
}

std::string CodePageToUTF8( const std::string& text, const UINT codePage )
{
	return WideStringToUTF8( CodePageToWideString( text, codePage ) );
}

std::wstring WideStringToLower( const std::wstring& text )
{
	std::wstring result;
	std::locale locale;
	for ( const auto& iter : text ) {
		result += std::tolower( iter, locale );
	}
	return result;
}

std::wstring WideStringToUpper( const std::wstring& text )
{
	std::wstring result;
	std::locale locale;
	for ( const auto& iter : text ) {
		result += std::toupper( iter, locale );
	}
	return result;
}

std::string StringToLower( const std::string& text )
{
	std::string result;
	std::locale locale;
	for ( const auto& iter : text ) {
		result += std::tolower( iter, locale );
	}
	return result;
}

std::string StringToUpper( const std::string& text )
{
	std::string result;
	std::locale locale;
	for ( const auto& iter : text ) {
		result += std::toupper( iter, locale );
	}
	return result;
}

std::string Base64Encode( const BYTE* bytes, const int byteCount )
{
	std::string result;
	if ( ( nullptr != bytes ) && ( byteCount > 0 ) ) {
		DWORD bufferSize = 0;
		if ( FALSE != CryptBinaryToStringA( bytes, static_cast<DWORD>( byteCount ), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr /*buffer*/, &bufferSize ) ) {
			if ( bufferSize > 0 ) {
				std::vector<char> buffer( bufferSize );
				if ( FALSE != CryptBinaryToStringA( bytes, static_cast<DWORD>( byteCount ), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, buffer.data(), &bufferSize ) ) {
					result = buffer.data();
				}
			}
		}
	}
	return result;
}

std::vector<BYTE> Base64Decode( const std::string& text )
{
	std::vector<BYTE> result;
	if ( !text.empty() ) {
		DWORD bufferSize = 0;
		if ( FALSE != CryptStringToBinaryA( text.c_str(), static_cast<DWORD>( text.size() ), CRYPT_STRING_BASE64, nullptr /*buffer*/, &bufferSize, nullptr /*skip*/, nullptr /*flags*/ ) ) {
			if ( bufferSize > 0 ) {
				result.resize( bufferSize );
				if ( FALSE == CryptStringToBinaryA( text.c_str(), static_cast<DWORD>( text.size() ), CRYPT_STRING_BASE64, result.data(), &bufferSize, nullptr /*skip*/, nullptr /*flags*/ ) ) {
					result.clear();
				}
			}
		}
	}
	return result;
}

#ifndef _CONSOLE

std::wstring FilesizeToString( const HINSTANCE instance, const long long filesize )
{
	std::wstringstream ss;
	if ( filesize > 0x40000000 ) {
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		if ( 0 != LoadString( instance, IDS_UNITS_GB, buf, bufSize ) ) {
			const float size = static_cast<float>( filesize ) / 0x40000000;
			ss << std::fixed << std::setprecision( 2 ) << size << L" " << buf;
		}
	} else if ( filesize > 0x100000 ) {
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		if ( 0 != LoadString( instance, IDS_UNITS_MB, buf, bufSize ) ) {
			const float size = static_cast<float>( filesize ) / 0x100000;
			ss << std::fixed << std::setprecision( 2 ) << size << L" " << buf;
		}
	} else if ( filesize > 0 ) {
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		if ( 0 != LoadString( instance, IDS_UNITS_KB, buf, bufSize ) ) {
			const float size = static_cast<float>( filesize ) / 0x400;
			ss << std::fixed << std::setprecision( 2 ) << size << L" " << buf;
		}
	}
	const std::wstring str = ss.str();
	return str;
}

std::wstring DurationToString( const HINSTANCE instance, const double _duration, const bool colonDelimited )
{
	std::wstringstream ss;
	const long duration = static_cast<long>( _duration );
	const long days = duration / 86400;
	if ( days > 0 ) {
		ss << days;
		if ( colonDelimited ) {
			ss << L":";
		} else {
			const int bufSize = 16;
			WCHAR buf[ bufSize ] = {};
			LoadString( instance, IDS_UNITS_DAYS, buf, bufSize );
			ss << buf << L" ";
		}
	}

	const long hours = ( duration % 86400 ) / 3600;
	if ( ( hours > 0 ) || ( days > 0 ) ) {
		if ( days > 0 ) {
			ss << std::setfill( static_cast<wchar_t>( '0' ) ) << std::setw( 2 );
		}
		ss << hours;
		if ( colonDelimited ) {
			ss << L":";
		} else {
			const int bufSize = 16;
			WCHAR buf[ bufSize ] = {};
			LoadString( instance, IDS_UNITS_HOURS, buf, bufSize );
			ss << buf << L" ";
		}
	}

	const long minutes = ( duration % 3600 ) / 60;
	if ( ( hours > 0 ) || ( days > 0 ) ) {
		ss << std::setfill( static_cast<wchar_t>( '0' ) ) << std::setw( 2 );
	}
	ss << minutes;
	if ( colonDelimited ) {
		ss << L":";
	} else {
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		LoadString( instance, IDS_UNITS_MINUTES, buf, bufSize );
		ss << buf << L" ";
	}

	const long seconds = duration % 60;
	ss << std::setfill( static_cast<wchar_t>( '0' ) ) << std::setw( 2 ) << seconds;
	if ( !colonDelimited ) {
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		LoadString( instance, IDS_UNITS_SECONDS, buf, bufSize );
		ss << buf;
	}
	const std::wstring str = ss.str();
	return str;
}

std::wstring BitrateToString( const HINSTANCE instance, const float bitrate )
{
	std::wstringstream ss;
	const int bufSize = 16;
	WCHAR buf[ bufSize ] = {};
	if ( 0 != LoadString( instance, IDS_UNITS_BITRATE, buf, bufSize ) )
		ss << std::to_wstring( std::lroundf( bitrate ) ) << L" " << buf;
	return ss.str();
}

std::wstring SamplerateToString( const HINSTANCE instance, const long samplerate, const bool kHz )
{
	std::wstringstream ss;
	if ( samplerate > 0 ) {
		const float value = kHz ? samplerate / 1000.f : samplerate;
		const int bufSize = 16;
		WCHAR buf[ bufSize ] = {};
		if ( 0 != LoadString( instance, kHz ? IDS_UNITS_KHZ : IDS_UNITS_HZ, buf, bufSize ) ) {
			ss << value << L" " << buf;
		}
	}
	return ss.str();
}

std::wstring FiletimeToString( const long long filetime )
{
	std::wstringstream ss;
	if ( filetime > 0 ) {
		FILETIME ft;
		ft.dwHighDateTime = static_cast<DWORD>( filetime >> 32 );
		ft.dwLowDateTime = static_cast<DWORD>( filetime & 0xffffffff );
		SYSTEMTIME st;
		if ( 0 != FileTimeToSystemTime( &ft, &st ) ) {
			SYSTEMTIME lt;
			if ( 0 != SystemTimeToTzSpecificLocalTime( NULL /*timeZone*/, &st, &lt ) ) {
				const int bufSize = 128;
				WCHAR dateBuf[ bufSize ] = {};
				WCHAR timeBuf[ bufSize ] = {};
				if ( ( 0 != GetDateFormat( LOCALE_USER_DEFAULT, DATE_SHORTDATE, &lt, NULL /*format*/, dateBuf, bufSize ) ) &&
					( 0 != GetTimeFormat( LOCALE_USER_DEFAULT, TIME_NOSECONDS, &lt, NULL /*format*/, timeBuf, bufSize ) ) ) {
					ss << dateBuf << L" " << timeBuf;
				}
			}
		}
	}
	return ss.str();
}

std::wstring GainToString( const HINSTANCE instance, const float gain )
{
	std::wstringstream ss;
	const int bufSize = 16;
	WCHAR buf[ bufSize ] = {};
	if ( ( 0 != LoadString( instance, IDS_UNITS_DB, buf, bufSize ) ) && std::isfinite( gain ) )
		ss << std::fixed << std::setprecision( 2 ) << std::showpos << gain << L" " << buf;
	return ss.str();
}

std::string GainToString( const HINSTANCE instance, const std::optional<float> gain )
{
	return gain.has_value() ? WideStringToUTF8( GainToString( instance, *gain ) ) : std::string();
}

std::string GenerateGUIDString()
{
	std::string result;
	UUID uuid = {};
	if (RPC_S_OK == UuidCreate( &uuid ) ) {
		RPC_CSTR uuidStr;
		if ( RPC_S_OK == UuidToStringA( &uuid, &uuidStr ) ) {
			result = reinterpret_cast<char*>( uuidStr );
			RpcStringFreeA( &uuidStr );
		}
	}
	return result;
}

void WideStringReplace( std::wstring& text, const std::wstring& original, const std::wstring& replacement )
{
	const size_t originalLength = original.size();
	if ( ( originalLength > 0 ) && ( original != replacement ) ) {
		const size_t replacementLength = replacement.size();
		size_t offset = text.find( original, 0 );
		while ( std::wstring::npos != offset ) {
			text.replace( offset, originalLength, replacement );
			offset = text.find( original, offset + replacementLength );
		}
	}
}

std::list<std::wstring> WideStringSplit( const std::wstring& text, const wchar_t delimiter )
{
	std::list<std::wstring> parts;
	std::wstring part = text;
	while ( !part.empty() ) {
		const size_t offset = 0;
		const size_t count = part.find( delimiter, offset );
		parts.push_back( part.substr( offset, count ) );
		if ( std::wstring::npos == count ) {
			break;
		} else {
			part = part.substr( count + 1 );
		}
	}
	return parts;
}

std::wstring WideStringJoin( const std::list<std::wstring>& parts, const wchar_t delimiter )
{
	std::wstring result;
	for ( const auto& part : parts ) {
		result += part;
		result += delimiter;
	}
	if ( !result.empty() ) {
		result.pop_back();
	}
	return result;
}

float GetDPIScaling()
{
	HDC dc = GetDC( NULL );
	const float scaling = static_cast<float>( GetDeviceCaps( dc, LOGPIXELSX ) ) / 96;
	ReleaseDC( 0, dc );
	return scaling;
}

long long GetRandomNumber( const long long minimum, const long long maximum )
{
	std::uniform_int_distribution<long long> dist( minimum, maximum );
	const long long value = dist( sRandomEngine );
	return value;
}

std::default_random_engine& GetRandomEngine()
{
	return sRandomEngine;
}

bool FolderExists( const std::wstring& folder )
{
	const DWORD attributes = GetFileAttributes( folder.c_str() );
	const bool exists = ( INVALID_FILE_ATTRIBUTES != attributes ) ? ( attributes & FILE_ATTRIBUTE_DIRECTORY ) : false;
	return exists;
}

void WideStringReplaceInvalidFilenameCharacters( std::wstring& filename, const std::wstring& replacement, const bool replaceFolderDelimiters )
{
	std::wstring output;
	const std::wstring invalid( replaceFolderDelimiters ? L"\\/:*?\"<>|\t\r\n" : L":*?\"<>|\t\r\n" );
	for ( const auto& c : filename ) {
		if ( ( c < 0x20 ) || ( ( c >= 0x7f ) && ( c <= 0xa0 ) ) || ( std::wstring::npos != invalid.find( c ) ) ) {
			output += replacement;
		} else {
			output += c;
		}
	}
	filename = output;
}

void CentreDialog( const HWND dialog )
{
	if ( nullptr != dialog ) {
		const HWND parent = ( nullptr != GetParent( dialog ) ) ? GetParent( dialog ) : GetDesktopWindow();
		if ( nullptr != parent ) {
			RECT parentRect = {};
			RECT dialogRect = {};
			if ( ( FALSE != GetWindowRect( parent, &parentRect ) ) && ( FALSE != GetWindowRect( dialog, &dialogRect ) ) ) {
				const int parentWidth = parentRect.right - parentRect.left;
				const int parentHeight = parentRect.bottom - parentRect.top;
				const int dialogWidth = dialogRect.right - dialogRect.left;
				const int dialogHeight = dialogRect.bottom - dialogRect.top;
				const int x = parentRect.left + ( parentWidth - dialogWidth ) / 2;
				const int y = parentRect.top + ( parentHeight - dialogHeight ) / 2;
				SetWindowPos( dialog, HWND_TOP, x, y, 0 /*cx*/, 0 /*cy*/, SWP_NOSIZE );
			}
		}
	}
}

// Subclass ID for vertically centred edit controls.
static const UINT_PTR s_CentredEditSubclassID = 1;

// Subclass procedure for single line edit controls, which vertically centres the text by reducing the client area to the height of the text.
static LRESULT CALLBACK CentredEditSubclassProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR /*refData*/ )
{
	switch ( message ) {
		case WM_NCCALCSIZE: {
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			RECT* clientRect = reinterpret_cast<RECT*>( lParam );
			int textHeight = 0;
			if ( const HDC dc = GetDC( hwnd ); nullptr != dc ) {
				const HGDIOBJ previousFont = SelectObject( dc, reinterpret_cast<HFONT>( SendMessage( hwnd, WM_GETFONT, 0, 0 ) ) );
				TEXTMETRIC textMetric = {};
				if ( GetTextMetrics( dc, &textMetric ) ) {
					// Allow for the single pixel inset a bordered edit control applies around the text.
					const bool bordered = ( WS_BORDER & GetWindowLongPtr( hwnd, GWL_STYLE ) ) || ( WS_EX_CLIENTEDGE & GetWindowLongPtr( hwnd, GWL_EXSTYLE ) );
					textHeight = textMetric.tmHeight + ( bordered ? 2 : 0 );
				}
				SelectObject( dc, previousFont );
				ReleaseDC( hwnd, dc );
			}
			const int clientHeight = clientRect->bottom - clientRect->top;
			if ( ( textHeight > 0 ) && ( clientHeight > textHeight ) ) {
				clientRect->top += ( clientHeight - textHeight ) / 2;
				clientRect->bottom = clientRect->top + textHeight;
			}
			return result;
		}
		case WM_NCPAINT: {
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			// Fill the areas between the border and the (reduced) client area, using the brush the control uses for its background.
			RECT windowRect = {};
			GetWindowRect( hwnd, &windowRect );
			RECT clientRect = {};
			GetClientRect( hwnd, &clientRect );
			MapWindowRect( hwnd, NULL /*hwndTo*/, &clientRect );
			OffsetRect( &clientRect, -windowRect.left, -windowRect.top );
			OffsetRect( &windowRect, -windowRect.left, -windowRect.top );
			const int border = clientRect.left;
			const RECT topRect = { clientRect.left, border, clientRect.right, clientRect.top };
			const RECT bottomRect = { clientRect.left, clientRect.bottom, clientRect.right, windowRect.bottom - border };
			if ( const HDC dc = GetWindowDC( hwnd ); nullptr != dc ) {
				const bool useStaticColour = !IsWindowEnabled( hwnd ) || ( ES_READONLY & GetWindowLongPtr( hwnd, GWL_STYLE ) );
				const HBRUSH brush = reinterpret_cast<HBRUSH>( SendMessage( GetParent( hwnd ), useStaticColour ? WM_CTLCOLORSTATIC : WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>( dc ), reinterpret_cast<LPARAM>( hwnd ) ) );
				if ( nullptr != brush ) {
					FillRect( dc, &topRect, brush );
					FillRect( dc, &bottomRect, brush );
				}
				ReleaseDC( hwnd, dc );
			}
			return result;
		}
		case WM_NCHITTEST: {
			// Treat the areas above and below the text as part of the client area, so that clicking there sets the focus and shows the text cursor.
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			if ( HTCLIENT != result ) {
				RECT windowRect = {};
				GetWindowRect( hwnd, &windowRect );
				const POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				if ( PtInRect( &windowRect, pt ) ) {
					return HTCLIENT;
				}
			}
			return result;
		}
		case WM_ENABLE: {
			const LRESULT result = DefSubclassProc( hwnd, message, wParam, lParam );
			RedrawWindow( hwnd, NULL /*rect*/, NULL /*region*/, RDW_FRAME | RDW_INVALIDATE );
			return result;
		}
		case WM_NCDESTROY: {
			RemoveWindowSubclass( hwnd, CentredEditSubclassProc, subclassID );
			break;
		}
		default: {
			break;
		}
	}
	return DefSubclassProc( hwnd, message, wParam, lParam );
}

// Returns whether 'hwnd' is a single line edit control.
static bool IsSingleLineEdit( const HWND hwnd )
{
	WCHAR className[ 64 ] = {};
	GetClassName( hwnd, className, static_cast<int>( std::size( className ) ) );
	return ( 0 == _wcsicmp( className, WC_EDIT ) ) && !( ES_MULTILINE & GetWindowLongPtr( hwnd, GWL_STYLE ) );
}

// Vertically centres the text of the 'hwnd' control, if it is a single line edit control.
static BOOL CALLBACK CentreEditText( HWND hwnd, LPARAM /*param*/ )
{
	if ( IsSingleLineEdit( hwnd ) ) {
		SetWindowSubclass( hwnd, CentredEditSubclassProc, s_CentredEditSubclassID, 0 /*refData*/ );
		SetWindowPos( hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
	}
	return TRUE;
}

// Returns the area, in 'dialog' coordinates, in which the 'control' text is displayed, or nullopt if the control is not a single line edit control or combo box.
static std::optional<RECT> GetControlTextRect( const HWND dialog, const HWND control )
{
	RECT textRect = {};
	if ( COMBOBOXINFO comboBoxInfo = { sizeof( COMBOBOXINFO ) }; GetComboBoxInfo( control, &comboBoxInfo ) ) {
		if ( ( nullptr != comboBoxInfo.hwndItem ) && ( comboBoxInfo.hwndItem != comboBoxInfo.hwndCombo ) ) {
			GetClientRect( comboBoxInfo.hwndItem, &textRect );
			MapWindowRect( comboBoxInfo.hwndItem, dialog, &textRect );
		} else {
			textRect = comboBoxInfo.rcItem;
			MapWindowRect( control, dialog, &textRect );
		}
		return textRect;
	}
	if ( IsSingleLineEdit( control ) ) {
		GetClientRect( control, &textRect );
		MapWindowRect( control, dialog, &textRect );
		return textRect;
	}
	return std::nullopt;
}

int GetDialogComboBoxHeight( const HWND dialog )
{
	int comboHeight = 0;
	if ( const HWND combo = CreateWindowEx( 0, WC_COMBOBOX, L"", WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 100, 100, dialog, nullptr, GetModuleHandle( nullptr ), nullptr ); nullptr != combo ) {
		SendMessage( combo, WM_SETFONT, SendMessage( dialog, WM_GETFONT, 0, 0 ), FALSE /*redraw*/ );
		RECT comboRect = {};
		GetWindowRect( combo, &comboRect );
		comboHeight = comboRect.bottom - comboRect.top;
		DestroyWindow( combo );
	}
	return comboHeight;
}

int GetPushButtonInset()
{
	// Themed push buttons (light & dark) leave a one pixel margin between the window edge and the button frame, at all display scaling levels.
	return 1;
}

int GetDialogButtonHeight( const HWND dialog )
{
	// The visible button frame is midway between the combo box height and a standard (14 dialog unit) button,
	// with any extra height split evenly above & below, so that buttons can be centred on adjacent controls.
	const int comboHeight = GetDialogComboBoxHeight( dialog );
	const int inset = GetPushButtonInset();
	RECT standardRect = { 0, 0, 0, 14 };
	MapDialogRect( dialog, &standardRect );
	const int standardHeight = standardRect.bottom - standardRect.top - 2 * inset;
	const int extra = std::max( 0, static_cast<int>( std::lround( ( standardHeight - comboHeight ) / 4.0 ) ) );
	return comboHeight + 2 * extra + 2 * inset;
}

// Returns whether 'hwnd' is a push button (or a check box with a push button appearance).
static bool IsPushButton( const HWND hwnd )
{
	WCHAR className[ 64 ] = {};
	GetClassName( hwnd, className, static_cast<int>( std::size( className ) ) );
	if ( 0 != _wcsicmp( className, WC_BUTTON ) ) {
		return false;
	}
	const LONG_PTR style = GetWindowLongPtr( hwnd, GWL_STYLE );
	const LONG_PTR type = BS_TYPEMASK & style;
	return ( BS_PUSHBUTTON == type ) || ( BS_DEFPUSHBUTTON == type ) || ( ( BS_PUSHLIKE & style ) && ( ( BS_CHECKBOX == type ) || ( BS_AUTOCHECKBOX == type ) ) );
}

void LayoutDialogTextControls( const HWND dialog )
{
	if ( nullptr == dialog ) {
		return;
	}

	// Match the edit control & button heights to the natural height of a combo box using the dialog font.
	// (Combo boxes do not vertically centre their text if they are made taller, so the other controls are adjusted instead.)
	const int comboHeight = GetDialogComboBoxHeight( dialog );
	if ( comboHeight > 0 ) {
		// Edit controls and combo boxes are both vertically centred within a standard (13 dialog unit) row, starting at the top of the control in the dialog template.
		RECT rowRect = { 0, 0, 0, 13 };
		MapDialogRect( dialog, &rowRect );
		const int rowHeight = rowRect.bottom - rowRect.top;
		for ( HWND control = GetWindow( dialog, GW_CHILD ); nullptr != control; control = GetWindow( control, GW_HWNDNEXT ) ) {
			WCHAR className[ 64 ] = {};
			GetClassName( control, className, static_cast<int>( std::size( className ) ) );
			if ( 0 == _wcsicmp( className, WC_COMBOBOX ) ) {
				RECT comboRect = {};
				GetWindowRect( control, &comboRect );
				MapWindowRect( NULL /*hwndFrom*/, dialog, &comboRect );
				if ( const int offset = ( rowHeight - ( comboRect.bottom - comboRect.top ) ) / 2; offset > 0 ) {
					SetWindowPos( control, NULL, comboRect.left, comboRect.top + offset, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
				}
			} else if ( IsSingleLineEdit( control ) ) {
				RECT editRect = {};
				GetWindowRect( control, &editRect );
				MapWindowRect( NULL /*hwndFrom*/, dialog, &editRect );
				const int editHeight = editRect.bottom - editRect.top;
				if ( editHeight != comboHeight ) {
					// Keep the edit control vertically centred on its original position.
					const int top = editRect.top + ( editHeight - comboHeight ) / 2;
					SetWindowPos( control, NULL, editRect.left, top, editRect.right - editRect.left, comboHeight, SWP_NOZORDER | SWP_NOACTIVATE );
				}
			}
		}

		// Buttons are centred on an edit control or combo box on the same row, otherwise they are kept vertically centred on their original position.
		const int newButtonHeight = GetDialogButtonHeight( dialog );
		const int buttonOffset = ( newButtonHeight - comboHeight ) / 2;
		for ( HWND button = GetWindow( dialog, GW_CHILD ); nullptr != button; button = GetWindow( button, GW_HWNDNEXT ) ) {
			if ( !IsPushButton( button ) ) {
				continue;
			}
			RECT buttonRect = {};
			GetWindowRect( button, &buttonRect );
			MapWindowRect( NULL /*hwndFrom*/, dialog, &buttonRect );
			const int buttonHeight = buttonRect.bottom - buttonRect.top;
			int top = buttonRect.top + ( buttonHeight - newButtonHeight ) / 2;
			for ( HWND control = GetWindow( dialog, GW_CHILD ); nullptr != control; control = GetWindow( control, GW_HWNDNEXT ) ) {
				WCHAR className[ 64 ] = {};
				GetClassName( control, className, static_cast<int>( std::size( className ) ) );
				if ( ( WS_VISIBLE & GetWindowLongPtr( control, GWL_STYLE ) ) && ( IsSingleLineEdit( control ) || ( 0 == _wcsicmp( className, WC_COMBOBOX ) ) ) ) {
					RECT controlRect = {};
					GetWindowRect( control, &controlRect );
					MapWindowRect( NULL /*hwndFrom*/, dialog, &controlRect );
					const int buttonCentre = ( buttonRect.top + buttonRect.bottom ) / 2;
					if ( ( buttonCentre > controlRect.top ) && ( buttonCentre < controlRect.bottom ) ) {
						top = controlRect.top - buttonOffset;
						break;
					}
				}
			}
			if ( ( top != buttonRect.top ) || ( newButtonHeight != buttonHeight ) ) {
				SetWindowPos( button, NULL, buttonRect.left, top, buttonRect.right - buttonRect.left, newButtonHeight, SWP_NOZORDER | SWP_NOACTIVATE );
			}
		}
	}

	// Vertically centre the edit control text (including the edit part of combo boxes).
	EnumChildWindows( dialog, CentreEditText, 0 /*param*/ );

	// Align the labels with the text of the control which follows them.
	for ( HWND label = GetWindow( dialog, GW_CHILD ); nullptr != label; label = GetWindow( label, GW_HWNDNEXT ) ) {
		WCHAR className[ 64 ] = {};
		GetClassName( label, className, static_cast<int>( std::size( className ) ) );
		const LONG_PTR style = GetWindowLongPtr( label, GWL_STYLE );
		const LONG_PTR type = SS_TYPEMASK & style;
		if ( ( 0 != _wcsicmp( className, WC_STATIC ) ) || !( WS_VISIBLE & style ) ||
				( ( SS_LEFT != type ) && ( SS_RIGHT != type ) && ( SS_CENTER != type ) && ( SS_LEFTNOWORDWRAP != type ) ) ) {
			continue;
		}
		// Note that the dialog is not yet visible, so the control style is checked (rather than using IsWindowVisible).
		const HWND control = GetWindow( label, GW_HWNDNEXT );
		if ( ( nullptr == control ) || !( WS_VISIBLE & GetWindowLongPtr( control, GWL_STYLE ) ) ) {
			continue;
		}
		const auto textRect = GetControlTextRect( dialog, control );
		if ( !textRect.has_value() ) {
			continue;
		}

		// Only align single line labels which are on the same row as the control.
		RECT labelRect = {};
		GetWindowRect( label, &labelRect );
		MapWindowRect( NULL /*hwndFrom*/, dialog, &labelRect );
		RECT controlRect = {};
		GetWindowRect( control, &controlRect );
		MapWindowRect( NULL /*hwndFrom*/, dialog, &controlRect );
		const int labelHeight = labelRect.bottom - labelRect.top;
		const int controlHeight = controlRect.bottom - controlRect.top;
		const bool sameRow = ( labelRect.top < controlRect.bottom ) && ( labelRect.bottom > controlRect.top );
		if ( !sameRow || ( labelHeight > controlHeight * 3 / 2 ) ) {
			continue;
		}

		SetWindowLongPtr( label, GWL_STYLE, style | SS_CENTERIMAGE );
		SetWindowPos( label, NULL, labelRect.left, textRect->top, labelRect.right - labelRect.left, textRect->bottom - textRect->top, SWP_NOZORDER | SWP_NOACTIVATE );
	}
}

std::wstring GetFileExtension( const std::wstring& filename )
{
	const size_t pos = filename.rfind( '.' );
	const std::wstring ext = ( std::wstring::npos != pos ) ? WideStringToLower( filename.substr( pos + 1 ) ) : std::wstring();
	return ext;
}

bool IsURL( const std::wstring filename )
{
	bool isURL = false;
	constexpr std::array schemes{ L"ftp", L"http", L"https" };
	const size_t pos = filename.find( ':' );
	if ( std::wstring::npos != pos ) {
		isURL = ( schemes.end() != std::find( schemes.begin(), schemes.end(), WideStringToLower( filename.substr( 0, pos ) ) ) );
	}
	return isURL;
}

void SetWindowAccessibleName( const HINSTANCE instance, const HWND hwnd, const UINT stringID )
{
	const int textSize = 256;
	WCHAR text[ textSize ] = {};
	LoadString( instance, stringID, text, textSize );
	IAccPropServices* accPropServices = nullptr;
	HRESULT hr = CoCreateInstance( CLSID_AccPropServices, nullptr, CLSCTX_INPROC, IID_PPV_ARGS( &accPropServices ) );
	if ( SUCCEEDED( hr ) ) {
		hr = accPropServices->SetHwndPropStr( hwnd, static_cast<DWORD>( OBJID_CLIENT ), CHILDID_SELF, Name_Property_GUID, text );
		accPropServices->Release();
	}
}

bool AreRoughlyEqual( const float value1, const float value2, const float tolerance )
{
	return ( fabsf( value1 - value2 ) < tolerance ) || ( std::isnan( value1 ) && std::isnan( value2 ) );
}

std::string CalculateHash( const std::string& value, const ALG_ID algorithm, const bool base64encode )
{
	std::string result;
	if ( !value.empty() ) {
		if ( HCRYPTPROV provider = 0; CryptAcquireContext( &provider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) ) {
			if ( HCRYPTHASH hash = 0; CryptCreateHash( provider, algorithm, 0, 0, &hash ) ) {
				if ( CryptHashData( hash, reinterpret_cast<const unsigned char*>( value.data() ), static_cast<DWORD>( value.size() ), 0 ) ) {
					if ( DWORD hashSize = 0; CryptGetHashParam( hash, HP_HASHVAL, nullptr, &hashSize, 0 ) && ( hashSize > 0 ) ) {
						if ( std::vector<unsigned char> hashValue( hashSize ); CryptGetHashParam( hash, HP_HASHVAL, hashValue.data(), &hashSize, 0 ) ) {
							if ( base64encode ) {
								result = Base64Encode( hashValue.data(), static_cast<int>( hashValue.size() ) );
							} else {
								std::stringstream ss;
								for ( const auto& i : hashValue ) {
									ss << std::hex << std::setfill( '0' ) << std::setw( 2 ) << static_cast<int>( i );
								}
								result = ss.str();
							}
						}
					}
				}
				CryptDestroyHash( hash );
			}
			CryptReleaseContext( provider, 0 );
		}
	}
	return result;
}

std::filesystem::path ChooseArtwork( const HINSTANCE instance, const HWND hwnd, const std::filesystem::path& initialFolder )
{
	WCHAR title[ MAX_PATH ] = {};
	LoadString( instance, IDS_CHOOSEARTWORK_TITLE, title, MAX_PATH );
	WCHAR filterImages[ MAX_PATH ] = {};
	LoadString( instance, IDS_CHOOSEARTWORK_FILTERIMAGES, filterImages, MAX_PATH );
	WCHAR filterAll[ MAX_PATH ] = {};
	LoadString( instance, IDS_CHOOSE_FILTERALL, filterAll, MAX_PATH );
	const FileFilters filters = {
		{ filterImages, L"*.bmp;*.jpg;*.jpeg;*.png;*.gif;*.tiff;*.tif;*.webp" },
		{ filterAll, L"*.*" }
	};
	return ChooseFileToOpen( hwnd, title, filters, initialFolder );
}

// Creates a file chooser dialog of type 'T' (IFileOpenDialog or IFileSaveDialog), from the class ID 'clsid'.
// 'title' - dialog title (or an empty string for the default title).
// 'filters' - file filters (the first filter is initially selected).
// 'initialFolder' - initial folder location (or an empty path for the default location).
// 'options' - additional options to set, in addition to the dialog defaults.
// Returns the dialog, or nullptr on failure.
template <typename T>
static Microsoft::WRL::ComPtr<T> CreateFileDialog( const CLSID& clsid, const std::wstring& title, const FileFilters& filters, const std::filesystem::path& initialFolder, const FILEOPENDIALOGOPTIONS options )
{
	Microsoft::WRL::ComPtr<T> dialog;
	if ( SUCCEEDED( CoCreateInstance( clsid, nullptr /*outer*/, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dialog ) ) ) ) {
		FILEOPENDIALOGOPTIONS currentOptions = 0;
		if ( SUCCEEDED( dialog->GetOptions( &currentOptions ) ) ) {
			dialog->SetOptions( currentOptions | FOS_FORCEFILESYSTEM | options );
		}
		if ( !title.empty() ) {
			dialog->SetTitle( title.c_str() );
		}
		if ( !filters.empty() ) {
			std::vector<COMDLG_FILTERSPEC> filterSpecs;
			filterSpecs.reserve( filters.size() );
			for ( const auto& [description, patterns] : filters ) {
				filterSpecs.push_back( { description.c_str(), patterns.c_str() } );
			}
			dialog->SetFileTypes( static_cast<UINT>( filterSpecs.size() ), filterSpecs.data() );
			dialog->SetFileTypeIndex( 1 );
		}
		if ( !initialFolder.empty() ) {
			Microsoft::WRL::ComPtr<IShellItem> initialItem;
			if ( SUCCEEDED( SHCreateItemFromParsingName( initialFolder.c_str(), nullptr /*bindContext*/, IID_PPV_ARGS( &initialItem ) ) ) ) {
				dialog->SetFolder( initialItem.Get() );
			}
		}
	}
	return dialog;
}

// Returns the file system path of the shell 'item', or an empty path on failure.
static std::filesystem::path GetShellItemPath( IShellItem* item )
{
	std::filesystem::path result;
	LPWSTR path = nullptr;
	if ( ( nullptr != item ) && SUCCEEDED( item->GetDisplayName( SIGDN_FILESYSPATH, &path ) ) ) {
		result = path;
		CoTaskMemFree( path );
	}
	return result;
}

// Shows the file chooser 'dialog' and returns the chosen item path, or an empty path on failure.
static std::filesystem::path ShowFileDialog( IFileDialog* dialog, const HWND hwnd )
{
	std::filesystem::path result;
	if ( ( nullptr != dialog ) && SUCCEEDED( dialog->Show( hwnd ) ) ) {
		Microsoft::WRL::ComPtr<IShellItem> item;
		if ( SUCCEEDED( dialog->GetResult( &item ) ) ) {
			result = GetShellItemPath( item.Get() );
		}
	}
	return result;
}

std::filesystem::path ChooseFolder( const HWND hwnd, const std::wstring& title, const std::filesystem::path& initialFolder )
{
	const auto dialog = CreateFileDialog<IFileOpenDialog>( CLSID_FileOpenDialog, title, {} /*filters*/, initialFolder, FOS_PICKFOLDERS | FOS_PATHMUSTEXIST );
	return ShowFileDialog( dialog.Get(), hwnd );
}

std::filesystem::path ChooseFileToOpen( const HWND hwnd, const std::wstring& title, const FileFilters& filters, const std::filesystem::path& initialFolder )
{
	const auto dialog = CreateFileDialog<IFileOpenDialog>( CLSID_FileOpenDialog, title, filters, initialFolder, FOS_FILEMUSTEXIST );
	return ShowFileDialog( dialog.Get(), hwnd );
}

std::vector<std::filesystem::path> ChooseFilesToOpen( const HWND hwnd, const std::wstring& title, const FileFilters& filters, const std::filesystem::path& initialFolder )
{
	std::vector<std::filesystem::path> result;
	const auto dialog = CreateFileDialog<IFileOpenDialog>( CLSID_FileOpenDialog, title, filters, initialFolder, FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT );
	if ( dialog && SUCCEEDED( dialog->Show( hwnd ) ) ) {
		Microsoft::WRL::ComPtr<IShellItemArray> items;
		DWORD itemCount = 0;
		if ( SUCCEEDED( dialog->GetResults( &items ) ) && SUCCEEDED( items->GetCount( &itemCount ) ) ) {
			result.reserve( itemCount );
			for ( DWORD itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
				Microsoft::WRL::ComPtr<IShellItem> item;
				if ( SUCCEEDED( items->GetItemAt( itemIndex, &item ) ) ) {
					if ( const auto path = GetShellItemPath( item.Get() ); !path.empty() ) {
						result.push_back( path );
					}
				}
			}
		}
	}
	return result;
}

std::filesystem::path ChooseFileToSave( const HWND hwnd, const std::wstring& title, const FileFilters& filters, const std::filesystem::path& initialFolder,
	const std::wstring& initialFilename, const std::wstring& defaultExtension, UINT* selectedFilter )
{
	std::filesystem::path result;
	const auto dialog = CreateFileDialog<IFileSaveDialog>( CLSID_FileSaveDialog, title, filters, initialFolder, FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST );
	if ( dialog ) {
		if ( !initialFilename.empty() ) {
			dialog->SetFileName( initialFilename.c_str() );
		}
		if ( !defaultExtension.empty() ) {
			dialog->SetDefaultExtension( defaultExtension.c_str() );
		}
		result = ShowFileDialog( dialog.Get(), hwnd );
		if ( !result.empty() && ( nullptr != selectedFilter ) ) {
			UINT filterIndex = 1;
			dialog->GetFileTypeIndex( &filterIndex );
			*selectedFilter = filterIndex;
		}
	}
	return result;
}

std::optional<COLORREF> ChooseColour( const HWND hwnd, const COLORREF initialColour, COLORREF* customColours )
{
	std::optional<COLORREF> result;
	CHOOSECOLOR chooseColor = {};
	chooseColor.lStructSize = sizeof( CHOOSECOLOR );
	chooseColor.hwndOwner = hwnd;
	chooseColor.rgbResult = initialColour;
	chooseColor.lpCustColors = customColours;
	chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
	if ( TRUE == ChooseColor( &chooseColor ) && ( initialColour != chooseColor.rgbResult ) ) {
		result = chooseColor.rgbResult;
	}
	return result;
}

HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int size, const COLORREF colour, const BYTE alpha )
{
	return CreateColourBitmap( instance, iconID, size, size, colour, alpha );
}

HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int width, const int height, const COLORREF colour, const BYTE alpha )
{
	HBITMAP hBitmap = nullptr;
	if ( const HICON hIcon = static_cast<HICON>( LoadImage( instance, MAKEINTRESOURCE( iconID ), IMAGE_ICON, width, height, LR_DEFAULTCOLOR | LR_SHARED ) ); nullptr != hIcon ) {
		if ( ICONINFO iconInfo = {}; ( nullptr != hIcon ) && GetIconInfo( hIcon, &iconInfo ) ) {
			if ( BITMAP bm = {}; GetObject( iconInfo.hbmColor, sizeof( BITMAP ), &bm ) ) {
				if ( const HDC dc = GetDC( nullptr ); nullptr != dc ) {
					BITMAPINFO bitmapInfo = {};
					bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
					bitmapInfo.bmiHeader.biWidth = bm.bmWidth;
					bitmapInfo.bmiHeader.biHeight = bm.bmHeight;
					bitmapInfo.bmiHeader.biPlanes = 1;
					bitmapInfo.bmiHeader.biBitCount = 32;
					bitmapInfo.bmiHeader.biCompression = BI_RGB;
					std::vector<unsigned char> bits( bm.bmHeight * bm.bmWidth * 4 );
					if ( GetDIBits( dc, iconInfo.hbmColor, 0, bm.bmHeight, bits.data(), &bitmapInfo, DIB_RGB_COLORS ) ) {
						const unsigned char r = GetRValue( colour );
						const unsigned char g = GetGValue( colour );
						const unsigned char b = GetBValue( colour );
						const bool modifyAlpha = alpha < 255;
						for ( int i = 0; i < bm.bmWidth * bm.bmHeight * 4; i += 4 ) {
							if ( 0 != bits[ i + 3 ] ) {
								bits[ i ] = b;
								bits[ i + 1 ] = g;
								bits[ i + 2 ] = r;
								if ( modifyAlpha ) {
									const BYTE originalAlpha = bits[ i + 3 ];
									const BYTE modifiedAlpha = static_cast<BYTE>( originalAlpha * alpha / 255.0f );
									bits[ i + 3 ] = modifiedAlpha;
								}
							}
						}
						if ( SetDIBits( dc, iconInfo.hbmColor, 0, bm.bmHeight, bits.data(), &bitmapInfo, DIB_RGB_COLORS ) ) {
							hBitmap = iconInfo.hbmColor;
						}
					}
					ReleaseDC( nullptr, dc );
				}
			}
			if ( iconInfo.hbmColor != hBitmap ) {
				DeleteObject( iconInfo.hbmColor );
			}
			DeleteObject( iconInfo.hbmMask );
		}
	}
	return hBitmap;
}

bool IsHighContrastActive()
{
	bool isActive = false;
	HIGHCONTRAST highContrast = {};
	highContrast.cbSize = sizeof( HIGHCONTRAST );
	if ( SystemParametersInfo( SPI_GETHIGHCONTRAST, sizeof( HIGHCONTRAST ), &highContrast, 0 ) ) {
		isActive = ( HCF_HIGHCONTRASTON & highContrast.dwFlags );
	}
	return isActive;
}

bool IsClassicThemeActive()
{
	return !IsAppThemed();
}

COLORREF GetSystemTextColour()
{
	return GetSysColor( COLOR_WINDOWTEXT );
}

COLORREF GetSystemWindowColour()
{
	return GetSysColor( COLOR_WINDOW );
}

COLORREF GetSystemHighlightColour()
{
	return GetSysColor( COLOR_HIGHLIGHT );
}

bool IsWindows10()
{
	return IsWindows10OrGreater();
}

FILETIME GetLastModifiedTime( const std::filesystem::path& filepath )
{
	FILETIME lastModifiedTime = {};
	constexpr DWORD accessMode = GENERIC_READ;
	constexpr DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	if ( const HANDLE handle = CreateFile( filepath.c_str(), accessMode, shareMode, nullptr /*security*/, OPEN_EXISTING, 0 /*flags*/, nullptr /*template*/ ); INVALID_HANDLE_VALUE != handle ) {
		GetFileTime( handle, nullptr /*creationTime*/, nullptr /*lastAccessTime*/, &lastModifiedTime );
		CloseHandle( handle );
	}
	return lastModifiedTime;
}

void SetLastModifiedTime( const std::filesystem::path& filepath, const FILETIME lastModified )
{
	constexpr DWORD accessMode = FILE_WRITE_ATTRIBUTES;
	constexpr DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	if ( const HANDLE handle = CreateFile( filepath.c_str(), accessMode, shareMode, nullptr /*security*/, OPEN_EXISTING, 0 /*flags*/, nullptr /*template*/ ); INVALID_HANDLE_VALUE != handle ) {
		SetFileTime( handle, nullptr, nullptr, &lastModified );
		CloseHandle( handle );
	}
}

bool DeleteFiles( const std::set<std::filesystem::path> files, const HWND hwnd )
{
	std::error_code ec;
	std::vector<wchar_t> filenameBuffer;
	for ( const auto& file : files ) {
		if ( file.is_absolute() && std::filesystem::exists( file, ec ) ) {
			filenameBuffer.insert( filenameBuffer.end(), file.native().begin(), file.native().end() );
			filenameBuffer.push_back( '\0' );
		}
	}
	if ( !filenameBuffer.empty() ) {
		filenameBuffer.push_back( '\0' );
		SHFILEOPSTRUCT fileOp = {};
		fileOp.hwnd = hwnd;
		fileOp.wFunc = FO_DELETE;
		fileOp.pFrom = filenameBuffer.data();
		return ( 0 == SHFileOperation( &fileOp ) );
	}
	return false;
}

// Encodes 'bitmap' in an image 'format' (e.g. Gdiplus::ImageFormatPNG), using optional encoder 'parameters'.
// Returns the encoded image, or an empty vector if the bitmap could not be encoded.
static std::vector<BYTE> EncodeImage( Gdiplus::Bitmap& bitmap, const GUID& format, const Gdiplus::EncoderParameters* parameters = nullptr )
{
	std::vector<BYTE> image;
	CLSID encoderClsid = {};
	UINT numEncoders = 0;
	UINT bufferSize = 0;
	if ( ( Gdiplus::Ok == Gdiplus::GetImageEncodersSize( &numEncoders, &bufferSize ) ) && ( bufferSize > 0 ) ) {
		std::vector<char> buffer( bufferSize, 0 );
		Gdiplus::ImageCodecInfo* imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>( buffer.data() );
		if ( Gdiplus::Ok == Gdiplus::GetImageEncoders( numEncoders, bufferSize, imageCodecInfo ) ) {
			for ( UINT index = 0; index < numEncoders; index++ ) {
				if ( format == imageCodecInfo[ index ].FormatID ) {
					encoderClsid = imageCodecInfo[ index ].Clsid;
					break;
				}
			}
		}
	}
	IStream* encoderStream = nullptr;
	if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &encoderStream ) ) ) {
		if ( Gdiplus::Ok == bitmap.Save( encoderStream, &encoderClsid, parameters ) ) {
			STATSTG stats = {};
			if ( SUCCEEDED( encoderStream->Stat( &stats, STATFLAG_NONAME ) ) && ( stats.cbSize.QuadPart > 0 ) && ( stats.cbSize.QuadPart < sMaxConvertImageBytes ) ) {
				if ( SUCCEEDED( encoderStream->Seek( { 0 }, STREAM_SEEK_SET, NULL /*newPosition*/ ) ) ) {
					const ULONG encoderBufferSize = static_cast<ULONG>( stats.cbSize.QuadPart );
					std::vector<BYTE> encoderBuffer( encoderBufferSize );
					ULONG bytesRead = 0;
					if ( SUCCEEDED( encoderStream->Read( &encoderBuffer[ 0 ], encoderBufferSize, &bytesRead ) ) && ( bytesRead == encoderBufferSize ) ) {
						image = std::move( encoderBuffer );
					}
				}
			}
		}
		encoderStream->Release();
	}
	return image;
}

// Decodes the WebP 'image' to a 32bpp ARGB bitmap (using the first frame of an animated image).
// Returns the bitmap, or nullptr if the image could not be decoded.
static std::unique_ptr<Gdiplus::Bitmap> DecodeWebPImage( const std::vector<BYTE>& image )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	WebPAnimDecoderOptions options = {};
	if ( WebPAnimDecoderOptionsInit( &options ) ) {
		// Non-premultiplied BGRA has the same memory layout as GDI+ 32bpp ARGB.
		options.color_mode = MODE_BGRA;
		const WebPData data = { image.data(), image.size() };
		if ( WebPAnimDecoder* decoder = WebPAnimDecoderNew( &data, &options ); nullptr != decoder ) {
			WebPAnimInfo info = {};
			uint8_t* frame = nullptr;
			int timestamp = 0;
			if ( WebPAnimDecoderGetInfo( decoder, &info ) && ( info.canvas_width > 0 ) && ( info.canvas_height > 0 ) && WebPAnimDecoderGetNext( decoder, &frame, &timestamp ) && ( nullptr != frame ) ) {
				try {
					const INT width = static_cast<INT>( info.canvas_width );
					const INT height = static_cast<INT>( info.canvas_height );
					bitmap = std::make_unique<Gdiplus::Bitmap>( width, height, PixelFormat32bppARGB );
					Gdiplus::Rect rect( 0 /*x*/, 0 /*y*/, width, height );
					Gdiplus::BitmapData bitmapData = {};
					if ( ( Gdiplus::Ok == bitmap->GetLastStatus() ) && ( Gdiplus::Ok == bitmap->LockBits( &rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bitmapData ) ) ) {
						const size_t frameStride = static_cast<size_t>( width ) * 4;
						for ( INT row = 0; row < height; row++ ) {
							memcpy( static_cast<BYTE*>( bitmapData.Scan0 ) + static_cast<ptrdiff_t>( row ) * bitmapData.Stride, frame + row * frameStride, frameStride );
						}
						bitmap->UnlockBits( &bitmapData );
					} else {
						bitmap.reset();
					}
				} catch ( ... ) {
					bitmap.reset();
				}
			}
			WebPAnimDecoderDelete( decoder );
		}
	}
	return bitmap;
}

void GetImageInformation( const std::string& image, std::string& mimeType, int& width, int& height, int& depth, int& colours )
{
	mimeType.clear();
	width = 0;
	height = 0;
	depth = 0;
	colours = 0;
	std::vector<BYTE> imageBytes = Base64Decode( image );
	if ( IsWebPImage( imageBytes ) ) {
		if ( WebPBitstreamFeatures features = {}; VP8_STATUS_OK == WebPGetFeatures( imageBytes.data(), imageBytes.size(), &features ) ) {
			mimeType = "image/webp";
			width = features.width;
			height = features.height;
			depth = features.has_alpha ? 32 : 24;
		}
		return;
	}
	const ULONG imageSize = static_cast<ULONG>( imageBytes.size() );
	if ( imageSize > 0 ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( &imageBytes[ 0 ], imageSize, NULL /*bytesWritten*/ ) ) ) {
				try {
					Gdiplus::Bitmap bitmap( stream );

					width = static_cast<int>( bitmap.GetWidth() );
					height = static_cast<int>( bitmap.GetHeight() );

					const Gdiplus::PixelFormat pixelFormat = bitmap.GetPixelFormat();
					switch ( pixelFormat ) {
						case PixelFormat4bppIndexed: {
							depth = 4;
							break;
						}
						case PixelFormat8bppIndexed: {
							depth = 8;
							break;
						}
						case PixelFormat16bppGrayScale:
						case PixelFormat16bppRGB555:
						case PixelFormat16bppRGB565:
						case PixelFormat16bppARGB1555: {
							depth = 16;
							break;
						}
						case PixelFormat24bppRGB: {
							depth = 24;
							break;
						}
						case PixelFormat32bppRGB:
						case PixelFormat32bppARGB:
						case PixelFormat32bppPARGB:
						case PixelFormat32bppCMYK: {
							depth = 32;
							break;
						}
						case PixelFormat48bppRGB: {
							depth = 48;
							break;
						}
						case PixelFormat64bppARGB:
						case PixelFormat64bppPARGB: {
							depth = 64;
							break;
						}
						default: {
							break;
						}
					}

					const INT paletteSize = bitmap.GetPaletteSize();
					if ( paletteSize > 0 ) {
						std::vector<char> buffer( paletteSize, 0 );
						Gdiplus::ColorPalette* palette = reinterpret_cast<Gdiplus::ColorPalette*>( buffer.data() );
						if ( Gdiplus::Ok == bitmap.GetPalette( palette, paletteSize ) ) {
							colours = static_cast<int>( palette->Count );
						}
					}

					GUID format = {};
					if ( Gdiplus::Ok == bitmap.GetRawFormat( &format ) ) {
						if ( ( Gdiplus::ImageFormatMemoryBMP == format ) || ( Gdiplus::ImageFormatBMP == format ) ) {
							mimeType = "image/bmp";
						} else if ( Gdiplus::ImageFormatEMF == format ) {
							mimeType = "image/x-emf";
						} else if ( Gdiplus::ImageFormatWMF == format ) {
							mimeType = "image/x-wmf";
						} else if ( Gdiplus::ImageFormatJPEG == format ) {
							mimeType = "image/jpeg";
						} else if ( Gdiplus::ImageFormatPNG == format ) {
							mimeType = "image/png";
						} else if ( Gdiplus::ImageFormatGIF == format ) {
							mimeType = "image/gif";
						} else if ( Gdiplus::ImageFormatTIFF == format ) {
							mimeType = "image/tiff";
						}
					}
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}
}

std::string ConvertImage( const std::vector<BYTE>& imageBytes )
{
	std::string encodedImage;
	if ( IsWebPImage( imageBytes ) ) {
		// GDI+ does not support WebP, so just check that the image can be decoded, and keep it in its original format.
		if ( WebPGetInfo( imageBytes.data(), imageBytes.size(), nullptr /*width*/, nullptr /*height*/ ) ) {
			encodedImage = Base64Encode( imageBytes.data(), static_cast<int>( imageBytes.size() ) );
		}
		return encodedImage;
	}
	const ULONG imageSize = static_cast<ULONG>( imageBytes.size() );
	if ( imageSize > 0 ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( &imageBytes[ 0 ], imageSize, NULL /*bytesWritten*/ ) ) ) {
				try {
					Gdiplus::Bitmap bitmap( stream );
					GUID format = {};
					if ( Gdiplus::Ok == bitmap.GetRawFormat( &format ) ) {
						if ( ( Gdiplus::ImageFormatPNG == format ) || ( Gdiplus::ImageFormatJPEG == format ) || ( Gdiplus::ImageFormatGIF == format ) ) {
							encodedImage = Base64Encode( &imageBytes[ 0 ], static_cast<int>( imageSize ) );
						} else if ( const std::vector<BYTE> png = EncodeImage( bitmap, Gdiplus::ImageFormatPNG ); !png.empty() ) {
							encodedImage = Base64Encode( png.data(), static_cast<int>( png.size() ) );
						}
					}
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}
	return encodedImage;
}

bool IsWebPImage( const std::vector<BYTE>& image )
{
	// RIFF container, with a WebP form type.
	return ( image.size() >= 12 ) && ( 0 == memcmp( image.data(), "RIFF", 4 ) ) && ( 0 == memcmp( image.data() + 8, "WEBP", 4 ) );
}

std::unique_ptr<Gdiplus::Bitmap> CreateBitmapFromImage( const std::vector<BYTE>& image )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	if ( IsWebPImage( image ) ) {
		bitmap = DecodeWebPImage( image );
	} else if ( !image.empty() ) {
		IStream* stream = nullptr;
		if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
			if ( SUCCEEDED( stream->Write( image.data(), static_cast<ULONG>( image.size() ), NULL /*bytesWritten*/ ) ) ) {
				try {
					bitmap = std::make_unique<Gdiplus::Bitmap>( stream );
				} catch ( ... ) {
				}
			}
			stream->Release();
		}
	}
	if ( bitmap && ( ( Gdiplus::Ok != bitmap->GetLastStatus() ) || ( bitmap->GetWidth() == 0 ) || ( bitmap->GetHeight() == 0 ) ) ) {
		bitmap.reset();
	}
	return bitmap;
}

std::unique_ptr<Gdiplus::Bitmap> CreateBitmapFromFile( const std::filesystem::path& filename )
{
	std::unique_ptr<Gdiplus::Bitmap> bitmap;
	std::error_code ec;
	if ( const auto fileSize = std::filesystem::file_size( filename, ec ); !ec && ( fileSize > 0 ) && ( fileSize < sMaxImageFileBytes ) ) {
		std::ifstream stream( filename, std::ios::binary | std::ios::in );
		if ( stream.is_open() ) {
			std::vector<BYTE> image( static_cast<size_t>( fileSize ) );
			if ( stream.read( reinterpret_cast<char*>( image.data() ), static_cast<std::streamsize>( image.size() ) ) ) {
				bitmap = CreateBitmapFromImage( image );
			}
		}
	}
	return bitmap;
}

std::vector<BYTE> ConvertImageToJPEG( const std::vector<BYTE>& image, const ULONG quality )
{
	std::vector<BYTE> jpeg;
	if ( const auto bitmap = CreateBitmapFromImage( image ); bitmap ) {
		try {
			// JPEG does not support transparency, so draw the image over a white background.
			const INT width = static_cast<INT>( bitmap->GetWidth() );
			const INT height = static_cast<INT>( bitmap->GetHeight() );
			Gdiplus::Bitmap flattened( width, height, PixelFormat24bppRGB );
			if ( Gdiplus::Ok == flattened.GetLastStatus() ) {
				{
					Gdiplus::Graphics graphics( &flattened );
					graphics.Clear( static_cast<Gdiplus::ARGB>( Gdiplus::Color::White ) );
					// Draw at the original size, without any resampling.
					graphics.SetInterpolationMode( Gdiplus::InterpolationModeNearestNeighbor );
					graphics.SetPixelOffsetMode( Gdiplus::PixelOffsetModeHalf );
					graphics.DrawImage( bitmap.get(), Gdiplus::Rect( 0, 0, width, height ) );
				}
				ULONG encoderQuality = quality;
				Gdiplus::EncoderParameters parameters = {};
				parameters.Count = 1;
				parameters.Parameter[ 0 ].Guid = Gdiplus::EncoderQuality;
				parameters.Parameter[ 0 ].Type = Gdiplus::EncoderParameterValueTypeLong;
				parameters.Parameter[ 0 ].NumberOfValues = 1;
				parameters.Parameter[ 0 ].Value = &encoderQuality;
				jpeg = EncodeImage( flattened, Gdiplus::ImageFormatJPEG, &parameters );
			}
		} catch ( ... ) {
		}
	}
	return jpeg;
}

bool IsJPEGImage( const std::vector<BYTE>& image )
{
	// JPEG start of image marker, followed by the start of another marker.
	return ( image.size() >= 3 ) && ( 0xff == image[ 0 ] ) && ( 0xd8 == image[ 1 ] ) && ( 0xff == image[ 2 ] );
}
#endif // !_CONSOLE