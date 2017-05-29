/*
** $Id: printing.c,v 1.26 2007-11-23 07:24:52 jpzhang Exp $
**
** Author:  Zhang Jipeng <jpzhang@minigui.org>
**
** Copyright (C) 2006 Feynman Software.
** Copyright (C) 1999 ~ 2002 Wei Yongming.
**
** Create date: 2005/11/10
*/

/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "mgpconfig.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "printing.h"
#include "nparser.h"

#define MM(n) (int)((n * 720 + 127) / 254)
#define IN(n) (int)(n * 72)

typedef struct PAGE_SIZE {
    int width, height;
} PAGE_SIZE;

static const PAGE_SIZE PageSizes [NPageSize] = 
{
    {  MM(210), MM(297) },      // A4
    {  MM(176), MM(250) },      // B5
    {  IN(8.5), IN(11) },       // Letter
    {  IN(8.5), IN(14) },       // Legal
    {  IN(7.5), IN(10) },       // Executive
    {  MM(841), MM(1189) },     // A0
    {  MM(594), MM(841) },      // A1
    {  MM(420), MM(594) },      // A2
    {  MM(297), MM(420) },      // A3
    {  MM(148), MM(210) },      // A5
    {  MM(105), MM(148) },      // A6
    {  MM(74), MM(105)},        // A7
    {  MM(52), MM(74) },        // A8
    {  MM(37), MM(52) },        // A9
    {  MM(1000), MM(1414) },    // B0
    {  MM(707), MM(1000) },     // B1
    {  MM(31), MM(44) },        // B10
    {  MM(500), MM(707) },      // B2
    {  MM(353), MM(500) },      // B3
    {  MM(250), MM(353) },      // B4
    {  MM(125), MM(176) },      // B6
    {  MM(88), MM(125) },       // B7
    {  MM(62), MM(88) },        // B8
    {  MM(44), MM(62) },        // B9
    {  MM(162),    MM(229) },   // C5E
    {  IN(4.125),  IN(9.5) },   // Comm10E
    {  MM(110),    MM(220) },   // DLE
    {  IN(8.5),    IN(13) },    // Folio
    {  IN(17),     IN(11) },    // Ledger
    {  IN(11),     IN(17) }     // Tabloid
};


extern int __StartPrintProc (HDC hdc, const MGPRINT_INFO * info);
extern int CreatePrinterSettingDialog(HWND hwnd, void * info , unsigned int totalpages);

static HDC create_printerDC (const MGPRINT_INFO* info);
static void  delete_printerDC (HDC hdc);



BOOL GUIAPI mgpInitPrintInfo (MGPRINT_INFO* info)
{
    if (NULL == info)
        return FALSE;

    info->nr_copies = 1;
    info->nr_totalpages = 0;
    info->page_type = A4;      /* page size A4*/
    info->custom_width = 0.0f;
    info->custom_height = 0.0f;
    info->flags = MGPI_DEPTH8 | MGPI_MM; /* 8 bit color and unit of mm*/
    info->margin_left = 0.0f;
    info->margin_top = 0.0f;
    info->margin_right = 0.0f;
    info->margin_bottom = 0.0f;


    info->page_range = (char*)calloc(1, 2);
    if (NULL == info->page_range)
        return FALSE;

    strcpy(info->page_range, "c"); /* current page*/

    info->printer_name = (char*)calloc(1, NAME_MAX + 1);
    if (NULL == info->printer_name)
    {
        free (info->page_range);
        fprintf (stderr , "Not have enough memory!\n");
        return FALSE;
    } 
    else
    {
        strcpy(info->printer_name , "Default");
    }

    info->ppd_name = (char*)calloc(1, NAME_MAX + 1);
    if (NULL == info->ppd_name)
    {
        free (info->page_range);
        free (info->printer_name);
        fprintf (stderr , "Not have enough memory!\n");
        return FALSE;
    }
    else
    {
        if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", info->printer_name , "ppd", info->ppd_name, NAME_MAX) < 0)
        {   
            free (info->page_range);
            free (info->printer_name);
            free (info->ppd_name);
            fprintf (stderr , "Init ppd name error!\n");
            return FALSE;
        }
    }
    return TRUE;
}

int GUIAPI mgpInitPrintInfoByDialog (MGPRINT_INFO* info, unsigned int totalpages)
{
    int ret;
    if (NULL == info)
        return -2;

    ret = CreatePrinterSettingDialog(HWND_DESKTOP, info , totalpages);
        
    return ret;
}

#define  VALID_DC(x)  if ((x) == 0) goto error

int GUIAPI mgpStartPrintDocs (const  MGPRINT_INFO* info, cb_printpage cb, DWORD flags, void * context)
{
    unsigned int i, j, total, pagenum;
    HDC hdc;
    int ret = SPD_ERROR;
    struct list_table * prange = NULL;
    struct list_table * prange_cur = NULL;

    if (NULL == info || NULL == cb)
        return -3;
    
    total = info->nr_totalpages;


    for (i = 0; i < info->nr_copies; i++)
    {
    
        pagenum = _create_range_table (info->page_range, &prange);

        switch (pagenum)
        {
        case -3:  /* Error!*/
            goto error;
        case -1: /* Print all pages*/
                VALID_DC(hdc = create_printerDC(info)); 

                if (total == 0) { /*user control*/
                    do
                    {
                        if ((ret = (*cb)(info, context, hdc, ++total)) != CBR_ABORT) {
                            if (__StartPrintProc (hdc, info) == 0){
                                if (ret == CBR_NEXT){ 
                                    continue;
                                } else {
                                    ret = SPD_FINISH;
                                    break;
                                }

                            } else {
                                ret = SPD_ERROR;
                                break; 
                            }
                        } else {
                            ret = SPD_USERABORT;
                            break;
                        }

                    } while(TRUE);                   

                } else { /*total pages control*/
                    for (j = 1; j <= total; j++ )
                    {
                        if ((ret = (*cb)(info, context, hdc, j)) != CBR_ABORT) {
                            if (__StartPrintProc (hdc, info) == 0){
                                if (ret == CBR_FINISH){ 
                                    ret = SPD_FINISH;
                                    break;
                                }

                            } else {
                                ret = SPD_ERROR;
                                break; 
                            }
                        } else {
                            ret = SPD_USERABORT;
                            break;
                        }
                    }
                }

                delete_printerDC(hdc);
            break;
        case 0: /* Print current pages*/
                VALID_DC(hdc = create_printerDC(info)); 

                if ((ret = (*cb)(info, context, hdc, 0)) != CBR_ABORT) {
                    if (__StartPrintProc (hdc, info) == 0) 
                        ret = SPD_FINISH;
                    else
                        ret = SPD_ERROR; 
                } else {
                    ret = SPD_USERABORT;
                }
                
                delete_printerDC(hdc);
            break;
        default: /* page numbers*/
                VALID_DC(hdc = create_printerDC(info)); 
                prange_cur = prange;

                for (; prange_cur; prange_cur = prange_cur->next)
                {
                    if (prange_cur->type == RANGE_SINGLE) {
                            if ((ret = (*cb)(info, context, hdc, prange_cur->val)) != CBR_ABORT) {
                                if (__StartPrintProc (hdc, info) == 0){
                                    if (ret == CBR_FINISH){ 
                                        ret = SPD_FINISH;
                                        break;
                                    }

                                } else {
                                    ret = SPD_ERROR;
                                    break; 
                                }
                            } else {
                                ret = SPD_USERABORT;
                                break;
                            }
                    
                    } else if (prange_cur->type == RANGE_START) {
                        for (j = prange_cur->val; j <= (prange_cur->next)->val; j++) 
                            if ((ret = (*cb)(info, context, hdc, j)) != CBR_ABORT) {
                                if (__StartPrintProc (hdc, info) == 0){
                                    if (ret == CBR_FINISH){ 
                                        ret = SPD_FINISH;
                                        goto interrupt;
                                    }

                                } else {
                                    ret = SPD_ERROR;
                                    goto interrupt;
                                }
                            } else {
                                ret = SPD_USERABORT;
                                goto interrupt;
                            }
                    }

                }
interrupt:
                delete_printerDC(hdc);
            break;
        }     
    _destroy_range_table(&prange);
    }

    return ret;

error:
    _destroy_range_table(&prange);
    return SPD_ERROR;
}

void GUIAPI mgpReleasePrintInfo (MGPRINT_INFO* info)
{
    if (NULL == info)
        return;

    free (info->ppd_name);
    free (info->printer_name);
    free (info->page_range);
}



static HDC create_printerDC (const MGPRINT_INFO* info)
{
    HDC hdc;
    int width , height;


    if (info->page_type == Custom)
    {
        if (info->flags & MGPI_MM)
        {
            width = MM(info->custom_width);
            height = MM(info->custom_height);
        }
        else
        {
            width = IN(info->custom_width);
            height = IN(info->custom_height);
        }
    }
    else
    {
        width = PageSizes[info->page_type].width;
        height = PageSizes[info->page_type].height;
    }




/* setting the page size */
    if (info->flags & MGPI_DEPTH24)
        hdc = CreateMemDC (width, height, 24, MEMDC_FLAG_SWSURFACE,
                                             0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);
    else
        hdc = CreateMemDC (width, height, 8, MEMDC_FLAG_SWSURFACE,
                                             0x0, 0x0, 0x0, 0x0);

    SetColorfulPalette (hdc);
    SetBkColor (hdc , RGB2Pixel (hdc, 0xFF, 0xFF, 0xFF));
    SetBrushColor (hdc , RGB2Pixel (hdc, 0xFF, 0xFF, 0xFF));
    FillBox (hdc, 0, 0, width, height);

    return hdc;
}

static void  delete_printerDC (HDC hdc)
{
    DeleteMemDC (hdc);
}

/* convert functions*/

int GUIAPI mgpMM2Pixel (const MGPRINT_INFO* info, float mm)
{
    return MM(mm);
}

int GUIAPI mgpInch2Pixel (const MGPRINT_INFO* info, float inch)
{
   return IN(inch);
}



