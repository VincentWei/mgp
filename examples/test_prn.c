/* 
** $Id: test_prn.c,v 1.39 2007-12-07 07:07:45 jpzhang Exp $
**
** test_prn.c: Sample program for MiniGUI Programming Guide
**         Print a table to printer.
**
** Copyright (C) 2004 Feynman Software.
**
** License: GPL
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#ifdef VxWorks
#include "drv/parallel/lptDrv.h"
#endif

#include "printing.h"
#include "pic.h"

static void DrawTestPage (HDC hdc)
{
	char *format[] = {"%I: %M: %S %p %m/%d %a", "%x %X %Y", NULL};
	char buf[30];
	time_t clock;
	struct tm *tm;
	BITMAP bmp;
	
	time(&clock);
	tm = gmtime(&clock);
	
	strftime(buf, sizeof(buf), format[0], tm);
	
	if (LoadBitmapFromMem (hdc, &bmp, fm_data_bmp, sizeof(fm_data_bmp), "bmp"))
		printf("Loading FMSoft Logo image faild!\n");
    
	FillBoxWithBitmap(hdc, 0, 0, 320/2, 240/2, &bmp);

	TextOut(hdc, 10, 150, "This is a test page of MiniGUI printing component mGp V1.2.");
	TextOut(hdc, 10, 180, "Congratulations now you already have made mGp run on you system successfully!");
	TextOut(hdc, 10, 210, "Current printer: Epson Stylus C65 micro printer.");
	TextOut(hdc, 10, 240, buf);
	UnloadBitmap (&bmp);

	return ;
}

static int Printpage (const MGPRINT_INFO* info, void * context, HDC hdc, int pageno)
{
    DrawTestPage (hdc); /* draw MiniGUI Logo*/
    return 0;
}

static int Printmydoc (HWND hWnd)
{
    MGPRINT_INFO info;
    memset (&info, 0, sizeof(MGPRINT_INFO));

    if (!mgpInitPrintInfo (&info))  /* Init the printer, info is the default values*/
        return -1; 

    info.page_type = A4;
    info.nr_copies = 1;

    if (mgpStartPrintDocs (&info, Printpage, 0, NULL) < 0) {
        fprintf (stderr,"print error !\n");
        mgpReleasePrintInfo(&info);
		MessageBox (hWnd, "print error!", "eror", MB_OK);
        return -1;
    }

    MessageBox (hWnd, "print ok!", "eror", MB_OK);
    mgpReleasePrintInfo (&info);
    return 0;
}

static int LoadBmpWinProc(HWND hWnd, int message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    switch (message) {
        case MSG_CREATE:
    		Printmydoc (hWnd); /* draw MiniGUI Logo*/
            break;

		case MSG_PAINT:
			hdc = BeginPaint (hWnd);
			DrawTestPage (hdc);
			EndPaint (hWnd, hdc);
			return 0;

        case MSG_CLOSE:
            DestroyMainWindow (hWnd);
            PostQuitMessage (hWnd);
            return 0;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

int MiniGUIMain (int argc, const char* argv[])
{
    MSG Msg;
    HWND hMainWnd;
    MAINWINCREATE CreateInfo;

#ifdef VxWorks
       lptDevCreate ("/lpt/1", 1);/*FIXME: change this for your kernel config and bios config*/
#endif

#ifdef _MGRM_PROCESSES
    JoinLayer(NULL , "test" , 0 , 0);
#endif
    
    CreateInfo.dwStyle = WS_VISIBLE | WS_BORDER | WS_CAPTION;
    CreateInfo.dwExStyle = WS_EX_NONE;
    CreateInfo.spCaption = "Sample for mGp V1.2.0";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = LoadBmpWinProc;
    CreateInfo.lx = 0;
    CreateInfo.ty = 0;
    CreateInfo.rx = 640;
    CreateInfo.by = 440;
    CreateInfo.iBkColor = PIXEL_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = HWND_DESKTOP;
    
    hMainWnd = CreateMainWindow (&CreateInfo);

    if (hMainWnd == HWND_INVALID)
        return -1;

    ShowWindow (hMainWnd, SW_SHOWNORMAL);

    while (GetMessage(&Msg, hMainWnd)) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    MainWindowThreadCleanup (hMainWnd);
    return 0;
}


#ifdef VxWorks
void start_minigui (void *data)
{
  minigui_entry(0, data);
}

void create_minigui(void)
{
  pthread_t th;
  pthread_attr_t new_attr;
  pthread_attr_init (&new_attr);
  pthread_attr_setdetachstate (&new_attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize (&new_attr, 512 * 1024);
  pthread_create(&th, &new_attr, start_minigui, NULL);
  pthread_attr_destroy (&new_attr);
  pthread_join(th, NULL);
}

#endif

#ifndef _LITE_VERSION
#include <minigui/dti.c>
#endif

