/*
**  $Id: dfbvideo.h 7347 2007-08-16 03:58:56Z xgwang $
**  
**  Copyright (C) 2005 ~ 2007 Feynman Software.
**
**  All rights reserved by Feynman Software.
**
**  This is the NEWGAL engine runs on DirectFB.
**
**  Author: Wei Yongming (2005-12-15)
**
*/

#ifndef _GAL_dfbvideo_h
#define _GAL_dfbvideo_h

#include "sysvideo.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	GAL_VideoDevice *this

/* Private display data */

struct GAL_PrivateVideoData {
    int w, h;
    void *framebuffer;
};

#endif /* _GAL_dfbvideo_h */
