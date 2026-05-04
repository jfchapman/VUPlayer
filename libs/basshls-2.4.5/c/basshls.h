/*
	BASSHLS 2.4 C/C++ header file
	Copyright (c) 2015-2025 Un4seen Developments Ltd.

	See the BASSHLS.CHM file for more detailed documentation
*/

#ifndef BASSHLS_H
#define BASSHLS_H

#include "bass.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSHLS versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSHLSDEF
#define BASSHLSDEF(f) WINAPI f
#else
#define NOBASSHLSOVERLOADS
#endif

// Additional BASS_SetConfig options
#define BASS_CONFIG_HLS_DOWNLOAD_TAGS	0x10900
#define BASS_CONFIG_HLS_BANDWIDTH		0x10901
#define BASS_CONFIG_HLS_DELAY			0x10902
#define BASS_CONFIG_HLS_TSSCAN			0x10903

// Additional sync type
#define BASS_SYNC_HLS_SEGMENT	0x10300
#define BASS_SYNC_HLS_SDT		0x10301
#define BASS_SYNC_HLS_EMSG		0x10302

// Additional tag types
#define BASS_TAG_HLS_EXTINF			0x14000 // segment's EXTINF tag : UTF-8 string
#define BASS_TAG_HLS_STREAMINF		0x14001 // EXT-X-STREAM-INF tag : UTF-8 string
#define BASS_TAG_HLS_DATE			0x14002 // EXT-X-PROGRAM-DATE-TIME tag : UTF-8 string
#define BASS_TAG_HLS_SDT			0x14003 // DVB SDT : variable length block
#define BASS_TAG_HLS_EMSG			0x14004 // fMP4 emsg : variable length block
#define BASS_TAG_HLS_SDT_BINARY		0x14005 // DVB SDT : TAB_BINARY
#define BASS_TAG_HLS_EMSG_BINARY	0x14006 // fMP4 emsg : TAB_BINARY

// Additional BASS_StreamGetFilePosition mode
#define BASS_FILEPOS_HLS_SEGMENT	0x10000	// segment sequence number

HSTREAM BASSHLSDEF(BASS_HLS_StreamCreateFile)(DWORD filetype, const void *file, QWORD offset, QWORD length, DWORD flags);
HSTREAM BASSHLSDEF(BASS_HLS_StreamCreateURL)(const char *url, DWORD flags, DOWNLOADPROC *proc, void *user);

#ifdef __cplusplus
}

#if defined(_WIN32) && !defined(NOBASSHLSOVERLOADS)
static inline HSTREAM BASS_HLS_StreamCreateFile(DWORD filetype, const WCHAR *file, QWORD offset, QWORD length, DWORD flags)
{
	return BASS_HLS_StreamCreateFile(filetype, (const void*)file, offset, length, flags | BASS_UNICODE);
}

static inline HSTREAM BASS_HLS_StreamCreateURL(const WCHAR *url, DWORD flags, DOWNLOADPROC *proc, void *user)
{
	return BASS_HLS_StreamCreateURL((const char*)url, flags | BASS_UNICODE, proc, user);
}
#endif
#endif

#endif
