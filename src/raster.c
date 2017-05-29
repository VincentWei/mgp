/*
 *  Copyright (C) 2002-2005 Feynman Software.
 */
#include "mgpconfig.h"
#include "raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>


/*
 * 'RasterClose()' - Close a raster stream.
 */

void
RasterClose(PrinterRaster *r)	/* I - Stream to close */
{
  if (r != NULL)
    free(r);
}


/*
 * 'RasterOpen()' - Open a raster stream.
 */

PrinterRaster *				/* O - New stream */
RasterOpen(int         fd,		/* I - File descriptor */
               PrinterMode mode)	/* I - Mode */
{
  PrinterRaster	*r;			/* New stream */


  if ((r = (PrinterRaster*)calloc(sizeof(PrinterRaster), 1)) == NULL)
    return (NULL);

  r->fd   = fd;
  r->mode = mode;

  if (mode == RASTER_READ)
  {
   /*
    * Open for read - get sync word...
    */

    if (read(fd, (char*)&(r->sync), sizeof(r->sync)) < (int)sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }

    if (r->sync != RASTER_SYNC &&
        r->sync != RASTER_REVSYNC)
    {
      free(r);
      return (NULL);
    }
  }
  else
  {
   /*
    * Open for write - put sync word...
    */

    r->sync = RASTER_SYNC;
    if (write(fd, (char*)&(r->sync), sizeof(r->sync)) < (int)sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }
  }

  return (r);
}


/*
 * 'RasterReadHeader()' - Read a raster page header.
 */

unsigned					/* O - 1 on success, 0 on fail */
RasterReadHeader(PrinterRaster      *r,	/* I - Raster stream */
                     PrinterPageHeader *h)	/* I - Pointer to header data */
{
  int		len;				/* Number of words to swap */
  union swap_s					/* Swapping structure */
  {
    unsigned char	b[4];
    unsigned		v;
  }		*s;


  if (r == NULL || r->mode != RASTER_READ)
    return (0);

  if (RasterReadPixels(r, (unsigned char *)h, sizeof(PrinterPageHeader)) <
          sizeof(PrinterPageHeader))
    return (0);

  if (r->sync == RASTER_REVSYNC)
    for (len = (sizeof(PrinterPageHeader) - 256) / 4,
             s = (union swap_s *)&(h->AdvanceDistance);
	 len > 0;
	 len --, s ++)
      s->v = (((((s->b[0] << 8) | s->b[1]) << 8) | s->b[2]) << 8) | s->b[3];

  return (1);
}


/*
 * 'RasterReadPixels()' - Read raster pixels.
 */

unsigned				/* O - Number of bytes read */
RasterReadPixels(PrinterRaster *r,	/* I - Raster stream */
                     unsigned char *p,	/* I - Pointer to pixel buffer */
		     unsigned      len)	/* I - Number of bytes to read */
{
  int		bytes;			/* Bytes read */
  unsigned	remaining;		/* Bytes remaining */


  if (r == NULL || r->mode != RASTER_READ)
    return (0);

  remaining = len;

  while (remaining > 0)
  {
    bytes = read(r->fd, (char*)p, remaining);

    if (bytes == 0)
      return (0);
    else if (bytes < 0)
    {
      if (errno != EINTR)
        return (0);
      else
        continue;
    }

    remaining -= bytes;
    p += bytes;
  }

  return (len);
}


/*
 * 'RasterWriteHeader()' - Write a raster page header.
 */
 
unsigned
RasterWriteHeader(PrinterRaster *r, PrinterPageHeader *h)
{
  if (r == NULL || r->mode != RASTER_WRITE)
    return (0);

  return (RasterWritePixels(r, (unsigned char *)h,
                                sizeof(PrinterPageHeader)) ==
              sizeof(PrinterPageHeader));
}


/*
 * 'RasterWritePixels()' - Write raster pixels.
 */

unsigned				/* O - Number of bytes written */
RasterWritePixels(PrinterRaster *r,	/* I - Raster stream */
                      unsigned char *p,	/* I - Bytes to write */
		      unsigned      len)/* I - Number of bytes to write */
{
  int		bytes;			/* Bytes read */
  unsigned	remaining;		/* Bytes remaining */

  if (r == NULL || r->mode != RASTER_WRITE)
    return (0);

  remaining = len;

  while (remaining > 0)
  {
    bytes = write(r->fd, (char*)p, remaining);

    if (bytes == 0)
      return (0);
    else if (bytes < 0)
    {
      if (errno != EINTR)
        return (0);
      else
        continue;
    }

    remaining -= bytes;
    p += bytes;
  }

  return (len);
}


/*
 * End of "$Id: raster.c,v 1.5 2007-11-21 07:54:35 jpzhang Exp $".
 */
