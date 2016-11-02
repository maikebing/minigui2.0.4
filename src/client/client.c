/*
** $Id: client.c 7962 2007-10-25 07:28:40Z weiym $
** 
** client.c: routines for client.
** 
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2000 ~ 2002 Wei Yongming.
**
** All rights reserved by Feynman Software.
**
** Current maintainer: Wei Yongming.
**
** NOTE: The idea comes from sample code in APUE.
** Thank Mr. Richard Stevens for his perfect work.
**
** Create date: 2000/12/20
*/

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "common.h"
#include "minigui.h"
#include "gdi.h"
#include "window.h"
#include "gal.h"
#include "cliprect.h"
#include "internals.h"
#include "timer.h"
#include "ourhdr.h"
#include "client.h"
#include "server.h"
#include "sockio.h"
#include "sharedres.h"
#include "drawsemop.h"

int __mg_client_id;

static int conn_fd = -1;         /* connected socket */

ON_LOCK_CLIENT_REQ  OnLockClientReq = NULL;
ON_TRYLOCK_CLIENT_REQ  OnTrylockClientReq = NULL;
ON_UNLOCK_CLIENT_REQ  OnUnlockClientReq = NULL;

#define TIMEOUT_START_REPEAT    20
#define TIMEOUT_REPEAT          10
static struct timeval my_timeout;

void __mg_set_select_timeout (unsigned int usec)
{
    my_timeout.tv_sec = 0;

    if (usec > 0) {
        if (usec > USEC_10MS * TIMEOUT_START_REPEAT)
            my_timeout.tv_usec = USEC_10MS * TIMEOUT_REPEAT;
        else if (usec > USEC_10MS * TIMEOUT_REPEAT)
            my_timeout.tv_usec = USEC_10MS * TIMEOUT_REPEAT >> 1;
        else
            my_timeout.tv_usec = USEC_10MS;
    }
    else {
        my_timeout.tv_usec = USEC_10MS * TIMEOUT_START_REPEAT;
    }
}

BOOL ClientStartup (void)
{
    struct sigaction siga;

    /* connect to server */
    if ( (conn_fd = cli_conn (CS_PATH, 'a')) < 0)
        return FALSE;

    FD_ZERO (&mg_rfdset);
    FD_SET (conn_fd, &mg_rfdset);
    mg_maxfd = conn_fd;

    __mg_set_select_timeout (0);

    /* ignore the SIGPIPE signal */
    siga.sa_handler = SIG_IGN;
    siga.sa_flags = 0;
    memset (&siga.sa_mask, 0, sizeof (sigset_t));
    sigaction (SIGPIPE, &siga, NULL);

    return TRUE;
}

void ClientCleanup (void)
{
    close (conn_fd);
    conn_fd = -1;
}

int GUIAPI GetSockFD2Server (void)
{
    if (mgIsServer)
        return -1;

    return conn_fd;
}

int GUIAPI ClientRequest (PREQUEST request, void* result, int len_rslt)
{
    int n;
    MSG msg;

    if (mgIsServer)
        return -1;

    if (__mg_client_id == 0 && request->id != REQID_JOINLAYER) {
        fprintf (stderr, "MiniGUI Client: please call JoinLayer first.\n");
        exit (255);
        return -1;
    }

    if (OnLockClientReq && OnUnlockClientReq)
        OnLockClientReq();

    if ((n = sock_write (conn_fd, &request->id, sizeof (int))) == SOCKERR_IO) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        return -1;
    }
    else if (n == SOCKERR_CLOSED) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        exit (-1);
    }

    if ((n = sock_write (conn_fd, 
                &request->len_data, sizeof (size_t))) == SOCKERR_IO) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        return -1;
    }
    else if (n == SOCKERR_CLOSED) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        exit (-1);
    }

    if ((n = sock_write (conn_fd, 
                request->data, request->len_data)) == SOCKERR_IO) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        return -1;
    }
    else if (n == SOCKERR_CLOSED) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        exit (-1);
    }

    if (result == NULL || len_rslt == 0) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        return 0;
    }

    do {
        if ((n = sock_read (conn_fd, &msg, sizeof (MSG))) == SOCKERR_IO) {
            if (OnLockClientReq && OnUnlockClientReq)
                OnUnlockClientReq();
            return -1;
        }
        else if (n == SOCKERR_CLOSED) {
            if (OnLockClientReq && OnUnlockClientReq)
                OnUnlockClientReq();
            exit (-1);
        }

        if (msg.hwnd == HWND_INVALID)
            break;
        else if (msg.message == MSG_UPDATECLIWIN && 
                        __mg_client_check_hwnd (msg.hwnd, __mg_client_id)) {
            __mg_update_window (msg.hwnd, 
                                LOSWORD(msg.wParam), HISWORD(msg.wParam),
                                LOSWORD(msg.lParam), HISWORD(msg.lParam));
        }
        else {
            if (msg.hwnd == 0) {
                msg.hwnd = HWND_DESKTOP;
                QueueDeskMessage (&msg);
            }
            else if (__mg_client_check_hwnd (msg.hwnd, __mg_client_id))
                QueueDeskMessage (&msg);
        }
    } while (TRUE);

    if ((n = sock_read (conn_fd, result, len_rslt)) == SOCKERR_IO) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        return -1;
    }
    else if (n == SOCKERR_CLOSED) {
        if (OnLockClientReq && OnUnlockClientReq)
            OnUnlockClientReq();
        exit (-1);
    }

    if (OnLockClientReq && OnUnlockClientReq)
        OnUnlockClientReq();

    return 0;
}

static void check_live (void)
{
    REQUEST req;

    if (__mg_timer_counter != SHAREDRES_TIMER_COUNTER) {
        SetDesktopTimerFlag ();
        __mg_timer_counter = SHAREDRES_TIMER_COUNTER;
    }

    /* Tell server that I am live */
    req.id = REQID_IAMLIVE;
    req.data = &__mg_timer_counter;
    req.len_data = sizeof (unsigned int);
    ClientRequest (&req, NULL, 0);
}

BOOL IdleHandler4Client (PMSGQUEUE msg_que)
{
    static unsigned int old_timer;
    static int repeat_timeout = TIMEOUT_START_REPEAT;
    fd_set rset, wset, eset;
    fd_set* wsetptr = NULL;
    fd_set* esetptr = NULL;
    int i, n, nread;
    struct timeval sel_timeout;
    MSG Msg;

    check_live ();

    rset = mg_rfdset;        /* rset gets modified each time around */
    if (mg_wfdset) {
        wset = *mg_wfdset;
        wsetptr = &wset;
    }
    if (mg_efdset) {
        eset = *mg_efdset;
        esetptr = &eset;
    }

    if (msg_que)
        sel_timeout = my_timeout;
    else {  /* check fd only: for HavePendingMessage function */
        sel_timeout.tv_sec = 0;
        sel_timeout.tv_usec = 0;
    }

    if ((n = select (mg_maxfd + 1, 
            &rset, wsetptr, esetptr, &sel_timeout)) < 0) {
        if (errno == EINTR) {
            /* it is time to check message again. */
            return FALSE;
        }
        err_sys ("client: select error");
    }
    else if (n == 0) {
        check_live ();
        if (msg_que 
                && (__mg_timer_counter - old_timer) >= repeat_timeout) {
            Msg.hwnd = HWND_DESKTOP;
            Msg.message = MSG_TIMEOUT;
            Msg.wParam = (WPARAM)__mg_timer_counter;
            Msg.lParam = 0;
            Msg.time = __mg_timer_counter;
            QueueMessage (msg_que, &Msg);

            old_timer = __mg_timer_counter;
            repeat_timeout = TIMEOUT_REPEAT;
        }
        return FALSE;
    }
    /* check fd only: for HavePendingMessage function. */
    else if (msg_que == NULL) 
        return TRUE;

    old_timer = __mg_timer_counter;
    repeat_timeout = TIMEOUT_START_REPEAT;

    if (FD_ISSET (conn_fd, &rset) && 
        (!OnTrylockClientReq || !OnUnlockClientReq
        || (OnTrylockClientReq && OnUnlockClientReq && !OnTrylockClientReq()))) {

        if ( (nread = sock_read (conn_fd, &Msg, sizeof (MSG))) < 0) {
            if (OnTrylockClientReq && OnUnlockClientReq)
                OnUnlockClientReq();
            err_sys ("client: read error on fd %d", conn_fd);
        }
        else if (nread == 0) {
            if (OnTrylockClientReq && OnUnlockClientReq)
                OnUnlockClientReq();
            err_sys ("client: server closed");
            close (conn_fd);
        }
        else {           /* process event from server */
            if (OnTrylockClientReq && OnUnlockClientReq)
                OnUnlockClientReq();
            if (Msg.message == MSG_UPDATECLIWIN && 
                        __mg_client_check_hwnd (Msg.hwnd, __mg_client_id)) {
                __mg_update_window (Msg.hwnd, 
                                LOSWORD(Msg.wParam), HISWORD(Msg.wParam),
                                LOSWORD(Msg.lParam), HISWORD(Msg.lParam));
            }
            else {
                if (Msg.hwnd == 0) {
                    Msg.hwnd = HWND_DESKTOP;
                    QueueMessage (msg_que, &Msg);
                }
                else if (__mg_client_check_hwnd (Msg.hwnd, __mg_client_id))
                    QueueMessage (msg_que, &Msg);
            }
        }
    }

    /* go through registered listen fds */
    for (i = 0; i < MAX_NR_LISTEN_FD; i++) {
        MSG Msg;

        Msg.message = MSG_FDEVENT;

        if (mg_listen_fds [i].fd) {
            fd_set* temp = NULL;
            int type = mg_listen_fds [i].type;

            switch (type) {
            case POLLIN:
                temp = &rset;
                break;
            case POLLOUT:
                temp = wsetptr;
                break;
            case POLLERR:
                temp = esetptr;
                break;
            }

            if (temp && FD_ISSET (mg_listen_fds [i].fd, temp)) {
                Msg.hwnd = (HWND)mg_listen_fds [i].hwnd;
                Msg.wParam = MAKELONG (mg_listen_fds [i].fd, type);
                Msg.lParam = (LPARAM)mg_listen_fds [i].context;
                QueueMessage (msg_que, &Msg);
            }
        }
    }

    check_live ();
    return TRUE;
}

GHANDLE GUIAPI JoinLayer (const char* layer_name, const char* client_name,
               int max_nr_topmosts, int max_nr_normals)
{
    GHANDLE layer_handle = INV_LAYER_HANDLE;
    REQUEST req;
    JOINLAYERINFO info;
    JOINEDCLIENTINFO joined_info;

    if (mgIsServer)
        return layer_handle;

    if (__mg_client_id != 0) /* Already joined, return the handle */
        return GetLayerInfo (NAME_SELF_LAYER, NULL, NULL, NULL);

    if (layer_name) {
        strncpy (info.layer_name, layer_name, LEN_LAYER_NAME);
        info.layer_name [LEN_LAYER_NAME] = '\0';
    }
    else {
        /* treat as the topmost layer */
        info.layer_name [0] = '\0';
    }

    if (client_name) {
        strncpy (info.client_name, client_name, LEN_CLIENT_NAME);
        info.client_name [LEN_CLIENT_NAME] = '\0';
    }
    else
        info.client_name [0] = '\0';

    if (max_nr_topmosts <= 0) max_nr_topmosts = DEF_NR_TOPMOSTS;
    if (max_nr_normals <= 0) max_nr_normals = DEF_NR_NORMALS;

    info.max_nr_topmosts = (max_nr_topmosts + 7) & ~0x07;
    info.max_nr_normals = (max_nr_normals + 7) & ~0x07;

    req.id = REQID_JOINLAYER;
    req.data = &info;
    req.len_data = sizeof (JOINLAYERINFO);

    if (ClientRequest (&req, &joined_info, sizeof (JOINEDCLIENTINFO)) < 0)
        goto ret;

    __mg_client_id = joined_info.cli_id;

    layer_handle = joined_info.layer;

    if (layer_handle != INV_LAYER_HANDLE) {
        ZORDERINFO* zi;

        __mg_layer = layer_handle;

        zi  = (ZORDERINFO*) shmat (joined_info.zo_shmid, 0, SHM_RDONLY);

        if (zi == (void*)-1) {
            return INV_LAYER_HANDLE;
        }

        __mg_zorder_info = zi;
    }

    __mg_start_client_desktop ();

ret:
    return layer_handle;
}

GHANDLE GUIAPI GetLayerInfo (const char* layer_name, 
                int* nr_clients, BOOL* is_topmost, int* cli_active)
{
    if (mgIsServer)
        return INV_LAYER_HANDLE;

    if (layer_name) {
        LAYERINFO info;
        REQUEST req;

        req.id = REQID_LAYERINFO;
        req.data = layer_name;
        req.len_data = strlen (layer_name) + 1;

        if (ClientRequest (&req, &info, sizeof (LAYERINFO)) < 0)
            goto ret;

        if (info.handle) {
            if (nr_clients) *nr_clients = info.nr_clients;
            if (is_topmost) *is_topmost = info.is_topmost;
            if (cli_active) *cli_active = info.cli_active;
        }

        return info.handle;
    }

ret:
    return INV_LAYER_HANDLE;
}

BOOL GUIAPI SetTopmostLayer (BOOL handle_name, 
                GHANDLE handle, const char* name)
{
    BOOL ret = FALSE;
    LAYEROPINFO info;
    REQUEST req;

    if (mgIsServer)
        return ret;

    if (!handle_name && name == NULL)
        return ret;

    if (handle_name && handle == INV_LAYER_HANDLE)
        return ret;

    info.id_op = ID_LAYEROP_SETTOP;
    info.handle_name = handle_name;

    if (handle_name) {
        info.layer.handle = handle;
    }
    else {
        strncpy (info.layer.name, name, LEN_LAYER_NAME);
        info.layer.name [LEN_LAYER_NAME] = '\0';
    }

    req.id = REQID_LAYEROP;
    req.data = &info;
    req.len_data = sizeof (LAYEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (BOOL)) < 0)
        ret = FALSE;

    return ret;
}

BOOL GUIAPI DeleteLayer (BOOL handle_name, 
                GHANDLE handle, const char* name)
{
    BOOL ret = FALSE;
    LAYEROPINFO info;
    REQUEST req;

    if (mgIsServer)
        return ret;

    if (!handle_name && name == NULL)
        return ret;

    if (handle_name && handle == INV_LAYER_HANDLE)
        return ret;

    info.id_op = ID_LAYEROP_DELETE;
    info.handle_name = handle_name;

    if (handle_name) {
        info.layer.handle = handle;
    }
    else {
        strncpy (info.layer.name, name, LEN_LAYER_NAME);
        info.layer.name [LEN_LAYER_NAME] = '\0';
    }

    req.id = REQID_LAYEROP;
    req.data = &info;
    req.len_data = sizeof (LAYEROPINFO);

    if (ClientRequest (&req, &ret, sizeof (BOOL)) < 0)
        ret = FALSE;

    return ret;
}

