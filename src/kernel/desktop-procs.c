/*
** $Id: desktop-procs.c 7861 2007-10-17 07:11:59Z xwyan $
**
** desktop-procs.c: The desktop procedures for MiniGUI-Processes
**
** Copyright (C) 2002 ~ 2007 Feynman Software.
**
** Current maintainer: Wei Yongming.
**
** Derived from original desktop-lite.c (2005/08/16)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#if defined (_LITE_VERSION) && !defined (_STAND_ALONE)

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
#include "ourhdr.h"
#include "client.h"
#include "server.h"
#include "drawsemop.h"

/******************************* global data *********************************/

RECT g_rcScr;

ZORDERINFO* __mg_zorder_info;

MSGQUEUE __mg_desktop_msg_queue;
PMSGQUEUE __mg_dsk_msg_queue = &__mg_desktop_msg_queue;

GHANDLE __mg_layer;

/* pointer to desktop window */
PMAINWIN __mg_dsk_win;

/* handle to desktop window */
HWND __mg_hwnd_desktop;

/* the capture window */
HWND __mg_capture_wnd;

/* handle to the ime window - server only. */
HWND __mg_ime_wnd;

/* always be zero for clients. */
BOOL __mg_switch_away;
/*default window procedure*/
WNDPROC __mg_def_proc[3];
/***************** Initialization/Termination routines ***********************/

static HWND sg_hCaretWnd;
static UINT sg_uCaretBTime;
static GCRINFO sg_ScrGCRInfo;
static CLIPRGN sg_UpdateRgn;

static PTRACKMENUINFO sg_ptmi;

static BOOL InitWndManagementInfo (void)
{
    if (!InitMainWinMetrics())
        return FALSE;

    __mg_capture_wnd = 0;

    __mg_ime_wnd = 0;

    sg_ptmi = NULL;

    sg_hCaretWnd = 0;

    return TRUE;
}

PBITMAP SystemBitmap [SYSBMP_ITEM_NUMBER];
HICON  LargeSystemIcon [SYSICO_ITEM_NUMBER];
HICON  SmallSystemIcon [SYSICO_ITEM_NUMBER];

static void init_system_bitmap (int id)
{
    int i;
    void *temp;
	int bpp;

    bpp = BYTESPERPHYPIXEL;
    temp   = mgSharedRes + ((PG_RES)mgSharedRes)->bmpoffset;
    if (id < ((PG_RES)mgSharedRes)->bmpnum) {

        if (!(SystemBitmap [id] = (PBITMAP) malloc (sizeof (BITMAP))))
            return;

        for (i = 0; i < id; i++)
		    temp += sizeof(BITMAP)
                        + ((PBITMAP)temp)->bmPitch * ((PBITMAP)temp)->bmHeight;

		SystemBitmap[id]->bmWidth  = ((PBITMAP)temp)->bmWidth;
		SystemBitmap[id]->bmHeight = ((PBITMAP)temp)->bmHeight;

		SystemBitmap[id]->bmType = BMP_TYPE_NORMAL;
		SystemBitmap[id]->bmBitsPerPixel = bpp << 3;
		SystemBitmap[id]->bmBytesPerPixel = bpp;
		SystemBitmap[id]->bmPitch  = ((PBITMAP)temp)->bmPitch;
		SystemBitmap[id]->bmColorKey = ((PBITMAP)temp)->bmColorKey;

		SystemBitmap[id]->bmBits = temp + sizeof(BITMAP);
    }
}

PBITMAP GUIAPI GetSystemBitmap (int id)
{
    if (id >= SYSBMP_ITEM_NUMBER || id < 0)
        return NULL;

    if (SystemBitmap [id] == NULL)
        init_system_bitmap (id);

    return SystemBitmap [id];
}

static void init_large_system_icon (int id)
{
    void *temp;
	int bpp;

    bpp = BYTESPERPHYPIXEL;
	temp = mgSharedRes + ((PG_RES)mgSharedRes)->iconoffset;

    if (id < ((PG_RES)mgSharedRes)->iconnum) {
        if (!(LargeSystemIcon[id] = (HICON)malloc (sizeof(ICON))))
            return;

		temp += (sizeof(ICON) * 2 + (2*32*32 + 2*16*16)*bpp) * id;
        ((PICON)LargeSystemIcon[id])->width   = ((PICON)temp)->width;
        ((PICON)LargeSystemIcon[id])->height  = ((PICON)temp)->height;
#ifdef _USE_NEWGAL
        ((PICON)LargeSystemIcon[id])->pitch   = ((PICON)temp)->pitch;
#endif
        ((PICON)LargeSystemIcon[id])->AndBits = temp + sizeof(ICON);
        ((PICON)LargeSystemIcon[id])->XorBits = temp 
                + sizeof(ICON) + bpp*32*32;
    }
}

static void init_small_system_icon (int id)
{
    void *temp;
	int bpp;

    bpp = BYTESPERPHYPIXEL;
	temp = mgSharedRes + ((PG_RES)mgSharedRes)->iconoffset;

    if (id < ((PG_RES)mgSharedRes)->iconnum) {
        if (!(SmallSystemIcon[id] = (HICON)malloc (sizeof(ICON))))
            return;

		temp += (sizeof(ICON)*2 + (2*32*32 + 2*16*16)*bpp) * id;
		temp += sizeof(ICON) + 2*bpp*32*32;
        ((PICON)SmallSystemIcon[id])->width   = ((PICON)temp)->width;
        ((PICON)SmallSystemIcon[id])->height  = ((PICON)temp)->height;
#ifdef _USE_NEWGAL
        ((PICON)SmallSystemIcon[id])->pitch   = ((PICON)temp)->pitch;
#endif
        ((PICON)SmallSystemIcon[id])->AndBits = temp + sizeof(ICON);
        ((PICON)SmallSystemIcon[id])->XorBits = temp 
                + sizeof(ICON) + bpp*16*16;
    }
}

HICON GUIAPI GetLargeSystemIcon (int id)
{
    if (id >= SYSICO_ITEM_NUMBER || id < 0)
        return 0;

    if (LargeSystemIcon [id] == 0)
        init_large_system_icon (id);

    return LargeSystemIcon [id];
}

HICON GUIAPI GetSmallSystemIcon (int id)
{
    if (id >= SYSICO_ITEM_NUMBER || id < 0)
        return 0;

    if (SmallSystemIcon [id] == 0)
        init_small_system_icon (id);

    return SmallSystemIcon [id];
}

static void TerminateSharedSysRes (void)
{
    int i;
    
    for (i=0; i<SYSBMP_ITEM_NUMBER; i++) {
        if (SystemBitmap [i]) {
            free (SystemBitmap [i]);
        }
    }

    for (i=0; i<SYSICO_ITEM_NUMBER; i++) {
        if (SmallSystemIcon [i])
            free ((PICON)SmallSystemIcon[i]);

        if (LargeSystemIcon [i])
            free ((PICON)LargeSystemIcon[i]);
    }
}

static void init_desktop_win (void)
{
    static MAINWIN desktop_win;
    PMAINWIN pDesktopWin;

    pDesktopWin = &desktop_win;

    pDesktopWin->pMessages         = __mg_dsk_msg_queue;
    pDesktopWin->MainWindowProc    = DesktopWinProc;

    pDesktopWin->DataType          = TYPE_HWND;
    pDesktopWin->WinType           = TYPE_ROOTWIN;

    pDesktopWin->pLogFont          = NULL;
    pDesktopWin->spCaption         = NULL;
    pDesktopWin->iBkColor          = 0;

    if (mgIsServer) {
        pDesktopWin->pGCRInfo      = &sg_ScrGCRInfo;
        pDesktopWin->idx_znode     = 0;
    }

    if (mgIsServer)
        pDesktopWin->spCaption = "THE DESKTOP OF THE SERVER";
    else
        pDesktopWin->spCaption = "THE VIRTUAL DESKTOP OF CLIENT";

    pDesktopWin->pMainWin          = pDesktopWin;

    __mg_hwnd_desktop = (HWND)pDesktopWin;
    __mg_dsk_win  = pDesktopWin;

}

static BLOCKHEAP sgFreeInvRectList;
static BLOCKHEAP sgFreeClipRectList;

BOOL InitDesktop (void)
{
    /*
     * Init heap of clipping rects.
     */
    InitFreeClipRectList (&sgFreeClipRectList, SIZE_CLIPRECTHEAP);

    /*
     * Init heap of invalid rects.
     */
    InitFreeClipRectList (&sgFreeInvRectList, SIZE_INVRECTHEAP);

    // Init Window Management information.
    if (!InitWndManagementInfo ())
        return FALSE;

    init_desktop_win ();

    return TRUE;
}

extern void TerminateStockBitmap (void);
void TerminateDesktop (void)
{
    TerminateSharedSysRes ();
    TerminateStockBitmap ();

    DestroyFreeClipRectList (&sgFreeClipRectList);
    DestroyFreeClipRectList (&sgFreeInvRectList);
}

PGCRINFO GetGCRgnInfo (HWND hWnd)
{
    return ((PMAINWIN)hWnd)->pGCRInfo;
}

static void dskScreenToClient (PMAINWIN pWin, 
                const RECT* rcScreen, RECT* rcClient)
{
    PCONTROL pParent;

    rcClient->top = rcScreen->top - pWin->ct;
    rcClient->left = rcScreen->left - pWin->cl;
    rcClient->right = rcScreen->right - pWin->cl;
    rcClient->bottom = rcScreen->bottom - pWin->ct;

    pParent = (PCONTROL) pWin;
    while ((pParent = pParent->pParent)) {
        rcClient->top -= pParent->ct;
        rcClient->left -= pParent->cl;
        rcClient->right -= pParent->cl;
        rcClient->bottom -= pParent->ct;
    }
}

static void dskScreenToWindow (PMAINWIN pWin, 
                const RECT* rcScreen, RECT* rcWindow)
{
    PCONTROL pParent;

    rcWindow->top = rcScreen->top - pWin->top;
    rcWindow->left = rcScreen->left - pWin->left;
    rcWindow->right = rcScreen->right - pWin->left;
    rcWindow->bottom = rcScreen->bottom - pWin->top;

    pParent = (PCONTROL) pWin;
    while ((pParent = pParent->pParent)) {
        rcWindow->top -= pParent->ct;
        rcWindow->left -= pParent->cl;
        rcWindow->right -= pParent->cl;
        rcWindow->bottom -= pParent->ct;
    }
}

static void dskClientToScreen (PMAINWIN pWin, 
                const RECT* rcClient, RECT* rcScreen)
{
    PCONTROL pParent;

    rcScreen->top = rcClient->top + pWin->ct;
    rcScreen->left = rcClient->left + pWin->cl;
    rcScreen->right = rcClient->right + pWin->cl;
    rcScreen->bottom = rcClient->bottom + pWin->ct;

    pParent = (PCONTROL) pWin;
    while ((pParent = pParent->pParent)) {
        rcScreen->top += pParent->ct;
        rcScreen->left += pParent->cl;
        rcScreen->right += pParent->cl;
        rcScreen->bottom += pParent->ct;
    }
}

#if 0
static void dskWindowToScreen (PMAINWIN pWin, 
                const RECT* rcWindow, RECT* rcScreen)
{
    PCONTROL pParent;

    rcScreen->top = rcWindow->top + pWin->top;
    rcScreen->left = rcWindow->left + pWin->left;
    rcScreen->right = rcWindow->right + pWin->left;
    rcScreen->bottom = rcWindow->bottom + pWin->top;

    pParent = (PCONTROL) pWin;
    while ((pParent = pParent->pParent)) {
        rcScreen->top += pParent->ct;
        rcScreen->left += pParent->cl;
        rcScreen->right += pParent->cl;
        rcScreen->bottom += pParent->ct;
    }
}

static void dskGetClientRectInScreen (PMAINWIN pWin, RECT* prc)
{
    PCONTROL pCtrl;
    PCONTROL pParent;
    pParent = pCtrl = (PCONTROL) pWin;

    prc->left = pCtrl->cl;
    prc->top  = pCtrl->ct;
    prc->right = pCtrl->cr;
    prc->bottom = pCtrl->cb;
    while ((pParent = pParent->pParent)) {
        prc->left += pParent->cl;
        prc->top  += pParent->ct;
        prc->right += pParent->cl;
        prc->bottom += pParent->ct;
    }
}

#endif

static void dskGetWindowRectInScreen (PMAINWIN pWin, RECT* prc)
{
    PCONTROL pParent;
    PCONTROL pCtrl;

    pParent = pCtrl = (PCONTROL)pWin;

    prc->left = pCtrl->left;
    prc->top  = pCtrl->top;
    prc->right = pCtrl->right;
    prc->bottom = pCtrl->bottom;
    while ((pParent = pParent->pParent)) {
        prc->left += pParent->cl;
        prc->top  += pParent->ct;
        prc->right += pParent->cl;
        prc->bottom += pParent->ct;
    }
}

/********************** Common routines for ZORDERINFO ***********************/
#define GET_ZORDER_NODES(zorder)    \
        ((ZORDERNODE*)((unsigned char*)((zorder) + 1) + \
                       (zorder)->size_usage_bmp))

#define GET_ZORDER_USAGE_BMP(zorder) (unsigned char *)((zorder)+1)

#define ZT_GLOBAL       0x00000001
#define ZT_TOPMOST      0x00000002
#define ZT_NORMAL       0x00000004
#define ZT_ALL          0x0000000F

typedef BOOL (* CB_ONE_ZNODE) (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node);


static int do_for_all_znodes (void* context, const ZORDERINFO* zi, 
                CB_ONE_ZNODE cb, DWORD types)
{
    int slot;
    int count = 0;

    ZORDERNODE* nodes = (ZORDERNODE*) ((char*)zi + sizeof (ZORDERINFO)
                    + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (types & ZT_GLOBAL) {
        slot = zi->first_global;
        for (; slot > 0; slot = nodes [slot].next) {
            if (cb (context, zi, nodes + slot))
                count ++;
        }
    }

    if (types & ZT_TOPMOST) {
        slot = zi->first_topmost;
        for (; slot > 0; slot = nodes [slot].next) {
            if (cb (context, zi, nodes + slot))
                count ++;
        }
    }

    if (types & ZT_NORMAL) {
        slot = zi->first_normal;
        for (; slot > 0; slot = nodes [slot].next) {
            if (cb (context, zi, nodes + slot))
                count ++;
        }
    }

    return count;
}
 
static void lock_zi_for_change (const ZORDERINFO* zi)
{
    int clients = 0;
    struct sembuf sb;

    clients = zi->max_nr_popupmenus + zi->max_nr_globals 
            + zi->max_nr_topmosts + zi->max_nr_normals;
    
    /* Cancel the current drag and drop operation */
    __mg_do_drag_drop_window (MSG_IDLE, 0, 0);

again:
    sb.sem_num = zi->zi_semnum;
    sb.sem_op = -clients;
    sb.sem_flg = SEM_UNDO;

    if (semop (zi->zi_semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            goto again;
        }
    }
}

static void unlock_zi_for_change (const ZORDERINFO* zi)
{
    int clients = 0;
    struct sembuf sb;
    clients = zi->max_nr_popupmenus + zi->max_nr_globals 
            + zi->max_nr_topmosts + zi->max_nr_normals;
    
again:
    sb.sem_num = zi->zi_semnum;
    sb.sem_op = clients;
    sb.sem_flg = SEM_UNDO;

    if (semop (zi->zi_semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            goto again;
        }
    }
}

static void lock_zi_for_read (const ZORDERINFO* zi)
{
    struct sembuf sb;
    
    if (mgIsServer) return;

again:
    sb.sem_num = zi->zi_semnum;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;

    if (semop (zi->zi_semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            goto again;
        }
    }

}

static void unlock_zi_for_read (const ZORDERINFO* zi)
{
    struct sembuf sb;
    
    if (mgIsServer) return;

again:
    sb.sem_num = zi->zi_semnum;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;

    if (semop (zi->zi_semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            goto again;
        }
    }
}

int __mg_post_msg_by_znode (const ZORDERINFO* zi, int znode, 
                int message, WPARAM wParam, LPARAM lParam)
{
    int ret;
    ZORDERNODE* nodes;

    if (znode < 0)
        return -1;

    nodes = (ZORDERNODE*) ((char*)(zi + 1)
                    + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (nodes [znode].cli == 0) {
        ret = PostMessage (nodes [znode].main_win, message, wParam, lParam);
    }
    else {
        MSG msg = {nodes [znode].main_win, 
                message, wParam, lParam, __mg_timer_counter};

        ret = __mg_send2client (&msg, mgClients + nodes [znode].cli);
    }

    return ret;
}

static int post_msg_by_znode_p (const ZORDERINFO* zi, const ZORDERNODE* znode, 
                int message, WPARAM wParam, LPARAM lParam)
{
    int ret;

    if (znode->cli == 0) {
        ret = PostMessage (znode->main_win, message, wParam, lParam);
    }
    else {
        MSG msg = {znode->main_win, 
                message, wParam, lParam, __mg_timer_counter};

        ret = __mg_send2client (&msg, mgClients + znode->cli);
    }

    return ret;
}

void __mg_update_window (HWND hwnd, int left, int top, int right, int bottom)
{
    PMAINWIN pWin = (PMAINWIN)hwnd;
    RECT invrc = {left, top, right, bottom};

    if (pWin->WinType != TYPE_CONTROL 
                    && pWin->dwStyle & WS_VISIBLE) {
        if (IsRectEmpty (&invrc)) {
            SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);
            InvalidateRect ((HWND)pWin, NULL, TRUE);
        }
        else {
            RECT rcTemp, rcInv;

            if (IntersectRect (&rcTemp, 
                        (RECT*)(&pWin->left), &invrc)) {
                dskScreenToWindow (pWin, &rcTemp, &rcInv);
                SendAsyncMessage ((HWND)pWin, 
                                MSG_NCPAINT, 0, (LPARAM)(&rcInv));
                dskScreenToClient (pWin, &rcTemp, &rcInv);
                InvalidateRect ((HWND)pWin, &rcInv, TRUE);
            }
        }
    }
}

static int update_client_window (ZORDERNODE* znode, const RECT* rc)
{

    if (!mgIsServer)
        return -1;

    if (znode->cli == 0) {
        if (rc)
            __mg_update_window (znode->hwnd, rc->left, rc->top, 
                            rc->right, rc->bottom);
        else
            __mg_update_window (znode->hwnd, 0, 0, 0, 0);
    }
    else {

        if (rc) {

            if (IsRectEmpty (&znode->dirty_rc)){
                SetRect(&znode->dirty_rc, rc->left, rc->top, rc->right, rc->bottom);  
            }
            else{
                GetBoundRect (&znode->dirty_rc, &znode->dirty_rc, rc);    
            }
        }
        mgClients [znode->cli].has_dirty = TRUE;
    }

    return 0;
}

static int update_client_window_rgn (int cli, HWND hwnd)
{
    int ret = 0;
    CLIPRECT* crc = sg_UpdateRgn.head;

    if (!mgIsServer)
        return -1;

    if (cli == 0) {
        while (crc) {
            __mg_update_window (hwnd, crc->rc.left, crc->rc.top, 
                            crc->rc.right, crc->rc.bottom);

            crc = crc->next;
        }
    }
    else {
        MSG msg = {hwnd, MSG_UPDATECLIWIN, 0, 0, __mg_timer_counter};

        while (crc) {

            msg.wParam = MAKELONG (crc->rc.left, crc->rc.top);
            msg.lParam = MAKELONG (crc->rc.right, crc->rc.bottom);

            ret = __mg_send2client (&msg, mgClients + cli);
            if (ret < 0)
                break;

            crc = crc->next;
        }
    }

    if (ret < 0)
        return -1;

    return 0;
}

static void reset_window (PMAINWIN pWin, const RECT* rcWin)
{
    PGCRINFO pGCRInfo;
    RECT rcTemp;

    pGCRInfo = pWin->pGCRInfo;
    IntersectRect (&rcTemp, rcWin, &g_rcScr);
    SetClipRgn (&pGCRInfo->crgn, &rcTemp);
}

static BOOL _cb_recalc_gcrinfo (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    PGCRINFO gcrinfo;

    if (!(node->flags & ZOF_VISIBLE))
        return FALSE;

    gcrinfo = (PGCRINFO)context;
    SubtractClipRect (&gcrinfo->crgn, &node->rc);

    return TRUE;
}

void __mg_lock_recalc_gcrinfo (PDC pdc)
{
    PGCRINFO gcrinfo;
    PMAINWIN mainwin;
    ZORDERINFO* zi = __mg_zorder_info;
    ZORDERNODE* menu_nodes, *nodes;
    int i, slot = 0, idx_znode;

    gcrinfo = GetGCRgnInfo (pdc->hwnd);
    mainwin = (PMAINWIN)(pdc->hwnd);

    lock_zi_for_read (zi);

    menu_nodes = (ZORDERNODE*) ((char*)(zi + 1)+ zi->size_usage_bmp);
    nodes = menu_nodes + DEF_NR_POPUPMENUS;

    idx_znode = mainwin->idx_znode;

    if (gcrinfo->old_zi_age == nodes [idx_znode].age) {
        return;
    }

    reset_window (mainwin, &nodes [idx_znode].rc);

    /* clip by popup menus */
    for (i = 0; i < zi->nr_popupmenus; i++) {
        SubtractClipRect (&gcrinfo->crgn, &menu_nodes [i].rc);
    }

    switch (nodes[idx_znode].flags & ZOF_TYPE_MASK) {
        case ZOF_TYPE_NORMAL:
            do_for_all_znodes (gcrinfo, zi, 
                            _cb_recalc_gcrinfo, ZT_GLOBAL);
            do_for_all_znodes (gcrinfo, zi, 
                            _cb_recalc_gcrinfo, ZT_TOPMOST);
            slot = zi->first_normal;
            break;
        case ZOF_TYPE_TOPMOST:
            do_for_all_znodes (gcrinfo, zi, 
                            _cb_recalc_gcrinfo, ZT_GLOBAL);
            slot = zi->first_topmost;
            break;
        case ZOF_TYPE_GLOBAL:
            slot = zi->first_global;
            break;
        case ZOF_TYPE_DESKTOP:
            do_for_all_znodes (gcrinfo, zi, 
                            _cb_recalc_gcrinfo, ZT_ALL);
            break;
        default:
            break;
    }

    while (slot) {
        if (slot == idx_znode) {
            break;
        }

        if (nodes [slot].flags & ZOF_VISIBLE) {
            SubtractClipRect (&gcrinfo->crgn, &nodes [slot].rc);
        }

        slot = nodes [slot].next;
    }

    gcrinfo->old_zi_age = nodes [idx_znode].age;
    gcrinfo->age ++;
}

void __mg_unlock_gcrinfo (PDC pdc)
{
    unlock_zi_for_read (__mg_zorder_info);
    return;
}

/*********************** Client-side routines ********************************/

void __mg_start_client_desktop (void)
{
    SendMessage (HWND_DESKTOP, MSG_STARTSESSION, 0, 0);
}

static DWORD get_znode_flags_from_style (PMAINWIN pWin)
{
    DWORD zt_type = 0;

    if (mgIsServer) {
       zt_type |= ZOF_TYPE_GLOBAL;
    } else {
        if (pWin->dwExStyle & WS_EX_TOPMOST)
            zt_type |= ZOF_TYPE_TOPMOST;
        else if (pWin->WinType == TYPE_CONTROL && 
                    (pWin->pMainWin->dwExStyle & WS_EX_TOPMOST))
            zt_type |= ZOF_TYPE_TOPMOST;
        else
            zt_type |= ZOF_TYPE_NORMAL;
    }

    if (pWin->dwStyle & WS_VISIBLE)
        zt_type |= ZOF_VISIBLE;
    if (pWin->dwStyle & WS_DISABLED)
        zt_type |= ZOF_DISABLED;

    if (pWin->WinType == TYPE_MAINWIN)
        zt_type |= ZOF_TF_MAINWIN;

    if (pWin->dwExStyle & WS_EX_TOOLWINDOW)
        zt_type |= ZOF_TF_TOOLWIN;

    return zt_type;
}

static int cliAllocZOrderNode (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_ALLOC;
    info.flags = get_znode_flags_from_style (pWin);
    info.hwnd = (HWND)pWin;
    info.main_win = (HWND)pWin->pMainWin;
    
    if (pWin->spCaption) {
        if (strlen (pWin->spCaption) <= MAX_CAPTION_LEN) {
            strcpy (info.caption, pWin->spCaption);
        } else {
            memcpy (info.caption, pWin->spCaption, MAX_CAPTION_LEN);
            info.caption[MAX_CAPTION_LEN] = '\0';
        }
    }
    
    dskGetWindowRectInScreen (pWin, &info.rc);

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliFreeZOrderNode (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_FREE;
    info.idx_znode = pWin->idx_znode;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliMove2Top (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_MOVE2TOP;
    info.idx_znode = pWin->idx_znode;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliShowWindow (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_SHOW;
    info.idx_znode = pWin->idx_znode;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliHideWindow (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_HIDE;
    info.idx_znode = pWin->idx_znode;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliMoveWindow (PMAINWIN pWin, const RECT* rcWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_MOVEWIN;
    info.idx_znode = pWin->idx_znode;
    info.rc = *rcWin;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliSetActiveWindow (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_SETACTIVE;
    info.idx_znode = pWin?pWin->idx_znode:0;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliStartTrackPopupMenu (PTRACKMENUINFO ptmi)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_START_TRACKMENU;
    info.rc = ptmi->rc;
    info.hwnd = (HWND)ptmi;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliEndTrackPopupMenu (PTRACKMENUINFO ptmi)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_END_TRACKMENU;
    info.idx_znode = ptmi->idx_znode;
    info.hwnd = 0;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliForceCloseMenu (void)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_CLOSEMENU;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliEnableWindow (PMAINWIN pWin, int flags)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_ENABLEWINDOW;
    info.idx_znode = pWin?pWin->idx_znode:0;
    info.flags = flags;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliStartDragWindow (PMAINWIN pWin, const DRAGINFO* drag_info)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_STARTDRAG;
    info.idx_znode = pWin->idx_znode;
    info.hwnd = (HWND)drag_info->location;
    info.rc.left = drag_info->init_x;
    info.rc.top = drag_info->init_y;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliCancelDragWindow (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_CANCELDRAG;
    info.idx_znode = pWin->idx_znode;

    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (int)) < 0)
        return -1;

    return ret;
}

static int cliChangeCaption (PMAINWIN pWin)
{
    int ret;
    REQUEST req;
    ZORDEROPINFO info;

    info.id_op = ID_ZOOP_CHANGECAPTION;
    info.idx_znode = pWin->idx_znode;
    
    if (strlen (pWin->spCaption) <= MAX_CAPTION_LEN) {
        strcpy (info.caption, pWin->spCaption);
    } else {
        memcpy (info.caption, pWin->spCaption, MAX_CAPTION_LEN);
        info.caption[MAX_CAPTION_LEN] = '\0';
    }
    
    req.id = REQID_ZORDEROP;
    req.data = &info;
    req.len_data = sizeof (ZORDEROPINFO);
    if ((ret = ClientRequest (&req, &ret, sizeof (int))) < 0) {
        return -1;
    }

    return ret;
}

/*********************** Server-side routines ********************************/

void __mg_start_server_desktop (void)
{
    InitClipRgn (&sg_ScrGCRInfo.crgn, &sgFreeClipRectList);
    SetClipRgn (&sg_ScrGCRInfo.crgn, &g_rcScr);
    sg_ScrGCRInfo.age = 0;
    sg_ScrGCRInfo.old_zi_age = 0;

    InitClipRgn (&sg_UpdateRgn, &sgFreeClipRectList);

    SendMessage (HWND_DESKTOP, MSG_STARTSESSION, 0, 0);
    SendMessage (HWND_DESKTOP, MSG_ERASEDESKTOP, 0, 0);
}

#if 0
static void dump_zorder_list (const ZORDERINFO* zi, const ZORDERNODE* nodes)
{
    int slot;

    printf ("\n\n**** The zorder list of %p: ****\n", zi);

    slot = zi->first_global;

    printf ("****** The globals ****** \n");
    if (slot == 0)
        printf ("******** There is no globals ********.\n");
    for (; slot > 0; slot = nodes [slot].next) {
        printf ("******** idx (%d) from client %d for window %x is "
                        "(%d, %d, %d, %d), flags is %x, its age is %u.\n",
                        slot, nodes [slot].cli, nodes [slot].hwnd,
                        nodes [slot].rc.left,
                        nodes [slot].rc.top,
                        nodes [slot].rc.right,
                        nodes [slot].rc.bottom,
                        nodes [slot].flags,
                        nodes [slot].age);
    }
    printf ("****** End of globals ****** \n");

    slot = zi->first_topmost;

    printf ("****** The topmosts ****** \n");
    if (slot == 0)
        printf ("******** There is no topmosts ********.\n");
    for (; slot > 0; slot = nodes [slot].next) {
        printf ("******** idx (%d) from client %d for window %x is "
                        "(%d, %d, %d, %d), flags is %x, its age is %u\n",
                        slot, nodes [slot].cli, nodes [slot].hwnd,
                        nodes [slot].rc.left,
                        nodes [slot].rc.top,
                        nodes [slot].rc.right,
                        nodes [slot].rc.bottom,
                        nodes [slot].flags,
                        nodes [slot].age);
    }
    printf ("****** End of topmosts ****** \n");

    slot = zi->first_normal;

    printf ("****** The normals ****** \n");
    if (slot == 0)
        printf ("******** There is no normals ********.\n");

    for (; slot > 0; slot = nodes [slot].next) {
        printf ("******** idx (%d) from client %d for window %x is "
                        "(%d, %d, %d, %d), flags is %x, its age is %u\n",
                        slot, nodes [slot].cli, nodes [slot].hwnd,
                        nodes [slot].rc.left,
                        nodes [slot].rc.top,
                        nodes [slot].rc.right,
                        nodes [slot].rc.bottom,
                        nodes [slot].flags,
                        nodes [slot].age);
    }
    printf ("****** End of normals ****** \n");

    printf ("**** End of the zorder list of %p: ****\n", zi);
}
#endif

inline void* get_zi_from_client(int cli) 
{
    return (((cli>0)?mgClients[cli].layer:mgTopmostLayer)->zorder_info);
}


static int srvForceCloseMenu (int cli);

static void GetTextCharPos (PLOGFONT log_font, const char *text, 
               int len, int fit_bytes, int *fit_chars, int *pos_chars)
{
    DEVFONT* sbc_devfont = log_font->sbc_devfont;
    DEVFONT* mbc_devfont = log_font->mbc_devfont;
    int len_cur_char;
    int left_bytes = len;
    int char_count = 0, bytes = 0;

    while (left_bytes > 0 &&  bytes < fit_bytes) {
        if (pos_chars) {
            pos_chars[char_count] = len - left_bytes;
        }
        if ((mbc_devfont) && 
            (len_cur_char = (*mbc_devfont->charset_ops->len_first_char) 
             ((const unsigned char*)text, left_bytes)) > 0) {
            char_count ++;
            left_bytes -= len_cur_char;
            text += len_cur_char;
            bytes += len_cur_char;
        } else {
            if ((len_cur_char = (*sbc_devfont->charset_ops->len_first_char)
                 ((const unsigned char*)text, left_bytes)) > 0) {
                char_count ++;
                left_bytes -= len_cur_char;
                text += len_cur_char;
                bytes += len_cur_char;
            } else {
                break;
            }
        }
    }
    
    if (fit_chars) {
        *fit_chars = char_count;
    }
}

ON_ZNODE_OPERATION OnZNodeOperation;

static int srvAllocZOrderNode (int cli, HWND hwnd, HWND main_win, 
                DWORD flags, const RECT *rc, const char *caption)
{
    DWORD type = flags & ZOF_TYPE_MASK;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    int *first = NULL, *nr_nodes = NULL;
    int free_slot, slot, old_first;
    ZORDERNODE* nodes;

    usage_bmp += sizeof (ZORDERINFO);

    switch (type) {
        case ZOF_TYPE_GLOBAL:
            if (zi->nr_globals < zi->max_nr_globals) {
                first = &zi->first_global;
                nr_nodes = &zi->nr_globals;
            }
            break;
        case ZOF_TYPE_TOPMOST:
            if (zi->nr_topmosts < zi->max_nr_topmosts) {
                first = &zi->first_topmost;
                nr_nodes = &zi->nr_topmosts;
            }
            break;
        case ZOF_TYPE_NORMAL:
            if (zi->nr_normals < zi->max_nr_normals) {
                first = &zi->first_normal;
                nr_nodes = &zi->nr_normals;
            }
            break;
        default:
            break;
    }

    if (first == NULL) {
        return -1;
    }

    if (zi->cli_trackmenu >= 0 && flags & ZOF_VISIBLE)
        srvForceCloseMenu (0);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    /* lock zi for change */
    lock_zi_for_change (zi);

    /* the slot must be larger than zero */
    if (type == ZOF_TYPE_GLOBAL)
        free_slot = __mg_lookfor_unused_slot ((BYTE*)(zi + 1), 
                                        zi->max_nr_globals, 1);
    else {
        free_slot = __mg_lookfor_unused_slot ((BYTE*)(zi + 1) 
                        + (zi->max_nr_globals >> 3), 
                        zi->size_usage_bmp - (zi->max_nr_globals >> 3), 1);
        if (free_slot >= 0) {
            free_slot += zi->max_nr_globals;
        }
    }
    
    /* there is no slots in the bitmap */
    if (-1 == free_slot) {
        return -1;
    }
    
    nodes [free_slot].flags = flags;
    nodes [free_slot].rc = *rc;
    nodes [free_slot].age = 1;
    nodes [free_slot].cli = cli;
    nodes [free_slot].hwnd = hwnd;
    nodes [free_slot].main_win = main_win;
    
    /* &&&&&&& copy caption 
       we should cut off caption here. add ...  */
    if (caption) {
        PLOGFONT menufont ;
        int fit_chars, pos_chars[MAX_CAPTION_LEN];
        int caplen;
        menufont = GetSystemFont (SYSLOGFONT_MENU);
        caplen = strlen(caption);

        if (caplen < 32) {
            strcpy (nodes[free_slot].caption, caption);
        } else {
            int tail_pos;
            GetTextCharPos (menufont, caption, caplen, (32 - 3), /* '...' = 3*/
                            &fit_chars, pos_chars);
            /*
            memcpy(nodes[free_slot].caption, 
                   caption, pos_chars[fit_chars-1]);
            strcpy ((nodes[free_slot].caption + pos_chars[fit_chars-1]), 
                    "...");
            */
            tail_pos = pos_chars[fit_chars-1];
            
            if ((tail_pos + 3) >= 32 && fit_chars > 4) {
                tail_pos = pos_chars[fit_chars-2];
                if (tail_pos + 3 >= 32) {
                    tail_pos = pos_chars[fit_chars-3];
                    if (tail_pos + 3 >= 32) {
                        tail_pos = pos_chars[fit_chars-4];
                    }
                }
            }
            memcpy(nodes[free_slot].caption, caption, tail_pos);
            strcpy ((nodes[free_slot].caption + tail_pos), "...");
            
        }
    }

    /* check influenced zorder nodes */
    if (flags & ZOF_VISIBLE) {
        if (type >= ZOF_TYPE_NORMAL) {
            slot = zi->first_normal;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE &&
                                DoesIntersect (rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                }
            }
        }
        if (type >= ZOF_TYPE_TOPMOST) {
            slot = zi->first_topmost;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                                DoesIntersect (rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                }
            }
        }
        if (type >= ZOF_TYPE_GLOBAL) {
            slot = zi->first_global;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                                DoesIntersect (rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                }
            }
        }
        if (DoesIntersect (rc, &g_rcScr)) {
            nodes [0].age ++;
        }
    }

    /* chain the new node */
    old_first = *first;
    nodes [old_first].prev = free_slot;
    nodes [free_slot].prev = 0;
    nodes [free_slot].next = old_first;
    *first = free_slot;

    *nr_nodes += 1;

    /* unlock zi for change ... */
    unlock_zi_for_change (zi);

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_ALLOCATE, cli, free_slot);

    return free_slot;
}

static BOOL _cb_update_znode (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* znode)
{
    const RECT* rc = (RECT*)context;
    if (znode->flags & ZOF_VISIBLE && znode->flags & ZOF_REFERENCE) {
        update_client_window (znode, rc);
        znode->flags &= ~ZOF_REFERENCE;
        return TRUE;
    }

    return FALSE;
}

static BOOL _cb_update_cli_znode (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* znode)
{
    RECT rcInv;
    int cli = (int)context;

    if (znode->cli == cli && znode->flags & ZOF_VISIBLE) {
       MSG msg = {znode->hwnd, MSG_UPDATECLIWIN, 0, 0, __mg_timer_counter};

        if (!IsRectEmpty(&(znode->dirty_rc))) {

            IntersectRect(&rcInv, &znode->dirty_rc, &g_rcScr);    

            msg.wParam = MAKELONG (rcInv.left, rcInv.top);
            msg.lParam = MAKELONG (rcInv.right, rcInv.bottom);

            SetRectEmpty (&(znode->dirty_rc));

            __mg_send2client (&msg, mgClients + znode->cli);
            return TRUE;
        }
    }

    return FALSE;
}

void __mg_check_dirty_znode (int cli)
{
    ZORDERINFO* zi = (ZORDERINFO *)get_zi_from_client (cli);

    do_for_all_znodes ((void*)cli, zi, 
                    _cb_update_cli_znode, ZT_TOPMOST | ZT_NORMAL);

    mgClients [cli].has_dirty = FALSE;
}

static int srvSetActiveWindow (int cli, int idx_znode)
{
    int old_active = 0;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;
    HWND old_hwnd = HWND_NULL, new_hwnd = HWND_NULL;

    if (idx_znode > zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) {
        return (int)HWND_INVALID;
    }

    nodes = (ZORDERNODE*) ((BYTE*)(zi + 1) + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (__mg_ime_wnd == nodes [idx_znode].main_win ||
                    nodes [idx_znode].flags & ZOF_TF_TOOLWIN)
        return (int)HWND_INVALID;

    if (zi->cli_trackmenu >= 0)
        srvForceCloseMenu (0);

    /* lock zi for change */
    lock_zi_for_change (zi);

    if (zi->active_win) {
        old_active = zi->active_win;
    }

    zi->active_win = idx_znode;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    if (old_active) {
        if (nodes [old_active].cli == cli) {
            old_hwnd = nodes [old_active].hwnd;
        }
        else
            old_hwnd = HWND_OTHERPROC;
    }

    if (idx_znode) {
        if (nodes [idx_znode].cli == cli) {
            new_hwnd = nodes [idx_znode].hwnd;
        }
        else
            new_hwnd = HWND_OTHERPROC;
    }

    if (old_active && (nodes [zi->active_win].flags & ZOF_VISIBLE)) {
        post_msg_by_znode_p (zi, nodes + old_active, 
                        MSG_NCACTIVATE, FALSE, 0);
        post_msg_by_znode_p (zi, nodes + old_active, 
                        MSG_ACTIVE, FALSE, 0);
        post_msg_by_znode_p (zi, nodes + old_active, 
                        MSG_KILLFOCUS, new_hwnd, 0);
    }

    if (idx_znode) {
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_NCACTIVATE, TRUE, 0);
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_ACTIVE, TRUE, 0);
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_SETFOCUS, old_hwnd, 0);
    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_SETACTIVE, cli, idx_znode);

    return old_hwnd;
}

static void unchain_znode (unsigned char* usage_bmp,
                ZORDERNODE* nodes, int idx_znode)
{
    if (nodes [idx_znode].prev) {
        nodes [nodes [idx_znode].prev].next = nodes [idx_znode].next;
    }
    if (nodes [idx_znode].next) {
        nodes [nodes [idx_znode].next].prev = nodes [idx_znode].prev;
    }

    __mg_slot_clear_use (usage_bmp, idx_znode);
    nodes [idx_znode].flags = 0;
}

static int get_next_visible_mainwin (const ZORDERINFO* zi, int from)
{
    ZORDERNODE* nodes;
    int next;

    nodes = (ZORDERNODE*) ((char*)(zi + 1) + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (from) {
        next = nodes [from].next;

        while (next) {
            if (nodes [next].flags & ZOF_TF_MAINWIN
                    && (nodes [next].flags & ZOF_VISIBLE) 
                    && !(nodes [next].flags & ZOF_DISABLED))
                return next;

            next = nodes [next].next;
        }
    }

    next = zi->first_global;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN
                && (nodes [next].flags & ZOF_VISIBLE) 
                && !(nodes [next].flags & ZOF_DISABLED))
            return next;

        next = nodes [next].next;
    }

    next = zi->first_topmost;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN
                && (nodes [next].flags & ZOF_VISIBLE) 
                && !(nodes [next].flags & ZOF_DISABLED))
            return next;

        next = nodes [next].next;
    }

    next = zi->first_normal;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN
                && (nodes [next].flags & ZOF_VISIBLE) 
                && !(nodes [next].flags & ZOF_DISABLED))
            return next;

        next = nodes [next].next;
    }

    return 0;
}

static int srvFreeZOrderNode (int cli, int idx_znode)
{
    DWORD type;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    int *first = NULL, *nr_nodes = NULL;
    ZORDERNODE* nodes;
    int slot, old_active;
    RECT rc;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals)
            || idx_znode <= 0) {
        return -1;
    }

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    if (zi->cli_trackmenu >= 0 && nodes [idx_znode].flags & ZOF_VISIBLE)
        srvForceCloseMenu (0);

    type = nodes [idx_znode].flags & ZOF_TYPE_MASK;
    switch (type) {
        case ZOF_TYPE_GLOBAL:
            first = &zi->first_global;
            nr_nodes = &zi->nr_globals;
            break;
        case ZOF_TYPE_TOPMOST:
            first = &zi->first_topmost;
            nr_nodes = &zi->nr_topmosts;
            break;
        case ZOF_TYPE_NORMAL:
            first = &zi->first_normal;
            nr_nodes = &zi->nr_normals;
            break;
        default:
            break;
    }

    if (first == NULL)
        return -1;

    /* lock zi for change */
    lock_zi_for_change (zi);

    SetClipRgn (&sg_UpdateRgn, &nodes [idx_znode].rc);

    /* check influenced zorder nodes */
    if (nodes [idx_znode].flags & ZOF_VISIBLE) {
        rc = nodes [idx_znode].rc;

        slot = nodes [idx_znode].next;
        for (; slot > 0; slot = nodes [slot].next) {
            if (nodes [slot].flags & ZOF_VISIBLE &&
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot].rc)) {
                nodes [slot].age ++;
                nodes [slot].flags |= ZOF_REFERENCE;
            }
        }
        if (type > ZOF_TYPE_TOPMOST) {
            slot = zi->first_topmost;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE &&
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    nodes [slot].flags |= ZOF_REFERENCE;
                }
            }
        }
        if (type > ZOF_TYPE_NORMAL) {
            slot = zi->first_normal;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    nodes [slot].flags |= ZOF_REFERENCE;
                }
            }
        }

        if (SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
            nodes [0].age ++;
            nodes [0].flags |= ZOF_REFERENCE;
        }
    }

    /* unchain it */
    unchain_znode (usage_bmp, nodes, idx_znode);
    nodes [idx_znode].hwnd = 0;
    if (*first == idx_znode) {
        *first = nodes [idx_znode].next;
    }
    *nr_nodes -= 1;

    old_active = zi->active_win;
    if (idx_znode == zi->active_win)
        zi->active_win = 0;

    /* unlock zi for change  */
    unlock_zi_for_change (zi);

    /* update all znode if it's dirty */
    do_for_all_znodes (&rc, zi, _cb_update_znode, ZT_ALL);

    if (nodes [0].flags & ZOF_REFERENCE) {
        SendMessage (HWND_DESKTOP, 
                        MSG_ERASEDESKTOP, 0, (WPARAM)&rc);
        nodes [0].flags &= ~ZOF_REFERENCE;
    }

    /* if active_win is this window, change it */
    if (idx_znode == old_active) {
        int next_active = get_next_visible_mainwin (zi, idx_znode);
        srvSetActiveWindow (nodes [next_active].cli, next_active);
    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_FREE, cli, idx_znode);

    return 0;
}

static int srvMove2Top (int cli, int idx_znode)
{
    DWORD type;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    int *first = NULL;
    ZORDERNODE* nodes;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals)
            || idx_znode <= 0) {
        return -1;
    }

    if (zi->cli_trackmenu >= 0)
        srvForceCloseMenu (0);

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    type = nodes [idx_znode].flags & ZOF_TYPE_MASK;
    switch (type) {
        case ZOF_TYPE_GLOBAL:
            first = &zi->first_global;
            break;
        case ZOF_TYPE_TOPMOST:
            first = &zi->first_topmost;
            break;
        case ZOF_TYPE_NORMAL:
            first = &zi->first_normal;
            break;
        default:
            break;
    }

    if (first == NULL || *first == idx_znode)
        return -1;

    EmptyClipRgn (&sg_UpdateRgn);

    /* lock zi for change */
    lock_zi_for_change (zi);

    if (nodes [idx_znode].flags & ZOF_VISIBLE) {
        int slot;
        RECT rc = nodes [idx_znode].rc;

        if (type == ZOF_TYPE_NORMAL) {
            slot = zi->first_normal;
            for (; slot != idx_znode; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE &&
                                DoesIntersect (&rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    AddClipRect(&sg_UpdateRgn, &nodes [slot].rc);
                }
            }
        }
        if (type == ZOF_TYPE_TOPMOST) {
            slot = zi->first_topmost;
            for (; slot != idx_znode; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                                DoesIntersect (&rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    AddClipRect (&sg_UpdateRgn, &nodes [slot].rc);
                }
            }
        }
        if (type == ZOF_TYPE_GLOBAL) {
            slot = zi->first_global;
            for (; slot != idx_znode; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                                DoesIntersect (&rc, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    AddClipRect (&sg_UpdateRgn, &nodes [slot].rc);
                }
            }
        }
    }

    /* unchain it and move to top */
    if (nodes [idx_znode].prev) {
        nodes [nodes [idx_znode].prev].next = nodes [idx_znode].next;
    }
    if (nodes [idx_znode].next) {
        nodes [nodes [idx_znode].next].prev = nodes [idx_znode].prev;
    }
    nodes [idx_znode].prev = 0;
    nodes [idx_znode].next = *first;
    nodes [*first].prev = idx_znode;
    *first = idx_znode;

    nodes [idx_znode].age ++;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    if (nodes [idx_znode].flags & ZOF_VISIBLE) {
        update_client_window_rgn (nodes [idx_znode].cli, 
                        nodes [idx_znode].hwnd);

    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_MOVE2TOP, cli, idx_znode);

    return 0;
}

static BOOL _cb_intersect_rc (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    RECT* rc = (RECT*)context;
    if (node->flags & ZOF_VISIBLE && DoesIntersect (rc, &node->rc)) {
        node->age ++;
        return TRUE;
    }

    return FALSE;
}

static int srvShowWindow (int cli, int idx_znode)
{
    DWORD type;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;
    int *first = NULL;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) ||
            idx_znode <= 0) {
        return -1;
    }

    if (zi->cli_trackmenu >= 0)
        srvForceCloseMenu (0);

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    type = nodes [idx_znode].flags & ZOF_TYPE_MASK;
    switch (type) {
        case ZOF_TYPE_GLOBAL:
            first = &zi->first_global;
            break;
        case ZOF_TYPE_TOPMOST:
            first = &zi->first_topmost;
            break;
        case ZOF_TYPE_NORMAL:
            first = &zi->first_normal;
            break;
        default:
            break;
    }

    if (first == NULL)
        return -1;

    /* lock zi for change */
    lock_zi_for_change (zi);

    {
        int slot;
        RECT rc = nodes [idx_znode].rc;

        if (type > ZOF_TYPE_NORMAL) {
            do_for_all_znodes (&rc, zi, _cb_intersect_rc, ZT_NORMAL);
        }
        if (type > ZOF_TYPE_TOPMOST) {
            do_for_all_znodes (&rc, zi, _cb_intersect_rc, ZT_TOPMOST);
        }
        if (type > ZOF_TYPE_GLOBAL) {
            do_for_all_znodes (&rc, zi, _cb_intersect_rc, ZT_GLOBAL);
        }

        slot = nodes [idx_znode].next;
        for (; slot != 0; slot = nodes [slot].next)
        {
            if (nodes [slot].flags & ZOF_VISIBLE && 
                            DoesIntersect (&rc, &nodes [slot].rc)) {
                nodes [slot].age ++;
            }
        }

        if (DoesIntersect (&rc, &g_rcScr)) {
            nodes [0].age ++;
        }
        nodes [idx_znode].age ++;
        nodes [idx_znode].flags |= ZOF_VISIBLE;
    }

    /* unlock zi for change ... */
    unlock_zi_for_change (zi);

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_SHOW, cli, idx_znode);

    return 0;
}

static BOOL _cb_update_rc (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    CLIPRGN* cliprgn = (CLIPRGN*)context;

    if (node->flags & ZOF_VISIBLE && 
                    SubtractClipRect (cliprgn, &node->rc)) {
        node->age ++;
        node->flags |= ZOF_REFERENCE;
        return TRUE;
    }

    return FALSE;
}

static int srvHideWindow (int cli, int idx_znode)
{
    DWORD type;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    int *first = NULL;
    ZORDERNODE* nodes;

    if (idx_znode > zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) {
        return -1;
    }

    if (zi->cli_trackmenu >= 0)
        srvForceCloseMenu (0);

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    type = nodes [idx_znode].flags & ZOF_TYPE_MASK;
    switch (type) {
        case ZOF_TYPE_GLOBAL:
            first = &zi->first_global;
            break;
        case ZOF_TYPE_TOPMOST:
            first = &zi->first_topmost;
            break;
        case ZOF_TYPE_NORMAL:
            first = &zi->first_normal;
            break;
        default:
            break;
    }

    if (first == NULL)
        return -1;

    /* lock zi for change */
    lock_zi_for_change (zi);

    /* check influenced zorder nodes */
    SetClipRgn (&sg_UpdateRgn, &nodes [idx_znode].rc);
    if (nodes [idx_znode].flags & ZOF_VISIBLE) {
        int slot;

        slot = nodes [idx_znode].next;
        for (; slot > 0; slot = nodes [slot].next) {
            if (nodes [slot].flags & ZOF_VISIBLE && 
                    SubtractClipRect (&sg_UpdateRgn, &nodes [slot].rc)) {
                nodes [slot].age ++;
                nodes [slot].flags |= ZOF_REFERENCE;
            }
        }
        if (type > ZOF_TYPE_TOPMOST) {
            do_for_all_znodes (&sg_UpdateRgn, zi, _cb_update_rc, ZT_TOPMOST);
        }
        if (type > ZOF_TYPE_NORMAL) {
            do_for_all_znodes (&sg_UpdateRgn, zi, _cb_update_rc, ZT_NORMAL);
        }
        if (SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
            nodes [0].age ++;
            nodes [0].flags |= ZOF_REFERENCE;
        }
    }

    if (idx_znode && (nodes [idx_znode].flags & ZOF_TF_MAINWIN
         && (nodes [idx_znode].flags & ZOF_VISIBLE))) {
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_NCACTIVATE, FALSE, 0);
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_ACTIVE, FALSE, 0);
        post_msg_by_znode_p (zi, nodes + idx_znode, 
                        MSG_KILLFOCUS, 0, 0);
    }

    nodes [idx_znode].flags &= ~ZOF_VISIBLE;
    /*
     * do not reset the age to zero.
     * nodes [idx_znode].age = 0;
     */

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    /* update all znode if it's dirty */
    do_for_all_znodes (&nodes [idx_znode].rc, zi, _cb_update_znode, ZT_ALL);

    if (nodes [0].flags & ZOF_REFERENCE) {
        SendMessage (HWND_DESKTOP, 
                        MSG_ERASEDESKTOP, 0, (WPARAM)&nodes [idx_znode].rc);
        nodes [0].flags &= ~ZOF_REFERENCE;
    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_HIDE, cli, idx_znode);

    return 0;
}

static BOOL _cb_exclude_rc (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    if (!(node->flags & ZOF_VISIBLE))
        return FALSE;

    ExcludeClipRect (HDC_SCREEN, &node->rc);

    return TRUE;
}

static int srvMoveWindow (int cli, int idx_znode, const RECT* rcWin)
{
    DWORD type;
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    int *first = NULL;
    ZORDERNODE* nodes;
    RECT rcInv[4], rcOld, rcInter;
    int nInvCount;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) ||
            idx_znode < 0) {
        return -1;
    }

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    type = nodes [idx_znode].flags & ZOF_TYPE_MASK;

    switch (type) {
        case ZOF_TYPE_GLOBAL:
            first = &zi->first_global;
            break;
        case ZOF_TYPE_TOPMOST:
            first = &zi->first_topmost;
            break;
        case ZOF_TYPE_NORMAL:
            first = &zi->first_normal;
            break;
        default:
            break;
    }

    if (first == NULL)
        return -1;

    if (memcmp (&nodes [idx_znode].rc, rcWin, sizeof (RECT)) == 0)
        return 0;

    if (nodes [idx_znode].flags & ZOF_VISIBLE && zi->cli_trackmenu >= 0)
        srvForceCloseMenu (0);

    nInvCount = SubtractRect (rcInv, &nodes [idx_znode].rc, rcWin);


    if (nodes [idx_znode].flags & ZOF_VISIBLE) {
        int i, slot;

        /* lock zi for change */
        lock_zi_for_change (zi);

        if (type > ZOF_TYPE_NORMAL) {
            do_for_all_znodes ((void*)rcWin, zi, _cb_intersect_rc, ZT_NORMAL);
        }
        if (type > ZOF_TYPE_TOPMOST) {
            do_for_all_znodes ((void*)rcWin, zi, _cb_intersect_rc, ZT_TOPMOST);
        }

        slot = nodes [idx_znode].next;
        for (; slot != 0; slot = nodes [slot].next)
        {
            if (nodes [slot].flags & ZOF_VISIBLE && 
                            DoesIntersect (rcWin, &nodes [slot].rc)) {
                nodes [slot].age ++;
            }
        }

        if (DoesIntersect (rcWin, &g_rcScr)) {
            nodes [0].age ++;
        }

        /* check influenced zorder nodes */
        for (i = 0; i < nInvCount; i++) {
            SetClipRgn (&sg_UpdateRgn, rcInv + i);

            slot = nodes [idx_znode].next;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                        !(nodes [slot].flags & ZOF_REFERENCE) &&
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot].rc)) {
                    nodes [slot].age ++;
                    nodes [slot].flags |= ZOF_REFERENCE;
                }
            }

            if (type > ZOF_TYPE_TOPMOST) {
                slot = zi->first_topmost;
                for (; slot > 0; slot = nodes [slot].next) {
                    if (nodes [slot].flags & ZOF_VISIBLE && 
                            !(nodes [slot].flags & ZOF_REFERENCE) &&
                            SubtractClipRect (&sg_UpdateRgn, &nodes[slot].rc)) {
                        nodes [slot].age ++;
                        nodes [slot].flags |= ZOF_REFERENCE;
                    }
                }
            }

            if (type > ZOF_TYPE_NORMAL) {
                slot = zi->first_normal;
                for (; slot > 0; slot = nodes [slot].next) {
                    if (nodes [slot].flags & ZOF_VISIBLE && 
                            !(nodes [slot].flags & ZOF_REFERENCE) &&
                            SubtractClipRect (&sg_UpdateRgn, &nodes[slot].rc)) {
                        nodes [slot].age ++;
                        nodes [slot].flags |= ZOF_REFERENCE;
                    }
                }
            }

            if (!(nodes [0].flags & ZOF_REFERENCE) &&
                            SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
                nodes [0].age ++;
                nodes [0].flags |= ZOF_REFERENCE;
            }
        }

        rcOld = nodes [idx_znode].rc;
        nodes [idx_znode].rc = *rcWin;
        nodes [idx_znode].age ++;

        if (cli == 0
                || (DWORD)mgClients [cli].layer == SHAREDRES_TOPMOST_LAYER) {
            /* Copy window content to new postion */
            SelectClipRect (HDC_SCREEN, rcWin);

            slot = 0;
            switch (type) {
            case ZOF_TYPE_NORMAL:
                do_for_all_znodes (NULL, zi, 
                                _cb_exclude_rc, ZT_GLOBAL);
                do_for_all_znodes (NULL, zi, 
                                _cb_exclude_rc, ZT_TOPMOST);
                slot = zi->first_normal;
                break;
            case ZOF_TYPE_TOPMOST:
                do_for_all_znodes (NULL, zi, 
                                _cb_exclude_rc, ZT_GLOBAL);
                slot = zi->first_topmost;
                break;
            case ZOF_TYPE_GLOBAL:
                slot = zi->first_global;
                break;
            case ZOF_TYPE_DESKTOP:
                do_for_all_znodes (NULL, zi, 
                                _cb_exclude_rc, ZT_ALL);
                break;
            default:
                break;
            }

            while (slot) {
                if (slot == idx_znode) {
                    break;
                }

                if (nodes [slot].flags & ZOF_VISIBLE) {
                    ExcludeClipRect (HDC_SCREEN, &nodes [slot].rc);
                }

                slot = nodes [slot].next;
            }

            BitBlt (HDC_SCREEN, rcOld.left, rcOld.top, 
                            MIN (RECTWP (rcWin), RECTW (rcOld)), 
                            MIN (RECTHP (rcWin), RECTH (rcOld)),
                            HDC_SCREEN, rcWin->left, rcWin->top, 0);
            /* Restore the clip region of HDC_SCREEN */
            SelectClipRect (HDC_SCREEN, &g_rcScr);
        }

        /* unlock zi for change ... */
        unlock_zi_for_change (zi);

        /* check the invalid rect of the window */
        EmptyClipRgn (&sg_UpdateRgn);
        nInvCount = SubtractRect (rcInv, &rcOld, &g_rcScr);
        for (i = 0; i < nInvCount; i++) {
            AddClipRect (&sg_UpdateRgn, rcInv + i);
        }

        if (type < ZOF_TYPE_GLOBAL) {
            slot = zi->first_global;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                            IntersectRect (&rcInter, &rcOld, &nodes [slot].rc))
                    AddClipRect(&sg_UpdateRgn, &rcInter);
            }
        }

        if (type < ZOF_TYPE_TOPMOST) {
            slot = zi->first_topmost;
            for (; slot > 0; slot = nodes [slot].next) {
                if (nodes [slot].flags & ZOF_VISIBLE && 
                            IntersectRect (&rcInter, &rcOld, &nodes [slot].rc))
                    AddClipRect(&sg_UpdateRgn, &rcInter);
            }
        }

        slot = *first;
        for (; slot != idx_znode; slot = nodes [slot].next) {
            if (nodes [slot].flags & ZOF_VISIBLE && 
                        IntersectRect (&rcInter, &rcOld, &nodes [slot].rc))
                AddClipRect(&sg_UpdateRgn, &rcInter);
        }
        do_for_all_znodes (&rcOld, zi, _cb_update_znode, ZT_ALL);

        if (nodes [0].flags & ZOF_REFERENCE) {

            if (cli == 0 
                    || (DWORD)mgClients [cli].layer == SHAREDRES_TOPMOST_LAYER)
                SendMessage (HWND_DESKTOP, 
                                MSG_ERASEDESKTOP, 0, (LPARAM)&rcOld); 
            nodes [0].flags &= ~ZOF_REFERENCE;
        }

        OffsetRegion (&sg_UpdateRgn, 
                        rcWin->left - rcOld.left, 
                        rcWin->top - rcOld.top);

        update_client_window_rgn (nodes [idx_znode].cli, 
                        nodes [idx_znode].hwnd);
    }
    else {
        lock_zi_for_change (zi);

        nodes [idx_znode].rc = *rcWin;
        nodes [idx_znode].age ++;

        unlock_zi_for_change (zi);
    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_MOVEWIN, cli, idx_znode);

    return 0;
}

static int srvStartTrackPopupMenu (int cli, const RECT* rc, HWND ptmi)
{
    ZORDERINFO* zi = get_zi_from_client (cli);
    ZORDERNODE* menu_nodes;
    ZORDERNODE* win_nodes;

    if (zi->cli_trackmenu >= 0 && zi->cli_trackmenu != cli) {
        srvForceCloseMenu (0);
    }

    if (zi->nr_popupmenus == zi->max_nr_popupmenus)
        return -1;

    menu_nodes = (ZORDERNODE*) ((char*)(zi + 1)+ zi->size_usage_bmp);
    win_nodes = menu_nodes + DEF_NR_POPUPMENUS;
    
    /* lock zi for change */
    lock_zi_for_change (zi);

    /* check influenced window zorder nodes */
    do_for_all_znodes ((void*)rc, zi, _cb_intersect_rc, ZT_ALL);

    if (DoesIntersect (rc, &g_rcScr)) {
        win_nodes [0].age ++;
    }

    menu_nodes [zi->nr_popupmenus].rc = *rc;
    menu_nodes [zi->nr_popupmenus].hwnd = ptmi;

    if (zi->cli_trackmenu == -1)
        zi->cli_trackmenu = cli;
    zi->nr_popupmenus ++;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    return zi->nr_popupmenus - 1;
}

static int srvEndTrackPopupMenu (int cli, int idx_znode)
{
    ZORDERINFO* zi = get_zi_from_client (cli);
    ZORDERNODE* menu_nodes;
    ZORDERNODE* win_nodes;
    RECT rc;

    if (zi->cli_trackmenu != cli
                    || zi->nr_popupmenus != (idx_znode + 1))
        return -1;

    menu_nodes = (ZORDERNODE*) ((char*)(zi + 1)+ zi->size_usage_bmp);
    win_nodes = menu_nodes + DEF_NR_POPUPMENUS;
    
    /* lock zi for change */
    lock_zi_for_change (zi);

    rc = menu_nodes [idx_znode].rc;
    SetClipRgn (&sg_UpdateRgn, &rc);

    /* check influenced window zorder nodes */
    do_for_all_znodes (&sg_UpdateRgn, zi, _cb_update_rc, ZT_ALL);

    if (SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
        win_nodes [0].age ++;
        win_nodes [0].flags |= ZOF_REFERENCE;
    }

    zi->nr_popupmenus --;
    if (zi->nr_popupmenus == 0)
        zi->cli_trackmenu = -1;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    /* update all znode if it's dirty */
    do_for_all_znodes (&rc, zi, _cb_update_znode, ZT_ALL);

    if (win_nodes [0].flags & ZOF_REFERENCE) {
        SendMessage (HWND_DESKTOP, 
                        MSG_ERASEDESKTOP, 0, (LPARAM)&rc);
        win_nodes [0].flags &= ~ZOF_REFERENCE;
    }

    return 0;
}

static int srvForceCloseMenu (int cli)
{
    ZORDERINFO* zi = get_zi_from_client (cli);
    int i, ret = 0, cli_trackmenu;
    ZORDERNODE* menu_nodes;
    ZORDERNODE* win_nodes;
    RECT rc_bound = {0, 0, 0, 0};

    if (zi->cli_trackmenu < 0 || zi->nr_popupmenus == 0)
        return 0;

    menu_nodes = (ZORDERNODE*) ((char*)(zi + 1)+ zi->size_usage_bmp);
    win_nodes = menu_nodes + DEF_NR_POPUPMENUS;

    /* lock zi for change */
    lock_zi_for_change (zi);

    for (i = 0; i < zi->nr_popupmenus; i++) {
        GetBoundRect (&rc_bound, &rc_bound, &menu_nodes [i].rc);
    }

    SetClipRgn (&sg_UpdateRgn, &rc_bound);

    /* check influenced window zorder nodes */
    do_for_all_znodes (&sg_UpdateRgn, zi, _cb_update_rc, ZT_ALL);

    if (SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
        win_nodes [0].age ++;
        win_nodes [0].flags |= ZOF_REFERENCE;
    }

    cli_trackmenu = zi->cli_trackmenu;
    zi->cli_trackmenu = -1;
    zi->nr_popupmenus = 0;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    /* update all znode if it's dirty */
    do_for_all_znodes (&rc_bound, zi, _cb_update_znode, ZT_ALL);

    if (win_nodes [0].flags & ZOF_REFERENCE) {
        SendMessage (HWND_DESKTOP, 
                        MSG_ERASEDESKTOP, 0, (LPARAM)&rc_bound);
        win_nodes [0].flags &= ~ZOF_REFERENCE;
    }

    /* notify the client to close the menu */
    {
        MSG msg = {0, MSG_CLOSEMENU, 0, 0, __mg_timer_counter};

        if (cli_trackmenu)
            ret = __mg_send2client (&msg, mgClients + cli_trackmenu);
        else
            SendMessage (HWND_DESKTOP, MSG_CLOSEMENU, 0, 0);
    }

    return ret;
}

static int srvEnableWindow (int cli, int idx_znode, int flags)
{
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) ||
            idx_znode < 0) {
        return -1;
    }

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    /* lock zi for change */
    lock_zi_for_change (zi);

    if (flags)  /* enable window */
        nodes [idx_znode].flags &= ~ZOF_DISABLED;
    else
        nodes [idx_znode].flags |= ZOF_DISABLED;

    /* unlock zi for change */
    unlock_zi_for_change (zi);

    if (OnZNodeOperation) {
        if (flags)
            OnZNodeOperation (ZNOP_ENABLEWINDOW, cli, idx_znode);
        else
            OnZNodeOperation (ZNOP_DISABLEWINDOW, cli, idx_znode);
    }

    return 0;
}

typedef struct _DRAGDROPINFO {
    int cli, idx_znode;
    HWND hwnd;
    ZORDERINFO* zi;
    RECT rc;

    int location;
    int last_x, last_y;
} DRAGDROPINFO;

static DRAGDROPINFO _dd_info = {-1};

static int srvStartDragWindow (int cli, int idx_znode, 
                int location, int init_x, int init_y)
{
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) ||
            idx_znode < 0) {
        return -1;
    }

    if (_dd_info.cli >= 0)
        return -1;

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    lock_zi_for_change (zi);
    
    _dd_info.cli = cli;
    _dd_info.idx_znode = idx_znode;
    _dd_info.hwnd = nodes [idx_znode].hwnd;
    _dd_info.zi = zi;
    _dd_info.rc = nodes [idx_znode].rc;

    _dd_info.location = location;
    _dd_info.last_x = init_x;
    _dd_info.last_y = init_y;

    switch (location) {
        case HT_CAPTION:
            SetDefaultCursor (GetSystemCursor (IDC_MOVE));
            break;
        case HT_BORDER_TOP:
        case HT_BORDER_BOTTOM:
            SetDefaultCursor (GetSystemCursor (IDC_SIZENS));
            break;
        case HT_BORDER_LEFT:
        case HT_BORDER_RIGHT:
            SetDefaultCursor (GetSystemCursor (IDC_SIZEWE));
            break;
        case HT_CORNER_TL:
        case HT_CORNER_BR:
            SetDefaultCursor (GetSystemCursor (IDC_SIZENWSE));
            break;
        case HT_CORNER_BL:
        case HT_CORNER_TR:
            SetDefaultCursor (GetSystemCursor (IDC_SIZENESW));
            break;
        default:
            fprintf (stderr, 
                "DESKTOP Drag and drop window: bad location\n");
            break;
    }

    SetPenColor (HDC_SCREEN, PIXEL_lightwhite);
    SelectClipRect (HDC_SCREEN, &g_rcScr);
    do_for_all_znodes (NULL, zi, _cb_exclude_rc, ZT_GLOBAL);
    FocusRect (HDC_SCREEN, _dd_info.rc.left, _dd_info.rc.top,
                _dd_info.rc.right, _dd_info.rc.bottom);

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_STARTDRAG, cli, idx_znode);

    return 0;
}

static int srvCancelDragWindow (int cli, int idx_znode)
{
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;

    if (idx_znode > (zi->max_nr_globals 
                    + zi->max_nr_topmosts + zi->max_nr_normals) ||
            idx_znode < 0) {
        return -1;
    }

    if (_dd_info.cli == -1 
                    || _dd_info.cli != cli 
                    || _dd_info.idx_znode != idx_znode)
        return -1;

    _dd_info.cli = -1;
    unlock_zi_for_change (zi);
    SelectClipRect (HDC_SCREEN, &g_rcScr);
    SetDefaultCursor (GetSystemCursor (IDC_ARROW));

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_CANCELDRAG, cli, idx_znode);

    return 0;
}

int __mg_do_drag_drop_window (int msg, int x, int y)
{
    if (_dd_info.cli < 0)
        return 0;

    if (msg == MSG_MOUSEMOVE) {
        SetPenColor (HDC_SCREEN, PIXEL_lightwhite);
        FocusRect (HDC_SCREEN, _dd_info.rc.left, _dd_info.rc.top,
                _dd_info.rc.right, _dd_info.rc.bottom);

        switch (_dd_info.location) {
                case HT_CAPTION:
                    OffsetRect (&_dd_info.rc, 
                                    x - _dd_info.last_x, 
                                    y - _dd_info.last_y);
                    break;

                case HT_BORDER_TOP:
                    _dd_info.rc.top += y - _dd_info.last_y;
                    break;

                case HT_BORDER_BOTTOM:
                    _dd_info.rc.bottom += y - _dd_info.last_y;
                    break;

                case HT_BORDER_LEFT:
                    _dd_info.rc.left += x - _dd_info.last_x;
                    break;

                case HT_BORDER_RIGHT:
                    _dd_info.rc.right += x - _dd_info.last_x;
                    break;

                case HT_CORNER_TL:
                    _dd_info.rc.left += x - _dd_info.last_x;
                    _dd_info.rc.top += y - _dd_info.last_y;
                    break;

                case HT_CORNER_TR:
                    _dd_info.rc.right += x - _dd_info.last_x;
                    _dd_info.rc.top += y - _dd_info.last_y;
                    break;

                case HT_CORNER_BL:
                    _dd_info.rc.left += x - _dd_info.last_x;
                    _dd_info.rc.bottom += y - _dd_info.last_y;
                    break;

                case HT_CORNER_BR:
                    _dd_info.rc.right += x - _dd_info.last_x;
                    _dd_info.rc.bottom += y - _dd_info.last_y;
                    break;

                default:
                    fprintf (stderr, 
                        "__mg_do_drag_drop_window: bad location\n");
                    break;
        }

        FocusRect (HDC_SCREEN, _dd_info.rc.left, _dd_info.rc.top,
                _dd_info.rc.right, _dd_info.rc.bottom);
                    
        _dd_info.last_x = x;
        _dd_info.last_y = y;
    }
    else {
        MSG msg = {_dd_info.hwnd, MSG_WINDOWDROPPED, 0, 0, __mg_timer_counter};

        msg.wParam = MAKELONG (_dd_info.rc.left, _dd_info.rc.top);
        msg.lParam = MAKELONG (_dd_info.rc.right, _dd_info.rc.bottom);

        SetPenColor (HDC_SCREEN, PIXEL_lightwhite);
        FocusRect (HDC_SCREEN, _dd_info.rc.left, _dd_info.rc.top,
                _dd_info.rc.right, _dd_info.rc.bottom);

        /* post MSG_WINDOWDROPPED to the target window */
        if (_dd_info.cli == 0) {
            PostMessage (_dd_info.hwnd, MSG_WINDOWDROPPED,
                            msg.wParam, msg.lParam);
        }
        else {
            mgClients [_dd_info.cli].last_live_time = __mg_timer_counter;
            __mg_send2client (&msg, mgClients + _dd_info.cli);
        }

        _dd_info.cli = -1;
        unlock_zi_for_change (_dd_info.zi);
        SelectClipRect (HDC_SCREEN, &g_rcScr);
        SetDefaultCursor (GetSystemCursor (IDC_ARROW));
    }

    return 1;
}

static int srvChangeCaption (int cli, int idx_znode, const char *caption)
{
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;

    usage_bmp += sizeof (ZORDERINFO);

    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (caption && idx_znode > 0) {
        PLOGFONT menufont ;
        int fit_chars, pos_chars[MAX_CAPTION_LEN];
        int caplen;
        menufont = GetSystemFont (SYSLOGFONT_MENU);
        caplen = strlen(caption);

        if (caplen < 32) {
            strcpy (nodes[idx_znode].caption, caption);
        } else {
            GetTextCharPos (menufont, caption, caplen, (32 - 3), /* '...' = 3*/
                                 &fit_chars, pos_chars);
            memcpy(nodes[idx_znode].caption, 
                   caption, pos_chars[fit_chars-1]);
            strcpy ((nodes[idx_znode].caption + pos_chars[fit_chars-1]), 
                    "...");
        }
    }

    if (OnZNodeOperation)
        OnZNodeOperation (ZNOP_CHANGECAPTION, cli, idx_znode);

    return 0;
}

int __mg_do_zorder_operation (int cli, const ZORDEROPINFO* info)
{
    int ret = -1;
    
    switch (info->id_op) {
        case ID_ZOOP_ALLOC:
            ret = srvAllocZOrderNode (cli, info->hwnd, info->main_win, 
                            info->flags, &info->rc, info->caption);
            break;
        case ID_ZOOP_FREE:
            ret = srvFreeZOrderNode (cli, info->idx_znode);
            break;
        case ID_ZOOP_MOVE2TOP:
            ret = srvMove2Top (cli, info->idx_znode);
            break;
        case ID_ZOOP_SHOW:
            ret = srvShowWindow (cli, info->idx_znode);
            break;
        case ID_ZOOP_HIDE:
            ret = srvHideWindow (cli, info->idx_znode);
            break;
        case ID_ZOOP_MOVEWIN:
            ret = srvMoveWindow (cli, info->idx_znode, &info->rc);
            break;
        case ID_ZOOP_SETACTIVE:
            ret = srvSetActiveWindow (cli, info->idx_znode);
            break;
        case ID_ZOOP_START_TRACKMENU:
            ret = srvStartTrackPopupMenu (cli, &info->rc, info->hwnd);
            break;
        case ID_ZOOP_END_TRACKMENU:
            ret = srvEndTrackPopupMenu (cli, info->idx_znode);
            break;
        case ID_ZOOP_CLOSEMENU:
            ret = srvForceCloseMenu (cli);
            break;
        case ID_ZOOP_ENABLEWINDOW:
            ret = srvEnableWindow (cli, info->idx_znode, info->flags);
            break;
        case ID_ZOOP_STARTDRAG:
            ret = srvStartDragWindow (cli, info->idx_znode, (int)info->hwnd,
                            info->rc.left, info->rc.top);
            break;
        case ID_ZOOP_CANCELDRAG:
            ret = srvCancelDragWindow (cli, info->idx_znode);
            break;
        case ID_ZOOP_CHANGECAPTION:
            srvChangeCaption (cli, info->idx_znode, info->caption);
            break;
        default:
            break;
    }

#if 0
    {
        ZORDERINFO* zi = get_zi_from_client (cli);
        ZORDERNODE* nodes;

        nodes = (ZORDERNODE*) ((char*)(zi + 1) + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
        dump_zorder_list (zi, nodes);
    }
#endif

    return ret;
}

static BOOL _cb_intersect_rc_no_cli (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    ZORDERNODE* del_node = (ZORDERNODE*)context;

    if (node->cli != del_node->cli && 
                    node->flags & ZOF_VISIBLE && 
                    SubtractClipRect (&sg_UpdateRgn, &node->rc)) {
        node->age ++;
        node->flags |= ZOF_REFERENCE;
        return TRUE;
    }

    return FALSE;
}

static BOOL _cb_update_rc_nocli (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    int cli = (int)context;

    if (node->flags & ZOF_VISIBLE && node->cli != cli &&
                    SubtractClipRect (&sg_UpdateRgn, &node->rc)) {
        node->age ++;
        node->flags |= ZOF_REFERENCE;
        return TRUE;
    }

    return FALSE;
}

int __mg_remove_all_znodes_of_client (int cli)
{
    unsigned char* usage_bmp = get_zi_from_client (cli);
    ZORDERINFO* zi = (ZORDERINFO*)usage_bmp;
    ZORDERNODE* nodes;
    int slot, slot2, old_active;
    RECT rc_bound = {0, 0, 0, 0};

    usage_bmp += sizeof (ZORDERINFO);
    nodes = (ZORDERNODE*) (usage_bmp + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    if (zi->cli_trackmenu == cli) {
        int i;
        ZORDERNODE* menu_nodes;

        menu_nodes = (ZORDERNODE*) ((char*)(zi + 1)+ zi->size_usage_bmp);

        /* lock zi for change */
        lock_zi_for_change (zi);

        for (i = 0; i < zi->nr_popupmenus; i++) {
            GetBoundRect (&rc_bound, &rc_bound, &menu_nodes [i].rc);
        }

        SetClipRgn (&sg_UpdateRgn, &rc_bound);

        /* check influenced window zorder nodes */
        do_for_all_znodes ((void*)cli, zi, _cb_update_rc_nocli, ZT_ALL);

        if (SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
            nodes [0].age ++;
            nodes [0].flags |= ZOF_REFERENCE;
        }

        zi->cli_trackmenu = -1;
        zi->nr_popupmenus = 0;

        /* unlock zi for change */
        unlock_zi_for_change (zi);
    }

    /* lock zi for change */
    lock_zi_for_change (zi);

    /* handle topmosts */
    slot = zi->first_topmost;
    for (; slot > 0; slot = nodes [slot].next) {
        if (nodes [slot].cli == cli) {
            if (nodes [slot].flags & ZOF_VISIBLE) {
                SetClipRgn (&sg_UpdateRgn, &nodes [slot].rc);
                GetBoundRect (&rc_bound, &rc_bound, &nodes [slot].rc);

                slot2 = nodes [slot].next;
                for (; slot2 > 0; slot2 = nodes [slot2].next) {
                    if (nodes [slot2].cli != cli &&
                        nodes [slot2].flags & ZOF_VISIBLE && 
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot2].rc)) {

                        nodes [slot2].age ++;
                        nodes [slot2].flags |= ZOF_REFERENCE;
                    }
                }

                do_for_all_znodes (nodes + slot, zi, 
                                _cb_intersect_rc_no_cli, ZT_NORMAL);

                if (!(nodes [0].flags & ZOF_REFERENCE) &&
                                SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
                    nodes [0].age ++;
                    nodes [0].flags |= ZOF_REFERENCE;
                }
            }

            unchain_znode (usage_bmp, nodes, slot);

            if (zi->first_topmost == slot) {
                zi->first_topmost = nodes [slot].next;
            }
            zi->nr_topmosts --;
        }
    }

    /* handle normals */
    slot = zi->first_normal;
    for (; slot > 0; slot = nodes [slot].next) {
        if (nodes [slot].cli == cli) {
            if (nodes [slot].flags & ZOF_VISIBLE) {
                SetClipRgn (&sg_UpdateRgn, &nodes [slot].rc);
                GetBoundRect (&rc_bound, &rc_bound, &nodes [slot].rc);

                slot2 = nodes [slot].next;
                for (; slot2 > 0; slot2 = nodes [slot2].next) {
                    if (nodes [slot2].cli != cli &&
                        nodes [slot2].flags & ZOF_VISIBLE && 
                        SubtractClipRect (&sg_UpdateRgn, &nodes [slot2].rc)) {

                        nodes [slot2].age ++;
                        nodes [slot2].flags |= ZOF_REFERENCE;
                    }
                }
                if (!(nodes [0].flags & ZOF_REFERENCE) &&
                        SubtractClipRect (&sg_UpdateRgn, &g_rcScr)) {
                    nodes [0].age ++;
                    nodes [0].flags |= ZOF_REFERENCE;
                }
            }

            unchain_znode (usage_bmp, nodes, slot);

            if (zi->first_normal == slot) {
                zi->first_normal = nodes [slot].next;
            }
            zi->nr_normals --;
        }
    }

    old_active = zi->active_win;
    if (nodes [old_active].cli == cli)
        zi->active_win = 0; /* set the active_win to desktop temp */

    /* unlock zi for change  */
    unlock_zi_for_change (zi);

    /* update all znode if it's dirty */
    do_for_all_znodes (&rc_bound, zi, _cb_update_znode, ZT_ALL);

    if (nodes [0].flags & ZOF_REFERENCE) {
        SendMessage (HWND_DESKTOP, 
                        MSG_ERASEDESKTOP, 0, (WPARAM)&rc_bound);
        nodes [0].flags &= ~ZOF_REFERENCE;
    }

    /* if active_win belongs to the client, change it */
    if (nodes [old_active].cli == cli) {
        int next_active = get_next_visible_mainwin (zi, 0);
        srvSetActiveWindow (nodes [next_active].cli, next_active);
    }

    return 0;
}

int __mg_do_change_topmost_layer (void)
{
    ZORDERINFO* old_zorder_info = __mg_zorder_info;
    ZORDERNODE *new_nodes, *old_nodes;
    unsigned char *old_use_bmp, *new_use_bmp;
    int i;

    srvForceCloseMenu (0);

    old_use_bmp = (unsigned char*)old_zorder_info + sizeof (ZORDERINFO);

    old_nodes = (ZORDERNODE*) (old_use_bmp
                    + old_zorder_info->size_usage_bmp
                    + sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    __mg_zorder_info = mgTopmostLayer->zorder_info;

    lock_zi_for_change (__mg_zorder_info);
    __mg_zorder_info->nr_globals = old_zorder_info->nr_globals;
    __mg_zorder_info->first_global = old_zorder_info->first_global;

    if (old_zorder_info->active_win < __mg_zorder_info->nr_globals) {
        __mg_zorder_info->active_win = old_zorder_info->active_win;
    }

    new_use_bmp = (unsigned char*)__mg_zorder_info + sizeof (ZORDERINFO);
    new_nodes = (ZORDERNODE*) (new_use_bmp
                    + __mg_zorder_info->size_usage_bmp
                    + sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    memcpy (new_use_bmp, old_use_bmp, old_zorder_info->max_nr_globals/8);

    memcpy (new_nodes, old_nodes, 
                    (old_zorder_info->max_nr_globals)*sizeof(ZORDERNODE));

    for (i = old_zorder_info->max_nr_globals;
            i <= old_zorder_info->max_nr_globals + 
            old_zorder_info->max_nr_topmosts + 
            old_zorder_info->max_nr_normals; i++)
    {
        new_nodes [i].age = old_nodes [i].age + 1;
    }

    new_nodes [0].age = old_nodes [0].age + 1;
    unlock_zi_for_change (__mg_zorder_info);
    return 0;
}

/*
 * Add new hosted main window.
 */
static void dskAddNewHostedMainWindow (PMAINWIN pHosting, PMAINWIN pHosted)
{
    PMAINWIN head, prev;
    
    pHosted->pNextHosted = NULL;

    head = pHosting->pFirstHosted;
    if (head) {
        while (head) {
            prev = head;
            head = head->pNextHosted;
        }

        prev->pNextHosted = pHosted;
    }
    else
        pHosting->pFirstHosted = pHosted;

    return;
}

/* 
 * Remove a hosted main window.
 */
void dskRemoveHostedMainWindow (PMAINWIN pHosting, PMAINWIN pHosted)
{
    PMAINWIN head, prev;
    
    head = pHosting->pFirstHosted;
    if (head == pHosted) {
        pHosting->pFirstHosted = head->pNextHosted;

        return;
    }

    while (head) {
        prev = head;
        head = head->pNextHosted;
            
        if (head == pHosted) {
            prev->pNextHosted = head->pNextHosted;
            return;
        }
    }

    return;
}

/*
 * Init a window's global clipping region.
 */
static void dskInitGCRInfo (PMAINWIN pWin)
{
    RECT rcWin, rcTemp;

    dskGetWindowRectInScreen (pWin, &rcWin);

    pWin->pGCRInfo->age = 0;
    pWin->pGCRInfo->old_zi_age = 0;
    InitClipRgn (&pWin->pGCRInfo->crgn, &sgFreeClipRectList);
    IntersectRect (&rcTemp, &rcWin, &g_rcScr);
    SetClipRgn (&pWin->pGCRInfo->crgn, &rcTemp);
}

/*
 * Init a window's invalid region.
 */
static void dskInitInvRgn (PMAINWIN pWin)
{
    pWin->InvRgn.frozen = 0;
    InitClipRgn (&pWin->InvRgn.rgn, &sgFreeInvRectList);
}

static HWND dskGetActiveWindow (int* cli)
{
    int active_cli = -1;
    HWND active = HWND_NULL;
    ZORDERNODE* nodes = (ZORDERNODE*) ((char*)__mg_zorder_info
                    + sizeof (ZORDERINFO) + __mg_zorder_info->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    lock_zi_for_read (__mg_zorder_info);
    if (__mg_zorder_info->active_win) {
        active_cli = nodes [__mg_zorder_info->active_win].cli;
        if (active_cli == __mg_client_id)
            active = nodes [__mg_zorder_info->active_win].hwnd;
        else
            active = HWND_OTHERPROC;

    }

    unlock_zi_for_read (__mg_zorder_info);

    if (cli) *cli = active_cli;

    return active;
}

/*
 * Sets the active main window.
 */
static HWND dskSetActiveWindow (PMAINWIN pWin)
{
    int old_cli;
    HWND old;

    old = dskGetActiveWindow (&old_cli);

    if ((old_cli == __mg_client_id && pWin == (PMAINWIN)old) || 
                    (pWin && (pWin->dwExStyle & WS_EX_TOOLWINDOW)))
        return old;

    if (mgIsServer)
        old = (HWND) srvSetActiveWindow (0, pWin?pWin->idx_znode:0);
    else
        old = (HWND) cliSetActiveWindow (pWin);

    return old;
}

/* 
 * This funciton add the new main window to the z-order list.
 * If new main window is a visible window,
 * this new main window becomes the active window.
 *
 * Return 0 if OK, else -1;
 */
static int dskAddNewMainWindow (PMAINWIN pWin)
{
    RECT rcWin;

    if (mgIsServer) {
        memcpy (&rcWin, &pWin->left, sizeof (RECT));
        pWin->idx_znode = srvAllocZOrderNode (0, (HWND)pWin, (HWND)pWin->pMainWin,
                                              get_znode_flags_from_style (pWin), 
                                              &rcWin, pWin->spCaption);
    }
    else
        pWin->idx_znode = cliAllocZOrderNode (pWin);

    if (pWin->idx_znode <= 0)
        return -1;

    /* Handle main window hosting. */
    if (pWin->pHosting)
        dskAddNewHostedMainWindow (pWin->pHosting, pWin);

    /* Init Global Clip Region info. */
    dskInitGCRInfo (pWin);

    /* Init Invalid Region info. */
    dskInitInvRgn (pWin);

    /* show and active this main window. */
    if (pWin->dwStyle & WS_VISIBLE) {

        SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);

        SendNotifyMessage ((HWND)pWin, MSG_SHOWWINDOW, SW_SHOWNORMAL, 0);

        InvalidateRect ((HWND)pWin, NULL, TRUE);

        dskSetActiveWindow (pWin);
    }

    return 0;
}

/*
 * This function removes a main window from z-order list.
 */
static void dskRemoveMainWindow (PMAINWIN pWin)
{
    if (mgIsServer)
        srvFreeZOrderNode (0, pWin->idx_znode);
    else
        cliFreeZOrderNode (pWin);

    /* Handle main window hosting. */
    if (pWin->pHosting)
        dskRemoveHostedMainWindow (pWin->pHosting, pWin);
}

/*
 * Moves a window to topmost.
 *
 * dskIsTopmost
 * dskMoveToTopmost
 */
static void dskSetPrimitiveChildren (PMAINWIN pWin, BOOL bSet)
{
    PMAINWIN pParent = (PMAINWIN) pWin->hParent;

    if (bSet) {
        while (pParent) {
            pParent->hPrimitive = (HWND)pWin;

            pWin = pParent;
            pParent = (PMAINWIN) pWin->hParent;
        }
    }
    else {
        while (pParent) {
            pParent->hPrimitive = 0;

            pWin = pParent;
            pParent = (PMAINWIN) pWin->hParent;
        }
    }
}

static BOOL dskIsTopmost (PMAINWIN pWin)
{
    BOOL ret = FALSE;

    lock_zi_for_read (__mg_zorder_info);

    if (__mg_zorder_info->first_global == pWin->idx_znode)
        ret = TRUE;
    else if (__mg_zorder_info->first_topmost == pWin->idx_znode)
        ret = TRUE;
    else if (__mg_zorder_info->first_normal == pWin->idx_znode)
        ret = TRUE;

    unlock_zi_for_read (__mg_zorder_info);
    return ret;
}

static void dskHideGlobalControl (PMAINWIN pWin, int reason, LPARAM lParam)
{
    int first = 0;
    ZORDERNODE* nodes = (ZORDERNODE*) ((char*)(__mg_zorder_info + 1) 
                    + __mg_zorder_info->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    
    lock_zi_for_read (__mg_zorder_info);
    switch (nodes [pWin->idx_znode].flags & ZOF_TYPE_MASK) {
        case ZOF_TYPE_GLOBAL:
            first = __mg_zorder_info->first_global;
            break;
        case ZOF_TYPE_TOPMOST:
            first = __mg_zorder_info->first_topmost;
            break;
        case ZOF_TYPE_NORMAL:
            first = __mg_zorder_info->first_normal;
            break;
        default:
            break;
    }
    unlock_zi_for_read (__mg_zorder_info);

    if (first > 0 && !(nodes [first].flags & ZOF_TF_MAINWIN)
                    && (nodes [first].flags & ZOF_VISIBLE)) {

        if (nodes [first].cli == __mg_client_id) {
            RECT rc = nodes [first].rc;
            PMAINWIN pCurTop = (PMAINWIN) nodes [first].hwnd;

            pCurTop->dwStyle &= ~WS_VISIBLE;
            cliHideWindow (pCurTop);
            dskSetPrimitiveChildren (pCurTop, FALSE);
            SendNotifyMessage (pCurTop->hParent, 
                            MSG_CHILDHIDDEN, reason, lParam);

            dskScreenToClient (pCurTop->pMainWin, &rc, &rc);
            InvalidateRect ((HWND)pCurTop->pMainWin, &rc, TRUE);
        }
    }
}

static void dskMoveToTopmost (PMAINWIN pWin, int reason, LPARAM lParam)
{
    if (!pWin) return;

    if (dskIsTopmost (pWin) && (pWin->dwStyle & WS_VISIBLE)) {
        return;
    }

    dskHideGlobalControl (pWin, reason, lParam);

    if (mgIsServer)
        srvMove2Top (0, pWin->idx_znode);
    else
        cliMove2Top (pWin);

    /* activate this main window. */
    if (!(pWin->dwStyle & WS_VISIBLE)) {
        pWin->dwStyle |= WS_VISIBLE;

        if (mgIsServer)
            srvShowWindow (0, pWin->idx_znode);
        else
            cliShowWindow (pWin);

        SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);
    
        InvalidateRect ((HWND)pWin, NULL, TRUE);
    }
    else {
        SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);
    }

    if (reason != RCTM_SHOWCTRL)
        dskSetActiveWindow (pWin);
}

/*
 * Shows/Activates a main window.
 */
static void dskShowMainWindow (PMAINWIN pWin, BOOL bActive)
{
    if (pWin->dwStyle & WS_VISIBLE)
        return;

    pWin->dwStyle |= WS_VISIBLE;

    if (mgIsServer)
        srvShowWindow (0, pWin->idx_znode);
    else
        cliShowWindow (pWin);

    SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);

    InvalidateRect ((HWND)pWin, NULL, TRUE);

    if (bActive)
        dskSetActiveWindow (pWin);

    return; 
}

/*
 * Hides a main window
 */
static void dskHideMainWindow (PMAINWIN pWin)
{
    if (!(pWin->dwStyle & WS_VISIBLE))
        return;

    pWin->dwStyle &= ~WS_VISIBLE;

    if (mgIsServer)
        srvHideWindow (0, pWin->idx_znode);
    else
        cliHideWindow (pWin);
}

static void dskMoveGlobalControl (PMAINWIN pCtrl, RECT* prcExpect)
{
    RECT newWinRect, rcResult;

    SendAsyncMessage ((HWND)pCtrl, MSG_CHANGESIZE, 
                    (WPARAM)(prcExpect), (LPARAM)(&rcResult));
    dskClientToScreen ((PMAINWIN)(pCtrl->hParent), &rcResult, &newWinRect);

    if (mgIsServer)
        srvMoveWindow (0, pCtrl->idx_znode, &newWinRect);
    else
        cliMoveWindow (pCtrl, &newWinRect);

    if (pCtrl->dwStyle & WS_VISIBLE) {
        SendAsyncMessage ((HWND)pCtrl, MSG_NCPAINT, 0, 0);
        InvalidateRect ((HWND)pCtrl, NULL, TRUE);
    }
}

static void dskMoveMainWindow (PMAINWIN pWin, RECT* prcExpect)
{
    RECT oldWinRect, rcResult;

    memcpy (&oldWinRect, &pWin->left, sizeof (RECT));
    SendAsyncMessage ((HWND)pWin, MSG_CHANGESIZE, 
                    (WPARAM)(prcExpect), (LPARAM)(&rcResult));

    if (mgIsServer)
        srvMoveWindow (0, pWin->idx_znode, &rcResult);
    else
        cliMoveWindow (pWin, &rcResult);
}

/* 
 * The callback procedure of tracking menu.
 * It is defined in Menu module.
 */
int PopupMenuTrackProc (PTRACKMENUINFO ptmi, 
                int message, WPARAM wParam, LPARAM lParam);

static void dskForceCloseMenu (void)
{
    ZORDERINFO* zi = __mg_zorder_info;

    if (zi->cli_trackmenu < 0)
        return;

    if (mgIsServer)
        srvForceCloseMenu (0);
    else
        cliForceCloseMenu ();
}

static int dskStartTrackPopupMenu (PTRACKMENUINFO ptmi)
{
    PTRACKMENUINFO plast;

    if (__mg_zorder_info->cli_trackmenu != -1 &&
                    __mg_zorder_info->cli_trackmenu != __mg_client_id) {
        dskForceCloseMenu ();
    }

    if (sg_ptmi) {
        plast = sg_ptmi;
        while (plast->next) {
            plast = plast->next;
        }

        plast->next = ptmi;
        ptmi->prev = plast;
        ptmi->next = NULL;
    }
    else {
        sg_ptmi = ptmi;
        ptmi->next = NULL;
        ptmi->prev = NULL;
    }

    PopupMenuTrackProc (ptmi, MSG_INITMENU, 0, 0);

    if (mgIsServer)
        ptmi->idx_znode = srvStartTrackPopupMenu (0, &ptmi->rc, (HWND)ptmi);
    else
        ptmi->idx_znode = cliStartTrackPopupMenu (ptmi);

    if (ptmi->idx_znode < 0) {
        if (sg_ptmi == ptmi) {
            sg_ptmi = NULL;
        }
        else {
            plast = sg_ptmi;
            while (plast->next) {
                plast = plast->next;
            }
            plast->prev->next = NULL;
            plast = plast->prev;
        }

        return -1;
    }

    PopupMenuTrackProc (ptmi, MSG_SHOWMENU, 0, 0);
    return 0;
}

static int dskEndTrackPopupMenu (PTRACKMENUINFO ptmi)
{
    PTRACKMENUINFO plast = NULL;
    RECT rc;
    
    if (sg_ptmi == ptmi) {
        sg_ptmi = NULL;
    }
    else {
        plast = sg_ptmi;
        while (plast->next) {
            plast = plast->next;
        }
        plast->prev->next = NULL;
        plast = plast->prev;
    }

    if (__mg_zorder_info->cli_trackmenu == __mg_client_id) {
        if (mgIsServer)
            srvEndTrackPopupMenu (0, ptmi->idx_znode);
        else
            cliEndTrackPopupMenu (ptmi);
    }

    PopupMenuTrackProc (ptmi, MSG_HIDEMENU, 0, 0);

    PopupMenuTrackProc (ptmi, MSG_ENDTRACKMENU, 0, 0);

    rc = ptmi->rc;
    ptmi = sg_ptmi;
    while (ptmi) {
        if (DoesIntersect (&rc, &ptmi->rc)) {
            SelectClipRect (HDC_SCREEN, &rc);
            PopupMenuTrackProc (ptmi, MSG_SHOWMENU, 0, 0);
        }
        ptmi = ptmi->next;
    }
    SelectClipRect (HDC_SCREEN, &g_rcScr);

    return 0;
}

static BOOL dskCloseMenu (void)
{
    if (sg_ptmi == NULL)
        return FALSE;

    SendNotifyMessage (sg_ptmi->hwnd, MSG_DEACTIVEMENU, 
                       (WPARAM)sg_ptmi->pmb, (LPARAM)sg_ptmi->pmi);

    PopupMenuTrackProc (sg_ptmi, MSG_CLOSEMENU, 0, 0);

    sg_ptmi = NULL;

    return TRUE;
}

static void dskEnableWindow (PMAINWIN pWin, int flags)
{
    if (!mgIsServer)
        cliEnableWindow (pWin, flags);
    else
        srvEnableWindow (0, pWin?pWin->idx_znode:0, flags);

    if ( (!(pWin->dwStyle & WS_DISABLED) && !flags)
            || ((pWin->dwStyle & WS_DISABLED) && flags) ) {
        if (flags)
            pWin->dwStyle &= ~WS_DISABLED;
        else
            pWin->dwStyle |=  WS_DISABLED;

        if (pWin->dwStyle & WS_DISABLED) {
            if (__mg_capture_wnd && 
                GetMainWindowPtrOfControl (__mg_capture_wnd) == pWin) 
                __mg_capture_wnd = 0;

            if (dskGetActiveWindow (NULL) == (HWND)pWin) {
                dskSetActiveWindow (NULL);
                return;
            }
        }

        SendAsyncMessage ((HWND)pWin, MSG_NCPAINT, 0, 0);
    }
}

/* This function defined in gui/window.c. */
BOOL wndInvalidateRect (HWND hWnd, const RECT* prc, BOOL bEraseBkgnd);

static int dskScrollMainWindow (PMAINWIN pWin, PSCROLLWINDOWINFO pswi)
{
    HDC hdc;
    RECT rcScreen, rcInvalid;
    BOOL inved = FALSE;
    PCLIPRECT pcrc;

    if (!pWin->dwStyle & WS_VISIBLE)
        return 0;

    lock_zi_for_read (__mg_zorder_info);

    dskClientToScreen (pWin, pswi->rc1, &rcScreen);

    hdc = GetClientDC ((HWND)pWin);

    pcrc = GetGCRgnInfo ((HWND)pWin)->crgn.head;
    while (pcrc) {
        RECT rcMove;

        if (!IntersectRect (&rcMove, &pcrc->rc, &rcScreen)) {
            pcrc = pcrc->next;
            continue;
        }

        dskScreenToClient (pWin, &rcMove, &rcMove);
        if (!IntersectRect (&rcMove, &rcMove, pswi->rc1)) {
            pcrc = pcrc->next;
            continue;
        }

        SelectClipRect (hdc, &rcMove);

        BitBlt (hdc, rcMove.left, rcMove.top, 
            rcMove.right - rcMove.left,
            rcMove.bottom - rcMove.top,
            hdc, pswi->iOffx + rcMove.left, pswi->iOffy + rcMove.top, 0);

        pcrc = pcrc->next;
    }
    ReleaseDC (hdc);

    pcrc = GetGCRgnInfo ((HWND)pWin)->crgn.head;
    while (pcrc) {
        RECT rcMove;
        if (!IntersectRect (&rcMove, &pcrc->rc, &rcScreen)) {
            pcrc = pcrc->next;
            continue;
        }

        dskScreenToClient (pWin, &rcMove, &rcMove);

        rcInvalid = rcMove;
        if (pswi->iOffx < 0) {
            rcInvalid.left = rcInvalid.right + pswi->iOffx;
            wndInvalidateRect ((HWND)pWin, &rcInvalid, TRUE);
            inved = TRUE;
        }
        else if (pswi->iOffx > 0) {
            rcInvalid.right = rcInvalid.left + pswi->iOffx;
            wndInvalidateRect ((HWND)pWin, &rcInvalid, TRUE);
            inved = TRUE;
        }
    
        if (pswi->iOffy < 0) {
            rcInvalid.top = rcInvalid.bottom + pswi->iOffy;
            wndInvalidateRect ((HWND)pWin, &rcInvalid, TRUE);
            inved = TRUE;
        }
        else if (pswi->iOffy > 0) {
            rcInvalid.bottom = rcInvalid.top + pswi->iOffy;
            wndInvalidateRect ((HWND)pWin, &rcInvalid, TRUE);
            inved = TRUE;
        }

        pcrc = pcrc->next;
    }

    unlock_zi_for_read (__mg_zorder_info);

    if (inved)
        PostMessage ((HWND)pWin, MSG_PAINT, 0, 0);

    return 0;
}

static HWND dskGetCaptureWindow (void)
{
    return __mg_capture_wnd;
}

static HWND dskSetCaptureWindow (PMAINWIN pWin)
{
    HWND old;

    old = __mg_capture_wnd;
    __mg_capture_wnd = (HWND)pWin;

    return old;
}

static HWND dskGetNextMainWindow (PMAINWIN pWin)
{
    ZORDERNODE* nodes;
    int next;
    int from = pWin?pWin->idx_znode:0;

    nodes = (ZORDERNODE*) ((char*)(__mg_zorder_info + 1) + 
                    __mg_zorder_info->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    if (from) {
        next = nodes [from].next;

        while (next) {
            if (nodes [next].flags & ZOF_TF_MAINWIN)
                return nodes [next].hwnd;

            next = nodes [next].next;
        }
    }

    next = __mg_zorder_info->first_global;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN)
            return nodes [next].hwnd;

        next = nodes [next].next;
    }

    next = __mg_zorder_info->first_topmost;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN)
            return nodes [next].hwnd;

        next = nodes [next].next;
    }

    next = __mg_zorder_info->first_normal;
    while (next) {
        if (nodes [next].flags & ZOF_TF_MAINWIN)
            return nodes [next].hwnd;

        next = nodes [next].next;
    }

    return HWND_NULL;
}

static int dskStartDragWindow (PMAINWIN pWin, const DRAGINFO* drag_info)
{
    int ret;

    if (!(pWin->dwStyle & WS_VISIBLE))
        return -1;

    if (mgIsServer)
        ret = srvStartDragWindow (0, pWin->idx_znode, drag_info->location,
                        drag_info->init_x, drag_info->init_y);
    else
        ret = cliStartDragWindow (pWin, drag_info);

    return ret;
}

static int dskCancelDragWindow (PMAINWIN pWin)
{
    int ret;

    if (!(pWin->dwStyle & WS_VISIBLE))
        return -1;

    if (mgIsServer)
        ret = srvCancelDragWindow (0, pWin->idx_znode);
    else
        ret = cliCancelDragWindow (pWin);

    return ret;
}

static int dskChangeCaption (PMAINWIN pWin)
{
    if (mgIsServer) {
        return srvChangeCaption (0, pWin->idx_znode, pWin->spCaption);
    }
    else
        return cliChangeCaption (pWin);
}

static int dskWindowMessageHandler (int message, PMAINWIN pWin, LPARAM lParam)
{
    switch (message) {
        case MSG_ADDNEWMAINWIN:
            return dskAddNewMainWindow (pWin);

        case MSG_REMOVEMAINWIN:
            dskRemoveMainWindow (pWin);
            break;

        case MSG_MOVETOTOPMOST:
            dskMoveToTopmost (pWin, RCTM_MESSAGE, 0);
            break;

        case MSG_SHOWMAINWIN:
            dskShowMainWindow (pWin, TRUE);
            break;
            
        case MSG_HIDEMAINWIN:
            dskHideMainWindow (pWin);
            break;

        case MSG_MOVEMAINWIN:
            if (pWin->WinType == TYPE_CONTROL)
                dskMoveGlobalControl (pWin, (RECT*)lParam);
            else
                dskMoveMainWindow (pWin, (RECT*)lParam);
            break;

        case MSG_GETACTIVEMAIN:
            return (int)dskGetActiveWindow (NULL);
        
        case MSG_SETACTIVEMAIN:
            return (int)dskSetActiveWindow (pWin);

        case MSG_GETCAPTURE:
            return (int)dskGetCaptureWindow ();

        case MSG_SETCAPTURE:
            return (int)dskSetCaptureWindow (pWin);
 
        case MSG_TRACKPOPUPMENU:
            return dskStartTrackPopupMenu ((PTRACKMENUINFO)lParam);

        case MSG_ENDTRACKMENU:
            return dskEndTrackPopupMenu ((PTRACKMENUINFO)lParam);

        case MSG_CLOSEMENU:
            dskCloseMenu ();
            break;

        case MSG_SCROLLMAINWIN:
            dskScrollMainWindow (pWin, (PSCROLLWINDOWINFO)lParam);
            break;

        case MSG_CARET_CREATE:
            sg_hCaretWnd = (HWND)pWin;
            sg_uCaretBTime = pWin->pCaretInfo->uTime;
            return 0;

        case MSG_CARET_DESTROY:
            sg_hCaretWnd = 0;
            return 0;

        case MSG_ENABLEMAINWIN:
            dskEnableWindow (pWin, lParam);
            break;
        
        case MSG_ISENABLED:
            return !(pWin->dwStyle & WS_DISABLED);
        
        case MSG_SETWINCURSOR:
            {
                HCURSOR old = pWin->hCursor;

                pWin->hCursor = (HCURSOR)lParam;
                return old;
            }

        case MSG_GETNEXTMAINWIN:
            return (int)dskGetNextMainWindow (pWin);

        case MSG_SHOWGLOBALCTRL:
            {
                dskMoveGlobalControl (pWin, (RECT*)&(pWin->left));
                dskMoveToTopmost (pWin, RCTM_SHOWCTRL, 0);
                dskSetPrimitiveChildren (pWin, TRUE);
                break;
            }

        case MSG_HIDEGLOBALCTRL:
            dskHideMainWindow (pWin);
            dskSetPrimitiveChildren (pWin, FALSE);
            break;

        case MSG_STARTDRAGWIN:
            return dskStartDragWindow (pWin, (DRAGINFO*)lParam);

        case MSG_CANCELDRAGWIN:
            return dskCancelDragWindow (pWin);

        case MSG_CHANGECAPTION:
            return dskChangeCaption (pWin);
   }

   return 0;
}

/*********************** Hook support ****************************************/
typedef struct HookInfo
{
    int cli;
    HWND hwnd;
    DWORD flag;
} HOOKINFO;

static HOOKINFO keyhook = {0, HWND_NULL, 0};
static HOOKINFO mousehook = {0, HWND_NULL, 0};

HWND __mg_do_reghook_operation (int cli, const REGHOOKINFO* info)
{
    HWND ret = HWND_NULL;

    switch (info->id_op) {
        case ID_REG_KEY:
            ret = keyhook.hwnd;
            keyhook.cli = cli;
            keyhook.hwnd = info->hwnd;
            keyhook.flag = info->flag;
            break;

        case ID_REG_MOUSE:
            ret = mousehook.hwnd;
            mousehook.cli = cli;
            mousehook.hwnd = info->hwnd;
            mousehook.flag = info->flag;
            break;

        default:
            break;
    }
    return ret;
}

static int srvHandleKeyHook (int message, WPARAM wParam, LPARAM lParam)
{
    MSG msg;

    if (keyhook.cli <= 0 || keyhook.hwnd == HWND_NULL)
        return HOOK_GOON;

    msg.hwnd = keyhook.hwnd;
    msg.message = message;
    msg.wParam = wParam;
    msg.lParam = lParam;

    Send2Client (&msg, keyhook.cli);
    return keyhook.flag;
}

int __mg_handle_mouse_hook (int message, WPARAM wParam, LPARAM lParam)
{
    MSG msg;
    if (mousehook.cli <= 0 || mousehook.hwnd == HWND_NULL)
        return HOOK_GOON;

    msg.hwnd = mousehook.hwnd;
    msg.message = message;
    msg.wParam = wParam;
    msg.lParam = lParam;

    Send2Client (&msg, mousehook.cli);

    return mousehook.flag;
}

/* used by client to check the validation of a hwnd */
BOOL __mg_client_check_hwnd (HWND hwnd, int cli)
{
    ZORDERINFO* zi = __mg_zorder_info;
    int slot;

    ZORDERNODE* nodes = (ZORDERNODE*) ((char*)zi + sizeof (ZORDERINFO)
                    + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    lock_zi_for_read (zi);

    slot = zi->first_topmost;
    for (; slot > 0; slot = nodes [slot].next) {
        if (hwnd == nodes [slot].hwnd && cli == nodes [slot].cli) {
            goto ret_true;
        }
    }

    slot = zi->first_normal;
    for (; slot > 0; slot = nodes [slot].next) {
        if (hwnd == nodes [slot].hwnd && cli == nodes [slot].cli) {
            goto ret_true;
        }
    }

    unlock_zi_for_read (zi);
    return FALSE;

ret_true:
    unlock_zi_for_read (zi);
    return TRUE;
}

/*
 * Key message handling.
 *
 * handle_special_key
 * srvKeyMessageHandler
 */
static int handle_special_key (int scancode)
{
    switch (scancode) {
    case SCANCODE_BACKSPACE:
        ExitGUISafely (-1);
        return 0;
    }

    return 0;
}

static int srvKeyMessageHandler (int message, int scancode, DWORD status)
{
    int next_node;
    static int mg_altdown = 0;
    static int mg_modal = 0;

    if ((message == MSG_KEYDOWN) && (status & KS_ALT) && (status & KS_CTRL))
        return handle_special_key (scancode);

    if (scancode == SCANCODE_LEFTALT || scancode == SCANCODE_RIGHTALT) {
        if (message == MSG_KEYDOWN) {
            mg_altdown = 1;
            return 0;
        }
        else {
            mg_altdown = 0;
            if (mg_modal == 1) {
                mg_modal = 0;
                return 0;
            }
        }
    }

    if (mg_altdown) {
        if (message == MSG_KEYDOWN) {
            if (scancode == SCANCODE_TAB) {
                mg_modal = 1;
                srvForceCloseMenu (0);

                next_node = get_next_visible_mainwin (__mg_zorder_info,
                                    __mg_zorder_info->active_win);
                if (next_node) {
                    srvMove2Top (0, next_node);
                    srvSetActiveWindow (0, next_node);
                }
                return 0;
            }
            else if (scancode == SCANCODE_ESCAPE) {

                mg_modal = 1;

                if (__mg_zorder_info->active_win) {
                    __mg_post_msg_by_znode (__mg_zorder_info,
                                    __mg_zorder_info->active_win,
                                    MSG_CLOSE, 0, 0);
                    return 0;
                }
            }
        }
        else if (mg_modal == 1)
            return 0;
    }
    
    if (scancode == SCANCODE_LEFTALT
             || scancode == SCANCODE_RIGHTALT || mg_altdown) {
        if (message == MSG_KEYDOWN)
            message = MSG_SYSKEYDOWN;
        else {
            message = MSG_SYSKEYUP;
            mg_altdown = 0;
        }
    }

    if (srvHandleKeyHook (message, 
                            (WPARAM)scancode, (LPARAM)status) == HOOK_STOP)
        return 0;

    if (__mg_ime_wnd) {
        PostMessage (__mg_ime_wnd, 
                        message, (WPARAM)scancode, (LPARAM)status);
        return 0;
    }

    if (__mg_zorder_info->active_win) {
        __mg_post_msg_by_znode (__mg_zorder_info,
                        __mg_zorder_info->active_win, message, 
                        (WPARAM)scancode, (LPARAM)status);
    }
    else {
        SendMessage (HWND_DESKTOP, MSG_DT_KEYOFF + message, 
                        (WPARAM)scancode, (LPARAM)status);
    }

    return 0;
}

/*
 * Mouse message handling.
 *
 * extern:
 *      get_znode_at_point
 *      __mg_znode_at_point
 *      GetMainWindowPtrUnderPoint
 *
 * static: dskMouseMessageHandler 
 */

static int get_znode_at_point (const ZORDERINFO* zi, const ZORDERNODE* nodes,
                int x, int y)
{
    int slot = 0;

    slot = zi->first_global;
    for (; slot > 0; slot = nodes [slot].next) {
        if (nodes [slot].flags & ZOF_VISIBLE && 
                        PtInRect (&nodes [slot].rc, x, y))
            goto ret;
    }

    slot = zi->first_topmost;
    for (; slot > 0; slot = nodes [slot].next) {
        if (nodes [slot].flags & ZOF_VISIBLE &&
                        PtInRect (&nodes [slot].rc, x, y))
            goto ret;
    }

    slot = zi->first_normal;
    for (; slot > 0; slot = nodes [slot].next) {
        if (nodes [slot].flags & ZOF_VISIBLE &&
                        PtInRect (&nodes [slot].rc, x, y)) {
            goto ret;
        }
    }

ret:
    return slot;
}

int __mg_get_znode_at_point (const ZORDERINFO* zi, int x, int y, HWND* hwnd)
{
    const ZORDERNODE* nodes = (const ZORDERNODE*) ((char*)zi 
                    + sizeof (ZORDERINFO) + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);
    int slot;
    
    if (zi->cli_trackmenu >= 0)
        return -1;

    slot = get_znode_at_point (zi, nodes, x, y);
    if (slot == 0)
        return -1;

    if (hwnd) *hwnd = nodes [slot].hwnd;
    return nodes [slot].cli;
}

int __mg_handle_normal_mouse_move (const ZORDERINFO* zi, int x, int y)
{
    const ZORDERNODE* nodes = (const ZORDERNODE*) ((char*)zi 
                    + sizeof (ZORDERINFO) + zi->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

    static int old_slot, old_cli;
    static int old_hwnd;
    int cur_slot;
    int cur_cli = 0;

    if (zi->cli_trackmenu >= 0)
        return zi->cli_trackmenu;

    cur_slot = get_znode_at_point (zi, nodes, x, y);

    if (old_slot != cur_slot) {
        if (old_slot > 0 && nodes [old_slot].cli == old_cli &&
                    nodes [old_slot].hwnd == old_hwnd) {
            post_msg_by_znode_p (zi, nodes + old_slot,
                        MSG_MOUSEMOVEIN, FALSE, 0);

            post_msg_by_znode_p (zi, nodes + old_slot,
                        MSG_NCMOUSEMOVE, HT_OUT, MAKELONG (x, y));
        }

        if (cur_slot > 0) {
            HWND hwnd;

            if (nodes [cur_slot].flags & ZOF_DISABLED) {
                HCURSOR def_cursor = GetDefaultCursor ();
                if (def_cursor)
                    SetCursor (def_cursor);
            }
            else {
                cur_cli = nodes [cur_slot].cli;
            }

            if (nodes [cur_slot].cli == old_cli)
                hwnd = old_hwnd;
            else
                hwnd = HWND_OTHERPROC;

            post_msg_by_znode_p (zi, nodes + cur_slot, 
                            MSG_MOUSEMOVEIN, TRUE, (LPARAM)hwnd);
        }
        else 
            SetCursor (GetSystemCursor (IDC_ARROW));
    }
    else if (cur_slot > 0) {
        if (nodes [cur_slot].flags & ZOF_DISABLED) {
            HCURSOR def_cursor = GetDefaultCursor ();
            if (def_cursor)
                SetCursor (def_cursor);
        }
        else {
            cur_cli = nodes [cur_slot].cli;
        }
    }

    old_slot = cur_slot;
    if (cur_slot > 0) {
        old_cli = nodes [cur_slot].cli;
        old_hwnd = nodes [cur_slot].hwnd;
    }
    else {
        old_cli = -1;
        old_hwnd = 0;
    }

    return cur_cli;
}

PMAINWIN GetMainWindowPtrUnderPoint (int x, int y)
{
    HWND hwnd;

    if (__mg_get_znode_at_point (__mg_zorder_info, x, y, &hwnd) 
                    == __mg_client_id) {
        return (PMAINWIN)hwnd;
    }

    return NULL;
}

/* defined in ../gui/window.c */
extern void __mg_reset_mainwin_capture_info (PCONTROL ctrl);

static PMAINWIN mgs_captured_main_win = (void*)HWND_INVALID;
static int mgs_captured_by;

void __mg_reset_desktop_capture_info (PMAINWIN pWin)
{
    if (pWin == mgs_captured_main_win) {
        mgs_captured_main_win = (void*)HWND_INVALID;
        mgs_captured_by = 0;
    }

    if ((HWND)pWin == __mg_capture_wnd)
        __mg_capture_wnd = 0;

    __mg_reset_mainwin_capture_info ((PCONTROL)pWin);
}

static int check_capture (int message)
{
    if (mgs_captured_main_win != (void*)HWND_INVALID
                    && mgs_captured_main_win != NULL) {

        switch (message) {
            case MSG_LBUTTONDOWN:
            case MSG_RBUTTONDOWN:
                if (mgs_captured_by != message)
                    return 1;
                break;

            case MSG_LBUTTONUP:
                if (mgs_captured_by != MSG_LBUTTONDOWN)
                    return 1;
                break;

            case MSG_RBUTTONUP:
                if (mgs_captured_by != MSG_RBUTTONDOWN)
                    return 1;
                break;
        }
    }

    return 0;
}

static int dskMouseMessageHandler (int message, WPARAM flags, int x, int y)
{
    PMAINWIN pUnderPointer;
    PMAINWIN pCtrlPtrIn;
    int CapHitCode = HT_UNKNOWN;
    int UndHitCode = HT_UNKNOWN;
    int cx = 0, cy = 0;

    if (__mg_capture_wnd) {
        PostMessage (__mg_capture_wnd, 
            message, flags | KS_CAPTURED, MAKELONG (x, y));
        return 0;
    }

    if (mgs_captured_main_win != (void*)HWND_INVALID 
                    && mgs_captured_main_win != NULL) {
        CapHitCode = SendAsyncMessage((HWND)mgs_captured_main_win, 
                        MSG_HITTEST, (WPARAM)x, (LPARAM)y);
    }

    pCtrlPtrIn = GetMainWindowPtrUnderPoint (x, y);

    if (pCtrlPtrIn && pCtrlPtrIn->WinType == TYPE_CONTROL) {
        pUnderPointer = pCtrlPtrIn->pMainWin;
        UndHitCode = HT_CLIENT;
        cx = x - pUnderPointer->cl;
        cy = y - pUnderPointer->ct;
    }
    else {
        pUnderPointer = pCtrlPtrIn;
        pCtrlPtrIn = NULL;

        if (pUnderPointer) {
            UndHitCode = SendAsyncMessage((HWND)pUnderPointer, MSG_HITTEST, 
                                        (WPARAM)x, (LPARAM)y);
            cx = x - pUnderPointer->cl;
            cy = y - pUnderPointer->ct;
        }
    }

    switch (message) {
        case MSG_MOUSEMOVE:
            if (mgs_captured_main_win != (void *)HWND_INVALID) {
                if (mgs_captured_main_win)
                    PostMessage((HWND)mgs_captured_main_win, MSG_NCMOUSEMOVE, 
                                CapHitCode, MAKELONG (x, y));
                else
                    PostMessage(HWND_DESKTOP, MSG_DT_MOUSEMOVE,
                                pUnderPointer == NULL, MAKELONG (x, y));
                break;
            }

            if (pUnderPointer) {
                HCURSOR def_cursor = GetDefaultCursor ();
                if (UndHitCode == HT_CLIENT) {
                    if (def_cursor)
                        SetCursor (def_cursor);
                    PostMessage ((HWND)pUnderPointer, MSG_SETCURSOR, 
                            UndHitCode, MAKELONG (cx, cy));
                    PostMessage((HWND)pUnderPointer, MSG_NCMOUSEMOVE, 
                            UndHitCode, MAKELONG (x, y));
                    PostMessage((HWND)pUnderPointer, MSG_MOUSEMOVE, 
                            flags, MAKELONG (cx, cy));
                }
                else {
                    if (def_cursor)
                        SetCursor (def_cursor);
                    PostMessage ((HWND)pUnderPointer, MSG_NCSETCURSOR, 
                            UndHitCode, MAKELONG (x, y));
                    PostMessage((HWND)pUnderPointer, MSG_NCMOUSEMOVE, 
                            UndHitCode, MAKELONG (x, y));
                }
            }
        break;

        case MSG_LBUTTONDOWN:
        case MSG_RBUTTONDOWN:
            if (check_capture (message)) /* ignore the event */
                break;

            if (pUnderPointer) {
                dskForceCloseMenu ();

                if (pUnderPointer->dwStyle & WS_DISABLED) {
                    Ping ();
                    break;
                }

                if (pCtrlPtrIn == NULL) {
                    if (!dskIsTopmost (pUnderPointer)) {
                        dskMoveToTopmost (pUnderPointer, 
                                        RCTM_CLICK, MAKELONG (x, y));
                    }
                
                    if (pUnderPointer != 
                            (PMAINWIN)dskSetActiveWindow (pUnderPointer))
                        PostMessage ((HWND) pUnderPointer,
                                MSG_MOUSEACTIVE, UndHitCode, 0);
                }
                
                if (UndHitCode != HT_CLIENT) {
                    if (UndHitCode & HT_NEEDCAPTURE) {
                        mgs_captured_main_win = pUnderPointer;
                        mgs_captured_by = message;
                    }
                    else
                        mgs_captured_main_win = (void*)HWND_INVALID;

                    PostMessage ((HWND)pUnderPointer, message + MSG_NCMOUSEOFF,
                            UndHitCode, MAKELONG (x, y));
                }
                else {
                    PostMessage((HWND)pUnderPointer, message, 
                        flags, MAKELONG(cx, cy));
                    mgs_captured_main_win = (void*)HWND_INVALID;
                }
            }
            else {
                dskSetActiveWindow (NULL);
                mgs_captured_main_win = NULL;
                PostMessage (HWND_DESKTOP, message + MSG_DT_MOUSEOFF,
                            flags, MAKELONG (x, y));
            }
        break;

        case MSG_LBUTTONUP:
        case MSG_RBUTTONUP:
            if (check_capture (message)) /* ignore the event */
                break;

            if (mgs_captured_main_win != (void*)HWND_INVALID) {
                if (mgs_captured_main_win)
                    PostMessage ((HWND)mgs_captured_main_win, 
                        message + MSG_NCMOUSEOFF,
                        CapHitCode, MAKELONG (x, y));
                else if (!pUnderPointer)
                    PostMessage (HWND_DESKTOP, message + MSG_DT_MOUSEOFF,
                        flags, MAKELONG (x, y));
                
                mgs_captured_main_win = (void*)HWND_INVALID;
                break;
            }
            else {
                if (pUnderPointer) {
                    if (pUnderPointer->dwStyle & WS_DISABLED) {
                        break;
                    }
                    
                    if (UndHitCode == HT_CLIENT) {
                        PostMessage((HWND)pUnderPointer, message, 
                            flags, MAKELONG (cx, cy));
                    }
                    else {
                        PostMessage((HWND)pUnderPointer, 
                            message + MSG_NCMOUSEOFF, 
                            UndHitCode, MAKELONG (x, y));
                    }
                }
                else
                    PostMessage (HWND_DESKTOP, message + MSG_DT_MOUSEOFF,
                        flags, MAKELONG (x, y));
            }
        break;
        
        case MSG_LBUTTONDBLCLK:
        case MSG_RBUTTONDBLCLK:
            if (pUnderPointer)
            {
                if (pUnderPointer->dwStyle & WS_DISABLED) {
                    Ping ();
                    break;
                }

                if(UndHitCode == HT_CLIENT)
                    PostMessage((HWND)pUnderPointer, message, 
                        flags, MAKELONG(cx, cy));
                else
                    PostMessage((HWND)pUnderPointer, message + MSG_NCMOUSEOFF, 
                        UndHitCode, MAKELONG (x, y));
            }
            else {
                PostMessage(HWND_DESKTOP, message + MSG_DT_MOUSEOFF, 
                        flags, MAKELONG (x, y));
            }
        break;

    }

    return 0;
}

/***********************************************************
 * Common message handling for the server and all clients.
 **********************************************************/

static BOOL _cb_bcast_msg (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    PMAINWIN pWin;
    PMSG pMsg = (PMSG)context;

    if (node->cli == __mg_client_id) {
        pWin = (PMAINWIN)node->hwnd;
        if (pWin->WinType != TYPE_CONTROL) {
            PostMessage ((HWND)pWin, pMsg->message, pMsg->wParam, pMsg->lParam);
            return TRUE;
        }
    }

    return FALSE;
}

static int dskBroadcastMessage (PMSG pMsg)
{
    int count = 0;
    
    lock_zi_for_read (__mg_zorder_info);
    count = do_for_all_znodes (pMsg, __mg_zorder_info, _cb_bcast_msg, ZT_ALL);
    unlock_zi_for_read (__mg_zorder_info);

    return count;
}

static int dskOnNewCtrlInstance (PCONTROL pParent, PCONTROL pNewCtrl)
{
    PCONTROL pFirstCtrl, pLastCtrl;
 
    if (pNewCtrl->dwExStyle & WS_EX_CTRLASMAINWIN) {
        RECT rcWin;

        dskGetWindowRectInScreen ((PMAINWIN)pNewCtrl, &rcWin);

        /* Add to z-order. */
        if (mgIsServer)
            pNewCtrl->idx_znode = srvAllocZOrderNode (0, 
                            (HWND)pNewCtrl, (HWND)pNewCtrl->pMainWin, 
                            get_znode_flags_from_style ((PMAINWIN)pNewCtrl), 
                            &rcWin, pNewCtrl->spCaption);
        else
            pNewCtrl->idx_znode = cliAllocZOrderNode ((PMAINWIN)pNewCtrl);

        if (pNewCtrl->idx_znode <= 0)
            return -1;

        /* Init Global Clip Region info. */
        dskInitGCRInfo ((PMAINWIN)pNewCtrl);
    }

    pFirstCtrl = pParent->children;

    pNewCtrl->next = NULL;
    
    if (!pFirstCtrl) {
        pParent->children = pNewCtrl;
        pNewCtrl->prev = NULL;
    }
    else {
        pLastCtrl = pFirstCtrl;
        
        while (pLastCtrl->next)
            pLastCtrl = pLastCtrl->next;
            
        pLastCtrl->next = pNewCtrl;
        pNewCtrl->prev = pLastCtrl;
    }

    dskInitInvRgn ((PMAINWIN)pNewCtrl);

    pNewCtrl->pcci->nUseCount ++;

    return 0;
}

static int dskOnRemoveCtrlInstance (PCONTROL pParent, PCONTROL pCtrl)
{
    PCONTROL pFirstCtrl;
    BOOL fFound = FALSE;

    pFirstCtrl = pParent->children;

    if (!pFirstCtrl)
        return -1;
    else {
        if (pFirstCtrl == pCtrl) {
            pParent->children = pCtrl->next;
            if (pCtrl->next)
                pCtrl->next->prev = NULL;
            fFound = TRUE;
        }
        else {
            while (pFirstCtrl->next) {
                if (pFirstCtrl->next == pCtrl) {
                    pFirstCtrl->next = pCtrl->next;
                    if (pFirstCtrl->next)
                        pFirstCtrl->next->prev = pCtrl->prev;
                    fFound = TRUE;
                    break;
                }

                pFirstCtrl = pFirstCtrl->next;
            }
        }
    }

    /* remove from z-order */
    if (pCtrl->dwExStyle & WS_EX_CTRLASMAINWIN) {
        if (mgIsServer)
            srvFreeZOrderNode (0, pCtrl->idx_znode);
        else
            cliFreeZOrderNode ((PMAINWIN)pCtrl);
    }

    if (fFound) {
        pCtrl->pcci->nUseCount --;
        return 0;
    }

    if ((HWND)pCtrl == __mg_capture_wnd)
        /* force release the capture */
        __mg_capture_wnd = 0;

    return -1;
}

/***********************************************************
 * Session message handling for the server.
 **********************************************************/

static int srvGetBgPicturePos (void)
{
    char szValue [21];

    if( GetMgEtcValue ("bgpicture", "position", szValue, 20) < 0 ) {
        strcpy (szValue, "center");
    }

    if (!strcmp (szValue, "none"))
        return -1;
    if (!strcmp (szValue, "center"))
        return 0;
    if (!strcmp (szValue, "upleft"))
        return 1;
    if (!strcmp (szValue, "downleft"))
        return 2;
    if (!strcmp (szValue, "upright"))
        return 3;
    if (!strcmp (szValue, "downright"))
        return 4;
    if (!strcmp (szValue, "upcenter"))
        return 5;
    if (!strcmp (szValue, "downcenter"))
        return 6;
    if (!strcmp (szValue, "vcenterleft"))
        return 7;
    if (!strcmp (szValue, "vcenterright"))
        return 8;

    return -1;
}

static void srvGetBgPictureXY (int pos, int w, int h, int* x, int* y)
{
    switch (pos) {
    case 0: /* center */
        *x = (g_rcScr.right - w) >> 1;
        *y = (g_rcScr.bottom - h) >> 1;
        break;
    case 1: /* upleft */
        *x = 0;
        *y = 0;
        break;
    case 2: /* downleft */
        *x = 0;
        *y = g_rcScr.bottom - h;
        break;
    case 3: /* upright */
        *x = g_rcScr.right - w;
        *y = 0;
        break;
    case 4: /* downright */
        *x = g_rcScr.right - w;
        *y = g_rcScr.bottom - h;
        break;
    case 5: /* upcenter */
        *x = (g_rcScr.right - w) >> 1;
        *y = 0;
        break;
    case 6: /* downcenter */
        *x = (g_rcScr.right - w) >> 1;
        *y = g_rcScr.bottom - h;
        break;
    case 7: /* vcenterleft */
        *x = 0;
        *y = (g_rcScr.bottom - h) >> 1;
        break;
    case 8: /* vcenterright */
        *x = g_rcScr.right - w;
        *y = (g_rcScr.bottom - h) >> 1;
        break;
    default:
        *x = 0;
        *y = 0;
        break;
    }
}

static BITMAP srv_bg_picture;

static PBITMAP srvLoadBgPicture (void)
{
    char path_name [129];

    if (GetMgEtcValue ("bgpicture", "file", path_name, 128) < 0 ) {
        return NULL;
    }

    if (LoadBitmapFromFile (HDC_SCREEN, &srv_bg_picture, path_name))
        return NULL;

    return &srv_bg_picture;
}

#define IDM_REDRAWBG    MINID_RESERVED
#define IDM_CLOSEALLWIN (MINID_RESERVED + 1)
#define IDM_ENDSESSION  (MINID_RESERVED + 2)

#define IDM_FIRSTWINDOW (MINID_RESERVED + 101)

#define IDM_SWITCH_LAYER (MINID_RESERVED + 201)

#define IDM_DELETE_LAYER (MINID_RESERVED + 301)

static HMENU sg_srvDesktopMenu;

/*
 * When the user clicks right mouse button on the server desktop, 
 * MiniGUI will display a menu for the user. You can use this 
 * function to customize the desktop menu. e.g. add a new 
 * menu item.
 *
 * Please use an integer larger than IDM_DTI_FIRST as the 
 * command ID.
 */

#define IDC_DTI_ABOUT   (IDM_DTI_FIRST)

static void CustomizeDesktopMenuDefault (HMENU hmnu, int iPos)
{
#ifdef _MISC_ABOUTDLG
    MENUITEMINFO mii;

    memset (&mii, 0, sizeof(MENUITEMINFO));
    mii.type        = MFT_STRING;
    mii.id          = IDC_DTI_ABOUT;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_ABOUTMG);
    mii.hsubmenu    = 0;
    InsertMenuItem (hmnu, iPos, TRUE, &mii);
#endif
}

/*
 * When user choose a custom menu item on desktop menu,
 * MiniGUI will call this function, and pass the command ID
 * of selected menu item.
 */
static int CustomDesktopCommandDefault (int id)
{
#ifdef _MISC_ABOUTDLG
    if (id == IDC_DTI_ABOUT)
        OpenAboutDialog (HWND_DESKTOP);
#endif

    return 0;
}

CustomizeDesktopMenuFunc CustomizeDesktopMenu = CustomizeDesktopMenuDefault;
CustomDesktopCommandFunc CustomDesktopCommand = CustomDesktopCommandDefault;

static HMENU srvCreateWindowSubMenu (void)
{
    HMENU hmnu;
    MENUITEMINFO mii;

    memset (&mii, 0, sizeof(MENUITEMINFO));
    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_WINDOW);

    hmnu = CreatePopupMenu (&mii);
    return hmnu;
}

static HMENU srvCreateLayerSubMenu (BOOL flag)
{
    HMENU hmnu;
    MENUITEMINFO mii;

    memset (&mii, 0, sizeof(MENUITEMINFO));
    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = 
        flag ? (DWORD)GetSysText(IDS_MGST_SWITCHLAYER)
             : (DWORD)GetSysText(IDS_MGST_DELLAYER);

    hmnu = CreatePopupMenu (&mii);
    return hmnu;
}

static HMENU srvCreateDesktopMenu (void)
{
    HMENU hmnu;
    MENUITEMINFO mii;

    memset (&mii, 0, sizeof(MENUITEMINFO));
    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_START);

    hmnu = CreatePopupMenu (&mii);

    memset (&mii, 0, sizeof(MENUITEMINFO));
    mii.type        = MFT_STRING;
    mii.id          = IDM_REDRAWBG;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_REFRESH); 
    InsertMenuItem (hmnu, 0, TRUE, &mii);

    mii.type        = MFT_STRING;
    mii.id          = IDM_CLOSEALLWIN;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_CLOSEALLWIN); 
    InsertMenuItem (hmnu, 1, TRUE, &mii);

    mii.type        = MFT_STRING;
    mii.id          = IDM_ENDSESSION;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_ENDSESSION);
    InsertMenuItem (hmnu, 2, TRUE, &mii);

    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_WINDOW);
    mii.hsubmenu     = srvCreateWindowSubMenu();
    InsertMenuItem (hmnu, 3, TRUE, &mii);

    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_SWITCHLAYER);
    mii.hsubmenu     = srvCreateLayerSubMenu(TRUE);
    InsertMenuItem (hmnu, 4, TRUE, &mii);

    mii.type        = MFT_STRING;
    mii.id          = 0;
    mii.typedata    = (DWORD)GetSysText(IDS_MGST_DELLAYER);
    mii.hsubmenu     = srvCreateLayerSubMenu(FALSE);
    InsertMenuItem (hmnu, 5, TRUE, &mii);

    mii.type        = MFT_SEPARATOR;
    mii.id          = 0;
    mii.typedata    = 0;
    mii.hsubmenu    = 0;
    InsertMenuItem (hmnu, 6, TRUE, &mii);

    return hmnu;
}

typedef struct _UPDATA_DSKMENU_INFO
{
    MENUITEMINFO mii;
    HMENU menu;
    int id;
    int pos;
} UPDATA_DSKMENU_INFO;

static BOOL _cb_update_dskmenu (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    UPDATA_DSKMENU_INFO* info = (UPDATA_DSKMENU_INFO*) context;

    if (node->flags & ZOF_TF_MAINWIN) {
        if (node->flags & ZOF_VISIBLE)
            info->mii.state       = MFS_ENABLED;
        else
            info->mii.state       = MFS_DISABLED;
    }
    else
        return FALSE;

    info->mii.id              = info->id;
    info->mii.typedata        = (DWORD)(node->caption); 
    info->mii.itemdata        = (DWORD)node; 
    InsertMenuItem (info->menu, info->pos, TRUE, &info->mii);

    info->id++;
    info->pos++;
    return TRUE;
}

static void srvUpdateDesktopMenu (void)
{
    UPDATA_DSKMENU_INFO info;
    int nCount, count, iPos;
    MG_Layer* layer;

    info.menu = GetSubMenu (sg_srvDesktopMenu, 3);

    nCount = GetMenuItemCount (info.menu);
    for (iPos = nCount; iPos > 0; iPos --)
        DeleteMenu (info.menu, iPos - 1, MF_BYPOSITION);
    
    memset (&info.mii, 0, sizeof (MENUITEMINFO));
    info.mii.type = MFT_STRING;
    info.id = IDM_FIRSTWINDOW;
    info.pos = 0;
    
    count = do_for_all_znodes (&info, 
                    __mg_zorder_info, _cb_update_dskmenu, ZT_GLOBAL);

    if (count) {
        info.mii.type            = MFT_SEPARATOR;
        info.mii.state           = 0;
        info.mii.id              = 0;
        info.mii.typedata        = 0;
        InsertMenuItem (info.menu, info.pos, TRUE, &info.mii);

        info.pos ++;
    }

    info.mii.type = MFT_STRING;
    count = do_for_all_znodes (&info, 
                    __mg_zorder_info, _cb_update_dskmenu, ZT_TOPMOST);

    if (count) {
        info.mii.type            = MFT_SEPARATOR;
        info.mii.state           = 0;
        info.mii.id              = 0;
        info.mii.typedata        = 0;
        InsertMenuItem (info.menu, info.pos, TRUE, &info.mii);

        info.pos ++;
        info.mii.type = MFT_STRING;
    }

    info.mii.type = MFT_STRING;
    count = do_for_all_znodes (&info, 
                    __mg_zorder_info, _cb_update_dskmenu, ZT_NORMAL);

    info.menu = GetSubMenu (sg_srvDesktopMenu, 4);
    nCount = GetMenuItemCount (info.menu);
    for (iPos = nCount; iPos > 0; iPos --)
        DeleteMenu (info.menu, iPos - 1, MF_BYPOSITION);

    info.mii.type = MFT_STRING;
    info.id = IDM_SWITCH_LAYER;
    info.pos = 0;
    layer = mgLayers;
    while (layer) {
        info.mii.id              = info.id;
        info.mii.typedata        = (DWORD)layer->name;
        info.mii.itemdata        = (DWORD)layer;
        if (mgTopmostLayer == layer)
            info.mii.state       = MFS_DISABLED;
        else
            info.mii.state       = MFS_ENABLED;

        InsertMenuItem (info.menu, info.pos, TRUE, &info.mii);

        info.pos ++;
        info.id ++;

        layer = layer->next;
    }

    info.menu = GetSubMenu (sg_srvDesktopMenu, 5);
    nCount = GetMenuItemCount (info.menu);
    for (iPos = nCount; iPos > 0; iPos --)
        DeleteMenu (info.menu, iPos - 1, MF_BYPOSITION);

    info.mii.type = MFT_STRING;
    info.id = IDM_DELETE_LAYER;
    info.pos = 0;
    layer = mgLayers;
    while (layer) {
        info.mii.id              = info.id;
        info.mii.typedata        = (DWORD)layer->name;
        info.mii.itemdata        = (DWORD)layer;
        if (mgTopmostLayer == layer)
            info.mii.state       = MFS_DISABLED;
        else
            info.mii.state       = MFS_ENABLED;

        InsertMenuItem (info.menu, info.pos, TRUE, &info.mii);

        info.pos ++;
        info.id ++;

        layer = layer->next;
    }

    nCount = GetMenuItemCount (sg_srvDesktopMenu);
    for (iPos = nCount; iPos > 7; iPos --)
        DeleteMenu (sg_srvDesktopMenu, iPos - 1, MF_BYPOSITION);

    CustomizeDesktopMenu (sg_srvDesktopMenu, 7);
}

static BOOL _cb_close_mainwin (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    if (node->flags & ZOF_TF_MAINWIN) {
        post_msg_by_znode_p (zi, node, MSG_CLOSE, 0, 0);
    }
    return TRUE;
}

static int nr_of_all_znodes (void)
{
    MG_Layer* layer = mgLayers;
    int count = 0;
    ZORDERINFO* zi;

    while (layer) {
        zi = layer->zorder_info;
        count += zi->nr_topmosts;
        count += zi->nr_normals;

        layer = layer->next;
    }

    if (__mg_zorder_info)
        count += (__mg_zorder_info->nr_globals - 1);

    return count;
}

static int srvDesktopCommand (int id)
{
    if (id == IDM_REDRAWBG) {
        SendMessage (HWND_DESKTOP, MSG_ERASEDESKTOP, 0, 0);
    }
    else if (id == IDM_CLOSEALLWIN) {
        lock_zi_for_read (__mg_zorder_info);
        do_for_all_znodes (NULL, 
                    __mg_zorder_info, _cb_close_mainwin, ZT_ALL);
        unlock_zi_for_read (__mg_zorder_info);
    }
    else if (id == IDM_ENDSESSION) {
        if (nr_of_all_znodes () != 0
                    && MessageBox (HWND_DESKTOP,
                    "There are some windows not closed.\n"
                    "Do you want to quit the session any way?",
                    "Warning!", MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
                return 0;

        ExitGUISafely (-1);
    }
    else if (id >= IDM_FIRSTWINDOW && id < IDM_SWITCH_LAYER) {
        HMENU win_menu;
        MENUITEMINFO mii = {MIIM_DATA};
        ZORDERNODE* node;

        win_menu = GetSubMenu (sg_srvDesktopMenu, 3);
        if (GetMenuItemInfo (win_menu, id, MF_BYCOMMAND, &mii) == 0) {
            node = (ZORDERNODE*)mii.itemdata;
            if (node && (node->flags & ZOF_TF_MAINWIN) 
                            && !(node->flags & ZOF_DISABLED)) {
                ZORDERNODE * win_nodes = 
                        (ZORDERNODE*) ((char*)(__mg_zorder_info + 1) + 
                                       __mg_zorder_info->size_usage_bmp);
                win_nodes += DEF_NR_POPUPMENUS;

                srvMove2Top (node->cli, node - win_nodes);
                srvSetActiveWindow (node->cli, node - win_nodes);
            }
        }
    }
    else if (id >= IDM_SWITCH_LAYER && id < IDM_DELETE_LAYER) {
        HMENU win_menu;
        MENUITEMINFO mii = {MIIM_DATA};

        win_menu = GetSubMenu (sg_srvDesktopMenu, 4);
        if (GetMenuItemInfo (win_menu, id, MF_BYCOMMAND, &mii) == 0) {
            ServerSetTopmostLayer ((MG_Layer*) mii.itemdata);
        }
    }
    else if (id >= IDM_DELETE_LAYER) {
        HMENU win_menu;
        MENUITEMINFO mii = {MIIM_DATA};

        win_menu = GetSubMenu (sg_srvDesktopMenu, 5);
        if (GetMenuItemInfo (win_menu, id, MF_BYCOMMAND, &mii) == 0) {
            ServerDeleteLayer ((MG_Layer*) mii.itemdata);
        }
    }

    return 0;
}

static int srvSesseionMessageHandler (int message, WPARAM wParam, LPARAM lParam)
{
    static PBITMAP bg_bmp;
    static int pic_x, pic_y;
    static HDC hDesktopDC;
    RECT* pInvalidRect;

    switch (message) {
        case MSG_STARTSESSION:
            __mg_init_local_sys_text ();
            hDesktopDC = CreatePrivateDC (HWND_DESKTOP);

            sg_srvDesktopMenu = srvCreateDesktopMenu ();

            if (srvGetBgPicturePos () < 0)
                bg_bmp = NULL;
            else
                bg_bmp = srvLoadBgPicture ();

            if (bg_bmp)
                srvGetBgPictureXY (srvGetBgPicturePos (), 
                            bg_bmp->bmWidth, bg_bmp->bmHeight,
                            &pic_x, &pic_y);
            break;

        case MSG_REINITSESSION:
            if (wParam)
                __mg_init_local_sys_text ();

            DestroyMenu (sg_srvDesktopMenu);
            sg_srvDesktopMenu = srvCreateDesktopMenu ();
            SendMessage (HWND_DESKTOP, MSG_ERASEDESKTOP, 0, 0);
            break;

        case MSG_ENDSESSION:
            if (bg_bmp)
                UnloadBitmap (bg_bmp);

            if (hDesktopDC) {
                DeletePrivateDC (hDesktopDC);
                hDesktopDC = 0;
            }

            if (sg_srvDesktopMenu) {
                DestroyMenu (sg_srvDesktopMenu);
                sg_srvDesktopMenu = 0;
            }
            return -1;

        case MSG_ERASEDESKTOP:
            SetBrushColor (hDesktopDC, GetWindowElementColor (BKC_DESKTOP));
            pInvalidRect = (PRECT)lParam;
            if (pInvalidRect) {
                SelectClipRect (hDesktopDC, pInvalidRect);
                FillBox (hDesktopDC, pInvalidRect->left, pInvalidRect->top, 
                            RECTWP (pInvalidRect), 
                            RECTHP (pInvalidRect));
            }
            else {
                SelectClipRect (hDesktopDC, &g_rcScr);
                FillBox(hDesktopDC, g_rcScr.left, g_rcScr.top,
                       g_rcScr.right, g_rcScr.bottom);
            }

            if (bg_bmp) {
                FillBoxWithBitmap (hDesktopDC, pic_x, pic_y,
                                  bg_bmp->bmWidth, bg_bmp->bmHeight, bg_bmp);
            }
            break;

        case MSG_DT_KEYLONGPRESS:
        case MSG_DT_KEYALWAYSPRESS:
        case MSG_DT_KEYDOWN:
        case MSG_DT_CHAR:
        case MSG_DT_KEYUP:
        case MSG_DT_SYSKEYDOWN:
        case MSG_DT_SYSCHAR:
        case MSG_DT_SYSKEYUP:
        case MSG_DT_LBUTTONDOWN:
        case MSG_DT_LBUTTONUP:
        case MSG_DT_LBUTTONDBLCLK:
        case MSG_DT_MOUSEMOVE:
        case MSG_DT_RBUTTONDOWN:
        case MSG_DT_RBUTTONDBLCLK:
        break;
        
        case MSG_DT_RBUTTONUP:
        {
            int x, y;
            x = LOSWORD (lParam);
            y = HISWORD (lParam);

            srvUpdateDesktopMenu ();
            TrackPopupMenu (sg_srvDesktopMenu, TPM_DEFAULT, x, y, HWND_DESKTOP);
        }
        break;

    }

    return 0;
}

static int srvRegisterIMEWnd (HWND hwnd)
{
    if (!mgIsServer)
        return ERR_INV_HWND;

    if (__mg_ime_wnd != 0)
        return ERR_IME_TOOMUCHIMEWND;

    if (!CheckAndGetMainWindowPtr (hwnd))
        return ERR_INV_HWND;

    __mg_ime_wnd = hwnd;

    SendNotifyMessage (__mg_ime_wnd, MSG_IME_CLOSE, 0, 0);

    return ERR_OK;
}

static int srvUnregisterIMEWnd (HWND hwnd)
{
    if (__mg_ime_wnd != hwnd)
        return ERR_IME_NOSUCHIMEWND;

    __mg_ime_wnd = 0;

    return ERR_OK;
}

static int srvSetIMEStatus (int iIMEStatusCode, int Value)
{
    if (__mg_ime_wnd == 0)
        return ERR_IME_NOIMEWND;

    SendMessage (__mg_ime_wnd, 
        MSG_IME_SETSTATUS, (WPARAM)iIMEStatusCode, (LPARAM)Value);

    return ERR_OK;
}

static int srvGetIMEStatus (int iIMEStatusCode)
{
    if (__mg_ime_wnd == 0)
        return ERR_IME_NOIMEWND;

    return SendMessage (__mg_ime_wnd, MSG_IME_GETSTATUS, iIMEStatusCode, 0);
}

/***********************************************************
 * Session message handling for the clients.
 **********************************************************/
static BOOL _cb_count_znodes (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    if (node->cli == __mg_client_id) {
        return TRUE;
    }

    return FALSE;
}

static int znodes_of_this_client (void)
{
    int count = 0;

    lock_zi_for_read (__mg_zorder_info);
    count = do_for_all_znodes (NULL, __mg_zorder_info, 
                    _cb_count_znodes, ZT_ALL);
    unlock_zi_for_read (__mg_zorder_info);

    return count;
}

static int cliSesseionMessageHandler (int message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_STARTSESSION:
            __mg_init_local_sys_text ();
            break;

        case MSG_REINITSESSION:
            if (wParam)
                __mg_init_local_sys_text ();
            break;

        case MSG_ENDSESSION:
            if (znodes_of_this_client () == 0) {
                PostQuitMessage (HWND_DESKTOP);
                return 1;
            }
            break;

        default:
            break;
#if 0
        case MSG_ERASEDESKTOP:
        case MSG_DT_KEYDOWN:
        case MSG_DT_CHAR:
        case MSG_DT_KEYUP:
        case MSG_DT_SYSKEYDOWN:
        case MSG_DT_SYSCHAR:
        case MSG_DT_SYSKEYUP:
        case MSG_DT_LBUTTONDOWN:
        case MSG_DT_LBUTTONUP:
        case MSG_DT_LBUTTONDBLCLK:
        case MSG_DT_MOUSEMOVE:
        case MSG_DT_RBUTTONDOWN:
        case MSG_DT_RBUTTONDBLCLK:
        case MSG_DT_RBUTTONUP:
            break;
#endif
    }

    return 0;
}

/***********************************************************
 * Handler for MSG_PAINT for the server and all clients.
 **********************************************************/

typedef struct _REFRESH_INFO
{
    const RECT* invrc;
    BOOL is_empty_invrc;
} REFRESH_INFO;

static BOOL _cb_refresh_znode (void* context, 
                const ZORDERINFO* zi, ZORDERNODE* node)
{
    PMAINWIN pTemp;
    REFRESH_INFO* info = (REFRESH_INFO*) context;

    if (node->cli == __mg_client_id) {
        pTemp = (PMAINWIN)node->hwnd;

        if (pTemp->WinType != TYPE_CONTROL 
                        && pTemp->dwStyle & WS_VISIBLE) {
            if (info->is_empty_invrc) {
                SendAsyncMessage ((HWND)pTemp, MSG_NCPAINT, 0, 0);
                InvalidateRect ((HWND)pTemp, NULL, TRUE);
            }
            else {
                RECT rcTemp, rcInv;
                if (IntersectRect (&rcTemp, 
                            (RECT*)(&pTemp->left), info->invrc)) {
                    dskScreenToWindow (pTemp, &rcTemp, &rcInv);
                    SendAsyncMessage ((HWND)pTemp, 
                                    MSG_NCPAINT, 0, (LPARAM)(&rcInv));
                    dskScreenToClient (pTemp, &rcTemp, &rcInv);
                    InvalidateRect ((HWND)pTemp, &rcInv, TRUE);
                }
            }
        }

        return TRUE;
    }

    return FALSE;
}

static void dskRefreshAllWindow (const RECT* invrc)
{
    REFRESH_INFO info = {invrc, FALSE};

    if (invrc->top == invrc->bottom || invrc->left == invrc->right)
        info.is_empty_invrc = TRUE;

    if (mgIsServer)
        SendMessage (HWND_DESKTOP, MSG_ERASEDESKTOP, 0, 
                            (LPARAM)(info.is_empty_invrc?0:&invrc));

    lock_zi_for_read (__mg_zorder_info);
    do_for_all_znodes (&info, __mg_zorder_info, _cb_refresh_znode, ZT_ALL);
    unlock_zi_for_read (__mg_zorder_info);
}

/***********************************************************
 * Handler for MSG_TIMER for the server and all clients.
 **********************************************************/

static void dskOnTimer (void)
{
    static UINT uCounter = 0;
    static UINT blink_counter = 0;
    static UINT sg_old_counter = 0;

    if (sg_old_counter == 0)
        sg_old_counter = SHAREDRES_TIMER_COUNTER;

    DispatchTimerMessage (SHAREDRES_TIMER_COUNTER - sg_old_counter);
    sg_old_counter = SHAREDRES_TIMER_COUNTER;

    if (SHAREDRES_TIMER_COUNTER < (blink_counter + 10))
        return;

    uCounter += (SHAREDRES_TIMER_COUNTER - blink_counter) * 10;
    blink_counter = SHAREDRES_TIMER_COUNTER;

    if (sg_hCaretWnd != 0
            && (HWND)GetMainWindowPtrOfControl (sg_hCaretWnd)
                    == dskGetActiveWindow (NULL) 
            && uCounter >= sg_uCaretBTime) {
        PostMessage (sg_hCaretWnd, MSG_CARETBLINK, 0, 0);
        uCounter = 0;
    }
}

#ifdef _MISC_SAVESCREEN
static void srvSaveScreen (BOOL active)
{
    RECT rcActive;
    HWND hwndActive = 0;
    int cliActive = -1;

    static int n = 1;
    char buffer[20];

    if (active) {
        ZORDERNODE* nodes = (ZORDERNODE*) ((char*)__mg_zorder_info
                    + sizeof (ZORDERINFO) + __mg_zorder_info->size_usage_bmp + 
                    sizeof (ZORDERNODE) * DEF_NR_POPUPMENUS);

        if (__mg_zorder_info->active_win) {
            cliActive = nodes [__mg_zorder_info->active_win].cli;
            hwndActive = nodes [__mg_zorder_info->active_win].hwnd;
            rcActive = nodes [__mg_zorder_info->active_win].rc;
        }
    }

    if (cliActive == -1) {
        cliActive = 0;
        hwndActive = 0;
        rcActive = g_rcScr;
    }

    sprintf (buffer, "%d-%x-%d.bmp", cliActive, hwndActive, n);
    if (SaveScreenRectContent (&rcActive, buffer)) {
        Ping ();
        n ++;
    }
}
#endif /* _MISC_SAVESCREEN */

int DesktopWinProc (HWND hWnd, int message, WPARAM wParam, LPARAM lParam)
{
    int flags, x, y;

    if (message >= MSG_FIRSTWINDOWMSG && message <= MSG_LASTWINDOWMSG) {
        return dskWindowMessageHandler (message, (PMAINWIN)wParam, lParam);
    }
    else if (message >= MSG_FIRSTKEYMSG && message <= MSG_LASTKEYMSG) {
        if (mgIsServer) {
            if (wParam == SCANCODE_PRINTSCREEN && message == MSG_KEYDOWN) {
#ifdef _MISC_SAVESCREEN
            srvSaveScreen (lParam & KS_CTRL);
#endif
            }
            else if (wParam == SCANCODE_ESCAPE && lParam & KS_CTRL) {
                srvUpdateDesktopMenu ();
                TrackPopupMenu (sg_srvDesktopMenu, TPM_DEFAULT, 
                        0, g_rcScr.bottom, HWND_DESKTOP);
            }

            if (__mg_zorder_info->cli_trackmenu > 0) {

                MSG msg = {0, message, wParam, lParam, __mg_timer_counter};

                __mg_send2client (&msg, mgClients + 
                                __mg_zorder_info->cli_trackmenu);
            }
            else if (sg_ptmi)
                PopupMenuTrackProc (sg_ptmi, message, wParam, lParam);
            else
                srvKeyMessageHandler (message, (int)wParam, (DWORD)lParam);
        }
        else if (sg_ptmi)
            PopupMenuTrackProc (sg_ptmi, message, wParam, lParam);

        return 0;
    }
    else if (message >= MSG_FIRSTMOUSEMSG && message <= MSG_LASTMOUSEMSG) {
        flags = (int)wParam;

        x = LOSWORD (lParam);
        y = HISWORD (lParam);

        if (mgIsServer) {
            if (__mg_zorder_info->cli_trackmenu > 0 ) {

                MSG msg = {0, message, wParam, lParam, __mg_timer_counter};

                __mg_send2client (&msg, mgClients + 
                                __mg_zorder_info->cli_trackmenu);
            }
            else if (sg_ptmi) {
                if (PopupMenuTrackProc (sg_ptmi, message, x, y) == 0)
                    return 0;
                dskMouseMessageHandler (message, flags, x, y);
            }
            else
                dskMouseMessageHandler (message, flags, x, y);
        }
        else if (sg_ptmi)
            PopupMenuTrackProc (sg_ptmi, message, x, y);
        else
            dskMouseMessageHandler (message, flags, x, y);

        return 0;
    }
    else if (message == MSG_COMMAND) {
        if (wParam <= MAXID_RESERVED && wParam >= MINID_RESERVED)
            return srvDesktopCommand ((int)wParam);
        else
            return CustomDesktopCommand ((int)wParam);
    }
    else if (message >= MSG_FIRSTSESSIONMSG && message <= MSG_LASTSESSIONMSG) {
        if (mgIsServer)
            return srvSesseionMessageHandler (message, wParam, lParam);
        else
            return cliSesseionMessageHandler (message, wParam, lParam);
    }

    switch (message) {
        case MSG_TIMEOUT:
            {
                MSG msg = {0, MSG_IDLE, wParam, 0};
                dskBroadcastMessage (&msg);
                break;
            }

        case MSG_SRVNOTIFY:
            {
                MSG msg = {0, MSG_SRVNOTIFY, wParam, lParam};
                dskBroadcastMessage (&msg);
                break;
            }

        case MSG_PAINT:
            {
                RECT invrc = {LOSWORD(wParam), HISWORD(wParam),
                                LOSWORD(lParam), HISWORD(lParam)};
                dskRefreshAllWindow (&invrc);
                break;
            }

        case MSG_REGISTERWNDCLASS:
            return AddNewControlClass ((PWNDCLASS)lParam);

        case MSG_UNREGISTERWNDCLASS:
            return DeleteControlClass ((const char*)lParam);

        case MSG_NEWCTRLINSTANCE:
            return dskOnNewCtrlInstance ((PCONTROL)wParam, (PCONTROL)lParam);

        case MSG_REMOVECTRLINSTANCE:
            return dskOnRemoveCtrlInstance ((PCONTROL)wParam, (PCONTROL)lParam);

        case MSG_GETCTRLCLASSINFO:
            return (int)GetControlClassInfo ((const char*)lParam);

        case MSG_CTRLCLASSDATAOP:
            return (int)ControlClassDataOp (wParam, (WNDCLASS*)lParam);

        case MSG_IME_REGISTER:
            if (mgIsServer)
                return srvRegisterIMEWnd ((HWND)wParam);
            break;

        case MSG_IME_UNREGISTER:
            if (mgIsServer)
                return srvUnregisterIMEWnd ((HWND)wParam);
            break;

        case MSG_IME_SETSTATUS:
            if (mgIsServer)
                return srvSetIMEStatus ((int)wParam, (int)lParam);
            break;

        case MSG_IME_GETSTATUS:
            if (mgIsServer)
                return srvGetIMEStatus ((int)wParam);
            break;

        case MSG_BROADCASTMSG:
            return dskBroadcastMessage ((PMSG)lParam);

        case MSG_TIMER:
            dskOnTimer ();
            break;
    }

    return 0;
}

#ifdef _DEBUG
void GUIAPI DumpWindow (FILE* fp, HWND hWnd)
{
    PMAINWIN pWin = (PMAINWIN)hWnd;
    PCONTROL pCtrl;

    if (pWin->DataType != TYPE_HWND) {
        fprintf (fp, "DumpWindow: Invalid DataType!\n");
        return;
    }

    if (pWin->WinType == TYPE_MAINWIN) {
        fprintf (fp, "=============== Main Window %#x==================\n", 
                        hWnd);

        fprintf (fp, "Rect        -- (%d, %d, %d, %d)\n", 
                        pWin->left, pWin->top, pWin->right, pWin->bottom);
        fprintf (fp, "Client      -- (%d, %d, %d, %d)\n", 
                        pWin->cl, pWin->ct, pWin->cr, pWin->cb);

        fprintf (fp, "Style       -- %lx\n", pWin->dwStyle);
        fprintf (fp, "ExStyle     -- %lx\n", pWin->dwExStyle);

        fprintf (fp, "PrivCDC     -- %#x\n", pWin->privCDC);

        fprintf (fp, "AddData     -- %lx\n", pWin->dwAddData);
        fprintf (fp, "AddData2    -- %lx\n", pWin->dwAddData2);

        fprintf (fp, "WinProc     -- %p\n", pWin->MainWindowProc);

        fprintf (fp, "Caption     -- %s\n", pWin->spCaption);
        fprintf (fp, "ID          -- %d\n", pWin->id);

        fprintf (fp, "FirstChild  -- %#x\n", pWin->hFirstChild);
        pCtrl = (PCONTROL)pWin->hFirstChild;
        while (pCtrl) {
            fprintf (fp, "    Child   -- %p(%d), %s(%d)\n", pCtrl, pCtrl->id, 
                                pCtrl->pcci->name, pCtrl->pcci->nUseCount);
            pCtrl = pCtrl->next;
        }
        fprintf (fp, "ActiveChild -- %#x\n", pWin->hActiveChild);

        fprintf (fp, "Hosting     -- %p\n", pWin->pHosting);
        fprintf (fp, "FirstHosted -- %p\n", pWin->pFirstHosted);
        fprintf (fp, "NextHosted  -- %p\n", pWin->pNextHosted);
        fprintf (fp, "BkColor     -- %d\n",  pWin->iBkColor);
        fprintf (fp, "Menu        -- %#x\n", pWin->hMenu);
        fprintf (fp, "Accel       -- %#x\n", pWin->hAccel);
        fprintf (fp, "Cursor      -- %#x\n", pWin->hCursor);
        fprintf (fp, "Icon        -- %#x\n", pWin->hIcon);
        fprintf (fp, "SysMenu     -- %#x\n", pWin->hSysMenu);
        fprintf (fp, "MsgQueue    -- %p\n", pWin->pMessages);
    }
    else {
        fprintf (fp, "=============== Control %#x==================\n", hWnd);
        pCtrl = (PCONTROL)hWnd;

        fprintf (fp, "Rect        -- (%d, %d, %d, %d)\n", 
                        pCtrl->left, pCtrl->top, pCtrl->right, pCtrl->bottom);
        fprintf (fp, "Client      -- (%d, %d, %d, %d)\n", 
                        pCtrl->cl, pCtrl->ct, pCtrl->cr, pCtrl->cb);

        fprintf (fp, "Style       -- %lx\n", pCtrl->dwStyle);
        fprintf (fp, "ExStyle     -- %lx\n", pCtrl->dwExStyle);

        fprintf (fp, "PrivCDC     -- %#x\n", pCtrl->privCDC);

        fprintf (fp, "AddData     -- %lx\n", pCtrl->dwAddData);
        fprintf (fp, "AddData2    -- %lx\n", pCtrl->dwAddData2);

        fprintf (fp, "WinProc     -- %p\n", pCtrl->ControlProc);

        fprintf (fp, "Caption     -- %s\n", pCtrl->spCaption);
        fprintf (fp, "ID          -- %d\n", pCtrl->id);

        fprintf (fp, "FirstChild  -- %p\n", pCtrl->children);
        fprintf (fp, "ActiveChild -- %p\n", pCtrl->active);
        fprintf (fp, "Parent      -- %p\n", pCtrl->pParent);
        fprintf (fp, "Next        -- %p\n", pCtrl->next);

        pCtrl = (PCONTROL)pCtrl->children;
        while (pCtrl) {
            fprintf (fp, "    Child   -- %p(%d), %s(%d)\n", pCtrl, pCtrl->id, 
                                pCtrl->pcci->name, pCtrl->pcci->nUseCount);
            pCtrl = pCtrl->next;
        }
    }

    fprintf (fp, "=================== End ==================\n");
    return; 
}
#endif /* _DEBUG */

#endif /* _LITE_VERSION && !_STAND_ALONE */

