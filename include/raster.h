/**
 * \file raster.h
 * \author Jipeng Zhang <jpzhang@minigui.org>
 * \date 2005/12/22
 *
 \verbatim
    Copyright (C) 2006 Feynman Software.
 \endverbatim
 */

/*
 * $Id: raster.h,v 1.4 2006-01-14 07:51:18 xwyan Exp $
 *
 * Copyright (C) 2006 Feynman Software, all rights reserved.
 *                  
 * URL: http://www.minigui.com
 *
 * Author: Jipeng Zhang  
 */

#ifndef __RASTER_H__
#define __RASTER_H__

#include <minigui/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Every non-PostScript printer driver that supports raster images should
 * use the application/vnd.raster image file format.  Since both the
 * PostScript RIP (pstoraster, based on GNU Ghostscript) and Image RIP
 * (imagetoraster, located in the filter directory) use it, using this format
 * saves you a lot of work.  Also, the PostScript RIP passes any printer
 * options that are in a PS file to your driver this way as well...
 */

/*
 * Constants...
 */

#define RASTER_SYNC	    0x52615374	/* RaSt */
#define RASTER_REVSYNC	0x74536152	/* tSaR */

typedef struct				/**** Printer Options ****/
{
  char		*name;			/* Name of option */
  char		*value;			/* Value of option */
} PrinterOption;

typedef enum
{
    CSPACE_W,			/* Luminance */
    CSPACE_RGB,			/* Red, green, blue */
    CSPACE_RGBA,			/* Red, green, blue, alpha */
    CSPACE_K,			/* Black */
    CSPACE_CMY,			/* Cyan, magenta, yellow */
    CSPACE_YMC,			/* Yellow, magenta, cyan */
    CSPACE_CMYK,			/* Cyan, magenta, yellow, black */
    CSPACE_YMCK,			/* Yellow, magenta, cyan, black */
    CSPACE_KCMY,			/* Black, cyan, magenta, yellow */
    CSPACE_KCMYcm,			/* Black, cyan, magenta, yellow, *
             * light-cyan, light-magenta     */
    CSPACE_GMCK,			/* Gold, magenta, yellow, black */
    CSPACE_GMCS,			/* Gold, magenta, yellow, silver */
    CSPACE_WHITE,			/* White ink (as black) */
    CSPACE_GOLD,			/* Gold foil */
    CSPACE_SILVER,			/* Silver foil */

    CSPACE_CIEXYZ,			/* CIE XYZ */
    CSPACE_CIELab,			/* CIE Lab */

    CSPACE_ICC1 = 32,		/* ICC-based, 1 color */
    CSPACE_ICC2,			/* ICC-based, 2 colors */
    CSPACE_ICC3,			/* ICC-based, 3 colors */
    CSPACE_ICC4,			/* ICC-based, 4 colors */
    CSPACE_ICC5,			/* ICC-based, 5 colors */
    CSPACE_ICC6,			/* ICC-based, 6 colors */
    CSPACE_ICC7,			/* ICC-based, 7 colors */
    CSPACE_ICC8,			/* ICC-based, 8 colors */
    CSPACE_ICC9,			/* ICC-based, 9 colors */
    CSPACE_ICCA,			/* ICC-based, 10 colors */
    CSPACE_ICCB,			/* ICC-based, 11 colors */
    CSPACE_ICCC,			/* ICC-based, 12 colors */
    CSPACE_ICCD,			/* ICC-based, 13 colors */
    CSPACE_ICCE,			/* ICC-based, 14 colors */
    CSPACE_ICCF			/* ICC-based, 15 colors */
} PRINTER_CSPACE;

typedef enum
{
    JOG_NONE,			/* Never move pages */
    JOG_FILE,			/* Move pages after this file */
    JOG_JOB,				/* Move pages after this job */
    JOG_SET				/* Move pages after this set */
} PRINTER_JOG;

typedef enum
{
    CUT_NONE,			/* Never cut the roll */
    CUT_FILE,			/* Cut the roll after this file */
    CUT_JOB,				/* Cut the roll after this job */
    CUT_SET,				/* Cut the roll after this set */
    CUT_PAGE				/* Cut the roll after this page */
} PRINTER_CUT;

typedef enum
{
    ADVANCE_NONE,			/* Never advance the roll */
    ADVANCE_FILE,			/* Advance the roll after this file */
    ADVANCE_JOB,			/* Advance the roll after this job */
    ADVANCE_SET,			/* Advance the roll after this set */
    ADVANCE_PAGE			/* Advance the roll after this page */
} PRINTER_ADVANCE;

typedef enum
{
    ORIENT_0,			/* Don't rotate the page */
    ORIENT_90,			/* Rotate the page counter-clockwise */
    ORIENT_180,			/* Turn the page upside down */
    ORIENT_270			/* Rotate the page clockwise */
} PRINTER_ORIENT;

typedef enum
{
    ORDER_CHUNKED,			/* CMYK CMYK CMYK ... */
    ORDER_BANDED,			/* CCC MMM YYY KKK ... */
    ORDER_PLANAR			/* CCC ... MMM ... YYY ... KKK ... */
} PRINTER_ORDER;

typedef enum
{
    EDGE_TOP,			/* Leading edge is the top of the page */
    EDGE_RIGHT,			/* Leading edge is the right of the page */
    EDGE_BOTTOM,			/* Leading edge is the bottom of the page */
    EDGE_LEFT			/* Leading edge is the left of the page */
} PRINTER_EDGE;

/*
 * The page header structure contains the standard PostScript page device
 * dictionary, along with some specific parameters that are provided
 * by the RIPs...
 */

typedef struct
{
    /**** Standard Page Device Dictionary String Values ****/
    char		MediaClass[64];		/* MediaClass string */
    char		MediaColor[64];		/* MediaColor string */
    char		MediaType[64];		/* MediaType string */
    char		OutputType[64];		/* OutputType string */

    /**** Standard Page Device Dictionary Integer Values ****/
    unsigned	AdvanceDistance;	/* AdvanceDistance value in points */
    PRINTER_ADVANCE	AdvanceMedia;		/* AdvanceMedia value (see above) */
    BOOL	Collate;		/* Collated copies value */
    PRINTER_CUT	CutMedia;		/* CutMedia value (see above) */
    BOOL	Duplex;			/* Duplexed (double-sided) value */
    unsigned	HWResolution[2];	/* Resolution in dots-per-inch */
    unsigned	ImagingBoundingBox[4];	/* Pixel region that is painted (points) */
    BOOL	InsertSheet;		/* InsertSheet value */
    PRINTER_JOG	Jog;			/* Jog value (see above) */
    PRINTER_EDGE	LeadingEdge;		/* LeadingEdge value (see above) */
    unsigned	Margins[2];		/* Lower-lefthand margins in points */
    BOOL	ManualFeed;		/* ManualFeed value */
    unsigned	MediaPosition;		/* MediaPosition value */
    unsigned	MediaWeight;		/* MediaWeight value in grams/m^2 */
    BOOL	MirrorPrint;		/* MirrorPrint value */
    BOOL	NegativePrint;		/* NegativePrint value */
    unsigned	NumCopies;		/* Number of copies to produce */
    PRINTER_ORIENT	Orientation;		/* Orientation value (see above) */
    BOOL	OutputFaceUp;		/* OutputFaceUp value */
    unsigned	PageSize[2];		/* Width and length of page in points */
    BOOL	Separations;		/* Separations value */
    BOOL	TraySwitch;		/* TraySwitch value */
    BOOL	Tumble;			/* Tumble value */

    /**** Page Device Dictionary Values ****/
    unsigned	Width;		/* Width of page image in pixels */
    unsigned	Height;		/* Height of page image in pixels */
    unsigned	PageMediaType;		/* Media type code */
    unsigned	BitsPerColor;	/* Number of bits for each color */
    unsigned	BitsPerPixel;	/* Number of bits for each pixel */
    unsigned	BytesPerLine;	/* Number of bytes per line */
    PRINTER_ORDER	ColorOrder;		/* Order of colors */
    PRINTER_CSPACE	ColorSpace;		/* True colorspace */
    unsigned	Compression;	/* Device compression to use */
    unsigned	RowCount;		/* Rows per band */
    unsigned	RowFeed;		/* Feed between bands */
    unsigned	RowStep;		/* Spacing between lines */
} PrinterPageHeader;

/*
 * Types...
 */

typedef enum
{
  RASTER_READ,			/* Open stream for reading */
  RASTER_WRITE			/* Open stream for writing */
} PrinterMode;

/*
 * The raster structure maintains information about a raster data
 * stream...
 */

typedef struct
{
  unsigned	    sync;			/* Sync word from start of stream */
  int		    fd;			/* File descriptor */
  PrinterMode   mode;			/* Read/write mode */
} PrinterRaster;

/*
 * Prototypes...
 */

extern void		RasterClose(PrinterRaster *r);
extern PrinterRaster	*RasterOpen(int fd, PrinterMode mode);
extern unsigned		RasterReadHeader(PrinterRaster *r, PrinterPageHeader *h);
extern unsigned		RasterReadPixels(PrinterRaster *r, unsigned char *p, unsigned len);
extern unsigned		RasterWriteHeader(PrinterRaster *r, PrinterPageHeader *h);
extern unsigned		RasterWritePixels(PrinterRaster *r, unsigned char *p, unsigned len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RASTER_H__ */
