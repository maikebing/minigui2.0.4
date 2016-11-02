/*
** $Id: sharedres.h 7337 2007-08-16 03:44:29Z xgwang $
**
** sharedres.h: structure of shared resource.
**
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2000 ~ 2002 Wei Yongming.
**
** Create date: 2000/12/xx
*/

#ifndef GUI_SHAREDRES_H
    #define GUI_SHAREDRES_H

#include <sys/time.h>
#include <sys/termios.h>

#define MAX_SRV_CLIP_RECTS      8

#define _NR_SEM         2
#define _IDX_SEM_DRAW   0
#define _IDX_SEM_SCR    1

#ifdef _CURSOR_SUPPORT

#undef _NR_SEM
#define _NR_SEM             4
#define _IDX_SEM_CURSOR     2
#define _IDX_SEM_HIDECOUNT  3

#endif /* _IDX_SEM_CURSOR */

typedef struct tagG_RES {
    int semid;
    int shmid;

    int nr_layers;
    int semid_layer;

    int nr_globals;
    int def_nr_topmosts;
    int dev_nr_normals;

    GHANDLE topmost_layer;

    unsigned int timer_counter;
    unsigned int tick_on_locksem;
    struct timeval timeout;
    struct termios savedtermio;
    int mousex, mousey;
    int mousebutton;
    int shiftstatus;
    
#ifdef _CURSOR_SUPPORT
    int cursorx, cursory;
    int oldboxleft, oldboxtop;
    HCURSOR csr_current;
    int xhotspot, yhotspot;
	int csr_show_count;
#endif

#ifdef _CURSOR_SUPPORT
	int csrnum;
#endif
	int iconnum;
	int bmpnum;
	int sysfontnum;
	int rbffontnum;
	int varfontnum;

#ifdef _CURSOR_SUPPORT
	unsigned long svdbitsoffset;
	unsigned long csroffset;
#endif
	unsigned long iconoffset;
	unsigned long sfontoffset;
	unsigned long rfontoffset;
	unsigned long vfontoffset;
	unsigned long bmpoffset;

} G_RES;
typedef G_RES* PG_RES;

#define SHAREDRES_TIMER_COUNTER (((PG_RES)mgSharedRes)->timer_counter)
#define SHAREDRES_TICK_ON_LOCKSEM  (((PG_RES)mgSharedRes)->tick_on_locksem)
#define SHAREDRES_TIMEOUT       (((PG_RES)mgSharedRes)->timeout)
#define SHAREDRES_TERMIOS       (((PG_RES)mgSharedRes)->savedtermio)
#define SHAREDRES_MOUSEX        (((PG_RES)mgSharedRes)->mousex)
#define SHAREDRES_MOUSEY        (((PG_RES)mgSharedRes)->mousey)
#define SHAREDRES_BUTTON        (((PG_RES)mgSharedRes)->mousebutton)
#define SHAREDRES_SHIFTSTATUS   (((PG_RES)mgSharedRes)->shiftstatus)
#define SHAREDRES_SEMID         (((PG_RES)mgSharedRes)->semid)
#define SHAREDRES_SHMID         (((PG_RES)mgSharedRes)->shmid)
#define SHAREDRES_TOPMOST_LAYER (((PG_RES)mgSharedRes)->topmost_layer)

#define SHAREDRES_SEMID         (((PG_RES)mgSharedRes)->semid)
#define SHAREDRES_SHMID         (((PG_RES)mgSharedRes)->shmid)
#define SHAREDRES_TOPMOST_LAYER (((PG_RES)mgSharedRes)->topmost_layer)

#define SHAREDRES_NR_LAYSERS    (((PG_RES)mgSharedRes)->nr_layers)
#define SHAREDRES_SEMID_LAYER   (((PG_RES)mgSharedRes)->semid_layer)

#define SHAREDRES_NR_GLOBALS    (((PG_RES)mgSharedRes)->nr_globals)
#define SHAREDRES_DEF_NR_TOPMOSTS   (((PG_RES)mgSharedRes)->def_nr_topmosts)
#define SHAREDRES_DEF_NR_NORMALS    (((PG_RES)mgSharedRes)->dev_nr_normals)

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


#ifdef __cplusplus

#endif  /* __cplusplus */

#endif // GUI_SHAREDRES_H

