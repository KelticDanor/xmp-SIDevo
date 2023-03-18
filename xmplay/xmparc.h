// XMPlay archive plugin header
// new plugins can be submitted to plugins@xmplay.com

#pragma once

#include "xmpfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMPARC_FACE
#define XMPARC_FACE 0 // "face"
#endif

/*
	Simultaneous calls of the XMPARC functions are possible, so some synchronization (eg. CRITICAL_SECTION)
	is needed if that'll be a problem.

	Returned data will be freed by XMPlay and should be allocated via XMPFUNC_MISC functions.
*/

#define XMPARC_FLAG_CONFIG		1 // has About/Config functions

typedef struct {
	DWORD flags; // XMPARC_FLAG_xxx
	const char *exts; // supported file extensions (description\0ext1/ext2/etc...)
	BOOL (WINAPI *CheckFile)(XMPFILE file); // verify file (basic checks, ie. don't parse the entire file)
	char *(WINAPI *GetFileList)(XMPFILE file); // get file list (series of NULL-terminated entries)
	void *(WINAPI *DecompressFile)(XMPFILE file, const char *entry, DWORD *len); // extract file (len=in:wanted/out:delivered)

	void (WINAPI *About)(HWND win); // (OPTIONAL)
	void (WINAPI *Config)(HWND win); // present config options to user (OPTIONAL)
} XMPARC;

#ifdef __cplusplus
}
#endif
