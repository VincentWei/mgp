/*
 * "$Id: rastertoepson.c,v 1.25 2007-12-04 11:12:12 jpzhang Exp $"
 *
 *   GIMP-print based raster filter for the Common UNIX Printing System.
 *
 *   Copyright 1993-2003 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License,
 *   version 2, as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: -info@.org
 *         WWW: http://www..org
 *
 * Contents:
 *
 *   main()                    - Main entry and processing of driver.
 *   _writefunc()          - Write data to a file...
 *   cancel_job()              - Cancel the current job...
 *   Image_bpp()               - Return the bytes-per-pixel of an image.
 *   Image_get_appname()       - Get the application we are running.
 *   Image_get_row()           - Get one row of the image.
 *   Image_height()            - Return the height of an image.
 *   Image_init()              - Initialize an image.
 *   Image_note_progress()     - Notify the user of our progress.
 *   Image_progress_conclude() - Close the progress display.
 *   Image_progress_init()     - Initialize progress display.
 *   Image_rotate_ccw()        - Rotate the image counter-clockwise
 *                               (unsupported).
 *   Image_width()             - Return the width of an image.
 */

/*
 * Include necessary headers...
 */

#include "mgpconfig.h"
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include "common.h"
#include "epson.h"
#include "epson_i.h"
#include "ppd.h"
#include "raster.h"

#ifdef NOUNIX
#include "ioLib.h"
#endif

/* Solaris with gcc has problems because gcc's limits.h doesn't #define */
/* this */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/*
 * Structure for page raster data...
 */

typedef struct
{
  PrinterRaster		*ras;		/* Raster stream to read from */
  int			page;		/* Current page number */
  int			row;		/* Current row number */
  int			left;
  int			right;
  int			bottom;
  int			top;
  int			width;
  int			height;
  PrinterPageHeader	header;		/* Page header from file */
} raster_image_t;

static void	writefunc(void *file, const char *buf, size_t bytes);
static void	cancel_job(int sig);
static const char *Image_get_appname(stp_image_t *image);
static void	 Image_progress_conclude(stp_image_t *image);
static void	Image_note_progress(stp_image_t *image,
				    double current, double total);
static void	Image_progress_init(stp_image_t *image);
static stp_image_status_t Image_get_row(stp_image_t *image,
					unsigned char *data, int row);
static int	Image_height(stp_image_t *image);
static int	Image_width(stp_image_t *image);
static int	Image_bpp(stp_image_t *image);
static void	Image_rotate_180(stp_image_t *image);
static void	Image_rotate_cw(stp_image_t *image);
static void	Image_rotate_ccw(stp_image_t *image);
static void	Image_init(stp_image_t *image);

static stp_image_t theImage =
{
  Image_init,
  NULL,				/* reset */
  NULL,				/* transpose */
  NULL,				/* hflip */
  NULL,				/* vflip */
  NULL,				/* crop */
  Image_rotate_ccw,
  Image_rotate_cw,
  Image_rotate_180,
  Image_bpp,
  Image_width,
  Image_height,
  Image_get_row,
  Image_get_appname,
  Image_progress_init,
  Image_note_progress,
  Image_progress_conclude,
  NULL
};

static volatile stp_image_status_t Image_status;

/*
 * 'main()' - Main entry and processing of driver.
 */
#ifdef NOUNIX
int epson65_rasterentry(int in, int out)
#else
int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
#endif
{
  raster_image_t		rass;		/* CUPS image */
  ppd_file_t		*ppd;		/* PPD file */
  ppd_option_t		*option;	/* PPD option */
  stp_printer_t		printer;	/* Printer driver */
  stp_vars_t		v;		/* Printer driver variables */
  stp_papersize_t	size;		/* Paper size */
  char			*buffer;	/* Overflow buffer */
  int			num_res;	/* Number of printer resolutions */
  stp_param_t		*res;		/* Printer resolutions */
  float			stp_gamma,	/* STP options */
			stp_brightness,
			stp_cyan,
			stp_magenta,
			stp_yellow,
			stp_contrast,
			stp_saturation,
			stp_density;



 /*
  * Initialise libgimpprint
  */

  theImage.rep = &rass;

  stp_init();


  Image_status = STP_IMAGE_OK;

 /*
  * Get the PPD file...
  */

  if ((ppd = ppdOpenFile(PATH_DRIVER"/etc/epson-c64.ppd")) == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: Unable to load PPD file!\n");
    return (1);
  }

  if (ppd->modelname == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: No ModelName attribute in PPD file!\n");
    ppdClose(ppd);
    return (1);
  }

 /*
  * Get the STP options, if any...
  */
  if ((option = ppdFindOption(ppd, "stpGamma")) != NULL)
    stp_gamma = atof(option->defchoice) * 0.001;
  else
    stp_gamma = 0.688;

  if ((option = ppdFindOption(ppd, "stpBrightness")) != NULL)
    stp_brightness = atof(option->defchoice) * 0.001;
  else
    stp_brightness = 1.0;

  if ((option = ppdFindOption(ppd, "stpCyan")) != NULL)
    stp_cyan = atof(option->defchoice) * 0.001;
  else
    stp_cyan = 0.96;

  if ((option = ppdFindOption(ppd, "stpMagenta")) != NULL)
    stp_magenta = atof(option->defchoice) * 0.001;
  else
    stp_magenta = 1.02;

  if ((option = ppdFindOption(ppd, "stpYellow")) != NULL)
    stp_yellow = atof(option->defchoice) * 0.001;
  else
    stp_yellow = 1.02;

  if ((option = ppdFindOption(ppd, "stpContrast")) != NULL)
    stp_contrast = atof(option->defchoice) * 0.001;
  else
    stp_contrast = 1.0;

  if ((option = ppdFindOption(ppd, "stpSaturation")) != NULL)
    stp_saturation = atof(option->defchoice) * 0.001;
  else
    stp_saturation = 1.0;

  if ((option = ppdFindOption(ppd, "stpDensity")) != NULL)
    stp_density = atof(option->defchoice) * 0.001;
  else
    stp_density = 1.0;
 /*
  * Figure out which driver to use...
  */

  if ((printer = stp_get_printer_by_long_name(ppd->modelname)) == NULL)
    if ((printer = stp_get_printer_by_driver(ppd->modelname)) == NULL)
    {
      fprintf(stderr, "ERROR: Fatal error: Unable to find driver named \"%s\"!\n",
              ppd->modelname);
      ppdClose(ppd);
      return (1);
    }

  ppdClose(ppd);

 /*
  * Get the resolution options...
  */

  res = stp_printer_get_printfuncs(printer)->parameters(printer, NULL,
                                                        "Resolution", &num_res);

 /*
  * Open the page stream...
  */
#ifdef NOUNIX
  rass.ras = RasterOpen(in, RASTER_READ);
#else
  rass.ras = RasterOpen(0, RASTER_READ);
#endif

 /*
  * Setup default print variables...
  */

  v = stp_allocate_copy(stp_printer_get_printvars(printer));

  stp_set_scaling(v, 0); /* No scaling */
  stp_set_cmap(v, NULL);
  stp_set_left(v, 0);
  stp_set_top(v, 0);
  stp_set_orientation(v, ORIENT_PORTRAIT);
  stp_set_outfunc(v, writefunc);
  stp_set_errfunc(v, writefunc);
#ifdef NOUNIX
  stp_set_outdata(v, (void*)out);
  stp_set_errdata(v, (void*)2);
#else
  stp_set_outdata(v, stdout);
  stp_set_errdata(v, stderr);
#endif
  stp_set_job_mode(v, STP_JOB_MODE_JOB);

 /*
  * Process pages as needed...
  */

  rass.page = 0;


  if  (RasterReadHeader(rass.ras, &rass.header))
  {
   /*
    * Update the current page...
    */

    rass.row = 0;

    fprintf(stderr, "PAGE: %d 1\n", rass.page + 1);
    /* use 1-based page logging */

   /*
    * Setup printer driver variables...
    */

    stp_set_page_width(v, rass.header.PageSize[0]);
    stp_set_page_height(v, rass.header.PageSize[1]);
    stp_set_image_type(v, rass.header.RowCount);

    switch (rass.header.ColorSpace)
    {
      case CSPACE_W :
	  stp_set_output_type(v, OUTPUT_GRAY);
	  break;
      case CSPACE_K :
	  stp_set_output_type(v, OUTPUT_MONOCHROME);
	  break;
      case CSPACE_RGB :
	  stp_set_output_type(v, OUTPUT_COLOR);
	  break;
      case CSPACE_CMYK :
	  stp_set_output_type(v, OUTPUT_RAW_CMYK);
	  break;
      default :
	  fprintf(stderr, "ERROR: Bad colorspace %d!",
		  rass.header.ColorSpace);
	  break;
    }

    if (rass.header.RowStep >= stp_dither_algorithm_count())
      fprintf(stderr, "ERROR: Unable to set dither algorithm!\n");
    else
      stp_set_dither_algorithm(v,
			       stp_dither_algorithm_name(rass.header.RowStep));

    if (rass.header.MediaClass && strlen(rass.header.MediaClass) > 0)
      stp_set_media_source(v, rass.header.MediaClass);
    if (rass.header.MediaType && strlen(rass.header.MediaType) > 0)
      stp_set_media_type(v, rass.header.MediaType);
    if (rass.header.OutputType && strlen(rass.header.OutputType) > 0)
      stp_set_ink_type(v, rass.header.OutputType);

    fprintf(stderr, "DEBUG: PageSize = %dx%d\n", rass.header.PageSize[0],
	    rass.header.PageSize[1]);

    if ((size = stp_get_papersize_by_size(rass.header.PageSize[1],
					  rass.header.PageSize[0])) != NULL)
      stp_set_media_size(v, stp_papersize_get_name(size));
    else
      fprintf(stderr, "ERROR: Unable to get media size!\n");

    if (rass.header.Compression >= num_res)
      fprintf(stderr, "ERROR: Unable to set printer resolution!\n");
    else
      stp_set_resolution(v, res[rass.header.Compression].name);

    stp_set_app_gamma(v, 1.0);
    stp_set_brightness(v, stp_brightness);
    stp_set_contrast(v, stp_contrast);
    stp_set_cyan(v, stp_cyan);
    stp_set_magenta(v, stp_magenta);
    stp_set_yellow(v, stp_yellow);
    stp_set_saturation(v, stp_saturation);
    stp_set_density(v, stp_density);
    stp_set_gamma(v, stp_gamma);
    stp_merge_printvars(v, stp_printer_get_printvars(printer));
    stp_set_page_number(v, rass.page);

    (*stp_printer_get_printfuncs(printer)->media_size)
      (printer, v, &(rass.width), &(rass.height));
    (*stp_printer_get_printfuncs(printer)->imageable_area)
      (printer, v, &(rass.left), &(rass.right), &(rass.bottom), &(rass.top));
    rass.right = rass.width - rass.right;
    rass.width = rass.width - rass.left - rass.right;
    rass.width = rass.header.HWResolution[0] * rass.width / 72;
    rass.left = rass.header.HWResolution[0] * rass.left / 72;
    rass.right = rass.header.HWResolution[0] * rass.right / 72;

    rass.top = rass.height - rass.top;
    rass.height = rass.height - rass.top - rass.bottom;
    rass.height = rass.header.HWResolution[1] * rass.height / 72;
    rass.top = rass.header.HWResolution[1] * rass.top / 72;
    rass.bottom = rass.header.HWResolution[1] * rass.bottom / 72;

   /*
    * Print the page...
    */

    if (stp_printer_get_printfuncs(printer)->verify(printer, v))
    {
      signal(SIGTERM, cancel_job);
      if (rass.page == 0)
	stp_printer_get_printfuncs(printer)->start_job(printer, &theImage, v);
      stp_printer_get_printfuncs(printer)->print(printer, &theImage, v);
#ifdef NOUNIX
      ioctl(out, FIOFLUSH, 0);
#else
      fflush(stdout);
#endif
    }
    else
      fputs("ERROR: Invalid printer settings!\n", stderr);

   /*
    * Purge any remaining bitmap data...
    */
    if (rass.row < (int)rass.header.Height)
    {
      if ((buffer = (char*)malloc(rass.header.BytesPerLine+1)) == NULL)
        goto abort;//break;


      while (rass.row < (int)rass.header.Height)
      {
        RasterReadPixels(rass.ras, (unsigned char *)buffer,
	                     rass.header.BytesPerLine);
	rass.row ++;
      }
    }
    rass.page ++;
  }
abort:

  if (rass.page > 0)
    stp_printer_get_printfuncs(printer)->end_job(printer, &theImage, v);

  stp_free_vars(v);

 /*
  * Close the raster stream...
  */

  RasterClose(rass.ras);

 /*
  * If no pages were printed, send an error message...
  */

  if (rass.page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: MiniGUI Ready to print.\n", stderr);


  return (rass.page == 0);
}


/*
 * '_writefunc()' - Write data to a file...
 */
#ifdef NOUNIX
static void
writefunc(void *file, const char *buf, size_t bytes)
{
  int prn = (int)file;
  write(prn, (char*)buf, bytes);
}
#else
static void
writefunc(void *file, const char *buf, size_t bytes)
{
  FILE *prn = (FILE *)file;
  fwrite(buf, 1, bytes, prn);
}
#endif


/*
 * 'cancel_job()' - Cancel the current job...
 */

void
cancel_job(int sig)			/* I - Signal */
{
  (void)sig;

  Image_status = STP_IMAGE_ABORT;
}


/*
 * 'Image_bpp()' - Return the bytes-per-pixel of an image.
 */

static int				/* O - Bytes per pixel */
Image_bpp(stp_image_t *image)		/* I - Image */
{
  raster_image_t	*rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return (0);

 /*
  * For now, we only support RGB and grayscale input from the
  * raster filters.
  */

  switch (rass->header.ColorSpace)
  {
    default :
        return (1);
    case CSPACE_RGB :
        return (3);
    case CSPACE_CMYK :
        return (4);
  }
}


/*
 * 'Image_get_appname()' - Get the application we are running.
 */

static const char *				/* O - Application name */
Image_get_appname(stp_image_t *image)		/* I - Image */
{
  (void)image;

  return ("CUPS 1.1.x driver based on GIMP-print");
}


/*
 * 'Image_get_row()' - Get one row of the image.
 */

static void
throwaway_data(int amount, raster_image_t * rass)
{
  unsigned char trash[4096];	/* Throwaway */
  int block_count = amount / 4096;
  int leftover = amount % 4096;
  while (block_count > 0)
    {
      RasterReadPixels(rass->ras, trash, 4096);
      block_count--;
    }
  if (leftover)
    RasterReadPixels(rass->ras, trash, leftover);
}

stp_image_status_t
Image_get_row(stp_image_t   *image,	/* I - Image */
	      unsigned char *data,	/* O - Row */
	      int           row)	/* I - Row number (unused) */
{
  raster_image_t	*rass;			/* CUPS image */
  int		i;			/* Looping var */
  int 		bytes_per_line;
  int		margin;
  unsigned char *orig = data;
  static int warned = 0;


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return STP_IMAGE_ABORT;
  bytes_per_line = rass->width * rass->header.BitsPerPixel / CHAR_BIT;
  margin = rass->header.BytesPerLine - bytes_per_line;

  if (rass->row < rass->header.Height)
  {
    RasterReadPixels(rass->ras, data, bytes_per_line);
    rass->row ++;
    if (margin)
      throwaway_data(margin, rass);

   /*
    * Invert black data for monochrome output...
    */

    if (rass->header.ColorSpace == CSPACE_K)
      for (i = bytes_per_line; i > 0; i --, data ++)
        *data = ((1 << CHAR_BIT) - 1) - *data;
  }
  else
    {
      if (rass->header.ColorSpace == CSPACE_CMYK)
	memset(data, 0, bytes_per_line);
      else
	memset(data, ((1 << CHAR_BIT) - 1), bytes_per_line);
    }

  /*
   * This exists to print non-ADSC input which has messed up the job
   * input, such as that generated by psnup.
   */
  data = orig;
  if (rass->header.BitsPerPixel == 1)
    {
      if (warned == 0)
            {
              fprintf(stderr,
                  "WARNING: GIMP-PRINT detected broken job options.  "
                  "Output quality is degraded.  Are you using psnup or non-ADSC PostScript?\n");
              warned = 1;
            }
      for (i =rass->width - 1; i >= 0; i--)
            {
              if ( (data[i/8] >> (7 - i%8)) &0x1)
                data[i]=255;
              else
                data[i]=0;
            }
    }

  return Image_status;
}


/*
 * 'Image_height()' - Return the height of an image.
 */

static int				/* O - Height in pixels */
Image_height(stp_image_t *image)	/* I - Image */
{
  raster_image_t	*rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return (0);

  fprintf(stderr, "DEBUG: GIMP-PRINT: Image_height %d\n", rass->height);
  return (rass->height);
}


/*
 * 'Image_init()' - Initialize an image.
 */

static void
Image_init(stp_image_t *image)		/* I - Image */
{
  (void)image;
}


/*
 * 'Image_note_progress()' - Notify the user of our progress.
 */

void
Image_note_progress(stp_image_t *image,	/* I - Image */
		    double current,	/* I - Current progress */
		    double total)	/* I - Maximum progress */
{
  raster_image_t	*rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Printing page %d, %.0f%%\n",
         rass ->page +1, 100.0 * current / total);
    /* ->page + 1 because users expect 1-based counting */
}


/*
 * 'Image_progress_conclude()' - Close the progress display.
 */

static void
Image_progress_conclude(stp_image_t *image)	/* I - Image */
{
  raster_image_t	*rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Finished page %d...\n",rass ->page + 1);
  /* ->page + 1 because users expect 1-based counting */
}


/*
 * 'Image_progress_init()' - Initialize progress display.
 */

static void
Image_progress_init(stp_image_t *image)/* I - Image */
{
  raster_image_t	* rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Starting page %d...\n",rass->page + 1);
  /* ->page + 1 because users expect 1-based counting */
}


/*
 * 'Image_rotate_180()' - Rotate the image 180 degrees (unsupported).
 */

static void
Image_rotate_180(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_ccw()' - Rotate the image counter-clockwise (unsupported).
 */

static void
Image_rotate_ccw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_cw()' - Rotate the image clockwise (unsupported).
 */

static void
Image_rotate_cw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_width()' - Return the width of an image.
 */

static int				/* O - Width in pixels */
Image_width(stp_image_t *image)	/* I - Image */
{
  raster_image_t	*rass;		/* CUPS image */


  if ((rass = (raster_image_t *)(image->rep)) == NULL)
    return (0);

  fprintf(stderr, "DEBUG: GIMP-PRINT: Image_width %d\n", rass->width);
  return (rass->width);
}


/*
 * End of "$Id: rastertoepson.c,v 1.25 2007-12-04 11:12:12 jpzhang Exp $".
 */
