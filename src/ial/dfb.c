/*
** $Id: dfb.c 7335 2007-08-16 03:38:27Z xgwang $
**
** dfb.c: Low Level Input Engine for DirectFB.
** 
** Copyright (C) 2005 ~ 2007, Feynman Software.
**
** All rights reserved by Feynman Software.
**
** Created by Xiaogang Du, 2005/12/15
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifdef _DFB_IAL

#include <unistd.h>
#include <fcntl.h>

#include <directfb.h>
#include <directfb_keyboard.h>

#include "ial.h"
#include "dfb.h"

extern IDirectFB *__mg_dfb;

static IDirectFBEventBuffer *evtbuf;
static unsigned char state[NR_KEYS];
static int curkey;
static int dev_fd;

static int mouse_update(void)
{
	return 0;
}

static void mouse_getxy (int* x, int* y)
{
	*x = 0;
    *y = 0;
}

static int mouse_getbutton(void)
{
	return 0;
}

static struct _dfb2mgkey keymap[] = {
    /* num key */
    {DIKI_0,                SCANCODE_0},
    {DIKI_1,                SCANCODE_1},
    {DIKI_2,                SCANCODE_2},
    {DIKI_3,                SCANCODE_3},
    {DIKI_4,                SCANCODE_4},
    {DIKI_5,                SCANCODE_5},
    {DIKI_6,                SCANCODE_6},
    {DIKI_7,                SCANCODE_7},
    {DIKI_8,                SCANCODE_8},
    {DIKI_9,                SCANCODE_9},
    /* cursor key */
    {DIKI_LEFT,             SCANCODE_CURSORBLOCKLEFT},
    {DIKI_UP,               SCANCODE_CURSORBLOCKUP},
    {DIKI_RIGHT,            SCANCODE_CURSORBLOCKRIGHT},
    {DIKI_DOWN,             SCANCODE_CURSORBLOCKDOWN},
    /* control key */
    {DIKS_OK,               SCANCODE_ENTER},
    {DIKS_RED,              SCANCODE_ESCAPE},
    {DIKS_GREEN,            SCANCODE_TAB}
};
static int keymaplen = sizeof(keymap)/sizeof(struct _dfb2mgkey);

static int findkey(int dfbkey)
{
    int i;

    for (i = 0; i < keymaplen; i++) {
        if (dfbkey == keymap[i].dfbkey) {
            return keymap[i].mgkey;
        }
    }

    return -1;
}

static int keyboard_update (void)
{
    int pckey;

    if (curkey == 0) {
        memset (state, 0, sizeof(state));
    } else {
        pckey = findkey (curkey);
        if (pckey >= 0) {
            state[pckey] = 1;
        } else {
            curkey = 0;
            return 0;
        }
    }

    return NR_KEYS;
}

static const char *keyboard_get_state(void)
{
    return (char *)state;
}

#ifdef  _LITE_VERSION
static int wait_event (int which, int maxfd, fd_set *in, fd_set *out, fd_set *except,
                struct timeval *timeout)
#else
static int wait_event (int which, fd_set *in, fd_set *out, fd_set *except,
                struct timeval *timeout)
#endif
{
    fd_set    rfds;
    int    retvalue = 0;
    int    fd, e;
    DFBEvent evt;

    if (!in) {
        in = &rfds;
        FD_ZERO (in);
    }

    if (which & IAL_KEYEVENT) {
        fd = dev_fd;          /* FIXME: mouse fd may be changed in vt switch! */
        FD_SET (fd, in);
#ifdef _LITE_VERSION
        if (fd > maxfd) maxfd = fd;
#endif
    }

#ifdef _LITE_VERSION
    e = select (maxfd + 1, in, out, except, timeout) ;
#else
    e = select (dev_fd + 1, in, out, except, timeout) ;
#endif

    if (e > 0) {
        if (FD_ISSET (dev_fd, in)) {
            read(dev_fd,&evt,sizeof(DFBEvent));
            FD_CLR (dev_fd, in);
            switch (evt.input.type) {
            case DIET_KEYPRESS:
                curkey = evt.input.key_symbol;
                retvalue |= IAL_KEYEVENT;
                break;

            case DIET_KEYRELEASE:
                curkey = 0;
                retvalue |= IAL_KEYEVENT;
                break;

            }
        }
    } else if (e < 0) {
        return -1;
    }
    return retvalue;
}

BOOL InitDFBInput (INPUT* input, const char* mdev, const char* mtype)
{
    DFBResult ret;
    
    ret = __mg_dfb->CreateInputEventBuffer(__mg_dfb, 
            DICAPS_KEYS|DICAPS_BUTTONS|DICAPS_AXES,
            DFB_TRUE,
            &evtbuf);

    if (ret) {
        fprintf(stderr, "CreateInputEventBuffer error\n");
        return FALSE;
    }
    
    evtbuf->CreateFileDescriptor (evtbuf, &dev_fd);

    if (dev_fd < 0) {
        fprintf(stderr, "CreateFileDescriptor error\n");
        return FALSE;
    }

    memset (state, 0, sizeof (state));

    input->update_mouse = mouse_update;
    input->get_mouse_xy = mouse_getxy;
    input->set_mouse_xy = NULL;
    input->get_mouse_button = mouse_getbutton;
    input->set_mouse_range = NULL;

    input->update_keyboard = keyboard_update;
    input->get_keyboard_state = keyboard_get_state;
    input->set_leds = NULL;

    input->wait_event = wait_event;

    return TRUE;
}

void TermDFBInput (void)
{
}

#endif /* _DFB_IAL */
