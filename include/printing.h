/**
 * \file printing.h
 * \author Jipeng Zhang <jpzhang@minigui.org>
 * \date 2005/12/22
 *
 * This file include printing support system interface of MiniGUI. 
 *
 \verbatim
    Copyright (C) 2006 Feynman Software.
 \endverbatim
 */

/*
 * $Id: printing.h,v 1.15 2006-01-14 07:51:18 xwyan Exp $
 * printing.h: printing support system interface of MiniGUI. 
 *
 * Copyright (C) 2006 Feynman Software, all rights reserved.
 *                  
 * URL: http://www.minigui.com
 *
 * Author : Jipeng Zhang  
 */

#ifndef _PRINTING_H_
#define _PRINTING_H_


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /**
     * \addtogroup defs Defines
     * @{
     */
/** 8 bit color DC*/
#define MGPI_DEPTH8     0x00000001
/** 16 bit color DC*/
#define MGPI_DEPTH24    0x00000002

/** unit base on millimeter*/
#define MGPI_MM         0x00000010
/** unit base on inch*/
#define MGPI_INCH       0x00000020

/** callback return value, print next page*/
#define CBR_NEXT        0

/** callback return value, if nr_totalpage is 0, you can finished print
process with it*/
#define CBR_FINISH      -1

/** callback return value,  you can return this value on error*/
#define CBR_ABORT       -2

    /** @} end of defs */

    /**
     * \addtogroup enum Enums
     * @{
     */
/** 
*  \enum _tagpage_type
*   This enum type specifies what paper size should use.
*   
*    A0       841 x 1189 mm\n
*    A1       594 x 841 mm\n
*    A2       420 x 594 mm\n
*    A3       297 x 420 mm\n
*    A4       210 x 297 mm, 8.26 x 11.7 inches\n
*    A5       148 x 210 mm\n
*    A6       105 x 148 mm\n
*    A7       74 x 105 mm\n
*    A8       52 x 74 mm\n
*    A9       37 x 52 mm\n
*    B0       1030 x 1456 mm\n
*    B1       728 x 1030 mm\n
*    B10      32 x 45 mm\n
*    B2       515 x 728 mm\n
*    B3       364 x 515 mm\n
*    B4       257 x 364 mm\n
*    B5       182 x 257 mm, 7.17 x 10.13 inches\n
*    B6       128 x 182 mm\n
*    B7       91 x 128 mm\n
*    B8       64 x 91 mm\n
*    B9       45 x 64 mm\n
*    C5E      163 x 229 mm\n
*    Comm10E  105 x 241 mm, US Common #10 Envelope\n
*    DLE      110 x 220 mm\n
*    Executive  7.5 x 10 inches, 191 x 254 mm\n
*    Folio    210 x 330 mm\n
*    Ledger   432 x 279 mm\n
*    Legal    8.5 x 14 inches, 216 x 356 mm\n
*    Letter   8.5 x 11 inches, 216 x 279 mm\n
*    Tabloid  279 x 432 mm\n
*    Custom\n
*    NPageSize (internal)\n
*/
enum _tagpage_type {
    A4,
    B5,
    Letter,
    Legal,
    Executive,
    A0, A1, A2, A3, A5, A6, A7, A8, A9,
    B0, B1, B10, B2, B3, B4, B6, B7, B8, B9,
    C5E,
    Comm10E,
    DLE,
    Folio,
    Ledger,
    Tabloid,
    Custom,
    NPageSize = Custom
};

    /** @} end of enum */

    /**
     * \addtogroup typedef Typedefs
     * @{
     */

/**
 * \var typedef enum _tagpage_type  PAGE_TYPE
 * \brief Type of page size
 * 
 * \sa
 */
typedef enum _tagpage_type  PAGE_TYPE;

/**
 * \var typedef struct _tagPrint_info   MGPRINT_INFO
 * \brief Type of printing control struct
 *
 * \sa InitPrintInfo, InitPrintInfoByDialog, StartPrintDocs, ReleasePrintInfo
 */
typedef struct _tagPrint_info   MGPRINT_INFO; 

/**
 * \var typedef int (* cb_printpage) (const MGPRINT_INFO* info, void * context, HDC hdc, int pageno)
 * \brief Type of printing page callback
 * 
 * \sa StartPrintDocs
 */
typedef int (* cb_printpage) (const MGPRINT_INFO* info, void * context, HDC hdc, int pageno);

    /** @} end of typedef */


    /**
     * \addtogroup fns Functions
     * @{
     */

    /**
     * \defgroup print_fns Print support functions
     * @{
     */

/** Print control information. */ 
struct _tagPrint_info
{
    /** copies */
    unsigned int nr_copies;            
    /** document total pages */
    unsigned int nr_totalpages;        
    /** print range (example: "1,2,3-4,5,7-8"), must alloc by malloc, terminate in zero*/
    char * page_range;                 
    /** control flags (unit: mm, inchs; bpps: 8, 16) */
    DWORD  flags;                      
    /** left margin */
    float margin_left;                 
    /** top margin */
    float margin_top;                  
    /** right margin */
    float margin_right;                
    /** bottom margin */
    float margin_bottom;               
    /** page type (A4 , B5) */
    PAGE_TYPE page_type;                
    /** custom page width */
    float custom_width;                
    /** custom page height */
    float custom_height;               
    /** printer name, must alloc by malloc */
    char * printer_name;               
    /** the ppd file name, must alloc by malloc */
    char * ppd_name;                   
};


/**
 * \fn BOOL GUIAPI mgpInitPrintInfo (MGPRINT_INFO* info)
 * \brief Initialize the printing control struct by default values
 *
 * You can initialize the control struct through this function. The pointer fields is 
 * alloced by malloc function.
 *
 * \param info Address of MGPRINT_INFO struct.
 *
 * \return Return TRUE on success, FALSE on error.
 *
 * \sa mgpReleasePrintInfo
 */
BOOL GUIAPI mgpInitPrintInfo (MGPRINT_INFO* info);

/**
 * \fn BOOL GUIAPI mgpInitPrintInfoByDialog (MGPRINT_INFO* info, unsigned int totalpages)
 * \brief Initialize the printing control struct by dialog
 *
 * You can initialize the control struct from a dialog through this function. The pointer fields is 
 * alloced by malloc function.
 *
 * \param info        Address of MGPRINT_INFO struct.
 * \param totalpages  Total pages.
 *
 * \return Return the button pressed  0 is ok, -1 is cancel, -2 is error.
 *
 * \sa mgpReleasePrintInfo
 */
int GUIAPI mgpInitPrintInfoByDialog (MGPRINT_INFO* info, unsigned int totalpages);


/**
 * \fn int GUIAPI mgpStartPrintDocs (const  MGPRINT_INFO* info, cb_printpage cb, DWORD flags, void * context)
 * \brief Start the printing prosess. 
 *
 * This function start a printing process and call your callback function to print every page.
 *
 *
 * \param info Address of MGPRINT_INFO struct. 
 * \param cb  Your callback function.
 * \param flags Control flags. 
 * \param context User define data context.
 *
 * \return SPD_FINISH  on success , < 0 on error.
 *
 * \retval SPD_FINISH        Printing successfully.
 * \retval SPD_USERABORT     User abort.
 * \retval SPD_ERROR         On error.
 *
 * \sa cb_printpage 
 */
#define  SPD_FINISH     0
#define  SPD_USERABORT  -2
#define  SPD_ERROR      -3    
int GUIAPI mgpStartPrintDocs (const  MGPRINT_INFO* info, cb_printpage cb, DWORD flags, void * context);


/**
 * \fn void GUIAPI mgpReleasePrintInfo (MGPRINT_INFO* info)
 * \brief Release the printing control struct.
 *
 * You always call this function after you call InitPrintInfo function or 
 * InitPrintInfoByDialog function.
 *
 * \param info Address of MGPRINT_INFO struct.
 *
 * \sa mgpInitPrintInfo, mgpInitPrintInfoByDialog 
 */
void GUIAPI mgpReleasePrintInfo (MGPRINT_INFO* info);

    
    /** @} end of print_fns */

    /** @} end of fns */

    /**
     * \addtogroup fns Functions
     * @{
     */

    /**
     * \defgroup convert_fns Unit convert functions
     * @{
     */


/**
 * \fn int GUIAPI mgpMM2Pixel (const MGPRINT_INFO* info, float mm)
 * \brief conversion routine, millimeter to pixel.   
 *
 * Call this function to convert millimeter to pixel.
 * 
 *
 * \param info Address of MGPRINT_INFO struct.
 * \param mm   Value of input base on millimeter.
 *
 * \sa mgpInch2Pixel 
 */
int GUIAPI mgpMM2Pixel (const MGPRINT_INFO* info, float mm); 

/**
 * \fn int GUIAPI mgpInch2Pixel (const  MGPRINT_INFO* info, float inch)
 * \brief conversion routine, inch to pixel.   
 *
 * Call this function to convert inch to pixel.
 * 
 *
 * \param info Address of MGPRINT_INFO struct.
 * \param inch Value of input base on inch.
 *
 * \sa mgpMM2Pixel 
 */
int GUIAPI mgpInch2Pixel (const  MGPRINT_INFO* info, float inch);


    /** @} end of convert_fns */

    /** @} end of fns */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PRINTING_H_*/


