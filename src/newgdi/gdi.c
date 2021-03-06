/*
** $Id: gdi.c 11449 2009-04-01 03:50:50Z weiym $
**
** The graphics display interface module of MiniGUI.
**
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 1999 ~ 2002 Wei Yongming.
**
** Current maintainer: Wei Yongming.
**
** Create date: 1999.01.03
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "minigui.h"
#include "gdi.h"
#include "window.h"
#include "cliprect.h"
#include "gal.h"
#include "cursor.h"
#include "internals.h"
#include "inline.h"
#include "memops.h"
#include "ctrlclass.h"
#include "dc.h"
#include "sysfont.h"
#include "devfont.h"
#include "drawtext.h"
/************************* global data define ********************************/
DC __mg_screen_dc;

gal_pixel SysPixelIndex [17];

/* This should be the standard EGA palette. */
const RGB SysPixelColor [] = {
    {0x00, 0x00, 0x00},     /* transparent   --0 */
    {0x00, 0x00, 0x80},     /* dark blue     --1 */
    {0x00, 0x80, 0x00},     /* dark green    --2 */
    {0x00, 0x80, 0x80},     /* dark cyan     --3 */
    {0x80, 0x00, 0x00},     /* dark red      --4 */
    {0x80, 0x00, 0x80},     /* dark magenta  --5 */
    {0x80, 0x80, 0x00},     /* dark yellow   --6 */
    {0x80, 0x80, 0x80},     /* dark gray     --7 */
    {0xC0, 0xC0, 0xC0},     /* light gray    --8 */
    {0x00, 0x00, 0xFF},     /* blue          --9 */
    {0x00, 0xFF, 0x00},     /* green         --10 */
    {0x00, 0xFF, 0xFF},     /* cyan          --11 */
    {0xFF, 0x00, 0x00},     /* red           --12 */
    {0xFF, 0x00, 0xFF},     /* magenta       --13 */
    {0xFF, 0xFF, 0x00},     /* yellow        --14 */
    {0xFF, 0xFF, 0xFF},     /* light white   --15 */
    {0x00, 0x00, 0x00}      /* black         --16 */
};

#ifndef _LITE_VERSION
/* mutex ensuring exclusive access to gdi.  */
pthread_mutex_t __mg_gdilock;
#endif

/************************* global functions declaration **********************/
/* the following functions, which defined in other module */
/* but used in this module. */
extern PGCRINFO GetGCRgnInfo (HWND hWnd);

/**************************** static data ************************************/
/* General DC */
static DC DCSlot [DCSLOTNUMBER];

#ifndef _LITE_VERSION
/* mutex ensuring exclusive access to DC slot.  */
static pthread_mutex_t dcslot;
#endif

BLOCKHEAP __mg_FreeClipRectList;

/************************* static functions declaration **********************/
static void dc_InitClipRgnInfo (void);
static void dc_InitDC (PDC pdc, HWND hWnd, BOOL bIsClient);
static void dc_InitMemDCFrom (PDC pdc, const PDC pdc_ref);
static void dc_InitScreenDC (PDC pdc, GAL_Surface* surface);

/************************** inline functions *********************************/
inline void WndRect(HWND hWnd, PRECT prc)
{
    PCONTROL pParent;
    PCONTROL pCtrl;

    pParent = pCtrl = (PCONTROL) hWnd;

    if (hWnd == HWND_DESKTOP) {
        *prc = g_rcScr;
        return;
    }

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

inline void WndClientRect(HWND hWnd, PRECT prc)
{
    PCONTROL pCtrl;
    PCONTROL pParent;
    pParent = pCtrl = (PCONTROL) hWnd;

    if (hWnd == HWND_DESKTOP) {
        *prc = g_rcScr;
        return;
    }

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

static void RestrictControlECRGN (RECT* minimal, PCONTROL pCtrl)
{
    RECT rc;
    PCONTROL pRoot = (PCONTROL) (pCtrl->pMainWin);
    int off_x = 0, off_y = 0;

    do {
        PCONTROL pParent = pCtrl;

        rc.left = pRoot->cl + off_x;
        rc.top  = pRoot->ct + off_y;
        rc.right = pRoot->cr + off_x;
        rc.bottom = pRoot->cb + off_y;

        if (!IntersectRect (minimal, minimal, &rc)) {
            SetRect (minimal, 0, 0, 0, 0);
            return;
        }

        if (pRoot == pCtrl->pParent)
            break;

        off_x += pRoot->cl;
        off_y += pRoot->ct;

        while (TRUE) {
            if (pRoot->children == pParent->pParent->children) {
                pRoot = pParent;
                break;
            }
            pParent = pParent->pParent;
        }
    } while (TRUE);
}

/******************* Initialization and termination of GDI *******************/
BOOL InitScreenDC (void* surface)
{
    InitFreeClipRectList (&__mg_FreeClipRectList, SIZE_CLIPRECTHEAP);
    
    INIT_LOCK (&__mg_gdilock, NULL);
    INIT_LOCK (&dcslot, NULL);

    dc_InitClipRgnInfo ();
    dc_InitScreenDC (&__mg_screen_dc, (GAL_Surface *)surface);
    return TRUE;
}

void TerminateScreenDC (void)
{
    PDC pdc;
    SelectClipRect (HDC_SCREEN, &g_rcScr);
    SetBrushColor (HDC_SCREEN, 0);
#if defined (_LITE_VERSION) && !defined (_STAND_ALONE)
    if (mgIsServer)
#endif
        FillBox (HDC_SCREEN, g_rcScr.left, g_rcScr.top,
                       g_rcScr.right, g_rcScr.bottom);

    DestroyFreeClipRectList (&__mg_FreeClipRectList);

    pdc = dc_HDC2PDC (HDC_SCREEN);

    if (pdc->alpha_pixel_format)
        free (pdc->alpha_pixel_format);

    DESTROY_LOCK (&__mg_gdilock);
    DESTROY_LOCK (&dcslot);
}

BOOL InitGDI (void)
{   
    if (!InitTextBitmapBuffer ()) {
        fprintf (stderr, 
            "GDI: Can not initialize text bitmap buffer!\n");
        goto error;
    }

#ifdef _INCORE_RES
#ifdef _RBF_SUPPORT
    if (!InitIncoreRBFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize incore RBF fonts!\n");
        goto error;
    }
#endif

#ifdef _QPF_SUPPORT
    if (!InitIncoreQPFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize incore QPF fonts!\n");
        goto error;
    }
#endif
#endif /* _INCORE_RES */

#ifdef _VBF_SUPPORT
    if (!InitIncoreVBFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize incore VBF fonts!\n");
        goto error;
    }
#endif

#ifndef _INCORE_RES

#ifdef _RBF_SUPPORT
    if (!InitRawBitmapFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize raw bitmap fonts!\n");
        goto error;
    }
#endif

#ifdef _VBF_SUPPORT
    if (!InitVarBitmapFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize var bitmap fonts!\n");
        goto error;
    }
#endif

#ifdef _QPF_SUPPORT
    if (!InitQPFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize QPF fonts!\n");
        goto error;
    }
#endif

#ifndef _LITE_VERSION

#ifdef _TYPE1_SUPPORT
    if (!InitType1Fonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize Type1 fonts!\n");
        goto error;
    }
#endif

#endif /* !_LITE_VERSION */

#endif /* !_INCORE_RES */

#if (defined (_TTF_SUPPORT) || defined (_FT2_SUPPORT)) && !defined(_LITE_VERSION)
    if (!InitFreeTypeFonts ()) {
        fprintf (stderr, 
            "GDI: Can not initialize TrueType fonts!\n");
        goto error;
    }
#endif

    /* TODO: add other font support here */
#if defined (_SCRIPT_EASY_SUPPORT) && !defined(_LITE_VERSION)
    if(!initialize_scripteasy())
    {
        fprintf (stderr, 
            "GDI: Can not initialize ScriptEasy fonts!\n");
        goto error;
    }
#endif


#ifdef _DEBUG
    dumpDevFonts ();
#endif

    if (!InitSysFont ()) {
        fprintf (stderr, 
            "GDI: Can not create system fonts!\n");
        goto error;
    }

    return TRUE;

error:
    return FALSE;
}

void TerminateGDI( void )
{
    TermSysFont ();

    /* TODO: add other font support here */

#ifndef _INCORE_RES

#ifndef _LITE_VERSION

#if defined (_TTF_SUPPORT) || defined (_FT2_SUPPORT)
    TermFreeTypeFonts ();
#endif

#ifdef _TYPE1_SUPPORT
    TermType1Fonts();
#endif

#if defined (_SCRIPT_EASY_SUPPORT)
    uninitialize_scripteasy();
#endif

#endif /* _LITE_VERSION */

#ifdef _QPF_SUPPORT
    TermQPFonts ();
#endif

#ifdef _VBF_SUPPORT
    TermVarBitmapFonts ();
#endif

#ifdef _RBF_SUPPORT
    TermRawBitmapFonts ();
#endif

#endif /* _INCORE_RES */

#ifdef _VBF_SUPPORT
    TermIncoreVBFonts ();
#endif

#ifdef _INCORE_RES
#ifdef _QPF_SUPPORT
    TermIncoreQPFonts ();
#endif

#ifdef _RBF_SUPPORT
    TermIncoreRBFonts ();
#endif
#endif

    ResetDevFont ();

    TermTextBitmapBuffer ();
}

/*
 * Function: int GUIAPI GetGDCapability( int iItem) 
 *      This Function return DC parameters.
 * Parameters:
 *      The element want to retrive.
 * Return:
 *      The parameter.
 */
unsigned int GUIAPI GetGDCapability (HDC hdc, int iItem)
{
    PDC pdc;
    unsigned int iret = 0xFFFFFFFF;

    pdc = dc_HDC2PDC (hdc);

    LOCK (&__mg_gdilock);

    switch (iItem)
    {
        case GDCAP_DEPTH:
            iret = GAL_BitsPerPixel (pdc->surface);
/*
 * FIXME
 * for 1555 pixel format, color depth should be 15
 */
            if (iret == 16 && GAL_RMask(pdc->surface) == 0x7c00 && 
                              GAL_GMask(pdc->surface) == 0x3e00)
                iret = 15;
            break;

        case GDCAP_BITSPP:
            iret = GAL_BitsPerPixel (pdc->surface);
            break;

        case GDCAP_BPP:
            iret = GAL_BytesPerPixel (pdc->surface);
            break;

        case GDCAP_RMASK:
            iret = GAL_RMask (pdc->surface);
            break;
        case GDCAP_GMASK:
            iret = GAL_GMask (pdc->surface);
            break;
        case GDCAP_BMASK:
            iret = GAL_BMask (pdc->surface);
            break;

        case GDCAP_COLORNUM:
            iret = GAL_BitsPerPixel (pdc->surface);
            if (iret < 32)
                iret = 1 << iret;
            else
                iret = 0xFFFFFFFF;
            break;
 
        case GDCAP_HPIXEL:
            iret = GAL_Width (pdc->surface);
            break;

        case GDCAP_VPIXEL:
            iret = GAL_Height (pdc->surface);
            break;

        case GDCAP_MAXX:
            iret = RECTW (pdc->DevRC) - 1;
            break;

        case GDCAP_MAXY:
            iret = RECTH (pdc->DevRC) - 1;
            break;
    }

    UNLOCK(&__mg_gdilock);
    return iret;
}

/* This function init clip region in all DC slots. */
static void dc_InitClipRgnInfo(void)
{
    int i;

    for (i=0; i<DCSLOTNUMBER; i++) {
        /* Local clip region */
        InitClipRgn (&DCSlot[i].lcrgn, &__mg_FreeClipRectList);

        /* Global clip region info */
        DCSlot[i].pGCRInfo = NULL;
        DCSlot[i].oldage = 0;

        /* Effective clip region */
        InitClipRgn (&DCSlot[i].ecrgn, &__mg_FreeClipRectList);
    }
   
}

/*
 * This function generates effective clip region from
 * local clip region and global clip region.
 * if the global clip region has a new age,
 * this function empty effective clip region first,
 * and then intersect local clip region and global clip region.
 */

BOOL dc_GenerateECRgn(PDC pdc, BOOL fForce)
{
    PCLIPRECT pcr;
    PCONTROL pCtrl;
    RECT minimal;

    /* is global clip region is empty? */
    if ((!fForce) && (!dc_IsVisible (pdc)))
        return FALSE;

    /* need regenerate? */
    if (fForce || (pdc->oldage != pdc->pGCRInfo->age)) {

        /*
         * update pdc->DevRC, and restrict the effective 
         */
        if (pdc->bIsClient)
            WndClientRect (pdc->hwnd, &pdc->DevRC);
        else
            WndRect (pdc->hwnd, &pdc->DevRC);

        /* copy local clipping region to effective clipping region. */
        ClipRgnCopy (&pdc->ecrgn, &pdc->lcrgn);

        /* transfer device coordinates to screen coordinates. */
        pcr = pdc->ecrgn.head;
        while (pcr) {
            coor_DP2SP (pdc, &pcr->rc.left, &pcr->rc.top);
            coor_DP2SP (pdc, &pcr->rc.right, &pcr->rc.bottom);
            
            pcr = pcr->next;
        }

        /* intersect with global clipping region. */
        if (pdc->lcrgn.head == NULL)
            ClipRgnCopy (&pdc->ecrgn, &pdc->pGCRInfo->crgn);
        else {
            coor_DP2SP (pdc, &pdc->ecrgn.rcBound.left, 
                            &pdc->ecrgn.rcBound.top);
            coor_DP2SP (pdc, &pdc->ecrgn.rcBound.right, 
                            &pdc->ecrgn.rcBound.bottom);
            ClipRgnIntersect (&pdc->ecrgn, &pdc->ecrgn, &pdc->pGCRInfo->crgn);
#ifdef _REGION_DEBUG
            dumpRegion (&pdc->pGCRInfo->crgn);
            dumpRegion (&pdc->ecrgn);
#endif
        }

        /* 
         * clipping region more with pdc->DevRC.
         */
        minimal = pdc->DevRC;
#if 0
        if (pdc->oldage == 0) {
            printf ("device rect: %d, %d, %d, %d\n",
                            minimal.left,
                            minimal.top,
                            minimal.right,
                            minimal.bottom);
            printf ("ecrgn bounding rect: %d, %d, %d, %d\n",
                            pdc->ecrgn.rcBound.left,
                            pdc->ecrgn.rcBound.top,
                            pdc->ecrgn.rcBound.right,
                            pdc->ecrgn.rcBound.bottom);
            printf ("grgn bounding rect: %d, %d, %d, %d\n",
                            pdc->pGCRInfo->crgn.rcBound.left,
                            pdc->pGCRInfo->crgn.rcBound.top,
                            pdc->pGCRInfo->crgn.rcBound.right,
                            pdc->pGCRInfo->crgn.rcBound.bottom);
        }
#endif

        /* restrict control's effective region. */
        pCtrl = Control (pdc->hwnd);
        if (pCtrl && !(pCtrl->dwExStyle & WS_EX_CTRLASMAINWIN))
            RestrictControlECRGN (&minimal, pCtrl);

        IntersectClipRect (&pdc->ecrgn, &minimal);

        pdc->oldage = pdc->pGCRInfo->age;
    }

    return TRUE;
}

PDC __mg_check_ecrgn (HDC hdc)
{
    PDC pdc = dc_HDC2PDC(hdc);

    if (dc_IsGeneralDC (pdc)) {
        LOCK_GCRINFO (pdc);
        if (!dc_GenerateECRgn (pdc, FALSE)) {
            UNLOCK_GCRINFO (pdc);
            return NULL;
        }
    }

    return pdc;
}

int __mg_enter_drawing (PDC pdc)
{
    BLOCK_DRAW_SEM (pdc);

#ifdef _LITE_VERSION
    if (CHECK_DRAWING (pdc))
        goto fail;
    if (CHECK_CLI_SCREEN (pdc, pdc->rc_output))
        goto fail;
#endif

    if (!IntersectRect (&pdc->rc_output, &pdc->rc_output, &pdc->ecrgn.rcBound))
        goto fail;

    LOCK (&__mg_gdilock);
    if (!dc_IsMemDC (pdc))
        ShowCursorForGDI (FALSE, pdc);

    return 0;

fail:
    UNBLOCK_DRAW_SEM (pdc);
    return -1;
}

void __mg_enter_drawing_nocheck (PDC pdc)
{
    BLOCK_DRAW_SEM (pdc);

    LOCK (&__mg_gdilock);
    if (!dc_IsMemDC (pdc))
        ShowCursorForGDI (FALSE, pdc);
}

void __mg_leave_drawing(PDC pdc)
{
    if (!dc_IsMemDC (pdc))
        ShowCursorForGDI (TRUE, pdc);
    UNLOCK (&__mg_gdilock);

    UNBLOCK_DRAW_SEM (pdc);
}

static void _dc_set_pixel_1 (PDC pdc)
{
    *pdc->cur_dst = (Uint8) pdc->cur_pixel;
}

static void _dc_set_pixel_2 (PDC pdc)
{
    *(Uint16 *) pdc->cur_dst = (Uint16) pdc->cur_pixel;
}

static void _dc_set_pixel_3 (PDC pdc)
{
#if 0
    *(Uint16*) pdc->cur_dst = (Uint16) pdc->cur_pixel;
    *(pdc->cur_dst + 2) = (Uint8) (pdc->cur_pixel >> 16);
#else
    SETVAL_24BIT (pdc->cur_dst, pdc->cur_pixel);
#endif
}

static void _dc_set_pixel_4 (PDC pdc)
{
    *(Uint32 *) pdc->cur_dst = (Uint32) pdc->cur_pixel;
}

static void _dc_and_pixel_1 (PDC pdc)
{
    *pdc->cur_dst &= (Uint8) pdc->cur_pixel;
}

static void _dc_and_pixel_2 (PDC pdc)
{
    *(Uint16 *) pdc->cur_dst &= (Uint16) pdc->cur_pixel;
}

static void _dc_and_pixel_3 (PDC pdc)
{
#if 0
    *(Uint16*) pdc->cur_dst &= (Uint16) pdc->cur_pixel;
    *(pdc->cur_dst + 2) &= (Uint8) (pdc->cur_pixel >> 16);
#else
    ANDVAL_24BIT (pdc->cur_dst, pdc->cur_pixel);
#endif
}

static void _dc_and_pixel_4 (PDC pdc)
{
    *(Uint32 *) pdc->cur_dst &= (Uint32) pdc->cur_pixel;
}

static void _dc_or_pixel_1 (PDC pdc)
{
    *pdc->cur_dst |= (Uint8) pdc->cur_pixel;
}

static void _dc_or_pixel_2 (PDC pdc)
{
    *(Uint16 *) pdc->cur_dst |= (Uint16) pdc->cur_pixel;
}

static void _dc_or_pixel_3 (PDC pdc)
{
#if 0
    *(Uint16*) pdc->cur_dst |= (Uint16) pdc->cur_pixel;
    *(pdc->cur_dst + 2) |= (Uint8) (pdc->cur_pixel >> 16);
#else
    ORVAL_24BIT (pdc->cur_dst, pdc->cur_pixel);
#endif
}

static void _dc_or_pixel_4 (PDC pdc)
{
    *(Uint32 *) pdc->cur_dst |= (Uint32) pdc->cur_pixel;
}

static void _dc_xor_pixel_1 (PDC pdc)
{
    *pdc->cur_dst ^= (Uint8) pdc->cur_pixel;
}

static void _dc_xor_pixel_2 (PDC pdc)
{
    *(Uint16 *) pdc->cur_dst ^= (Uint16) pdc->cur_pixel;
}

static void _dc_xor_pixel_3 (PDC pdc)
{
#if 0
    *(Uint16*) pdc->cur_dst ^= (Uint16) pdc->cur_pixel;
    *(pdc->cur_dst + 2) ^= (Uint8) (pdc->cur_pixel >> 16);
#else
    XORVAL_24BIT (pdc->cur_dst, pdc->cur_pixel);
#endif
}

static void _dc_xor_pixel_4 (PDC pdc)
{
    *(Uint32 *) pdc->cur_dst ^= (Uint32) pdc->cur_pixel;
}

static void _dc_move_to_1 (PDC pdc, int x, int y)
{
    pdc->cur_dst = (BYTE*)pdc->surface->pixels + pdc->surface->pitch * y;
    pdc->cur_dst += x;
}

static void _dc_move_to_2 (PDC pdc, int x, int y)
{
    pdc->cur_dst = (BYTE*)pdc->surface->pixels + pdc->surface->pitch * y;
    pdc->cur_dst += x << 1;
}

static void _dc_move_to_3 (PDC pdc, int x, int y)
{
    pdc->cur_dst = (BYTE*)pdc->surface->pixels + pdc->surface->pitch * y;
    pdc->cur_dst += (x << 1) + x;
}

static void _dc_move_to_4 (PDC pdc, int x, int y)
{
    pdc->cur_dst = (BYTE*)pdc->surface->pixels + pdc->surface->pitch * y;
    pdc->cur_dst += x << 2;
}

static void _dc_step_x_1 (PDC pdc, int step)
{
    pdc->cur_dst += step;
}

static void _dc_step_x_2 (PDC pdc, int step)
{
    pdc->cur_dst += step << 1;
}

static void _dc_step_x_3 (PDC pdc, int step)
{
    pdc->cur_dst += (step << 1) + step;
}

static void _dc_step_x_4 (PDC pdc, int step)
{
    pdc->cur_dst += step << 2;
}

/* 
 * a fast draw line operation if (cur_pixel == 0 && pdc->step == 1 && pdc->rop == ROP_SET) 
 * or (BytesPerPixel == 1)
 */ 
static void _dc_draw_hline_set_0 (PDC pdc, int w)
{
    Uint8* row = pdc->cur_dst;
    int n = w * pdc->surface->format->BytesPerPixel;

    if (!pdc->cur_pixel && !((long)row&3) && !(n&3)) {
        n = n >> 2;
        GAL_memset4 (row, 0, n);
    }
    else
        memset (row, pdc->cur_pixel, n);
}

static void _dc_draw_hline_set_1 (PDC pdc, int w)
{
    if (pdc->step == 1) {
        _dc_draw_hline_set_0 (pdc, w);
    }
    else {
        Uint8* row = pdc->cur_dst;

        while (w > 0) {
            *row = (Uint8) pdc->cur_pixel;
            row += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_draw_hline_set_2 (PDC pdc, int w)
{
    if (pdc->step == 1 && pdc->cur_pixel == 0) {
        _dc_draw_hline_set_0 (pdc, w);
    }
#ifdef ASM_memset2
    else if (pdc->step == 1 && !((long)pdc->cur_dst & 2)) {
        ASM_memset2 (pdc->cur_dst, pdc->cur_pixel, w);
    }
#endif
    else {
        Uint16* row = (Uint16*)pdc->cur_dst;

        while (w > 0) {
            *row = (Uint16) pdc->cur_pixel;
            row += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_draw_hline_set_3 (PDC pdc, int w)
{
    if (pdc->step == 1 && pdc->cur_pixel == 0) {
        _dc_draw_hline_set_0 (pdc, w);
    }
#ifdef ASM_memset3
    else if (pdc->step == 1) {
        ASM_memset3 (pdc->cur_dst, pdc->cur_pixel, w);
    }
#endif
    else {
        Uint8* row = (Uint8*)pdc->cur_dst;
        int step = (pdc->step << 1) + pdc->step;

        while (w > 0) {
#if 0
#if MGUI_BYTEORDER == MGUI_LIL_ENDIAN
            *(Uint16*) row = (Uint16) pdc->cur_pixel;
            *(row + 2) = (Uint8) (pdc->cur_pixel >> 16);
#else
            *(Uint16*) row = ((Uint16) pdc->cur_pixel << 8);
            *(row + 2) = (Uint8) (pdc->cur_pixel);
#endif
#else
            SETVAL_24BIT (row, pdc->cur_pixel);
#endif
            row += step;
            w -= pdc->step;
        }
    }
}

static void _dc_draw_hline_set_4 (PDC pdc, int w)
{
    Uint32* row = (Uint32*)pdc->cur_dst;

    if (pdc->step == 1) {
         GAL_memset4 (row, pdc->cur_pixel, w);
    }
    else {
        while (w > 0) {
            *row = pdc->cur_pixel;
            row += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_draw_hline_and_1 (PDC pdc, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memandset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 3)) {
        Uint16 _w = MAKEWORD (pdc->cur_pixel, pdc->cur_pixel);
        Uint32 _u = MAKELONG (_w, _w);
        ASM_memandset4 (row, _u, w >> 2);
        return;
    }
#endif

    while (w > 0) {
        *row &= (Uint8) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_and_2 (PDC pdc, int w)
{
    Uint16* row = (Uint16*)pdc->cur_dst;

#ifdef ASM_memandset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 1)) {
        Uint32 u = MAKELONG (pdc->cur_pixel, pdc->cur_pixel);
        ASM_memandset4 (row, u, w >> 1);
        return;
    }
#endif

    while (w > 0) {
        *row &= (Uint16) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_and_3 (PDC pdc, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memandset3
    if (pdc->step == 1) {
        ASM_memandset3 (row, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
#if 0
#if MGUI_BYTEORDER == MGUI_LIL_ENDIAN
        *(Uint16*) row &= (Uint16) pdc->cur_pixel;
        *(row + 2) &= (Uint8) (pdc->cur_pixel >> 16);
#else
        *(Uint16*) row &= ((Uint16) pdc->cur_pixel << 8);
        *(row + 2) &= (Uint8) (pdc->cur_pixel);
#endif
#else
        ANDVAL_24BIT (row, pdc->cur_pixel);
#endif
        row += step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_and_4 (PDC pdc, int w)
{
    Uint32* row = (Uint32*)pdc->cur_dst;

#ifdef ASM_memandset4
    if (pdc->step == 1) {
        ASM_memandset4 (row, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
        *row &= pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_or_1 (PDC pdc, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memorset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 3)) {
        Uint16 _w = MAKEWORD (pdc->cur_pixel, pdc->cur_pixel);
        Uint32 _u = MAKELONG (_w, _w);
        ASM_memorset4 (row, _u, w >> 2);
        return;
    }
#endif

    while (w > 0) {
        *row |= (Uint8) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_or_2 (PDC pdc, int w)
{
    Uint16* row = (Uint16*)pdc->cur_dst;

#ifdef ASM_memorset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 1)) {
        Uint32 u = MAKELONG (pdc->cur_pixel, pdc->cur_pixel);
        ASM_memorset4 (row, u, w>>1);
        return;
    }
#endif

    while (w > 0) {
        *row |= (Uint16) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_or_3 (PDC pdc, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memorset3
    if (pdc->step == 1) {
        ASM_memorset3 (row, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
#if 0
#if MGUI_BYTEORDER == MGUI_LIL_ENDIAN
        *(Uint16*) row |= (Uint16) pdc->cur_pixel;
        *(row + 2) |= (Uint8) (pdc->cur_pixel >> 16);
#else
        *(Uint16*) row |= ((Uint16) pdc->cur_pixel << 8);
        *(row + 2) |= (Uint8) (pdc->cur_pixel);
#endif
#else
        ORVAL_24BIT (row, pdc->cur_pixel);
#endif
        row += step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_or_4 (PDC pdc, int w)
{
    Uint32* row = (Uint32*)pdc->cur_dst;

#ifdef ASM_memorset4
    if (pdc->step == 1) {
        ASM_memorset4 (row, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
        *row |= pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_xor_1 (PDC pdc, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memxorset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 3)) {
        Uint16 _w = MAKEWORD (pdc->cur_pixel, pdc->cur_pixel);
        Uint32 _u = MAKELONG (_w, _w);
        ASM_memxorset4 (pdc->cur_dst, _u, w >> 2);
        return;
    }
#endif
    
    while (w > 0) {
        *row ^= (Uint8) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_xor_2 (PDC pdc, int w)
{
    Uint16* row = (Uint16*)pdc->cur_dst;

#ifdef ASM_memxorset4
    if (pdc->step == 1 && !((Uint32)pdc->cur_dst & 3) && !(w & 1)) {
        Uint32 u = MAKELONG (pdc->cur_pixel, pdc->cur_pixel);
        ASM_memxorset4 (row, u, w>>1);
        return;
    }
#endif

    while (w > 0) {
        *row ^= (Uint16) pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_xor_3 (PDC pdc, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memxorset3
    if (pdc->step == 1) {
        ASM_memxorset3 (pdc->cur_dst, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
#if 0
#if MGUI_BYTEORDER == MGUI_LIL_ENDIAN
        *(Uint16*) row ^= (Uint16) pdc->cur_pixel;
        *(row + 2) ^= (Uint8) (pdc->cur_pixel >> 16);
#else
        *(Uint16*) row ^= ((Uint16) pdc->cur_pixel << 8);
        *(row + 2) ^= (Uint8) (pdc->cur_pixel);
#endif
#else
        XORVAL_24BIT (row, pdc->cur_pixel);
#endif
        row += step;
        w -= pdc->step;
    }
}

static void _dc_draw_hline_xor_4 (PDC pdc, int w)
{
    Uint32* row = (Uint32*)pdc->cur_dst;

#ifdef ASM_memxorset4
    if (pdc->step == 1) {
        ASM_memxorset4 (pdc->cur_dst, pdc->cur_pixel, w);
        return;
    }
#endif

    while (w > 0) {
        *row ^= pdc->cur_pixel;
        row += pdc->step;
        w -= pdc->step;
    }
}

/* 
 * a fast put line operation if (pdc->step == 1 && pdc->rop == ROP_SET) 
 * or (BytesPerPixel == 1)
 */ 
static void _dc_put_hline_set_0 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = pdc->cur_dst;
    int n = w * pdc->surface->format->BytesPerPixel;

    if (!((long)row&3) && !(n&3) && !((long)src&3)) {
        GAL_memcpy4 (row, src, n >> 2);
    }
    else
        ASM_memcpy (row, src, n);
}

static void _dc_put_hline_set_1 (PDC pdc, Uint8* src, int w)
{
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        _dc_put_hline_set_0 (pdc, src, w);
    }
    else {
        Uint8* row = pdc->cur_dst;

        if (pdc->bkmode == BM_TRANSPARENT) {
            while (w > 0) {
                if (*src != pdc->skip_pixel)
                    *row = *src;
                row += pdc->step;
                src += pdc->step;
                w -= pdc->step;
            }
        }
        else while (w > 0) {
            *row = *src;
            row += pdc->step;
            src += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_put_hline_set_2 (PDC pdc, Uint8* src, int w)
{
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        _dc_put_hline_set_0 (pdc, src, w);
    }
    else {
        Uint16* dstrow = (Uint16*)pdc->cur_dst;
        Uint16* srcrow = (Uint16*)src;

        if (pdc->bkmode == BM_TRANSPARENT) {
            while (w > 0) {
                if (*srcrow != pdc->skip_pixel)
                    *dstrow = *srcrow;
                dstrow += pdc->step;
                srcrow += pdc->step;
                w -= pdc->step;
            }
        }
        else while (w > 0) {
            *dstrow = *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_put_hline_set_3 (PDC pdc, Uint8* src, int w)
{
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        _dc_put_hline_set_0 (pdc, src, w);
    }
    else {
        Uint8* row = (Uint8*)pdc->cur_dst;
        int step = (pdc->step << 1) + pdc->step;

        if (pdc->bkmode == BM_TRANSPARENT) {
            while (w > 0) {
#if 0
                if (((* (Uint32*)row) & 0x00FFFFFF) != pdc->skip_pixel) {
#else
                if (!EQUAL_24BIT (row, pdc->skip_pixel)) {
#endif
                    *row = *src;
                    *(row + 1) = *(src + 1);
                    *(row + 2) = *(src + 2);
                }
                row += step;
                src += step;
                w -= pdc->step;
            }
        }
        else while (w > 0) {
            *row = *src;
            *(row + 1) = *(src + 1);
            *(row + 2) = *(src + 2);
            row += step;
            src += step;
            w -= pdc->step;
        }
    }
}

static void _dc_put_hline_set_4 (PDC pdc, Uint8* src, int w)
{
    Uint32* dstrow = (Uint32*)pdc->cur_dst;
    Uint32* srcrow = (Uint32*)src;

    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        _dc_put_hline_set_0 (pdc, src, w);
    }
    else {
        if (pdc->bkmode == BM_TRANSPARENT) {
            while (w > 0) {
                if (*srcrow != pdc->skip_pixel)
                    *dstrow = *srcrow;
                dstrow += pdc->step;
                srcrow += pdc->step;
                w -= pdc->step;
            }
        }
        else while (w > 0) {
            *dstrow = *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
}

static void _dc_put_hline_and_1 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memandcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT
            && !((Uint32)row & 3) && !((Uint32)src & 3) && !(w & 3)) {
        ASM_memandcpy4 (row, src, w >> 2);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*src != pdc->skip_pixel)
                *row &= *src;
            row += pdc->step;
            src += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *row &= *src;
        row += pdc->step;
        src += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_and_2 (PDC pdc, Uint8* src, int w)
{
    Uint16* dstrow = (Uint16*)pdc->cur_dst;
    Uint16* srcrow = (Uint16*)src;

#ifdef ASM_memandcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT
            && !((Uint32)dstrow & 3) && !((Uint32)srcrow & 3) && !(w & 1)) {
        ASM_memandcpy4 (dstrow, srcrow, w >> 1);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow &= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *dstrow &= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_and_3 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memandcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        int i;
        int n = (w << 1) + w;
        int len = n >> 2;
        if (!((Uint32)row & 3) && !((Uint32)src & 3) && len)
            ASM_memandcpy4 (row, src, len);

        len <<= 2;
        row += len; src += len;
        for (i = 0; i < (n % 4); i++) {
            *row++ &= *src++;
        }
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
#if 0
            if (((* (Uint32*)row) & 0x00FFFFFF) != pdc->skip_pixel) {
#else
            if (!EQUAL_24BIT (row, pdc->skip_pixel)) {
#endif
               *row &= *src;
               *(row + 1) &= *(src + 1);
               *(row + 2) &= *(src + 2);
            }
            row += step;
            src += step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *row &= *src;
        *(row + 1) &= *(src + 1);
        *(row + 2) &= *(src + 2);
        row += step;
        src += step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_and_4 (PDC pdc, Uint8* src, int w)
{
    Uint32* dstrow = (Uint32*)pdc->cur_dst;
    Uint32* srcrow = (Uint32*)src;

#ifdef ASM_memandcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        ASM_memandcpy4 (dstrow, srcrow, w);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow &= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *dstrow &= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_or_1 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT
            && !((Uint32)row & 3) && !((Uint32)src & 3) && !(w & 3)) {
        ASM_memorcpy4 (row, src, w >> 2);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*src != pdc->skip_pixel)
                *row |= *src;
            row += pdc->step;
            src += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *row |= *src;
        row += pdc->step;
        src += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_or_2 (PDC pdc, Uint8* src, int w)
{
    Uint16* dstrow = (Uint16*)pdc->cur_dst;
    Uint16* srcrow = (Uint16*)src;

#ifdef ASM_memorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT
            && !((Uint32)dstrow & 3) && !((Uint32)srcrow & 3) && !(w & 1)) {
        ASM_memorcpy4 (dstrow, srcrow, w >> 1);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow |= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *dstrow |= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_or_3 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        int i;
        int n = (w << 1) + w;
        int len = n >> 2;
        if (!((Uint32)row & 3) && !((Uint32)src & 3) && len)
            ASM_memorcpy4 (row, src, len);

        len <<= 2;
        row += len; src += len;
        for (i = 0; i < (n % 4); i++) {
            *row++ |= *src++;
        }
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
#if 0
            if (((* (Uint32*)row) & 0x00FFFFFF) != pdc->skip_pixel) {
#else
            if (!EQUAL_24BIT (row, pdc->skip_pixel)) {
#endif
                *row |= *src;
                *(row + 1) |= *(src + 1);
                *(row + 2) |= *(src + 2);
            }
            row += step;
            src += step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *row |= *src;
        *(row + 1) |= *(src + 1);
        *(row + 2) |= *(src + 2);
        row += step;
        src += step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_or_4 (PDC pdc, Uint8* src, int w)
{
    Uint32* dstrow = (Uint32*)pdc->cur_dst;
    Uint32* srcrow = (Uint32*)src;

#ifdef ASM_memorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        ASM_memorcpy4 (dstrow, srcrow, w);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow |= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    while (w > 0) {
        *dstrow |= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_xor_1 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = pdc->cur_dst;

#ifdef ASM_memxorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT 
            && !((Uint32)row & 3) && !((Uint32)src & 3) && !(w & 3)) {
        ASM_memxorcpy4 (row, src, w >> 2);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*src != pdc->skip_pixel)
                *row ^= *src;
            row += pdc->step;
            src += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *row ^= *src;
        row += pdc->step;
        src += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_xor_2 (PDC pdc, Uint8* src, int w)
{
    Uint16* dstrow = (Uint16*)pdc->cur_dst;
    Uint16* srcrow = (Uint16*)src;

#ifdef ASM_memxorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT
            && !((Uint32)dstrow & 3) && !((Uint32)srcrow & 3) && !(w & 1)) {
        ASM_memxorcpy4 (dstrow, srcrow, w >> 1);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow ^= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    else while (w > 0) {
        *dstrow ^= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_xor_3 (PDC pdc, Uint8* src, int w)
{
    Uint8* row = (Uint8*)pdc->cur_dst;
    int step = (pdc->step << 1) + pdc->step;

#ifdef ASM_memxorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        int i;
        int n = (w << 1) + w;
        int len = n >> 2;
        if (!((Uint32)row & 3) && !((Uint32)src & 3) && len)
            ASM_memxorcpy4 (row, src, len);

        len <<= 2;
        row += len; src += len;
        for (i = 0; i < (n % 4); i++) {
            *row++ ^= *src++;
        }
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
#if 0
            if (((* (Uint32*)row) & 0x00FFFFFF) != pdc->skip_pixel) {
#else
            if (!EQUAL_24BIT (row, pdc->skip_pixel)) {
#endif
                *row ^= *src;
                *(row + 1) ^= *(src + 1);
                *(row + 2) ^= *(src + 2);
            }
            row += step;
            src += step;
            w -= pdc->step;
        }
    }
    while (w > 0) {
        *row ^= *src;
        *(row + 1) ^= *(src + 1);
        *(row + 2) ^= *(src + 2);
        row += step;
        src += step;
        w -= pdc->step;
    }
}

static void _dc_put_hline_xor_4 (PDC pdc, Uint8* src, int w)
{
    Uint32* dstrow = (Uint32*)pdc->cur_dst;
    Uint32* srcrow = (Uint32*)src;

#ifdef ASM_memxorcpy4
    if (pdc->step == 1 && pdc->bkmode != BM_TRANSPARENT) {
        ASM_memxorcpy4 (dstrow, srcrow, w);
        return;
    }
#endif

    if (pdc->bkmode == BM_TRANSPARENT) {
        while (w > 0) {
            if (*srcrow != pdc->skip_pixel)
                *dstrow ^= *srcrow;
            dstrow += pdc->step;
            srcrow += pdc->step;
            w -= pdc->step;
        }
    }
    while (w > 0) {
        *dstrow ^= *srcrow;
        dstrow += pdc->step;
        srcrow += pdc->step;
        w -= pdc->step;
    }
}

#define NR_ROPS         4
#define NR_PIXEL_LEN    4

static DC_SET_PIXEL set_pixel_ops [NR_ROPS][NR_PIXEL_LEN] =
{
    {_dc_set_pixel_1, _dc_set_pixel_2, _dc_set_pixel_3, _dc_set_pixel_4},
    {_dc_and_pixel_1, _dc_and_pixel_2, _dc_and_pixel_3, _dc_and_pixel_4},
    {_dc_or_pixel_1, _dc_or_pixel_2, _dc_or_pixel_3, _dc_or_pixel_4},
    {_dc_xor_pixel_1, _dc_xor_pixel_2, _dc_xor_pixel_3, _dc_xor_pixel_4}
};

static DC_DRAW_HLINE draw_hline_ops [NR_ROPS][NR_PIXEL_LEN] =
{
    {_dc_draw_hline_set_1, _dc_draw_hline_set_2, 
        _dc_draw_hline_set_3, _dc_draw_hline_set_4},
    {_dc_draw_hline_and_1, _dc_draw_hline_and_2, 
        _dc_draw_hline_and_3, _dc_draw_hline_and_4},
    {_dc_draw_hline_or_1, _dc_draw_hline_or_2, 
        _dc_draw_hline_or_3, _dc_draw_hline_or_4},
    {_dc_draw_hline_xor_1, _dc_draw_hline_xor_2, 
        _dc_draw_hline_xor_3, _dc_draw_hline_xor_4}
};

static DC_PUT_HLINE put_hline_ops [NR_ROPS][NR_PIXEL_LEN] =
{
    {_dc_put_hline_set_1, _dc_put_hline_set_2, 
        _dc_put_hline_set_3, _dc_put_hline_set_4},
    {_dc_put_hline_and_1, _dc_put_hline_and_2, 
        _dc_put_hline_and_3, _dc_put_hline_and_4},
    {_dc_put_hline_or_1, _dc_put_hline_or_2, 
        _dc_put_hline_or_3, _dc_put_hline_or_4},
    {_dc_put_hline_xor_1, _dc_put_hline_xor_2, 
        _dc_put_hline_xor_3, _dc_put_hline_xor_4}
};

static DC_MOVE_TO move_to_ops [NR_PIXEL_LEN] = 
{
    _dc_move_to_1,
    _dc_move_to_2,
    _dc_move_to_3,
    _dc_move_to_4
};

static DC_STEP_X step_x_ops [NR_PIXEL_LEN] = 
{
    _dc_step_x_1,
    _dc_step_x_2,
    _dc_step_x_3,
    _dc_step_x_4
};

/* This function initializes a DC: set the default parameters. */
static void dc_InitDC (PDC pdc, HWND hWnd, BOOL bIsClient)
{
    PCONTROL pCtrl;

    pdc->hwnd = hWnd;

    pdc->bkcolor = GAL_MapRGB (pdc->surface->format, 0xFF, 0xFF, 0xFF);
    pdc->bkmode = 0;

    pdc->pencolor = GAL_MapRGB (pdc->surface->format, 0x00, 0x00, 0x00);
    pdc->brushcolor = GAL_MapRGB (pdc->surface->format, 0xFF, 0xFF, 0xFF);

    pdc->textcolor = GAL_MapRGB (pdc->surface->format, 0x00, 0x00, 0x00);
    if (!(pdc->pLogFont = GetWindowFont (hWnd)))
        pdc->pLogFont = GetSystemFont (SYSLOGFONT_WCHAR_DEF);
    pdc->tabstop = 8;
    pdc->cExtra = pdc->alExtra = pdc->blExtra = 0;

    pdc->mapmode = MM_TEXT;
    pdc->ViewOrig.x = pdc->ViewOrig.y = 0;
    pdc->ViewExtent.x = pdc->ViewExtent.y = 1;
    pdc->WindowOrig.x = pdc->WindowOrig.y = 0;
    pdc->WindowExtent.x = pdc->WindowExtent.y = 1;

#ifdef _ADV_2DAPI
    pdc->pen_type = PT_SOLID;
    pdc->pen_cap_style = PT_CAP_BUTT;
    pdc->pen_join_style = PT_JOIN_MITER;
    pdc->pen_width = 0;
    pdc->dash_offset = 0;
    pdc->dash_list = NULL;
    pdc->dash_list_len = 0;

    pdc->brush_type = BT_SOLID;
    pdc->brush_orig.x = pdc->brush_orig.y = 0;
    pdc->brush_tile = NULL;
    pdc->brush_stipple = NULL;
#endif

    /* Assume that the local clip region is empty. */
    /* Get global clip region info and generate effective clip region. */
    if (dc_IsGeneralDC (pdc)) {
        RECT minimal;

        pdc->pGCRInfo = GetGCRgnInfo (hWnd);
        LOCK_GCRINFO (pdc);

        pdc->oldage = pdc->pGCRInfo->age;
        ClipRgnCopy (&pdc->ecrgn, &pdc->pGCRInfo->crgn);

        pdc->bIsClient = bIsClient;
        if (bIsClient)
            WndClientRect (pdc->hwnd, &pdc->DevRC);
        else
            WndRect (pdc->hwnd, &pdc->DevRC);

        minimal = pdc->DevRC;

        pCtrl = Control (pdc->hwnd);
        if (pCtrl && !(pCtrl->dwExStyle & WS_EX_CTRLASMAINWIN))
            RestrictControlECRGN (&minimal, pCtrl);

        IntersectClipRect (&pdc->ecrgn, &minimal);

        UNLOCK_GCRINFO (pdc);
    }

    /* context info and raster operations. */
    pdc->CurPenPos.x = pdc->CurTextPos.x = 0;
    pdc->CurPenPos.y = pdc->CurTextPos.y = 0;

    pdc->rop = ROP_SET;
    pdc->step = 1;
    pdc->set_pixel = 
        set_pixel_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->draw_hline = 
        draw_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->put_hline = 
        put_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];

    pdc->cur_dst = (BYTE*)pdc->surface->pixels 
            + pdc->surface->pitch * pdc->DevRC.top
            + pdc->surface->format->BytesPerPixel * pdc->DevRC.left;

    pdc->move_to = move_to_ops [pdc->surface->format->BytesPerPixel - 1];
    pdc->step_x = step_x_ops [pdc->surface->format->BytesPerPixel - 1];

    memset (pdc->gray_pixels, 0, sizeof (pdc->gray_pixels));
    memset (pdc->filter_pixels, 0, sizeof (pdc->filter_pixels));
    pdc->alpha_pixel_format = NULL;
}

/* This function initilaizes a memory DC from a compliant reference dc 
 * and copies the DC attributes from the refrence DC. */
static void dc_InitMemDCFrom (PDC pdc, const PDC pdc_ref)
{
    /* copy attributes from reference DC */
    memcpy (&pdc->bkcolor, &pdc_ref->bkcolor, 
                sizeof (gal_pixel)*4 + sizeof (int)*6);
#ifdef _ADV_2DAPI
    memcpy (&pdc->pen_type, &pdc_ref->pen_type, 
                (sizeof(int)*7) + sizeof(POINT) + (sizeof (void*)*3));
#endif
    pdc->pLogFont = pdc_ref->pLogFont;

    /* reset view point info. */
    pdc->ViewOrig.x = pdc->ViewOrig.y = 0;
    pdc->ViewExtent.x = pdc->ViewExtent.y = 1;
    pdc->WindowOrig.x = pdc->WindowOrig.y = 0;
    pdc->WindowExtent.x = pdc->WindowExtent.y = 1;

    /* reset context info. */
    pdc->CurPenPos.x = pdc->CurTextPos.x = 0;
    pdc->CurPenPos.y = pdc->CurTextPos.y = 0;

    /* reset raster operations. */
    pdc->rop = pdc_ref->rop;
    pdc->step = 1;
    pdc->set_pixel = 
        set_pixel_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->draw_hline = 
        draw_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->put_hline = 
        put_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];

    pdc->cur_dst = (BYTE*)pdc->surface->pixels 
            + pdc->surface->pitch * pdc->DevRC.top
            + pdc->surface->format->BytesPerPixel * pdc->DevRC.left;

    pdc->move_to = move_to_ops [pdc->surface->format->BytesPerPixel - 1];
    pdc->step_x = step_x_ops [pdc->surface->format->BytesPerPixel - 1];

    memset (pdc->gray_pixels, 0, sizeof (pdc->gray_pixels));
    memset (pdc->filter_pixels, 0, sizeof (pdc->filter_pixels));
    pdc->alpha_pixel_format = NULL;
}

static void dc_InitScreenDC (PDC pdc, GAL_Surface *surface)
{
    pdc->DataType = TYPE_HDC;
    pdc->DCType   = TYPE_SCRDC;
    pdc->hwnd = 0;
    pdc->surface = surface;

    pdc->bkcolor = PIXEL_lightwhite;
    pdc->bkmode = 0;

    pdc->brushcolor = PIXEL_lightwhite;

    pdc->pencolor = PIXEL_black;
    pdc->CurPenPos.x = pdc->CurPenPos.y = 0;

    pdc->textcolor = PIXEL_black;
    pdc->pLogFont = GetSystemFont (SYSLOGFONT_WCHAR_DEF);
    pdc->tabstop = 8;
    pdc->CurTextPos.x = pdc->CurTextPos.y = 0;
    pdc->cExtra = pdc->alExtra = pdc->blExtra = 0;

    pdc->ViewOrig.x = pdc->ViewOrig.y = 0;
    pdc->ViewExtent.x = pdc->ViewExtent.y = 1;
    pdc->WindowOrig.x = pdc->WindowOrig.y = 0;
    pdc->WindowExtent.x = pdc->WindowExtent.y = 1;

#ifdef _ADV_2DAPI
    pdc->pen_type = PT_SOLID;
    pdc->pen_cap_style = PT_CAP_BUTT;
    pdc->pen_join_style = PT_JOIN_MITER;
    pdc->pen_width = 0;
    pdc->dash_offset = 0;
    pdc->dash_list = NULL;
    pdc->dash_list_len = 0;

    pdc->brush_type = BT_SOLID;
    pdc->brush_orig.x = pdc->brush_orig.y = 0;
    pdc->brush_tile = NULL;
    pdc->brush_stipple = NULL;
#endif

    pdc->bIsClient = FALSE;

    /* init local clippping region */
    /* InitClipRgn (&pdc->lcrgn, &__mg_FreeClipRectList); */
    /* init effective clippping region */
    InitClipRgn (&pdc->ecrgn, &__mg_FreeClipRectList);

    /* init global clip region information */
    pdc->pGCRInfo = NULL;
    pdc->oldage = 0;

    pdc->DevRC.left = 0;
    pdc->DevRC.top  = 0;
    pdc->DevRC.right = surface->w;
    pdc->DevRC.bottom = surface->h;

    /* Set effetive clippping region to the screen. */
    /* SetClipRgn (&pdc->lcrgn, &pdc->DevRC); */
    SetClipRgn (&pdc->ecrgn, &pdc->DevRC);

    /* context info and raster operations. */
    pdc->rop = ROP_SET;
    pdc->step = 1;
    pdc->set_pixel = set_pixel_ops [pdc->rop]
            [pdc->surface->format->BytesPerPixel - 1];
    pdc->draw_hline = draw_hline_ops [pdc->rop]
            [pdc->surface->format->BytesPerPixel - 1];
    pdc->put_hline = put_hline_ops [pdc->rop]
            [pdc->surface->format->BytesPerPixel - 1];

    pdc->cur_dst = pdc->surface->pixels;
    pdc->move_to = move_to_ops [pdc->surface->format->BytesPerPixel - 1];
    pdc->step_x = step_x_ops [pdc->surface->format->BytesPerPixel - 1];

    memset (pdc->gray_pixels, 0, sizeof (pdc->gray_pixels));
    memset (pdc->filter_pixels, 0, sizeof (pdc->filter_pixels));
    pdc->alpha_pixel_format = NULL;
}
        
int GUIAPI GetRasterOperation (HDC hdc)
{
    PDC pdc;

    pdc = dc_HDC2PDC (hdc);
    return pdc->rop;
}

int GUIAPI SetRasterOperation (HDC hdc, int rop)
{
    PDC pdc;
    int old;

    pdc = dc_HDC2PDC (hdc);
    old = pdc->rop;

    if (rop >= 0 && rop < NR_ROPS) {
        pdc->rop = rop;
        pdc->set_pixel = set_pixel_ops [rop]
                [pdc->surface->format->BytesPerPixel - 1];
        pdc->draw_hline = draw_hline_ops [rop]
                [pdc->surface->format->BytesPerPixel - 1];
        pdc->put_hline = put_hline_ops [rop]
                [pdc->surface->format->BytesPerPixel - 1];
    }

    return old;
}

/*
 * Function: HDC GUIAPI GetClientDC (HWND hWnd)
 *     This function get the specified window client's DC.
 * Parameter:
 *     HWND hWnd: The window, 0 for screen.
 * Return:
 *     The handle of wanted DC.
 */

HDC GUIAPI GetClientDC (HWND hWnd)
{
    int i;

    LOCK (&dcslot);
    for (i = 0; i < DCSLOTNUMBER; i++) {
        if (!DCSlot[i].inuse) {
            DCSlot[i].inuse = TRUE;
            DCSlot[i].DataType = TYPE_HDC;
            DCSlot[i].DCType   = TYPE_GENDC;
            DCSlot[i].surface = __gal_screen;
            break;
        }
    }
    UNLOCK (&dcslot);

    if (i >= DCSLOTNUMBER)
        return HDC_SCREEN;

    dc_InitDC (DCSlot + i, hWnd, TRUE);
    return (HDC) (DCSlot + i);
}

/*
 * Function: HDC GUIAPI GetDC(HWND hWnd)
 *     This function get the specified window's DC.
 * Parameter:
 *     HWND hWnd: The window, 0 for screen.
 * Return:
 *     The handle of wanted DC.
 */

HDC GUIAPI GetDC(HWND hWnd)
{
    int i;

    /* allocate an empty dc slot exclusively */
    LOCK (&dcslot);
    for (i = 0; i < DCSLOTNUMBER; i++) {
        if(!DCSlot[i].inuse) {
            DCSlot[i].inuse = TRUE;
            DCSlot[i].DataType = TYPE_HDC;
            DCSlot[i].DCType   = TYPE_GENDC;
            DCSlot[i].surface = __gal_screen;
            break;
        }
    }
    UNLOCK(&dcslot);

    if (i >= DCSLOTNUMBER)
        return HDC_SCREEN;

    dc_InitDC (DCSlot + i, hWnd, FALSE);
    return (HDC)(DCSlot + i);
}

/*
 * Function: void GUIAPI ReleaseDC(HDC hDC)
 *     This function release the specified DC.
 * Parameter:
 *     HDC hDC: The DC handle want to release.
 * Return:
 *     None. 
 */
void GUIAPI ReleaseDC (HDC hDC)
{
    PMAINWIN pWin;
    PDC pdc;
    PCONTROL pCtrl;

    pdc = dc_HDC2PDC(hDC);

    if (pdc->alpha_pixel_format)
        GAL_FreeFormat (pdc->alpha_pixel_format);

    EmptyClipRgn (&pdc->lcrgn);

    pWin = (PMAINWIN)(pdc->hwnd);
    if (pWin && pWin->privCDC == hDC) {
        RECT minimal;

        /* for private DC, we reset the clip region info. */
        LOCK (&pdc->pGCRInfo->lock);

        pdc->oldage = pdc->pGCRInfo->age;
        ClipRgnCopy (&pdc->ecrgn, &pdc->pGCRInfo->crgn);

        if (pdc->bIsClient)
            WndClientRect (pdc->hwnd, &pdc->DevRC);
        else
            WndRect (pdc->hwnd, &pdc->DevRC);

        minimal = pdc->DevRC;

        pCtrl = Control (pdc->hwnd);
        if (pCtrl && !(pCtrl->dwExStyle & WS_EX_CTRLASMAINWIN))
            RestrictControlECRGN (&minimal, pCtrl);

        IntersectClipRect (&pdc->ecrgn, &minimal);

        UNLOCK (&pdc->pGCRInfo->lock);
    }
    else {
        EmptyClipRgn (&pdc->ecrgn);
        pdc->pGCRInfo = NULL;
        pdc->oldage = 0;

        LOCK (&dcslot);
        pdc->inuse = FALSE;
        UNLOCK(&dcslot);
    }
}

/*
 * Function: HDC GUIAPI GetSubDC(HDC hdc,int off_x,int off_y,int width,int height);
 *     This function get the specified window's DC compatible with hdc.
 * Parameter:
 *      HDC hdc: The client DC of window. cannot be 0
 *      int off_x: the x-start pos of sub dc. if x<0  x=0. Device coordinate
 *      int off_y: the y-start pos of sub dc. if y<0 y=0. Device coordinate
 *      int width: the width of sub dc. if width>x+windows.width,width = window.width-x
 *      int height: the height of sub dc. if height>y+window.height,height=window.height-y
 */
HDC GUIAPI GetSubDC (HDC hdc, int off_x, int off_y, int width, int height)
{
    int i;
    int parent_width,parent_height;
    PDC pdc;
    PDC pdc_parent;

    pdc_parent = dc_HDC2PDC (hdc);
    if (pdc_parent == NULL || pdc_parent->DataType != TYPE_HDC 
            || pdc_parent->DCType != TYPE_GENDC)
        return HDC_INVALID;

    parent_width = RECTW (pdc_parent->DevRC);
    parent_height = RECTH (pdc_parent->DevRC);
    if (off_x < 0 || off_x  >=  parent_width 
            || off_y < 0 || off_y >= parent_height || width <= 0 || height <= 0)
        return HDC_INVALID;

    if (off_x + width > parent_width)
        width =  parent_width - off_x;
    if (off_y+height > parent_height)
        height = parent_height - off_y;

    pdc = NULL;
    LOCK (&dcslot);
    for (i = 0; i < DCSLOTNUMBER; i++) {
        if (!DCSlot[i].inuse) {
            pdc = &DCSlot [i];
            pdc->inuse = TRUE;
            pdc->DataType = TYPE_HDC;
            pdc->DCType = TYPE_GENDC;
            break;
        }
    }
    UNLOCK (&dcslot);

    if (pdc == NULL)
        return HDC_INVALID;

    memcpy (&pdc, &pdc_parent, sizeof (DC));

    pdc->ViewOrig.x = pdc->ViewOrig.y = 0;
    pdc->ViewExtent.x = pdc->ViewExtent.y = 1;
    pdc->WindowOrig.x = pdc->WindowOrig.y = 0;
    pdc->WindowExtent.x = pdc->WindowExtent.y = 1;

    pdc->CurPenPos.x = pdc->CurTextPos.x = 0;
    pdc->CurPenPos.y = pdc->CurTextPos.y = 0;

    pdc->bIsClient = FALSE;
    pdc->DevRC.left = pdc_parent->DevRC.left + off_x;
    pdc->DevRC.top = pdc_parent->DevRC.top + off_y;
    pdc->DevRC.right = pdc->DevRC.left +width;
    pdc->DevRC.bottom = pdc->DevRC.top + height;

    pdc->cur_dst = (BYTE*)pdc->surface->pixels 
        + pdc->surface->pitch * pdc->DevRC.top
        + pdc->surface->format->BytesPerPixel * pdc->DevRC.left;

    CopyRegion (&pdc->lcrgn, &pdc_parent->lcrgn);
    CopyRegion (&pdc->ecrgn, &pdc_parent->ecrgn);
    IntersectClipRect (&pdc->ecrgn, &pdc->DevRC);

    memset (pdc->gray_pixels, 0, sizeof (pdc->gray_pixels));
    memset (pdc->filter_pixels, 0, sizeof (pdc->filter_pixels));
    pdc->alpha_pixel_format = NULL;

    return (HDC)pdc;
}

HDC GUIAPI CreatePrivateDC(HWND hwnd)
{
    PDC pdc;

    if (!(pdc = malloc (sizeof(DC)))) return HDC_INVALID;

    InitClipRgn (&pdc->lcrgn, &__mg_FreeClipRectList);
    InitClipRgn (&pdc->ecrgn, &__mg_FreeClipRectList);
    
    pdc->inuse = TRUE;
    pdc->DataType = TYPE_HDC;
    pdc->DCType   = TYPE_GENDC;
    pdc->surface = __gal_screen;
    dc_InitDC (pdc, hwnd, FALSE);
    return (HDC)(pdc);
}

HDC GUIAPI CreatePrivateClientDC(HWND hwnd)
{
    PDC pdc;

    if (!(pdc = malloc (sizeof(DC)))) return HDC_INVALID;

    InitClipRgn (&pdc->lcrgn, &__mg_FreeClipRectList);
    InitClipRgn (&pdc->ecrgn, &__mg_FreeClipRectList);
    
    pdc->inuse = TRUE;
    pdc->DataType = TYPE_HDC;
    pdc->DCType   = TYPE_GENDC;
    pdc->surface = __gal_screen;
    dc_InitDC(pdc, hwnd, TRUE);
    return (HDC)(pdc);
}

void GUIAPI DeletePrivateDC(HDC hdc)
{
    PDC pdc;

    pdc = (PDC)hdc;
    
    if (pdc->alpha_pixel_format)
        GAL_FreeFormat (pdc->alpha_pixel_format);

    EmptyClipRgn (&pdc->lcrgn);
    EmptyClipRgn (&pdc->ecrgn);

    free (pdc);
}

HDC GUIAPI GetPrivateClientDC (HWND hwnd)
{
    PMAINWIN pWin = (PMAINWIN)hwnd;

    return pWin->privCDC;
}

/* LockDC/UnlockDC to get direct access to the pixels in a DC. */
Uint8* GUIAPI LockDC (HDC hdc, const RECT* rw_rc, int* width, int* height, 
                int* pitch)
{
    PDC pdc;
    Uint8* pixels = NULL;

    if (!(pdc = __mg_check_ecrgn (hdc)))
        return NULL;

    if (rw_rc) {
        pdc->rc_output = *rw_rc;

        /* Transfer device to device to screen here. */
        coor_DP2SP (pdc, &pdc->rc_output.left, &pdc->rc_output.top);
        coor_DP2SP (pdc, &pdc->rc_output.right, &pdc->rc_output.bottom);
        if (!IntersectRect (&pdc->rc_output, &pdc->rc_output, &pdc->DevRC))
            goto fail;
    }
    else
        pdc->rc_output = pdc->DevRC;

    BLOCK_DRAW_SEM (pdc);

    LOCK (&__mg_gdilock);
    if (!dc_IsMemDC (pdc)) ShowCursorForGDI (FALSE, pdc);

    if (width) *width = RECTW(pdc->rc_output);
    if (height) *height = RECTH(pdc->rc_output);
    if (pitch) *pitch = pdc->surface->pitch;

    pixels = pdc->surface->pixels;
    pixels += pdc->surface->pitch * pdc->rc_output.top;
    pixels += pdc->surface->format->BytesPerPixel * pdc->rc_output.left;

    return pixels;

fail:
    UNBLOCK_DRAW_SEM (pdc);
    if (dc_IsGeneralDC (pdc))
        UNLOCK_GCRINFO (pdc);
    return NULL;
}

void GUIAPI UnlockDC (HDC hdc)
{
    PDC pdc;

    pdc = dc_HDC2PDC (hdc);
    if (!dc_IsMemDC (pdc))
        ShowCursorForGDI (TRUE, pdc);

    UNLOCK (&__mg_gdilock);
    UNBLOCK_DRAW_SEM (pdc);
    if (dc_IsGeneralDC (pdc))
        UNLOCK_GCRINFO (pdc);
}

/******************************* Memory DC ***********************************/
HDC GUIAPI CreateCompatibleDCEx (HDC hdc, int width, int height)
{
    PDC pdc;
    PDC pmem_dc = NULL;
    GAL_Surface* surface;
    DWORD flags;

    pdc = dc_HDC2PDC (hdc);

    if (!(pmem_dc = malloc (sizeof(DC))))
        return HDC_INVALID;

    if (width <= 0 || height <= 0) {
        width = RECTW (pdc->DevRC);
        height = RECTH (pdc->DevRC);
    }

    if ((pdc->surface->flags & GAL_HWSURFACE) == GAL_HWSURFACE)
        flags = GAL_HWSURFACE;
    else
        flags = GAL_SWSURFACE;

    LOCK (&__mg_gdilock);
    surface = GAL_CreateRGBSurface (flags,
                    width, height,
                    pdc->surface->format->BitsPerPixel, 
                    pdc->surface->format->Rmask,
                    pdc->surface->format->Gmask,
                    pdc->surface->format->Bmask,
                    pdc->surface->format->Amask);
    UNLOCK (&__mg_gdilock);

    if (!surface) {
        free (pmem_dc);
        return HDC_INVALID;
    }

    /* Set surface attributes */
    if (pdc->surface->format->BitsPerPixel <= 8) {
        GAL_SetPalette (surface, GAL_LOGPAL, 
            (GAL_Color*) pdc->surface->format->palette->colors, 
            0, 1<<pdc->surface->format->BitsPerPixel);
    }

    if (pdc->surface->flags & GAL_SRCALPHA) {
        GAL_SetAlpha (surface, GAL_SRCALPHA, 
                    pdc->surface->format->alpha);
    }

    if (pdc->surface->flags & GAL_SRCCOLORKEY) {
        GAL_SetColorKey (surface, GAL_SRCCOLORKEY, 
                pdc->surface->format->colorkey);
    }

    pmem_dc->DataType = TYPE_HDC;
    pmem_dc->DCType   = TYPE_MEMDC;
    pmem_dc->inuse = TRUE;
    pmem_dc->surface = surface;

    pmem_dc->DevRC.left = 0;
    pmem_dc->DevRC.top  = 0;
    pmem_dc->DevRC.right = width;
    pmem_dc->DevRC.bottom = height;

    dc_InitMemDCFrom (pmem_dc, pdc);

    /* clip region info */
    pmem_dc->pGCRInfo = NULL;
    pmem_dc->oldage = 0;
    InitClipRgn (&pmem_dc->ecrgn, &__mg_FreeClipRectList);
    SetClipRgn (&pmem_dc->ecrgn, &pmem_dc->DevRC);
    
    return (HDC)pmem_dc;
}

HDC GUIAPI CreateMemDC (int width, int height, int depth, DWORD flags,
                        Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    PDC pmem_dc = NULL;
    GAL_Surface* surface;

    if (!(pmem_dc = malloc (sizeof(DC))))
        return HDC_INVALID;

    LOCK (&__mg_gdilock);
    surface = GAL_CreateRGBSurface (flags, width, height, depth,
                    Rmask, Gmask, Bmask, Amask);
    UNLOCK (&__mg_gdilock);

    if (!surface) {
        free (pmem_dc);
        return HDC_INVALID;
    }

    pmem_dc->DataType = TYPE_HDC;
    pmem_dc->DCType   = TYPE_MEMDC;
    pmem_dc->inuse    = TRUE;
    pmem_dc->surface  = surface;

    dc_InitDC (pmem_dc, HWND_DESKTOP, FALSE);

    InitClipRgn (&pmem_dc->ecrgn, &__mg_FreeClipRectList);
    pmem_dc->pGCRInfo = NULL;
    pmem_dc->oldage = 0;

    pmem_dc->DevRC.left = 0;
    pmem_dc->DevRC.top  = 0;
    pmem_dc->DevRC.right = width;
    pmem_dc->DevRC.bottom = height;

    SetClipRgn (&pmem_dc->ecrgn, &pmem_dc->DevRC);
    
    return (HDC)pmem_dc;
}

HDC GUIAPI CreateSubMemDC (HDC parent, int off_x, int off_y, 
                int width, int height, BOOL comp_to_parent)
{
    PDC pdc_parent;
    PDC pdc_sub;
    GAL_Surface* surface;
    BYTE* sub_pixels;
 
    pdc_parent = dc_HDC2PDC (parent);
    if (pdc_parent->DCType != TYPE_MEMDC)
        return HDC_INVALID;

    if (off_x < 0 || off_x >= pdc_parent->DevRC.right
            || off_y < 0 || off_y >= pdc_parent->DevRC.bottom
            || width <= 0 || height <= 0)
        return HDC_INVALID;

    if (width > RECTW (pdc_parent->DevRC))
        width = RECTW (pdc_parent->DevRC);
    if (height > RECTH (pdc_parent->DevRC))
        height = RECTH (pdc_parent->DevRC);

    if (off_x + width > pdc_parent->DevRC.right)
        width = pdc_parent->DevRC.right - off_x;
    if (off_y + height > pdc_parent->DevRC.bottom)
        height = pdc_parent->DevRC.bottom - off_y;
    
    if (!(pdc_sub = malloc (sizeof(DC))))
        return HDC_INVALID;

    sub_pixels = (BYTE*)(pdc_parent->surface->pixels) +
                pdc_parent->surface->pitch * off_y +
                pdc_parent->surface->format->BytesPerPixel * off_x;

    surface = GAL_CreateRGBSurfaceFrom (sub_pixels,
                    width, height, pdc_parent->surface->format->BitsPerPixel,
                    pdc_parent->surface->pitch,
                    pdc_parent->surface->format->Rmask,
                    pdc_parent->surface->format->Gmask,
                    pdc_parent->surface->format->Bmask,
                    pdc_parent->surface->format->Amask);

    if (!surface) {
        free (pdc_sub);
        return HDC_INVALID;
    }

    pdc_sub->DataType = TYPE_HDC;
    pdc_sub->DCType   = TYPE_MEMDC;
    pdc_sub->inuse    = TRUE;
    pdc_sub->surface  = surface;

    pdc_sub->DevRC.left = 0;
    pdc_sub->DevRC.top  = 0;
    pdc_sub->DevRC.right = width;
    pdc_sub->DevRC.bottom = height;

    if (comp_to_parent) {
        dc_InitMemDCFrom (pdc_sub, pdc_parent);

        /* Set surface attributes */
        if (pdc_parent->surface->format->BitsPerPixel <= 8) {
            GAL_SetPalette (surface, GAL_LOGPAL, 
                        pdc_parent->surface->format->palette->colors, 
                        0, 1<<pdc_parent->surface->format->BitsPerPixel);
        }

        if (pdc_parent->surface->flags & GAL_SRCALPHA) {
            GAL_SetAlpha (surface, GAL_SRCALPHA, 
                        pdc_parent->surface->format->alpha);
        }

        if (pdc_parent->surface->flags & GAL_SRCCOLORKEY) {
            GAL_SetColorKey (surface, GAL_SRCCOLORKEY, 
                    pdc_parent->surface->format->colorkey);
        }
    }
    else {
        dc_InitDC (pdc_sub, HWND_DESKTOP, FALSE);
    }

    pdc_sub->pGCRInfo = NULL;
    pdc_sub->oldage = 0;
    InitClipRgn (&pdc_sub->ecrgn, &__mg_FreeClipRectList);

    if (comp_to_parent) {
        CopyRegion (&pdc_sub->ecrgn, &pdc_parent->ecrgn);
        if (off_x || off_y)
            OffsetRegion (&pdc_sub->ecrgn, -off_x, -off_y);
        IntersectClipRect (&pdc_sub->ecrgn, &pdc_sub->DevRC);
    }
    else {
        SetClipRgn (&pdc_sub->ecrgn, &pdc_sub->DevRC);
    }

    return (HDC)pdc_sub;
}

HDC GUIAPI CreateMemDCFromBitmap (HDC hdc, BITMAP* bmp)
{
    PDC pdc, pmem_dc = NULL;
    GAL_Surface* surface;
    Uint32 Rmask = 0, Gmask = 0, Bmask = 0, Amask = 0;

    pdc = dc_HDC2PDC (hdc);
    if (bmp->bmType & BMP_TYPE_ALPHA) {
        switch (bmp->bmBytesPerPixel) {
            case 1:
                break;
            case 2:
                Rmask = 0x0000F000;
                Gmask = 0x00000F00;
                Bmask = 0x000000F0;
                Amask = 0x0000000F;
                break;
            case 3:
                Rmask = 0x00FC0000;
                Gmask = 0x0003F000;
                Bmask = 0x00000FC0;
                Amask = 0x0000003F;
                break;
            case 4:
                Rmask = 0xFF000000;
                Gmask = 0x00FF0000;
                Bmask = 0x0000FF00;
                Amask = 0x000000FF;
                break;
            default:    /* not supported */
                return HDC_INVALID;
        }
    }
    else {
        Rmask = pdc->surface->format->Rmask;
        Gmask = pdc->surface->format->Gmask;
        Bmask = pdc->surface->format->Bmask;
        Amask = pdc->surface->format->Amask;
    }

    if (!(pmem_dc = malloc (sizeof(DC))))
        return HDC_INVALID;

    surface = GAL_CreateRGBSurfaceFrom (bmp->bmBits, 
                    bmp->bmWidth, bmp->bmHeight, bmp->bmBitsPerPixel, 
                    bmp->bmPitch, Rmask, Gmask, Bmask, Amask);

    if (!surface) {
        free (pmem_dc);
        return HDC_INVALID;
    }

    /* Set surface attributes */
    if (bmp->bmBitsPerPixel <= 8) {
        GAL_SetPalette (surface, GAL_LOGPAL, 
                        pdc->surface->format->palette->colors, 0, 
                        1<<bmp->bmBitsPerPixel);
    }

    if (bmp->bmType & BMP_TYPE_ALPHACHANNEL) {
        GAL_SetAlpha (surface, GAL_SRCALPHA, bmp->bmAlpha);
    }

    if (bmp->bmType & BMP_TYPE_COLORKEY) {
        GAL_SetColorKey (surface, GAL_SRCCOLORKEY, bmp->bmColorKey);
    }

    pmem_dc->DataType = TYPE_HDC;
    pmem_dc->DCType   = TYPE_MEMDC;
    pmem_dc->inuse    = TRUE;
    pmem_dc->surface  = surface;

    dc_InitDC (pmem_dc, HWND_DESKTOP, FALSE);

    InitClipRgn (&pmem_dc->ecrgn, &__mg_FreeClipRectList);

    pmem_dc->pGCRInfo = NULL;
    pmem_dc->oldage = 0;

    pmem_dc->DevRC.left = 0;
    pmem_dc->DevRC.top  = 0;
    pmem_dc->DevRC.right = bmp->bmWidth;
    pmem_dc->DevRC.bottom = bmp->bmHeight;

    SetClipRgn (&pmem_dc->ecrgn, &pmem_dc->DevRC);
    return (HDC)pmem_dc;
}

HDC GUIAPI CreateMemDCFromMyBitmap (const MYBITMAP* my_bmp, RGB* pal)
{
    PDC pmem_dc;
    GAL_Surface* surface;
    Uint32 Rmask = 0, Gmask = 0, Bmask = 0, Amask = 0;

    if (my_bmp->flags & MYBMP_FLOW_UP)
        return HDC_INVALID;

    switch (my_bmp->depth) {
    case 8:
        break;

    case 24:
        if (my_bmp->flags & MYBMP_ALPHA) {
            Amask = 0x0000003F;
            if ((my_bmp->flags & MYBMP_TYPE_MASK) == MYBMP_TYPE_RGB) {
                Rmask = 0x00FC0000;
                Gmask = 0x0003F000;
                Bmask = 0x00000FC0;
            }
            else {
                Rmask = 0x00000FC0;
                Gmask = 0x0003F000;
                Bmask = 0x00FC0000;
            }
        }
        else {
            Amask = 0x00000000;
            if ((my_bmp->flags & MYBMP_TYPE_MASK) == MYBMP_TYPE_RGB) {
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
            }
            else {
                Rmask = 0x000000FF;
                Gmask = 0x0000FF00;
                Bmask = 0x00FF0000;
            }
        }
        break;

    case 32:
        if ((my_bmp->flags & MYBMP_TYPE_MASK) == MYBMP_TYPE_RGB) {
            Rmask = 0xFF000000;
            Gmask = 0x00FF0000;
            Bmask = 0x0000FF00;
        }
        else {
            Bmask = 0xFF000000;
            Gmask = 0x00FF0000;
            Rmask = 0x0000FF00;
        }
        if (my_bmp->flags & MYBMP_ALPHA)
            Amask = 0x000000FF;
        else
            Amask = 0x00000000;
        break;

    default:
        return HDC_INVALID;
    }

    if (!(pmem_dc = malloc (sizeof(DC))))
        return HDC_INVALID;

    surface = GAL_CreateRGBSurfaceFrom (my_bmp->bits,
                    my_bmp->w, my_bmp->h, my_bmp->depth, my_bmp->pitch,
                    Rmask, Gmask, Bmask, Amask);

    if (!surface) {
        free (pmem_dc);
        return HDC_INVALID;
    }

    /* Set surface attributes */
    if (my_bmp->depth <= 8) {
        GAL_SetPalette (surface, GAL_LOGPAL, (GAL_Color*) pal, 0, 
                1<<my_bmp->depth);
    }

    if (my_bmp->flags & MYBMP_ALPHACHANNEL) {
        GAL_SetAlpha (surface, GAL_SRCALPHA, my_bmp->alpha);
    }

    if (my_bmp->flags & MYBMP_TRANSPARENT) {
        GAL_SetColorKey (surface, GAL_SRCCOLORKEY, 
                my_bmp->transparent);
    }

    pmem_dc->DataType = TYPE_HDC;
    pmem_dc->DCType   = TYPE_MEMDC;
    pmem_dc->inuse    = TRUE;
    pmem_dc->surface  = surface;

    dc_InitDC (pmem_dc, HWND_DESKTOP, FALSE);

    InitClipRgn (&pmem_dc->ecrgn, &__mg_FreeClipRectList);

    pmem_dc->pGCRInfo = NULL;
    pmem_dc->oldage = 0;

    pmem_dc->DevRC.left = 0;
    pmem_dc->DevRC.top  = 0;
    pmem_dc->DevRC.right = my_bmp->w;
    pmem_dc->DevRC.bottom = my_bmp->h;

    SetClipRgn (&pmem_dc->ecrgn, &pmem_dc->DevRC);
    return (HDC)pmem_dc;
}

BOOL GUIAPI ConvertMemDC (HDC mem_dc, HDC ref_dc, DWORD flags)
{
    PDC pmem_dc, pref_dc;
    GAL_Surface* new_surface;

    pmem_dc = dc_HDC2PDC (mem_dc);
    pref_dc = dc_HDC2PDC (ref_dc);

    new_surface = GAL_ConvertSurface (pmem_dc->surface, 
                pref_dc->surface->format, flags);
    if (!new_surface)
        return FALSE;

    GAL_FreeSurface (pmem_dc->surface);
    pmem_dc->surface = new_surface;
    return TRUE;
}

BOOL GUIAPI SetMemDCAlpha (HDC mem_dc, DWORD flags, Uint8 alpha)
{
    PDC pmem_dc = dc_HDC2PDC (mem_dc);

    return !GAL_SetAlpha (pmem_dc->surface, flags, alpha);
}

BOOL GUIAPI SetMemDCColorKey (HDC mem_dc, DWORD flags, Uint32 color_key)
{
    PDC pmem_dc = dc_HDC2PDC (mem_dc);

    return !GAL_SetColorKey (pmem_dc->surface, flags, color_key);
}

void GUIAPI DeleteMemDC (HDC hdc)
{
    PDC pmem_dc;
    
    pmem_dc = dc_HDC2PDC(hdc);

    GAL_FreeSurface (pmem_dc->surface);

    if (pmem_dc->alpha_pixel_format)
        GAL_FreeFormat (pmem_dc->alpha_pixel_format);

    EmptyClipRgn (&pmem_dc->ecrgn);

    free (pmem_dc);
}

HDC GUIAPI InitSlaveScreen (const char* name, const char* mode)
{
    PDC pmem_dc = NULL;
    GAL_Surface* surface = NULL;

    /* create slavescreen memory dc */
    if (!(pmem_dc = malloc (sizeof(DC))))
        return HDC_INVALID;

    if ((surface = Slave_VideoInit (name, mode))) {
        dc_InitScreenDC (pmem_dc, surface); 
        return (HDC)pmem_dc;
    }
    else {
        free (pmem_dc);
        fprintf (stderr, "NEWGDI: Can not init the slave screen: %s (%s)\n",
                        name, mode);
        return HDC_INVALID;
    }
}

void TerminateSlaveScreen (HDC hdc)
{
    PDC pmem_dc;
    
    if (hdc == HDC_INVALID)
        return;

    pmem_dc = dc_HDC2PDC (hdc);

    if (pmem_dc->DataType != TYPE_HDC || pmem_dc->DCType != TYPE_SCRDC)
        return;

    if (pmem_dc->alpha_pixel_format)
        GAL_FreeFormat (pmem_dc->alpha_pixel_format);

    EmptyClipRgn (&pmem_dc->ecrgn); 
    Slave_VideoQuit (pmem_dc->surface);

    free (pmem_dc);
}

HWND GUIAPI WindowFromDC (HDC hdc)
{
    PDC pdc;

    if (hdc == HDC_INVALID || hdc == HDC_SCREEN)
        return HWND_NULL;

    pdc = dc_HDC2PDC (hdc);

    if (pdc->DCType == TYPE_GENDC) {
        return pdc->hwnd;
    }

    return HWND_NULL;
}

typedef struct _DCSTATE
{
    GAL_Color bkcolor, pencolor, brushcolor, textcolor;
    /* bkmode, tabstop, cExtra, alExtra, blExtra, mapmode */
    char attrs_g1 [sizeof(int)*6];

#ifdef _ADV_2DAPI
    /* 
     * int: pen_type, pen_cap_style, pen_join_style, pen_width, brush_type;
     * POINT: brush_orig;
     * void*: brush_tile, brush_stipple;
     * int: dash_offset;
     * void* dash_list;
     * size_t: dash_list_len.
     */
    char attrs_adv [(sizeof(int)*7) + sizeof(POINT) + (sizeof (void*)*3)];
#endif

    /*
     * void*: pLogFont;
     * POINT: ViewOrig, ViewExtent, WindowOrig, WindowExtent;
     * POINT: CurPenPos, CurTextPos;
     * int rop.
     */
    char attrs_g2 [sizeof (void*) + sizeof(POINT)*6 + sizeof (int)];

    CLIPRGN lcrgn;

    GAL_Palette pal;

    struct _DCSTATE *next, *prev;
} DCSTATE;

static DCSTATE* dc_state_stack = NULL;
static int nr_dc_states = 0;

int GUIAPI SaveDC (HDC hdc)
{
    DCSTATE* dc_state;
    PDC pdc;

    dc_state = calloc (1, sizeof (DCSTATE));
    if (dc_state == NULL)
        return 0;

    dc_state->next = NULL;
    dc_state->prev = NULL;

    dc_state->pal.ncolors = 0;
    dc_state->pal.colors = NULL;

    pdc = dc_HDC2PDC (hdc);
    LOCK (&__mg_gdilock);

    InitClipRgn (&dc_state->lcrgn, &__mg_FreeClipRectList);

    if (pdc->surface->format->palette) {
        dc_state->pal.ncolors = pdc->surface->format->palette->ncolors;
        dc_state->pal.colors = calloc (dc_state->pal.ncolors, 
                sizeof (GAL_Color));

        if (dc_state->pal.colors == NULL)
            goto fail;
    }

    if (dc_IsGeneralDC (pdc)) {
        if (!ClipRgnCopy (&dc_state->lcrgn, &pdc->lcrgn))
            goto fail;
    }
    else {
        if (!ClipRgnCopy (&dc_state->lcrgn, &pdc->ecrgn))
            goto fail;
    }

    GAL_GetRGBA (pdc->bkcolor, pdc->surface->format, 
                &dc_state->bkcolor.r, &dc_state->bkcolor.g,
                &dc_state->bkcolor.b, &dc_state->bkcolor.a);
    GAL_GetRGBA (pdc->pencolor, pdc->surface->format, 
                &dc_state->pencolor.r, &dc_state->pencolor.g,
                &dc_state->pencolor.b, &dc_state->pencolor.a);
    GAL_GetRGBA (pdc->brushcolor, pdc->surface->format, 
                &dc_state->brushcolor.r, &dc_state->brushcolor.g,
                &dc_state->brushcolor.b, &dc_state->brushcolor.a);
    GAL_GetRGBA (pdc->textcolor, pdc->surface->format, 
                &dc_state->textcolor.r, &dc_state->textcolor.g,
                &dc_state->textcolor.b, &dc_state->textcolor.a);

    memcpy (dc_state->attrs_g1, &pdc->bkmode, sizeof (dc_state->attrs_g1));
#ifdef _ADV_2DAPI
    memcpy (dc_state->attrs_adv, &pdc->pen_type, sizeof (dc_state->attrs_adv));
#endif
    memcpy (dc_state->attrs_g2, &pdc->pLogFont, sizeof (dc_state->attrs_g2));

    nr_dc_states ++;
    if (dc_state_stack == NULL) {
        dc_state_stack = dc_state;
    }
    else {
        dc_state_stack->next = dc_state;
        dc_state->prev = dc_state_stack;
        dc_state_stack = dc_state;
    }

    UNLOCK (&__mg_gdilock);

    return nr_dc_states;

fail:
    UNLOCK (&__mg_gdilock);

    EmptyClipRgn (&dc_state->lcrgn);
    if (dc_state->pal.colors)
        free (dc_state->pal.colors);
    free (dc_state);
    return 0;
}

static void destroy_dc_state (DCSTATE* dc_state, BOOL empty_crgn)
{
    if (empty_crgn)
        EmptyClipRgn (&dc_state->lcrgn);
    free (dc_state->pal.colors);
    free (dc_state);
}

BOOL GUIAPI RestoreDC (HDC hdc, int saved_dc)
{
    DCSTATE* dc_state;
    PDC pdc;

    if ((saved_dc == 0) || 
                (saved_dc > nr_dc_states) || (-saved_dc > nr_dc_states))
        return FALSE;

    pdc = dc_HDC2PDC (hdc);
    LOCK (&__mg_gdilock);

    if (saved_dc > 0) {
        int n = nr_dc_states - saved_dc;
        while (n--) {
            dc_state = dc_state_stack->prev;
            destroy_dc_state (dc_state_stack, TRUE);
            dc_state_stack = dc_state;
            dc_state_stack->next = NULL;
            nr_dc_states --;
        }
    }
    else { /* saved_dc < 0 */
        int n = -saved_dc - 1;
        while (n--) {
            dc_state = dc_state_stack->prev;
            destroy_dc_state (dc_state_stack, TRUE);
            dc_state_stack = dc_state;
            dc_state_stack->next = NULL;
            nr_dc_states --;
        }
    }

    dc_state = dc_state_stack;
    if (pdc->surface->format->palette 
            && dc_state->pal.ncolors == pdc->surface->format->palette->ncolors){
        memcpy (pdc->surface->format->palette->colors, dc_state->pal.colors,
                sizeof (GAL_Color) * dc_state->pal.ncolors);
    }

    if (dc_IsGeneralDC (pdc)) {
        EmptyClipRgn (&pdc->lcrgn);
        pdc->lcrgn = dc_state->lcrgn;
    } else {
        EmptyClipRgn (&pdc->ecrgn);
        pdc->ecrgn = dc_state->lcrgn;
    }
    
    pdc->bkcolor = GAL_MapRGBA (pdc->surface->format, 
                dc_state->bkcolor.r, dc_state->bkcolor.g,
                dc_state->bkcolor.b, dc_state->bkcolor.a);
    pdc->pencolor = GAL_MapRGBA (pdc->surface->format, 
                dc_state->pencolor.r, dc_state->pencolor.g,
                dc_state->pencolor.b, dc_state->pencolor.a);
    pdc->brushcolor = GAL_MapRGBA (pdc->surface->format, 
                dc_state->brushcolor.r, dc_state->brushcolor.g,
                dc_state->brushcolor.b, dc_state->brushcolor.a);
    pdc->textcolor = GAL_MapRGBA (pdc->surface->format, 
                dc_state->textcolor.r, dc_state->textcolor.g,
                dc_state->textcolor.b, dc_state->textcolor.a);

    memcpy (&pdc->bkmode, dc_state->attrs_g1, sizeof (dc_state->attrs_g1));
#ifdef _ADV_2DAPI
    memcpy (&pdc->pen_type, dc_state->attrs_adv, sizeof (dc_state->attrs_adv));
#endif
    memcpy (&pdc->pLogFont, dc_state->attrs_g2, sizeof (dc_state->attrs_g2));

    /* calculate gray_pixels, filter_pixels, and alpha_pixel_format */
    SetDCAttr (hdc, DC_ATTR_TEXT_COLOR, pdc->textcolor);

    /* initialize the pixel operation methods */
    pdc->set_pixel = 
        set_pixel_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->draw_hline = 
        draw_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->put_hline = 
        put_hline_ops [pdc->rop][pdc->surface->format->BytesPerPixel - 1];
    pdc->move_to = move_to_ops [pdc->surface->format->BytesPerPixel - 1];
    pdc->step_x = step_x_ops [pdc->surface->format->BytesPerPixel - 1];

    if (dc_IsGeneralDC (pdc)) {
        /* force to regenerate the ecrgn */
        LOCK_GCRINFO (pdc);
        dc_GenerateECRgn (pdc, TRUE);
        UNLOCK_GCRINFO (pdc);
    }
    else {
        IntersectClipRect (&pdc->ecrgn, &pdc->DevRC);
    }

    dc_state_stack = dc_state->prev;
    if (dc_state_stack)
        dc_state_stack->next = NULL;
    nr_dc_states --;

    destroy_dc_state (dc_state, FALSE);

#if 0
    printf ("RestoreDC called: stack: %p, nr: %d\n",
        dc_state_stack, nr_dc_states);
#endif

    UNLOCK (&__mg_gdilock);
    return TRUE;
}

