#pragma once

#if !defined(PLATFORM_WINDOWS)

// we treat bool as a global type, so don't declare it in the namespace
#ifdef _MSC_VER
    typedef int BOOL;
#elif defined(PLATFORM_APPLE)
    #ifndef OBJC_BOOL_DEFINED
        #if OBJC_BOOL_IS_BOOL
            typedef bool BOOL;
        #else
            typedef signed char BOOL; // this is the way it's defined in Obj-C
        #endif
    #endif
#else
    typedef unsigned char BOOL; // this is the way it's defined in X11
#endif

namespace APE
{
#undef __forceinline
#define __forceinline inline
#define __stdcall

#define NEAR
#define FAR

typedef unsigned long       DWORD;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef void *              HANDLE;
typedef unsigned int        UINT;
typedef long                LRESULT;
typedef wchar_t *           LPTSTR;
typedef const wchar_t *     LPCTSTR;
typedef wchar_t             TCHAR;

#undef ZeroMemory
#define ZeroMemory(POINTER, BYTES) memset(POINTER, 0, BYTES);

#define TRUE 1
#define FALSE 0

#define CALLBACK

#ifdef _T
#undef _T
#endif
#define _T(x) L ## x

#define _strnicmp strncasecmp
#define _wtoi(x) wcstol(x, NULL, 10)
#define _tcscat wcscat
#undef _totlower
#define _totlower towlower
#define _totupper towupper
#define _tcschr wcschr
#ifdef _MSC_VER
#define _tcsicmp _wcsicmp
#else
#define _tcsicmp wcscasecmp
#endif
#define _tcscpy wcscpy
#define _tcslen wcslen
#define _tcsncpy wcsncpy
#define _tcsstr wcsstr
#define _ftprintf fwprintf
#define _tcsnicmp _wcsnicmp
#define _tcscpy_s wcscpy_s
#define _tcsncpy_s wcsncpy_s
#define _ttoi _wtoi
#define _tcscmp wcscmp
#define strncpy_s(a, b, c, d) strncpy(a, c, d)
#define MAX_PATH    4096

}

#include <wctype.h>
#include <string.h>

#endif // #ifndef _WIN32
