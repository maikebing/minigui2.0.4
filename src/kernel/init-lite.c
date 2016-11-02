/*
** $Id: init-lite.c 7962 2007-10-25 07:28:40Z weiym $
**
** init-lite.c: The Initialization/Termination routines for 
** MiniGUI-Processes and MiniGUI-Standalone.
**
** Copyright (C) 2003 ~ 2007, Feynman Software.
** Copyright (C) 1999 ~ 2002, Wei Yongming.
**
** All rights reserved by Feynman Software.
**
** Current maintainer: Wei Yongming.
**
** Create date: 2000/11/05
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifdef _LITE_VERSION

#include <unistd.h>
#include <signal.h>
#include <sys/termios.h>

#include "minigui.h"
#include "gdi.h"
#include "window.h"
#include "cliprect.h"
#include "gal.h"
#include "internals.h"
#include "ctrlclass.h"
#include "cursor.h"
#include "event.h"
#include "misc.h"
#include "menu.h"
#include "timer.h"
#include "accelkey.h"


#ifndef _STAND_ALONE
  #include "sharedres.h"
  #include "client.h"
  #include "server.h"

  void* mgSharedRes;
  size_t mgSizeRes;

#endif

BOOL GUIAPI ReinitDesktopEx (BOOL init_sys_text)
{
    return SendMessage (HWND_DESKTOP, MSG_REINITSESSION, init_sys_text, 0) == 0;
}

static struct termios savedtermio;

void* GUIAPI GetOriginalTermIO (void)
{
#ifdef _STAND_ALONE
    return &savedtermio;
#else
    return &SHAREDRES_TERMIOS;
#endif
}

static BOOL InitResource (void)
{
#ifdef _CURSOR_SUPPORT
    if (!InitCursor ()) {
        fprintf (stderr, "InitGUI: Can not initialize mouse cursor!\n");
        goto failure;
    }
#endif

    if (!InitMenu ()) {
        fprintf (stderr, "InitGUI: Init Menu module failure!\n");
        goto failure;
    }

    if (!InitControlClass ()) {
        fprintf(stderr, "InitGUI: Init Control Class failure!\n");
        goto failure;
    }

    if (!InitAccel ()) {
        fprintf(stderr, "InitGUI: Init Accelerator failure!\n");
        goto failure;
    }

    if (!InitDesktop ()) {
        fprintf (stderr, "InitGUI: Init Desktop error!\n");
        goto failure;
    }

    if (!InitFreeQMSGList ()) {
        fprintf (stderr, "InitGUI: Init free QMSG list error!\n");
        goto failure;
    }

    if (!InitDskMsgQueue ()) {
        fprintf (stderr, "InitGUI: Init MSG queue error!\n");
        goto failure;
    }

    return TRUE;

failure:
    return FALSE;
}

#define err_message(step, message) fprintf (stderr, "InitGUI (step %d): %s\n", step, message)

#ifdef _STAND_ALONE

static void segvsig_handler (int v)
{
    TerminateLWEvent ();
    TerminateGAL ();

    if (v == SIGSEGV)
        kill (getpid(), SIGABRT); /* cause core dump */
    else
        _exit (v);
}

#else

static void segvsig_handler (int v)
{
    TerminateLWEvent ();
    TerminateGAL ();
    __mg_delete_sharedres_sem ();
    __mg_delete_zi_sem ();
    __mg_cleanup_layers ();

    unlink (LOCKFILE);
    unlink (CS_PATH);

    if (v == SIGSEGV)
        kill (getpid(), SIGABRT); /* cause core dump */
    else
        _exit (v);
}

#endif

static BOOL InstallSEGVHandler (void)
{
    struct sigaction siga;
    
    siga.sa_handler = segvsig_handler;
    siga.sa_flags = 0;
    
    memset (&siga.sa_mask, 0, sizeof (sigset_t));
    sigaction (SIGSEGV, &siga, NULL);
    sigaction (SIGTERM, &siga, NULL);
    sigaction (SIGINT, &siga, NULL);

    /* ignore the SIGPIPE signal */
    siga.sa_handler = SIG_IGN;
    sigaction (SIGPIPE, &siga, NULL);

    return TRUE;
}

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

int InitGUI (int argc, const char* agr[])
{
    int step = 1;
#if !defined(_STAND_ALONE)
    const char* name;

    if ((name = strrchr (agr [0], '/')) == NULL)
        name = agr [0];
    else
        name ++;

    if (!strcmp (name, "mginit"))
        mgIsServer = TRUE;
#endif

#ifdef HAVE_SETLOCALE
    setlocale (LC_ALL, "C");
#endif

    /* Save original termio */
#ifndef _STAND_ALONE
    if (mgIsServer)
#endif
        tcgetattr (0, &savedtermio);

    /*Initialize default window process*/
    __mg_def_proc[0] = PreDefMainWinProc;
    __mg_def_proc[1] = PreDefDialogProc;
    __mg_def_proc[2] = PreDefControlProc;

    if (!InitFixStr ()) {
        err_message (step, "Can not initialize Fixed String heap!\n");
        return step;
    }
    step++;

    if (!InitMisc ()) {
        err_message (step, "Can not initialize miscellous things!");
        return step;
    }
    step++;

#ifndef _STAND_ALONE
    if (mgIsServer && !IsOnlyMe ()) {
        err_message (step, "There is already an instance of 'mginit'!");
        return step;
    }
    step++;
#endif

    /* Init GAL engine. */
    switch (InitGAL ()) {
    case ERR_CONFIG_FILE:
        err_message (step, "Reading configuration failure!");
        return step;

    case ERR_NO_ENGINE:
        err_message (step, "No graphics engine defined!");
        return step;

    case ERR_NO_MATCH:
        err_message (step, "Can not get graphics engine information!");
        return step;

    case ERR_GFX_ENGINE:
        err_message (step, "Can not initialize graphics engine!");
        return step;
    }
    step++;
    atexit (TerminateGAL);

    /* install signal handlers */
#ifndef _STAND_ALONE
    if (mgIsServer)
#endif
        InstallSEGVHandler ();

    if (!InitGDI ()) {
        err_message (step, "Initialization of GDI resource failure!\n");
        return step;
    }
    step++;
    atexit (TerminateGDI);

    /* Init Master Screen DC here */
    if (!InitScreenDC (__gal_screen)) {
        err_message (step, "Can not initialize screen DC!");
        return step;
    }
    step++;
    atexit (TerminateScreenDC);

    g_rcScr.left = 0;
    g_rcScr.top = 0;
    g_rcScr.right = GetGDCapability (HDC_SCREEN, GDCAP_MAXX) + 1;
    g_rcScr.bottom = GetGDCapability (HDC_SCREEN, GDCAP_MAXY) + 1;

    if (!InitWindowElementColors ()) {
        err_message (step, "Can not initialize colors of window element!");
        return step;
    }
    step++;

#ifndef _STAND_ALONE
    if (mgIsServer) {
        /* Load shared resource and create shared memory. */
        if ((mgSharedRes = LoadSharedResource ()) == NULL) {
            err_message (step, "Can not load shared resource!");
            return step;
        }
        atexit (UnloadSharedResource);
        SHAREDRES_TERMIOS = savedtermio;
    }
    else {
        if ((mgSharedRes = AttachSharedResource ()) == NULL) {
            err_message (step, "Can not attach shared resource, please run 'mginit' first!");
            return step;
        }
        atexit (UnattachSharedResource);
    }
    step++;

#endif

    /* Initialize resource */
    if (!InitResource ()) {
        err_message (step, "Can not initialize resource!");
        return step;
    }
    step++;

#ifdef _STAND_ALONE

    /* Init IAL engine.. */
    if (!InitLWEvent ()) {
        err_message (step, "Can not initialize low level event!");
        return step;
    }
    step++;

    atexit (TerminateLWEvent);

    SetDskIdleHandler (IdleHandler4StandAlone);

    if (!StandAloneStartup ()) {
        fprintf (stderr, "Can not start MiniGUI-StandAlone version.\n");
        return step;
    }

#else

    if (mgIsServer) {
        /* Init IAL engine.. */
        if (!InitLWEvent ()) {
            err_message (step, "Can not initialize low level event!");
            return step;
        }
        step++;

        atexit (TerminateLWEvent);

        SetDskIdleHandler (IdleHandler4Server);
    }
    else {
        if (!ClientStartup ()) {
            err_message (step, "Can not start client!");
            return step;
        }
        step++;

        SetDskIdleHandler (IdleHandler4Client);
    }
#endif

    SetKeyboardLayout ("default");

    TerminateMgEtc ();
    return 0;
}

void TerminateGUI (int rcByGUI)
{
    DestroyDskMsgQueue ();
    DestroyFreeQMSGList ();
    TerminateDesktop ();
    TerminateAccel ();
    TerminateControlClass ();
    TerminateMenu ();
#ifdef _CURSOR_SUPPORT
    TerminateCursor ();
#endif
    TerminateMisc ();
    TerminateFixStr ();

#ifdef _STAND_ALONE
    SendMessage (HWND_DESKTOP, MSG_ENDSESSION, 0, 0);

    StandAloneCleanup ();
#else
    if (mgIsServer) {
        SendMessage (HWND_DESKTOP, MSG_ENDSESSION, 0, 0);

        /* Cleanup UNIX domain socket and other IPC objects. */
        ServerCleanup ();
    }
    else {
        ClientCleanup ();
    }
#endif
}

void GUIAPI ExitGUISafely (int exitcode)
{
    TerminateGUI ((exitcode > 0) ? -exitcode : exitcode);
#ifndef __NOUNIX__
    exit (2);
#endif
}

#endif /* _LITE_VERSION */

