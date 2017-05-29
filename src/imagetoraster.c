/*
 * "$Id: imagetoraster.c,v 1.52 2007-12-06 06:14:49 jpzhang Exp $"
 *
 * Contents:
 *
 *   exec_code()     - Execute PostScript setpagedevice commands as
 *                     appropriate.
 *   format_CMY()    - Convert image data to CMY.
 *   format_CMYK()   - Convert image data to CMYK.
 *   format_K()      - Convert image data to black.
 *   format_KCMY()   - Convert image data to KCMY.
 *   format_KCMYcm() - Convert image data to KCMYcm.
 *   format_RGBA()   - Convert image data to RGBA.
 *   format_W()      - Convert image data to luminance.
 *   format_YMC()    - Convert image data to YMC.
 *   format_YMCK()   - Convert image data to YMCK.
 *   make_lut()      - Make a lookup table given gamma and brightness values.
 */

/*
 * Include necessary headers...
 */
#include "mgpconfig.h"
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "ppd.h"
#include "common.h"
#include "image.h"
#include "raster.h"
#include "printing.h"


#ifdef NOUNIX
#include "filters.h"
#include "portclib.h"
#include <types.h>
#include <sockLib.h>
#include <inetLib.h>
#include <ioLib.h>
#endif

/*
 * Globals...
 */

int	Flip = 0,		/* Flip/mirror pages */
	XPosition = 0,		/* Horizontal position on page */
	YPosition = 0,		/* Vertical position on page */
	Collate = 0,		/* Collate copies? */
	Copies = 1;		/* Number of copies */
int	Floyd16x16[16][16] =	/* Traditional Floyd ordered dither */
	{
	  { 0,   128, 32,  160, 8,   136, 40,  168,
	    2,   130, 34,  162, 10,  138, 42,  170 },
	  { 192, 64,  224, 96,  200, 72,  232, 104,
	    194, 66,  226, 98,  202, 74,  234, 106 },
	  { 48,  176, 16,  144, 56,  184, 24,  152,
	    50,  178, 18,  146, 58,  186, 26,  154 },
	  { 240, 112, 208, 80,  248, 120, 216, 88,
	    242, 114, 210, 82,  250, 122, 218, 90 },
	  { 12,  140, 44,  172, 4,   132, 36,  164,
	    14,  142, 46,  174, 6,   134, 38,  166 },
	  { 204, 76,  236, 108, 196, 68,  228, 100,
	    206, 78,  238, 110, 198, 70,  230, 102 },
	  { 60,  188, 28,  156, 52,  180, 20,  148,
	    62,  190, 30,  158, 54,  182, 22,  150 },
	  { 252, 124, 220, 92,  244, 116, 212, 84,
	    254, 126, 222, 94,  246, 118, 214, 86 },
	  { 3,   131, 35,  163, 11,  139, 43,  171,
	    1,   129, 33,  161, 9,   137, 41,  169 },
	  { 195, 67,  227, 99,  203, 75,  235, 107,
	    193, 65,  225, 97,  201, 73,  233, 105 },
	  { 51,  179, 19,  147, 59,  187, 27,  155,
	    49,  177, 17,  145, 57,  185, 25,  153 },
	  { 243, 115, 211, 83,  251, 123, 219, 91,
	    241, 113, 209, 81,  249, 121, 217, 89 },
	  { 15,  143, 47,  175, 7,   135, 39,  167,
	    13,  141, 45,  173, 5,   133, 37,  165 },
	  { 207, 79,  239, 111, 199, 71,  231, 103,
	    205, 77,  237, 109, 197, 69,  229, 101 },
	  { 63,  191, 31,  159, 55,  183, 23,  151,
	    61,  189, 29,  157, 53,  181, 21,  149 },
	  { 254, 127, 223, 95,  247, 119, 215, 87,
	    253, 125, 221, 93,  245, 117, 213, 85 }
	};
int	Floyd8x8[8][8] =
	{
	  {  0, 32,  8, 40,  2, 34, 10, 42 },
	  { 48, 16, 56, 24, 50, 18, 58, 26 },
	  { 12, 44,  4, 36, 14, 46,  6, 38 },
	  { 60, 28, 52, 20, 62, 30, 54, 22 },
	  {  3, 35, 11, 43,  1, 33,  9, 41 },
	  { 51, 19, 59, 27, 49, 17, 57, 25 },
	  { 15, 47,  7, 39, 13, 45,  5, 37 },
	  { 63, 31, 55, 23, 61, 29, 53, 21 }
	};
int	Floyd4x4[4][4] =
	{
	  {  0,  8,  2, 10 },
	  { 12,  4, 14,  6 },
	  {  3, 11,  1,  9 },
	  { 15,  7, 13,  5 }
	};

ib_t	OnPixels[256],	/* On-pixel LUT */
	OffPixels[256];	/* Off-pixel LUT */
int	Planes[] =	/* Number of planes for each colorspace */
	{
	  1,		/* CSPACE_W */
	  3,		/* CSPACE_RGB */
	  4,		/* CSPACE_RGBA */
	  1,		/* CSPACE_K */
	  3,		/* CSPACE_CMY */
	  3,		/* CSPACE_YMC */
	  4,		/* CSPACE_CMYK */
	  4,		/* CSPACE_YMCK */
	  4,		/* CSPACE_KCMY */
	  6,		/* CSPACE_KCMYcm */
	  4,		/* CSPACE_GMCK */
	  4,		/* CSPACE_GMCS */
	  1,		/* CSPACE_WHITE */
	  1,		/* CSPACE_GOLD */
	  1,		/* CSPACE_SILVER */
	  3,		/* CSPACE_CIEXYZ */
	  3,		/* CSPACE_CIELab */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  0,		/* ... reserved ... */
	  3,		/* CSPACE_ICC1 */
	  3,		/* CSPACE_ICC2 */
	  3,		/* CSPACE_ICC3 */
	  3,		/* CSPACE_ICC4 */
	  3,		/* CSPACE_ICC5 */
	  3,		/* CSPACE_ICC6 */
	  3,		/* CSPACE_ICC7 */
	  3,		/* CSPACE_ICC8 */
	  3,		/* CSPACE_ICC9 */
	  3,		/* CSPACE_ICCA */
	  3,		/* CSPACE_ICCB */
	  3,		/* CSPACE_ICCC */
	  3,		/* CSPACE_ICCD */
	  3,		/* CSPACE_ICCE */
	  3		/* CSPACE_ICCF */
	};


/*
 * Local functions...
 */
 
static void	exec_code(PrinterPageHeader *header, const char *code);
static void	format_CMY(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_CMYK(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_K(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_KCMYcm(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_KCMY(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
#define		format_RGB format_CMY
static void	format_RGBA(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_W(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_YMC(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	format_YMCK(PrinterPageHeader *header, unsigned char *row, int y, int z, int xsize, int ysize, int yerr0, int yerr1, ib_t *r0, ib_t *r1);
static void	make_lut(ib_t *, int, float, float);

const char * const g_PageSize[] = { 
"A4", "B5", "Letter", "Legal", "Executive",
"A0", "A1", "A2", "A3", "A5", "A6", "A7", "A8", "A9", "B0", "B1",
"B10", "B2", "B3", "B4", "B6", "B7", "B8", "B9", "C5E", "Comm10E",
"DLE", "Folio", "Ledger", "Tabloid", 0 };
/*
 * 'StartPrintProc()' - Main entry...
 */

#ifdef NOUNIX
static int initialized = 0;

static int (*raster_callback)(int, int);
char buf[512];
size_t buf_size = 512;
size_t rb ;
void * pringing_thread(void * data)
{
   int srv_fd, on, s_fd, size_a;
   struct sockaddr_in addr;
   int driver_fd = (int)data;

   if ((srv_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0){
        perror("can't create server side socket printing server!\n");
        initialized = -1;
        pthread_exit(0);
        return 0;
   }

   on = 0;
   ioctl(srv_fd, FIONBIO, (int)&on);

   addr.sin_family = AF_INET;
   addr.sin_port = htons(4009); 
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_len = sizeof(struct sockaddr_in);

   if (bind (srv_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in))<0){
        perror("can't bind server side socket printing server!\n");
        initialized = -2;
        pthread_exit(0);
        return 0;
   }

   if (listen (srv_fd, 1)<0){
        perror("can't listen server side socket printing server!\n");
        initialized = -3;
        pthread_exit(0);
        return 0;
   }

   size_a = sizeof(struct sockaddr_in);

   initialized = 1;

   s_fd = accept (srv_fd, (struct sockaddr *) &addr, &size_a);

   if (raster_callback) 
       raster_callback(s_fd, driver_fd);

   close(s_fd);
   close(srv_fd);
   pthread_exit(0);
   return 0;
}
#endif

int __StartPrintProc (HDC hdc , const MGPRINT_INFO * info)
{
  int		i;		/* Looping var */
  image_t	*img;		/* Image to print */
  float		xprint,		/* Printable area */
		yprint,
		xinches,	/* Total size in inches */
		yinches;
  float		xsize,		/* Total size in points */
		ysize,
		xsize2,
		ysize2;
  float		aspect;		/* Aspect ratio */
  int		xpages,		/* # x pages */
		ypages,		/* # y pages */
		xpage,		/* Current x page */
		ypage,		/* Current y page */
		xtemp,		/* Bitmap width in pixels */
		ytemp,		/* Bitmap height in pixels */
		page;		/* Current page number */
  int		x0, y0,		/* Corners of the page in image coords */
		x1, y1;
  ppd_file_t	*ppd;		/* PPD file */
  ppd_choice_t	*choice,	/* PPD option choice */
		**choices;	/* List of marked choices */
  int		count;		/* Number of marked choices */
  char		*resolution,	/* Output resolution */
		*media_type;	/* Media type */
  ppd_profile_t	*profile;	/* Color profile */
  ppd_profile_t	userprofile;	/* User-specified profile */
  PrinterRaster	*ras;		/* Raster stream */
  PrinterPageHeader header;	/* Page header */
  int		num_options;	/* Number of print options */
  PrinterOption	*options;	/* Print options */
  const char	*val;		/* Option value */
  int		slowcollate,	/* Collate copies the slow way */
		slowcopies;	/* Make copies the "slow" way? */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  float		zoom;		/* Zoom facter */
  int		xppi, yppi;	/* Pixels-per-inch */
  int		hue, sat;	/* Hue and saturation adjustment */
  izoom_t	*z;		/* ImageZoom buffer */
  int		primary,	/* Primary image colorspace */
		secondary;	/* Secondary image colorspace */
  ib_t		*row,		/* Current row */
		*r0,		/* Top row */
		*r1;		/* Bottom row */
  int		y,		/* Current Y coordinate on page */
		iy,		/* Current Y coordinate in image */
		last_iy,	/* Previous Y coordinate in image */
		yerr0,		/* Top Y error value */
		yerr1,		/* Bottom Y error value */
		blank;		/* Blank value */
  ib_t		lut[256];	/* Gamma/brightness LUT */
  int		plane,		/* Current color plane */
		num_planes;	/* Number of color planes */

#ifdef NOUNIX
   int write_fd, driver_fd, j, on; //write socket fd
   pthread_t th;
   pthread_attr_t new_attr;
   struct sockaddr_in addr;
   char buf[PATH_MAX+1] = {0};
#else
  pid_t pid = 0;
#endif

  char psizebuf[32] = {0};
  sprintf (psizebuf, "PageSize=%s", g_PageSize[info->page_type]);


 options     = NULL;

 num_options = ParseOptions(psizebuf, 0, &options);

 /*
  * Process command-line options and write the prolog...
  */

  Copies = 1;/* info->copies;*/ /* copies 1*/
  Flip = 0; /* no mirror*/



  zoom = 0.0;
  xppi = 0;
  yppi = 0;
  hue  = 0;
  sat  = 100;
  g    = 1.0;
  b    = 1.0;

  ppd = SetCommonOptions(info->ppd_name, num_options, options, 0);

  if ((val = GetOption("multiple-document-handling", num_options, options)) != NULL)
  {
   /*
    * This IPP attribute is unnecessarily complicated...
    *
    *   single-document, separate-documents-collated-copies, and
    *   single-document-new-sheet all require collated copies.
    *
    *   separate-documents-collated-copies allows for uncollated copies.
    */

    Collate = strcasecmp(val, "separate-documents-collated-copies") != 0;
  }

  if ((val = GetOption("Collate", num_options, options)) != NULL &&
      strcasecmp(val, "True") == 0)
    Collate = 1;

  if ((val = GetOption("gamma", num_options, options)) != NULL)
    g = atoi(val) * 0.001f;


  if ((val = GetOption("brightness", num_options, options)) != NULL)
    b = atoi(val) * 0.01f;

  if ((val = GetOption("scaling", num_options, options)) != NULL)
    zoom = atoi(val) * 0.01;

  if ((val = GetOption("ppi", num_options, options)) != NULL)
    if (sscanf(val, "%dx%d", &xppi, &yppi) < 2)
      yppi = xppi;

  if ((val = GetOption("position", num_options, options)) != NULL)
  {
    if (strcasecmp(val, "center") == 0)
    {
      XPosition = 0;
      YPosition = 0;
    }
    else if (strcasecmp(val, "top") == 0)
    {
      XPosition = 0;
      YPosition = 1;
    }
    else if (strcasecmp(val, "left") == 0)
    {
      XPosition = -1;
      YPosition = 0;
    }
    else if (strcasecmp(val, "right") == 0)
    {
      XPosition = 1;
      YPosition = 0;
    }
    else if (strcasecmp(val, "top-left") == 0)
    {
      XPosition = -1;
      YPosition = 1;
    }
    else if (strcasecmp(val, "top-right") == 0)
    {
      XPosition = 1;
      YPosition = 1;
    }
    else if (strcasecmp(val, "bottom") == 0)
    {
      XPosition = 0;
      YPosition = -1;
    }
    else if (strcasecmp(val, "bottom-left") == 0)
    {
      XPosition = -1;
      YPosition = -1;
    }
    else if (strcasecmp(val, "bottom-right") == 0)
    {
      XPosition = 1;
      YPosition = -1;
    }
  }

  if ((val = GetOption("saturation", num_options, options)) != NULL)
    sat = atoi(val);

  if ((val = GetOption("hue", num_options, options)) != NULL)
    hue = atoi(val);



 /*
  * Set the needed options in the page header...
  */

  memset(&header, 0, sizeof(header));
  header.HWResolution[0]  = 100;
  header.HWResolution[1]  = 100;
  header.BitsPerColor = 1;
  header.ColorOrder   = ORDER_CHUNKED;
  header.ColorSpace   = CSPACE_K;

  if (ppd && ppd->patches)
    exec_code(&header, ppd->patches);

  if ((count = ppdCollect(ppd, PPD_ORDER_DOCUMENT, &choices)) > 0)
    for (i = 0; i < count; i ++)
      exec_code(&header, choices[i]->code);

  if ((count = ppdCollect(ppd, PPD_ORDER_ANY, &choices)) > 0)
    for (i = 0; i < count; i ++)
      exec_code(&header, choices[i]->code);

  if ((count = ppdCollect(ppd, PPD_ORDER_PROLOG, &choices)) > 0)
    for (i = 0; i < count; i ++)
      exec_code(&header, choices[i]->code);

  if ((count = ppdCollect(ppd, PPD_ORDER_PAGE, &choices)) > 0)
    for (i = 0; i < count; i ++)
      exec_code(&header, choices[i]->code);

 /*
  * Get the media type and resolution that have been chosen...
  */

  if ((choice = ppdFindMarkedChoice(ppd, "MediaType")) != NULL)
    media_type = choice->choice;
  else
    media_type = "";

  if ((choice = ppdFindMarkedChoice(ppd, "Resolution")) != NULL)
    resolution = choice->choice;
  else
    resolution = "";

 /*
  * Choose the appropriate colorspace...
  */

  switch (header.ColorSpace)
  {
    case CSPACE_W :
        primary   = IMAGE_WHITE;
	secondary = IMAGE_WHITE;
        header.BitsPerPixel = header.BitsPerColor;
	break;

    default :
    case CSPACE_RGB :
    case CSPACE_RGBA :
        primary   = IMAGE_RGB;
	secondary = IMAGE_RGB;

       /*
        * Ensure that colorimetric colorspaces use at least 8 bits per
	* component...
	*/

        if (header.ColorSpace >= CSPACE_CIEXYZ &&
	    header.BitsPerColor < 8)
	  header.BitsPerColor = 8;

	if (header.ColorOrder == ORDER_CHUNKED)
	{
	  if (header.BitsPerColor >= 8)
            header.BitsPerPixel = header.BitsPerColor * 3;
	  else
            header.BitsPerPixel = header.BitsPerColor * 4;
	}
	else
	  header.BitsPerPixel = header.BitsPerColor;
	break;

    case CSPACE_K :
    case CSPACE_WHITE :
    case CSPACE_GOLD :
    case CSPACE_SILVER :
        primary   = IMAGE_BLACK;
	secondary = IMAGE_BLACK;
        header.BitsPerPixel = header.BitsPerColor;
	break;

    case CSPACE_CMYK :
    case CSPACE_YMCK :
    case CSPACE_KCMY :
    case CSPACE_GMCK :
    case CSPACE_GMCS :
        primary   = IMAGE_CMYK;
	secondary = IMAGE_CMYK;

	if (header.ColorOrder == ORDER_CHUNKED)
          header.BitsPerPixel = header.BitsPerColor * 4;
	else
	  header.BitsPerPixel = header.BitsPerColor;
	break;

    case CSPACE_CMY :
    case CSPACE_YMC :
        primary   = IMAGE_CMY;
	secondary = IMAGE_CMY;

	if (header.ColorOrder == ORDER_CHUNKED)
	{
	  if (header.BitsPerColor >= 8)
            header.BitsPerPixel = 24;
	  else
            header.BitsPerPixel = header.BitsPerColor * 4;
	}
	else
	  header.BitsPerPixel = header.BitsPerColor;
	break;

    case CSPACE_KCMYcm :
	if (header.BitsPerPixel == 1)
	{
          primary   = IMAGE_CMY;
	  secondary = IMAGE_CMY;

	  if (header.ColorOrder == ORDER_CHUNKED)
	    header.BitsPerPixel = 8;
	  else
	    header.BitsPerPixel = 1;
	}
	else
	{
          primary   = IMAGE_CMYK;
	  secondary = IMAGE_CMYK;

	  if (header.ColorOrder == ORDER_CHUNKED)
	    header.BitsPerPixel = header.BitsPerColor * 4;
	  else
	    header.BitsPerPixel = header.BitsPerColor;
	}
	break;
  }

 /*
  * Find a color profile matching the current options...
  */

  if ((val = GetOption("profile", num_options, options)) != NULL)
  {
    profile = &userprofile;
    sscanf(val, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
           &(userprofile.density), &(userprofile.gamma),
	   userprofile.matrix[0] + 0, userprofile.matrix[0] + 1,
	   userprofile.matrix[0] + 2,
	   userprofile.matrix[1] + 0, userprofile.matrix[1] + 1,
	   userprofile.matrix[1] + 2,
	   userprofile.matrix[2] + 0, userprofile.matrix[2] + 1,
	   userprofile.matrix[2] + 2);

    userprofile.density      *= 0.001f;
    userprofile.gamma        *= 0.001f;
    userprofile.matrix[0][0] *= 0.001f;
    userprofile.matrix[0][1] *= 0.001f;
    userprofile.matrix[0][2] *= 0.001f;
    userprofile.matrix[1][0] *= 0.001f;
    userprofile.matrix[1][1] *= 0.001f;
    userprofile.matrix[1][2] *= 0.001f;
    userprofile.matrix[2][0] *= 0.001f;
    userprofile.matrix[2][1] *= 0.001f;
    userprofile.matrix[2][2] *= 0.001f;
  }
  else if (ppd != NULL)
  {
    fprintf(stderr, "DEBUG: Searching for profile \"%s/%s\"...\n",
            resolution, media_type);

    for (i = 0, profile = ppd->profiles; i < ppd->num_profiles; i ++, profile ++)
    {
      fprintf(stderr, "DEBUG: \"%s/%s\" = ", profile->resolution,
              profile->media_type);

      if ((strcmp(profile->resolution, resolution) == 0 ||
           profile->resolution[0] == '-') &&
          (strcmp(profile->media_type, media_type) == 0 ||
           profile->media_type[0] == '-'))
      {
        fputs("MATCH!\n", stderr);
	break;
      }
      else
        fputs("no.\n", stderr);
    }

   /*
    * If we found a color profile, use it!
    */

    if (i >= ppd->num_profiles)
      profile = NULL;
  }
  else
    profile = NULL;

  if (profile)
    ImageSetProfile(profile->density, profile->gamma, profile->matrix);

  ImageSetColorSpace(header.ColorSpace);

 /*
  * Create a gamma/brightness LUT...
  */

  make_lut(lut, primary, g, b);


  img = ImageOpenDC (hdc, primary, sat, hue, lut , (void*)info);

  if (img == NULL)
  {
    fputs("ERROR: Unable to open image file for printing!\n", stderr);
    ppdClose(ppd);
    return (1);
  }

 /*
  * Scale as necessary...
  */

  if (zoom == 0.0 && xppi == 0)
  {
    xppi = img->xppi;
    yppi = img->yppi;
  }

  if (yppi == 0)
    yppi = xppi;

  fprintf(stderr, "DEBUG: Before scaling: xppi=%d, yppi=%d, zoom=%.2f\n",
          xppi, yppi, zoom);

  if (xppi > 0)
  {
   /*
    * Scale the image as neccesary to match the desired pixels-per-inch.
    */
    
    if (Orientation & 1)
    {
      xprint = (PageTop - PageBottom) / 72.0;
      yprint = (PageRight - PageLeft) / 72.0;
    }
    else
    {
      xprint = (PageRight - PageLeft) / 72.0;
      yprint = (PageTop - PageBottom) / 72.0;
    }

    fprintf(stderr, "DEBUG: Before scaling: xprint=%.1f, yprint=%.1f\n",
            xprint, yprint);

    xinches = (float)img->xsize / (float)xppi;
    yinches = (float)img->ysize / (float)yppi;

    fprintf(stderr, "DEBUG: Image size is %.1f x %.1f inches...\n",
            xinches, yinches);

    if ((val = GetOption("natural-scaling", num_options, options)) != NULL)
    {
      xinches = xinches * atoi(val) / 100;
      yinches = yinches * atoi(val) / 100;
    }

    if (GetOption("orientation-requested", num_options, options) == NULL &&
        GetOption("landscape", num_options, options) == NULL)
    {
     /*
      * Rotate the image if it will fit landscape but not portrait...
      */

      fputs("DEBUG: Auto orientation...\n", stderr);

      if ((xinches > xprint || yinches > yprint) &&
          xinches <= yprint && yinches <= xprint)
      {
       /*
	* Rotate the image as needed...
	*/

        fputs("DEBUG: Using landscape orientation...\n", stderr);

	Orientation = (Orientation + 1) & 3;
	xsize       = yprint;
	yprint      = xprint;
	xprint      = xsize;
      }
    }
  }
  else
  {
   /*
    * Scale percentage of page size...
    */

    xprint = (PageRight - PageLeft) / 72.0;
    yprint = (PageTop - PageBottom) / 72.0;
    aspect = (float)img->yppi / (float)img->xppi;

    fprintf(stderr, "DEBUG: Before scaling: xprint=%.1f, yprint=%.1f\n",
            xprint, yprint);

    fprintf(stderr, "DEBUG: img->xppi = %d, img->yppi = %d, aspect = %f\n",
            img->xppi, img->yppi, aspect);

    xsize = xprint * zoom;
    ysize = xsize * img->ysize / img->xsize / aspect;

    if (ysize > (yprint * zoom))
    {
      ysize = yprint * zoom;
      xsize = ysize * img->xsize * aspect / img->ysize;
    }

    xsize2 = yprint * zoom;
    ysize2 = xsize2 * img->ysize / img->xsize / aspect;

    if (ysize2 > (xprint * zoom))
    {
      ysize2 = xprint * zoom;
      xsize2 = ysize2 * img->xsize * aspect / img->ysize;
    }

    fprintf(stderr, "DEBUG: Portrait size is %.2f x %.2f inches\n", xsize, ysize);
    fprintf(stderr, "DEBUG: Landscape size is %.2f x %.2f inches\n", xsize2, ysize2);

    if (GetOption("orientation-requested", num_options, options) == NULL &&
        GetOption("landscape", num_options, options) == NULL)
    {
     /*
      * Choose the rotation with the largest area, but prefer
      * portrait if they are equal...
      */

      fputs("DEBUG: Auto orientation...\n", stderr);

      if ((xsize * ysize) < (xsize2 * xsize2))
      {
       /*
	* Do landscape orientation...
	*/
        fputs("DEBUG: Using landscape orientation...\n", stderr);

        Orientation = 1;
        xinches     = xsize2;
        yinches     = ysize2;
        xprint      = (PageTop - PageBottom) / 72.0;
        yprint      = (PageRight - PageLeft) / 72.0;
      }
      else
      {
       /*
	* Do portrait orientation...
	*/
        fputs("DEBUG: Using portrait orientation...\n", stderr);

        Orientation = 0;
        xinches     = xsize;
        yinches     = ysize;
      }
    }
    else if (Orientation & 1)
    {
      fputs("DEBUG: Using landscape orientation...\n", stderr);

      xinches     = xsize2;
      yinches     = ysize2;
      xprint      = (PageTop - PageBottom) / 72.0;
      yprint      = (PageRight - PageLeft) / 72.0;
    }
    else
    {
      fputs("DEBUG: Using portrait orientation...\n", stderr);

      xinches     = xsize;
      yinches     = ysize;
      xprint      = (PageRight - PageLeft) / 72.0;
      yprint      = (PageTop - PageBottom) / 72.0;
    }
  }

 /*
  * Compute the number of pages to print and the size of the image on each
  * page...
  */

  xpages = (int)ceil(xinches / xprint);
  ypages = (int)ceil(yinches / yprint);

  xprint = xinches / xpages;
  yprint = yinches / ypages;

  fprintf(stderr, "DEBUG: xpages = %dx%.2fin, ypages = %dx%.2fin\n",
          xpages, xprint, ypages, yprint);

 /*
  * Compute the bitmap size...
  */

  if ((choice = ppdFindMarkedChoice(ppd, "PageSize")) != NULL &&
      strcasecmp(choice->choice, "Custom") == 0)
  {
    float	width,		/* New width in points */
		length;		/* New length in points */

   /*
    * Use the correct width and length for the current orientation...
    */

        if (Orientation & 1)
        {
          width  = yprint * 72.0;
          length = xprint * 72.0;
        }
        else
        {
          width  = xprint * 72.0;
          length = yprint * 72.0;
        }


        width  += ppd->custom_margins[0] + ppd->custom_margins[2];
        length += ppd->custom_margins[1] + ppd->custom_margins[3];

       /*
        * Enforce minimums...
        */

        if (width < ppd->custom_min[0])
          width = ppd->custom_min[0];

        if (length < ppd->custom_min[1])
          length = ppd->custom_min[1];

        fprintf(stderr, "DEBUG: Updated custom page size to %.2f x %.2f inches...\n",
                width / 72.0, length / 72.0);

       /*
        * Set the new custom size...
        */

        header.PageSize[0] = (unsigned int)(width + 0.5);
        header.PageSize[1] = (unsigned int)(length + 0.5);

       /*
        * Update page variables...
        */

        PageWidth  = width;
        PageLength = length;

        PageLeft   = ppd->custom_margins[0];
        PageRight  = width - ppd->custom_margins[2];
        PageBottom = ppd->custom_margins[1];
        PageTop    = length - ppd->custom_margins[3];

       /*
        * Remove margins from page size...
        */

        width  -= ppd->custom_margins[0] + ppd->custom_margins[2];
        length -= ppd->custom_margins[1] + ppd->custom_margins[3];

       /*
        * Set the bitmap size...
        */

        header.Width  = (unsigned int)(width * header.HWResolution[0] / 72.0);
        header.Height = (unsigned int)(length * header.HWResolution[1] / 72.0);
  }
  else
  {
        header.Width   = (unsigned int)((PageRight - PageLeft) * header.HWResolution[0] / 72.0);
        header.Height  = (unsigned int)((PageTop - PageBottom) * header.HWResolution[1] / 72.0);
        header.PageSize[0] = (unsigned int)PageWidth;
        header.PageSize[1] = (unsigned int)PageLength;
  }


  header.Margins[0] = (unsigned int)PageLeft;
  header.Margins[1] = (unsigned int)PageBottom;

  fprintf(stderr, "DEBUG: PageSize = [%d %d]\n", header.PageSize[0],
          header.PageSize[1]);

  switch (Orientation)
  {
    case 0 :
	switch (XPosition)
	{
	  case -1 :
              header.ImagingBoundingBox[0] = (unsigned int)PageLeft;
	      header.ImagingBoundingBox[2] = (unsigned int)(PageLeft + xprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[0] = (unsigned int)((PageRight + PageLeft - xprint * 72) / 2);
	      header.ImagingBoundingBox[2] = (unsigned int)((PageRight + PageLeft + xprint * 72) / 2);
	      break;
	  case 1 :
              header.ImagingBoundingBox[0] = (unsigned int)(PageRight - xprint * 72);
	      header.ImagingBoundingBox[2] = (unsigned int)PageRight;
	      break;
	}

	switch (YPosition)
	{
	  case -1 :
              header.ImagingBoundingBox[1] = (unsigned int)PageBottom;
	      header.ImagingBoundingBox[3] = (unsigned int)(PageBottom + yprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[1] = (unsigned int)((PageTop + PageBottom - yprint * 72) / 2);
	      header.ImagingBoundingBox[3] = (unsigned int)((PageTop + PageBottom + yprint * 72) / 2);
	      break;
	  case 1 :
              header.ImagingBoundingBox[1] = (unsigned int)(PageTop - yprint * 72);
	      header.ImagingBoundingBox[3] = (unsigned int)PageTop;
	      break;
	}
	break;

    case 1 :
	switch (XPosition)
	{
	  case -1 :
              header.ImagingBoundingBox[0] = (unsigned int)PageBottom;
	      header.ImagingBoundingBox[2] = (unsigned int)(PageBottom + yprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[0] = (unsigned int)((PageTop + PageBottom - yprint * 72) / 2);
	      header.ImagingBoundingBox[2] = (unsigned int)((PageTop + PageBottom + yprint * 72) / 2);
	      break;
	  case 1 :
              header.ImagingBoundingBox[0] = (unsigned int)(PageTop - yprint * 72);
	      header.ImagingBoundingBox[2] = (unsigned int)PageTop;
	      break;
	}

	switch (YPosition)
	{
	  case -1 :
              header.ImagingBoundingBox[1] = (unsigned int)PageLeft;
	      header.ImagingBoundingBox[3] = (unsigned int)(PageLeft + xprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[1] = (unsigned int)((PageRight + PageLeft - xprint * 72) / 2);
	      header.ImagingBoundingBox[3] = (unsigned int)((PageRight + PageLeft + xprint * 72) / 2);
	      break;
	  case 1 :
              header.ImagingBoundingBox[1] = (unsigned int)(PageRight - xprint * 72);
	      header.ImagingBoundingBox[3] = (unsigned int)PageRight;
	      break;
	}
	break;

    case 2 :
	switch (XPosition)
	{
	  case 1 :
              header.ImagingBoundingBox[0] = (unsigned int)PageLeft;
	      header.ImagingBoundingBox[2] = (unsigned int)(PageLeft + xprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[0] = (unsigned int)((PageRight + PageLeft - xprint * 72) / 2);
	      header.ImagingBoundingBox[2] = (unsigned int)((PageRight + PageLeft + xprint * 72) / 2);
	      break;
	  case -1 :
              header.ImagingBoundingBox[0] = (unsigned int)(PageRight - xprint * 72);
	      header.ImagingBoundingBox[2] = (unsigned int)PageRight;
	      break;
	}

	switch (YPosition)
	{
	  case 1 :
              header.ImagingBoundingBox[1] = (unsigned int)PageBottom;
	      header.ImagingBoundingBox[3] = (unsigned int)(PageBottom + yprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[1] = (unsigned int)((PageTop + PageBottom - yprint * 72) / 2);
	      header.ImagingBoundingBox[3] = (unsigned int)((PageTop + PageBottom + yprint * 72) / 2);
	      break;
	  case -1 :
              header.ImagingBoundingBox[1] = (unsigned int)(PageTop - yprint * 72);
	      header.ImagingBoundingBox[3] = (unsigned int)PageTop;
	      break;
	}
	break;

    case 3 :
	switch (XPosition)
	{
	  case 1 :
              header.ImagingBoundingBox[0] = (unsigned int)PageBottom;
	      header.ImagingBoundingBox[2] = (unsigned int)(PageBottom + yprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[0] = (unsigned int)((PageTop + PageBottom - yprint * 72) / 2);
	      header.ImagingBoundingBox[2] = (unsigned int)((PageTop + PageBottom + yprint * 72) / 2);
	      break;
	  case -1 :
              header.ImagingBoundingBox[0] = (unsigned int)(PageTop - yprint * 72);
	      header.ImagingBoundingBox[2] = (unsigned int)PageTop;
	      break;
	}

	switch (YPosition)
	{
	  case 1 :
              header.ImagingBoundingBox[1] = (unsigned int)PageLeft;
	      header.ImagingBoundingBox[3] = (unsigned int)(PageLeft + xprint * 72);
	      break;
	  default :
              header.ImagingBoundingBox[1] = (unsigned int)((PageRight + PageLeft - xprint * 72) / 2);
	      header.ImagingBoundingBox[3] = (unsigned int)((PageRight + PageLeft + xprint * 72) / 2);
	      break;
	  case -1 :
              header.ImagingBoundingBox[1] = (unsigned int)(PageRight - xprint * 72);
	      header.ImagingBoundingBox[3] = (unsigned int)PageRight;
	      break;
	}
	break;
  }

  switch (header.ColorOrder)
  {
    default :
        header.BytesPerLine = (header.BitsPerPixel *
	                           header.Width + 7) / 8;
        num_planes = 1;
        break;

    case ORDER_BANDED :
        if (header.ColorSpace == CSPACE_KCMYcm &&
	    header.BitsPerColor > 1)
          header.BytesPerLine = (header.BitsPerPixel *
	                             header.Width + 7) / 8 * 4;
        else
          header.BytesPerLine = (header.BitsPerPixel *
	                             header.Width + 7) / 8 *
				    Planes[header.ColorSpace];
        num_planes = 1;
        break;

    case ORDER_PLANAR :
        header.BytesPerLine = (header.BitsPerPixel *
	                           header.Width + 7) / 8;
        num_planes = Planes[header.ColorSpace];
        break;
  }

 /*
  * See if we need to collate, and if so how we need to do it...
  */

  if (xpages == 1 && ypages == 1)
    Collate = 0;

  slowcollate = Collate && ppdFindOption(ppd, "Collate") == NULL;
  if (ppd != NULL)
    slowcopies = ppd->manual_copies;
  else
    slowcopies = 1;

  if (Copies > 1 && !slowcollate && !slowcopies)
  {
    header.Collate   = (BOOL)Collate;
    header.NumCopies = Copies;

    Copies = 1;
  }
  else
    header.NumCopies = 1;

 /*
  * Create the dithering lookup tables...
  */

  OnPixels[0]    = 0x00;
  OnPixels[255]  = 0xff;
  OffPixels[0]   = 0x00;
  OffPixels[255] = 0xff;

  switch (header.BitsPerColor)
  {
    case 2 :
        for (i = 1; i < 255; i ++)
        {
          OnPixels[i]  = 0x55 * (i / 85 + 1);
          OffPixels[i] = 0x55 * (i / 64);
        }
        break;
    case 4 :
        for (i = 1; i < 255; i ++)
        {
          OnPixels[i]  = 17 * (i / 17 + 1);
          OffPixels[i] = 17 * (i / 16);
        }

	OnPixels[255] = OffPixels[255] = 0xff;
        break;
  }
 /*
  * Output the pages...
  */
  row   = (ib_t*)malloc(2 * header.BytesPerLine);

#ifdef NOUNIX
   if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", info->printer_name, "device", buf, PATH_MAX) < 0)
   { 
       perror ("Read config file error, use default! \n");  
       strcpy(buf, "/dev/lp0"); 
   }  

   if ((driver_fd = open (buf, O_RDWR|O_SYNC, 0777)) == -1 ){
        perror ("Unable to open device! \n");  
        ppdClose(ppd);
        ImageClose(img);
        return 1;
   }

   if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", info->printer_name, "name", buf, PATH_MAX) < 0)
   { 
       perror ("Read config file error for printer name! \n");  
       ppdClose(ppd);
       ImageClose(img);
       return 1;
   }  

   raster_callback = NULL;
   for (j=0; j<(int)TABLESIZE(FILTER_TABLE);j++) {
       if (strcasecmp (FILTER_TABLE[j].name, buf) == 0) {
           raster_callback = FILTER_TABLE[j].raster_entry;
           break;
       }
   }

   if (!raster_callback){
        perror ("Can't prepare filter for printer name! \n");  
        ppdClose(ppd);
        ImageClose(img);
        return 1;
   }

   pthread_attr_init (&new_attr);
   pthread_attr_setdetachstate (&new_attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setstacksize (&new_attr, 512 * 1024);
   pthread_create(&th, &new_attr, pringing_thread, (void*)driver_fd);
   pthread_attr_destroy (&new_attr);
   while (!initialized)
       sleep(1);

   if (initialized < 0){
        perror("server initialize error!\n");
        initialized = 0;
        ppdClose(ppd);
        ImageClose(img);
        return 1;
   }

   addr.sin_family = AF_INET;
   addr.sin_port = htons(4009); 
   addr.sin_addr.s_addr = inet_addr("127.0.0.1");
   addr.sin_len = sizeof(struct sockaddr_in);

   if ((write_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0) {
        perror("can't create socket for connect server!\n");
        pthread_cancel (th);
        initialized = 0;
        ppdClose(ppd);
        ImageClose(img);
        return 1;
   }

   on = 0;
   ioctl(write_fd , FIONBIO, (int)&on);
   if (connect(write_fd,(struct sockaddr *) &addr, sizeof(struct sockaddr_in))<0) {
        perror("can't connect to server!\n");
        pthread_cancel (th);
        initialized = 0;
        ppdClose(ppd);
        ImageClose(img);
        return 1;
   }

   ras   = RasterOpen(write_fd, RASTER_WRITE);   /*output to stdout*/
#else
   int pd[2];
   int fd , cstdout;
   char buf[PATH_MAX+1] = {0};

  if (pipe (pd) == 0)
  {

    if ((pid = fork()) == 0)
    {
        /*child process*/
      close(0);
      dup(pd[0]);

/* redirect to lp0 */
    if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", info->printer_name, "device", buf, PATH_MAX) < 0)
    { 
        perror ("Read config file error, use default! \n");  
        strcpy(buf, "/dev/lp0"); 
    }  

      if ((fd = open (buf , O_RDWR|O_SYNC)) == -1 ){
        perror ("Unable to open /dev/lp0 \n");  
        exit(0);
      }
      else
      {
        close(1);
        dup(fd);
        close (fd); 
      }
      close(pd[0]);
      close(pd[1]);
   
      memset (buf , 0 , sizeof (buf));


    if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", info->printer_name, "filter", buf, PATH_MAX) < 0)
        exit(0);
    else
        execl (buf, buf, (char*)0);
    }
    else if (pid < 0)
      perror ("Unable to fork rastertoprinter\n");

  }
  else
  {
     perror ("Can not create the pipes!");
  }

    cstdout = dup(1);

    close(1);
    dup(pd[1]);
    close(pd[0]);
    close(pd[1]);

  ras   = RasterOpen(1, RASTER_WRITE);   /*output to stdout*/
#endif


  blank = img->colorspace < 0 ? 0 : ~0;

/*force only page */
xpages = ypages =1;

  for (i = 0, page = 1; i < Copies; i ++)
    for (xpage = 0; xpage < xpages; xpage ++)
      for (ypage = 0; ypage < ypages; ypage ++, page ++)
      {

        fprintf(stderr, "INFO: Formatting page %d...\n", page);

        if (Orientation & 1)
        {
              x0    = img->xsize * ypage / ypages;
              x1    = img->xsize * (ypage + 1) / ypages - 1;
              y0    = img->ysize * xpage / xpages;
              y1    = img->ysize * (xpage + 1) / xpages - 1;

              xtemp = (int)(header.HWResolution[0] * yprint);
              ytemp = (int)(header.HWResolution[1] * xprint);
        }
        else
        {
              x0    = img->xsize * xpage / xpages;
              x1    = img->xsize * (xpage + 1) / xpages - 1;
              y0    = img->ysize * ypage / ypages;
              y1    = img->ysize * (ypage + 1) / ypages - 1;

              xtemp = (int)(header.HWResolution[0] * xprint);
              ytemp = (int)(header.HWResolution[1] * yprint);
        }
        RasterWriteHeader(ras, &header);

    for (plane = 0; plane < num_planes; plane ++)
	{
	 /*
	  * Initialize the image "zoom" engine...
	  */

          if (Flip)
	         z = ImageZoomAlloc(img, x0, y0, x1, y1, -xtemp, ytemp,
	                       Orientation & 1);
          else
	         z = ImageZoomAlloc(img, x0, y0, x1, y1, xtemp, ytemp,
	                       Orientation & 1);

         /*
	  * Write leading blank space as needed...
	  */

          if (header.Height > z->ysize && YPosition <= 0)
          {
            memset(row, blank, header.BytesPerLine);

                y = header.Height - z->ysize;
            if (YPosition == 0)
              y /= 2;


                for (; y > 0; y --)
                {
                      if ( RasterWritePixels(ras, row, header.BytesPerLine) < header.BytesPerLine)
                      {
                            fputs("ERROR: Unable to write raster data to driver!\n", stderr);
                            free(row);
                            RasterClose(ras);
                            ImageClose(img);
                            ppdClose(ppd);
                            ImageZoomFree(z);
#ifdef NOUNIX
                            goto error;
#else
                            return 1;
#endif
                      }
                }
          }

         /*
	  * Then write image data...
	  */

	  for (y = z->ysize, yerr0 = 0, yerr1 = z->ysize, iy = 0, last_iy = -2;
               y > 0;
               y --)
	  {
	    if (iy != last_iy)
	    {
	      if (header.BitsPerColor >= 8)
	      {
	       /*
	        * Do bilinear interpolation for 8+ bpp images...
		*/

        	if ((iy - last_iy) > 1)
        	  ImageZoomFill(z, iy);

        	ImageZoomFill(z, iy + z->yincr);
           }
          else
	      {
	       /*
	        * Just do nearest-neighbor sampling for < 8 bpp images...
		*/

        	ImageZoomQFill(z, iy);
	      }

              last_iy = iy;
	    }

           /*
	    * Format this line of raster data for the printer...
	    */

    	    memset(row, blank, header.BytesPerLine);

            r0 = z->rows[z->row];
            r1 = z->rows[1 - z->row];

        switch (header.ColorSpace)
	    {
	      case CSPACE_W :
	          format_W(&header, row, y, plane, z->xsize, z->ysize,
		           yerr0, yerr1, r0, r1);
		  break;
              default :
	      case CSPACE_RGB :
	          format_RGB(&header, row, y, plane, z->xsize, z->ysize,
		             yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_RGBA :
	          format_RGBA(&header, row, y, plane, z->xsize, z->ysize,
		              yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_K :
	      case CSPACE_WHITE :
	      case CSPACE_GOLD :
	      case CSPACE_SILVER :
	          format_K(&header, row, y, plane, z->xsize, z->ysize,
		           yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_CMY :
	          format_CMY(&header, row, y, plane, z->xsize, z->ysize,
		             yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_YMC :
	          format_YMC(&header, row, y, plane, z->xsize, z->ysize,
		             yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_CMYK :
	          format_CMYK(&header, row, y, plane, z->xsize, z->ysize,
		              yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_YMCK :
	      case CSPACE_GMCK :
	      case CSPACE_GMCS :
	          format_YMCK(&header, row, y, plane, z->xsize, z->ysize,
		              yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_KCMY :
	          format_KCMY(&header, row, y, plane, z->xsize, z->ysize,
		              yerr0, yerr1, r0, r1);
		  break;
	      case CSPACE_KCMYcm :
	          format_KCMYcm(&header, row, y, plane, z->xsize, z->ysize,
		                yerr0, yerr1, r0, r1);
		  break;
	    }

           /*
	    * Write the raster data to the driver...
	    */

	    if (RasterWritePixels(ras, row, header.BytesPerLine) <
	                              header.BytesPerLine)
	    {
            fputs("ERROR: Unable to write raster data to driver!\n", stderr);
            free(row);
            RasterClose(ras);
            ImageClose(img);
            ppdClose(ppd);
            ImageZoomFree(z);
#ifdef NOUNIX
            goto error;
#else
            return 1;
#endif
	    }

           /*
	    * Compute the next scanline in the image...
	    */

	    iy    += z->ystep;
	    yerr0 += z->ymod;
	    yerr1 -= z->ymod;
	    if (yerr1 <= 0)
	    {
              yerr0 -= z->ysize;
              yerr1 += z->ysize;
              iy    += z->yincr;
	    }
	  }

         /*
	  * Write trailing blank space as needed...
	  */

      if (header.Height > z->ysize && YPosition >= 0)
	  {
	    memset(row, blank, header.BytesPerLine);

            y = header.Height - z->ysize;
	    if (YPosition == 0)
	      y = y - y / 2;


	    for (; y > 0; y --)
	    {
	      if (RasterWritePixels(ras, row, header.BytesPerLine) <
	              header.BytesPerLine)
	      {
                fputs("ERROR: Unable to write raster data to driver!\n", stderr);
                free(row);
                RasterClose(ras);
                ImageClose(img);
                ppdClose(ppd);
                ImageZoomFree(z);
#ifdef NOUNIX
                goto error;
#else
                return 1;
#endif
	      }
        }
	  }

         /*
	  * Free memory used for the "zoom" engine...
	  */
          ImageZoomFree(z);
        }
   }
 /*
  * Close files...
  */

  free(row);
  RasterClose(ras);
  ImageClose(img);
  ppdClose(ppd);


#ifdef NOUNIX

   ioctl(write_fd, FIOFLUSH, 0);
   initialized = 0;
   close(write_fd);
   pthread_join (th, NULL);
   close(driver_fd);
#else 
    fflush(stdout);
    
   waitpid (pid, NULL, 0);
   close(1);
   dup(cstdout);
   close(cstdout);
#endif
  return (0);

#ifdef NOUNIX
error:
   initialized = 0;
   close(write_fd);
   pthread_cancel(th);
   close(driver_fd);
   return 1;
#endif
}


/*
 * 'exec_code()' - Execute PostScript setpagedevice commands as appropriate.
 */

static void
exec_code(PrinterPageHeader *header,	/* I - Page header */
          const char         *code)	/* I - Option choice to execute */
{
  char	*ptr;				/* Pointer into name/value string */
 char	name[255];			/* Name of pagedevice entry */
char 	value[1024];			/* Value of pagedevice entry */


  for (; *code != '\0';)
  {
   /*
    * Search for the start of a dictionary name...
    */

    while (*code != '/' && *code != '\0')
      code ++;

    if (*code == '\0')
      break;

   /*
    * Get the name...
    */

    code ++;
    for (ptr = name; isalnum(*code & 255) && (ptr - name) < (int)(sizeof(name) - 1);)
      *ptr++ = *code++;
    *ptr = '\0';

   /*
    * The parse the value as needed...
    */

    while (isspace(*code & 255))
      code ++;

    if (*code == '\0')
      break;

    if (*code == '[')
    {
     /*
      * Read array of values...
      */

      code ++;
      for (ptr = value;
           *code != ']' && *code != '\0' &&
	       (ptr - value) < (int)(sizeof(value) - 1);)
	*ptr++ = *code++;
      *ptr = '\0';
    }
    else if (*code == '(')
    {
     /*
      * Read string value...
      */

      code ++;
      for (ptr = value;
           *code != ')' && *code != '\0' &&
	       (ptr - value) < (int)(sizeof(value) - 1);)
        if (*code == '\\')
	{
	  code ++;
	  if (isdigit(*code & 255))
	    *ptr++ = (char)strtol(code, (char **)&code, 8);
          else
	    *ptr++ = *code++;
	}
	else
          *ptr++ = *code++;

      *ptr = '\0';
    }
    else if (isdigit(*code & 255) || *code == '-')
    {
     /*
      * Read single number...
      */

      for (ptr = value;
           (isdigit(*code & 255) || *code == '-') &&
	       (ptr - value) < (int)(sizeof(value) - 1);)
	*ptr++ = *code++;
      *ptr = '\0';
    }
    else
    {
     /*
      * Read a single name...
      */

      for (ptr = value;
           (isalnum(*code & 255) || *code == '_') &&
	       (ptr - value) < (int)(sizeof(value) - 1);)
	*ptr++ = *code++;
      *ptr = '\0';
    }

   /*
    * Assign the value as needed...
    */

    if (!strcmp(name, "MediaClass"))
      strncpy(header->MediaClass, value, sizeof(header->MediaClass));
    else if (!strcmp(name, "MediaColor"))
      strncpy(header->MediaColor, value, sizeof(header->MediaColor));
    else if (!strcmp(name, "MediaType"))
      strncpy(header->MediaType, value, sizeof(header->MediaType));
    else if (!strcmp(name, "OutputType"))
      strncpy(header->OutputType, value, sizeof(header->OutputType));
    else if (!strcmp(name, "AdvanceDistance"))
      header->AdvanceDistance = atoi(value);
    else if (!strcmp(name, "AdvanceMedia"))
      header->AdvanceMedia = (PRINTER_ADVANCE)atoi(value);
    else if (!strcmp(name, "Collate"))
      header->Collate = !strcmp(value, "true");
    else if (!strcmp(name, "CutMedia"))
      header->CutMedia = (PRINTER_CUT)atoi(value);
    else if (!strcmp(name, "Duplex"))
      header->Duplex = !strcmp(value, "true");
    else if (!strcmp(name, "HWResolution"))
      sscanf(value, "%d%d", header->HWResolution + 0, header->HWResolution + 1);
    else if (!strcmp(name, "InsertSheet"))
      header->InsertSheet = !strcmp(value, "true");
    else if (!strcmp(name, "Jog"))
      header->Jog = (PRINTER_JOG)atoi(value);
    else if (!strcmp(name, "LeadingEdge"))
      header->LeadingEdge = (PRINTER_EDGE)atoi(value);
    else if (!strcmp(name, "Margins"))
      sscanf(value, "%d%d", header->Margins + 0, header->Margins + 1);
    else if (!strcmp(name, "ManualFeed"))
      header->ManualFeed = !strcmp(value, "true");
    else if (!strcmp(name, "MediaPosition") || /* Compatibility */
             !strcmp(name, "MediaPosition"))
      header->MediaPosition = atoi(value);
    else if (!strcmp(name, "MediaWeight"))
      header->MediaWeight = atoi(value);
    else if (!strcmp(name, "MirrorPrint"))
      header->MirrorPrint = !strcmp(value, "true");
    else if (!strcmp(name, "NegativePrint"))
      header->NegativePrint = !strcmp(value, "true");
    else if (!strcmp(name, "Orientation"))
      header->Orientation = (PRINTER_ORIENT)atoi(value);
    else if (!strcmp(name, "OutputFaceUp"))
      header->OutputFaceUp = !strcmp(value, "true");
    else if (!strcmp(name, "Separations"))
      header->Separations = !strcmp(value, "true");
    else if (!strcmp(name, "TraySwitch"))
      header->TraySwitch = !strcmp(value, "true");
    else if (!strcmp(name, "Tumble"))
      header->Tumble = !strcmp(value, "true");
    else if (!strcmp(name, "MediaType"))
      header->PageMediaType = atoi(value);
    else if (!strcmp(name, "BitsPerColor"))
      header->BitsPerColor = atoi(value);
    else if (!strcmp(name, "ColorOrder"))
      header->ColorOrder = (PRINTER_ORDER)atoi(value);
    else if (!strcmp(name, "ColorSpace"))
      header->ColorSpace = (PRINTER_CSPACE)atoi(value);
    else if (!strcmp(name, "Compression"))
      header->Compression = atoi(value);
    else if (!strcmp(name, "RowCount"))
      header->RowCount = atoi(value);
    else if (!strcmp(name, "RowFeed"))
      header->RowFeed = atoi(value);
    else if (!strcmp(name, "RowStep"))
      header->RowStep = atoi(value);
  }
}


/*
 * 'format_CMY()' - Convert image data to CMY.
 */

static void
format_CMY(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 3;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 64 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --)
              {
	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 2;
		else
        	{
        	  bitmask = 64;
        	  ptr ++;
        	}
              }
              break;

          case 2 :
	      dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
	       	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[0]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[1]]);

        	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr++ ^= (0x03 & OnPixels[r0[2]]);
        	else
        	  *ptr++ ^= (0x03 & OffPixels[r0[2]]);
              }
              break;

          case 4 :
	      dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[0]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[0]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[1]]);

        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[2]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[2]]);
              }
              break;

          case 8 :
              for (x = xsize  * 3; x > 0; x --, r0 ++, r1 ++)
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              break;
        }
        break;

    case ORDER_BANDED :
	cptr = ptr;
	mptr = ptr + bandwidth;
	yptr = ptr + 2 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              switch (z)
	      {
	        case 0 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[0] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 1 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[1] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 2 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[2] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];
              r0 += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];
              r0 += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              r0 += z;
	      r1 += z;

              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_CMYK()' - Convert image data to CMYK.
 */

static void
format_CMYK(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		*kptr,		/* Pointer into black */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 4;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 128 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --)
              {
	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
        	{
        	  bitmask = 128;
        	  ptr ++;
        	}
              }
              break;

          case 2 :
	      dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
	       	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr ^= (0xc0 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0xc0 & OffPixels[r0[0]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[1]]);

        	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[2]]);

        	if ((r0[3] & 63) > dither[x & 7])
        	  *ptr++ ^= (0x03 & OnPixels[r0[3]]);
        	else
        	  *ptr++ ^= (0x03 & OffPixels[r0[3]]);
              }
              break;

          case 4 :
	      dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[0]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[1]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[1]]);

        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[2]]);

        	if ((r0[3] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[3]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[3]]);
              }
              break;

          case 8 :
              for (x = xsize  * 4; x > 0; x --, r0 ++, r1 ++)
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              break;
        }
        break;

    case ORDER_BANDED :
	cptr = ptr;
	mptr = ptr + bandwidth;
	yptr = ptr + 2 * bandwidth;
	kptr = ptr + 3 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *kptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[3] == r1[3])
                  *kptr++ = r0[3];
        	else
                  *kptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];
              r0      += z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if (*r0 > dither[x & 15])
        	  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  ptr ++;
        	}
	      }
	      break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];
              r0      += z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];
              r0 += z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              r0 += z;
	      r1 += z;

              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_K()' - Convert image data to black.
 */

static void
format_K(PrinterPageHeader *header,	/* I - Page header */
         unsigned char      *row,	/* IO - Bitmap data for device */
	 int                y,		/* I - Current row */
	 int                z,		/* I - Current plane */
	 int                xsize,	/* I - Width of image data */
	 int	            ysize,	/* I - Height of image data */
	 int                yerr0,	/* I - Top Y error */
	 int                yerr1,	/* I - Bottom Y error */
	 ib_t               *r0,	/* I - Primary image data */
	 ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  (void)z;

  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr = row + bitoffset / 8;

  switch (header->BitsPerColor)
  {
    case 1 :
        bitmask = 0x80 >> (bitoffset & 7);
        dither  = Floyd16x16[y & 15];

        for (x = xsize; x > 0; x --)
        {
          if (*r0++ > dither[x & 15])
            *ptr ^= bitmask;

          if (bitmask > 1)
	    bitmask >>= 1;
	  else
	  {
	    bitmask = 0x80;
	    ptr ++;
          }
	}
        break;

    case 2 :
        bitmask = 0xc0 >> (bitoffset & 7);
        dither  = Floyd8x8[y & 7];

        for (x = xsize; x > 0; x --)
        {
          if ((*r0 & 63) > dither[x & 7])
            *ptr ^= (bitmask & OnPixels[*r0++]);
          else
            *ptr ^= (bitmask & OffPixels[*r0++]);

          if (bitmask > 3)
	    bitmask >>= 2;
	  else
	  {
	    bitmask = 0xc0;

	    ptr ++;
          }
	}
        break;

    case 4 :
        bitmask = 0xf0 >> (bitoffset & 7);
        dither  = Floyd4x4[y & 3];

        for (x = xsize; x > 0; x --)
        {
          if ((*r0 & 15) > dither[x & 3])
            *ptr ^= (bitmask & OnPixels[*r0++]);
          else
            *ptr ^= (bitmask & OffPixels[*r0++]);

          if (bitmask == 0xf0)
	    bitmask = 0x0f;
	  else
	  {
	    bitmask = 0xf0;

	    ptr ++;
          }
	}
        break;

    case 8 :
        for (x = xsize; x > 0; x --, r0 ++, r1 ++)
	{
          if (*r0 == *r1)
            *ptr++ = *r0;
          else
            *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
        }
        break;
  }
}


/*
 * 'format_KCMY()' - Convert image data to KCMY.
 */

static void
format_KCMY(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		*kptr,		/* Pointer into black */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 4;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 128 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
	        if (r0[3] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[0] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[1] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[2] > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
        	{
        	  bitmask = 128;
        	  ptr ++;
        	}
              }
              break;

          case 2 :
              dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
	       	if ((r0[3] & 63) > dither[x & 7])
        	  *ptr ^= (0xc0 & OnPixels[r0[3]]);
        	else
        	  *ptr ^= (0xc0 & OffPixels[r0[3]]);

        	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[0]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[1]]);

        	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr++ ^= (0x03 & OnPixels[r0[2]]);
        	else
        	  *ptr++ ^= (0x03 & OffPixels[r0[2]]);
              }
              break;

          case 4 :
              dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
        	if ((r0[3] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[3]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[3]]);

        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[0]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[0]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[1]]);

        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[2]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[2]]);
              }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[3] == r1[3])
                  *ptr++ = r0[3];
        	else
                  *ptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;

        	if (r0[0] == r1[0])
                  *ptr++ = r0[0];
        	else
                  *ptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *ptr++ = r0[1];
        	else
                  *ptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *ptr++ = r0[2];
        	else
                  *ptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_BANDED :
	kptr = ptr;
	cptr = ptr + bandwidth;
	mptr = ptr + 2 * bandwidth;
	yptr = ptr + 3 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *kptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
              dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
              dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[3] == r1[3])
                  *kptr++ = r0[3];
        	else
                  *kptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];
	      if (z == 0)
	        r0 += 3;
	      else
	        r0 += z - 1;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if (*r0 > dither[x & 15])
        	  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  ptr ++;
        	}
	      }
	      break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
              dither  = Floyd8x8[y & 7];
              if (z == 0)
	        r0 += 3;
	      else
	        r0 += z - 1;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
              dither  = Floyd4x4[y & 3];
              if (z == 0)
	        r0 += 3;
	      else
	        r0 += z - 1;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              if (z == 0)
	      {
	        r0 += 3;
	        r1 += 3;
	      }
	      else
	      {
	        r0 += z - 1;
	        r1 += z - 1;
	      }

              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_KCMYcm()' - Convert image data to KCMYcm.
 */

static void
format_KCMYcm(PrinterPageHeader *header,/* I - Page header */
              unsigned char      *row,	/* IO - Bitmap data for device */
	      int                y,	/* I - Current row */
	      int                z,	/* I - Current plane */
	      int                xsize,	/* I - Width of image data */
	      int	         ysize,	/* I - Height of image data */
	      int                yerr0,	/* I - Top Y error */
	      int                yerr1,	/* I - Bottom Y error */
	      ib_t               *r0,	/* I - Primary image data */
	      ib_t               *r1)	/* I - Image data for interpolation */
{
  int		pc, pm, py, pk;	/* Cyan, magenta, yellow, and black values */
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		*kptr,		/* Pointer into black */
		*lcptr,		/* Pointer into light cyan */
		*lmptr,		/* Pointer into light magenta */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr = row + bitoffset / 8;
  if (header->BitsPerColor == 1)
    bandwidth = header->BytesPerLine / 6;
  else
    bandwidth = header->BytesPerLine / 4;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --)
              {
	        pc = *r0++ > dither[x & 15];
		pm = *r0++ > dither[x & 15];
		py = *r0++ > dither[x & 15];
		pk = *r0++ > dither[x & 15];

	        if (pk)
		  *ptr++ ^= 32;	/* Black */
		else if (pc && pm)
		  *ptr++ ^= 17;	/* Blue (cyan + light magenta) */
		else if (pc && py)
		  *ptr++ ^= 6;	/* Green (light cyan + yellow) */
		else if (pm && py)
		  *ptr++ ^= 12;	/* Red (magenta + yellow) */
		else if (pc)
		  *ptr++ ^= 16;
		else if (pm)
		  *ptr++ ^= 8;
		else if (py)
		  *ptr++ ^= 4;
              }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[3] == r1[3])
                  *ptr++ = r0[3];
        	else
                  *ptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;

        	if (r0[0] == r1[0])
                  *ptr++ = r0[0];
        	else
                  *ptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *ptr++ = r0[1];
        	else
                  *ptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *ptr++ = r0[2];
        	else
                  *ptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_BANDED :
	kptr  = ptr;
	cptr  = ptr + bandwidth;
	mptr  = ptr + 2 * bandwidth;
	yptr  = ptr + 3 * bandwidth;
	lcptr = ptr + 4 * bandwidth;
	lmptr = ptr + 5 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
	        pc = *r0++ > dither[x & 15];
		pm = *r0++ > dither[x & 15];
		py = *r0++ > dither[x & 15];
		pk = *r0++ > dither[x & 15];

	        if (pk)
		  *kptr ^= bitmask;	/* Black */
		else if (pc && pm)
		{
		  *cptr ^= bitmask;	/* Blue (cyan + light magenta) */
		  *lmptr ^= bitmask;
		}
		else if (pc && py)
		{
		  *lcptr ^= bitmask;	/* Green (light cyan + yellow) */
		  *yptr  ^= bitmask;
		}
		else if (pm && py)
		{
		  *mptr ^= bitmask;	/* Red (magenta + yellow) */
		  *yptr ^= bitmask;
		}
		else if (pc)
		  *cptr ^= bitmask;
		else if (pm)
		  *mptr ^= bitmask;
		else if (py)
		  *yptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
		  lcptr ++;
		  lmptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[3] == r1[3])
                  *kptr++ = r0[3];
        	else
                  *kptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              switch (z)
	      {
	        case 0 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[3] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 1 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[0] > dither[x & 15] &&
			  r0[2] < dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 2 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[1] > dither[x & 15] &&
			  (r0[0] < dither[x & 15] ||
			   r0[2] > dither[x & 15]))
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 3 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[2] > dither[x & 15] &&
			  (r0[0] < dither[x & 15] ||
			   r0[1] < dither[x & 15]))
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 4 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[0] > dither[x & 15] &&
			  r0[2] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 5 :
        	    for (x = xsize; x > 0; x --, r0 += 4)
        	    {
        	      if (r0[0] > dither[x & 15] &&
			  r0[1] > dither[x & 15] &&
			  r0[2] < dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;
	      }
              break;

          case 8 :
              if (z == 0)
	      {
	        r0 += 3;
	        r1 += 3;
	      }
	      else
	      {
	        r0 += z - 1;
	        r1 += z - 1;
	      }

              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_RGBA()' - Convert image data to RGBA.
 */

static void
format_RGBA(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 4;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 128 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --)
              {
	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (*r0++ > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 2)
		{
		  *ptr ^= 16;
		  bitmask >>= 2;
		}
		else
        	{
        	  bitmask = 128;
        	  *ptr++ ^= 1;
        	}
              }
              break;

          case 2 :
	      dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
	       	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr ^= (0xc0 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0xc0 & OffPixels[r0[0]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[1]]);

        	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[2]]);

                *ptr++ ^= 0x03;
              }
              break;

          case 4 :
	      dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[0]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[1]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[1]]);

        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[2]]);

                *ptr++ ^= 0x0f;
              }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (r0[0] == r1[0])
                  *ptr++ = r0[0];
        	else
                  *ptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *ptr++ = r0[1];
        	else
                  *ptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *ptr++ = r0[2];
        	else
                  *ptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

                *ptr++ = 255;
              }
	      break;
        }
        break;

    case ORDER_BANDED :
	cptr = ptr;
	mptr = ptr + bandwidth;
	yptr = ptr + 2 * bandwidth;

        memset(ptr + 3 * bandwidth, 255, bandwidth);

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        if (z == 3)
	{
          memset(row, 255, header->BytesPerLine);
	  break;
        }

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              switch (z)
	      {
	        case 0 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[0] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 1 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[1] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 2 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[2] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];
              r0 += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];
              r0 += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              r0 += z;
	      r1 += z;

              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_W()' - Convert image data to luminance.
 */

static void
format_W(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  (void)z;

  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr = row + bitoffset / 8;

  switch (header->BitsPerColor)
  {
    case 1 :
        bitmask = 0x80 >> (bitoffset & 7);
        dither  = Floyd16x16[y & 15];

        for (x = xsize; x > 0; x --)
        {
          if (*r0++ > dither[x & 15])
            *ptr ^= bitmask;

          if (bitmask > 1)
	    bitmask >>= 1;
	  else
	  {
	    bitmask = 0x80;
	    ptr ++;
          }
	}
        break;

    case 2 :
        bitmask = 0xc0 >> (bitoffset & 7);
        dither  = Floyd8x8[y & 7];

        for (x = xsize; x > 0; x --)
        {
          if ((*r0 & 63) > dither[x & 7])
            *ptr ^= (bitmask & OnPixels[*r0++]);
          else
            *ptr ^= (bitmask & OffPixels[*r0++]);

          if (bitmask > 3)
	    bitmask >>= 2;
	  else
	  {
	    bitmask = 0xc0;

	    ptr ++;
          }
	}
        break;

    case 4 :
        bitmask = 0xf0 >> (bitoffset & 7);
        dither  = Floyd4x4[y & 3];

        for (x = xsize; x > 0; x --)
        {
          if ((*r0 & 15) > dither[x & 3])
            *ptr ^= (bitmask & OnPixels[*r0++]);
          else
            *ptr ^= (bitmask & OffPixels[*r0++]);

          if (bitmask == 0xf0)
	    bitmask = 0x0f;
	  else
	  {
	    bitmask = 0xf0;

	    ptr ++;
          }
	}
        break;

    case 8 :
        for (x = xsize; x > 0; x --, r0 ++, r1 ++)
	{
          if (*r0 == *r1)
            *ptr++ = *r0;
          else
            *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
        }
        break;
  }
}


/*
 * 'format_YMC()' - Convert image data to YMC.
 */

static void
format_YMC(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 3;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 64 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
	        if (r0[2] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[1] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[0] > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 2;
		else
        	{
        	  bitmask = 64;
        	  ptr ++;
        	}
              }
              break;

          case 2 :
	      dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
	       	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[2]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[1]]);

        	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr++ ^= (0x03 & OnPixels[r0[0]]);
        	else
        	  *ptr++ ^= (0x03 & OffPixels[r0[0]]);
              }
              break;

          case 4 :
	      dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 3)
              {
        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[2]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[2]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[1]]);

        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[0]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[0]]);
              }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (r0[2] == r1[2])
                  *ptr++ = r0[2];
        	else
                  *ptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *ptr++ = r0[1];
        	else
                  *ptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[0] == r1[0])
                  *ptr++ = r0[0];
        	else
                  *ptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;
              }
	      break;
        }
        break;

    case ORDER_BANDED :
	yptr = ptr;
	mptr = ptr + bandwidth;
	cptr = ptr + 2 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
	      dither  = Floyd16x16[y & 15];

              switch (z)
	      {
	        case 2 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[0] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 1 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[1] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;

	        case 0 :
        	    for (x = xsize; x > 0; x --, r0 += 3)
        	    {
        	      if (r0[2] > dither[x & 15])
        		*ptr ^= bitmask;

                      if (bitmask > 1)
			bitmask >>= 1;
		      else
		      {
			bitmask = 0x80;
			ptr ++;
        	      }
	            }
		    break;
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
	      dither  = Floyd8x8[y & 7];
              z       = 2 - z;
              r0      += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
	      dither  = Floyd4x4[y & 3];
              z       = 2 - z;
              r0      += z;

              for (x = xsize; x > 0; x --, r0 += 3)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              z  = 2 - z;
              r0 += z;
	      r1 += z;

              for (x = xsize; x > 0; x --, r0 += 3, r1 += 3)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'format_YMCK()' - Convert image data to YMCK.
 */

static void
format_YMCK(PrinterPageHeader *header,	/* I - Page header */
            unsigned char      *row,	/* IO - Bitmap data for device */
	    int                y,	/* I - Current row */
	    int                z,	/* I - Current plane */
	    int                xsize,	/* I - Width of image data */
	    int	               ysize,	/* I - Height of image data */
	    int                yerr0,	/* I - Top Y error */
	    int                yerr1,	/* I - Bottom Y error */
	    ib_t               *r0,	/* I - Primary image data */
	    ib_t               *r1)	/* I - Image data for interpolation */
{
  ib_t		*ptr,		/* Pointer into row */
		*cptr,		/* Pointer into cyan */
		*mptr,		/* Pointer into magenta */
		*yptr,		/* Pointer into yellow */
		*kptr,		/* Pointer into black */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		bandwidth;	/* Width of a color band */
  int		x,		/* Current X coordinate on page */
		*dither;	/* Pointer into dither array */


  switch (XPosition)
  {
    case -1 :
        bitoffset = 0;
	break;
    default :
        bitoffset = header->BitsPerPixel * ((header->Width - xsize) / 2);
	break;
    case 1 :
        bitoffset = header->BitsPerPixel * (header->Width - xsize);
	break;
  }

  ptr       = row + bitoffset / 8;
  bandwidth = header->BytesPerLine / 4;

  switch (header->ColorOrder)
  {
    case ORDER_CHUNKED :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 128 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
	        if (r0[2] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[1] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[0] > dither[x & 15])
		  *ptr ^= bitmask;
		bitmask >>= 1;

	        if (r0[3] > dither[x & 15])
		  *ptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
        	{
        	  bitmask = 128;

        	  ptr ++;
        	}
              }
              break;

          case 2 :
              dither = Floyd8x8[y & 7];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
	       	if ((r0[2] & 63) > dither[x & 7])
        	  *ptr ^= (0xc0 & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0xc0 & OffPixels[r0[2]]);

        	if ((r0[1] & 63) > dither[x & 7])
        	  *ptr ^= (0x30 & OnPixels[r0[1]]);
        	else
        	  *ptr ^= (0x30 & OffPixels[r0[1]]);

        	if ((r0[0] & 63) > dither[x & 7])
        	  *ptr ^= (0x0c & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0x0c & OffPixels[r0[0]]);

        	if ((r0[3] & 63) > dither[x & 7])
        	  *ptr++ ^= (0x03 & OnPixels[r0[3]]);
        	else
        	  *ptr++ ^= (0x03 & OffPixels[r0[3]]);
              }
              break;

          case 4 :
              dither = Floyd4x4[y & 3];

              for (x = xsize ; x > 0; x --, r0 += 4)
              {
        	if ((r0[2] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[2]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[2]]);

        	if ((r0[1] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[1]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[1]]);

        	if ((r0[0] & 15) > dither[x & 3])
        	  *ptr ^= (0xf0 & OnPixels[r0[0]]);
        	else
        	  *ptr ^= (0xf0 & OffPixels[r0[0]]);

        	if ((r0[3] & 15) > dither[x & 3])
        	  *ptr++ ^= (0x0f & OnPixels[r0[3]]);
        	else
        	  *ptr++ ^= (0x0f & OffPixels[r0[3]]);
              }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[2] == r1[2])
                  *ptr++ = r0[2];
        	else
                  *ptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *ptr++ = r0[1];
        	else
                  *ptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[0] == r1[0])
                  *ptr++ = r0[0];
        	else
                  *ptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[3] == r1[3])
                  *ptr++ = r0[3];
        	else
                  *ptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_BANDED :
	yptr = ptr;
	mptr = ptr + bandwidth;
	cptr = ptr + 2 * bandwidth;
	kptr = ptr + 3 * bandwidth;

        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              for (x = xsize; x > 0; x --)
              {
        	if (*r0++ > dither[x & 15])
        	  *cptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *mptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *yptr ^= bitmask;
        	if (*r0++ > dither[x & 15])
        	  *kptr ^= bitmask;

                if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
              dither  = Floyd8x8[y & 7];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 63) > dither[x & 7])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
              dither  = Floyd4x4[y & 3];

              for (x = xsize; x > 0; x --)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *cptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *cptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *mptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *mptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *yptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *yptr ^= (bitmask & OffPixels[*r0++]);

        	if ((*r0 & 15) > dither[x & 3])
        	  *kptr ^= (bitmask & OnPixels[*r0++]);
        	else
        	  *kptr ^= (bitmask & OffPixels[*r0++]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
        	}
	      }
              break;

          case 8 :
              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (r0[0] == r1[0])
                  *cptr++ = r0[0];
        	else
                  *cptr++ = (r0[0] * yerr0 + r1[0] * yerr1) / ysize;

        	if (r0[1] == r1[1])
                  *mptr++ = r0[1];
        	else
                  *mptr++ = (r0[1] * yerr0 + r1[1] * yerr1) / ysize;

        	if (r0[2] == r1[2])
                  *yptr++ = r0[2];
        	else
                  *yptr++ = (r0[2] * yerr0 + r1[2] * yerr1) / ysize;

        	if (r0[3] == r1[3])
                  *kptr++ = r0[3];
        	else
                  *kptr++ = (r0[3] * yerr0 + r1[3] * yerr1) / ysize;
              }
              break;
        }
        break;

    case ORDER_PLANAR :
        switch (header->BitsPerColor)
        {
          case 1 :
              bitmask = 0x80 >> (bitoffset & 7);
              dither  = Floyd16x16[y & 15];

              if (z < 3)
	        r0 += 2 - z;
	      else
	        r0 += z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if (*r0 > dither[x & 15])
        	  *ptr ^= bitmask;

        	if (bitmask > 1)
		  bitmask >>= 1;
		else
		{
		  bitmask = 0x80;
		  ptr ++;
        	}
	      }
              break;

          case 2 :
              bitmask = 0xc0 >> (bitoffset & 7);
              dither  = Floyd8x8[y & 7];
              if (z == 3)
	        r0 += 3;
	      else
	        r0 += 2 - z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 63) > dither[x & 7])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask > 3)
		  bitmask >>= 2;
		else
		{
		  bitmask = 0xc0;

		  ptr ++;
        	}
	      }
              break;

          case 4 :
              bitmask = 0xf0 >> (bitoffset & 7);
              dither  = Floyd4x4[y & 3];
              if (z == 3)
	        r0 += 3;
	      else
	        r0 += 2 - z;

              for (x = xsize; x > 0; x --, r0 += 4)
              {
        	if ((*r0 & 15) > dither[x & 3])
        	  *ptr ^= (bitmask & OnPixels[*r0]);
        	else
        	  *ptr ^= (bitmask & OffPixels[*r0]);

                if (bitmask == 0xf0)
		  bitmask = 0x0f;
		else
		{
		  bitmask = 0xf0;

		  ptr ++;
        	}
	      }
              break;

          case 8 :
              if (z == 3)
	      {
	        r0 += 3;
	        r1 += 3;
	      }
	      else
	      {
	        r0 += 2 - z;
	        r1 += 2 - z;
	      }

              for (x = xsize; x > 0; x --, r0 += 4, r1 += 4)
	      {
        	if (*r0 == *r1)
                  *ptr++ = *r0;
        	else
                  *ptr++ = (*r0 * yerr0 + *r1 * yerr1) / ysize;
              }
              break;
        }
        break;
  }
}


/*
 * 'make_lut()' - Make a lookup table given gamma and brightness values.
 */

static void
make_lut(ib_t  *lut,		/* I - Lookup table */
	 int   colorspace,	/* I - Colorspace */
         float g,		/* I - Image gamma */
         float b)		/* I - Image brightness */
{
  int	i;			/* Looping var */
  int	v;			/* Current value */


  g = 1.0 / g;
  b = 1.0 / b;

  for (i = 0; i < 256; i ++)
  {
    if (colorspace < 0)
      v = (int)(255.0 * b * (1.0 - pow(1.0 - (float)i / 255.0, g)) + 0.5);
    else
      v = (int)(255.0 * (1.0 - b * (1.0 - pow((float)i / 255.0, g))) + 0.5);

    if (v < 0)
      *lut++ = 0;
    else if (v > 255)
      *lut++ = 255;
    else
      *lut++ = v;
  }
}


/*
 * End of "$Id: imagetoraster.c,v 1.52 2007-12-06 06:14:49 jpzhang Exp $".
 */
