
#include "mgpconfig.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "ppd.h"
#include "raster.h"

#ifdef NOUNIX
#include "ioLib.h"
#endif

/*
 * Macros...
 */
#ifdef NOUNIX
int global_out;
#define write_byte(byte) \
	do{ \
	char ob = byte; \
	write (out, &ob, 1); \
	}while(0) 
void write_printf(const char * format, ...) 
{
	int bytes; 
	char buf[64]; 
	va_list args; 
	va_start(args, format); 
	bytes = vsnprintf(buf, 64, format, args); 
	va_end(args); 
	write (global_out, buf, bytes);
}
#define write_out(buf, len) write (global_out, buf, len) 
#else
#define write_byte(byte) putchar(byte)
#define write_printf printf
#define write_out(buf, len)  fwrite(buf, len, 1, stdout)
#endif


static unsigned char	*Planes[4],		/* Output buffers */
		*CompBuffer,		/* Compression buffer */
		*BitBuffer;		/* Buffer for output bits */

static int NumPlanes,      /* Number of color planes */
        ColorBits,      /* Number of bits per color */
        Feed,           /* Number of lines to skip */
        Duplex,         /* Current duplex mode */
        Page;           /* Current page number */


static void HP_CompressData(unsigned char *line,	/* I - Data to compress */
                     int           length,	/* I - Number of bytes */
                     int           plane,	/* I - Color plane */
                     int           type);	/* I - Type of compression */
static void HP_CancelJob(int sig);			/* I - Signal */
static void HP_EndPage(int);
static void HP_Shutdown(int);

/*
 * 'Setup()' - Prepare the printer for printing.
 */
static void Setup(int out)
{
	write_byte(0x1b);
        write_byte('E');
}


/*
 * 'HP_StartPage()' - Start a page of graphics.
 */

static void
HP_StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          PrinterPageHeader *header, int out)	/* I - Page header */
{
  int	plane;				/* Looping var */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Register a signal handler to eject the current page if the
  * job is cancelled.
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, HP_CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = HP_CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, HP_CancelJob);
#endif /* HAVE_SIGSET */

 /*
  * Setup printer/job attributes...
  */

  Duplex    = header->Duplex;
  ColorBits = header->BitsPerColor;

  if ((!Duplex || (Page & 1)) && header->MediaPosition)
    write_printf("\033&l%dH",				/* Set media position */
           header->MediaPosition);

  if (Duplex && ppd && ppd->model_number == 2)
  {
   /*
    * Handle duplexing on new DeskJet printers...
    */

    write_printf("\033&l-2H");			/* Load media */

    if (Page & 1)
      write_printf("\033&l2S");			/* Set duplex mode */
  }

  if (!Duplex || (Page & 1) || (ppd && ppd->model_number == 2))
  {
   /*
    * Set the media size...
    */

    write_printf("\033&l6D\033&k12H");		/* Set 6 LPI, 10 CPI */
    write_printf("\033&l0O");				/* Set portrait orientation */

    switch (header->PageSize[1])
    {
      case 540 : /* Monarch Envelope */
          write_printf("\033&l80A");			/* Set page size */
	  break;

      case 624 : /* DL Envelope */
          write_printf("\033&l90A");			/* Set page size */
	  break;

      case 649 : /* C5 Envelope */
          write_printf("\033&l91A");			/* Set page size */
	  break;

      case 684 : /* COM-10 Envelope */
          write_printf("\033&l81A");			/* Set page size */
	  break;

      case 709 : /* B5 Envelope */
          write_printf("\033&l100A");			/* Set page size */
	  break;

      case 756 : /* Executive */
          write_printf("\033&l1A");			/* Set page size */
	  break;

      case 792 : /* Letter */
          write_printf("\033&l2A");			/* Set page size */
	  break;

      case 842 : /* A4 */
          write_printf("\033&l26A");			/* Set page size */
	  break;

      case 1008 : /* Legal */
          write_printf("\033&l3A");			/* Set page size */
	  break;

      case 1191 : /* A3 */
          write_printf("\033&l27A");			/* Set page size */
	  break;

      case 1224 : /* Tabloid */
          write_printf("\033&l6A");			/* Set page size */
	  break;
    }

    write_printf("\033&l%dP",				/* Set page length */
           header->PageSize[1] / 12);
    write_printf("\033&l0E");				/* Set top margin to 0 */
  }

  if (!Duplex || (Page & 1))
  {
   /*
    * Set other job options...
    */

    write_printf("\033&l%dX", header->NumCopies);	/* Set number copies */

    if (header->PageMediaType &&
        (!ppd || ppd->model_number != 2 || header->HWResolution[0] == 600))
      write_printf("\033&l%dM",			/* Set media type */
             header->PageMediaType);

    if (!ppd || ppd->model_number != 2)
    {
      if (header->Duplex)
	write_printf("\033&l%dS",			/* Set duplex mode */
               header->Duplex + header->Tumble);

      write_printf("\033&l0L");			/* Turn off perforation skip */
    }
  }
  else if (!ppd || ppd->model_number != 2)
    write_printf("\033&a2G");				/* Set back side */

 /*
  * Set graphics mode...
  */

  if (ppd->model_number == 2)
  {
   /*
    * Figure out the number of color planes...
    */

    if (header->ColorSpace == CSPACE_KCMY)
      NumPlanes = 4;
    else
      NumPlanes = 1;

   /*
    * Set the resolution and top-of-form...
    */

    write_printf("\033&u%dD", header->HWResolution[0]);
						/* Resolution */
    write_printf("\033&l0e0L");			/* Reset top and don't skip */
    write_printf("\033*p0Y\033*p0X");			/* Set top of form */

   /*
    * Send 26-byte configure image data command with horizontal and
    * vertical resolutions as well as a color count...
    */

    write_printf("\033*g26W");
    write_byte(2);					/* Format 2 */
    write_byte(NumPlanes);				/* Output planes */

    write_byte(header->HWResolution[0] >> 8);	/* Black resolution */
    write_byte(header->HWResolution[0]);
    write_byte(header->HWResolution[1] >> 8);
    write_byte(header->HWResolution[1]);
    write_byte(0);
    write_byte(1 << ColorBits);			/* # of black levels */

    write_byte(header->HWResolution[0] >> 8);	/* Cyan resolution */
    write_byte(header->HWResolution[0]);
    write_byte(header->HWResolution[1] >> 8);
    write_byte(header->HWResolution[1]);
    write_byte(0);
    write_byte(1 << ColorBits);			/* # of cyan levels */

    write_byte(header->HWResolution[0] >> 8);	/* Magenta resolution */
    write_byte(header->HWResolution[0]);
    write_byte(header->HWResolution[1] >> 8);
    write_byte(header->HWResolution[1]);
    write_byte(0);
    write_byte(1 << ColorBits);			/* # of magenta levels */

    write_byte(header->HWResolution[0] >> 8);	/* Yellow resolution */
    write_byte(header->HWResolution[0]);
    write_byte(header->HWResolution[1] >> 8);
    write_byte(header->HWResolution[1]);
    write_byte(0);
    write_byte(1 << ColorBits);			/* # of yellow levels */

    write_printf("\033&l0H");				/* Set media position */
  }
  else
  {
    write_printf("\033*t%dR", header->HWResolution[0]);
						/* Set resolution */

    if (header->ColorSpace == CSPACE_KCMY)
    {
      NumPlanes = 4;
      write_printf("\033*r-4U");			/* Set KCMY graphics */
    }
    else if (header->ColorSpace == CSPACE_CMY)
    {
      NumPlanes = 3;
      write_printf("\033*r-3U");			/* Set CMY graphics */
    }
    else
      NumPlanes = 1;				/* Black&white graphics */

   /*
    * Set size and position of graphics...
    */

    write_printf("\033*r%dS", header->Width);	/* Set width */
    write_printf("\033*r%dT", header->Height);	/* Set height */

    write_printf("\033&a0H");				/* Set horizontal position */

    if (ppd)
      write_printf("\033&a%.0fV", 			/* Set vertical position */
             10.0 * (ppd->sizes[0].length - ppd->sizes[0].top));
    else
      write_printf("\033&a0V");			/* Set top-of-page */
  }

  write_printf("\033*r1A");				/* Start graphics */

  if (header->Compression)
    write_printf("\033*b%dM",				/* Set compression */
           header->Compression);

  Feed = 0;					/* No blank lines yet */

 /*
  * Allocate memory for a line of graphics...
  */

  Planes[0] = (unsigned char*)malloc(header->BytesPerLine);
  for (plane = 1; plane < NumPlanes; plane ++)
    Planes[plane] = Planes[0] + plane * header->BytesPerLine / NumPlanes;

  if (ColorBits > 1)
    BitBuffer = (unsigned char*)malloc(ColorBits * ((header->Width + 7) / 8));
  else
    BitBuffer = NULL;

  if (header->Compression)
    CompBuffer = (unsigned char*)malloc(header->BytesPerLine * 2);
  else
    CompBuffer = NULL;
}

/*
 * 'EndPage()' - Finish a page of graphics.
 */

static void HP_EndPage(int out)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Eject the current page...
  */

  if (NumPlanes > 1)
  {
     write_printf("\033*rC");			/* End color GFX */

     if (!(Duplex && (Page & 1)))
       write_printf("\033&l0H");		/* Eject current page */
  }
  else
  {
     write_printf("\033*r0B");		/* End GFX */

     if (!(Duplex && (Page & 1)))
       write_printf("\014");			/* Eject current page */
  }

#ifdef NOUNIX
  ioctl (out, FIOFLUSH, 0);
#else
  fflush(stdout);
#endif

 /*
  * Unregister the signal handler...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Free memory...
  */

  free(Planes[0]);

  if (BitBuffer)
    free(BitBuffer);

  if (CompBuffer)
    free(CompBuffer);
}

/*
 * 'HP_Shutdown()' - Shutdown the printer.
 */

static void HP_Shutdown(int out)
{
 /*
  * Send a PCL reset sequence.
  */

  write_byte(0x1b);
  write_byte('E');
}

/*
 * 'CancelJob()' - Cancel the current job...
 */

static void HP_CancelJob(int sig)			/* I - Signal */
{
  int	i;				/* Looping var */
#ifdef NOUNIX
  int out = global_out;
#else
  int out = 0;
#endif
  (void)sig;

 /*
  * Send out lots of NUL bytes to clear out any pending raster data...
  */

  for (i = 0; i < 600; i ++)
    write_byte(0);

 /*
  * End the current page and exit...
  */

  HP_EndPage(out);
  HP_Shutdown(out);

#ifdef NOUNIX
	return;
#else
        exit(0);
#endif
}

/*
 * 'HP_CompressData()' - Compress a line of graphics.
 */

static void HP_CompressData(unsigned char *line,	/* I - Data to compress */
             int           length,	/* I - Number of bytes */
	     int           plane,	/* I - Color plane */
	     int           type)	/* I - Type of compression */
{
  unsigned char	*line_ptr,		/* Current byte pointer */
        	*line_end,		/* End-of-line byte pointer */
        	*comp_ptr,		/* Pointer into compression buffer */
        	*start;			/* Start of compression sequence */
  int           count;			/* Count of bytes for output */


  switch (type)
  {
    default :
       /*
	* Do no compression...
	*/

	line_ptr = line;
	line_end = line + length;
	break;

    case 1 :
       /*
        * Do run-length encoding...
        */

	line_end = line + length;
	for (line_ptr = line, comp_ptr = CompBuffer;
	     line_ptr < line_end;
	     comp_ptr += 2, line_ptr += count)
	{
	  for (count = 1;
               (line_ptr + count) < line_end &&
	           line_ptr[0] == line_ptr[count] &&
        	   count < 256;
               count ++);

	  comp_ptr[0] = count - 1;
	  comp_ptr[1] = line_ptr[0];
	}

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;

    case 2 :
       /*
        * Do TIFF pack-bits encoding...
        */

	line_ptr = line;
	line_end = line + length;
	comp_ptr = CompBuffer;

	while (line_ptr < line_end)
	{
	  if ((line_ptr + 1) >= line_end)
	  {
	   /*
	    * Single byte on the end...
	    */

	    *comp_ptr++ = 0x00;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else if (line_ptr[0] == line_ptr[1])
	  {
	   /*
	    * Repeated sequence...
	    */

	    line_ptr ++;
	    count = 2;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] == line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = 257 - count;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else
	  {
	   /*
	    * Non-repeated sequence...
	    */

	    start    = line_ptr;
	    line_ptr ++;
	    count    = 1;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] != line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = count - 1;

	    memcpy(comp_ptr, start, count);
	    comp_ptr += count;
	  }
	}

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;
  }

 /*
  * Set the length of the data and write a raster plane...
  */

  write_printf("\033*b%d%c", (int)(line_end - line_ptr), plane);
  write_out(line_ptr, line_end - line_ptr);
}

/*
 * 'HP_OutputLine()' - Output a line of graphics.
 */

static void
HP_OutputLine(PrinterPageHeader *header, int out)	/* I - Page header */
{
  int   plane,			/* Current plane */
		bytes,			/* Bytes to write */
		count;			/* Bytes to convert */
  unsigned char	bit,			/* Current plane data */
		bit0,			/* Current low bit data */
		bit1,			/* Current high bit data */
		*plane_ptr,		/* Pointer into Planes */
		*bit_ptr;		/* Pointer into BitBuffer */


 /*
  * Output whitespace as needed...
  */

  if (Feed > 0)
  {
    write_printf("\033*b%dY", Feed);
    Feed = 0;
  }

 /*
  * Write bitmap data as needed...
  */

  bytes = (header->Width + 7) / 8;

  for (plane = 0; plane < NumPlanes; plane ++)
    if (ColorBits == 1)
    {
     /*
      * Send bits as-is...
      */

      HP_CompressData(Planes[plane], bytes, plane < (NumPlanes - 1) ? 'V' : 'W',
		   header->Compression);
    }
    else
    {
     /*
      * Separate low and high bit data into separate buffers.
      */

      for (count = header->BytesPerLine / NumPlanes,
               plane_ptr = Planes[plane], bit_ptr = BitBuffer;
	   count > 0;
	   count -= 2, plane_ptr += 2, bit_ptr ++)
      {
        bit = plane_ptr[0];

        bit0 = ((bit & 64) << 1) | ((bit & 16) << 2) | ((bit & 4) << 3) | ((bit & 1) << 4);
        bit1 = (bit & 128) | ((bit & 32) << 1) | ((bit & 8) << 2) | ((bit & 2) << 3);

        if (count > 1)
        {
          bit = plane_ptr[1];

              bit0 |= (bit & 1) | ((bit & 4) >> 1) | ((bit & 16) >> 2) | ((bit & 64) >> 3);
              bit1 |= ((bit & 2) >> 1) | ((bit & 8) >> 2) | ((bit & 32) >> 3) | ((bit & 128) >> 4);
        }

        bit_ptr[0]     = bit0;
        bit_ptr[bytes] = bit1;
      }

     /*
      * Send low and high bits...
      */

      HP_CompressData(BitBuffer, bytes, 'V', header->Compression);
      HP_CompressData(BitBuffer + bytes, bytes, plane < (NumPlanes - 1) ? 'V' : 'W',
		   header->Compression);
    }

#ifdef NOUNIX
  ioctl (out, FIOFLUSH, 0);
#else
  fflush(stdout);
#endif
}

/*
 * 'main()' - Main entry and processing of driver.
 */
#ifdef NOUNIX
int hp_rasterentry(int in, int out)
#else
int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
#endif
{
  PrinterRaster		*ras;	/* Raster stream for printing */
  PrinterPageHeader	header;	/* Page header from file */
  int			y;	/* Current line */
  ppd_file_t		*ppd = NULL;	/* PPD file */
#if !defined (NOUNIX)
  int out = 0;
#endif


 /*
  * Make sure status messages are not buffered...
  */

#ifdef NOUNIX
  ras = RasterOpen(in, RASTER_READ);

  global_out = out;
#else
  setbuf(stderr, NULL);

 /*
  * Open the page stream...
  */
  ras = RasterOpen(0, RASTER_READ);
#endif

 /*
  * Initialize the print device...
  */


    ppd = ppdOpenFile(PATH_DRIVER"/etc/hp-laserjet.ppd");

    if (NULL == ppd)
    {
        perror ("Unable to open the lasterjet.ppd\n");
#ifdef NOUNIX
	RasterClose(ras);
	return -1;
#else
        exit(0);
#endif
    }

    Setup(out);
 /*
  * Process pages as needed...
  */

  Page = 0;

 if (RasterReadHeader(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    Page ++;

    fprintf(stderr, "PAGE: %d %d\n", Page, header.NumCopies);

   /*
    * Start the page...
    */

    HP_StartPage(ppd, &header, out);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < (int)header.Height; y ++)
    {
     /*
      * Let the user know how far we have progressed...
      */

      if ((y & 127) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", Page,
	        100 * y / header.Height);

     /*
      * Read a line of graphics...
      */

      if (RasterReadPixels(ras, Planes[0], header.BytesPerLine) < 1)
        break;


     /*
      * See if the line is blank; if not, write it to the printer...
      */
          if (Planes[0][0] ||
              memcmp(Planes[0], Planes[0] + 1, header.BytesPerLine - 1))
          {
                HP_OutputLine(&header, out);
          }
          else
            Feed ++;
      
     }

   /*
    * Eject the page...
    */

        HP_EndPage(out);
  }

 /*
  * Shutdown the printer...
  */

    HP_Shutdown(out);

  if (ppd)
    ppdClose(ppd);

 /*
  * Close the raster stream...
  */

  RasterClose(ras);

 /*
  * If no pages were printed, send an error message...
  */

  if (Page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: MiniGUI Printer is ready to print.\n", stderr);


  return (Page == 0);
}

