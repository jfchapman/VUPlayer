#pragma once

#include "stdafx.h"

#include <algorithm>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

// Converts UTF-8 'text' to a wide string.
std::wstring UTF8ToWideString( const std::string& text );

// Converts wide 'text' to UTF-8.
std::string WideStringToUTF8( const std::wstring& text );

// Converts default Windows ANSI code page 'text' to a wide string.
std::wstring AnsiCodePageToWideString( const std::string& text );

// Converts wide 'text' to the default Windows ANSI code page.
std::string WideStringToAnsiCodePage( const std::wstring& text );

// Converts 'text' to a wide string using the 'codePage'.
std::wstring CodePageToWideString( const std::string& text, const UINT codePage );

// Converts 'text' to UTF-8 using the 'codePage'.
std::string CodePageToUTF8( const std::string& text, const UINT codePage );

// Converts 'text' to lowercase.
std::wstring WideStringToLower( const std::wstring& text );

// Converts 'text' to uppercase.
std::wstring WideStringToUpper( const std::wstring& text );

// Converts 'text' to lowercase.
std::string StringToLower( const std::string& text );

// Converts 'text' to uppercase.
std::string StringToUpper( const std::string& text );

// Converts a file size to a string.
// 'instance' - module instance handle.
// 'filesize' - file size to convert.
std::wstring FilesizeToString( const HINSTANCE instance, const long long filesize );

// Converts a duration to a string.
// 'instance' - module instance handle.
// 'duration' - duration to convert.
// 'colonDelimited' - true to delimit with colons, false to delimit with resource strings.
std::wstring DurationToString( const HINSTANCE instance, const double duration, const bool colonDelimited );

// Converts a byte array to a base64 encoded string.
// 'bytes' - byte array.
// 'byteCount' - number of bytes.
std::string Base64Encode( const BYTE* bytes, const int byteCount );

// Converts a base64 encoded 'text' to a byte array.
std::vector<BYTE> Base64Decode( const std::string& text );

// Gets image information.
// 'image' - base64 encoded image.
// 'mimeType' - out, MIME type.
// 'width' - out, width in pixels.
// 'height' - out, height in pixels.
// 'depth' - out, bits per pixel.
// 'colours' - out, number of colours (for palette indexed images).
void GetImageInformation( const std::string& image, std::string& mimeType, int& width, int& height, int& depth, int& colours );

// Converts 'image' to a base64 encoded form, converting non-PNG/JPG/GIF images to PNG format.
std::string ConvertImage( const std::vector<BYTE>& image );

// Generates a GUID.
GUID GenerateGUID();

// Generates a GUID string.
std::string GenerateGUIDString();

// Replaces all occurences of the 'original' substring with 'replacement' in 'text'.
void WideStringReplace( std::wstring& text, const std::wstring& original, const std::wstring& replacement );

// Splits 'text' on the 'delimiter', returning the string parts (or the original string as a single part if it is not delimited).
std::list<std::wstring> WideStringSplit( const std::wstring& text, const wchar_t delimiter );

// Joins 'parts' using the 'delimiter', returning the delimited string.
std::wstring WideStringJoin( const std::list<std::wstring>& parts, const wchar_t delimiter );

// Gets the system DPI scaling factor.
float GetDPIScaling();

// Returns a random number in the range ['minimum','maximum'].
long long GetRandomNumber( const long long minimum, const long long maximum );

// Gets the random number engine.
std::default_random_engine& GetRandomEngine();

// Returns whether the 'folder' exists.
bool FolderExists( const std::wstring& folder );

// Replaces invalid characters in 'filename' with 'replacement'.
// 'replaceFolderDelimiters' - whether folder delimiters should be replaced.
void WideStringReplaceInvalidFilenameCharacters( std::wstring& filename, const std::wstring& replacement, const bool replaceFolderDelimiters );

// Converts a 'gain' value to a string (returns an empty string if the gain is not valid).
std::string GainToString( const std::optional<float> gain );

// Centres a 'dialog' with respect to its parent window, or the desktop window if the parent is not visible.
void CentreDialog( const HWND dialog );

// Returns the 'filename' extension in lowercase.
std::wstring GetFileExtension( const std::wstring& filename );

// Returns whether the 'filename' represents a URI with a FTP, HTTP or HTTPS scheme.
bool IsURL( const std::wstring filename );

// Sets the accessible name of a window.
// 'instance' - module instance handle.
// 'hwnd' - window handle.
// 'stringID' - string resource ID.
void SetWindowAccessibleName( const HINSTANCE instance, const HWND hwnd, const UINT stringID );

// Returns whether 'value1' and 'value2' are equal within the 'tolerance'.
bool AreRoughlyEqual( const float value1, const float value2, const float tolerance );

// Calculates a hash for a 'value' using the 'algorithm'.
// Returns a base64 encoded string if 'base64encode' is true, else the hex-encoded hash value.
std::string CalculateHash( const std::string& value, const ALG_ID algorithm, const bool base64encode );

// Launches a file chooser for the selection of an artwork image.
// 'instance' - module instance handle.
// 'hwnd' - parent window handle.
// 'initialFolder' - initial folder location.
// Returns the file path on success, an empty path on failure.
std::filesystem::path ChooseArtwork( const HINSTANCE instance, const HWND hwnd, const std::filesystem::path& initialFolder );

// Launches a colour chooser dialog.
// 'hwnd' - parent window handle.
// 'initialColour' - initial colour.
// 'customColours' - custom colours.
// Returns an optional colour value, if a different colour than the initial colour was chosen.
std::optional<COLORREF> ChooseColour( const HWND hwnd, const COLORREF initialColour, COLORREF* customColours );

// Creates a flat colour bitmap from an icon resource (32-bit, with transparency).
// 'instance' - module instance handle.
// 'iconID' - icon resource ID.
// 'size' - pixel size.
// 'colour' - colour value.
// Returns the bitmap handle on success (to be deleted by the caller), or nullptr on failure.
HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int size, const COLORREF colour );

// Creates a flat colour bitmap from an icon resource (32-bit, with transparency).
// 'instance' - module instance handle.
// 'iconID' - icon resource ID.
// 'width' - pixel width.
// 'height' - pixel height.
// 'colour' - colour value.
// Returns the bitmap handle on success (to be deleted by the caller), or nullptr on failure.
HBITMAP CreateColourBitmap( const HINSTANCE instance, const UINT iconID, const int width, const int height, const COLORREF colour );

// Returns whether high contrast mode is active.
bool IsHighContrastActive();

// Returns whether the Windows classic theme is active.
bool IsClassicThemeActive();

// Returns whether the OS is Windows 10 (or later).
bool IsWindows10();

// Returns the last modified time for a file.
FILETIME GetLastModifiedTime( const std::filesystem::path& filepath );

// Sets the last modified time for a file.
void SetLastModifiedTime( const std::filesystem::path& filepath, const FILETIME lastModified );

// Deletes the 'files' (which must be fully qualified paths), using the parent 'hwnd' window handle.
// Returns whether the delete file operation proceeded.
bool DeleteFiles( const std::set<std::filesystem::path> files, const HWND hwnd );

// Converts a floating point sample 'value' to 24-bit (clamping any value outside the range -1.0 to +1.0).
inline int32_t FloatTo24( const float value )
{
	const float scaledValue = value * 8388608;
	const int32_t result = ( scaledValue > static_cast<float>( 8388607 ) ) ? 8388607 :
		( ( scaledValue < static_cast<float>( -8388608 ) ) ? -8388608 : static_cast<int32_t>( scaledValue ) );
	return result;
}

// Converts a floating point sample 'value' to 16-bit (clamping any value outside the range -1.0 to +1.0).
inline int16_t FloatTo16( const float value )
{
	const float scaledValue = value * 32768;
	const int16_t result = ( scaledValue > static_cast<float>( 32767 ) ) ? 32767 :
		( ( scaledValue < static_cast<float>( -32768 ) ) ? -32768 : static_cast<int16_t>( scaledValue ) );
	return result;
}

// Converts a floating point sample 'value' to signed 8-bit (clamping any value outside the range -1.0 to +1.0).
inline int8_t FloatToSigned8( const float value )
{
	const float scaledValue = value * 128;
	const int8_t result = ( scaledValue > static_cast<float>( 127 ) ) ? 127 :
		( ( scaledValue < static_cast<float>( -128 ) ) ? -128 : static_cast<int8_t>( scaledValue ) );
	return result;
}

// Converts a floating point sample 'value' to unsigned 8-bit (clamping any value outside the range -1.0 to +1.0).
inline uint8_t FloatToUnsigned8( const float value )
{
	const float scaledValue = ( value + 1.0f ) * 128;
	const uint8_t result = ( scaledValue > static_cast<float>( 255 ) ) ? 255 :
		( ( scaledValue < static_cast<float>( 0 ) ) ? 0 : static_cast<uint8_t>( scaledValue ) );
	return result;
}

// Converts a signed 64-bit 'value' to a floating point value in the range -1.0 to +1.0.
inline float Signed64ToFloat( const int64_t value )
{
	return static_cast<float>( value ) / 0x8000000000000000ull;
}

// Converts a signed 32-bit 'value' to a floating point value in the range -1.0 to +1.0.
inline float Signed32ToFloat( const int32_t value )
{
	return static_cast<float>( value ) / 0x80000000ul;
}

// Converts a signed 16-bit 'value' to a floating point value in the range -1.0 to +1.0.
inline float Signed16ToFloat( const int16_t value )
{
	return static_cast<float>( value ) / 0x8000;
}

// Converts an unsigned 8-bit 'value' to a floating point value in the range -1.0 to +1.0.
inline float Unsigned8ToFloat( const uint8_t value )
{
	return static_cast<float>( static_cast<int>( value ) - 0x80 ) / 0x80;
}

// Returns the current timestamp (the number of 100-nanosecond intervals since January 1, 1601 UTC).
inline long long GetCurrentTimestamp()
{
	FILETIME fileTime = {};
	GetSystemTimeAsFileTime( &fileTime );
	return ( static_cast<long long>( fileTime.dwHighDateTime ) << 32 ) + fileTime.dwLowDateTime;
}

// Returns 'str' stripped of any leading or trailing whitespace.
template <typename T>
T StripWhitespace( const T& str )
{
	const T kWhitespace{ ' ', '\t' };
	const auto start = str.find_first_not_of( kWhitespace );
	const auto end = str.find_last_not_of( kWhitespace );
	if ( ( T::npos != start ) && ( T::npos != end ) )
		return str.substr( start, 1 + end - start );
	return {};
}

// Returns 'str' stripped of any enclosing double quotes.
template <typename T>
T StripQuotes( const T& str )
{
	return ( ( str.size() >= 2 ) && str.starts_with( '"' ) && str.ends_with( '"' ) ) ? str.substr( 1, str.size() - 2 ) : str;
}
