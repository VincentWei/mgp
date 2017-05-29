/*
 * "$Id: common.c,v 1.5 2007-11-23 06:12:11 jpzhang Exp $"
 *
 *  Copyright (C) 2002-2005 Feynman Software.
 *
 * Contents:
 *
 *   SetCommonOptions() - Set common filter options for media size,
 *                        etc.
 *   UpdatePageVars()   - Update the page variables for the orientation.
 *   WriteCommon()      - Write common procedures...
 *   WriteLabelProlog() - Write the prolog with the classification
 *                        and page label.
 *   WriteLabels()      - Write the actual page labels.
 */

/*
 * Include necessary headers...
 */
#include "mgpconfig.h"
#include "common.h"
#include <locale.h>
#include "ppd.h"

#ifdef NOUNIX
#include "portclib.h"
#endif

/*
 * Globals...
 */

int	Orientation = 0,	/* 0 = portrait, 1 = landscape, etc. */
	Duplex = 0,		/* Duplexed? */
	LanguageLevel = 1,	/* Language level of printer */
	ColorDevice = 1;	/* Do color text? */
float	PageLeft = 18.0f,	/* Left margin */
	PageRight = 594.0f,	/* Right margin */
	PageBottom = 36.0f,	/* Bottom margin */
	PageTop = 756.0f,	/* Top margin */
	PageWidth = 612.0f,	/* Total page width */
	PageLength = 792.0f;	/* Total page length */


/*
 * 'SetCommonOptions()' - Set common filter options for media size, etc.
 */

ppd_file_t *					/* O - PPD file */
SetCommonOptions(const char * ppdFilename , int           num_options,	/* I - Number of options */
                 PrinterOption *options,	/* I - Options */
		 int           change_size)	/* I - Change page size? */
{
  ppd_file_t	*ppd;		/* PPD file */
  ppd_size_t	*pagesize;	/* Current page size */
  const char	*val;		/* Option value */


#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */

  ppd = ppdOpenFile(ppdFilename);

  ppdMarkDefaults(ppd);
  MarkOptions(ppd, num_options, options);

  if ((pagesize = ppdPageSize(ppd, NULL)) != NULL)
  {
    PageWidth  = pagesize->width;
    PageLength = pagesize->length;
    PageTop    = pagesize->top;
    PageBottom = pagesize->bottom;
    PageLeft   = pagesize->left;
    PageRight  = pagesize->right;

    fprintf(stderr, "DEBUG: Page = %.0fx%.0f; %.0f,%.0f to %.0f,%.0f\n",
            PageWidth, PageLength, PageLeft, PageBottom, PageRight, PageTop);
  }

  if (ppd != NULL)
  {
    ColorDevice   = ppd->color_device;
    LanguageLevel = ppd->language_level;
  }

  if ((val = GetOption("landscape", num_options, options)) != NULL)
  {
    if (strcasecmp(val, "no") != 0 && strcasecmp(val, "off") != 0 &&
        strcasecmp(val, "false") != 0)
    {
      if (ppd && ppd->landscape > 0)
        Orientation = 1;
      else
        Orientation = 3;
    }
  }
  else if ((val = GetOption("orientation-requested", num_options, options)) != NULL)
  {
   /*
    * Map IPP orientation values to 0 to 3:
    *
    *   3 = 0 degrees   = 0
    *   4 = 90 degrees  = 1
    *   5 = -90 degrees = 3
    *   6 = 180 degrees = 2
    */

    Orientation = atoi(val) - 3;
    if (Orientation >= 2)
      Orientation ^= 1;
  }

  if ((val = GetOption("page-left", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageLeft = (float)atof(val);
	  break;
      case 1 :
          PageBottom = (float)atof(val);
	  break;
      case 2 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 3 :
          PageTop = PageLength - (float)atof(val);
	  break;
    }
  }

  if ((val = GetOption("page-right", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 1 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 2 :
          PageLeft = (float)atof(val);
	  break;
      case 3 :
          PageBottom = (float)atof(val);
	  break;
    }
  }

  if ((val = GetOption("page-bottom", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageBottom = (float)atof(val);
	  break;
      case 1 :
          PageLeft = (float)atof(val);
	  break;
      case 2 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 3 :
          PageRight = PageWidth - (float)atof(val);
	  break;
    }
  }

  if ((val = GetOption("page-top", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 1 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 2 :
          PageBottom = (float)atof(val);
	  break;
      case 3 :
          PageLeft = (float)atof(val);
	  break;
    }
  }

  if (change_size)
    UpdatePageVars();

  if (ppdIsMarked(ppd, "Duplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "Duplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "JCLDuplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "JCLDuplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "EFDuplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "EFDuplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "KD03Duplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "KD03Duplex", "DuplexTumble"))
    Duplex = 1;

  return (ppd);
}


/*
 * 'UpdatePageVars()' - Update the page variables for the orientation.
 */

void
UpdatePageVars(void)
{
  float		temp;		/* Swapping variable */


  switch (Orientation & 3)
  {
    case 0 : /* Portait */
        break;

    case 1 : /* Landscape */
	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;

    case 2 : /* Reverse Portrait */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;
        break;

    case 3 : /* Reverse Landscape */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;

	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;
  }
}


/*
 * 'WriteCommon()' - Write common procedures...
 */

void
WriteCommon(void)
{
  puts("% x y w h ESPrc - Clip to a rectangle.\n"
       "userdict/ESPrc/rectclip where{pop/rectclip load}\n"
       "{{newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath clip newpath}bind}ifelse put");
  puts("% x y w h ESPrf - Fill a rectangle.\n"
       "userdict/ESPrf/rectfill where{pop/rectfill load}\n"
       "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath fill grestore}bind}ifelse put");
  puts("% x y w h ESPrs - Stroke a rectangle.\n"
       "userdict/ESPrs/rectstroke where{pop/rectstroke load}\n"
       "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath stroke grestore}bind}ifelse put");
}


/*
 * 'WriteLabelProlog()' - Write the prolog with the classification
 *                        and page label.
 */

void
WriteLabelProlog(const char *label,	/* I - Page label */
		 float      bottom,	/* I - Bottom position in points */
		 float      top,	/* I - Top position in points */
		 float      width)	/* I - Width in points */
{
  const char	*classification;	/* CLASSIFICATION environment variable */
  const char	*ptr;			/* Temporary string pointer */


 /*
  * First get the current classification...
  */

  if ((classification = getenv("CLASSIFICATION")) == NULL)
    classification = "";
  if (strcmp(classification, "none") == 0)
    classification = "";

 /*
  * If there is nothing to show, bind an empty 'write labels' procedure
  * and return...
  */

  if (!classification[0] && (label == NULL || !label[0]))
  {
    puts("userdict/ESPwl{}bind put");
    return;
  }

 /*
  * Set the classification + page label string...
  */

  printf("userdict");
  if (strcmp(classification, "confidential") == 0)
    printf("/ESPpl(CONFIDENTIAL");
  else if (strcmp(classification, "classified") == 0)
    printf("/ESPpl(CLASSIFIED");
  else if (strcmp(classification, "secret") == 0)
    printf("/ESPpl(SECRET");
  else if (strcmp(classification, "topsecret") == 0)
    printf("/ESPpl(TOP SECRET");
  else if (strcmp(classification, "unclassified") == 0)
    printf("/ESPpl(UNCLASSIFIED");
  else
  {
    printf("/ESPpl(");

    for (ptr = classification; *ptr; ptr ++)
      if (*ptr < 32 || *ptr > 126)
        printf("\\%03o", *ptr);
      else if (*ptr == '_')
        putchar(' ');
      else
      {
	if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	  putchar('\\');

	putchar(*ptr);
      }
  }

  if (label)
  {
    if (classification[0])
      printf(" - ");

   /*
    * Quote the label string as needed...
    */

    for (ptr = label; *ptr; ptr ++)
      if (*ptr < 32 || *ptr > 126)
        printf("\\%03o", *ptr);
      else
      {
	if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	  putchar('\\');

	putchar(*ptr);
      }
  }

  puts(")put");

 /*
  * Then get a 14 point Helvetica-Bold font...
  */

  puts("userdict/ESPpf /Helvetica-Bold findfont 14 scalefont put");

 /*
  * Finally, the procedure to write the labels on the page...
  */

  puts("userdict/ESPwl{");
  puts("  ESPpf setfont");
  printf("  ESPpl stringwidth pop dup 12 add exch -0.5 mul %.0f add\n",
         width * 0.5f);
  puts("  1 setgray");
  printf("  dup 6 sub %.0f 3 index 20 ESPrf\n", bottom - 2.0);
  printf("  dup 6 sub %.0f 3 index 20 ESPrf\n", top - 18.0);
  puts("  0 setgray");
  printf("  dup 6 sub %.0f 3 index 20 ESPrs\n", bottom - 2.0);
  printf("  dup 6 sub %.0f 3 index 20 ESPrs\n", top - 18.0);
  printf("  dup %.0f moveto ESPpl show\n", bottom + 2.0);
  printf("  %.0f moveto ESPpl show\n", top - 14.0);
  puts("pop");
  puts("}bind put");
}


/*
 * 'WriteLabels()' - Write the actual page labels.
 */

void
WriteLabels(int orient)	/* I - Orientation of the page */
{
  float	width,		/* Width of page */
	length;		/* Length of page */


  puts("gsave");

  if ((orient ^ Orientation) & 1)
  {
    width  = PageLength;
    length = PageWidth;
  }
  else
  {
    width  = PageWidth;
    length = PageLength;
  }

  switch (orient & 3)
  {
    case 1 : /* Landscape */
        printf("%.1f 0.0 translate 90 rotate\n", length);
        break;
    case 2 : /* Reverse Portrait */
        printf("%.1f %.1f translate 180 rotate\n", width, length);
        break;
    case 3 : /* Reverse Landscape */
        printf("0.0 %.1f translate -90 rotate\n", width);
        break;
  }

  puts("ESPwl");
  puts("grestore");
}

const char *				/* O - Option value or NULL */
GetOption(const char    *name,	/* I - Name of option */
              int           num_options,/* I - Number of options */
              PrinterOption *options)	/* I - Options */
{
  int	i;				/* Looping var */


  if (name == NULL || num_options <= 0 || options == NULL)
    return (NULL);

  for (i = 0; i < num_options; i ++)
    if (strcasecmp(options[i].name, name) == 0)
      return (options[i].value);

  return (NULL);
}

int						/* O - 1 if conflicting */
MarkOptions(ppd_file_t    *ppd,		/* I - PPD file */
                int           num_options,	/* I - Number of options */
                PrinterOption *options)		/* I - Options */
{
  int		i;				/* Looping var */
  int		conflict;			/* Option conflicts */
  char		*val,				/* Pointer into value */
		*ptr,				/* Pointer into string */
		s[255];				/* Temporary string */
  PrinterOption *optptr;			/* Current option */


 /*
  * Check arguments...
  */

  if (ppd == NULL || num_options <= 0 || options == NULL)
    return (0);

 /*
  * Mark options...
  */

  conflict = 0;

  for (i = num_options, optptr = options; i > 0; i --, optptr ++)
    if (strcasecmp(optptr->name, "media") == 0)
    {
     /*
      * Loop through the option string, separating it at commas and
      * marking each individual option.
      */

      for (val = optptr->value; *val;)
      {
       /*
        * Extract the sub-option from the string...
	*/

        for (ptr = s; *val && *val != ',' && (ptr - s) < (int)(sizeof(s) - 1);)
	  *ptr++ = *val++;
	*ptr++ = '\0';

	if (*val == ',')
	  val ++;

       /*
        * Mark it...
	*/

        if (GetOption("PageSize", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "PageSize", s))
            conflict = 1;

        if (GetOption("InputSlot", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "InputSlot", s))
            conflict = 1;

        if (GetOption("MediaType", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "MediaType", s))
            conflict = 1;

        if (GetOption("EFMediaQualityMode", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "EFMediaQualityMode", s))	/* EFI */
            conflict = 1;

	if (strcasecmp(s, "manual") == 0 &&
	    GetOption("ManualFeed", num_options, options) == NULL)
          if (ppdMarkOption(ppd, "ManualFeed", "True"))
	    conflict = 1;
      }
    }
    else if (strcasecmp(optptr->name, "sides") == 0)
    {
      if (GetOption("Duplex", num_options, options) != NULL ||
          GetOption("JCLDuplex", num_options, options) != NULL ||
          GetOption("EFDuplex", num_options, options) != NULL ||
          GetOption("KD03Duplex", num_options, options) != NULL)
      {
       /*
        * Don't override the PPD option with the IPP attribute...
	*/

        continue;
      }
      else if (strcasecmp(optptr->value, "one-sided") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "None"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "None"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "None"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "None"))	/* Kodak */
	  conflict = 1;
      }
      else if (strcasecmp(optptr->value, "two-sided-long-edge") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "DuplexNoTumble"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "DuplexNoTumble"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "DuplexNoTumble"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "DuplexNoTumble"))	/* Kodak */
	  conflict = 1;
      }
      else if (strcasecmp(optptr->value, "two-sided-short-edge") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "DuplexTumble"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "DuplexTumble"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "DuplexTumble"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "DuplexTumble"))	/* Kodak */
	  conflict = 1;
      }
    }
    else if (strcasecmp(optptr->name, "resolution") == 0 ||
             strcasecmp(optptr->name, "printer-resolution") == 0)
    {
      if (ppdMarkOption(ppd, "Resolution", optptr->value))
        conflict = 1;
      if (ppdMarkOption(ppd, "SetResolution", optptr->value))
      	/* Calcomp, Linotype, QMS, Summagraphics, Tektronix, Varityper */
        conflict = 1;
      if (ppdMarkOption(ppd, "JCLResolution", optptr->value))	/* HP */
        conflict = 1;
      if (ppdMarkOption(ppd, "CNRes_PGP", optptr->value))	/* Canon */
        conflict = 1;
    }
    else if (strcasecmp(optptr->name, "output-bin") == 0)
    {
      if (GetOption("OutputBin", num_options, options) == NULL)
        if (ppdMarkOption(ppd, "OutputBin", optptr->value))
          conflict = 1;
    }
    else if (ppdMarkOption(ppd, optptr->name, optptr->value))
      conflict = 1;

  return (conflict);
}


int					/* O - Number of conflicts */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              const char *option,	/* I - Keyword */
              const char *choice)	/* I - Option name */
{
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Option pointer */
  ppd_choice_t	*c;		/* Choice pointer */


  if (ppd == NULL)
    return (0);

  if (strcasecmp(option, "PageSize") == 0 && strncasecmp(choice, "Custom.", 7) == 0)
  {
   /*
    * Handle variable page sizes...
    */

    ppdPageSize(ppd, choice);
    choice = "Custom";
  }

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (strcasecmp(c->choice, choice) == 0)
      break;

  if (i)
  {
   /*
    * Option found; mark it and then handle unmarking any other options.
    */

    c->marked = 1;

    if (o->ui != PPD_UI_PICKMANY)
      for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
	if (strcasecmp(c->choice, choice) != 0)
          c->marked = 0;

    if (strcasecmp(option, "PageSize") == 0 || strcasecmp(option, "PageRegion") == 0)
    {
     /*
      * Mark current page size...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	ppd->sizes[i].marked = strcasecmp(ppd->sizes[i].name, choice) == 0;

     /*
      * Unmark the current PageSize or PageRegion setting, as appropriate...
      */

      if (strcasecmp(option, "PageSize") == 0)
      {
	if ((o = ppdFindOption(ppd, "PageRegion")) != NULL)
	  for (i = 0; i < o->num_choices; i ++)
            o->choices[i].marked = 0;
      }
      else
      {
	if ((o = ppdFindOption(ppd, "PageSize")) != NULL)
	  for (i = 0; i < o->num_choices; i ++)
            o->choices[i].marked = 0;
      }
    }
    else if (strcasecmp(option, "InputSlot") == 0)
    {
     /*
      * Unmark ManualFeed option...
      */

      if ((o = ppdFindOption(ppd, "ManualFeed")) != NULL)
	for (i = 0; i < o->num_choices; i ++)
          o->choices[i].marked = 0;
    }
    else if (strcasecmp(option, "ManualFeed") == 0)
    {
     /*
      * Unmark InputSlot option...
      */

      if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
	for (i = 0; i < o->num_choices; i ++)
          o->choices[i].marked = 0;
    }
  }

  return (ppdConflicts(ppd));
}

static void
ppd_defaults(ppd_file_t  *ppd,	/* I - PPD file */
             ppd_group_t *g)	/* I - Group to default */
{
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Current option */
  ppd_group_t	*sg;		/* Current sub-group */


  if (g == NULL)
    return;

  for (i = g->num_options, o = g->options; i > 0; i --, o ++)
    if (strcasecmp(o->keyword, "PageRegion") != 0)
      ppdMarkOption(ppd, o->keyword, o->defchoice);

  for (i = g->num_subgroups, sg = g->subgroups; i > 0; i --, sg ++)
    ppd_defaults(ppd, sg);
}

void
ppdMarkDefaults(ppd_file_t *ppd)/* I - PPD file record */
{
  int		i;		/* Looping variables */
  ppd_group_t	*g;		/* Current group */


  if (ppd == NULL)
    return;

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
    ppd_defaults(ppd, g);
}

int				/* O - Non-zero if option is marked */
ppdIsMarked(ppd_file_t *ppd,	/* I - PPD file data */
            const char *option,	/* I - Option/Keyword name */
            const char *choice)	/* I - Choice name */
{
  ppd_option_t	*o;		/* Option pointer */
  ppd_choice_t	*c;		/* Choice pointer */


  if (ppd == NULL)
    return (0);

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);

  if ((c = ppdFindChoice(o, choice)) == NULL)
    return (0);

  return (c->marked);
}

int				/* O - Number of conflicts found */
ppdConflicts(ppd_file_t *ppd)	/* I - PPD to check */
{
  int		i, j, k,	/* Looping variables */
		conflicts;	/* Number of conflicts */
  ppd_const_t	*c;		/* Current constraint */
  ppd_group_t	*g, *sg;	/* Groups */
  ppd_option_t	*o1, *o2;	/* Options */
  ppd_choice_t	*c1, *c2;	/* Choices */


  if (ppd == NULL)
    return (0);

 /*
  * Clear all conflicts...
  */

  conflicts = 0;

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o1 = g->options; j > 0; j --, o1 ++)
      o1->conflicted = 0;

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o1 = sg->options; k > 0; k --, o1 ++)
        o1->conflicted = 0;
  }

 /*
  * Loop through all of the UI constraints and flag any options
  * that conflict...
  */

  for (i = ppd->num_consts, c = ppd->consts; i > 0; i --, c ++)
  {
   /*
    * Grab pointers to the first option...
    */

    o1 = ppdFindOption(ppd, c->option1);

    if (o1 == NULL)
      continue;
    else if (c->choice1[0] != '\0')
    {
     /*
      * This constraint maps to a specific choice.
      */

      c1 = ppdFindChoice(o1, c->choice1);
    }
    else
    {
     /*
      * This constraint applies to any choice for this option.
      */

      for (j = o1->num_choices, c1 = o1->choices; j > 0; j --, c1 ++)
        if (c1->marked)
	  break;

      if (j == 0 ||
          strcasecmp(c1->choice, "None") == 0 ||
          strcasecmp(c1->choice, "Off") == 0 ||
          strcasecmp(c1->choice, "False") == 0)
        c1 = NULL;
    }

   /*
    * Grab pointers to the second option...
    */

    o2 = ppdFindOption(ppd, c->option2);

    if (o2 == NULL)
      continue;
    else if (c->choice2[0] != '\0')
    {
     /*
      * This constraint maps to a specific choice.
      */

      c2 = ppdFindChoice(o2, c->choice2);
    }
    else
    {
     /*
      * This constraint applies to any choice for this option.
      */

      for (j = o2->num_choices, c2 = o2->choices; j > 0; j --, c2 ++)
        if (c2->marked)
	  break;

      if (j == 0 ||
          strcasecmp(c2->choice, "None") == 0 ||
          strcasecmp(c2->choice, "Off") == 0 ||
          strcasecmp(c2->choice, "False") == 0)
        c2 = NULL;
    }

   /*
    * If both options are marked then there is a conflict...
    */

    if (c1 != NULL && c1->marked &&
        c2 != NULL && c2->marked)
    {
      
      conflicts ++;
      o1->conflicted = 1;
      o2->conflicted = 1;
    }
  }

 /*
  * Return the number of conflicts found...
  */

  return (conflicts);
}


static int						/* O - Number of options */
AddOption(const char    *name,		/* I - Name of option */
              const char    *value,		/* I - Value of option */
	      int           num_options,	/* I - Number of options */
            PrinterOption  **options)		/* IO - Pointer to options */
{
  int		i;				/* Looping var */
 PrinterOption 	*temp;				/* Pointer to new option */


  if (name == NULL || !name[0] || value == NULL ||
      options == NULL || num_options < 0)
    return (num_options);

 /*
  * Look for an existing option with the same name...
  */

  for (i = 0, temp = *options; i < num_options; i ++, temp ++)
    if (strcasecmp(temp->name, name) == 0)
      break;

  if (i >= num_options)
  {
   /*
    * No matching option name...
    */

    if (num_options == 0)
      temp = (PrinterOption *)malloc(sizeof(PrinterOption));
    else
      temp = (PrinterOption *)realloc(*options, sizeof(PrinterOption) *
                                        	(num_options + 1));

    if (temp == NULL)
      return (0);

    *options    = temp;
    temp        += num_options;
    temp->name  = strdup(name);
    num_options ++;
  }
  else
  {
   /*
    * Match found; free the old value...
    */

    free(temp->value);
  }

  temp->value = strdup(value);

  return (num_options);
}



int						/* O - Number of options found */
ParseOptions(const char    *arg,		/* I - Argument to parse */
                 int           num_options,	/* I - Number of options */
             PrinterOption   **options)	/* O - Options found */
{
  char	*copyarg,				/* Copy of input string */
	*ptr,					/* Pointer into string */
	*name,					/* Pointer to name */
	*value;					/* Pointer to value */


  if (arg == NULL || options == NULL || num_options < 0)
    return (0);

 /*
  * Make a copy of the argument string and then divide it up...
  */

  copyarg     = strdup(arg);
  ptr         = copyarg;

 /*
  * Skip leading spaces...
  */

  while (isspace(*ptr & 255))
    ptr ++;

 /*
  * Loop through the string...
  */

  while (*ptr != '\0')
  {
   /*
    * Get the name up to a SPACE, =, or end-of-string...
    */

    name = ptr;
    while (!isspace(*ptr & 255) && *ptr != '=' && *ptr != '\0')
      ptr ++;

   /*
    * Avoid an empty name...
    */

    if (ptr == name)
      break;

   /*
    * Skip trailing spaces...
    */

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

    if (*ptr != '=')
    {
     /*
      * Start of another option...
      */

      if (strncasecmp(name, "no", 2) == 0)
        num_options = AddOption(name + 2, "false", num_options,
	                            options);
      else
        num_options = AddOption(name, "true", num_options, options);

      continue;
    }

   /*
    * Remove = and parse the value...
    */

    *ptr++ = '\0';

    if (*ptr == '\'')
    {
     /*
      * Quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\'' && *ptr != '\0')
      {
        if (*ptr == '\\')
	  strcpy(ptr, ptr + 1);

        ptr ++;
      }

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else if (*ptr == '\"')
    {
     /*
      * Double-quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\"' && *ptr != '\0')
      {
        if (*ptr == '\\')
	  strcpy(ptr, ptr + 1);

        ptr ++;
      }

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else if (*ptr == '{')
    {
     /*
      * Collection value...
      */

      int depth;

      value = ptr;

      for (depth = 1; *ptr; ptr ++)
        if (*ptr == '{')
	  depth ++;
	else if (*ptr == '}')
	{
	  depth --;
	  if (!depth)
	  {
	    ptr ++;

	    if (*ptr != ',')
	      break;
	  }
        }
        else if (*ptr == '\\')
	  strcpy(ptr, ptr + 1);

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else
    {
     /*
      * Normal space-delimited string...
      */

      value = ptr;

      while (!isspace(*ptr & 255) && *ptr != '\0')
      {
        if (*ptr == '\\')
	  strcpy(ptr, ptr + 1);

        ptr ++;
      }
    }

   /*
    * Skip trailing whitespace...
    */

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

   /*
    * Add the string value...
    */

    num_options = AddOption(name, value, num_options, options);
  }

 /*
  * Free the copy of the argument we made and return the number of options
  * found.
  */

  free(copyarg);

  return (num_options);
}
/*
 * End of "$Id: common.c,v 1.5 2007-11-23 06:12:11 jpzhang Exp $".
*/

