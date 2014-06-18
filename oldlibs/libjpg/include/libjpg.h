#ifndef __LIBJPG_H__
#define __LIBJPG_H__

#include <tamtypes.h>

// ntba2
//
// JPG_WIDTH_FIX ensures Width is a multiple of four.
//
#define JPG_NORMAL		0x00
#define	JPG_WIDTH_FIX	0x01

typedef struct {
	int width;
	int height;
	int bpp;

	void *priv;
} jpgData;

jpgData *jpgOpen( char *filename, int mode );
jpgData *jpgOpenRAW( u8 *data, int size, int mode );
jpgData *jpgOpenFILE( FILE *in_file, int mode );
jpgData *jpgCreate(char *filename, u8 *data, int width, int height, int bpp);
jpgData *jpgCreateRAW(u8 *data, int width, int height, int bpp);
int      jpgCompressImage(jpgData *jpg);
int      jpgCompressImageRAW(jpgData *jpg, u8 **dest);
int      jpgReadImage(jpgData *jpg, u8 *dest);
void     jpgClose(jpgData *jpg);
int      jpgScreenshot( const char* pFilename,unsigned int VramAdress, unsigned int Width, unsigned int Height, unsigned int Psm );

#endif /* __LIBJPG_H__ */
