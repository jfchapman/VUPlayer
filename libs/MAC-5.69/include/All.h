#pragma once

/*****************************************************************************************
Platform
 
One of the following platforms should be defined (either in code or as a project setting):
PLATFORM_WINDOWS
PLATFORM_APPLE
PLATFORM_LINUX
*****************************************************************************************/
#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_APPLE) && !defined(PLATFORM_LINUX)
    #pragma message("No platform set for MACLib, defaulting to Windows")
    #define PLATFORM_WINDOWS
#endif

#ifdef PLATFORM_ANDROID
#undef MBN
#undef ODN
#undef ODS
#undef CATCH_ERRORS
#undef ASSERT
#undef _totlower
#undef ZeroMemory
#undef __forceinline
#endif

/*****************************************************************************************
Global includes
*****************************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <math.h>

#if defined(PLATFORM_WINDOWS)
    #include "WindowsEnvironment.h"
    #include <windows.h>
    #include <tchar.h>
    #include <assert.h>
#else
    #ifndef _MSC_VER
        #include <unistd.h>
    #endif
    #include <time.h>
    #ifndef _MSC_VER
        #include <sys/time.h>
    #endif
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <wchar.h>
    #include "NoWindows.h"
#endif
#define ape_max(a,b)    (((a) > (b)) ? (a) : (b))
#define ape_min(a,b)    (((a) < (b)) ? (a) : (b))

#include "SmartPtr.h"

/*****************************************************************************************
Version
*****************************************************************************************/
#include "Version.h"

// year in the copyright strings
#define MAC_YEAR 2021

// build the version string
#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)
#define MAC_VER_FILE_VERSION_STR        STRINGIZE(MAC_VERSION_MAJOR) _T(".") STRINGIZE(MAC_VERSION_REVISION) 
#define MAC_VER_FILE_VERSION_STR_NARROW STRINGIZE(MAC_VERSION_MAJOR) "." STRINGIZE(MAC_VERSION_REVISION) 
#define MAC_VER_FILE_VERSION_STR_FULL    STRINGIZE(MAC_VERSION_MAJOR) _T(".") STRINGIZE(MAC_VERSION_REVISION) _T(".0.0")
#define MAC_VER_FILE_VERSION_FULL_NO_DOT APE_VERSION_MAJOR APE_VERSION_REVISION

#define MAC_FILE_VERSION_NUMBER                         3990
#define MAC_VERSION_STRING                              MAC_VER_FILE_VERSION_STR
#define MAC_VERSION_NUMBER                              MAC_VERSION_MAJOR MAC_VERSION_REVISION
#define MAC_NAME                                        _T("Monkey's Audio ") MAC_VER_FILE_VERSION_STR
#define PLUGIN_NAME                                     "Monkey's Audio Player " MAC_VER_FILE_VERSION_STR_NARROW
#define MJ_PLUGIN_NAME                                  _T("APE Plugin (v") MAC_VER_FILE_VERSION_STR _T(")")
#define MAC_RESOURCE_VERSION_COMMA                      MAC_VERSION_MAJOR, MAC_VERSION_REVISION, 0, 0
#define MAC_RESOURCE_VERSION_STRING                     MAC_VER_FILE_VERSION_STR_FULL
#define MAC_RESOURCE_COPYRIGHT                          "Copyright (c) 2000-" STRINGIZE(MAC_YEAR) " Matthew T. Ashland"
#define CONSOLE_NAME                                    _T("--- Monkey's Audio Console Front End (v ") MAC_VER_FILE_VERSION_STR _T(") (c) Matthew T. Ashland ---\n")
#define PLUGIN_ABOUT                                    _T("Monkey's Audio Player v") MAC_VER_FILE_VERSION_STR _T("\nCopyrighted (c) 2000-") STRINGIZE(MAC_YEAR) _T(" by Matthew T. Ashland")
#define MAC_DLL_INTERFACE_VERSION_NUMBER                1000

/*****************************************************************************************
Global compiler settings (useful for porting)
*****************************************************************************************/
#ifdef _MSC_VER
    #pragma warning(disable: 4100)
#endif

// assembly code (helps performance, but limits portability)
#if !defined(PLATFORM_ARM) && !defined(PLATFORM_ANDROID)
    #if defined __SSE2__ || _M_IX86_FP == 2 || defined _M_X64
        #define ENABLE_SSE_ASSEMBLY
    #elif defined(_M_IX86)
        #define ENABLE_SSE_ASSEMBLY
    #endif
    #ifdef _MSC_VER // doesn't compile in gcc
        #if defined(_M_IX86)
            #define ENABLE_MMX_ASSEMBLY
        #endif
    #endif
#endif

// APE_BACKWARDS_COMPATIBILITY is only needed for decoding APE 3.92 or earlier files.  It
// has not been possible to make these files for over 10 years, so it's unlikely
// that disabling APE_BACKWARDS_COMPATIBILITY would have any effect on a normal user.  For
// porting or third party usage, it's probably best to not bother with APE_BACKWARDS_COMPATIBILITY.
// A future release of Monkey's Audio itself may remove support for these obsolete files.
#if !defined(PLATFORM_ANDROID)
    #define APE_BACKWARDS_COMPATIBILITY
#endif

// compression modes
#define ENABLE_COMPRESSION_MODE_FAST
#define ENABLE_COMPRESSION_MODE_NORMAL
#define ENABLE_COMPRESSION_MODE_HIGH
#define ENABLE_COMPRESSION_MODE_EXTRA_HIGH

/*****************************************************************************************
Global types
*****************************************************************************************/
namespace APE
{
    // integer types
    typedef uint64_t                                    uint64;
    typedef uint32_t                                    uint32;
    typedef uint16_t                                    uint16;
    typedef uint8_t                                     uint8;
    
    typedef int64_t                                     int64;
    typedef int32_t                                     int32;
    typedef int16_t                                     int16;
    typedef int8_t                                      int8;

    typedef intptr_t                                    intn; // native integer, can safely hold a pointer
    typedef uintptr_t                                   uintn;

    // string types
    typedef char                                        str_ansi;
    typedef unsigned char                               str_utf8;
    typedef int16                                       str_utf16;
    typedef wchar_t                                     str_utfn; // could be UTF-16 or UTF-32 depending on platform
}

/*****************************************************************************************
Global macros
*****************************************************************************************/
#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#define APE_TRUNCATE ((size_t)-1)

#if defined(PLATFORM_WINDOWS)
    #define IO_USE_WIN_FILE_IO
    #define IO_HEADER_FILE                              "WinFileIO.h"
    #define IO_CLASS_NAME                               CWinFileIO
    #define DLLEXPORT                                   __declspec(dllexport)
    #define SLEEP(MILLISECONDS)                         ::Sleep(MILLISECONDS)
    #define MESSAGEBOX(PARENT, TEXT, CAPTION, TYPE)     ::MessageBox(PARENT, TEXT, CAPTION, TYPE)
    #define PUMP_MESSAGE_LOOP                           { MSG Msg; while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) != 0) { TranslateMessage(&Msg); DispatchMessage(&Msg); } }
    #define ODS                                         OutputDebugString
    #define TICK_COUNT_TYPE                             unsigned long
    #define TICK_COUNT_READ(VARIABLE)                   VARIABLE = GetTickCount()
    #define TICK_COUNT_FREQ                             1000

    #if !defined(ASSERT)
        #if defined(_DEBUG)
            #define ASSERT(e)                            assert(e)
        #else
            #define ASSERT(e)                            
        #endif
    #endif
#else
    #define IO_USE_STD_LIB_FILE_IO
    #define IO_HEADER_FILE                              "StdLibFileIO.h"
    #define IO_CLASS_NAME                               CStdLibFileIO
    #define DLLEXPORT
    #define SLEEP(MILLISECONDS)                         { struct timespec t; t.tv_sec = (MILLISECONDS) / 1000; t.tv_nsec = (MILLISECONDS) % 1000 * 1000000; nanosleep(&t, NULL); }
    #define MESSAGEBOX(PARENT, TEXT, CAPTION, TYPE)
    #define PUMP_MESSAGE_LOOP
    #undef    ODS
    #define ODS                                         printf
    #define TICK_COUNT_FREQ                             1000000
    #undef    ASSERT
    #define ASSERT(e)
    #define wcsncpy_s(A, B, C, D) wcsncpy(A, C, D)
    #define wcscpy_s(A, B, C) wcscpy(A, C)
    #define wcscat_s(A, B, C) wcscat(A, C)
    #define sprintf_s(A, B, C, D) sprintf(A, C, D)
    #define strcpy_s(A, B, C) strcpy(A, C)
    #define _tcscat_s(A, B, C) _tcscat(A, C)
#endif

/*****************************************************************************************
WAVE format descriptor (binary compatible with Windows define, but in the APE namespace)
*****************************************************************************************/
namespace APE
{
#pragma pack(push, 1)
    typedef struct tWAVEFORMATEX
    {
        WORD        wFormatTag;         /* format type */
        WORD        nChannels;          /* number of channels (i.e. mono, stereo...) */
        uint32      nSamplesPerSec;     /* sample rate */
        uint32      nAvgBytesPerSec;    /* for buffer estimation */
        WORD        nBlockAlign;        /* block size of data */
        WORD        wBitsPerSample;     /* number of bits per sample of mono data */
        WORD        cbSize;             /* the count in bytes of the size of */
        /* extra information (after cbSize) */
    } WAVEFORMATEX, *PWAVEFORMATEX, NEAR *NPWAVEFORMATEX, FAR *LPWAVEFORMATEX;
#pragma pack(pop)
}

/*****************************************************************************************
Global defines
*****************************************************************************************/
#define ONE_MILLION                                     1000000
#ifdef PLATFORM_WINDOWS
    #define APE_FILENAME_SLASH '\\'
#else
    #define APE_FILENAME_SLASH '/'
#endif

/*****************************************************************************************
Byte order
*****************************************************************************************/
#define __LITTLE_ENDIAN     1234
#define __BIG_ENDIAN        4321
#define __BYTE_ORDER        __LITTLE_ENDIAN

/*****************************************************************************************
Macros
*****************************************************************************************/
#define MB(TEST) MESSAGEBOX(NULL, TEST, _T("Information"), MB_OK);
#define MBN(NUMBER) { TCHAR cNumber[16]; _stprintf(cNumber, _T("%d"), NUMBER); MESSAGEBOX(NULL, cNumber, _T("Information"), MB_OK); }

#define SAFE_DELETE(POINTER) if (POINTER) { delete POINTER; POINTER = NULL; }
#define SAFE_ARRAY_DELETE(POINTER) if (POINTER) { delete [] POINTER; POINTER = NULL; }
#define SAFE_VOID_CLASS_DELETE(POINTER, Class) { Class *pClass = (Class *) POINTER; if (pClass) { delete pClass; POINTER = NULL; } }
#define SAFE_FILE_CLOSE(HANDLE) if (HANDLE != INVALID_HANDLE_VALUE) { CloseHandle(HANDLE); HANDLE = INVALID_HANDLE_VALUE; }

#define ODN(NUMBER) { TCHAR cNumber[16]; _stprintf(cNumber, _T("%d\n"), int(NUMBER)); ODS(cNumber); }

#define CATCH_ERRORS(CODE) try { CODE } catch(...) { }

#define RETURN_ON_ERROR(FUNCTION) {    int nFunctionResult = FUNCTION; if (nFunctionResult != 0) { return nFunctionResult; } }
#define RETURN_VALUE_ON_ERROR(FUNCTION, VALUE) { int nFunctionResult = FUNCTION; if (nFunctionResult != 0) { return VALUE; } }
#define RETURN_ON_EXCEPTION(CODE, VALUE) { try { CODE } catch(...) { return VALUE; } }

#define THROW_ON_ERROR(CODE) { intn nThrowResult = (intn) CODE; if (nThrowResult != 0) throw(nThrowResult); }

#define EXPAND_1_TIMES(CODE) CODE
#define EXPAND_2_TIMES(CODE) CODE CODE
#define EXPAND_3_TIMES(CODE) CODE CODE CODE
#define EXPAND_4_TIMES(CODE) CODE CODE CODE CODE
#define EXPAND_5_TIMES(CODE) CODE CODE CODE CODE CODE
#define EXPAND_6_TIMES(CODE) CODE CODE CODE CODE CODE CODE
#define EXPAND_7_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_8_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_9_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_12_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_14_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_15_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_16_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_30_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_31_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_32_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_64_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
#define EXPAND_N_TIMES(NUMBER, CODE) EXPAND_##NUMBER##_TIMES(CODE)

#define UNROLL_4_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3)
#define UNROLL_8_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7)
#define UNROLL_15_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7) MACRO(8) MACRO(9) MACRO(10) MACRO(11) MACRO(12) MACRO(13) MACRO(14)
#define UNROLL_16_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7) MACRO(8) MACRO(9) MACRO(10) MACRO(11) MACRO(12) MACRO(13) MACRO(14) MACRO(15)
#define UNROLL_64_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7) MACRO(8) MACRO(9) MACRO(10) MACRO(11) MACRO(12) MACRO(13) MACRO(14) MACRO(15) MACRO(16) MACRO(17) MACRO(18) MACRO(19) MACRO(20) MACRO(21) MACRO(22) MACRO(23) MACRO(24) MACRO(25) MACRO(26) MACRO(27) MACRO(28) MACRO(29) MACRO(30) MACRO(31) MACRO(32) MACRO(33) MACRO(34) MACRO(35) MACRO(36) MACRO(37) MACRO(38) MACRO(39) MACRO(40) MACRO(41) MACRO(42) MACRO(43) MACRO(44) MACRO(45) MACRO(46) MACRO(47) MACRO(48) MACRO(49) MACRO(50) MACRO(51) MACRO(52) MACRO(53) MACRO(54) MACRO(55) MACRO(56) MACRO(57) MACRO(58) MACRO(59) MACRO(60) MACRO(61) MACRO(62) MACRO(63)
#define UNROLL_128_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7) MACRO(8) MACRO(9) MACRO(10) MACRO(11) MACRO(12) MACRO(13) MACRO(14) MACRO(15) MACRO(16) MACRO(17) MACRO(18) MACRO(19) MACRO(20) MACRO(21) MACRO(22) MACRO(23) MACRO(24) MACRO(25) MACRO(26) MACRO(27) MACRO(28) MACRO(29) MACRO(30) MACRO(31) MACRO(32) MACRO(33) MACRO(34) MACRO(35) MACRO(36) MACRO(37) MACRO(38) MACRO(39) MACRO(40) MACRO(41) MACRO(42) MACRO(43) MACRO(44) MACRO(45) MACRO(46) MACRO(47) MACRO(48) MACRO(49) MACRO(50) MACRO(51) MACRO(52) MACRO(53) MACRO(54) MACRO(55) MACRO(56) MACRO(57) MACRO(58) MACRO(59) MACRO(60) MACRO(61) MACRO(62) MACRO(63) MACRO(64) MACRO(65) MACRO(66) MACRO(67) MACRO(68) MACRO(69) MACRO(70) MACRO(71) MACRO(72) MACRO(73) MACRO(74) MACRO(75) MACRO(76) MACRO(77) MACRO(78) MACRO(79) MACRO(80) MACRO(81) MACRO(82) MACRO(83) MACRO(84) MACRO(85) MACRO(86) MACRO(87) MACRO(88) MACRO(89) MACRO(90) MACRO(91) MACRO(92) MACRO(93) MACRO(94) MACRO(95) MACRO(96) MACRO(97) MACRO(98) MACRO(99) MACRO(100) MACRO(101) MACRO(102) MACRO(103) MACRO(104) MACRO(105) MACRO(106) MACRO(107) MACRO(108) MACRO(109) MACRO(110) MACRO(111) MACRO(112) MACRO(113) MACRO(114) MACRO(115) MACRO(116) MACRO(117) MACRO(118) MACRO(119) MACRO(120) MACRO(121) MACRO(122) MACRO(123) MACRO(124) MACRO(125) MACRO(126) MACRO(127)
#define UNROLL_256_TIMES(MACRO) MACRO(0) MACRO(1) MACRO(2) MACRO(3) MACRO(4) MACRO(5) MACRO(6) MACRO(7) MACRO(8) MACRO(9) MACRO(10) MACRO(11) MACRO(12) MACRO(13) MACRO(14) MACRO(15) MACRO(16) MACRO(17) MACRO(18) MACRO(19) MACRO(20) MACRO(21) MACRO(22) MACRO(23) MACRO(24) MACRO(25) MACRO(26) MACRO(27) MACRO(28) MACRO(29) MACRO(30) MACRO(31) MACRO(32) MACRO(33) MACRO(34) MACRO(35) MACRO(36) MACRO(37) MACRO(38) MACRO(39) MACRO(40) MACRO(41) MACRO(42) MACRO(43) MACRO(44) MACRO(45) MACRO(46) MACRO(47) MACRO(48) MACRO(49) MACRO(50) MACRO(51) MACRO(52) MACRO(53) MACRO(54) MACRO(55) MACRO(56) MACRO(57) MACRO(58) MACRO(59) MACRO(60) MACRO(61) MACRO(62) MACRO(63) MACRO(64) MACRO(65) MACRO(66) MACRO(67) MACRO(68) MACRO(69) MACRO(70) MACRO(71) MACRO(72) MACRO(73) MACRO(74) MACRO(75) MACRO(76) MACRO(77) MACRO(78) MACRO(79) MACRO(80) MACRO(81) MACRO(82) MACRO(83) MACRO(84) MACRO(85) MACRO(86) MACRO(87) MACRO(88) MACRO(89) MACRO(90) MACRO(91) MACRO(92) MACRO(93) MACRO(94) MACRO(95) MACRO(96) MACRO(97) MACRO(98) MACRO(99) MACRO(100) MACRO(101) MACRO(102) MACRO(103) MACRO(104) MACRO(105) MACRO(106) MACRO(107) MACRO(108) MACRO(109) MACRO(110) MACRO(111) MACRO(112) MACRO(113) MACRO(114) MACRO(115) MACRO(116) MACRO(117) MACRO(118) MACRO(119) MACRO(120) MACRO(121) MACRO(122) MACRO(123) MACRO(124) MACRO(125) MACRO(126) MACRO(127)    \
    MACRO(128) MACRO(129) MACRO(130) MACRO(131) MACRO(132) MACRO(133) MACRO(134) MACRO(135) MACRO(136) MACRO(137) MACRO(138) MACRO(139) MACRO(140) MACRO(141) MACRO(142) MACRO(143) MACRO(144) MACRO(145) MACRO(146) MACRO(147) MACRO(148) MACRO(149) MACRO(150) MACRO(151) MACRO(152) MACRO(153) MACRO(154) MACRO(155) MACRO(156) MACRO(157) MACRO(158) MACRO(159) MACRO(160) MACRO(161) MACRO(162) MACRO(163) MACRO(164) MACRO(165) MACRO(166) MACRO(167) MACRO(168) MACRO(169) MACRO(170) MACRO(171) MACRO(172) MACRO(173) MACRO(174) MACRO(175) MACRO(176) MACRO(177) MACRO(178) MACRO(179) MACRO(180) MACRO(181) MACRO(182) MACRO(183) MACRO(184) MACRO(185) MACRO(186) MACRO(187) MACRO(188) MACRO(189) MACRO(190) MACRO(191) MACRO(192) MACRO(193) MACRO(194) MACRO(195) MACRO(196) MACRO(197) MACRO(198) MACRO(199) MACRO(200) MACRO(201) MACRO(202) MACRO(203) MACRO(204) MACRO(205) MACRO(206) MACRO(207) MACRO(208) MACRO(209) MACRO(210) MACRO(211) MACRO(212) MACRO(213) MACRO(214) MACRO(215) MACRO(216) MACRO(217) MACRO(218) MACRO(219) MACRO(220) MACRO(221) MACRO(222) MACRO(223) MACRO(224) MACRO(225) MACRO(226) MACRO(227) MACRO(228) MACRO(229) MACRO(230) MACRO(231) MACRO(232) MACRO(233) MACRO(234) MACRO(235) MACRO(236) MACRO(237) MACRO(238) MACRO(239) MACRO(240) MACRO(241) MACRO(242) MACRO(243) MACRO(244) MACRO(245) MACRO(246) MACRO(247) MACRO(248) MACRO(249) MACRO(250) MACRO(251) MACRO(252) MACRO(253) MACRO(254) MACRO(255)

/*****************************************************************************************
Error Codes
*****************************************************************************************/

// success
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS                                   0
#endif

// file and i/o errors (1000's)
#define ERROR_IO_READ                                   1000
#define ERROR_IO_WRITE                                  1001
#define ERROR_INVALID_INPUT_FILE                        1002
#define ERROR_INVALID_OUTPUT_FILE                       1003
#define ERROR_INPUT_FILE_TOO_LARGE                      1004
#define ERROR_INPUT_FILE_UNSUPPORTED_BIT_DEPTH          1005
#define ERROR_INPUT_FILE_UNSUPPORTED_SAMPLE_RATE        1006
#define ERROR_INPUT_FILE_UNSUPPORTED_CHANNEL_COUNT      1007
#define ERROR_INPUT_FILE_TOO_SMALL                      1008
#define ERROR_INVALID_CHECKSUM                          1009
#define ERROR_DECOMPRESSING_FRAME                       1010
#define ERROR_INITIALIZING_UNMAC                        1011
#define ERROR_INVALID_FUNCTION_PARAMETER                1012
#define ERROR_UNSUPPORTED_FILE_TYPE                     1013
#define ERROR_UNSUPPORTED_FILE_VERSION                  1014

// memory errors (2000's)
#define ERROR_INSUFFICIENT_MEMORY                       2000

// dll errors (3000's)
#define ERROR_LOADINGAPE_DLL                            3000
#define ERROR_LOADINGAPE_INFO_DLL                       3001
#define ERROR_LOADING_UNMAC_DLL                         3002

// general and misc errors
#define ERROR_USER_STOPPED_PROCESSING                   4000
#define ERROR_SKIPPED                                   4001

// programmer errors
#define ERROR_BAD_PARAMETER                             5000

// IAPECompress errors
#define ERROR_APE_COMPRESS_TOO_MUCH_DATA                6000

// unknown error
#define ERROR_UNDEFINED                                -1
