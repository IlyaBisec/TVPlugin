#define _CRT_SECURE_NO_WARNINGS

// Sadochkov_Ilya 15/11/2024
// TVPlugin - updating the plugin for reading vorbis, 
// theora & ogg formats for new systems

//#ifdef THEOVORB_EXPORTS
//#define  __declspec(dllexport)
//#else
//#define  __declspec(dllimport)
//#endif THEOVORB_API

#ifndef _TV_PLUGIN_H_
#define _TV_PLUGIN_H_

#include <windows.h>
#include <d3d9.h>
#include "dsound.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dsound.lib")

#include <theoradec.h>
#include <vorbisfile.h>
#include <ogg.h>

#define TV_FAIL 0
#define TV_OK 1
#define TV_EOF 2

#define TV_VOLUME_MIN 0
#define TV_VOLUME_MAX 100

#define TV_COLOR_FORMAT_RGB 1
#define TV_COLOR_FORMAT_ARGB 2
#define TV_COLOR_FORMAT_RGBA 3

typedef unsigned char TV_VOLUME;
typedef char TV_PANNING;

// Structures audio & video contexts
typedef struct TV_HANDLE
{
	OggVorbis_File vorbisFile;

	th_dec_ctx *theoraDecoder;				// Pointer to theora decorder
	th_setup_info *theoraSetup;				// Pointer to setting of theora decorder for init
	th_ycbcr_buffer ycbcrBuffer;

	FILE *file;

	ogg_sync_state oggSyncState;			// Input buffer control
	ogg_page oggPage;						// 1 Page store
	ogg_stream_state theoraStreamState;		// Stream state
	ogg_stream_state vorbisStreamState;

	LPDIRECTSOUND				ds;
	LPDIRECTSOUNDBUFFER dsBuffer;
	LPDIRECT3DTEXTURE9		d3dTexture;
	LPDIRECT3DDEVICE9		d3dDevice;
} TV_HANDLE;

TV_HANDLE *tv_handle = NULL;

typedef TV_HANDLE *TV_AH;
typedef TV_HANDLE *TV_VH;


// Initializes theovorb library, allocates memory, creates helper threads, DirectSound buffers, etc
int tv_Init();

// Closes theovorb library, frees memory, destroys helper threads, DirectSound buffers, etc
int tv_Done();

// Opens sound for playing
// The handle returned can be used to pause and stop sound, to set volume and panning.
// Resources are not closed automatically, consider using tv_StreamClose() function
TV_AH tv_StreamOpen(const char *pcszFileName);

// Returns length of the stream in seconds
int tv_StreamLength(TV_AH tv_ah_handle);

// Starts playing stream
void tv_StreamPlay(TV_AH tv_ah_handle);

// Stops sound started with tv_StreamOpen() and releases resources
void tv_StreamClose(TV_AH tv_ah_handle);

// Returns TRUE when the stream is fineshed, otherwise FALSE
BYTE tv_StreamFinished(TV_AH tv_ah_handle);

// Sets volume [0..100] for the given stream
void tv_StreamSetVolume(TV_AH tv_ah_handle, TV_VOLUME tv_volume);

// Opens video file and returns handle
TV_VH tv_VideoOpen(const char *pcszFileName);

// Closes video handle and releases resources
int tv_VideoClose(TV_VH tv_vh_handle);

// Returns TRUE when the stream is fineshed, otherwise FALSE
int tv_VideoFinished(TV_VH tv_vh_handle);

// Plays video opened with tv_VideoOpen()
// pRect = NULL for full screen video
void tv_VideoPlay(TV_VH tv_vh_handle, BOOL bLooped, RECT *pRect);

// Resizes video overlay window played with tv_VideoPlay()
// pRect = NULL for full screen video
void tv_VideoResize(TV_VH tv_vh_handle, RECT *pRect);

#endif // _TV_PLUGIN_H_
