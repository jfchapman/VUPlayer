#include "Utility.h"

#include "resource.h"

#include "Gdiplusimaging.h"

#include <initguid.h> 
#include <Uxtheme.h>
#include <VersionHelpers.h>
#include "objbase.h"
#include "uiautomation.h"
#include <shellapi.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

// Maximum size when converting images.
static const int sMaxConvertImageBytes = 0x1000000;

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
				if ( FALSE == CryptStringToBinaryA( text.c_str(), static_cast<DWORD>( text.size() ) , CRYPT_STRING_BASE64, result.data(), &bufferSize, nullptr /*skip*/, nullptr /*flags*/ ) ) {
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

GUID GenerateGUID()
{
	UUID uuid = {};
	UuidCreate( &uuid );
	return uuid;
}

std::string GenerateGUIDString()
{
	std::string result;
	UUID uuid = GenerateGUID();
	RPC_CSTR uuidStr;
	if ( RPC_S_OK == UuidToStringA( &uuid, &uuidStr ) ) {
		result = reinterpret_cast<char*>( uuidStr );
		RpcStringFreeA( &uuidStr );
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
	std::filesystem::path result;

	WCHAR title[ MAX_PATH ] = {};
	LoadString( instance, IDS_CHOOSEARTWORK_TITLE, title, MAX_PATH );
	WCHAR filter[ MAX_PATH ] = {};
	LoadString( instance, IDS_CHOOSEARTWORK_FILTERIMAGES, filter, MAX_PATH );
	const std::wstring filter1( filter );
	const std::wstring filter2( L"*.bmp;*.jpg;*.jpeg;*.png;*.gif;*.tiff;*.tif" );
	LoadString( instance, IDS_CHOOSE_FILTERALL, filter, MAX_PATH );
	const std::wstring filter3( filter );
	const std::wstring filter4( L"*.*" );
	std::vector<WCHAR> filterStr;
	filterStr.reserve( MAX_PATH );
	filterStr.insert( filterStr.end(), filter1.begin(), filter1.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter2.begin(), filter2.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter3.begin(), filter3.end() );
	filterStr.push_back( 0 );
	filterStr.insert( filterStr.end(), filter4.begin(), filter4.end() );
	filterStr.push_back( 0 );
	filterStr.push_back( 0 );

	WCHAR buffer[ MAX_PATH ] = {};
	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hwnd;
	ofn.lpstrTitle = title;
	ofn.lpstrFilter = filterStr.data();
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
	if ( FALSE != GetOpenFileName( &ofn ) ) {
		result = ofn.lpstrFile;
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

HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int size, const COLORREF colour )
{
	return CreateColourBitmap( instance, iconID, size, size, colour );
}

HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int width, const int height, const COLORREF colour )
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
						for ( int i = 0; i < bm.bmWidth * bm.bmHeight * 4; i += 4 ) {
							if ( 0 != bits[ i + 3 ] ) {
								bits[ i ] = b;
								bits[ i + 1 ] = g;
								bits[ i + 2 ] = r;
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

void GetImageInformation( const std::string& image, std::string& mimeType, int& width, int& height, int& depth, int& colours )
{
	mimeType.clear();
	width = 0;
	height = 0;
	depth = 0;
	colours = 0;
	std::vector<BYTE> imageBytes = Base64Decode( image );
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
						} else {
							CLSID encoderClsid = {};
							UINT numEncoders = 0;
							UINT bufferSize = 0;
							if ( ( Gdiplus::Ok == Gdiplus::GetImageEncodersSize( &numEncoders, &bufferSize ) ) && ( bufferSize > 0 ) ) {
								std::vector<char> buffer( bufferSize, 0 );
								Gdiplus::ImageCodecInfo* imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>( buffer.data() );
								if ( Gdiplus::Ok == Gdiplus::GetImageEncoders( numEncoders, bufferSize, imageCodecInfo ) ) {
									for ( UINT index = 0; index < numEncoders; index++ ) {
										if ( Gdiplus::ImageFormatPNG == imageCodecInfo[ index ].FormatID ) {
											encoderClsid = imageCodecInfo[ index ].Clsid;
											break;
										}
									}
								}
							}
							IStream* encoderStream = nullptr;
							if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &encoderStream ) ) ) {
								if ( Gdiplus::Ok == bitmap.Save( encoderStream, &encoderClsid ) ) {
									STATSTG stats = {};
									if ( SUCCEEDED( encoderStream->Stat( &stats, STATFLAG_NONAME ) ) && ( stats.cbSize.QuadPart > 0 ) && ( stats.cbSize.QuadPart < sMaxConvertImageBytes ) ) {
										if ( SUCCEEDED( encoderStream->Seek( { 0 }, STREAM_SEEK_SET, NULL /*newPosition*/ ) ) ) {
											const ULONG encoderBufferSize = static_cast<ULONG>( stats.cbSize.QuadPart );
											std::vector<BYTE> encoderBuffer( encoderBufferSize );
											ULONG bytesRead = 0;
											if ( SUCCEEDED( encoderStream->Read( &encoderBuffer[ 0 ], encoderBufferSize, &bytesRead ) ) && ( bytesRead == encoderBufferSize ) ) {
												encodedImage = Base64Encode( &encoderBuffer[ 0 ], static_cast<int>( encoderBufferSize ) );
											}
										}
									}
								}
								encoderStream->Release();
							}
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
#endif // !_CONSOLE