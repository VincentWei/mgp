/*
 * "$Id: image-dc.c,v 1.12 2007-11-21 04:11:35 jpzhang Exp $"
 *
 *  This file convert DC data to raster.
 *          MiniGUI is a compact cross-platform Graphics User Interface 
 *         (GUI) support system for real-time embedded systems.
 *                  
 *             Copyright (C) 2002-2005 Feynman Software.
 *             Copyright (C) 1998-2002 Wei Yongming.
 */

/*
 * Include necessary headers...
 */

#include "mgpconfig.h"
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "image.h"
#include "printing.h"


/*
 * Constants for the bitmap compression...
 */

#  define BI_RGB       0             /* No compression - straight BGR data */
#  define BI_RLE8      1             /* 8-bit run-length compression */
#  define BI_RLE4      2             /* 4-bit run-length compression */
#  define BI_BITFIELDS 3             /* RGB bitmap with RGB masks */



/*
 * 'ImageReadDC()' - Read a image_t image from DC.
 */

int					/* O - Read status */
ImageReadDC(image_t    *img,		/* IO - Image */
             HDC        hdc,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut , void * context)		/* I - Lookup table for gamma/brightness */
{
  int	depth,			/* Depth of image (bits) */
		compression,		/* Type of compression */
		bpp,			/* Bytes per pixel */
		x, y, pr;			/* Looping vars */
  ib_t	*in,			/* Input pixels */
		*out,			/* Output pixels */
		*ptr,			/* Pointer into pixels */
        *src;
  GAL_Color		colormap[256];	/* Colormap */

  RECT rc;
  int width , height , pitch;
  int top , left , bottom , right , tmp;

   HDC mem;
   MGPRINT_INFO * info = (MGPRINT_INFO*)context;
    

 /*
  * Then the bitmap information...
  */

    rc.left = 0;
    rc.top = 0;
    width = rc.right = GetGDCapability (hdc , GDCAP_MAXX)+1; 
    height = rc.bottom = GetGDCapability (hdc , GDCAP_MAXY)+1; 


  img->xsize       = rc.right;
  img->ysize       = rc.bottom;
  depth            = GetGDCapability (hdc , GDCAP_DEPTH);
  compression      = 0;
  img->xppi        = (unsigned int)(GetGDCapability (hdc ,GDCAP_HPIXEL) * 0.0254 + 0.5);
  img->yppi        = (unsigned int)(GetGDCapability (hdc ,GDCAP_VPIXEL) * 0.0254 + 0.5);

  if (img->xsize == 0 || img->xsize > IMAGE_MAX_WIDTH ||
      img->ysize == 0 || img->ysize > IMAGE_MAX_HEIGHT ||
      (depth != 1 && depth != 4 && depth != 8 && depth != 24))
  {
    fprintf(stderr, "ERROR: Bad BMP dimensions %ux%ux%d\n",
            img->xsize, img->ysize, depth);
    return (1);
  }


  if (img->xppi == 0 || img->yppi == 0)
  {
    img->xppi = img->yppi = 128;
  }

 /*
  * Get colormap...
  */
   if (depth <= 8)
        GetPalette (hdc , 0 , 256 , (GAL_Color*) colormap);
        

 /*
  * Read the image data...
  */

    if (info->flags & MGPI_MM)
    {
        tmp = mgpMM2Pixel(info , info->margin_top);
        top = (tmp > (height/2)) ? (height/2) : tmp;

        tmp = mgpMM2Pixel(info ,info->margin_left);
        left = (tmp > (width/2)) ? (width/2) : tmp;

        tmp = mgpMM2Pixel(info , info->margin_bottom);
        bottom = (tmp > (height/2)) ? (height/2) : tmp;

        tmp = mgpMM2Pixel(info , info->margin_right);
        right = (tmp > (width/2)) ? (width/2) : tmp;

    }
    else
    {
        tmp = mgpInch2Pixel(info, info->margin_top);
        top = (tmp > (height/2)) ? (height/2) : tmp;

        tmp = mgpInch2Pixel(info , info->margin_left);
        left = (tmp > (width/2)) ? (width/2) : tmp;

        tmp = mgpInch2Pixel(info , info->margin_bottom);
        bottom = (tmp > (height/2)) ? (height/2) : tmp;

        tmp = mgpInch2Pixel(info , info->margin_right);
        right = (tmp > (width/2)) ? (width/2) : tmp;

    }

    if (info->flags & MGPI_DEPTH24)
        mem = CreateMemDC (rc.right , rc.bottom , 24, MEMDC_FLAG_SWSURFACE,
                                             0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);
    else
        mem = CreateMemDC (rc.right , rc.bottom , 8, MEMDC_FLAG_SWSURFACE,
                                             0x0, 0x0, 0x0, 0x0);
    SetColorfulPalette (mem);
    SetBkColor (mem , RGB2Pixel (mem, 0xFF, 0xFF, 0xFF));
    SetBrushColor (mem , RGB2Pixel (mem, 0xFF, 0xFF, 0xFF));
    FillBox (mem, rc.left, rc.top, rc.right, rc.bottom);
    BitBlt (hdc , 0  , 0 , width - (right+left) , height - (bottom+top) , 
                                    mem , rc.left + left , rc.top + top , 0 );
    

 /*
  * Setup image and buffers...
  */

  img->colorspace = (primary == IMAGE_RGB_CMYK) ? IMAGE_RGB : primary;

  ImageSetMaxTiles(img, 0);

  in  = (ib_t*)malloc(img->xsize *3 +3);
  bpp = ImageGetDepth(img);
  out = (ib_t*)malloc(img->xsize * bpp + 1);


/* start lock dc data*/

 if ((src = LockDC (mem , &rc , &width , &height , &pitch)) == NULL) {
    UnlockDC (mem);
    return 1;
 } 

  for (y = 0; y <= (int)(img->ysize - 1); y ++)
  {
    if (img->colorspace == IMAGE_RGB)
      ptr = out;
    else
      ptr = in;

    pr = 0; /*24 bit used only*/

    switch (depth)
    {

      case 8 : /* 256-color */
          for (x = 0; x < (int)img->xsize; x ++)
	      {
           /*
	    * Copy the color value...
	    */
            *ptr++ = colormap[src[y*pitch+x]].r;
            *ptr++ = colormap[src[y*pitch+x]].g;
            *ptr++ = colormap[src[y*pitch+x]].b;
	      }

          break;

      case 24 : /* 24-bit RGB */
          for (x = 0; x < (int)img->xsize; x ++, pr+=3)
          {
            ptr[0] = src[y*pitch+pr];
            ptr[1] = src[y*pitch+pr+1];
            ptr[2] = src[y*pitch+pr+2];

            ptr += 3;
          }
          break;
    } /*end switch*/


    if (img->colorspace == IMAGE_RGB)
    {
      if (saturation != 100 || hue != 0)
	ImageRGBAdjust(out, img->xsize, saturation, hue);
    }
    else
    {
      if (saturation != 100 || hue != 0)
	ImageRGBAdjust(in, img->xsize, saturation, hue);

      switch (img->colorspace)
      {
	case IMAGE_WHITE :
	    ImageRGBToWhite(in, out, img->xsize);
	    break;
	case IMAGE_BLACK :
	    ImageRGBToBlack(in, out, img->xsize);
	    break;
	case IMAGE_CMY :
	    ImageRGBToCMY(in, out, img->xsize);
	    break;
	case IMAGE_CMYK :
	    ImageRGBToCMYK(in, out, img->xsize);
	    break;
      }
    }

    if (lut)
      ImageLut(out, img->xsize * bpp, lut);

    ImagePutRow(img, 0, y, img->xsize, out);
  }

  UnlockDC (mem);

  DeleteMemDC (mem);

/* clear the HDC*/
    SetColorfulPalette (hdc);
    SetBkColor (hdc , RGB2Pixel (hdc, 0xFF, 0xFF, 0xFF));
    SetBrushColor (hdc , RGB2Pixel (hdc, 0xFF, 0xFF, 0xFF));
    FillBox (hdc, rc.left, rc.top, rc.right, rc.bottom);


/* end lock dc*/
  free(in);
  free(out);

  return (0);
}

