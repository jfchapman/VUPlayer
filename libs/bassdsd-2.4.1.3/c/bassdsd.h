/*
	BASSDSD 2.4 C/C++ header file
	Copyright (c) 2014-2017 Un4seen Developments Ltd.

	See the BASSDSD.CHM file for more detailed documentation
*/

#ifndef BASSDSD_H
#define BASSDSD_H

#include "bass.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSDSD versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSDSDDEF
#define BASSDSDDEF(f) WINAPI f
#else
#define NOBASSDSDOVERLOADS
#endif

// Additional BASS_SetConfig options
#define BASS_CONFIG_DSD_FREQ		0x10800
#define BASS_CONFIG_DSD_GAIN		0x10801

// Additional BASS_DSD_StreamCreateFile/etc flags
#define BASS_DSD_RAW				0x200
#define BASS_DSD_DOP				0x400
#define BASS_DSD_DOP_AA				0x800

// BASS_CHANNELINFO type
#define BASS_CTYPE_STREAM_DSD		0x11700

// Additional tag types
#define BASS_TAG_DSD_ARTIST			0x13000 // DSDIFF artist : ASCII string
#define BASS_TAG_DSD_TITLE			0x13001 // DSDIFF title : ASCII string
#define BASS_TAG_DSD_COMMENT		0x13100 // + index, DSDIFF comment : TAG_DSD_COMMENT structure

#pragma pack(push,1)
typedef struct {
	WORD timeStampYear;		// creation year
	BYTE timeStampMonth;	// creation month
	BYTE timeStampDay;		// creation day
	BYTE timeStampHour;		// creation hour
	BYTE timeStampMinutes;	// creation minutes
	WORD cmtType;			// comment type
	WORD cmtRef;			// comment reference
	DWORD count;			// string length
#if __GNUC__ && __GNUC__<3
	char commentText[0];	// text
#else
	char commentText[];		// text
#endif
} TAG_DSD_COMMENT;
#pragma pack(pop)

// Additional attributes
#define BASS_ATTRIB_DSD_GAIN		0x14000
#define BASS_ATTRIB_DSD_RATE		0x14001

HSTREAM BASSDSDDEF(BASS_DSD_StreamCreateFile)(BOOL mem, const void *file, QWORD offset, QWORD length, DWORD flags, DWORD freq);
HSTREAM BASSDSDDEF(BASS_DSD_StreamCreateURL)(const char *url, DWORD offset, DWORD flags, DOWNLOADPROC *proc, void *user, DWORD freq);
HSTREAM BASSDSDDEF(BASS_DSD_StreamCreateFileUser)(DWORD system, DWORD flags, const BASS_FILEPROCS *procs, void *user, DWORD freq);

#ifdef __cplusplus
}

#if defined(_WIN32) && !defined(NOBASSDSDOVERLOADS)
static inline HSTREAM BASS_DSD_StreamCreateFile(BOOL mem, const WCHAR *file, QWORD offset, QWORD length, DWORD flags, DWORD freq)
{
	return BASS_DSD_StreamCreateFile(mem, (const void*)file, offset, length, flags|BASS_UNICODE, freq);
}

static inline HSTREAM BASS_DSD_StreamCreateURL(const WCHAR *url, DWORD offset, DWORD flags, DOWNLOADPROC *proc, void *user, DWORD freq)
{
	return BASS_DSD_StreamCreateURL((const char*)url, offset, flags|BASS_UNICODE, proc, user, freq);
}
#endif
#endif

#endif
