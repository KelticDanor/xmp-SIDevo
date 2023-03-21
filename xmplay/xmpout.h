// XMPlay output plugin header
// new plugins can be submitted to plugins@xmplay.com

#pragma once

#include "xmpfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMPOUT_FACE
#define XMPOUT_FACE 0 // "face"
#endif

#define XMPOUT_OUTPUT_VOLUME	1 // XMPlay shouldn't apply volume/balance
#define XMPOUT_OUTPUT_NOAMP		2 // XMPlay shouldn't apply amplification
#define XMPOUT_OUTPUT_FILE		4 // file writer (limits looping and lowers thread priority)
#define XMPOUT_OUTPUT_NOBALANCE	8 // XMPlay should apply balance (overrides XMPOUT_OUTPUT_VOLUME)

typedef struct {
	XMPFORMAT form;	// sample format
	DWORD buffer;	// buffer length (in samples)
} XMPOUT_FORMAT;

typedef struct {
	DWORD flags; // unused (set to 0)
	const char *name; // plugin name

	DLGPROC Options; // options page handler (dialog in resource 1000, OPTIONAL)

	const char *(WINAPI *GetName)(DWORD output); // get output name
	DWORD (WINAPI *GetFlags)(DWORD output); // get output flags (XMPOUT_OUTPUT_xxx)
	BOOL (WINAPI *Open)(DWORD output, XMPOUT_FORMAT *form, HANDLE event); // start output
	void (WINAPI *Close)(); // close output
	BOOL (WINAPI *Reset)(); // reset
	BOOL (WINAPI *Pause)(BOOL resume); // pause/resume
	void (WINAPI *SetVolume)(float volume, float balance); // set volume & balance (OPTIONAL)
	DWORD (WINAPI *CanWrite)(); // get amount of data (in bytes) that can be written
	BOOL (WINAPI *Write)(const void *buf, DWORD length); // write data (length in bytes)
	DWORD (WINAPI *GetBuffered)(); // get amount of buffered data (bytes) yet to play

	void (WINAPI *GetGeneralInfo)(char *buf); // get General info window text (OPTIONAL)
	void (WINAPI *NewTrack)(const char *file); // new track (OPTIONAL)
} XMPOUT;

#define XMPFUNC_OUT_FACE		10

typedef struct {
	char *(WINAPI *GetFilename)(const char *defext); // get filename to write to disk
	BOOL (WINAPI *ReOpen)(); // reinitialize output
} XMPFUNC_OUT;

#ifdef __cplusplus
}
#endif
