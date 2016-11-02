/*
**  $Id: dfbvideo.c 7347 2007-08-16 03:58:56Z xgwang $
**  
**  Copyright (C) 2005 ~ 2007 Feynman Software.
**
**  All rights reserved by Feynman Software.
**
**  This is the NEWGAL engine runs on DirectFB.
**
**  Author: Wei Yongming (2005-12-15)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "newgal.h"
#include "sysvideo.h"
#include "pixels_c.h"

#ifdef _NEWGAL_ENGINE_DFB

#include <directfb.h>

#include "dfbvideo.h"

#define DFBVID_DRIVER_NAME "dfb"

/* The root interface of DirectFB. */
IDirectFB *__mg_dfb = NULL;

/* Initialization/Query functions */
static int DFB_VideoInit(_THIS, GAL_PixelFormat *vformat);
static GAL_Rect **DFB_ListModes(_THIS, GAL_PixelFormat *format, Uint32 flags);
static GAL_Surface *DFB_SetVideoMode(_THIS, GAL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int DFB_SetColors(_THIS, int firstcolor, int ncolors, GAL_Color *colors);
static void DFB_VideoQuit(_THIS);

/* Hardware surface functions */
static int DFB_AllocHWSurface(_THIS, GAL_Surface *surface);
static void DFB_FreeHWSurface(_THIS, GAL_Surface *surface);

/* DFB driver bootstrap functions */

static int DFB_Available(void)
{
    return(1);
}

static void DFB_DeleteDevice(GAL_VideoDevice *device)
{
    free(device->hidden);
    free(device);
}

static GAL_VideoDevice *DFB_CreateDevice (int devindex)
{
    GAL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (GAL_VideoDevice *)malloc (sizeof(GAL_VideoDevice));
    if (device) {
        memset (device, 0, (sizeof *device));
        device->hidden = (struct GAL_PrivateVideoData *)
                malloc ((sizeof *device->hidden));
    }
    if ((device == NULL) || (device->hidden == NULL)) {
        GAL_OutOfMemory ();
        if (device) {
            free (device);
        }

        return 0;
    }

    memset (device->hidden, 0, (sizeof *device->hidden));

    /* Set the function pointers */
    device->VideoInit = DFB_VideoInit;
    device->ListModes = DFB_ListModes;
    device->SetVideoMode = DFB_SetVideoMode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = DFB_SetColors;
    device->VideoQuit = DFB_VideoQuit;
#ifdef _LITE_VERSION
    device->RequestHWSurface = NULL;
#endif
    device->AllocHWSurface = DFB_AllocHWSurface;
    device->CheckHWBlit = NULL;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
    device->FreeHWSurface = DFB_FreeHWSurface;

    device->free = DFB_DeleteDevice;

    return device;
}

VideoBootStrap DFB_bootstrap = {
    DFBVID_DRIVER_NAME, "Video driver on DirectFB",
    DFB_Available, DFB_CreateDevice
};

static int DFB_VideoInit(_THIS, GAL_PixelFormat *vformat)
{
    /* The frame buffer surface, to which we write graphics. */
    IDirectFBSurface *pFrameBuffer = NULL;

    DFBSurfacePixelFormat pixelformat;

    /* The description of the frame buffer surface to create. */
    DFBSurfaceDescription surfaceDesc;

    fprintf (stderr, "NEWGAL>DFB: Calling DirectFBInit...\n");

    /* Initialise DirectFB, passing command line options.
     * Options recognised by DirectFB will be stripped. */
    if (DirectFBInit (NULL, NULL) != DFB_OK)
        goto init_err;

    /* Create the DirectFB root interface. */
    fprintf (stderr, "NEWGAL>DFB: Calling DirectFBCreate...\n");
    if (DirectFBCreate (&__mg_dfb) != DFB_OK)
        goto init_err;

    /* Use full screen mode so that a surface has full control of a layer
     * and no windows are created. */
    if (__mg_dfb->SetCooperativeLevel (__mg_dfb, DFSCL_FULLSCREEN) != DFB_OK)
        goto init_err;

    /* Set the surface description - specify which fields are set and set them. */
    surfaceDesc.flags = DSDESC_CAPS;
    surfaceDesc.caps  = DSCAPS_PRIMARY | DSCAPS_STATIC_ALLOC;

    fprintf (stderr, "NEWGAL>DFB: Creating the primary surface...\n");
    /* Create the frame buffer primary surface by passing our surface description. */
    if (__mg_dfb->CreateSurface (__mg_dfb, &surfaceDesc, &pFrameBuffer) != DFB_OK)
        goto init_err;

    this->hidden->framebuffer = pFrameBuffer;

    /* Determine the screen depth and pixel format */
    pFrameBuffer->GetPixelFormat (pFrameBuffer, &pixelformat);

    switch (pixelformat) {
        case DSPF_RGB32:
            vformat->BitsPerPixel = 32;
            vformat->BytesPerPixel = 4;
            break;
        default:
            fprintf (stderr, "NEWGAL>DFB: Unsupported pixel format\n");
            vformat->BitsPerPixel = 8;
            vformat->BytesPerPixel = 1;
            break;
    }

    fprintf (stderr, "NEWGAL>DFB: Initialized\n");

    /* We're done! */
    return 0;

init_err:
    return -1;
}

static GAL_Rect standard_mode = {0, 0, 640, 480};
static GAL_Rect* modes []  = {
    &standard_mode,
    NULL
};

static GAL_Rect **DFB_ListModes (_THIS, GAL_PixelFormat *format, Uint32 flags)
{
    if (format->BitsPerPixel == 32 || format->BitsPerPixel == 16) {
        return modes;
    }
    
    return NULL;
}

static GAL_Surface *DFB_SetVideoMode(_THIS, GAL_Surface *current,
                int width, int height, int bpp, Uint32 flags)
{
    int depth, Rmask, Gmask, Bmask, Amask = 0;
    DFBSurfacePixelFormat pixelformat;
    IDirectFBSurface *pFrameBuffer = this->hidden->framebuffer;

#if 0
    __mg_dfb->SetVideoMode (__mg_dfb, width, height, 32);
#endif

    pFrameBuffer->GetSize (pFrameBuffer, &current->w, &current->h);

    pFrameBuffer->GetPixelFormat (pFrameBuffer, &pixelformat);

    switch (pixelformat) {
        case DSPF_ARGB1555:
            depth = 16;
            Amask = 0x8000;
            Rmask = 0x7C00;
            Gmask = 0x03E0;
            Bmask = 0x001F;
            break;

        case DSPF_RGB16:
            depth = 16;
            Rmask = 0xF800;
            Gmask = 0x07E0;
            Bmask = 0x001F;
            break;

        case DSPF_RGB24:
            depth = 24;
            Rmask = 0x00FF0000;
            Gmask = 0x0000FF00;
            Bmask = 0x000000FF;
            break;

        case DSPF_RGB32:
            depth = 32;
            Rmask = 0x00FF0000;
            Gmask = 0x0000FF00;
            Bmask = 0x000000FF;
            break;

        case DSPF_ARGB:
            depth = 32;
            Amask = 0xFF000000;
            Rmask = 0x00FF0000;
            Gmask = 0x0000FF00;
            Bmask = 0x000000FF;
            break;

        case DSPF_ARGB2554:
            depth = 16;
            Amask = 0xC000;
            Rmask = 0x3E00;
            Gmask = 0x01F0;
            Bmask = 0x000F;
            break;

        case DSPF_ARGB4444:
            depth = 16;
            Amask = 0xF000;
            Rmask = 0x0F00;
            Gmask = 0x00F0;
            Bmask = 0x000F;
            break;

        default:
            fprintf (stderr, "NEWGAL>DFB: Unsupported pixel format: %x\n", pixelformat);
            return (NULL);
    }

    fprintf (stderr, "NEWGAL>DFB: the pixel format: %x\n", pixelformat);

    /* Allocate the new pixel format for the screen */
    if (!GAL_ReallocFormat (current, depth, Rmask, Gmask, Bmask, Amask)) {
        fprintf (stderr, "NEWGAL>DFB: "
                "Couldn't allocate new pixel format for requested mode\n");
        return (NULL);
    }

    /* Set up the new mode framebuffer */
    current->flags = flags & GAL_FULLSCREEN;
    this->hidden->w = current->w;
    this->hidden->h = current->h;

    pFrameBuffer->Lock (pFrameBuffer, DSLF_READ, &current->pixels, &current->pitch);

    fprintf (stderr, "NEWGAL>DFB: "
                "Address of framebuffer: %p, pitch: %d\n", current->pixels, current->pitch);

    pFrameBuffer->Unlock (pFrameBuffer);

    /* We're done */
    return (current);
}

/* We don't actually allow hardware surfaces other than the main one */
static int DFB_AllocHWSurface(_THIS, GAL_Surface *surface)
{
    return (-1);
}
static void DFB_FreeHWSurface(_THIS, GAL_Surface *surface)
{
    surface->pixels = NULL;
}

static int DFB_SetColors(_THIS, int firstcolor, int ncolors, GAL_Color *colors)
{
    /* do nothing of note. */
    return (1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another video routine -- notably UpdateRects.
*/
static void DFB_VideoQuit (_THIS)
{
    IDirectFBSurface *pFrameBuffer = this->hidden->framebuffer;

    pFrameBuffer->Release (pFrameBuffer);
    __mg_dfb->Release (__mg_dfb);
}

#endif /* _NEWGAL_ENGINE_DFB */

