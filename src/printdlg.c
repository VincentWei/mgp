
/* 
** $Id: printdlg.c,v 1.18 2007-11-21 07:54:35 jpzhang Exp $
**
** printdlg.c: print dialog for printer
**
** Copyright (C) 2004 Feynman Software.
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mgpconfig.h"
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "printing.h"

#define IDC_STATIC1     100
#define IDC_OK      200
#define IDC_CANCEL  300
#define IDC_PRINTER 301
#define IDC_RANGE   302
#define IDC_WIDTH   303
#define IDC_HEIGHT  304
#define IDC_TOP     305
#define IDC_BOTTOM  306 
#define IDC_LEFT    307
#define IDC_RIGHT   308
#define IDC_COPY    309
#define IDC_PRINTALL 310
#define IDC_PRINTCURRENT 311 
#define IDC_PRINTSELECT  312
#define IDC_PAGETOTAL    313
#define IDC_SELECTPAGE   314
#define IDC_UNIT         315

#define NAMEWIDTH 40
#define HEIGHT 20
#define CTRLWIDTH 80

#define XLEN0 15 
#define XLEN1 (XLEN0 + 10)
#define XLEN2 (XLEN1 + NAMEWIDTH)
#define XLEN3 (XLEN2 + CTRLWIDTH)
#define XLEN4 (XLEN3 + 20)
#define XLEN5 (XLEN4 + NAMEWIDTH)
#define XLEN6 (XLEN5 + CTRLWIDTH+10)
#define XLEN7 (XLEN6 + 11)
#define XLEN8 (XLEN7 + 10)
#define XLEN9 (XLEN8 + NAMEWIDTH)
#define XLEN10 (XLEN9 + CTRLWIDTH)
#define XLEN11 (XLEN10 + 20)
#define XLEN12 (XLEN11 + NAMEWIDTH)
#define XLEN13 (XLEN12 + CTRLWIDTH)
#define XLEN14 (XLEN13 + 10)
#define XLEN15 (XLEN14 + 20)


#define YLEN0 10
#define YLEN1 (YLEN0 + 25)
#define YLEN2 (YLEN1 + 30)

#define YLEN3 (YLEN2 + 5)  
#define YLEN4 (YLEN3 + 25) 
#define YLEN5 (YLEN4 + 25) 

#define YLEN6 (YLEN5 + 30) 
#define YLEN7 (YLEN6 + 5) 
#define YLEN8 (YLEN7 + 25) 
#define YLEN9 (YLEN8 + 25) 

#define YLEN10 (YLEN9 + 25)
#define YLEN11 (YLEN10 + 25)
#define YLEN12 (YLEN11 + 25)
#define YLEN13 (YLEN12 + 5)
#define YLEN14 (YLEN13 + 30)
#define YLEN15 (YLEN14 + 30)


#define ES_INTEGER      0x0001
#define ES_FLOAT        0x0002

static DLGTEMPLATE DlgPrinterSetting = {
    WS_BORDER | WS_CAPTION,
    WS_EX_NOCLOSEBOX,
    0, 0 , XLEN15, YLEN15,
#ifdef _LANG_ZHCN
    "打印机设置",
#else
    "Printer setting",
#endif
    0, 0,
    33, NULL,
    0
};

static CTRLDATA CtrlPrinterSetting[] = {
/*************************************/
    {
     CTRL_BUTTON,
     WS_TABSTOP | WS_VISIBLE | BS_DEFPUSHBUTTON ,
     XLEN14-130, YLEN13, 60, 30,
     IDC_OK,
#ifdef _LANG_ZHCN
     "打印",
#else
     "Print",
#endif
     0},
    {
     CTRL_BUTTON,
     WS_TABSTOP | WS_VISIBLE | BS_DEFPUSHBUTTON ,
     XLEN14-60, YLEN13, 60, 30,
     IDC_CANCEL,
#ifdef _LANG_ZHCN
     "取消",
#else
     "Cancel",
#endif
     0},
    {
/*************************************/
     CTRL_STATIC,
     WS_VISIBLE | SS_GROUPBOX,
     XLEN0, YLEN0, XLEN14 - XLEN0, YLEN2 -YLEN0,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "打印机",
#else
     "Printer",
#endif
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN1, YLEN1, NAMEWIDTH+20, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "名称",
#else
     "Name",
#endif
     0},
    {
     CTRL_COMBOBOX,
     WS_VISIBLE | CBS_DROPDOWNLIST | CBS_NOTIFY | CBS_READONLY| WS_TABSTOP,
     XLEN2, YLEN1, XLEN6 - XLEN2, HEIGHT,
     IDC_PRINTER,
     NULL,
     0},
/*************************************/
    {
     CTRL_STATIC,
     WS_VISIBLE | SS_GROUPBOX,
     XLEN0, YLEN3, XLEN6 - XLEN0, YLEN6 - YLEN3,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "打印纸设置",
#else
     "Paper Setting",
#endif
     0},

    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN1, YLEN4, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "大小",
#else
     "Size",
#endif
     0},
    {
     CTRL_COMBOBOX,
     WS_VISIBLE | CBS_DROPDOWNLIST | CBS_NOTIFY | CBS_READONLY| WS_TABSTOP,
     XLEN2, YLEN4, CTRLWIDTH, HEIGHT,
     IDC_RANGE,
     NULL,
     120},

    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN1, YLEN5, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "单位",
#else
     "Unit",
#endif
     0},
    {
     CTRL_COMBOBOX,
     WS_VISIBLE | CBS_DROPDOWNLIST | CBS_NOTIFY | CBS_READONLY| WS_TABSTOP,
     XLEN2, YLEN5, CTRLWIDTH, HEIGHT,
     IDC_UNIT,
     NULL,
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN4, YLEN4, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "宽度",
#else
     "Width",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE  | WS_BORDER| WS_TABSTOP,
     XLEN5, YLEN4, CTRLWIDTH, HEIGHT,
     IDC_WIDTH, 
     "",
     ES_FLOAT},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN4, YLEN5, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "高度",
#else
     "Height",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE | WS_BORDER| WS_TABSTOP,
     XLEN5, YLEN5, CTRLWIDTH, HEIGHT,
     IDC_HEIGHT, 
     "",
     ES_FLOAT},
/*************************************/
    {
     CTRL_STATIC,
     WS_VISIBLE | SS_GROUPBOX,
     XLEN7, YLEN3, XLEN14-XLEN7, YLEN6 -YLEN3,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "页边距设置",
#else
     "Page Margin Setting",
#endif
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN8, YLEN4, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "上",
#else
     "Top",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE  | WS_BORDER| WS_TABSTOP,
     XLEN9, YLEN4, CTRLWIDTH, HEIGHT,
     IDC_TOP, 
     "",
     ES_FLOAT},

    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN8, YLEN5, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "下",
#else
     "Bottom",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE  | WS_BORDER| WS_TABSTOP,
     XLEN9, YLEN5, CTRLWIDTH, HEIGHT,
     IDC_BOTTOM, 
     "",
     ES_FLOAT},

    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN11, YLEN4, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "左",
#else
     "Left",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE  | WS_BORDER| WS_TABSTOP,
     XLEN12, YLEN4, CTRLWIDTH, HEIGHT,
     IDC_LEFT, 
     "",
     ES_FLOAT},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN11, YLEN5, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "右",
#else
     "Right",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE  | WS_BORDER| WS_TABSTOP,
     XLEN12, YLEN5, CTRLWIDTH, HEIGHT,
     IDC_RIGHT, 
     "",
     ES_FLOAT},
/*************************************/
    {
     CTRL_STATIC,
     WS_VISIBLE | SS_GROUPBOX,
     XLEN0, YLEN7, XLEN6 - XLEN0, YLEN12-YLEN7,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "打印页面",
#else
     "Print Page",
#endif
     0},
    {
        "button",
        WS_VISIBLE | BS_AUTORADIOBUTTON| BS_CHECKED | WS_GROUP, 
        XLEN1, YLEN8, CTRLWIDTH+40, HEIGHT,
        IDC_PRINTCURRENT, 
#ifdef _LANG_ZHCN
        "当前页",
#else
        "Print current page",
#endif
        0
    },
    {
        "button",
        WS_VISIBLE | BS_AUTORADIOBUTTON ,
        XLEN1, YLEN9,CTRLWIDTH, HEIGHT,
        IDC_PRINTALL,
#ifdef _LANG_ZHCN
        "全部",
#else
        "Print All",
#endif
        0
    },
    {
        "button",
        WS_VISIBLE | BS_AUTORADIOBUTTON, 
        XLEN1, YLEN10, CTRLWIDTH+40, HEIGHT,
        IDC_PRINTSELECT, 
#ifdef _LANG_ZHCN
        "页码范围",
#else
        "Print selected page",
#endif
        0
    },
    {
     CTRL_SLEDIT,
     WS_VISIBLE | WS_BORDER| WS_TABSTOP,
     XLEN4, YLEN10, CTRLWIDTH+40, HEIGHT,
     IDC_SELECTPAGE,
     NULL,
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN1, YLEN11, 200, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "输入页码范围",
#else
     "Input the page you want to print ",
#endif
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE | SS_GROUPBOX,
     XLEN7, YLEN7, XLEN14-XLEN7, YLEN12-YLEN7,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "打印信息",
#else
     "Print Info",
#endif
     0},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN8, YLEN8, NAMEWIDTH, HEIGHT,
     IDC_STATIC1,
#ifdef _LANG_ZHCN
     "份数",
#else
     "Copies",
#endif
     0},
    {
     CTRL_SLEDIT,
     WS_VISIBLE | WS_BORDER | WS_TABSTOP,
     XLEN9, YLEN8, CTRLWIDTH, HEIGHT,
     IDC_COPY,
     NULL,
     ES_INTEGER},
    {
     CTRL_STATIC,
     WS_VISIBLE,
     XLEN8, YLEN9, 200, HEIGHT,
     IDC_PAGETOTAL,
#ifdef _LANG_ZHCN
     "总页数：",
#else
     "Pages Total:",
#endif
     0},
};


static const char *papersize[]=
{
    "A4",
    "B5",
    "Letter",
   "Legal",
    "Executive",
    "A0", "A1","A2","A3", "A5","A6", "A7", "A8", "A9",
    "B0", "B1", "B10", "B2", "B3", "B4", "B6","B7", "B8","B9",
    "C5E",
    "Comm10E",
    "DLE",
    "Folio",
    "Ledger",
    "Tabloid",
#ifdef _LANG_ZHCN
    "自定义",
#else
    "Custom",
#endif
};
static const char *unit[]=
{
#ifdef _LANG_ZHCN
    "毫米",
#else
    "mm",
#endif
#ifdef _LANG_ZHCN
    "英寸",
#else
    "inch",
#endif
};

static MGPRINT_INFO * pinfo = NULL; 
static int range_method;
static int sec_names(const char * str ,  char * name);
static FILE * pf;
static char pline[256];
static char pname[64];
static int unit_type = 0;

static WNDPROC old_edit_proc;


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



static int DigitEditBox(HWND hwnd, int message, WPARAM wParam, LPARAM lParam)
{
    if (MSG_CHAR == message) {
        DWORD style = GetWindowAdditionalData (hwnd); 
     
        if (style & ES_INTEGER){
            if ((wParam == 127) ||((wParam >= '0') && (wParam <= '9'))) 
                ;
            else
                return 0;
        } else if (style & ES_FLOAT) {
            if ((wParam == 127) ||(wParam == '.') || ((wParam >= '0') && (wParam <= '9')))
                ;
            else
                return 0;
        }
    }
        
    return (*old_edit_proc)(hwnd, message, wParam, lParam);
}

static void papersize_notif_proc (HWND hwnd, int id, int nc, DWORD add_data)
{
    HWND hwndp;

    char pbuf [32] = {0};
    float fsize;
    hwndp = GetParent(hwnd);
    if (nc == CBN_SELCHANGE) 
    {
        int cur_sel = SendMessage (hwnd, CB_GETCURSEL, 0, 0);

        if(cur_sel== Custom)
        {
            SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);
            SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);
            EnableWindow(GetDlgItem(hwndp,IDC_WIDTH),TRUE);
            EnableWindow(GetDlgItem(hwndp,IDC_HEIGHT),TRUE);
        }
        else
        {
            if (unit_type == 0) /*MM*/
            {
                fsize = ((PageSizes[cur_sel].width)*254 - 127)/720;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);

                fsize = ((PageSizes[cur_sel].height)*254 - 127)/720;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);

            }
            else /* INCH*/
            {

                fsize = (PageSizes[cur_sel].width)/72;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);

                fsize = (PageSizes[cur_sel].height)/72;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);
            }    
            
            EnableWindow(GetDlgItem(hwndp,IDC_WIDTH),FALSE);
            EnableWindow(GetDlgItem(hwndp,IDC_HEIGHT),FALSE);
        }
    }
}

static void printer_notif_proc (HWND hwnd, int id, int nc, DWORD add_data)
{
    if (nc == CBN_SELCHANGE) {
        SendMessage (hwnd, CB_GETCURSEL, 0, 0);
    }
}
static void edit_notif_proc (HWND hwnd, int id, int nc, DWORD add_data)
{
    char p[32] = {0};
    if (nc == EN_CHANGE) {
            GetWindowText (hwnd , p , 32);
        
            if (unit_type == 0) /*MM*/
            {
                if (atof(p) > 2000)
                    SetWindowText(hwnd , "2000");
            }
            else
            {
                if (atof(p) > 78)
                    SetWindowText(hwnd , "78");
            }   
    }
}

static void unit_notif_proc (HWND hwnd, int id, int nc, DWORD add_data)
{
    HWND hwndp;
    int cur_sel , cur;
    char pbuf [32] = {0};
    float fsize;
    hwndp = GetParent(hwnd);
    if (nc == CBN_SELCHANGE) {
        cur = SendMessage (hwnd, CB_GETCURSEL, 0, 0);
        unit_type = cur;    

        cur_sel = SendMessage (GetDlgItem (hwndp, IDC_RANGE), CB_GETCURSEL, 0, 0);

        
        if(cur_sel != Custom) {
            if (unit_type == 0) /*MM*/
            {
                fsize = ((PageSizes[cur_sel].width)*254 - 127)/720;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);

                fsize = ((PageSizes[cur_sel].height)*254 - 127)/720;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);

            }
            else /* INCH*/
            {

                fsize = (PageSizes[cur_sel].width)/72;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);

                fsize = (PageSizes[cur_sel].height)/72;
                sprintf (pbuf , "%.2f" , fsize);
                SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);
            }    

        }
        else
        {
                SetWindowText(GetDlgItem(hwndp,IDC_WIDTH), pbuf);
                SetWindowText(GetDlgItem(hwndp,IDC_HEIGHT), pbuf);

        }
    }
}
static void printpage_notif_proc (HWND hwnd, int id, int nc, DWORD add_data)
{
    HWND hwndp;

    hwndp = GetParent(hwnd);
    
    if(nc == BN_CLICKED)
    {
       switch(id)
      {
        case IDC_PRINTALL:
            range_method = 1;
            EnableWindow(GetDlgItem(hwndp,IDC_SELECTPAGE),FALSE);
            break;
        case IDC_PRINTCURRENT:
            range_method = 2;
            EnableWindow(GetDlgItem(hwndp,IDC_SELECTPAGE),FALSE);
            break;
        case IDC_PRINTSELECT:
            range_method = 3;
            EnableWindow(GetDlgItem(hwndp,IDC_SELECTPAGE),TRUE);
            break;
      }
    }
}

static int
PrinterDialogBoxProc(HWND hDlg, int message, WPARAM wParam, LPARAM lParam)
{
    int i;
    char tmp[32];
    float fsize;
    switch (message)
    {
    case MSG_INITDIALOG:
        
        while (NULL != fgets(pline , 255 , pf)) {
            if (sec_names (pline , pname) == 0)
                SendDlgItemMessage(hDlg, IDC_PRINTER, CB_ADDSTRING, 0, (LPARAM)pname);
        }
        fclose(pf);

        /* Initialize*/
#ifdef _LANG_ZHCN
	if (pinfo->nr_totalpages == 0)
	    sprintf(tmp, "总页数： 未知");
	else
     	    sprintf(tmp, "总页数： %d", pinfo->nr_totalpages);
#else
	if (pinfo->nr_totalpages == 0)
	    sprintf(tmp, "Pages Total: unknown");
	else
          sprintf(tmp, "Pages Total: %d", pinfo->nr_totalpages);
#endif

        SetWindowText(GetDlgItem(hDlg, IDC_PAGETOTAL), tmp);
        

        SetWindowText(GetDlgItem(hDlg,IDC_HEIGHT),"");
        SendMessage(GetDlgItem(hDlg,IDC_HEIGHT), EM_LIMITTEXT, 8, 0L);
        SetWindowText(GetDlgItem(hDlg,IDC_WIDTH),"");
        SendMessage(GetDlgItem(hDlg,IDC_WIDTH), EM_LIMITTEXT, 8, 0L);

        SetWindowText(GetDlgItem(hDlg,IDC_TOP),"0");
        SendMessage(GetDlgItem(hDlg,IDC_TOP), EM_LIMITTEXT, 5, 0L);
        SetWindowText(GetDlgItem(hDlg,IDC_LEFT),"0");
        SendMessage(GetDlgItem(hDlg,IDC_LEFT), EM_LIMITTEXT, 5, 0L);
        SetWindowText(GetDlgItem(hDlg,IDC_RIGHT),"0");
        SendMessage(GetDlgItem(hDlg,IDC_RIGHT), EM_LIMITTEXT, 5, 0L);
        SetWindowText(GetDlgItem(hDlg,IDC_BOTTOM),"0");
        SendMessage(GetDlgItem(hDlg,IDC_BOTTOM), EM_LIMITTEXT, 5, 0L);

        SetWindowText(GetDlgItem(hDlg,IDC_COPY),"1");
        SendMessage(GetDlgItem(hDlg,IDC_COPY), EM_LIMITTEXT, 4, 0L);

        EnableWindow(GetDlgItem(hDlg,IDC_SELECTPAGE),FALSE);
        EnableWindow(GetDlgItem(hDlg,IDC_WIDTH),FALSE);
        EnableWindow(GetDlgItem(hDlg,IDC_HEIGHT),FALSE);

        for (i = 0; i < (int)TABLESIZE(papersize); i++) {
            SendDlgItemMessage(hDlg, IDC_RANGE, CB_ADDSTRING, 0, (LPARAM)papersize[i]);
        }

        for (i = 0; i < (int)TABLESIZE(unit); i++) {
            SendDlgItemMessage(hDlg, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)unit[i]);
        }
        for(i = IDC_PRINTALL;i <= IDC_PRINTSELECT; i++)
        {
            SetNotificationCallback(GetDlgItem(hDlg,i),printpage_notif_proc);
        }

        SetNotificationCallback (GetDlgItem (hDlg, IDC_RANGE), papersize_notif_proc);
        SendDlgItemMessage(hDlg, IDC_RANGE, CB_SETCURSEL, 0, 0);

        SetNotificationCallback (GetDlgItem (hDlg, IDC_PRINTER), printer_notif_proc);
        SendDlgItemMessage(hDlg, IDC_PRINTER, CB_SETCURSEL, 0, 0);

        SetNotificationCallback (GetDlgItem (hDlg, IDC_UNIT), unit_notif_proc);
        SendDlgItemMessage(hDlg, IDC_UNIT, CB_SETCURSEL, 0, 0);


        SetNotificationCallback (GetDlgItem (hDlg, IDC_WIDTH), edit_notif_proc);
        SetNotificationCallback (GetDlgItem (hDlg, IDC_HEIGHT), edit_notif_proc);

        range_method = 2;
        fsize = ((PageSizes[0].width)*254 - 127)/720;
        sprintf (tmp , "%.2f" , fsize);
        SetWindowText(GetDlgItem(hDlg,IDC_WIDTH), tmp);

        fsize = ((PageSizes[0].height)*254 - 127)/720;
        sprintf (tmp , "%.2f" , fsize);
        SetWindowText(GetDlgItem(hDlg,IDC_HEIGHT), tmp);


        old_edit_proc = SetWindowCallbackProc (GetDlgItem(hDlg,IDC_COPY), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_WIDTH), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_HEIGHT), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_TOP), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_LEFT), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_RIGHT), DigitEditBox);
        SetWindowCallbackProc (GetDlgItem(hDlg,IDC_BOTTOM), DigitEditBox);

        return 1;


    case MSG_COMMAND:
        switch (wParam)
        {
        case IDC_OK:
            {
            int len, cur_sel;
            char temp[32] = {0};

            pinfo->printer_name = (char*)calloc(1, NAME_MAX + 1);
            GetWindowText(GetDlgItem(hDlg,IDC_PRINTER), pinfo->printer_name ,
                                 GetWindowTextLength(GetDlgItem(hDlg,IDC_PRINTER)));

            pinfo->ppd_name = (char*)calloc(1, NAME_MAX + 1);
            if (NULL == pinfo->ppd_name)
            {
                free (pinfo->printer_name);
                fprintf (stderr , "Not have enough memory!\n");
                EndDialog(hDlg, -2);
            }
            else
            {
                if (GetValueFromEtcFile (PATH_DRIVER"/etc/mgprinter.cfg", pinfo->printer_name,
                                             "ppd", pinfo->ppd_name, NAME_MAX) < 0)
                {   
                    free (pinfo->printer_name);
                    free (pinfo->ppd_name);
                    fprintf (stderr , "Init ppd name error!\n");
                    EndDialog(hDlg, -2);
                }
            }


            /* Get page range*/    
            switch (range_method){
                case 1:
                    pinfo->page_range = (char*)calloc(2 , sizeof(char));
                    strcpy(pinfo->page_range , "a"); 
                    break;
                case 2:
                    pinfo->page_range = (char*)calloc(2 , sizeof(char));
                    strcpy(pinfo->page_range , "c"); 
                    break;
                case 3:
                    len = GetWindowTextLength(GetDlgItem(hDlg,IDC_SELECTPAGE));
                    pinfo->page_range = (char*)calloc(1 ,len + 1);
                    GetWindowText(GetDlgItem(hDlg,IDC_SELECTPAGE), pinfo->page_range, len);
                    break;
            }

            /* Get copies*/
            GetWindowText(GetDlgItem(hDlg,IDC_COPY),temp,32);
            pinfo->nr_copies = (unsigned int)atof(temp);
            
            /* Get margin*/
            GetWindowText(GetDlgItem(hDlg,IDC_TOP),temp,32);
            pinfo->margin_top = (float)atof(temp);

            GetWindowText(GetDlgItem(hDlg,IDC_LEFT),temp,32);
            pinfo->margin_left = (float)atof(temp);

            GetWindowText(GetDlgItem(hDlg,IDC_RIGHT),temp,32);
            pinfo->margin_right = (float)atof(temp);

            GetWindowText(GetDlgItem(hDlg,IDC_BOTTOM),temp,32);
            pinfo->margin_bottom = (float)atof(temp);

            /* Get unit*/
            cur_sel = SendMessage (GetDlgItem(hDlg , IDC_UNIT), CB_GETCURSEL, 0, 0);
            if (cur_sel == 0)
                 pinfo->flags = MGPI_DEPTH8 | MGPI_MM; /* 8 bit color and unit of mm*/
            else
                 pinfo->flags = MGPI_DEPTH8 | MGPI_INCH; /* 8 bit color and unit of inch*/

            /* Get page size*/
            cur_sel = SendMessage (GetDlgItem(hDlg , IDC_RANGE), CB_GETCURSEL, 0, 0);
            if (cur_sel == Custom)
            {
                GetWindowText(GetDlgItem(hDlg,IDC_WIDTH),temp,32);
                pinfo->custom_width = (float)atof(temp);

                GetWindowText(GetDlgItem(hDlg,IDC_HEIGHT),temp,32);
                pinfo->custom_height = (float)atof(temp);


                pinfo->page_type = (PAGE_TYPE)cur_sel;
            }else
                pinfo->page_type = (PAGE_TYPE)cur_sel;
            /* commit*/
            EndDialog(hDlg, 0);
            break;
            }
        case IDC_CANCEL:
            EndDialog(hDlg, -1);
            break;
        }
        break;
    }

    return DefaultDialogProc(hDlg, message, wParam, lParam);
}

int
CreatePrinterSettingDialog(HWND hwnd , void * info , unsigned int totalpages)
{
    int ret;
    pf = fopen (PATH_DRIVER"/etc/mgprinter.cfg" , "r");

    if (NULL == pf){
        perror("Can not open /etc/mgprinter.cfg !\n");
        return -2;
    }

    pinfo = (MGPRINT_INFO *)info;
    pinfo->nr_totalpages = totalpages;
    DlgPrinterSetting.controls = CtrlPrinterSetting;
    ret = DialogBoxIndirectParam(&DlgPrinterSetting, hwnd, PrinterDialogBoxProc, (DWORD)info);

    return ret;
}

static int sec_names(const char * str ,  char * name)
{
    const char * ps = str;
    const char * pe = str;
    
    ps = strchr (str , '[');
    if (NULL == ps)
        return -1;
    
    pe = strchr (str , ']');
    
    memcpy (name , ps+1 , pe-ps-1);
    return 0;
}
