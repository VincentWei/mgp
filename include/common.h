/**
 * \file common.h
 * \author Jipeng Zhang <jpzhang@minigui.org>
 * \date 2005/12/22
 *
 \verbatim
    Copyright (C) 2006 Feynman Software.
 \endverbatim
 */

/*
 * $Id: common.h,v 1.4 2006-01-14 07:51:18 xwyan Exp $
 *
 * Copyright (C) 2006 Feynman Software, all rights reserved.
 *
 * URL: http://www.minigui.com
 *
 * Author : Jipeng Zhang  
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "raster.h"
#include "ppd.h"
/*
 * C++ magic...
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct				/**** Printer Options ****/
{
  char		*name;			/* Name of option */
  char		*value;			/* Value of option */
} option_t;



/*
 * Globals...
 */

extern int	Orientation,	/* 0 = portrait, 1 = landscape, etc. */
		Duplex,		/* Duplexed? */
		LanguageLevel,	/* Language level of printer */
		ColorDevice;	/* Do color text? */
extern float	PageLeft,	/* Left margin */
		PageRight,	/* Right margin */
		PageBottom,	/* Bottom margin */
		PageTop,	/* Top margin */
		PageWidth,	/* Total page width */
		PageLength;	/* Total page length */


/*
 * Prototypes...
 */

extern ppd_file_t *SetCommonOptions(const char * ppdFilename , int num_options, PrinterOption *options,
		                    int change_size);
extern void	UpdatePageVars(void);
extern void	WriteCommon(void);
extern void	WriteLabelProlog(const char *label, float bottom,
		                 float top, float width);
extern void	WriteLabels(int orient);

extern const char *				/* O - Option value or NULL */
GetOption(const char    *name,	/* I - Name of option */
              int           num_options,/* I - Number of options */
              PrinterOption *options);	/* I - Options */

extern int						/* O - 1 if conflicting */
MarkOptions(ppd_file_t    *ppd,		/* I - PPD file */
                int           num_options,	/* I - Number of options */
                PrinterOption *options);		/* I - Options */
extern int					/* O - Number of conflicts */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              const char *option,	/* I - Keyword */
              const char *choice);	/* I - Option name */
extern void
ppdMarkDefaults(ppd_file_t *ppd);/* I - PPD file record */

extern int						/* O - Number of options found */
ParseOptions(const char    *arg,		/* I - Argument to parse */
                 int           num_options,	/* I - Number of options */
             PrinterOption   **options);	/* O - Options found */
/*
 * C++ magic...
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

