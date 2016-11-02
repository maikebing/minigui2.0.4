/*
** $Id: desktop-sa.c 7339 2007-08-16 03:47:29Z xgwang $
**
** desktop-sa.c: The desktop for MiniGUI-Standalone
**
** Copyright (C) 2002 ~ 2007 Feynman Software.
**
** Current maintainer: Wei Yongming.
**
** Derived from desktop-lite.c (2005/08/15)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#if defined (_LITE_VERSION) && defined (_STAND_ALONE)

#include "minigui.h"
#include "gdi.h"
#include "window.h"
#include "cliprect.h"
#include "gal.h"
#include "internals.h"
#include "ctrlclass.h"
#include "menu.h"
#include "timer.h"
#include "dc.h"
#include "icon.h"
#include "sharedres.h"
#include "misc.h"

#include <sys/termios.h>

/******************************* global data *********************************/
RECT g_rcScr;

HWND __mg_capture_wnd;
HWND __mg_ime_wnd;
MSGQUEUE __mg_desktop_msg_queue;
PMSGQUEUE __mg_dsk_msg_queue = &__mg_desktop_msg_queue;
PMAINWIN __mg_active_mainwnd;
PTRACKMENUINFO __mg_ptmi;

BOOL __mg_switch_away; // always be zero for clients.

/********************* Window management support *****************************/
static BLOCKHEAP sg_FreeInvRectList;
static BLOCKHEAP sg_FreeClipRectList;
static ZORDERINFO sg_MainWinZOrder;
static ZORDERINFO sg_TopMostWinZOrder;

static HWND sg_hCaretWnd;
static UINT sg_uCaretBTime;

static GCRINFO sg_ScrGCRInfo;

static BOOL InitWndManagementInfo (void)
{
    if (!InitMainWinMetrics())
        return FALSE;

    __mg_capture_wnd = 0;
    __mg_active_mainwnd = NULL;

    __mg_ptmi = NULL;

    __mg_ime_wnd = 0;
    sg_hCaretWnd = 0;

    return TRUE;
}

static void InitZOrderInfo (PZORDERINFO pZOrderInfo, HWND hHost);

#include "desktop-comm.c"

BOOL InitDesktop (void)
{
    /*
     * Init ZOrderInfo here.
     * FIXME, HWND_DESKTOP
     */
    InitZOrderInfo (&sg_MainWinZOrder, HWND_DESKTOP);
    InitZOrderInfo (&sg_TopMostWinZOrder, HWND_DESKTOP);
    
    /*
     * Init heap of clipping rects.
     */
    InitFreeClipRectList (&sg_FreeClipRectList, SIZE_CLIPRECTHEAP);

    /*
     * Init heap of invalid rects.
     */
    InitFreeClipRectList (&sg_FreeInvRectList, SIZE_INVRECTHEAP);

    // Init Window Management information.
    if (!InitWndManagementInfo ())
        return FALSE;

    init_desktop_win ();

    /*
     * Load system resource here.
     */
    if (!InitSystemRes ()) {
        fprintf (stderr, "DESKTOP: Can not initialize system resource!\n");
        return FALSE;
    }

    InitClipRgn (&sg_ScrGCRInfo.crgn, &sg_FreeClipRectList);
    SetClipRgn (&sg_ScrGCRInfo.crgn, &g_rcScr);
    sg_ScrGCRInfo.age = 0;

    SendMessage (HWND_DESKTOP, MSG_STARTSESSION, 0, 0);
    SendMessage (HWND_DESKTOP, MSG_ERASEDESKTOP, 0, 0);

    return TRUE;
}

#endif /* _LITE_VERSION && _STAND_ALONE */
