/*
** $Id: freetype2.c 11497 2009-04-09 09:24:19Z weiym $

** freetype2.c: TrueType font support based on FreeType 2.
** 
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2002 Wei Yongming.
**
** All right reserved by Feynman Software.
**
** Current maintainer: Yan Xiaowei.
** 
** Author: WEI Yonming.
**
** Create date: 2002/01/18
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "common.h"
#include "minigui.h"
#include "gdi.h"
#include "devfont.h"
#include "charset.h"
#include "fontname.h"
#include "misc.h"

#ifdef _FT2_SUPPORT

#include "freetype2.h"

static FT_Library       ft_library = NULL;
#ifdef _FT2_CACHE_SUPPORT
static FTC_Manager      ft_cache_manager = NULL;
static FTC_CMapCache    ft_cmap_cache = NULL;
static FTC_ImageCache   ft_image_cache = NULL;

static FT_Error
get_size (FTINSTANCEINFO* ft_inst_info,
               FT_Size*   asize)
{
    FTC_ScalerRec   scaler;
    FT_Size         size;
    FT_Error        error;

    scaler.face_id = (FTC_FaceID)(ft_inst_info->ft_face_info);
    scaler.width   = ft_inst_info->ave_width;
    scaler.height  = ft_inst_info->height;
    scaler.pixel   = 1;

    error = FTC_Manager_LookupSize( ft_cache_manager, &scaler, &size );

    if ( !error )
        *asize = size;

    return error;
}

static FT_Error
my_face_requester (FTC_FaceID  face_id,
                   FT_Library  lib,
                   FT_Pointer  request_data,
                   FT_Face*    aface)
{
    PFTFACEINFO font = (PFTFACEINFO) face_id;

    FT_Error    error = FT_New_Face (lib,
                         font->filepathname,
                         font->face_index,
                         aface);
    if (!error)
    {
        if ((*aface)->charmaps)
            (*aface)->charmap = (*aface)->charmaps[font->cmap_index];
    }

    return error;
}
#endif

/*************** TrueType on FreeType font operations ************************/

/*for DEBUG
static void 
print_bitmap(char* bits, int width, int height, int pitch)
{
    int y = 0;
    int x = 0;
    char* p_line_head;
    char* p_cur_char;
    
    for (y = 0, p_line_head = bits; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            p_cur_char = (x >> 3) + p_line_head;
            if (*p_cur_char & (128 >> (x%8)))
                printf("@ ");
            else
                printf(". ");
        }
        printf("\n");
        
        p_line_head += pitch;
    }
}
*/

/*press double-byte align to byte align*/
static void 
press_bitmap (void* buffer, int width, int rows, int pitch)
{
    void*   src_pos;
    void*   dest_pos;
    int     dest_pitch = (width + 7) >> 3;
    int     i;

    if (dest_pitch == pitch)
        return;

    src_pos = buffer + pitch;
    dest_pos = buffer + dest_pitch;

    for (i = 1; i < rows; i++)
    {
        memmove (dest_pos, src_pos, dest_pitch);
        src_pos += pitch;
        dest_pos += dest_pitch;
    }
}

static int 
get_char_width (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->ave_width;
}

static int 
get_ave_width (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->ave_width;
}

static int 
get_max_width (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->max_width;
}

static int 
get_font_height (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->height;
}

static int 
get_font_size (LOGFONT* logfont, DEVFONT* devfont, int expect)
{
    return expect;
}

static int 
get_font_ascent (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->ascent;
}

static int 
get_font_descent (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return -ft_inst_info->descent;
}

/* call this function before getting the bitmap/pixmap of the char
 * to get the bbox of the char */
static int 
get_char_bbox (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len, 
                int* px, int* py, int* pwidth, int* pheight)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    FTFACEINFO*     ft_face_info = ft_inst_info->ft_face_info;
    FT_BBox         bbox;
    FT_Face         face;
#ifdef _FT2_CACHE_SUPPORT
    FT_Size         size;
#endif
    FT_UInt         uni_char = (*devfont->charset_ops->conv_to_uc32) (mchar);

    bbox.xMin = bbox.yMin = 32000;
    bbox.xMax = bbox.yMax = -32000;

#ifndef _FT2_CACHE_SUPPORT
    face = ft_face_info->face;
    ft_inst_info->cur_index = FT_Get_Char_Index (face, uni_char);
    
    if (FT_Load_Glyph (face, ft_inst_info->cur_index, 
                    FT_LOAD_DEFAULT|FT_LOAD_NO_BITMAP
                    | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH
                    | FT_LOAD_TARGET_LCD))
    {
        fprintf(stderr, "FT_Load_Glyph error\n");
        return 0;
    }

    if (ft_inst_info->glyph) 
    {
        FT_Done_Glyph (ft_inst_info->glyph); 
    }

    if (FT_Get_Glyph (face->glyph, &(ft_inst_info->glyph)))
    {
        fprintf(stderr, "FT_Get_Glyph error\n");
        return 0;
    }
#else
    if (get_size (ft_inst_info, &size))
    {
        fprintf(stderr, "can't access font file %p\n", ft_face_info);
        return 0;
    }

    face = size->face;
    ft_inst_info->cur_index = FTC_CMapCache_Lookup (ft_cmap_cache,
                            (FTC_FaceID) (ft_inst_info->image_type.face_id),
                            ft_inst_info->ft_face_info->cmap_index,
                            uni_char);

    if (ft_inst_info->glyph_done && ft_inst_info->glyph) 
    {
        FT_Done_Glyph (ft_inst_info->glyph); 
        ft_inst_info->glyph_done = 0;
    }

    if (FTC_ImageCache_Lookup (ft_image_cache,
                                &ft_inst_info->image_type,
                                ft_inst_info->cur_index,
                                &ft_inst_info->glyph,
                                NULL))
    {
        fprintf(stderr, "can't access image cache\n");
        return 0;
    }
#endif

    if (ft_inst_info->use_kerning) 
    {
        /*kerning is getted, and prev_index is cur_index*/
        if (ft_inst_info->is_index_old)
        {
            if (px)        
                *px += ft_inst_info->delta.x >> 6;
        }
        /*cur_index is new, kerning is not getted*/
        else
        {
            /*get kerning*/
            if (ft_inst_info->prev_index && ft_inst_info->cur_index) 
            {
                int error = FT_Get_Kerning (face, 
                        ft_inst_info->prev_index, ft_inst_info->cur_index,
                        FT_KERNING_DEFAULT, &(ft_inst_info->delta));

                if (error == 0)
                {
                    if (px)
                        *px += (ft_inst_info->delta.x >> 6);
                }
                else
                {
                    ft_inst_info->delta.x = 0;
                    ft_inst_info->delta.y = 0;
                }
            }

            /*make new index old*/
            ft_inst_info->prev_index = ft_inst_info->cur_index;
            ft_inst_info->is_index_old = 1;
        }
    }
        
    FT_Glyph_Get_CBox (ft_inst_info->glyph, ft_glyph_bbox_pixels, &bbox);

    if (bbox.xMin > bbox.xMax)
        return 0;

    if (ft_inst_info->ft_lcdfilter != FT_LCD_FILTER_NONE 
                    && (logfont->style & FS_WEIGHT_SUBPIXEL))
        bbox.xMax += 2;

    if (pwidth) *pwidth = bbox.xMax - bbox.xMin;

    if (pheight) *pheight = bbox.yMax - bbox.yMin;

    if (px) *px += bbox.xMin; 
    if (py) *py -= bbox.yMax;

    return (int)(bbox.xMax - bbox.xMin);
}

/* call this function to get the bitmap/pixmap of the char */ 
static const void* 
char_bitmap_pixmap (LOGFONT* logfont, DEVFONT* devfont, 
        const unsigned char* mchar, int len, int* pitch) 
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    FTFACEINFO*     ft_face_info = ft_inst_info->ft_face_info;
    FT_BitmapGlyph  glyph_bitmap;
    FT_Bitmap*      source;
    FT_Int          destroy;
    FT_UInt         uni_char = (*devfont->charset_ops->conv_to_uc32) (mchar);
    FT_Face         face;
#ifdef _FT2_CACHE_SUPPORT
    FT_Size         size;
#endif

    assert(ft_face_info->valid == TRUE);
    
#ifndef _FT2_CACHE_SUPPORT
    face = ft_face_info->face;
    ft_inst_info->cur_index = FT_Get_Char_Index (face, uni_char);
    
    if (FT_Load_Glyph (face, ft_inst_info->cur_index, 
                    FT_LOAD_DEFAULT|FT_LOAD_NO_BITMAP
                    | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH
                    | FT_LOAD_TARGET_LCD))
    {
        fprintf(stderr, "FT_Load_Glyph error\n");
        return 0;
    }

    if (ft_inst_info->glyph) 
    {
        FT_Done_Glyph (ft_inst_info->glyph); 
    }

    if (FT_Get_Glyph (ft_face_info->face->glyph, &ft_inst_info->glyph))
    {
        fprintf(stderr, "FT_Get_Glyph failed\n");
        return NULL;
    }

    destroy = 1;
#else
    if (get_size (ft_inst_info, &size))
    {
        fprintf(stderr, "can't access font file %p\n", ft_face_info);
        return 0;
    }

    face = size->face;
    ft_inst_info->cur_index = FTC_CMapCache_Lookup (ft_cmap_cache,
                            (FTC_FaceID) (ft_inst_info->image_type.face_id),
                            ft_inst_info->ft_face_info->cmap_index,
                            uni_char);

    if (ft_inst_info->glyph_done && ft_inst_info->glyph) 
    {
        FT_Done_Glyph (ft_inst_info->glyph); 
        ft_inst_info->glyph_done = 0;
    }

    if (FTC_ImageCache_Lookup (ft_image_cache,
                             &ft_inst_info->image_type,
                             ft_inst_info->cur_index,
                             &ft_inst_info->glyph,
                             NULL))
    {
        fprintf(stderr, "can't access image cache\n");
        return NULL;
    }

    destroy = 0;
#endif

    /*convert to a bitmap (default render mode + destroy old or not)*/
    if (ft_inst_info->glyph->format != ft_glyph_format_bitmap) {

        if (ft_inst_info->ft_lcdfilter != FT_LCD_FILTER_NONE &&
            logfont->style & FS_WEIGHT_SUBPIXEL)
        {
            if (FT_Glyph_To_Bitmap (&(ft_inst_info->glyph), 
                        pitch ? FT_RENDER_MODE_LCD : ft_render_mode_mono,
                        NULL, destroy))
            {
                fprintf(stderr, "FT_Glyph_To_Bitmap failed\n");
                return NULL;
            }
        }
        else
        {
            if (FT_Glyph_To_Bitmap (&(ft_inst_info->glyph), 
                        pitch ? FT_RENDER_MODE_NORMAL : ft_render_mode_mono,
                        NULL, destroy))
            {
                fprintf(stderr, "FT_Glyph_To_Bitmap failed\n");
                return NULL;
            }
        }

#ifdef _FT2_CACHE_SUPPORT
        ft_inst_info->glyph_done = 1;
#endif
    }
    
    /* access bitmap content by type changing*/
    glyph_bitmap = (FT_BitmapGlyph) ft_inst_info->glyph;

    source = &glyph_bitmap->bitmap;
    
    if (pitch)
        *pitch = source->pitch;

    if (!pitch)
        press_bitmap(source->buffer, 
            source->width, source->rows, source->pitch);

    return source->buffer;
}

static const void* 
get_char_bitmap (LOGFONT* logfont, DEVFONT* devfont,
    const unsigned char* mchar, int len, unsigned short* scale)
{
    if (scale) *scale = 1;
    return char_bitmap_pixmap (logfont, devfont, mchar, len, NULL);
}
static const void* 
get_char_pixmap (LOGFONT* logfont, DEVFONT* devfont, 
            const unsigned char* mchar, int len, int* pitch, 
            unsigned short* scale)
{
    if (scale) *scale = 1;
    return char_bitmap_pixmap (logfont, devfont, mchar, len, pitch); 
}

/* call this function before output a string */
static void 
start_str_output (LOGFONT* logfont, DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    FT_Size         size;

    if (logfont->style & FS_WEIGHT_SUBPIXEL)
        FT_Library_SetLcdFilter(ft_library, ft_inst_info->ft_lcdfilter);

#ifdef _FT2_CACHE_SUPPORT
    get_size (ft_inst_info, &size);
#else
    size = ft_inst_info->size;
#endif

    FT_Activate_Size (size);

    ft_inst_info->prev_index = 0;
    ft_inst_info->cur_index = 0;
    ft_inst_info->is_index_old = 0;

    ft_inst_info->delta.x = 0;
    ft_inst_info->delta.y = 0;
}

/* call this function after getting the bitmap/pixmap of the char
 * to get the advance of the char */
static void 
get_char_advance (LOGFONT* logfont, DEVFONT* devfont, 
    const unsigned char* mchar, int len, int* px, int* py)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);

    if (ft_inst_info->use_kerning 
        && ft_inst_info->prev_index && ft_inst_info->cur_index) 
    {
        if (ft_inst_info->is_index_old)
        {
            if (px)        
                *px += ft_inst_info->delta.x >> 6;
        }
    }

    *px += ((ft_inst_info->glyph->advance.x + 0x8000)>> 16);
    *py -= (ft_inst_info->glyph->advance.y + 0x8000)>> 16;

    ft_inst_info->is_index_old = 0;
}

static DEVFONT* 
new_instance (LOGFONT* logfont, DEVFONT* devfont, BOOL need_sbc_font)
{
    FTFACEINFO*     ft_face_info = FT_FACE_INFO_P (devfont);
    FTINSTANCEINFO* ft_inst_info = NULL;
    DEVFONT*        new_devfont = NULL;
    float           angle;
    FT_Face         face;
    FT_Size         size;

    if ((new_devfont = calloc (1, sizeof (DEVFONT))) == NULL)
        goto out;

    if ((ft_inst_info = calloc (1, sizeof (FTINSTANCEINFO))) == NULL)
        goto out;

    ft_inst_info->glyph = NULL;

    memcpy (new_devfont, devfont, sizeof (DEVFONT));

    /* copy CharsetOps */
    if (need_sbc_font && devfont->charset_ops->bytes_maxlen_char > 1) {
        char charset [LEN_FONT_NAME + 1];

        fontGetCompatibleCharsetFromName (devfont->name, charset);
        new_devfont->charset_ops = GetCharsetOps (charset);
    }

    new_devfont->data = ft_inst_info;
    ft_inst_info->ft_face_info = ft_face_info;

    if (logfont->style & FS_WEIGHT_SUBPIXEL) {
        ft_inst_info->ft_lcdfilter = FT_LCD_FILTER_DEFAULT;
    }
    else {
        ft_inst_info->ft_lcdfilter = FT_LCD_FILTER_NONE;
    }

#ifdef _FT2_CACHE_SUPPORT
    ft_inst_info->glyph_done = 0;

    if (FTC_Manager_LookupFace (ft_cache_manager,
                  (FTC_FaceID)ft_face_info, &face))
    {
        /* can't access the font file. do not render anything */
        fprintf(stderr, "can't access font file %p\n", ft_face_info);
        return 0;
    }
#else
    face = ft_face_info->face;
#endif

    /* Create instance */
    if (FT_New_Size (face, &size))
        goto out_size;


    if (FT_Activate_Size (size))
        goto out_size;

    ft_inst_info->rotation = logfont->rotation; /* in tenthdegrees */

    angle = ft_inst_info->rotation * M_PI / 1800;
#ifndef _FT2_CACHE_SUPPORT
    ft_inst_info->matrix.yy = cos (angle) * (1 << 16);
    ft_inst_info->matrix.yx = sin (angle) * (1 << 16);
    ft_inst_info->matrix.xx = ft_inst_info->matrix.yy;
    ft_inst_info->matrix.xy = -ft_inst_info->matrix.yx;
#endif
    
    if (FT_Set_Pixel_Sizes (face, logfont->size, 0))
        goto out_size;
    
    ft_inst_info->use_kerning = 0;
    ft_inst_info->use_kerning = FT_HAS_KERNING(face); 
    
    ft_inst_info->max_width = size->metrics.x_ppem;
    ft_inst_info->ave_width = ft_inst_info->max_width;

    ft_inst_info->height = (size->metrics.height + 0x20)>> 6;
    ft_inst_info->ascent = (size->metrics.ascender + 0x20) >> 6;
    ft_inst_info->descent = (size->metrics.descender + 0x20)>>6;

    if (ft_inst_info->height < ft_inst_info->ascent - ft_inst_info->descent)
    {
        ft_inst_info->height = 
            ft_inst_info->ascent - ft_inst_info->descent;
    }

#ifdef _FT2_CACHE_SUPPORT
    ft_inst_info->image_type.width = ft_inst_info->ave_width;
    ft_inst_info->image_type.height = ft_inst_info->height;
    ft_inst_info->image_type.face_id = (FTC_FaceID)ft_inst_info->ft_face_info;
    FT_Done_Size (size);
#else
    ft_inst_info->size = size;
#endif
    return new_devfont;

out_size:
    FT_Done_Size (size);

out:
    free (ft_inst_info);
    free (new_devfont);
    return NULL;
}

static void 
delete_instance (DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);

#ifdef _FT2_CACHE_SUPPORT
    if (ft_inst_info->glyph_done && ft_inst_info->glyph) 
        FT_Done_Glyph(ft_inst_info->glyph); 
#else
    if (ft_inst_info->glyph) 
        FT_Done_Glyph(ft_inst_info->glyph); 

    FT_Done_Size (ft_inst_info->size);
#endif

    free (ft_inst_info);
    free (devfont);
}

/**************************** Global data ************************************/
FONTOPS freetype_font_ops = {
    get_char_width,
    get_ave_width,
    get_max_width,  
    get_font_height,
    get_font_size, 
    get_font_ascent,
    get_font_descent,
    get_char_bitmap, 
    get_char_pixmap,    
    start_str_output,
    get_char_bbox,    
    get_char_advance,
    new_instance,    
    delete_instance
};

/************************ Create/Destroy FreeType font ***********************/
static BOOL 
CreateFreeTypeFont (const char *name, FTFACEINFO* ft_face_info)
{
    int         i;
    FT_Error    error;
    FT_Face     face;
    FT_CharMap  charmap;

#ifdef _FT2_CACHE_SUPPORT
    strcpy (ft_face_info->filepathname, name);
    ft_face_info->face_index = 0;

    error = FT_New_Face (ft_library, name, ft_face_info->face_index, &face);
#else
    error = FT_New_Face (ft_library, name, 0, &ft_face_info->face);
    face = ft_face_info->face;
#endif

    if (error == FT_Err_Unknown_File_Format) {
        fprintf (stderr, "... the font file could be opened and read, but it\n\
                ...appears that its font format is unsupported \n");
        return FALSE;
    }
    else if (error) {
        fprintf (stderr, "...another error code means that the font file \n\
                ... could not be opened or read. or simply that it is\
                broken ... \n ");
        return FALSE;
    }

    /* Look for a Unicode charmap: Windows flavor of Apple flavor only */
    for (i = 0; i < face->num_charmaps; i++) {
        charmap = face->charmaps [i];

        if (((charmap->platform_id == TT_PLATFORM_MICROSOFT) 
                && (charmap->encoding_id == TT_MS_ID_UNICODE_CS))
                || ((charmap->platform_id == TT_PLATFORM_APPLE_UNICODE)
                && (charmap->encoding_id == TT_APPLE_ID_DEFAULT)))
        {
            error = FT_Set_Charmap (face, charmap);
            if (error) {
                fprintf (stderr, "GDI (When Create TTF Font): can not set \
                        UNICODE CharMap, error is %d \n", error);
                return FALSE;
            }
#ifdef _FT2_CACHE_SUPPORT
            ft_face_info->cmap_index = FT_Get_Charmap_Index(face->charmap);
#endif
            break;
        }
    }

    if (i == face->num_charmaps) {
        fprintf (stderr, "GDI (When Create TTF Font): No UNICODE CharMap\n");
        return FALSE;
    }

    return TRUE;
}

static void 
DestroyFreeTypeFont (FTFACEINFO* ft_face_info)
{
    if (ft_face_info->valid) {
#ifndef _FT2_CACHE_SUPPORT
        FT_Done_Face (ft_face_info->face);
#endif
        ft_face_info->valid = FALSE;
    }
}

/************************ Init/Term of FreeType fonts ************************/
static int          nr_fonts;
static FTFACEINFO*  ft_face_infos;
static DEVFONT*     ft_dev_fonts;

#define SECTION_NAME    "truetypefonts"

static void 
ShowErr (const char*  message , int error)
{
    fprintf( stderr, "%s\n  error = 0x%04x\n", message, error );
}

static void add_devfont_for_other_charset (DEVFONT* ft_dev_font)
{
    int  nr_charsets;
    char charsets [LEN_UNIDEVFONT_NAME + 1];
    
    fontGetCharsetPartFromName (ft_dev_font->name, charsets);
    if ((nr_charsets = charsetGetCharsetsNumber (charsets)) > 1) {
        int j;
        for (j = 1; j < nr_charsets; j++) {
            char        charset [LEN_FONT_NAME + 1];
            CHARSETOPS* charset_ops;
            DEVFONT*    new_devfont;

            charsetGetSpecificCharset (charsets, j, charset);
            if ((charset_ops = GetCharsetOpsEx (charset)) == NULL)
                continue;

            new_devfont = calloc (1, sizeof (DEVFONT));
            memcpy (new_devfont, ft_dev_font, sizeof (DEVFONT));
            new_devfont->charset_ops = charset_ops;
            new_devfont->relationship = ft_dev_font;
            if (new_devfont->charset_ops->bytes_maxlen_char > 1)
                AddMBDevFont (new_devfont);
            else
                AddSBDevFont (new_devfont);
        }
    }
}

/******************************* Global data ********************************/
BOOL InitFreeTypeFonts (void)
{
    int         i;
    char        font_name [LEN_DEVFONT_NAME + 1];
    FT_Error    error;

    /* Init freetype library */
    error = FT_Init_FreeType (&ft_library);
    if (error) {
        ShowErr ("could not initialize FreeType 2 library", error);
        goto error_library;
    }

    /* Init freetype2 cache manager */
#ifdef _FT2_CACHE_SUPPORT
    error = FTC_Manager_New (ft_library, 0, 0, 0,
                 my_face_requester, 0, &ft_cache_manager);
    if (error) {
        ShowErr ("could not initialize cache manager", error);
        goto error_ftc_manager;
    }

    error = FTC_ImageCache_New (ft_cache_manager, &ft_image_cache);
    if (error) {
        ShowErr ("could not initialize glyph image cache", error);
        goto error_ftc_manager;
    }

    error = FTC_CMapCache_New (ft_cache_manager, &ft_cmap_cache);
    if (error) {
        ShowErr ("could not initialize charmap cache", error);
        goto error_ftc_manager;
    }
#endif
    /* Does load TrueType fonts? */
    if (GetMgEtcIntValue (SECTION_NAME, "font_number",
                &nr_fonts) < 0)
        return FALSE;

    if (nr_fonts < 1) return TRUE;

    /* Alloc space for devfont and ttfinfo. */
    ft_face_infos = calloc (nr_fonts, sizeof (FTFACEINFO));
    ft_dev_fonts = calloc (nr_fonts, sizeof (DEVFONT));
    if (ft_face_infos == NULL || ft_dev_fonts == NULL) {
        goto error_alloc;
    }

    for (i = 0; i < nr_fonts; i++)
        ft_face_infos [i].valid = FALSE;

    for (i = 0; i < nr_fonts; i++) {
        char        key [11];
        char        charset [LEN_FONT_NAME + 1];
        char        file [MAX_PATH + 1];
        CHARSETOPS* charset_ops;

        sprintf (key, "name%d", i);
        if (GetMgEtcValue (SECTION_NAME, key,
                   font_name, LEN_DEVFONT_NAME) < 0)
            goto error_load;

        if (!fontGetCharsetFromName (font_name, charset)) {
            fprintf (stderr, "GDI: Invalid font name (charset): %s.\n",
                    font_name);
            goto error_load;
        }

        if ((charset_ops = GetCharsetOps (charset)) == NULL) {
            fprintf (stderr, "GDI: Not supported charset: %s.\n", charset);
            goto error_load;
        }

        sprintf (key, "fontfile%d", i);
        if (GetMgEtcValue (SECTION_NAME, key, file, MAX_PATH) < 0)
            goto error_load;

        if (!CreateFreeTypeFont (file, ft_face_infos + i))
            goto error_load;

        strncpy (ft_dev_fonts[i].name, font_name, LEN_DEVFONT_NAME);
        ft_dev_fonts[i].name [LEN_DEVFONT_NAME] = '\0';
        ft_dev_fonts[i].font_ops = &freetype_font_ops;
        ft_dev_fonts[i].charset_ops = charset_ops;
        ft_dev_fonts[i].data = ft_face_infos + i;

        ft_face_infos [i].valid = TRUE;
    }

    for (i = 0; i < nr_fonts; i++) {
        if (ft_dev_fonts [i].charset_ops->bytes_maxlen_char > 1) {
            AddMBDevFont (ft_dev_fonts + i);
        }
        else
            AddSBDevFont (ft_dev_fonts + i);

        add_devfont_for_other_charset (ft_dev_fonts + i);
    }

    return TRUE;

error_load:
    fprintf (stderr, "GDI: Error in loading TrueType fonts\n");
    for (i = 0; i < nr_fonts; i++)
        DestroyFreeTypeFont (ft_face_infos + i);

error_alloc:
    free (ft_face_infos);
    free (ft_dev_fonts);
    ft_face_infos = NULL;
    ft_dev_fonts = NULL;

#ifdef _FT2_CACHE_SUPPORT
error_ftc_manager:
    FTC_Manager_Done (ft_cache_manager); 
#endif

error_library:
    fprintf (stderr, "Could not initialise FreeType 2 library\n");
    FT_Done_FreeType (ft_library);

    return FALSE;
}

void 
TermFreeTypeFonts (void)
{
    int i;
    for (i = 0; i < nr_fonts; i++)
        DestroyFreeTypeFont (ft_face_infos + i);

    if (ft_face_infos) {
        free (ft_face_infos);
        ft_face_infos = NULL;
    }

    if (ft_dev_fonts) {
        free (ft_dev_fonts);
        ft_dev_fonts = NULL;
    }

#ifdef _FT2_CACHE_SUPPORT
    if (ft_cache_manager)
        FTC_Manager_Done (ft_cache_manager); 
#endif
    if (ft_library)
        FT_Done_FreeType (ft_library);
}

BOOL 
ft2SetLcdFilter (LOGFONT* logfont, mg_FT_LcdFilter filter)
{
    BOOL        rv = FALSE;
    DEVFONT*    devfont;

    if (logfont->style & FS_WEIGHT_SUBPIXEL) {

        devfont = logfont->sbc_devfont;
        if (devfont && devfont->font_ops == &freetype_font_ops) {
            FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
            ft_inst_info->ft_lcdfilter = filter;
            rv = TRUE;
        }

        devfont = logfont->mbc_devfont;
        if (devfont && devfont->font_ops == &freetype_font_ops) {
            FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
            ft_inst_info->ft_lcdfilter = filter;
            rv = TRUE;
        }
    }

    return rv;
}

int 
ft2GetLcdFilter (DEVFONT* devfont)
{
    FTINSTANCEINFO* ft_inst_info = FT_INST_INFO_P (devfont);
    return ft_inst_info->ft_lcdfilter;
}

int 
ft2IsFreeTypeDevfont (DEVFONT* devfont)
{
    if (devfont && devfont->font_ops == &freetype_font_ops)
        return TRUE;

    return FALSE;
}

/* type-family-style-width-height-charset-encoding1[,encoding2,...] */
static BOOL 
create_newfamily_fontname( char* devfont_name, 
    const char* type, const char* family_name, const char* file_style, 
    const char *style, int w, int h, const char* charset_encoding)
{
    const char def_type [] = "*";
    const char def_family_name [] = "*";
    const char def_file_style [] = ".R";
    const char def_style [] = "*";
    const char def_charset_encoding [] = "UTF-16LE";

    if (devfont_name == NULL) 
        return FALSE;

    if (type == NULL)
        type = def_type;

    if (family_name == NULL)
        family_name = def_family_name;

    if (file_style == NULL)
        file_style = def_file_style;

    if (style == NULL)
        style = def_style;

    if (charset_encoding == NULL)
        charset_encoding = def_charset_encoding;

    sprintf(devfont_name, "%s-%s%s-%s-%d-%d-%s", 
            type, family_name, file_style, style, w, h, charset_encoding);
    
    return TRUE;
}

#define NR_LOOP_FOR_STYLE   2
static BOOL 
get_font_style (const char* name, char* font_style_name)
{
    const char* style_part = name;
    int         i;

    if (!name)
        return FALSE;

    for (i = 0; i < NR_LOOP_FOR_STYLE; i++) {
        if ((style_part = strchr (style_part, '-')) == NULL)
            return FALSE;

        if (*(++style_part) == '\0')
            return FALSE;
    }

    strncpy (font_style_name, style_part, 6);

    return TRUE;
}

const DEVFONT* 
__mg_ft2load_devfont_fromfile
           (const char* devfont_name, const char* file_name)
{
    FTFACEINFO* ft_face_info = NULL;
    DEVFONT*    ft_dev_font  = NULL;
    CHARSETOPS* charset_ops;
    char        charset [LEN_FONT_NAME + 1];
    char        font_style_name [7];
    char        type_name [LEN_FONT_NAME + 1];
    FT_Face     face;

    if ((devfont_name == NULL) || (file_name == NULL)) return NULL;

    if (!fontGetTypeNameFromName (devfont_name, type_name)) {
        fprintf(stderr, "font type error\n");
        return NULL;
    }

    if (strcasecmp (type_name, FONT_TYPE_NAME_SCALE_TTF)) {
        fprintf(stderr, "it's not free type font type error\n");
        return NULL;
    }

    ft_face_info = calloc (1, sizeof (FTFACEINFO));
    ft_dev_font = calloc (1, sizeof (DEVFONT));

    if (ft_face_info == NULL || ft_dev_font == NULL) {
        fprintf(stderr, 
            "ft2LoadDevFontFromFile: FTFACEINFO/DEVFONT memory alloc failed\n");
        goto error_alloc;
    }

    ft_face_info->valid = FALSE;

    if (!fontGetCharsetFromName (devfont_name, charset)) {
        fprintf (stderr, "ft2LoadDevFontFromFile: "
                "Invalid font name (charset): %s.\n", devfont_name);
        goto error_load;
    }

    if ((charset_ops = GetCharsetOps (charset)) == NULL) {
        fprintf (stderr, "ft2LoadDevFontFromFile: "
                "Not supported charset: %s.\n", charset);
        goto error_load;
    }

    if (!CreateFreeTypeFont (file_name, ft_face_info)) {
        fprintf (stderr, "ft2LoadDevFontFromFile: "
                "Error in loading TrueType fonts\n");
        goto error_load;
    }

    if (!get_font_style (devfont_name, font_style_name))
        strncpy (font_style_name, "rnncnn", 6);

    font_style_name[6] = '\0';

#ifdef _FT2_CACHE_SUPPORT
    if ( FTC_Manager_LookupFace (ft_cache_manager,
                                      (FTC_FaceID)ft_face_info, &face))
    {
        fprintf (stderr, "can't access font file %p\n", ft_face_info);
        return 0;
    }
#else
    face = ft_face_info->face;
#endif

    if (strcasecmp (face->style_name, "Bold") == 0) {
        if (!create_newfamily_fontname (ft_dev_font->name,
                            "ttf", face->family_name, ".B", 
                            font_style_name, 0, 0, charset)) {
            strncpy (ft_dev_font->name, devfont_name, LEN_DEVFONT_NAME);
        }

    } else if (strcasecmp (face->style_name, "Italic") == 0) {
        if (!create_newfamily_fontname (ft_dev_font->name,
                            "ttf", face->family_name, ".I",
                            font_style_name, 0, 0, charset)) {
            strncpy (ft_dev_font->name, devfont_name, LEN_DEVFONT_NAME);
        }

    } else if (strcasecmp (face->style_name, "Regular") == 0) {
        if (!create_newfamily_fontname (ft_dev_font->name,
                            "ttf", face->family_name, ".R",
                            font_style_name, 0, 0, charset)) {
            strncpy (ft_dev_font->name, devfont_name, LEN_DEVFONT_NAME);
        }
    }

    ft_dev_font->name [LEN_DEVFONT_NAME] = '\0';
    ft_dev_font->font_ops = &freetype_font_ops;
    ft_dev_font->charset_ops = charset_ops;
    ft_dev_font->data = ft_face_info;

    ft_face_info->valid = TRUE;

    if (ft_dev_font->charset_ops->bytes_maxlen_char > 1) {
        AddMBDevFont (ft_dev_font);
    }
    else
        AddSBDevFont (ft_dev_font);

    add_devfont_for_other_charset (ft_dev_font);

    return ft_dev_font;

error_load:
    fprintf (stderr, "__mg_ft2load_devfont_fromfile: Error in loading TrueType fonts\n");
    DestroyFreeTypeFont (ft_face_info);

error_alloc:
    free (ft_face_info);
    free (ft_dev_font);
    ft_face_info = NULL;
    ft_dev_font = NULL;

    return NULL;
}

void __mg_ft2_destroy_devfont_fromfile (DEVFONT **devfont)
{
    DestroyFreeTypeFont ((FTFACEINFO*)((*devfont)->data));
    free ((FTFACEINFO*)((*devfont)->data));

    DelDevFont (*devfont, TRUE);
}

#endif /* _FT2_SUPPORT */
