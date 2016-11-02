/*
** $Id: type1.c 7330 2007-08-16 03:17:58Z xgwang $
** 
** type1.c: Type1 font support based on t1lib.
** 
** Copyright (C) 2003 ~ 2007 Feynman Software.
** Copyright (C) 2000 ~ 2002 Wei Yongming, Song Lixin.
**
** All right reserved by Feynman Software.
**
** Current maintainer: Wei Yongming
**
** Create date: 2000/08/29
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "minigui.h"
#include "gdi.h"
#include "misc.h"
#include "devfont.h"
#include "charset.h"
#include "fontname.h"

#ifdef _TYPE1_SUPPORT

#include <t1lib.h>
#include "type1.h"

#define ADJUST_SIZE 1.1;

/************************ Init/Term of FreeType fonts ************************/
static const void* get_char_bitmap (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len, unsigned short* scale);
static const void* get_char_pixmap (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len, 
                int* pitch, unsigned short* scale);
static int get_font_ascent (LOGFONT* logfont, DEVFONT* devfont);
static int get_font_descent (LOGFONT* logfont, DEVFONT* devfont);

static int nr_fonts;
static TYPE1INFO * type1_infos;
static DEVFONT * type1_dev_fonts;

static encstruct default_enc;

#define SECTION_NAME    "type1fonts"
#define T1LIBENCFILE    "IsoLatin1.enc"

/*
 * I always use full path file name ,so it maybe of no use...:-)
 * Only for the sake of complety,I set search path to this .
 * */
#define T1FONTPATH      "/usr/local/lib/minigui/fonts"

#define CHARSPACEUNIT2PIXEL(x)  (x/1000)
/*
 * Because the uplayer did not provide the option of whether
 * to use anti-aliaing.We use it in default.
 * On day it supports the option,the code may be easily updated.
 * */

BOOL InitType1Fonts (void)
{
	int i,j;
	char font_name [LEN_DEVFONT_NAME + 1];
	int loglevel;

	char key [11];
	char charset [LEN_FONT_NAME + 1];
	CHARSETOPS* charset_ops;
	char file [MAX_PATH + 1];
	int font_id;

	/* Does load Type1 fonts? */
	if (GetMgEtcIntValue (SECTION_NAME, "font_number",&nr_fonts) < 0 )
		return FALSE;
	if ( nr_fonts < 1) return TRUE;

	loglevel = 0;
	loglevel |= NO_LOGFILE;
	loglevel |= IGNORE_CONFIGFILE;
	loglevel |= IGNORE_FONTDATABASE;

	T1_InitLib(loglevel);

	T1_SetBitmapPad(8);
	T1_AASetBitsPerPixel(8);
	T1_AASetGrayValues(0,1,2,3,4);

	/* 
	 * Set some default value  base on my own idea. 
	 * So u can change it if u have enough reason.
	 * */
#if 1  
	//T1_SetDeviceResolutions(72,72);
	default_enc.encoding = T1_LoadEncoding (T1LIBENCFILE);
	default_enc.encfilename = (char *) malloc (strlen (T1LIBENCFILE) + 1);
	strcpy (default_enc.encfilename, T1LIBENCFILE);
    	T1_SetDefaultEncoding (default_enc.encoding);
	T1_AASetLevel (T1_AA_LOW);
#endif

	/* Alloc space for devfont and type1info. */
	type1_infos = calloc (nr_fonts, sizeof (TYPE1INFO));
	type1_dev_fonts = calloc (nr_fonts, sizeof (DEVFONT));
	if (type1_infos == NULL || type1_dev_fonts == NULL) {
		goto error_alloc;
	}

	for (i = 0; i < nr_fonts; i++)
		type1_infos [i].valid = FALSE;

	/*
	 * I always use full path file name ,so it maybe of no use...:-)
	 * Only for the sake of complety,I set search path to this .
	 * */
	T1_SetFileSearchPath (T1_PFAB_PATH | T1_AFM_PATH | T1_ENC_PATH ,T1FONTPATH);

	for (i = 0; i < nr_fonts; i++) {
		sprintf (key, "name%d", i);
		if (GetMgEtcValue (SECTION_NAME, key,font_name, LEN_DEVFONT_NAME) < 0 )
			goto error_load;

		if (!fontGetCharsetFromName (font_name, charset)) {
			fprintf (stderr, "GDI: Invalid font name (charset): %s.\n",font_name);
			goto error_load;
		}

		if ((charset_ops = GetCharsetOpsEx (charset)) == NULL) {
			fprintf (stderr, "GDI: Not supported charset: %s.\n", charset);
			goto error_load;
		}

		sprintf (key, "fontfile%d", i);
		if (GetMgEtcValue (SECTION_NAME, key, file, MAX_PATH) < 0)
			goto error_load;
	
		T1_AddFont(file);

		font_id = T1_AddFont(file);
		if (font_id <= 0) goto error_load;
		j = strstr(file,"pfb") - file;
		if (font_id <= 0) goto error_load;
		strncpy(file+j,"afm",3);	
		if (T1_SetAfmFileName(font_id,file) < 0) goto error_load;	
		strncpy(file+j,"pfb",3);	

		strncpy (type1_dev_fonts[i].name, font_name, LEN_DEVFONT_NAME);
		type1_dev_fonts[i].name [LEN_DEVFONT_NAME] = '\0';
		type1_dev_fonts[i].font_ops = &type1_font_ops;
		type1_dev_fonts[i].charset_ops = charset_ops;
		type1_dev_fonts[i].data = type1_infos + i;

		type1_infos[i].font_id = font_id;

		type1_infos [i].valid = TRUE;
	}

	for (i = 0; i < nr_fonts; i++) 
		AddSBDevFont (type1_dev_fonts + i);

	return TRUE;

	error_load:
		free (type1_infos);
		free (type1_dev_fonts);
	error_alloc:
		fprintf (stderr, "GDI:font/type1.c: Error when init Type1 fonts\n");
		type1_infos = NULL;
		type1_dev_fonts = NULL;
		T1_CloseLib();
	return FALSE;
}

void TermType1Fonts (void)
{
	free (type1_infos);
	free (type1_dev_fonts);
	type1_infos = NULL;
	type1_dev_fonts = NULL;
	T1_CloseLib();
}

/*
 * The width of a character is the amount of horizontal 
 * escapement that the next character is shifted 
 * to the right with respect to the current position
 * */
static int get_char_width (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	TYPE1INFO* type1_info = type1_inst_info->type1_info;

	int width;
	width = T1_GetCharWidth ( type1_info->font_id, *mchar );
	width *= type1_inst_info->csUnit2Pixel;
	return width;
}

/*
 * The width of a character is the amount of horizontal 
 * escapement that the next character is shifted 
 * to the right with respect to the current position
 * */
static int get_max_width (LOGFONT* logfont, DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	return type1_inst_info->max_width;
}

static int get_ave_width (LOGFONT* logfont, DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	return type1_inst_info->ave_width;
}

static int get_font_height (LOGFONT* logfont, DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	return type1_inst_info->font_height;
}

static int get_font_size (LOGFONT* logfont, DEVFONT* devfont, int expect)
{
	return expect;
}

static int get_font_ascent (LOGFONT* logfont, DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	return type1_inst_info->font_ascent;
}

static int get_font_descent (LOGFONT* logfont, DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	return  -type1_inst_info->font_descent;
}

/*
 * FIXME: what the meaning of this functin when rotation is taken 
 * into account?
 * This implementation conflicts with that of freetype now.
 * */
static size_t char_bitmap_size (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len)
{
	int pixel_width,pixel_height;
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);

	get_char_bitmap (logfont, devfont, mchar, len, NULL);
	pixel_width = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing;
	pixel_height = type1_inst_info->last_ascent - type1_inst_info->last_descent;

	return pixel_width * pixel_height ;
}

static size_t max_bitmap_size (LOGFONT* logfont, DEVFONT* devfont)
{
	return get_max_width(logfont,devfont) * get_font_height(logfont,devfont);
}

/* 
 * NULL function
 * */
static void start_str_output (LOGFONT* logfont, DEVFONT* devfont)
{
	return;
}

/* 
 * call this function before getting the bitmap/pixmap of the char
 * to get the bbox of the char 
 * */

/*
 * FIXME: Because the limits of the interface(either ours of theirs
 * we can get the bounding box when rotation is in regard!
 * In detail: T1lib provide functinos to draw a string,
 * while we choose to draw it one by one!!
 * */
static int get_char_bbox (LOGFONT* logfont, DEVFONT* devfont, 
                const unsigned char* mchar, int len, 
                int* px, int* py, int* pwidth, int* pheight)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
    
	get_char_bitmap(logfont, devfont, mchar, len, NULL);
	
	if(px){ 
		*px += type1_inst_info->last_leftSideBearing;
	}
	if(py){
		*py -= type1_inst_info->last_ascent;
	}
  	if(pwidth) {
		*pwidth = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing;
	}
	if(pheight){
		*pheight = type1_inst_info->last_ascent - type1_inst_info->last_descent;
	}
	if(pwidth)
		return *pwidth;
	else 
		return -1;
}

static const void* get_char_bitmap (LOGFONT* logfont, DEVFONT* devfont,
            const unsigned char* mchar, int len, unsigned short* scale)
{
	GLYPH * glyph;
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	TYPE1INFO* type1_info = type1_inst_info->type1_info;
	unsigned char c = *mchar;

    if (scale) *scale = 1;

	if(type1_inst_info->last_bitmap_char == *mchar) {
		return type1_inst_info->last_bits;	
	}

	glyph = T1_SetChar (type1_info->font_id, c, type1_inst_info->size, type1_inst_info->pmatrix);

	type1_inst_info->last_bitmap_char = *mchar;
	type1_inst_info->last_pixmap_char = -1;
	if(type1_inst_info->last_bitmap_str){
		free (type1_inst_info->last_bitmap_str);
		type1_inst_info->last_bitmap_str = NULL;
	}
	if(type1_inst_info->last_pixmap_str){
		free (type1_inst_info->last_pixmap_str);
		type1_inst_info->last_pixmap_str = NULL;
	}

    	type1_inst_info->last_ascent = glyph->metrics.ascent;
    	type1_inst_info->last_descent = glyph->metrics.descent;
    	type1_inst_info->last_leftSideBearing = glyph->metrics.leftSideBearing;
    	type1_inst_info->last_rightSideBearing = glyph->metrics.rightSideBearing;
    	type1_inst_info->last_advanceX = glyph->metrics.advanceX;
    	type1_inst_info->last_advanceY = glyph->metrics.advanceY;
    	type1_inst_info->last_bpp = glyph->bpp;

	/*free the last char's bitmap*/
	if(!type1_inst_info->last_bits)
		free(type1_inst_info->last_bits);
    	type1_inst_info->last_bits = glyph->bits;
	/*change the endian*/
	{
		int height,width;
		int i,j,k;
		unsigned char c;
		unsigned char d;
		height = type1_inst_info->last_ascent - type1_inst_info->last_descent ;
		width  = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing ;   
			
		width  = ( width + 7 ) >> 3 << 3 ;
		
		//fprintf(stderr, "height:%d, width %d\n", height, width);
		
		for (i = 0; i < height; i++) {
			for (j = 0; j < width/8 ; j++) {
				c = type1_inst_info->last_bits[ i * width / 8 + j];
				if( c ) {
					d = 0;	
					for ( k = 0; k < 8; k++ ) {
						if ( ( c >> k ) & 0x01 )
							d |= 0x80 >> k ;
					}
					type1_inst_info->last_bits[ i * width / 8 + j] = d ;
				}
#if 0  
				c = type1_inst_info->last_bits[ i * width / 8 + j];
                		for (k = 0; k < 8; k++)
              			      if (c  & (0x80>>k))
                   			     fprintf (stderr, "*");
                		      else
                     			     fprintf (stderr, " ");

#endif 
			}
#if 0
            		fprintf (stderr, "\n");
#endif
		}
	}
	/*
	 * if not set it to null, T1_SetChar will free it next time it is called,
	 * we do so to keep it by ourself
	 * */ 
	glyph->bits = NULL;

	return type1_inst_info->last_bits;	

}
static const void* get_char_pixmap (LOGFONT* logfont, DEVFONT* devfont,
            const unsigned char* mchar, int len, 
            int* pitch, unsigned short* scale)
{
	GLYPH * glyph;
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	TYPE1INFO* type1_info = type1_inst_info->type1_info;

    if (scale) *scale = 1;

	if(type1_inst_info->last_pixmap_char == *mchar){
	/* same char as last call, use the cache one.*/
		if ( pitch )
	 		*pitch = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing;   
		return type1_inst_info->last_bits;	
	}
	
	glyph = T1_AASetChar ( type1_info->font_id , *mchar , type1_inst_info->size , type1_inst_info->pmatrix) ;

	type1_inst_info->last_pixmap_char = *mchar;
	type1_inst_info->last_bitmap_char = -1;
	if(type1_inst_info->last_bitmap_str){
		free (type1_inst_info->last_bitmap_str);
		type1_inst_info->last_bitmap_str = NULL;
	}
	if(type1_inst_info->last_pixmap_str){
		free (type1_inst_info->last_pixmap_str);
		type1_inst_info->last_pixmap_str = NULL;
	}
    	type1_inst_info->last_ascent = glyph->metrics.ascent;
    	type1_inst_info->last_descent = glyph->metrics.descent;
    	type1_inst_info->last_leftSideBearing = glyph->metrics.leftSideBearing;
    	type1_inst_info->last_rightSideBearing = glyph->metrics.rightSideBearing;
    	type1_inst_info->last_advanceX = glyph->metrics.advanceX;
    	type1_inst_info->last_advanceY = glyph->metrics.advanceY;
    	type1_inst_info->last_bpp = glyph->bpp;
	/*free the last char's bitmap*/
	if(!type1_inst_info->last_bits)
		free(type1_inst_info->last_bits);
    	type1_inst_info->last_bits = glyph->bits;

	if ( pitch )
	 	*pitch = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing;   

#if 0
	{
		int height,width;
		int i,j,k;
		unsigned char c;
		unsigned char d;
		height = type1_inst_info->last_ascent - type1_inst_info->last_descent ;
		width  = type1_inst_info->last_rightSideBearing - type1_inst_info->last_leftSideBearing ;   
			
		
		for (i = 0; i < height; i++) {
			for (j = 0; j < width ; j++) {
				c = type1_inst_info->last_bits[ i * width + j];
				if( c ) 
					fprintf(stderr, "%d",c);
			 	else 
					fprintf(stderr, " ");
				
			}
            		fprintf (stderr, "\n");
		}
	}
#endif
	/*
	 * if not set it to null, T1_AASetChar will free it next time it is called,
	 * we do so to keep it by ourself
	 * */ 
	glyph->bits = NULL;

	return type1_inst_info->last_bits;	
}

/* 
 * call this function after getting the bitmap/pixmap of the char 
 * to get the advance of the char 
 * */
static void get_char_advance (LOGFONT* logfont, DEVFONT* devfont,
                const unsigned char* mchar, int len, 
                int* px, int* py)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	*px += type1_inst_info->last_advanceX;
	*py -= type1_inst_info->last_advanceY;

}

static DEVFONT* new_instance (LOGFONT* logfont, DEVFONT* devfont, BOOL need_sbc_font)
{
	TYPE1INFO* type1_info = TYPE1_INFO_P (devfont);
	TYPE1INSTANCEINFO* type1_inst_info = NULL;

	DEVFONT* new_devfont = NULL;
	
	unsigned char c ;
	BBox bbox;
	int i;
	int width=0;
	int sum=0;
	int count=0;
	int max_width=0;

	if ((new_devfont = (DEVFONT *)calloc (1, sizeof (DEVFONT))) == NULL)
	        goto out;

	if ((type1_inst_info = (TYPE1INSTANCEINFO *)calloc (1, sizeof (TYPE1INSTANCEINFO))) == NULL)
		goto out;

	memcpy (new_devfont, devfont, sizeof (DEVFONT));

	new_devfont->data = type1_inst_info;
	type1_inst_info->type1_info = type1_info;

	type1_inst_info->size     = logfont->size;

	if(logfont->rotation)
	{
		type1_inst_info->rotation = logfont->rotation; /* in tenthdegrees */
		type1_inst_info->pmatrix = T1_RotateMatrix( NULL ,     type1_inst_info->rotation / 10.0 );	
	}

	type1_inst_info->last_bitmap_char = -1;
	type1_inst_info->last_pixmap_char = -1;
	type1_inst_info->last_bitmap_str  = NULL;	
	type1_inst_info->last_pixmap_str  = NULL;	
	c = 'A';
	get_char_bitmap (logfont,new_devfont, &c, 1, NULL);
	bbox = T1_GetCharBBox(type1_info->font_id,'A');
	type1_inst_info->csUnit2Pixel = (double) ( type1_inst_info->last_rightSideBearing - 
		type1_inst_info->last_leftSideBearing ) / ( double ) ( bbox.urx - bbox.llx )  ;

	bbox = T1_GetFontBBox(type1_info->font_id);
	type1_inst_info->font_ascent = bbox.ury * type1_inst_info->csUnit2Pixel ;
	type1_inst_info->font_descent = bbox.lly * type1_inst_info->csUnit2Pixel ; 
	type1_inst_info->font_height = type1_inst_info->font_ascent - type1_inst_info->font_descent ;
	
	for( i = 0 ; i<=255; i++)
	{
		width = T1_GetCharWidth ( type1_info->font_id, i );
		if ( width > 0 ) {
			count ++;
			sum += width;
			if ( width > max_width)
				max_width = width;
		}	
	}
	
	type1_inst_info->max_width = max_width * type1_inst_info->csUnit2Pixel;
	type1_inst_info->ave_width = ( sum / count + 1 ) * type1_inst_info->csUnit2Pixel ;

	return new_devfont;
	out:
		free (type1_inst_info);
		free (new_devfont);
		return NULL;
}

static void delete_instance (DEVFONT* devfont)
{
	TYPE1INSTANCEINFO* type1_inst_info = TYPE1_INST_INFO_P (devfont);
	TYPE1INFO* type1_info = type1_inst_info->type1_info;

	T1_DeleteSize(type1_info->font_id,type1_inst_info->size);
	if(type1_inst_info->pmatrix)
		free(type1_inst_info->pmatrix);
	if(type1_inst_info->last_bits)
		free(type1_inst_info->last_bits);
	if(type1_inst_info->last_bitmap_str)
		free(type1_inst_info->last_bitmap_str);
	if(type1_inst_info->last_pixmap_str)
		free(type1_inst_info->last_pixmap_str);
	free (type1_inst_info);
	free (devfont);
}

/**************************** Global data ************************************/
static FONTOPS type1_font_ops = {
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


/* General transformation */
/*  
       		( y11   y21 )   
 (x1,x2)    *   (           ) = (x1*y11 +x2 *y12, x1 *y21+x2*y22;
   		( y12   y22 )   
*/
/*
static void my_TransformMatrix( T1_TMATRIX *matrix, double * x1, double  * x2 )
{
	double y1 ;
	double y2 ;
	if (matrix==NULL) return( NULL);
   
	y1 = ( *x1 ) * matrix->cxx + ( *x2 ) * matrix->cxy;
	y2 = ( *x1 ) * matrix->cyx + ( *x2 ) * matrix->cyy; 
	*x1 = y1;
	*x2 = y2;

	return;
}
*/

#endif /* _TYPE1_SUPPORT */

