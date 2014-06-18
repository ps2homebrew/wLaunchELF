/*
  _____     ___ ____ 
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C) 2003 Marcus R. Brown (mrbrown@0xd6.org)
  ------------------------------------------------------------------------
  ioman.h
*/

#ifndef PS2LIB_IOP_IOMAN_H
#define PS2LIB_IOP_IOMAN_H

/* Compatibility with older fileio.h - This stuff is deprecated, it will be
   removed in the next major release! */

#include <tamtypes.h>

struct fileio_driver
{
   u8				*device;
   u32				xx1;			/*  always 16? */
   u32				version;		/*  1 */
   u8				*description;
   void				**function_list;
};

/* function list index */
enum 
{
   FIO_INITIALIZE,		/* initialize(struct fileio_driver *); */
   FIO_SHUTDOWN,		/* shutdown(struct fileio_driver *) */
   FIO_FORMAT,			/* format( int kernel_fd, char *name); */
   FIO_OPEN,			/* open( int kernel_fd, char *name, int mode); */
   FIO_CLOSE,			/* close( int kernel_fd); */
   FIO_READ,			/* read( int kernel_fd, void *ptr, int size); */
   FIO_WRITE,			/* write( int kernel_fd, void *ptr, int size); */
   FIO_SEEK,			/* seek( int kernel_fd, u32 pos, int mode); */
   FIO_IOCTL,			/* ioctl( int kernel_fd, u32 type, void *value); */
   FIO_REMOVE,			/* remove( int kernel_fd, char *name); */
   FIO_MKDIR,			/* mkdir( int kernel_fd, char *name); */
   FIO_RMDIR,			/* rmdir( int kernel_fd, char *name); */
   FIO_DOPEN,			/* dopen( int kernel_fd, char *name); */
   FIO_DCLOSE,			/* dclose( int kernel_fd); */
   FIO_DREAD,			/* dread( int kernel_fd, void *ptr); */
   FIO_GETSTAT,			/* getstat( int kernel_fd, char *name, void *buf); it is not void*, but a structure */
   FIO_CHSTAT			/* chstat( int kernel_fd, char *name, void *buf, unsigned int mask); */
};

int FILEIO_add( struct fileio_driver *driver);
int FILEIO_del( u8 *device);

#endif /* PS2LIB_IOP_IOMAN_H */
