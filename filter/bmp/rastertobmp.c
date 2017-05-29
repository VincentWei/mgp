
#include "mgpconfig.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>


#include "ppd.h"
#include "raster.h"


/*
 * Macros...
 */



static unsigned char	*Planes[4];		/* Output buffers */

static int     NumPlanes,      /* Number of color planes */
        ColorBits,      /* Number of bits per color */
        Page;           /* Current page number */



static MYBITMAP * pbmp = NULL;



static void HP_EndPage(void);


/*
 * 'HP_StartPage()' - Start a page of graphics.
 */

static void
HP_StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          PrinterPageHeader *header)	/* I - Page header */
{
    int plane;

    pbmp = (MYBITMAP*)malloc (sizeof(MYBITMAP));
    memset (pbmp , 0 , sizeof(MYBITMAP));
    
    pbmp->flags = MYBMP_TYPE_RGB | MYBMP_FLOW_DOWN | MYBMP_RGBSIZE_3;
    pbmp->frames = 1;
    pbmp->depth = 24 ;
    pbmp->w = header->Width;
    pbmp->h = header->Height;
    pbmp->pitch = header->Width*3;
    pbmp->size = header->Width * header->Height*3;  

    pbmp->bits = (BYTE*)calloc (1, header->Width * header->Height * 3);
    memset(pbmp->bits , 0xFF , header->Width * header->Height * 3);


  ColorBits = header->BitsPerColor;

    if (header->ColorSpace == CSPACE_KCMY)
      NumPlanes = 4;
    else if (header->ColorSpace == CSPACE_CMY)
      NumPlanes = 3;
    else
      NumPlanes = 1;
    
  Planes[0] = (unsigned char*)malloc(header->BytesPerLine);
  for (plane = 1; plane < NumPlanes; plane ++)
    Planes[plane] = Planes[0] + plane * header->BytesPerLine / NumPlanes;

 /*
  * Show page device dictionary...
  */
}

/*
 * 'EndPage()' - Finish a page of graphics.
 */
static int __mg_save_bmp (MG_RWops* fp, MYBITMAP* bmp, RGB* pal);

static void HP_EndPage(void)
{
/* save the bmp*/
    int fd , i;
    FILE * pf;
    char pname[64] = {0};
    i = 1;

    while (TRUE){
        
        sprintf(pname , PATH_DRIVER"%d.bmp" , i);    
    
        if ((fd = open(pname , O_CREAT|O_EXCL , S_IRWXU|S_IRWXG|S_IRWXO )) == -1)
        {
            if (errno == EEXIST) {
                i++;
                continue;
             }else
                return;
        }
        else
        {
            close(fd);
            break;
        }
    }
    
    pf = fopen(pname , "wb+");

    __mg_save_bmp ((MG_RWops*)pf , pbmp , NULL);

    fclose(pf);


    free (pbmp->bits);
    free (pbmp);
}

/*
 * 'HP_Shutdown()' - Shutdown the printer.
 */



static void ColorConverLine(const unsigned char * src , unsigned char * dst , int line_count)
{
int i,j;
unsigned char c;
const unsigned char * pc = src;
unsigned char * py = dst;

        for (i = 0; i < line_count; i++) {
            c = 128;
            for (j =0; j < 8; j++)
            {
               if ((*pc)&c){ 
                py[0] = 0;
                py[1] = 0;
                py[2] = 0;
               }
                c >>= 1;
                py+=3;

            }
            pc++;
        }
}

/*
 * 'main()' - Main entry and processing of driver.
 */

#ifdef NOUNIX
int bmp_rasterentry(int in, int out)
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


 /*
  * Make sure status messages are not buffered...
  */
#ifdef NOUNIX

  ras = RasterOpen(in, RASTER_READ);
#else
  setbuf(stderr, NULL);
 /*
  * Open the page stream...
  */
  ras = RasterOpen(0, RASTER_READ);
#endif

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

    HP_StartPage(ppd, &header);

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

      if (RasterReadPixels(ras, Planes[0], header.BytesPerLine) < 1){

        break;
       }

     /*
      * See if the line is blank; if not, write it to the printer...
      */
          if (Planes[0][0] ||
              memcmp(Planes[0], Planes[0] + 1, header.BytesPerLine - 1))
          {
            ColorConverLine(Planes[0] , pbmp->bits+y*header.Width*3 , header.BytesPerLine);
          }
      
     }

   /*
    * Eject the page...
    */

        HP_EndPage();
  }

 /*
  * Shutdown the printer...
  */


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
#if !defined(NOUNIX)
    close (0);   
#endif 

  return (Page == 0);
}

#define SIZEOF_RGBQUAD      4
#define BI_RGB          0
#define WININFOHEADERSIZE  40

#define SIZEOF_BMFH     14
#define SIZEOF_BMIH     40
typedef struct BITMAPFILEHEADER
{
   unsigned short bfType;
   unsigned long  bfSize;
   unsigned short bfReserved1;
   unsigned short bfReserved2;
   unsigned long  bfOffBits;
} BITMAPFILEHEADER;


typedef struct WINBMPINFOHEADER  /* size: 40 */
{
   unsigned long  biSize;
   unsigned long  biWidth;
   unsigned long  biHeight;
   unsigned short biPlanes;
   unsigned short biBitCount;
   unsigned long  biCompression;
   unsigned long  biSizeImage;
   unsigned long  biXPelsPerMeter;
   unsigned long  biYPelsPerMeter;
   unsigned long  biClrUsed;
   unsigned long  biClrImportant;
} WINBMPINFOHEADER;


typedef struct tagRGBQUAD
{
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} RGBQUAD;
typedef RGBQUAD* PRGBQUAD;

inline static int depth2bpp (int depth)
{
    switch (depth) {
    case 4:
    case 8:
        return 1;
    case 15:
    case 16:
        return 2;
    case 24:
        return 3;
    case 32:
        return 4;
    }
    
    return 1;
}

inline void pixel2rgb (gal_pixel pixel, GAL_Color* color, int depth)
{
    switch (depth) {
    case 24:
    case 32:
        color->r = (gal_uint8) ((pixel >> 16) & 0xFF);
        color->g = (gal_uint8) ((pixel >> 8) & 0xFF);
        color->b = (gal_uint8) (pixel & 0xFF);
        break;

    case 15:
        color->r = (gal_uint8)((pixel & 0x7C00) >> 7) | 0x07;
        color->g = (gal_uint8)((pixel & 0x03E0) >> 2) | 0x07;
        color->b = (gal_uint8)((pixel & 0x001F) << 3) | 0x07;
        break;

    case 16:
        color->r = (gal_uint8)((pixel & 0xF800) >> 8) | 0x07;
        color->g = (gal_uint8)((pixel & 0x07E0) >> 3) | 0x03;
        color->b = (gal_uint8)((pixel & 0x001F) << 3) | 0x07;
        break;
    }
}

static void bmpGetHighCScanline (BYTE* bits, BYTE* scanline, 
                        int pixels, int bpp, int depth)
{
    int i;
    gal_pixel c;
    GAL_Color color;

    for (i = 0; i < pixels; i++) {
        c = *((gal_pixel*)bits);

        pixel2rgb (c, &color, depth);
        *(scanline)     = color.b;
        *(scanline + 1) = color.g;
        *(scanline + 2) = color.r;

        bits += bpp;
        scanline += 3;
    }
}

static void bmpGet16CScanline(BYTE* bits, BYTE* scanline, 
                        int pixels)
{
    int i;

    for (i = 0; i < pixels; i++) {
        if (i % 2 == 0)
            *scanline = (bits [i] << 4) & 0xF0;
        else {
            *scanline |= bits [i] & 0x0F;
            scanline ++;
        }
    }
}

static inline void bmpGet256CScanline (BYTE* bits, BYTE* scanline, 
                        int pixels)
{
    memcpy (scanline, bits, pixels);
}

static int __mg_save_bmp (MG_RWops* fp, MYBITMAP* bmp, RGB* pal)
{
    BYTE* scanline = NULL;
    int i, bpp;
    int scanlinebytes;

    BITMAPFILEHEADER bmfh;
    WINBMPINFOHEADER bmih;

    memset (&bmfh, 0, sizeof (BITMAPFILEHEADER));
    bmfh.bfType         = MAKEWORD ('B', 'M');
    bmfh.bfReserved1    = 0;
    bmfh.bfReserved2    = 0;

    memset (&bmih, 0, sizeof (WINBMPINFOHEADER));
    bmih.biSize         = (DWORD)(WININFOHEADERSIZE);
    bmih.biWidth        = (DWORD)(bmp->w);
    bmih.biHeight       = (DWORD)(bmp->h);
    bmih.biPlanes       = 1;
    bmih.biCompression  = BI_RGB;

    bpp = depth2bpp (bmp->depth);
    switch (bmp->depth) {
        case 4:
            scanlinebytes       = (bmih.biWidth + 1)>>1;
            scanlinebytes       = ((scanlinebytes + 3)>>2)<<2;

#ifdef HAVE_ALLOCA
            if (!(scanline = (BYTE*)alloca (scanlinebytes))) return ERR_BMP_MEM;
#else
            if (!(scanline = (BYTE*)malloc (scanlinebytes))) return ERR_BMP_MEM;
#endif
            memset (scanline, 0, scanlinebytes);

            bmih.biSizeImage    = (DWORD)(bmih.biHeight*scanlinebytes);
            bmfh.bfOffBits      = SIZEOF_BMFH + SIZEOF_BMIH
                                    + (SIZEOF_RGBQUAD<<4);
            bmfh.bfSize         = (DWORD)(bmfh.bfOffBits + bmih.biSizeImage);
            bmih.biBitCount     = 4;
            bmih.biClrUsed      = 16L;
            bmih.biClrImportant = 16L;

            MGUI_RWwrite (fp, &bmfh.bfType, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved1, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved2, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfOffBits, sizeof (DWORD), 1);
            
            MGUI_RWwrite (fp, &bmih.biSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biWidth, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biHeight, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biPlanes, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biBitCount, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biCompression, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biSizeImage, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biXPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biYPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biClrUsed, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biClrImportant, sizeof (DWORD), 1);

            for (i = 0; i < 16; i++) {
                RGBQUAD rgbquad;
                rgbquad.rgbRed = pal [i].r;
                rgbquad.rgbBlue = pal [i].b;
                rgbquad.rgbGreen = pal [i].g;
                MGUI_RWwrite (fp, &rgbquad, sizeof (char), sizeof (RGBQUAD));
            }
            
            for (i = bmp->h  - 1; i >= 0; i--) {
                bmpGet16CScanline (bmp->bits + i * bmp->pitch, scanline, bmp->w);
                MGUI_RWwrite (fp, scanline, sizeof (char), scanlinebytes);
            }
        break;

        case 8:
            scanlinebytes       = bmih.biWidth;
            scanlinebytes       = ((scanlinebytes + 3)>>2)<<2;

#ifdef HAVE_ALLOCA
            if (!(scanline = (BYTE*)alloca (scanlinebytes))) return ERR_BMP_MEM;
#else
            if (!(scanline = (BYTE*)malloc (scanlinebytes))) return ERR_BMP_MEM;
#endif
            memset (scanline, 0, scanlinebytes);

            bmih.biSizeImage    = bmih.biHeight*scanlinebytes;
            bmfh.bfOffBits      = SIZEOF_BMFH + SIZEOF_BMIH
                                    + (SIZEOF_RGBQUAD<<8);
            bmfh.bfSize         = bmfh.bfOffBits + bmih.biSizeImage;
            bmih.biBitCount     = 8;
            bmih.biClrUsed      = 256;
            bmih.biClrImportant = 256;

            MGUI_RWwrite (fp, &bmfh.bfType, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved1, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved2, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfOffBits, sizeof (DWORD), 1);

            MGUI_RWwrite (fp, &bmih.biSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biWidth, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biHeight, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biPlanes, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biBitCount, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biCompression, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biSizeImage, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biXPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biYPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biClrUsed, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biClrImportant, sizeof (DWORD), 1);
            
            for (i = 0; i < 256; i++) {
                RGBQUAD rgbquad;
                rgbquad.rgbRed = pal [i].r;
                rgbquad.rgbBlue = pal [i].b;
                rgbquad.rgbGreen = pal [i].g;
                MGUI_RWwrite (fp, &rgbquad, sizeof (char), sizeof (RGBQUAD));
            }
            
            for (i = bmp->h - 1; i >= 0; i--) {
                bmpGet256CScanline (bmp->bits + bmp->pitch * i, scanline, bmp->w);
                MGUI_RWwrite (fp, scanline, sizeof (char), scanlinebytes);
            }
        break;

        default:
            scanlinebytes       = bmih.biWidth*3;
            scanlinebytes       = ((scanlinebytes + 3)>>2)<<2;

#ifdef HAVE_ALLOCA
            if (!(scanline = (BYTE*)alloca (scanlinebytes))) return ERR_BMP_MEM;
#else
            if (!(scanline = (BYTE*)malloc (scanlinebytes))) return ERR_BMP_MEM;
#endif
            memset (scanline, 0, scanlinebytes);

            bmih.biSizeImage    = bmih.biHeight*scanlinebytes;
            bmfh.bfOffBits      = SIZEOF_BMFH + SIZEOF_BMIH;
            bmfh.bfSize         = bmfh.bfOffBits + bmih.biSizeImage;
            bmih.biBitCount     = 24;
#undef MGUI_RWwrite
#define MGUI_RWwrite(a,b,c,d)  fwrite(b,c,d,(FILE*)a) 


            MGUI_RWwrite (fp, &bmfh.bfType, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved1, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfReserved2, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmfh.bfOffBits, sizeof (DWORD), 1);
            
            MGUI_RWwrite (fp, &bmih.biSize, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biWidth, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biHeight, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biPlanes, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biBitCount, sizeof (WORD), 1);
            MGUI_RWwrite (fp, &bmih.biCompression, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biSizeImage, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biXPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biYPelsPerMeter, sizeof (LONG), 1);
            MGUI_RWwrite (fp, &bmih.biClrUsed, sizeof (DWORD), 1);
            MGUI_RWwrite (fp, &bmih.biClrImportant, sizeof (DWORD), 1);

            for (i = bmp->h - 1; i >= 0; i--) {
                bmpGetHighCScanline (bmp->bits + i * bmp->pitch, scanline, 
                                bmp->w, bpp, bmp->depth);
                MGUI_RWwrite (fp, scanline, sizeof (char), scanlinebytes);
            }
        break;
    }

#ifndef HAVE_ALLOCA
    free (scanline);
#endif

    return ERR_BMP_OK;
}

