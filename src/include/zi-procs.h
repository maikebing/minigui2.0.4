/*
** $$
**
** zi-procs.h: this head file declares prototypes for MiniGUI-Processes.
**
** Copyright (C) 2006 ~ 2007 Feynman Software.
**
** Derived from internals.h: 2006/06/11
*/

#ifndef GUI_ZIPROCS_H
    #define GUI_ZIPROCS_H

#define ZOF_REFERENCE       0x00000001
#define ZOF_VISIBLE         0x00000002
#define ZOF_DISABLED        0x00000004
#define ZOF_FLAG_MASK       0x0000000F

#define ZOF_TYPE_DESKTOP    0x50000000
#define ZOF_TYPE_MENU       0x40000000
#define ZOF_TYPE_GLOBAL     0x30000000
#define ZOF_TYPE_TOPMOST    0x20000000
#define ZOF_TYPE_NORMAL     0x10000000
#define ZOF_TYPE_MASK       0xF0000000

#define ZOF_TF_MAINWIN      0x01000000
#define ZOF_TF_TOOLWIN      0x02000000
#define ZOF_TYPE_FLAG_MASK  0xFF000000

#ifndef MAX_CAPTION_LEN
 #define MAX_CAPTION_LEN 40
#endif 

typedef struct _ZORDERNODE
{
    DWORD           flags;

    char            caption[MAX_CAPTION_LEN+1];

    RECT            rc;
    RECT            dirty_rc;
    unsigned int    age;

    int             cli;    /* which client? */
    HWND            hwnd;   /* which window of the client? */
    HWND            main_win;   /* handle to the main window */

    int             next;
    int             prev;
} ZORDERNODE;
typedef ZORDERNODE* PZORDERNODE;

typedef struct _ZORDERINFO
{
    int             size_usage_bmp;

    int             max_nr_popupmenus;
    int             max_nr_globals;
    int             max_nr_topmosts;
    int             max_nr_normals;

    int             nr_popupmenus;
    int             nr_globals;
    int             nr_topmosts;
    int             nr_normals;

    int             first_global;
    int             first_topmost;
    int             first_normal;

    int             active_win;

    int             cli_trackmenu;
    HWND            ptmi_in_cli;

    int             zi_semid;
    int             zi_semnum;

    /* ZORDERNODE* zo_nodes; */
} ZORDERINFO;
typedef ZORDERINFO* PZORDERINFO;

#endif // GUI_ZIPROCS_H

