/*
** $Id: client.h 7337 2007-08-16 03:44:29Z xgwang $
**
** client.h: routines for client.
**
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2000 ~ 2002 Wei Yongming.
**
** Create date: 2000/12/xx
*/

#ifndef GUI_CLIENT_H
    #define GUI_CLIENT_H

#define CL_PATH "/var/tmp/"

#define REQID_LOADCURSOR        0x0001
#define REQID_CREATECURSOR      0x0002
#define REQID_DESTROYCURSOR     0x0003
#define REQID_CLIPCURSOR        0x0004
#define REQID_GETCLIPCURSOR     0x0005
#define REQID_SETCURSOR         0x0006
#define REQID_GETCURRENTCURSOR  0x0007
#define REQID_SHOWCURSOR        0x0008
#define REQID_SETCURSORPOS      0x0009
#define REQID_LAYERINFO         0x000A
#define REQID_JOINLAYER         0x000B
#define REQID_LAYEROP           0x000C
#define REQID_ZORDEROP          0x000D
#define REQID_IAMLIVE           0x000E
#define REQID_OPENIMEWND        0x000F
#define REQID_SETIMESTAT        0x0010
#define REQID_GETIMESTAT        0x0011
#define REQID_REGISTERHOOK      0x0012

#ifdef _USE_NEWGAL
#define REQID_HWSURFACE         0x0013
#endif

#ifdef _CLIPBOARD_SUPPORT
#define REQID_CLIPBOARD         0x0014
#endif

typedef struct JoinLayerInfo
{
    char layer_name [LEN_LAYER_NAME + 1];
    char client_name [LEN_CLIENT_NAME + 1];

    int max_nr_topmosts;
    int max_nr_normals;
} JOINLAYERINFO;

typedef struct JoinedClientInfo
{
    GHANDLE layer;

    int cli_id;

    int zo_shmid;
} JOINEDCLIENTINFO;

typedef struct LayerInfo {
    GHANDLE handle;

    int nr_clients;
    int cli_active;
    BOOL is_topmost;
} LAYERINFO;

#define ID_LAYEROP_DELETE   1
#define ID_LAYEROP_SETTOP   2

typedef struct LayerOpInfo {
    int id_op;
    BOOL handle_name;   /* specify the layer by handle or name */
    union {
        GHANDLE handle;
        char name [LEN_LAYER_NAME + 1];
    } layer;
} LAYEROPINFO;

#define ID_ZOOP_ALLOC       1
#define ID_ZOOP_FREE        2
#define ID_ZOOP_MOVE2TOP    3
#define ID_ZOOP_SHOW        4
#define ID_ZOOP_HIDE        5
#define ID_ZOOP_MOVEWIN     6
#define ID_ZOOP_SETACTIVE   7

#define ID_ZOOP_START_TRACKMENU     9
#define ID_ZOOP_END_TRACKMENU       10
#define ID_ZOOP_CLOSEMENU           11
#define ID_ZOOP_ENABLEWINDOW        12
#define ID_ZOOP_STARTDRAG           13
#define ID_ZOOP_CANCELDRAG          14
#define ID_ZOOP_CHANGECAPTION       15

#ifndef MAX_CAPTION_LEN
 #define MAX_CAPTION_LEN 40
#endif 

typedef struct ZorderOpInfo
{
    int     id_op;

    int     idx_znode;
    DWORD   flags;
    HWND    hwnd;
    HWND    main_win;
    RECT    rc;
    RECT    rcA;
    char    caption[MAX_CAPTION_LEN + 1];
} ZORDEROPINFO;

#define ID_REG_KEY          1
#define ID_REG_MOUSE        2

typedef struct RegHookInfo
{
    int     id_op;
    HWND    hwnd;
    DWORD   flag;
} REGHOOKINFO;

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#ifndef _MGUI_MINIGUI_H

typedef struct tagREQUEST {
    int id;
    const void* data;
    size_t len_data;
} REQUEST;

typedef REQUEST* PREQUEST;

int cli_conn (const char* name, char project);
int ClientRequest (PREQUEST request, void* result, int len_rslt);
#endif

void __mg_set_select_timeout (unsigned int usec);
void __mg_start_client_desktop (void);

void __mg_update_window (HWND hwnd, int left, int top, int right, int bottom);

BOOL __mg_client_check_hwnd (HWND hwnd, int cli);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // GUI_CLIENT_H

