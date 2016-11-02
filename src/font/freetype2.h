/*
** $Id: freetype2.h 8100 2007-11-16 08:45:38Z xwyan $
**
** freetype2.h: TrueType font support based on FreeType 2.
**
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2000 ~ 2002 Wei Yongming
**
** All right reserved by Feynman Software.
**
** Current maintainer: Yan Xiaowei.
**
** Created by WEI Yongming, 2000/8/21
*/

#ifndef GUI_FREETYP2_H
    #define GUI_FREETYP2_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <ft2build.h>
#include FT_FREETYPE_H /*installation-specific macros*/
#include FT_GLYPH_H
#include FT_TRUETYPE_IDS_H
#include FT_SIZES_H
#include FT_LCD_FILTER_H
#include FT_CACHE_H
#include FT_CACHE_MANAGER_H
#define _FT2_CACHE_SUPPORT 1

typedef struct tagFTFACEINFO {
#ifdef _FT2_CACHE_SUPPORT
    /*should be MAX_PATH*/
    char        filepathname[256];
    int         face_index;
    int         cmap_index;
#else
    FT_Face     face;
#endif
    BOOL        valid;
} FTFACEINFO, *PFTFACEINFO;

typedef struct tagFTINSTANCEINFO {
    PFTFACEINFO ft_face_info;
#ifdef _FT2_CACHE_SUPPORT
    FTC_ImageTypeRec  image_type;
    FT_Int      glyph_done;
#else
    FT_Size     size;
    FT_Matrix   matrix;
#endif
    FT_Glyph    glyph;
    int         rotation;

    int         max_width;
    int         ave_width;
    int         height;
    int         ascent;
    int         descent;

    FT_Bool     use_kerning; /*FT_HAS_KERNING(face)*/
    FT_Vector   delta;
    int         is_index_old;
    FT_UInt     cur_index;
    FT_UInt     prev_index;

    char        cur_mchar[16];
    FT_LcdFilter    ft_lcdfilter;
} FTINSTANCEINFO, *PFTINSTANCEINFO;

#define FT_FACE_INFO_P(devfont) ((FTFACEINFO*)(devfont->data))
#define FT_INST_INFO_P(devfont) ((FTINSTANCEINFO*)(devfont->data))

extern FONTOPS freetype_font_ops;

const DEVFONT* __mg_ft2load_devfont_fromfile(
		                  const char* devfont_name, const char* file_name);
void __mg_ft2_destroy_devfont_fromfile (DEVFONT **devfont);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // GUI_FREETYP2_H

