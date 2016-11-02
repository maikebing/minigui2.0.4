/*
** $Id: hi3510_fbvideo.h 8055 2007-11-07 08:26:03Z lbjiao $
**
** hi3510_fbvideo.h: header file for hi3510 framebuffer video
**
** Copyright (C) 2007 Feynman Software.
**
** All rights reserved by Feynman Software.
*/
#ifndef _GAL_fbvideo_h
#define _GAL_fbvideo_h

#include <sys/types.h>
#include <linux/fb.h>
#include "sysvideo.h"

#define GAL_mutexP(lock)    lock++;
#define GAL_mutexV(lock)    lock--;
#define GAL_DestroyMutex(lock)  lock=0;

#define _USE_DBUF 1
#define _USE_2D_ACCEL 1
#define _TAP_ALL 1
#if (defined(_TAP_ALL) || defined(_TAP3_VERT_HORI) || defined(_TAP2_VERT)  \
     || defined(_TAP2_VERT_HORI)|| defined(_TAP3_VERT))  \
     && defined(_USE_2D_ACCEL) && defined(_USE_DBUF) 
     
#define TAP_MIN_VALUE   1
#define TAP_2_VERT      1    /*2-tap vertical antiflicker*/
#define TAP_2_VERT_HORI 2    /*2-tap vertical and horizontal antiflicker*/
#define TAP_3_VERT      3    /*3-tap vertical antiflicker*/
#define TAP_3_VERT_HORI 4    /*3-tap vertical and horizontal antiflicker*/
#define TAP_MAX_VALUE   4   
#endif

#define _THIS    GAL_VideoDevice *this

/* This is the structure we use to keep track of video memory */
typedef struct vidmem_bucket {
    struct vidmem_bucket *prev;
    int used;
    Uint8 *base;
    unsigned int size;
    struct vidmem_bucket *next;
} vidmem_bucket;

/* Private display data */
struct GAL_PrivateVideoData {
    int console_fd1;
    struct fb_fix_screeninfo finfo;
    unsigned char *mapped_mem;
    int mapped_memlen;
    int mapped_offset;
    unsigned char *mapped_io;
    long mapped_iolen;
  
    vidmem_bucket surfaces;
    int surfaces_memtotal;
    int surfaces_memleft;

    GAL_mutex *hw_lock;

    void (*wait_vbl)(_THIS);
    void (*wait_idle)(_THIS);
#if (defined(_TAP_ALL) || defined(_TAP3_VERT_HORI) || defined(_TAP2_VERT) \
     || defined(_TAP2_VERT_HORI)|| defined(_TAP3_VERT)) \
     && defined(_USE_2D_ACCEL) && defined(_USE_DBUF) 
    /*these item record physics addr, 
      for antiflicker.*/
    Uint32 uiMiddleBuf1;
    Uint32 uiMiddleBuf2;
    Uint32 uiMiddleBuf3;
    Uint32 uiCurrentBuf;
    Uint32 uiShadowBuf;

    /*antiflicker level
      TAP_2_VERT       2 tap vertical antiflicker
      TAP_2_VERT_HORI  2 tap average antiflicker
      TAP_3_VERT       3 tap vertical antiflicker
      TAP_4_VERT_HORI  3 tap average antiflicker
      other denote don't antiflicker
    */
    Uint32 uiTapLevel;
#endif
};

#ifdef _LITE_VERSION
#define console_fd         (((struct GAL_PrivateVideoData*)(this->gamma))->console_fd1)
#define saved_finfo        (((struct GAL_PrivateVideoData*)(this->gamma))->finfo)
#define saved_cmaplen      (((struct GAL_PrivateVideoData*)(this->gamma))->saved_cmaplen)
#define saved_cmap         (((struct GAL_PrivateVideoData*)(this->gamma))->saved_cmap)
#define mapped_mem         (((struct GAL_PrivateVideoData*)(this->gamma))->mapped_mem)
#define mapped_memlen      (((struct GAL_PrivateVideoData*)(this->gamma))->mapped_memlen)
#define mapped_offset      (((struct GAL_PrivateVideoData*)(this->gamma))->mapped_offset)
#define mapped_io          (((struct GAL_PrivateVideoData*)(this->gamma))->mapped_io)
#define mapped_iolen       (((struct GAL_PrivateVideoData*)(this->gamma))->mapped_iolen)
#define surfaces           (((struct GAL_PrivateVideoData*)(this->gamma))->surfaces)
#define surfaces_memtotal  (((struct GAL_PrivateVideoData*)(this->gamma))->surfaces_memtotal)
#define surfaces_memleft   (((struct GAL_PrivateVideoData*)(this->gamma))->surfaces_memleft)
#define hw_lock            (((struct GAL_PrivateVideoData*)(this->gamma))->hw_lock)
#define wait_vbl           (((struct GAL_PrivateVideoData*)(this->gamma))->wait_vbl)
#define wait_idle          (((struct GAL_PrivateVideoData*)(this->gamma))->wait_idle)
#define middle_buf1        (((struct GAL_PrivateVideoData*)(this->gamma))->uiMiddleBuf1)
#define middle_buf2        (((struct GAL_PrivateVideoData*)(this->gamma))->uiMiddleBuf2)
#define middle_buf3        (((struct GAL_PrivateVideoData*)(this->gamma))->uiMiddleBuf3)
#define current_buf        (((struct GAL_PrivateVideoData*)(this->gamma))->uiCurrentBuf)
#define shadow_buf         (((struct GAL_PrivateVideoData*)(this->gamma))->uiShadowBuf)
#define tap_level          (((struct GAL_PrivateVideoData*)(this->gamma))->uiTapLevel)
BOOL FB_CreateDevice(GAL_VideoDevice *device, const Sint8* pszLayerName);
#else
#define console_fd         (g_stHidden.console_fd1)
#define saved_finfo        (g_stHidden.finfo)
#define saved_cmaplen      (g_stHidden.saved_cmaplen)
#define saved_cmap         (g_stHidden.saved_cmap)
#define mapped_mem         (g_stHidden.mapped_mem)
#define mapped_memlen      (g_stHidden.mapped_memlen)
#define mapped_offset      (g_stHidden.mapped_offset)
#define mapped_io          (g_stHidden.mapped_io)
#define mapped_iolen       (g_stHidden.mapped_iolen)
#define surfaces           (g_stHidden.surfaces)
#define surfaces_memtotal  (g_stHidden.surfaces_memtotal)
#define surfaces_memleft   (g_stHidden.surfaces_memleft)
#define hw_lock            (g_stHidden.hw_lock)
#define wait_vbl           (g_stHidden.wait_vbl)
#define wait_idle          (g_stHidden.wait_idle)
#define middle_buf1        (g_stHidden.uiMiddleBuf1)
#define middle_buf2        (g_stHidden.uiMiddleBuf2)
#define middle_buf3        (g_stHidden.uiMiddleBuf3)
#define current_buf        (g_stHidden.uiCurrentBuf)
#define shadow_buf         (g_stHidden.uiShadowBuf)
#define tap_level          (g_stHidden.uiTapLevel)
BOOL FB_CreateDevice(GAL_VideoDevice *device);
#endif

#endif /* _GAL_hi3510_fbvideo_h */

