/*
** $Id: hi3560_fb.h 7268 2007-07-03 08:08:43Z xwyan $
**
** hi3560_fb.h: header file. 
**
** Copyright (C) 2007 Feynman Software.
**
** All rights reserved by Feynman Software.
*/

#ifndef __HIFB_H__
#define __HIFB_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#define IOC_TYPE_HIFB       'F'
#define FBIOGET_COLORKEY_HIFB       _IOR(IOC_TYPE_HIFB, 90, fb_colorkey)
#define FBIOPUT_COLORKEY_HIFB       _IOW(IOC_TYPE_HIFB, 91, fb_colorkey)
#define FBIOGET_ALPHA_HIFB          _IOR(IOC_TYPE_HIFB, 92, fb_alpha)
#define FBIOPUT_ALPHA_HIFB          _IOW(IOC_TYPE_HIFB, 93, fb_alpha)
#define FBIOGET_SCREEN_ORIGIN_HIFB  _IOR(IOC_TYPE_HIFB, 94, fb_point)
#define FBIOPUT_SCREEN_ORIGIN_HIFB  _IOW(IOC_TYPE_HIFB, 95, fb_point)
#define FBIOGET_GAMMAP_HIFB          _IOR(IOC_TYPE_HIFB, 96, fb_gammap)
#define FBIOPUT_GAMMAP_HIFB          _IOR(IOC_TYPE_HIFB, 97, fb_gammap)

#define FB_ACCEL_HIFB         0x9E    /*hi 2d accerlerate*/

/*hifb extend*/
typedef struct tagfb_colorkey
{
    unsigned long key;
    unsigned char key_enable;	
    unsigned char mask_enable;	
    unsigned char rmask;
    unsigned char gmask;
    unsigned char bmask;
    char  reserved[3];
}fb_colorkey;

typedef struct tagfb_alpha
{
    unsigned char alpha0;   /* the value of alpha0 register */
    unsigned char alpha1;   /* the value of alpha1 register */
    unsigned char AOE;      /* channel alpha enable flag */
    unsigned char AEN;      /* pixel alpha enable flag */
}fb_alpha;

typedef struct tagfb_point
{
    unsigned short x;       /* horizontal position */
    unsigned short y;       /* vertical position */
}fb_point;


typedef struct tagfb_gammap
{
    unsigned char enable;   /* gamma table enable flag */
    unsigned int start;     /* first entry */
    unsigned int len;       /* table length */
    unsigned char *red;     /* red gamma values */
    unsigned char *green;   /* green gamma values */
    unsigned char *blue;    /* blue gamma values */
}fb_gammap;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* __HIFB_INC_VOU_H__ */

