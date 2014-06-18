/*
# PS2 JPEG Library Port
# Written by linuzappz.
# Modified by ntba2 for myPS2.
# Added jpgOpenFILE by Polo for ULE
*/

#include <tamtypes.h>
#include <kernel.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <screenshot.h>
#include <fileXio_rpc.h>

#include "libjpg.h"
#include "jpeglib.h"
#include "jerror.h"
#include "setjmp.h"

typedef struct {
	struct jpeg_error_mgr pub;	/* "public" fields */
	
	jmp_buf setjmp_buffer;	/* for return to caller */
} custom_error_mgr;

typedef struct {
	struct jpeg_destination_mgr pub; /* public fields */

	u8 *dest;
	int size;
			
	u8 *buffer;
} _destination_mgr;

typedef struct {
	/* libjpg private fields */
	struct jpeg_decompress_struct dinfo;
	struct jpeg_compress_struct cinfo;
	
	custom_error_mgr jerr;

	struct jpeg_source_mgr *jsrc;
	_destination_mgr *jdst;

	JSAMPLE *buffer;
	u8 *data;
	u8 *dest;
} jpgPrivate;

void _src_init_source( j_decompress_ptr cinfo )
{
}

boolean _src_fill_input_buffer( j_decompress_ptr cinfo )
{
	return TRUE;
}

void _src_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
}

boolean _src_resync_to_restart( j_decompress_ptr cinfo, int desired )
{
	return TRUE;
}

void _src_term_source( j_decompress_ptr cinfo )
{
}

//
// jpg_error_exit - this replaces the standard error_exit function
//

void custom_error_exit( j_common_ptr cinfo )
{
	custom_error_mgr *cerr = (custom_error_mgr*) cinfo->err;

	/* Always display the message */
	(*cinfo->err->output_message) (cinfo);

	/* Let the memory manager delete any temp files before we die */
	jpeg_destroy(cinfo);

	// perform a long jump to previously saved environment
	longjmp( cerr->setjmp_buffer, 1 );
}


//
// custom_emit_message - this replaces the standard emit_message function
//

void custom_emit_message( j_common_ptr cinfo, int msg_level )
{
	struct jpeg_error_mgr *err = cinfo->err;

	// this message is harmless, we will just ignore it
	if( cinfo->err->msg_code == JWRN_EXTRANEOUS_DATA )
		return;

	if( msg_level < 0 )
	{	
		/* It's a warning message.  Since corrupt files may generate many warnings,
		 * the policy implemented here is to show only the first warning,
		 * unless trace_level >= 3.
		 */
		if( err->num_warnings == 0 || err->trace_level >= 3 )
			(*err->output_message) (cinfo);
		
		/* Always count warnings in num_warnings. */
		err->num_warnings++;
	} else {
		/* It's a trace message.  Show it if trace_level >= msg_level. */
		if( err->trace_level >= msg_level )
			(*err->output_message) (cinfo);
	}
}

jpgData *jpgOpenRAW( u8 *data, int size, int mode )
{
	struct jpeg_decompress_struct *dinfo;
	jpgPrivate *priv;
	jpgData *jpg;

	jpg = malloc( sizeof(jpgData) );
	if( jpg == NULL )
		return NULL;

	memset( jpg, 0, sizeof(jpgData) );
	
	priv = malloc( sizeof(jpgPrivate) );
	if( priv == NULL )
		return NULL;

	jpg->priv	= priv;
	dinfo		= &priv->dinfo;
  
	/* Initialize the JPEG decompression object with default error handling. */
	dinfo->err	= jpeg_std_error(&priv->jerr.pub);

	// install custom handlers
	dinfo->err->emit_message	= custom_emit_message;
	dinfo->err->error_exit		= custom_error_exit;

	// execution will jump here if an error occurs while processing an jpg
	if( setjmp(priv->jerr.setjmp_buffer) ) {
		jpgClose(jpg);
		return NULL;
	}
	
	jpeg_create_decompress(dinfo);

	/* Specify data source for decompression */
	priv->jsrc->next_input_byte   = data;
	priv->jsrc->bytes_in_buffer   = size;
	priv->jsrc->init_source       = _src_init_source;
	priv->jsrc->fill_input_buffer = _src_fill_input_buffer;
	priv->jsrc->skip_input_data   = _src_skip_input_data;
	priv->jsrc->resync_to_restart = _src_resync_to_restart;
	priv->jsrc->term_source       = _src_term_source;
	dinfo->src = priv->jsrc;

	/* Read file header, set default decompression parameters */
	jpeg_read_header(dinfo, TRUE);

	/* Calculate output image dimensions so we can allocate space */
	jpeg_calc_output_dimensions(dinfo);

	priv->buffer = (*dinfo->mem->alloc_small) 
		((j_common_ptr) dinfo, JPOOL_IMAGE, dinfo->output_width * dinfo->out_color_components);

	/* Start decompressor */
	jpeg_start_decompress(dinfo);

	// ntba2
	if( (mode == JPG_WIDTH_FIX) && (dinfo->output_width % 4) )
		jpg->width = dinfo->output_width - (dinfo->output_width % 4);
	else
		jpg->width  = dinfo->output_width;
	
	jpg->height = dinfo->output_height;
	jpg->bpp    = dinfo->out_color_components * 8;

	/* All done. */
	return( priv->jerr.pub.num_warnings ? NULL : jpg );
}

jpgData *jpgOpen( char *filename, int mode )
{
	jpgData *jpg;
	jpgPrivate *priv;
	u8 *data;
	int size;
	int fd;

	fd = fileXioOpen( filename, O_RDONLY, 0 );
	if( fd == -1 ) {
		printf("jpgOpen: error opening '%s'\n", filename);
		return NULL;
	}
	
	size = fileXioLseek( fd, 0, SEEK_END );
	fileXioLseek( fd, 0, SEEK_SET );
	
	data = (u8*)malloc(size);
	if( data == NULL )
		return NULL;
	
	fileXioRead( fd, data, size );
	fileXioClose(fd);

	jpg = jpgOpenRAW( data, size, mode );
	if( jpg == NULL )
		return NULL;
	
	priv = jpg->priv;
	priv->data = data;

	return jpg;
}

int jpgReadImage( jpgData *jpg, u8 *dest )
{
	JDIMENSION num_scanlines;
	jpgPrivate *priv = jpg->priv;
	int width;
	
	width = jpg->width * priv->dinfo.out_color_components;

	// execution will jump here if an error occurs while processing an jpg
	if( setjmp(priv->jerr.setjmp_buffer) ) {
		jpgClose(jpg);
		return -1;
	}

	/* Process data */
	while( priv->dinfo.output_scanline < priv->dinfo.output_height )
	{
		num_scanlines = jpeg_read_scanlines(&priv->dinfo, &priv->buffer, 1);
		
		if( num_scanlines != 1 )
			break;

		// ntba2
		memcpy( dest, priv->buffer, jpg->width * priv->dinfo.out_color_components );		
		dest += width;
	}

	/* Finish decompression and release memory.
	 * I must do it in this order because output module has allocated memory
	 * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
	 */
	
	jpeg_finish_decompress(&priv->dinfo);
	jpeg_destroy_decompress(&priv->dinfo);

	return( priv->jerr.pub.num_warnings ? -1 : 0 );
}

#define OUTPUT_BUF_SIZE	(64*1024)

void _dest_init_destination( j_compress_ptr cinfo )
{
}

boolean _dest_empty_output_buffer( j_compress_ptr cinfo )
{

	_destination_mgr *dest = (_destination_mgr*) cinfo->dest;

	dest->dest = realloc(dest->dest, dest->size+OUTPUT_BUF_SIZE);
	memcpy(dest->dest+dest->size, dest->buffer, OUTPUT_BUF_SIZE);

	cinfo->dest->next_output_byte = dest->buffer;
	cinfo->dest->free_in_buffer   = OUTPUT_BUF_SIZE;

	dest->size+= OUTPUT_BUF_SIZE;

	return TRUE;
}

void _dest_term_destination( j_compress_ptr cinfo )
{
	_destination_mgr *dest = (_destination_mgr*) cinfo->dest;
	int size = OUTPUT_BUF_SIZE - cinfo->dest->free_in_buffer;

	dest->dest = realloc(dest->dest, dest->size+size);
	memcpy(dest->dest+dest->size, dest->buffer, size);

	dest->size+= size;
}

jpgData *jpgCreateRAW( u8 *data, int width, int height, int bpp )
{
	struct jpeg_compress_struct *cinfo;
	jpgPrivate *priv;
	jpgData *jpg;

	jpg = malloc(sizeof(jpgData));
	if( jpg == NULL )
		return NULL;

	memset(jpg, 0, sizeof(jpgData));
	priv = malloc(sizeof(jpgPrivate));
	if( priv == NULL )
		return NULL;

	jpg->priv = priv;
	cinfo = &priv->cinfo;

	/* Initialize the JPEG compression object with default error handling. */
	cinfo->err = jpeg_std_error(&priv->jerr.pub);
	jpeg_create_compress(cinfo);

	priv->data = data;
	priv->buffer = malloc(OUTPUT_BUF_SIZE);
	if( priv->buffer == NULL )
		return NULL;
	
	priv->jdst->pub.next_output_byte		= priv->buffer;
	priv->jdst->pub.free_in_buffer		= OUTPUT_BUF_SIZE;
	priv->jdst->pub.init_destination		= _dest_init_destination;
	priv->jdst->pub.empty_output_buffer	= _dest_empty_output_buffer;
	priv->jdst->pub.term_destination		= _dest_term_destination;
	priv->jdst->buffer					= priv->buffer;

	/* Specify data source for decompression */
	cinfo->dest = (struct jpeg_destination_mgr *)&priv->jdst;

	cinfo->image_width	= width; 					/* image width and height, in pixels */
	cinfo->image_height = height;
	
	if( bpp == 8 ) {
		cinfo->input_components = 1;				/* # of color components per pixel */
		cinfo->in_color_space	= JCS_GRAYSCALE;	/* colorspace of input image */
	}
	else {
		cinfo->input_components = 3;				/* # of color components per pixel */
		cinfo->in_color_space	= JCS_RGB;			/* colorspace of input image */
	}

	jpeg_set_defaults(cinfo);

	jpeg_set_quality( cinfo, 100, TRUE );
	jpeg_start_compress(cinfo, TRUE);

	jpg->width	= cinfo->image_width;
	jpg->height = cinfo->image_height;
	jpg->bpp	= cinfo->input_components*8;

	/* All done. */
	return( priv->jerr.pub.num_warnings ? NULL : jpg );
}

int jpgCompressImageRAW( jpgData *jpg, u8 **dest )
{
	jpgPrivate *priv = jpg->priv;
	_destination_mgr *_dest = (_destination_mgr*) priv->cinfo.dest;
	int width = priv->cinfo.image_width * priv->cinfo.input_components;
	JSAMPROW row_pointer[1];	/* pointer to a single row */

	_dest->dest = NULL;
	_dest->size = 0;

	while( priv->cinfo.next_scanline < priv->cinfo.image_height )
	{
		row_pointer[0] = & priv->data[priv->cinfo.next_scanline * width];
		jpeg_write_scanlines(&priv->cinfo, row_pointer, 1);
	}
	
	jpeg_finish_compress(&priv->cinfo);
	*dest = _dest->dest;

	return( priv->jerr.pub.num_warnings ? -1 : _dest->size );
}

void jpgClose( jpgData *jpg )
{
	jpgPrivate *priv = jpg->priv;

	if( priv->data )
		free(priv->data);
	
	free(priv);
	free(jpg);
}

#define PS2SS_GSPSMCT32		0
#define PS2SS_GSPSMCT24		1
#define PS2SS_GSPSMCT16		2

int jpgScreenshot( const char* pFilename,unsigned int VramAdress, unsigned int Width, unsigned int Height, unsigned int Psm )
{
	s32 file_handle;
	u32 y;
	static u32 in_buffer[1024*4];  // max 1024*32bit for a line, should be ok
	u8 *out_buffer;
	u8 *p_out;
	jpgData *jpg;
	u8 *data;
	int ret;

	file_handle = fileXioOpen( pFilename, O_CREAT | O_WRONLY, 0 );

	// make sure we could open the file for output
	if( file_handle < 0 )
		return 0;

	out_buffer = malloc( Width * Height * 3 );
	if( out_buffer == NULL )
		return 0;
	
	p_out = out_buffer;

	// Check if we have a tempbuffer, if we do we use it 
	for( y = 0; y < Height; y++ )
	{
		ps2_screenshot( in_buffer, VramAdress, 0, y, Width, 1, Psm );

		if( Psm == PS2SS_GSPSMCT16 )
		{
			u32 x;
			u16* p_in  = (u16*)&in_buffer;
        
			for( x = 0; x < Width; x++ )
			{
				u32 r = (p_in[x] & 31) << 3;
				u32 g = ((p_in[x] >> 5) & 31) << 3;
				u32 b = ((p_in[x] >> 10) & 31) << 3;
				
				p_out[x*3+0] = r;
				p_out[x*3+1] = g;
				p_out[x*3+2] = b;
			}
		}
		else
			if( Psm == PS2SS_GSPSMCT24 )
			{
				u32 x;
				u8* p_in  = (u8*)&in_buffer;
				
				for( x = 0; x < Width; x++ )
				{
					u8 r =  *p_in++;
					u8 g =  *p_in++;
					u8 b =  *p_in++;
					
					p_out[x*3+0] = r;
					p_out[x*3+1] = g;
					p_out[x*3+2] = b;
				}
			}
			else
			{
				u8 *p_in = (u8 *) &in_buffer;
				u32 x;
				
				for(x = 0; x < Width; x++)
				{
					u8 r = *p_in++;
					u8 g = *p_in++;
					u8 b = *p_in++;
					
					*p_in++;
					
					p_out[x*3+0] = r;
					p_out[x*3+1] = g;
					p_out[x*3+2] = b;
				}
			}

			p_out+= Width*3;
	}
	
	jpg = jpgCreateRAW(out_buffer, Width, Height, 24);
	ret = jpgCompressImageRAW(jpg, &data);
	
	fileXioWrite( file_handle, data, ret );
	jpgClose(jpg);

	fileXioClose( file_handle );
	free(out_buffer);

	return 0;
}

/*
 * Marker processor for COM and interesting APPn markers.
 * This replaces the library's built-in processor, which just skips the marker.
 * We want to print out the marker as text, to the extent possible.
 * Note this code relies on a non-suspending data source.
 */

LOCAL(unsigned int)
jpeg_getc (j_decompress_ptr dinfo)
/* Read next byte */
{
  struct jpeg_source_mgr * datasrc = dinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (dinfo))
      ERREXIT(dinfo, JERR_CANT_SUSPEND);
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}

METHODDEF(boolean)
print_text_marker (j_decompress_ptr dinfo)
{
  boolean traceit = (dinfo->err->trace_level >= 1);
  INT32 length;
  unsigned int ch;
  unsigned int lastch = 0;

  length = jpeg_getc(dinfo) << 8;
  length += jpeg_getc(dinfo);
  length -= 2;			/* discount the length word itself */

  if (traceit) {
    if (dinfo->unread_marker == JPEG_COM)
      fprintf(stderr, "Comment, length %ld:\n", (long) length);
    else			/* assume it is an APPn otherwise */
      fprintf(stderr, "APP%d, length %ld:\n",
	      dinfo->unread_marker - JPEG_APP0, (long) length);
  }

  while (--length >= 0) {
    ch = jpeg_getc(dinfo);
    if (traceit) {
      /* Emit the character in a readable form.
       * Nonprintables are converted to \nnn form,
       * while \ is converted to \\.
       * Newlines in CR, CR/LF, or LF form will be printed as one newline.
       */
      if (ch == '\r') {
	fprintf(stderr, "\n");
      } else if (ch == '\n') {
	if (lastch != '\r')
	  fprintf(stderr, "\n");
      } else if (ch == '\\') {
	fprintf(stderr, "\\\\");
      } else if (isprint(ch)) {
	putc(ch, stderr);
      } else {
	fprintf(stderr, "\\%03o", ch);
      }
      lastch = ch;
    }
  }

  if (traceit)
    fprintf(stderr, "\n");

  return TRUE;
}

jpgData *jpgOpenFILE( FILE *in_file, int mode )
{
	struct jpeg_decompress_struct *dinfo;
	jpgPrivate *priv;
	jpgData *jpg;

	jpg = malloc( sizeof(jpgData) );
	if( jpg == NULL )
		return NULL;

	memset( jpg, 0, sizeof(jpgData) );
	
	priv = malloc( sizeof(jpgPrivate) );
	if( priv == NULL )
		return NULL;

	jpg->priv	= priv;
	dinfo		= &priv->dinfo;
  
  /* Install default error handling. */
	dinfo->err	= jpeg_std_error(&priv->jerr.pub);
	dinfo->err->emit_message	= custom_emit_message;
	dinfo->err->error_exit		= custom_error_exit;

	/* Execution will jump here if an error occurs while processing an jpg */
	if( setjmp(priv->jerr.setjmp_buffer) ) {
		jpgClose(jpg);
		return NULL;
	}
	
  /* Initialize the JPEG decompression. */
	jpeg_create_decompress(dinfo);

  /* Insert custom marker processor for COM and APP12.
   * APP12 is used by some digital camera makers for textual info,
   * so we provide the ability to display it as text.
   * If you like, additional APPn marker types can be selected for display,
   * but don't try to override APP0 or APP14 this way (see libjpeg.doc).
   */
  jpeg_set_marker_processor(dinfo, JPEG_COM, print_text_marker);
  jpeg_set_marker_processor(dinfo, JPEG_APP0+12, print_text_marker);

  /* Specify data source for decompression */
  jpeg_stdio_src(dinfo, in_file);
	priv->jsrc = dinfo->src;

  /* Read file header, set default decompression parameters */
	jpeg_read_header(dinfo, TRUE);

	/* Calculate output image dimensions so we can allocate space */
	jpeg_calc_output_dimensions(dinfo);

	priv->buffer = (*dinfo->mem->alloc_small) 
		((j_common_ptr) dinfo, JPOOL_IMAGE, dinfo->output_width * dinfo->out_color_components);

  /* Start decompressor */
	jpeg_start_decompress(dinfo);

	// ntba2
	if( (mode == JPG_WIDTH_FIX) && (dinfo->output_width % 4) )
		jpg->width = dinfo->output_width - (dinfo->output_width % 4);
	else
		jpg->width  = dinfo->output_width;
	
	jpg->height = dinfo->output_height;
	jpg->bpp    = dinfo->out_color_components * 8;

	/* All done. */
	return( priv->jerr.pub.num_warnings ? NULL : jpg );
}
