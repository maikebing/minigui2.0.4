
/*
 * Copyright (C) 2000-2008 Beijing Komoxo Inc.
 * All rights reserved.
 */

#include <assert.h>
#include <string.h>
#include "common.h"
#include "minigui.h"
#include "gdi.h"
#include "misc.h"
#include "devfont.h"
#include "charset.h"
#include "fontname.h"
#include "se_minigui.h"

#ifdef _SCRIPT_EASY_SUPPORT 
static DEVFONT      *se_dev_fonts = NULL;

static FONTOPS scripteasy_font_ops;

static unsigned char   is_se_initialized = 0;
static pthread_mutex_t se_mutex;

#define SE_MINIGUI_TRACE          0

static void init_lock(void)
{
    assert(!is_se_initialized);
    pthread_mutex_init(&se_mutex, NULL);
}

static void destroy_lock(void)
{
    assert(is_se_initialized);
    pthread_mutex_destroy(&se_mutex);
}

static void lock(void)
{
    assert(is_se_initialized);
    pthread_mutex_lock(&se_mutex);
}

static void unlock(void)
{
    assert(is_se_initialized);
    pthread_mutex_unlock(&se_mutex);
}

static BOOL load_fonts(void)
{
    int i;
    int font_table_size = se_font_desc.font_name_map_size;    
    BOOL result = FALSE;

#if SE_MINIGUI_TRACE
    printf("load_fonts, font_table_size: %d...\n", font_table_size);
#endif

    se_dev_fonts = calloc (font_table_size, sizeof (DEVFONT));

    for (i = 0; i < font_table_size; i++) {
        char        charset[LEN_FONT_NAME + 1];
        CHARSETOPS  *charset_ops;
        const char  *font_name;
        int         font_id;

        font_name = se_font_desc.name_map[i].font_name;
        font_id = se_font_desc.name_map[i].font_id;

        if (!fontGetCharsetFromName (font_name, charset)) {
            fprintf (stderr, "GDI: Invalid font name (charset): %s.\n",
                    font_name);
            goto exit;
        }

        if ((charset_ops = GetCharsetOps (charset)) == NULL) {
            fprintf (stderr, "GDI: Not supported charset: %s.\n", charset);
            goto exit;
        }

        strncpy (se_dev_fonts[i].name, font_name, LEN_DEVFONT_NAME);
        se_dev_fonts[i].name [LEN_DEVFONT_NAME] = '\0';
        se_dev_fonts[i].font_ops = &scripteasy_font_ops;
        se_dev_fonts[i].charset_ops = charset_ops;
        se_dev_fonts[i].data = (void*)font_id;
#if 1
        fprintf (stderr, "GDI: TTFDevFont %i: %s.\n", i, se_dev_fonts[i].name);
#endif
    }

    /* houhh 20090112, surport the other GB2312,UTF-8 such font setting.*/
    for (i = 0; i < font_table_size; i++) {
        int nr_charsets;
        char charsets [LEN_UNIDEVFONT_NAME + 1];

        if (se_dev_fonts [i].charset_ops->bytes_maxlen_char > 1) {
            AddMBDevFont (se_dev_fonts + i);
            AddSBDevFont (se_dev_fonts + i);
        }
        else
            AddSBDevFont (se_dev_fonts + i);

        fontGetCharsetPartFromName (se_dev_fonts[i].name, charsets);
        if ((nr_charsets = charsetGetCharsetsNumber (charsets)) > 1) {
            int j;
            for (j = 1; j < nr_charsets; j++) {
                char charset [LEN_FONT_NAME + 1];
                CHARSETOPS* charset_ops;
                DEVFONT* new_devfont;

                charsetGetSpecificCharset (charsets, j, charset);
                if ((charset_ops = GetCharsetOpsEx (charset)) == NULL)
                    continue;

                new_devfont = calloc (1, sizeof (DEVFONT));
                memcpy (new_devfont, se_dev_fonts + i, sizeof (DEVFONT));
                new_devfont->charset_ops = charset_ops;
                new_devfont->relationship = se_dev_fonts + i;
                if (new_devfont->charset_ops->bytes_maxlen_char > 1)
                    AddMBDevFont (new_devfont);
                else
                    AddSBDevFont (new_devfont);
            }
        }
    }

    result = TRUE;

exit:

#if SE_MINIGUI_TRACE
    printf("load_fonts complete, result: %s...\n", result ? "success" : "fail");
#endif

    return result;
}

static BOOL init(
    void                *mem_pool, 
    int                 mem_pool_size, 
    void                *cache_mem_pool, 
    int                 cache_mem_pool_size)
{
    SeInitInfo  init_info;
    int         result;
    
    //
    // SeInitInfo initialization
    //
    memset( &init_info,0,sizeof(init_info) );
    init_info.language = SE_LANG_ZH_CN;
    init_info.mem_pool = (void*)mem_pool;
    init_info.mem_pool_size = mem_pool_size;
    init_info.font_table = se_font_desc.font_table;
    init_info.cache_mem_pool = (void*)cache_mem_pool;
    init_info.cache_mem_pool_size = cache_mem_pool_size;
	
    result = se_minigui_initialize( &init_info );
	if(result != SE_STATUS_OK)
        return FALSE;

    return TRUE;
}

SE_EXPORT BOOL initialize_scripteasy(void)
{
#if SE_MINIGUI_TRACE
    printf("in initialize_scripteasy...\n");
#endif
    if(is_se_initialized)
        return TRUE;

    if(se_font_desc.font_table_size < 1)
        return TRUE;
    
    if(!load_fonts())
        return FALSE;

    if(!init(se_minigui_mem_pool, se_minigui_mem_pool_size, se_minigui_cache_mem_pool, se_minigui_cache_mem_pool_size))
        return FALSE;

    init_lock();
    is_se_initialized = 1;

#if SE_MINIGUI_TRACE
    printf("initialize_scripteasy complete...\n");
#endif

    return TRUE;
}

SE_EXPORT void uninitialize_scripteasy(void)
{
#ifndef SE_OPT_ARM
    printf("in uninitialize_scripteasy...\n");
#endif
    if(!is_se_initialized)
        return;

    se_minigui_cleanup();

    if(se_dev_fonts != NULL)
    {
        free(se_dev_fonts);
        se_dev_fonts = NULL;
    }

    destroy_lock();
    is_se_initialized = 0;
}

static void convert_log_font(SeLogFont *selogfont, const LOGFONT *logfont, const DEVFONT *devfont)
{
    selogfont->size = logfont->size;
    selogfont->id = (unsigned char)(int)devfont->data;

    selogfont->attr = 0;

    if(selogfont->id > 0)
        selogfont->attr |= SE_LOGFONT_PREFERRED;

    if(logfont->style & FS_WEIGHT_BOLD)
        selogfont->attr |= SE_LOGFONT_BOLD;

    if(logfont->style & FS_SLANT_ITALIC)
        selogfont->attr |= SE_LOGFONT_ITALIC;

    if(logfont->style & FS_UNDERLINE_LINE)
        selogfont->attr |= SE_LOGFONT_UNDERLINE;

    if(logfont->style & FS_STRUCKOUT_LINE)
        selogfont->attr |= SE_LOGFONT_STRIKETHROUGH;

#if 0// SE_MINIGUI_TRACE
    printf("logfont >>> size: %d id: %d attr: 0x%02X\n", selogfont->size, selogfont->id, selogfont->attr);
#endif
}

static int get_char_width (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len)
{
    seunichar16 wc;
    SeLogFont   selogfont;
    int         width;

    wc = (*devfont->charset_ops->conv_to_uc32)(mchar);

#if SE_MINIGUI_TRACE
    printf("in get_char_width...\n");
#endif

    convert_log_font(&selogfont, logfont, devfont);

    lock();

    width = se_minigui_get_char_width(&selogfont, wc);

    unlock();

#if SE_MINIGUI_TRACE
    printf("get_char_width complete...\n");
#endif

    return width;
}

static int get_max_width (LOGFONT* logfont, DEVFONT* devfont)
{
    return logfont->size;
}

static int get_ave_width (LOGFONT* logfont, DEVFONT* devfont)
{
    return logfont->size;
}

static int get_font_height (LOGFONT* logfont, DEVFONT* devfont)
{
    return logfont->size;
}

static int get_font_size (LOGFONT* logfont, DEVFONT* devfont, int expect)
{
    return logfont->size;
}

static int get_font_ascent (LOGFONT* logfont, DEVFONT* devfont)
{
    SeLogFont   selogfont;
    int ascent;

    convert_log_font(&selogfont, logfont, devfont);

    lock();

    ascent = se_minigui_get_font_ascent(&selogfont);

    unlock();

#if SE_MINIGUI_TRACE
    printf("get_font_ascent complete. ascent: %d...\n", ascent);
#endif

    return ascent;
}

static int get_font_descent (LOGFONT* logfont, DEVFONT* devfont)
{
    SeLogFont   selogfont;
    int descent;

    convert_log_font(&selogfont, logfont, devfont);

    lock();

    descent = se_minigui_get_font_descent(&selogfont);

    unlock();

#if SE_MINIGUI_TRACE
    printf("get_font_descent complete. descent: %d...\n", descent);
#endif

    return descent;
}

static const void *get_char_pixmap(LOGFONT *logfont, DEVFONT *devfont,
             const unsigned char *mchar, int len,
             int *pitch, unsigned short *scale)
{
    seunichar16 wc;
    SeLogFont   selogfont;
    const void *result;
    
#if SE_MINIGUI_TRACE
    printf("in get_char_pixmap....\n");
#endif

    wc = (*devfont->charset_ops->conv_to_uc32)(mchar);

    convert_log_font(&selogfont, logfont, devfont);
    
    lock();
    result = se_minigui_get_char_pixmap(&selogfont, wc, pitch);
    unlock();    

    if(scale)
        *scale = 1;

    return result;
}

static const void* get_char_bitmap (LOGFONT* logfont, DEVFONT* devfont,
            const unsigned char* mchar, int len, unsigned short *scale)
{
    seunichar16 wc;
    SeLogFont   selogfont;
    int         pitch = 0;
    const void *result = NULL;

#if SE_MINIGUI_TRACE
    printf("in get_char_bitmap....\n");
#endif

    wc = (*devfont->charset_ops->conv_to_uc32)(mchar);

    convert_log_font(&selogfont, logfont, devfont);
    selogfont.attr |= SE_LOGFONT_MONO;
    
    lock();
    result = se_minigui_get_char_pixmap(&selogfont, wc, &pitch);
    unlock();

    if(scale)
        *scale = 1;

    return result;
}

static int get_char_bbox(LOGFONT *logfont, DEVFONT *devfont,
          const unsigned char *mchar, int len,
           int *px, int *py, int *pwidth, int *pheight)
{
    seunichar16 wc;
    SeLogFont   selogfont;
    int result = 0;

    if(px == 0 && py == 0 && pwidth == 0 && pheight == 0)
        return 1;

    wc = (*devfont->charset_ops->conv_to_uc32)(mchar);

    convert_log_font(&selogfont, logfont, devfont);

    lock();

    result = se_minigui_get_char_bbox(&selogfont, wc, px, py, pwidth, pheight);

    unlock();

#if SE_MINIGUI_TRACE
    printf("get_char_bbox result. wc: 0x%04X x: %d y: %d w: %d h: %d\n", wc, px ? *px : 0, py ? *py : 0,
              pwidth ? *pwidth : 0, pheight ? *pheight : 0);
#endif

    return result;
}

static void get_char_advance(LOGFONT *logfont, DEVFONT *devfont,
          const unsigned char *mchar, int len,
           int *px, int *py)
{
    seunichar16 wc;
    SeLogFont   selogfont;

    wc = (*devfont->charset_ops->conv_to_uc32)(mchar);

    convert_log_font(&selogfont, logfont, devfont);

    lock();

    se_minigui_get_char_advance(&selogfont, wc, px, py);

    unlock();

#if SE_MINIGUI_TRACE
    printf("get_char_advance result. wc: 0x%04X x: %d y: %d\n", wc, px ? *px : 0, py ? *py : 0);
#endif
}

/**************************** Global data ************************************/
static FONTOPS scripteasy_font_ops = {
    get_char_width,
    get_ave_width,
    get_max_width,  
    get_font_height,
    get_font_size,
    get_font_ascent,
    get_font_descent,
    get_char_bitmap,    
    get_char_pixmap,
    NULL,
    get_char_bbox,
    get_char_advance,
    NULL,
    NULL
};

#endif
