/*
 * Include necessary headers.
 *  Copyright (C) 2002-2005 Feynman Software.
 */

#include "mgpconfig.h"
#include "ppd.h"
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include "string.h"

#ifdef NOUNIX
#include <minigui/common.h>
#include "portclib.h"
#endif

/*
 * Definitions...
 */

#if defined(WIN32) || defined(__EMX__)
#  define READ_BINARY	"rb"		/* Open a binary file for reading */
#  define WRITE_BINARY	"wb"		/* Open a binary file for writing */
#else
#  define READ_BINARY	"r"		/* Open a binary file for reading */
#  define WRITE_BINARY	"w"		/* Open a binary file for writing */
#endif /* WIN32 || __EMX__ */

#define ppd_free(p)	if (p) free(p)	/* Safe free macro */

#define PPD_KEYWORD	1		/* Line contained a keyword */
#define PPD_OPTION	2		/* Line contained an option name */
#define PPD_TEXT	4		/* Line contained human-readable text */
#define PPD_STRING	8		/* Line contained a string or code */


/*
 * Local globals...
 */

static ppd_status_t	ppd_status = PPD_OK;
					/* Status of last ppdOpen*() */
static int		ppd_line = 0;	/* Current line number */
static ppd_conform_t	ppd_conform = PPD_CONFORM_RELAXED;
					/* Level of conformance required */


/*
 * Local functions...
 */

static ppd_attr_t	*ppd_add_attr(ppd_file_t *ppd, const char *name,
			              const char *spec, const char *text,
				      const char *value);
static ppd_choice_t	*ppd_add_choice(ppd_option_t *option, const char *name);
static ppd_size_t	*ppd_add_size(ppd_file_t *ppd, const char *name);
#ifndef __APPLE__
static int		ppd_compare_groups(ppd_group_t *g0, ppd_group_t *g1);
static int		ppd_compare_options(ppd_option_t *o0, ppd_option_t *o1);
#endif /* !__APPLE__ */
static int		ppd_decode(char *string);
#ifndef __APPLE__
static void		ppd_fix(char *string);
#else
#  define		ppd_fix(s)
#endif /* !__APPLE__ */
static void		ppd_free_group(ppd_group_t *group);
static void		ppd_free_option(ppd_option_t *option);
static ppd_group_t	*ppd_get_group(ppd_file_t *ppd, const char *name,
			               const char *text);
static ppd_option_t	*ppd_get_option(ppd_group_t *group, const char *name);
static int		ppd_read(FILE *fp, char *keyword, char *option,
			         char *text, char **string, int ignoreblank);



static void RestoreLocale (int category, char* oldlocale)
{
      if (oldlocale)
      {
           /*
            *     * Reset the locale and free the locale string...
            *         */

        setlocale(category, oldlocale);
        free(oldlocale);
      }

}

/*
 * 'ppdFindOption()' - Return a pointer to the specified option.
 */

ppd_option_t *				/* O - Pointer to option or NULL */
ppdFindOption(ppd_file_t *ppd,		/* I - PPD file data */
              const char *option)	/* I - Option/Keyword name */
{
  int		i, j, k;	/* Looping vars */
  ppd_option_t	*o;		/* Pointer to option */
  ppd_group_t	*g,		/* Pointer to group */
		*sg;		/* Pointer to subgroup */


  if (ppd == NULL || option == NULL)
    return (NULL);

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o = g->options; j > 0; j --, o ++)
      if (strcasecmp(o->keyword, option) == 0)
	return (o);

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o = sg->options; k > 0; k --, o ++)
	if (strcasecmp(o->keyword, option) == 0)
	  return (o);
  }

  return (NULL);
}

/*
 * 'ppdFindChoice()' - Return a pointer to an option choice.
 */

ppd_choice_t *				/* O - Choice pointer or NULL */
ppdFindChoice(ppd_option_t *o,		/* I - Pointer to option */
              const char   *choice)	/* I - Name of choice */
{
  int		i;		/* Looping var */
  ppd_choice_t	*c;		/* Current choice */


  if (o == NULL || choice == NULL)
    return (NULL);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (strcasecmp(c->choice, choice) == 0)
      return (c);

  return (NULL);
}

/*
 * 'ppdPageSize()' - Get the page size record for the given size.
 */

ppd_size_t *			/* O - Size record for page or NULL */
ppdPageSize(ppd_file_t *ppd,	/* I - PPD file record */
            const char *name)	/* I - Size name */
{
  int	i;			/* Looping var */
  float	w, l;			/* Width and length of page */
  char	units[255];		/* Page size units... */


  if (ppd == NULL)
    return (NULL);

  if (name != NULL)
  {
    if (strncmp(name, "Custom.", 7) == 0 && ppd->variable_sizes)
    {
     /*
      * Find the custom page size...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	if (strcmp("Custom", ppd->sizes[i].name) == 0)
          break;

      if (i == ppd->num_sizes)
        return (NULL);

     /*
      * Variable size; size name can be one of the following:
      *
      *    Custom.WIDTHxLENGTHin    - Size in inches
      *    Custom.WIDTHxLENGTHcm    - Size in centimeters
      *    Custom.WIDTHxLENGTHmm    - Size in millimeters
      *    Custom.WIDTHxLENGTH[pt]  - Size in points
      */

      units[0] = '\0';
      if (sscanf(name + 7, "%fx%f%254s", &w, &l, units) < 2)
        return (NULL);

      if (strcasecmp(units, "in") == 0)
      {
        ppd->sizes[i].width  = w * 72.0f;
	ppd->sizes[i].length = l * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l * 72.0f - ppd->custom_margins[3];
      }
      else if (strcasecmp(units, "cm") == 0)
      {
        ppd->sizes[i].width  = w / 2.54f * 72.0f;
	ppd->sizes[i].length = l / 2.54f * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w / 2.54f * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l / 2.54f * 72.0f - ppd->custom_margins[3];
      }
      else if (strcasecmp(units, "mm") == 0)
      {
        ppd->sizes[i].width  = w / 25.4f * 72.0f;
	ppd->sizes[i].length = l / 25.4f * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w / 25.4f * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l / 25.4f * 72.0f - ppd->custom_margins[3];
      }
      else
      {
        ppd->sizes[i].width  = w;
	ppd->sizes[i].length = l;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w - ppd->custom_margins[2];
	ppd->sizes[i].top    = l - ppd->custom_margins[3];
      }

      return (ppd->sizes + i);
    }
    else
    {
     /*
      * Lookup by name...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	if (strcmp(name, ppd->sizes[i].name) == 0)
          return (ppd->sizes + i);
    }
  }
  else
  {
   /*
    * Find default...
    */

    for (i = 0; i < ppd->num_sizes; i ++)
      if (ppd->sizes[i].marked)
        return (ppd->sizes + i);
  }

  return (NULL);
}

/*
 * '_ppd_attr_compare()' - Compare two attributes.
 */

int					/* O - Result of comparison */
_ppd_attr_compare(ppd_attr_t **a,	/* I - First attribute */
                  ppd_attr_t **b)	/* I - Second attribute */
{
  int	ret;				/* Result of comparison */


  if ((ret = strcasecmp((*a)->name, (*b)->name)) != 0)
    return (ret);
  else if ((*a)->spec[0] && (*b)->spec[0])
    return (strcasecmp((*a)->spec, (*b)->spec));
  else
    return (0);
}


/*
 * 'ppdClose()' - Free all memory used by the PPD file.
 */

void
ppdClose(ppd_file_t *ppd)		/* I - PPD file record */
{
  int		i;			/* Looping var */
  ppd_emul_t	*emul;			/* Current emulation */
  ppd_group_t	*group;			/* Current group */
  char		**font;			/* Current font */
  char		**filter;		/* Current filter */
  ppd_attr_t	**attr;			/* Current attribute */


 /*
  * Range check the PPD file record...
  */

  if (ppd == NULL)
    return;

 /*
  * Free all strings at the top level...
  */

  ppd_free(ppd->patches);
  ppd_free(ppd->jcl_begin);
  ppd_free(ppd->jcl_end);
  ppd_free(ppd->jcl_ps);

 /*
  * Free any emulations...
  */

  if (ppd->num_emulations > 0)
  {
    for (i = ppd->num_emulations, emul = ppd->emulations; i > 0; i --, emul ++)
    {
      ppd_free(emul->start);
      ppd_free(emul->stop);
    }

    ppd_free(ppd->emulations);
  }

 /*
  * Free any UI groups, subgroups, and options...
  */

  if (ppd->num_groups > 0)
  {
    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      ppd_free_group(group);

    ppd_free(ppd->groups);
  }

 /*
  * Free any page sizes...
  */

  if (ppd->num_sizes > 0)
  {
    ppd_free(ppd->sizes);
  }

 /*
  * Free any constraints...
  */

  if (ppd->num_consts > 0)
  {
    ppd_free(ppd->consts);
  }

 /*
  * Free any filters...
  */

  if (ppd->num_filters > 0)
  {
    for (i = ppd->num_filters, filter = ppd->filters; i > 0; i --, filter ++)
    {
      ppd_free(*filter);
    }

    ppd_free(ppd->filters);
  }

 /*
  * Free any fonts...
  */

  if (ppd->num_fonts > 0)
  {
    for (i = ppd->num_fonts, font = ppd->fonts; i > 0; i --, font ++)
    {
      ppd_free(*font);
    }

    ppd_free(ppd->fonts);
  }

 /*
  * Free any profiles...
  */

  if (ppd->num_profiles > 0)
  {
    ppd_free(ppd->profiles);
  }

 /*
  * Free any attributes...
  */

  if (ppd->num_attrs > 0)
  {
    for (i = ppd->num_attrs, attr = ppd->attrs; i > 0; i --, attr ++)
    {
      ppd_free((*attr)->value);
      ppd_free(*attr);
    }

    ppd_free(ppd->attrs);
  }

 /*
  * Free the whole record...
  */

  ppd_free(ppd);
}


/*
 * 'ppdErrorString()' - Returns the text assocated with a status.
 */

const char *				/* O - Status string */
ppdErrorString(ppd_status_t status)	/* I - PPD status */
{
  static const char * const messages[] =/* Status messages */
		{
		  "OK",
		  "Unable to open PPD file",
		  "NULL PPD file pointer",
		  "Memory allocation error",
		  "Missing PPD-Adobe-4.x header",
		  "Missing value string",
		  "Internal error",
		  "Bad OpenGroup",
		  "OpenGroup without a CloseGroup first",
		  "Bad OpenUI/JCLOpenUI",
		  "OpenUI/JCLOpenUI without a CloseUI/JCLCloseUI first",
		  "Bad OrderDependency",
		  "Bad UIConstraints",
		  "Missing asterisk in column 1",
		  "Line longer than the maximum allowed (255 characters)",
		  "Illegal control character",
		  "Illegal main keyword string",
		  "Illegal option keyword string",
		  "Illegal translation string",
		  "Illegal whitespace character"
		};


  if (status < PPD_OK || status > PPD_ILLEGAL_WHITESPACE)
    return ("Unknown");
  else
    return (messages[status]);
}


/*
 * 'ppdLastError()' - Return the status from the last ppdOpen*().
 */

ppd_status_t				/* O - Status code */
ppdLastError(int *line)			/* O - Line number */
{
  if (line)
    *line = ppd_line;

  return (ppd_status);
}


/*
 * 'ppdOpen()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpen(FILE *fp)			/* I - File to read from */
{
  char			*oldlocale;	/* Old locale settings */
  int			i, j, k, m;	/* Looping vars */
  int			count;		/* Temporary count */
  ppd_file_t		*ppd;		/* PPD file record */
  ppd_group_t		*group,		/* Current group */
			*subgroup;	/* Current sub-group */
  ppd_option_t		*option;	/* Current option */
  ppd_choice_t		*choice;	/* Current choice */
  ppd_const_t		*constraint;	/* Current constraint */
  ppd_size_t		*size;		/* Current page size */
  int			mask;		/* Line data mask */
  char			keyword[PPD_MAX_NAME],
  					/* Keyword from file */
			name[PPD_MAX_NAME],
					/* Option from file */
			text[PPD_MAX_LINE],
					/* Human-readable text from file */
			*string,	/* Code/text from file */
			*sptr,		/* Pointer into string */
			*nameptr,	/* Pointer into name */
			*temp,		/* Temporary string pointer */
			**tempfonts;	/* Temporary fonts pointer */
  float			order;		/* Order dependency number */
  ppd_section_t		section;	/* Order dependency section */
  ppd_profile_t		*profile;	/* Pointer to color profile */
  char			**filter;	/* Pointer to filter */
  int			ui_keyword;	/* Is this line a UI keyword? */
  static const char * const ui_keywords[] =
			{
			  /* Boolean keywords */
			  "BlackSubstitution",
			  "Booklet",
			  "Collate",
			  "ManualFeed",
			  "MirrorPrint",
			  "NegativePrint",
			  "Sorter",
			  "TraySwitch",

			  /* PickOne keywords */
			  "AdvanceMedia",
			  "BindColor",
			  "BindEdge",
			  "BindType",
			  "BindWhen",
			  "BitsPerPixel",
			  "ColorModel",
			  "CutMedia",
			  "Duplex",
			  "FoldType",
			  "FoldWhen",
			  "InputSlot",
			  "JCLFrameBufferSize",
			  "JCLResolution",
			  "Jog",
			  "MediaColor",
			  "MediaType",
			  "MediaWeight",
			  "OutputBin",
			  "OutputMode",
			  "OutputOrder",
			  "PageRegion",
			  "PageSize",
			  "Resolution",
			  "Separations",
			  "Signature",
			  "Slipsheet",
			  "Smoothing",
			  "StapleLocation",
			  "StapleOrientation",
			  "StapleWhen",
			  "StapleX",
			  "StapleY"
			};


 /*
  * Default to "OK" status...
  */

  ppd_status = PPD_OK;
  ppd_line   = 0;

 /*
  * Range check input...
  */

  if (fp == NULL)
  {
    ppd_status = PPD_NULL_FILE;
    return (NULL);
  }

 /*
  * Grab the first line and make sure it reads '*PPD-Adobe: "major.minor"'...
  */

  mask = ppd_read(fp, keyword, name, text, &string, 0);

  if (mask == 0 ||
      strcmp(keyword, "PPD-Adobe") != 0 ||
      string == NULL || string[0] != '4')
  {
   /*
    * Either this is not a PPD file, or it is not a 4.x PPD file.
    */

    if (ppd_status == PPD_OK)
      ppd_status = PPD_MISSING_PPDADOBE4;

    ppd_free(string);

    return (NULL);
  }

  ppd_free(string);

 /*
  * Allocate memory for the PPD file record...
  */

  if ((ppd = (ppd_file_t*)calloc(1, sizeof(ppd_file_t))) == NULL)
  {
    ppd_status = PPD_ALLOC_ERROR;

    return (NULL);
  }

  ppd->language_level = 1;
  ppd->color_device   = 0;
  ppd->colorspace     = PPD_CS_GRAY;
  ppd->landscape      = -90;

 /*
  * Get the default language for the user...
  */

#ifdef LC_NUMERIC
  if ((oldlocale = setlocale(LC_NUMERIC, NULL)) != NULL)
      oldlocale = strdup(oldlocale);
  setlocale (LC_NUMERIC, "C");
#else
  if ((oldlocale = setlocale(LC_ALL, NULL)) != NULL)
      oldlocale = strdup(oldlocale);
  setlocale (LC_ALL, "C");
#endif /* LC_NUMERIC */

 /*
  * Read lines from the PPD file and add them to the file record...
  */

  group      = NULL;
  subgroup   = NULL;
  option     = NULL;
  choice     = NULL;
  ui_keyword = 0;

  while ((mask = ppd_read(fp, keyword, name, text, &string, 1)) != 0)
  {
#ifdef DEBUG
    printf("mask = %x, keyword = \"%s\"", mask, keyword);

    if (name[0] != '\0')
      printf(", name = \"%s\"", name);

    if (text[0] != '\0')
      printf(", text = \"%s\"", text);

    if (string != NULL)
    {
      if (strlen(string) > 40)
        printf(", string = %p", string);
      else
        printf(", string = \"%s\"", string);
    }

    puts("");
#endif /* DEBUG */

    if (strcmp(keyword, "CloseUI") && strcmp(keyword, "CloseGroup") &&
	strcmp(keyword, "CloseSubGroup") && strncmp(keyword, "Default", 7) &&
        strcmp(keyword, "JCLCloseUI") && strcmp(keyword, "JCLOpenUI") &&
	strcmp(keyword, "OpenUI") && strcmp(keyword, "OpenGroup") &&
	strcmp(keyword, "OpenSubGroup") && string == NULL)
    {
     /*
      * Need a string value!
      */

      ppd_status = PPD_MISSING_VALUE;

      goto error;
    }

   /*
    * Certain main keywords (as defined by the PPD spec) may be used
    * without the usual OpenUI/CloseUI stuff.  Presumably this is just
    * so that Adobe wouldn't completely break compatibility with PPD
    * files prior to v4.0 of the spec, but it is hopelessly
    * inconsistent...  Catch these main keywords and automatically
    * create the corresponding option, as needed...
    */

    if (ui_keyword)
    {
     /*
      * Previous line was a UI keyword...
      */

      option     = NULL;
      ui_keyword = 0;
    }

    if (option == NULL &&
        (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	    (PPD_KEYWORD | PPD_OPTION | PPD_STRING))
    {
      for (i = 0; i < (int)(sizeof(ui_keywords) / sizeof(ui_keywords[0])); i ++)
        if (!strcmp(keyword, ui_keywords[i]))
	  break;

      if (i < (int)(sizeof(ui_keywords) / sizeof(ui_keywords[0])))
      {
       /*
        * Create the option in the appropriate group...
	*/

        ui_keyword = 1;

        if (!group)
	{
          if (strcmp(keyword, "Collate") && strcmp(keyword, "Duplex") &&
              strcmp(keyword, "InputSlot") && strcmp(keyword, "ManualFeed") &&
              strcmp(keyword, "MediaType") && strcmp(keyword, "MediaColor") &&
              strcmp(keyword, "MediaWeight") && strcmp(keyword, "OutputBin") &&
              strcmp(keyword, "OutputMode") && strcmp(keyword, "OutputOrder") &&
	      strcmp(keyword, "PageSize") && strcmp(keyword, "PageRegion"))
	    group = ppd_get_group(ppd, "Extra", "Extra");
	  else
	    group = ppd_get_group(ppd, "General", "General");

          if (group == NULL)
	    goto error;

          option = ppd_get_option(group, keyword);
	  group  = NULL;
	}
	else
          option = ppd_get_option(group, keyword);

	if (option == NULL)
	{
          ppd_status = PPD_ALLOC_ERROR;

          goto error;
	}

       /*
	* Now fill in the initial information for the option...
	*/

	if (!strncmp(keyword, "JCL", 3))
          option->section = PPD_ORDER_JCL;
	else
          option->section = PPD_ORDER_ANY;

	option->order = 10.0f;

	if (i < 8)
          option->ui = PPD_UI_BOOLEAN;
	else
          option->ui = PPD_UI_PICKONE;

        for (j = 0; j < ppd->num_attrs; j ++)
	  if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	      !strcmp(ppd->attrs[j]->name + 7, keyword) &&
	      ppd->attrs[j]->value)
	  {
	    strncpy(option->defchoice, ppd->attrs[j]->value,
	            sizeof(option->defchoice));
	    break;
	  }

        if (strcmp(keyword, "PageSize") == 0)
	  strncpy(option->text, "Media Size", sizeof(option->text));
	else if (strcmp(keyword, "MediaType") == 0)
	  strncpy(option->text, "Media Type", sizeof(option->text));
	else if (strcmp(keyword, "InputSlot") == 0)
	  strncpy(option->text, "Media Source", sizeof(option->text));
	else if (strcmp(keyword, "ColorModel") == 0)
	  strncpy(option->text, "Output Mode", sizeof(option->text));
	else if (strcmp(keyword, "Resolution") == 0)
	  strncpy(option->text, "Resolution", sizeof(option->text));
        else
	  strncpy(option->text, keyword, sizeof(option->text));
      }
    }

    if (strcmp(keyword, "LanguageLevel") == 0)
      ppd->language_level = atoi(string);
    else if (strcmp(keyword, "LanguageEncoding") == 0)
      ppd->lang_encoding = string;
    else if (strcmp(keyword, "LanguageVersion") == 0)
      ppd->lang_version = string;
    else if (strcmp(keyword, "Manufacturer") == 0)
      ppd->manufacturer = string;
    else if (strcmp(keyword, "ModelName") == 0)
      ppd->modelname = string;
    else if (strcmp(keyword, "Protocols") == 0)
      ppd->protocols = string;
    else if (strcmp(keyword, "PCFileName") == 0)
      ppd->pcfilename = string;
    else if (strcmp(keyword, "NickName") == 0)
      ppd->nickname = string;
    else if (strcmp(keyword, "Product") == 0)
      ppd->product = string;
    else if (strcmp(keyword, "ShortNickName") == 0)
      ppd->shortnickname = string;
    else if (strcmp(keyword, "TTRasterizer") == 0)
      ppd->ttrasterizer = string;
    else if (strcmp(keyword, "JCLBegin") == 0)
    {
      ppd->jcl_begin = strdup(string);
      ppd_decode(ppd->jcl_begin);	/* Decode quoted string */
    }
    else if (strcmp(keyword, "JCLEnd") == 0)
    {
      ppd->jcl_end = strdup(string);
      ppd_decode(ppd->jcl_end);		/* Decode quoted string */
    }
    else if (strcmp(keyword, "JCLToPSInterpreter") == 0)
    {
      ppd->jcl_ps = strdup(string);
      ppd_decode(ppd->jcl_ps);		/* Decode quoted string */
    }
    else if (strcmp(keyword, "AccurateScreensSupport") == 0)
      ppd->accurate_screens = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ColorDevice") == 0)
      ppd->color_device = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ContoneOnly") == 0)
      ppd->contone_only = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "FlipDuplex") == 0)
      ppd->flip_duplex = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ManualCopies") == 0)
      ppd->manual_copies = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ModelNumber") == 0)
      ppd->model_number = atoi(string);
    else if (strcmp(keyword, "ColorProfile") == 0)
    {
      if (ppd->num_profiles == 0)
        profile = (ppd_profile_t*)malloc(sizeof(ppd_profile_t));
      else
        profile = (ppd_profile_t*)realloc(ppd->profiles, sizeof(ppd_profile_t) *
	                                 (ppd->num_profiles + 1));

      ppd->profiles     = profile;
      profile           += ppd->num_profiles;
      ppd->num_profiles ++;

      memset(profile, 0, sizeof(ppd_profile_t));
      strncpy(profile->resolution, name, sizeof(profile->resolution));
      strncpy(profile->media_type, text, sizeof(profile->media_type));
      sscanf(string, "%f%f%f%f%f%f%f%f%f%f%f", &(profile->density),
	     &(profile->gamma),
	     profile->matrix[0] + 0, profile->matrix[0] + 1,
	     profile->matrix[0] + 2, profile->matrix[1] + 0,
	     profile->matrix[1] + 1, profile->matrix[1] + 2,
	     profile->matrix[2] + 0, profile->matrix[2] + 1,
	     profile->matrix[2] + 2);
    }
    else if (strcmp(keyword, "Filter") == 0)
    {
      if (ppd->num_filters == 0)
        filter = (char**)malloc(sizeof(char *));
      else
        filter = (char**)realloc(ppd->filters, sizeof(char *) * (ppd->num_filters + 1));

      if (filter == NULL)
      {
        ppd_free(filter);

        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      ppd->filters     = filter;
      filter           += ppd->num_filters;
      ppd->num_filters ++;

     /*
      * Copy filter string and prevent it from being freed below...
      */

      *filter = string;
      string  = NULL;
    }
    else if (strcmp(keyword, "Throughput") == 0)
      ppd->throughput = atoi(string);
    else if (strcmp(keyword, "Font") == 0)
    {
     /*
      * Add this font to the list of available fonts...
      */

      if (ppd->num_fonts == 0)
        tempfonts = (char **)malloc(sizeof(char *));
      else
        tempfonts = (char **)realloc(ppd->fonts,
	                             sizeof(char *) * (ppd->num_fonts + 1));

      if (tempfonts == NULL)
      {
        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }
      
      ppd->fonts                 = tempfonts;
      ppd->fonts[ppd->num_fonts] = strdup(name);
      ppd->num_fonts ++;
    }
    else if (strcmp(keyword, "ParamCustomPageSize") == 0)
    {
      if (strcmp(name, "Width") == 0)
        sscanf(string, "%*s%*s%f%f", ppd->custom_min + 0,
	       ppd->custom_max + 0);
      else if (strcmp(name, "Height") == 0)
        sscanf(string, "%*s%*s%f%f", ppd->custom_min + 1,
	       ppd->custom_max + 1);
    }
    else if (strcmp(keyword, "HWMargins") == 0)
      sscanf(string, "%f%f%f%f", ppd->custom_margins + 0,
             ppd->custom_margins + 1, ppd->custom_margins + 2,
             ppd->custom_margins + 3);
    else if (strcmp(keyword, "CustomPageSize") == 0 &&
             strcmp(name, "True") == 0)
    {
      if (!ppd->variable_sizes)
      {
	ppd->variable_sizes = 1;

       /*
	* Add a "Custom" page size entry...
	*/

	ppd_add_size(ppd, "Custom");

       /*
	* Add a "Custom" page size option...
	*/

        if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
	{
	  ppd_group_t	*temp;

	  if ((temp = ppd_get_group(ppd, "General", "General")) == NULL)
	  {
	    goto error;
	  }

	  if ((option = ppd_get_option(temp, "PageSize")) == NULL)
	  {
            ppd_status = PPD_ALLOC_ERROR;

	    goto error;
	  }
        }

	if ((choice = ppd_add_choice(option, "Custom")) == NULL)
	{
          ppd_status = PPD_ALLOC_ERROR;

	  goto error;
	}

	strncpy(choice->text, "Custom Size", sizeof(choice->text));
	option = NULL;
      }

      if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
      {
        ppd_status = PPD_INTERNAL_ERROR;

	goto error;
      }

      if ((choice = ppdFindChoice(option, "Custom")) == NULL)
      {
        ppd_status = PPD_INTERNAL_ERROR;

	goto error;
      }

      choice->code = string;
      option       = NULL;
      string       = NULL;		/* Don't add as an attribute below */
    }
    else if (strcmp(keyword, "LandscapeOrientation") == 0)
    {
      if (strcmp(string, "Minus90") == 0)
        ppd->landscape = -90;
      else if (strcmp(string, "Plus90") == 0)
        ppd->landscape = 90;
    }
    else if (strcmp(keyword, "Emulators") == 0)
    {
      for (count = 1, sptr = string; sptr != NULL;)
        if ((sptr = strchr(sptr, ' ')) != NULL)
	{
	  count ++;
	  while (*sptr == ' ')
	    sptr ++;
	}

      ppd->num_emulations = count;
      ppd->emulations     = (ppd_emul_t*)calloc(count, sizeof(ppd_emul_t));

      for (i = 0, sptr = string; i < count; i ++)
      {
        for (nameptr = ppd->emulations[i].name;
	     *sptr != '\0' && *sptr != ' ';
	     sptr ++)
	  if (nameptr < (ppd->emulations[i].name + sizeof(ppd->emulations[i].name) - 1))
	    *nameptr++ = *sptr;

	*nameptr = '\0';

	while (*sptr == ' ')
	  sptr ++;
      }
    }
    else if (strncmp(keyword, "StartEmulator_", 14) == 0)
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (strcmp(keyword + 14, ppd->emulations[i].name) == 0)
	{
	  ppd->emulations[i].start = string;
	  string = NULL;
	}
    }
    else if (strncmp(keyword, "StopEmulator_", 13) == 0)
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (strcmp(keyword + 13, ppd->emulations[i].name) == 0)
	{
	  ppd->emulations[i].stop = string;
	  string = NULL;
	}
    }
    else if (strcmp(keyword, "JobPatchFile") == 0)
    {
      if (ppd->patches == NULL)
        ppd->patches = strdup(string);
      else
      {
        temp = (char*)realloc(ppd->patches, strlen(ppd->patches) +
	                             strlen(string) + 1);
        if (temp == NULL)
	{
          ppd_status = PPD_ALLOC_ERROR;

	  goto error;
	}

        ppd->patches = temp;

        strcpy(ppd->patches + strlen(ppd->patches), string);
      }
    }
    else if (strcmp(keyword, "OpenUI") == 0)
    {
     /*
      * Don't allow nesting of options...
      */

      if (option && ppd_conform == PPD_CONFORM_STRICT)
      {
        ppd_status = PPD_NESTED_OPEN_UI;

	goto error;
      }

     /*
      * Add an option record to the current sub-group, group, or file...
      */

      if (name[0] == '*')
        strcpy(name, name + 1); /* Eliminate leading asterisk */

      for (i = strlen(name) - 1; i > 0 && isspace(name[i] & 255); i --)
        name[i] = '\0'; /* Eliminate trailing spaces */

      if (subgroup != NULL)
        option = ppd_get_option(subgroup, name);
      else if (group == NULL)
      {
        if (strcmp(name, "Collate") && strcmp(name, "Duplex") &&
            strcmp(name, "InputSlot") && strcmp(name, "ManualFeed") &&
            strcmp(name, "MediaType") && strcmp(name, "MediaColor") &&
            strcmp(name, "MediaWeight") && strcmp(name, "OutputBin") &&
            strcmp(name, "OutputMode") && strcmp(name, "OutputOrder") &&
	    strcmp(name, "PageSize") && strcmp(name, "PageRegion"))
	  group = ppd_get_group(ppd, "Extra", "Extra");
	else
	  group = ppd_get_group(ppd, "General", "General");

        if (group == NULL)
	  goto error;

        option = ppd_get_option(group, name);
	group  = NULL;
      }
      else
        option = ppd_get_option(group, name);

      if (option == NULL)
      {
        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (string && strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (string && strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else if (string && strcmp(string, "PickOne") == 0)
        option->ui = PPD_UI_PICKONE;
      else if (ppd_conform == PPD_CONFORM_STRICT)
      {
        ppd_status = PPD_BAD_OPEN_UI;

	goto error;
      }
      else
        option->ui = PPD_UI_PICKONE;

      for (j = 0; j < ppd->num_attrs; j ++)
	if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	    !strcmp(ppd->attrs[j]->name + 7, name) &&
	    ppd->attrs[j]->value)
	{
	  strncpy(option->defchoice, ppd->attrs[j]->value,
	          sizeof(option->defchoice));
	  break;
	}

      if (text[0])
      {
        strncpy(option->text, text, sizeof(option->text));
	ppd_fix(option->text);
      }
      else
      {
        if (strcmp(name, "PageSize") == 0)
	  strncpy(option->text, "Media Size", sizeof(option->text));
	else if (strcmp(name, "MediaType") == 0)
	  strncpy(option->text, "Media Type", sizeof(option->text));
	else if (strcmp(name, "InputSlot") == 0)
	  strncpy(option->text, "Media Source", sizeof(option->text));
	else if (strcmp(name, "ColorModel") == 0)
	  strncpy(option->text, "Output Mode", sizeof(option->text));
	else if (strcmp(name, "Resolution") == 0)
	  strncpy(option->text, "Resolution", sizeof(option->text));
        else
	  strncpy(option->text, name, sizeof(option->text));
      }

      option->section = PPD_ORDER_ANY;

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "JCLOpenUI") == 0)
    {
     /*
      * Don't allow nesting of options...
      */

      if (option && ppd_conform == PPD_CONFORM_STRICT)
      {
        ppd_status = PPD_NESTED_OPEN_UI;

	goto error;
      }

     /*
      * Find the JCL group, and add if needed...
      */

      group = ppd_get_group(ppd, "JCL", "JCL");

      if (group == NULL)
	goto error;

     /*
      * Add an option record to the current JCLs...
      */

      if (name[0] == '*')
        strcpy(name, name + 1);

      option = ppd_get_option(group, name);

      if (option == NULL)
      {
        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (string && strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (string && strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else if (string && strcmp(string, "PickOne") == 0)
        option->ui = PPD_UI_PICKONE;
      else
      {
        ppd_status = PPD_BAD_OPEN_UI;

	goto error;
      }

      for (j = 0; j < ppd->num_attrs; j ++)
	if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	    !strcmp(ppd->attrs[j]->name + 7, name) &&
	    ppd->attrs[j]->value)
	{
	  strncpy(option->defchoice, ppd->attrs[j]->value,
	          sizeof(option->defchoice));
	  break;
	}

      strncpy(option->text, text, sizeof(option->text));

      option->section = PPD_ORDER_JCL;
      group = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "CloseUI") == 0 ||
             strcmp(keyword, "JCLCloseUI") == 0)
    {
      option = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "OpenGroup") == 0)
    {
     /*
      * Open a new group...
      */

      if (group != NULL)
      {
        ppd_status = PPD_NESTED_OPEN_GROUP;

	goto error;
      }

      if (!string)
      {
        ppd_status = PPD_BAD_OPEN_GROUP;

	goto error;
      }

     /*
      * Separate the group name from the text (name/text)...
      */

      if ((sptr = strchr(string, '/')) != NULL)
        *sptr++ = '\0';
      else
        sptr = string;

     /*
      * Fix up the text...
      */

      ppd_decode(sptr);
      ppd_fix(sptr);

     /*
      * Find/add the group...
      */

      group = ppd_get_group(ppd, string, sptr);

      if (group == NULL)
	goto error;

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "CloseGroup") == 0)
    {
      group = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "OrderDependency") == 0 ||
             strcmp(keyword, "NonUIOrderDependency") == 0)
    {
      if (sscanf(string, "%f%40s%40s", &order, name, keyword) != 3)
      {
        ppd_status = PPD_BAD_ORDER_DEPENDENCY;

	goto error;
      }

      if (keyword[0] == '*')
        strcpy(keyword, keyword + 1);

      if (strcmp(name, "ExitServer") == 0)
        section = PPD_ORDER_EXIT;
      else if (strcmp(name, "Prolog") == 0)
        section = PPD_ORDER_PROLOG;
      else if (strcmp(name, "DocumentSetup") == 0)
        section = PPD_ORDER_DOCUMENT;
      else if (strcmp(name, "PageSetup") == 0)
        section = PPD_ORDER_PAGE;
      else if (strcmp(name, "JCLSetup") == 0)
        section = PPD_ORDER_JCL;
      else
        section = PPD_ORDER_ANY;

      if (option == NULL)
      {
        ppd_group_t	*temp;


       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, temp = ppd->groups; i > 0; i --, temp ++)
          if (temp->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < temp->num_options; i ++)
	    if (strcmp(keyword, temp->options[i].keyword) == 0)
	    {
	      temp->options[i].section = section;
	      temp->options[i].order   = order;
	      break;
	    }
      }
      else
      {
        option->section = section;
	option->order   = order;
      }

      ppd_free(string);
      string = NULL;
    }
    else if (strncmp(keyword, "Default", 7) == 0)
    {
      if (string == NULL)
        continue;

     /*
      * Drop UI text, if any, from value...
      */

      if (strchr(string, '/') != NULL)
        *strchr(string, '/') = '\0';

     /*
      * Assign the default value as appropriate...
      */

      if (strcmp(keyword, "DefaultColorSpace") == 0)
      {
       /*
        * Set default colorspace...
	*/

	if (strcmp(string, "CMY") == 0)
          ppd->colorspace = PPD_CS_CMY;
	else if (strcmp(string, "CMYK") == 0)
          ppd->colorspace = PPD_CS_CMYK;
	else if (strcmp(string, "RGB") == 0)
          ppd->colorspace = PPD_CS_RGB;
	else if (strcmp(string, "RGBK") == 0)
          ppd->colorspace = PPD_CS_RGBK;
	else if (strcmp(string, "N") == 0)
          ppd->colorspace = PPD_CS_N;
	else
          ppd->colorspace = PPD_CS_GRAY;
      }
      else if (option && strcmp(keyword + 7, option->keyword) == 0)
      {
       /*
        * Set the default as part of the current option...
	*/

        strncpy(option->defchoice, string, sizeof(option->defchoice));
      }
      else
      {
       /*
        * Lookup option and set if it has been defined...
	*/

        ppd_option_t	*toption;	/* Temporary option */


        if ((toption = ppdFindOption(ppd, keyword + 7)) != NULL)
	{
	  strncpy(toption->defchoice, string, sizeof(toption->defchoice));
	}
      }
    }
    else if (strcmp(keyword, "UIConstraints") == 0 ||
             strcmp(keyword, "NonUIConstraints") == 0)
    {
      if (ppd->num_consts == 0)
	constraint = (ppd_const_t*)calloc(1, sizeof(ppd_const_t));
      else
	constraint = (ppd_const_t*)realloc(ppd->consts,
	                     (ppd->num_consts + 1) * sizeof(ppd_const_t));

      if (constraint == NULL)
      {
        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      ppd->consts = constraint;
      constraint += ppd->num_consts;
      ppd->num_consts ++;

      switch (sscanf(string, "%40s%40s%40s%40s", constraint->option1,
                     constraint->choice1, constraint->option2,
		     constraint->choice2))
      {
        case 0 : /* Error */
	case 1 : /* Error */
	    ppd_status = PPD_BAD_UI_CONSTRAINTS;
	    goto error;

	case 2 : /* Two options... */
	   /*
	    * The following strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->choice1[0] == '*')
	      strcpy(constraint->option2, constraint->choice1 + 1);
	    else
	      strcpy(constraint->option2, constraint->choice1);

            constraint->choice1[0] = '\0';
            constraint->choice2[0] = '\0';
	    break;
	    
	case 3 : /* Two options, one choice... */
	   /*
	    * The following strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->choice1[0] == '*')
	    {
	      strcpy(constraint->choice2, constraint->option2);
	      strcpy(constraint->option2, constraint->choice1 + 1);
              constraint->choice1[0] = '\0';
	    }
	    else
	    {
	      if (constraint->option2[0] == '*')
  	        strcpy(constraint->option2, constraint->option2 + 1);

              constraint->choice2[0] = '\0';
	    }
	    break;
	    
	case 4 : /* Two options, two choices... */
	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->option2[0] == '*')
  	      strcpy(constraint->option2, constraint->option2 + 1);
	    break;
      }

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "PaperDimension") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      sscanf(string, "%f%f", &(size->width), &(size->length));

      ppd_free(string);
      string = NULL;
    }
    else if (strcmp(keyword, "ImageableArea") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      sscanf(string, "%f%f%f%f", &(size->left), &(size->bottom),
	     &(size->right), &(size->top));

      ppd_free(string);
      string = NULL;
    }
    else if (option != NULL &&
             (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	         (PPD_KEYWORD | PPD_OPTION | PPD_STRING) &&
	     strcmp(keyword, option->keyword) == 0)
    {
      if (strcmp(keyword, "PageSize") == 0)
      {
       /*
        * Add a page size...
	*/

        if (ppdPageSize(ppd, name) == NULL)
	  ppd_add_size(ppd, name);
      }

     /*
      * Add the option choice...
      */

      choice = ppd_add_choice(option, name);

      if (mask & PPD_TEXT)
      {
        strncpy(choice->text, text, sizeof(choice->text));
        ppd_fix(choice->text);
      }
      else if (strcmp(name, "True") == 0)
        strcpy(choice->text, "Yes");
      else if (strcmp(name, "False") == 0)
        strcpy(choice->text, "No");
      else
        strncpy(choice->text, name, sizeof(choice->text));

      if (option->section == PPD_ORDER_JCL)
        ppd_decode(string);		/* Decode quoted string */

      choice->code = string;
      string       = NULL;		/* Don't add as an attribute below */
    }

   /*
    * Add remaining lines with keywords and string values as attributes...
    */

    if (string &&
        (mask & (PPD_KEYWORD | PPD_STRING)) == (PPD_KEYWORD | PPD_STRING))
      ppd_add_attr(ppd, keyword, name, text, string);
    else
    {
      ppd_free(string);
    }
  }

#ifdef LC_NUMERIC
  RestoreLocale(LC_NUMERIC, oldlocale);
#else
  RestoreLocale(LC_ALL, oldlocale);
#endif /* LC_NUMERIC */

#ifdef DEBUG
  if (!feof(fp))
    printf("Premature EOF at %lu...\n", (unsigned long)ftell(fp));
#endif /* DEBUG */

  if (ppd_status != PPD_OK)
  {
   /*
    * Had an error reading the PPD file, cannot continue!
    */

    ppdClose(ppd);

    return (NULL);
  }

#ifndef __APPLE__
 /*
  * Make sure that all PPD files with an InputSlot option have an
  * "auto" choice that maps to no specific tray or media type.
  */

  if ((option = ppdFindOption(ppd, "InputSlot")) != NULL)
  {
    for (i = 0; i < option->num_choices; i ++)
      if (option->choices[i].code == NULL || !option->choices[i].code[0] ||
          !strncasecmp(option->choices[i].choice, "Auto", 4))
	break;

    if (i >= option->num_choices)
    {
     /*
      * No "auto" input slot, add one...
      */

      choice = ppd_add_choice(option, "Auto");

      strncpy(choice->text, "Auto", sizeof(choice->text));
      choice->code = NULL;
    }
  }
#endif /* !__APPLE__ */

 /*
  * Set the option back-pointer for each choice...
  */

#ifndef __APPLE__
  qsort(ppd->groups, ppd->num_groups, sizeof(ppd_group_t),
        (int (*)(const void *, const void *))ppd_compare_groups);
#endif /* !__APPLE__ */

  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {
#ifndef __APPLE__
    qsort(group->options, group->num_options, sizeof(ppd_option_t),
          (int (*)(const void *, const void *))ppd_compare_options);
#endif /* !__APPLE__ */

    for (j = group->num_options, option = group->options;
         j > 0;
	 j --, option ++)
    {
      for (k = 0; k < option->num_choices; k ++)
        option->choices[k].option = (void *)option;
    }

#ifndef __APPLE__
    qsort(group->subgroups, group->num_subgroups, sizeof(ppd_group_t),
          (int (*)(const void *, const void *))ppd_compare_groups);
#endif /* !__APPLE__ */

    for (j = group->num_subgroups, subgroup = group->subgroups;
         j > 0;
	 j --, subgroup ++)
    {
#ifndef __APPLE__
      qsort(subgroup->options, subgroup->num_options, sizeof(ppd_option_t),
            (int (*)(const void *, const void *))ppd_compare_options);
#endif /* !__APPLE__ */

      for (k = group->num_options, option = group->options;
           k > 0;
	   k --, option ++)
      {
        for (m = 0; m < option->num_choices; m ++)
          option->choices[m].option = (void *)option;
      }
    }
  }

 /*
  * Sort the attributes...
  */

  if (ppd->num_attrs > 1)
    qsort(ppd->attrs, ppd->num_attrs, sizeof(ppd_attr_t *),
          (int (*)(const void *, const void *))_ppd_attr_compare);

 /*
  * Return the PPD file structure...
  */

  return (ppd);

 /*
  * Common exit point for errors to save code size...
  */

  error:

  ppd_free(string);

  ppdClose(ppd);

#ifdef LC_NUMERIC
  RestoreLocale(LC_NUMERIC, oldlocale);
#else
  RestoreLocale(LC_ALL, oldlocale);
#endif /* LC_NUMERIC */

  return (NULL);
}


/*
 * 'ppdOpenFd()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpenFd(int fd)			/* I - File to read from */
{
  FILE		*fp;			/* File pointer */
  ppd_file_t	*ppd;			/* PPD file record */


 /*
  * Set the line number to 0...
  */

  ppd_line = 0;

 /*
  * Range check input...
  */

  if (fd < 0)
  {
    ppd_status = PPD_NULL_FILE;

    return (NULL);
  }

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fdopen(fd, "r")) != NULL)
  {
    setbuf(fp, NULL);

    ppd = ppdOpen(fp);

    fclose(fp);
  }
  else
  {
    ppd_status = PPD_FILE_OPEN_ERROR;
    ppd        = NULL;
  }

  return (ppd);
}


/*
 * 'ppdOpenFile()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpenFile(const char *filename)	/* I - File to read from */
{
  FILE		*fp;			/* File pointer */
  ppd_file_t	*ppd;			/* PPD file record */


 /*
  * Set the line number to 0...
  */

  ppd_line = 0;

 /*
  * Range check input...
  */

  if (filename == NULL)
  {
    ppd_status = PPD_NULL_FILE;

    return (NULL);
  }

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fopen(filename, "r")) != NULL)
  {
    ppd = ppdOpen(fp);

    fclose(fp);
  }
  else
  {
    ppd_status = PPD_FILE_OPEN_ERROR;
    ppd        = NULL;
  }

  return (ppd);
}


/*
 * 'ppdSetConformance()' - Set the conformance level for PPD files.
 */

void
ppdSetConformance(ppd_conform_t c)	/* I - Conformance level */
{
  ppd_conform = c;
}


/*
 * 'ppd_add_attr()' - Add an attribute to the PPD data.
 */

static ppd_attr_t *			/* O - New attribute */
ppd_add_attr(ppd_file_t *ppd,		/* I - PPD file data */
             const char *name,		/* I - Attribute name */
             const char *spec,		/* I - Specifier string, if any */
	     const char *text,		/* I - Text string, if any */
	     const char *value)		/* I - Value of attribute */
{
  ppd_attr_t	**ptr,			/* New array */
		*temp;			/* New attribute */


 /*
  * Range check input...
  */

  if (ppd == NULL || name == NULL || spec == NULL)
    return (NULL);

 /*
  * Allocate memory for the new attribute...
  */

  if (ppd->num_attrs == 0)
    ptr = (ppd_attr_t**)malloc(sizeof(ppd_attr_t *));
  else
    ptr = (ppd_attr_t**)realloc(ppd->attrs, (ppd->num_attrs + 1) * sizeof(ppd_attr_t *));

  if (ptr == NULL)
    return (NULL);

  ppd->attrs = ptr;
  ptr += ppd->num_attrs;

  if ((temp = (ppd_attr_t*)calloc(1, sizeof(ppd_attr_t))) == NULL)
    return (NULL);

  *ptr = temp;

  ppd->num_attrs ++;

 /*
  * Copy data over...
  */

  strncpy(temp->name, name, sizeof(temp->name));
  strncpy(temp->spec, spec, sizeof(temp->spec));
  strncpy(temp->text, text, sizeof(temp->text));
  temp->value = (char *)value;

 /*
  * Return the attribute...
  */

  return (temp);
}


/*
 * 'ppd_add_choice()' - Add a choice to an option.
 */

static ppd_choice_t *			/* O - Named choice */
ppd_add_choice(ppd_option_t *option,	/* I - Option */
               const char   *name)	/* I - Name of choice */
{
  ppd_choice_t	*choice;		/* Choice */


  if (option->num_choices == 0)
    choice = (ppd_choice_t*)malloc(sizeof(ppd_choice_t));
  else
    choice = (ppd_choice_t*)realloc(option->choices,
	             sizeof(ppd_choice_t) * (option->num_choices + 1));

  if (choice == NULL)
    return (NULL);

  option->choices = choice;
  choice += option->num_choices;
  option->num_choices ++;

  memset(choice, 0, sizeof(ppd_choice_t));
  strncpy(choice->choice, name, sizeof(choice->choice));

  return (choice);
}


/*
 * 'ppd_add_size()' - Add a page size.
 */

static ppd_size_t *			/* O - Named size */
ppd_add_size(ppd_file_t *ppd,		/* I - PPD file */
             const char *name)		/* I - Name of size */
{
  ppd_size_t	*size;			/* Size */


  if (ppd->num_sizes == 0)
    size = (ppd_size_t*)malloc(sizeof(ppd_size_t));
  else
    size = (ppd_size_t*)realloc(ppd->sizes, sizeof(ppd_size_t) * (ppd->num_sizes + 1));

  if (size == NULL)
    return (NULL);

  ppd->sizes = size;
  size += ppd->num_sizes;
  ppd->num_sizes ++;

  memset(size, 0, sizeof(ppd_size_t));
  strncpy(size->name, name, sizeof(size->name));

  return (size);
}


#ifndef __APPLE__
/*
 * 'ppd_compare_groups()' - Compare two groups.
 */

static int				/* O - Result of comparison */
ppd_compare_groups(ppd_group_t *g0,	/* I - First group */
                   ppd_group_t *g1)	/* I - Second group */
{
  return (strcasecmp(g0->text, g1->text));
}


/*
 * 'ppd_compare_options()' - Compare two options.
 */

static int				/* O - Result of comparison */
ppd_compare_options(ppd_option_t *o0,	/* I - First option */
                    ppd_option_t *o1)	/* I - Second option */
{
  return (strcasecmp(o0->text, o1->text));
}
#endif /* !__APPLE__ */


/*
 * 'ppd_decode()' - Decode a string value...
 */

static int				/* O - Length of decoded string */
ppd_decode(char *string)		/* I - String to decode */
{
  char	*inptr,				/* Input pointer */
	*outptr;			/* Output pointer */


  inptr  = string;
  outptr = string;

  while (*inptr != '\0')
    if (*inptr == '<' && isxdigit(inptr[1] & 255))
    {
     /*
      * Convert hex to 8-bit values...
      */

      inptr ++;
      while (isxdigit(*inptr & 255))
      {
	if (isalpha((int)(*inptr)))
	  *outptr = (tolower(*inptr) - 'a' + 10) << 4;
	else
	  *outptr = (*inptr - '0') << 4;

	inptr ++;

        if (!isxdigit(*inptr & 255))
	  break;

	if (isalpha((int)(*inptr)))
	  *outptr |= tolower(*inptr) - 'a' + 10;
	else
	  *outptr |= *inptr - '0';

	inptr ++;
	outptr ++;
      }

      while (*inptr != '>' && *inptr != '\0')
	inptr ++;
      while (*inptr == '>')
	inptr ++;
    }
    else
      *outptr++ = *inptr++;

  *outptr = '\0';

  return (outptr - string);
}


#ifndef __APPLE__
/*
 * 'ppd_fix()' - Fix WinANSI characters in the range 0x80 to 0x9f to be
 *               valid ISO-8859-1 characters...
 */

static void
ppd_fix(char *string)			/* IO - String to fix */
{
  unsigned char		*p;		/* Pointer into string */
  static const unsigned char lut[32] =	/* Lookup table for characters */
			{
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  'l',
			  '`',
			  '\'',
			  '^',
			  '~',
			  0x20, /* bar */
			  0x20, /* circumflex */
			  0x20, /* dot */
			  0x20, /* double dot */
			  0x20,
			  0x20, /* circle */
			  0x20, /* ??? */
			  0x20,
			  '\"', /* should be right quotes */
			  0x20, /* ??? */
			  0x20  /* accent */
			};


  for (p = (unsigned char *)string; *p; p ++)
    if (*p >= 0x80 && *p < 0xa0)
      *p = lut[*p - 0x80];
}
#endif /* !__APPLE__ */


/*
 * 'ppd_free_group()' - Free a single UI group.
 */

static void
ppd_free_group(ppd_group_t *group)	/* I - Group to free */
{
  int		i;			/* Looping var */
  ppd_option_t	*option;		/* Current option */
  ppd_group_t	*subgroup;		/* Current sub-group */


  if (group->num_options > 0)
  {
    for (i = group->num_options, option = group->options;
         i > 0;
	 i --, option ++)
      ppd_free_option(option);

    ppd_free(group->options);
  }

  if (group->num_subgroups > 0)
  {
    for (i = group->num_subgroups, subgroup = group->subgroups;
         i > 0;
	 i --, subgroup ++)
      ppd_free_group(subgroup);

    ppd_free(group->subgroups);
  }
}


/*
 * 'ppd_free_option()' - Free a single option.
 */

static void
ppd_free_option(ppd_option_t *option)	/* I - Option to free */
{
  int		i;			/* Looping var */
  ppd_choice_t	*choice;		/* Current choice */


  if (option->num_choices > 0)
  {
    for (i = option->num_choices, choice = option->choices;
         i > 0;
         i --, choice ++)
    {
      ppd_free(choice->code);
    }

    ppd_free(option->choices);
  }
}


/*
 * 'ppd_get_group()' - Find or create the named group as needed.
 */

static ppd_group_t *			/* O - Named group */
ppd_get_group(ppd_file_t *ppd,		/* I - PPD file */
              const char *name,		/* I - Name of group */
	      const char *text)		/* I - Text for group */
{
  int		i;			/* Looping var */
  ppd_group_t	*group;			/* Group */

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    if (strcmp(group->name, name) == 0)
      break;

  if (i == 0)
  {
    if (ppd_conform == PPD_CONFORM_STRICT && strlen(text) >= sizeof(group->text))
    {
      ppd_status = PPD_ILLEGAL_TRANSLATION;

      return (NULL);
    }
	    
    if (ppd->num_groups == 0)
      group = (ppd_group_t*)malloc(sizeof(ppd_group_t));
    else
      group = (ppd_group_t*)realloc(ppd->groups,
	              (ppd->num_groups + 1) * sizeof(ppd_group_t));

    if (group == NULL)
    {
      ppd_status = PPD_ALLOC_ERROR;

      return (NULL);
    }

    ppd->groups = group;
    group += ppd->num_groups;
    ppd->num_groups ++;

    memset(group, 0, sizeof(ppd_group_t));
    strncpy(group->name, name, sizeof(group->name));
    strncpy(group->text, text, sizeof(group->text));
  }

  return (group);
}


/*
 * 'ppd_get_option()' - Find or create the named option as needed.
 */

static ppd_option_t *			/* O - Named option */
ppd_get_option(ppd_group_t *group,	/* I - Group */
               const char  *name)	/* I - Name of option */
{
  int		i;			/* Looping var */
  ppd_option_t	*option;		/* Option */

  for (i = group->num_options, option = group->options; i > 0; i --, option ++)
    if (strcmp(option->keyword, name) == 0)
      break;

  if (i == 0)
  {
    if (group->num_options == 0)
      option = (ppd_option_t*)malloc(sizeof(ppd_option_t));
    else
      option = (ppd_option_t*)realloc(group->options,
	               (group->num_options + 1) * sizeof(ppd_option_t));

    if (option == NULL)
      return (NULL);

    group->options = option;
    option += group->num_options;
    group->num_options ++;

    memset(option, 0, sizeof(ppd_option_t));
    strncpy(option->keyword, name, sizeof(option->keyword));
  }

  return (option);
}


/*
 * 'ppd_read()' - Read a line from a PPD file, skipping comment lines as
 *                necessary.
 */

static int				/* O - Bitmask of fields read */
ppd_read(FILE *fp,			/* I - File to read from */
         char *keyword,			/* O - Keyword from line */
	 char *option,			/* O - Option from line */
         char *text,			/* O - Human-readable text from line */
	 char **string,			/* O - Code/string data */
         int  ignoreblank)		/* I - Ignore blank lines? */
{
  int		ch,			/* Character from file */
		col,			/* Column in line */
		colon,			/* Colon seen? */
		endquote,		/* Waiting for an end quote */
		mask,			/* Mask to be returned */
		startline,		/* Start line */
		textlen;		/* Length of text */
  char		*keyptr,		/* Keyword pointer */
		*optptr,		/* Option pointer */
		*textptr,		/* Text pointer */
		*strptr,		/* Pointer into string */
		*lineptr,		/* Current position in line buffer */
		line[65536];		/* Line buffer (64k) */


 /*
  * Range check everything...
  */

  if (fp == NULL || keyword == NULL || option == NULL || text == NULL ||
      string == NULL)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  *string   = NULL;
  col       = 0;
  startline = ppd_line + 1;

  do
  {
   /*
    * Read the line...
    */

    lineptr  = line;
    endquote = 0;
    colon    = 0;

    while ((ch = getc(fp)) != EOF &&
           (lineptr - line) < (int)(sizeof(line) - 1))
    {
      if (ch == '\r' || ch == '\n')
      {
       /*
	* Line feed or carriage return...
	*/

        ppd_line ++;
	col = 0;

	if (ch == '\r')
	{
	 /*
          * Check for a trailing line feed...
	  */

	  if ((ch = getc(fp)) == EOF)
	    break;
	  if (ch != 0x0a)
	    ungetc(ch, fp);
	}

	if (lineptr == line && ignoreblank)
          continue;			/* Skip blank lines */

	ch = '\n';

	if (!endquote)			/* Continue for multi-line text */
          break;

	*lineptr++ = '\n';
      }
      else if (ch < ' ' && ch != '\t' && ppd_conform == PPD_CONFORM_STRICT)
      {
       /*
        * Other control characters...
	*/

        ppd_line   = startline;
        ppd_status = PPD_ILLEGAL_CHARACTER;

        return (0);
      }
      else if (ch != 0x1a)
      {
       /*
	* Any other character...
	*/

	*lineptr++ = ch;
	col ++;

	if (col > (PPD_MAX_LINE - 1))
	{
	 /*
          * Line is too long...
	  */

          ppd_line   = startline;
          ppd_status = PPD_LINE_TOO_LONG;

          return (0);
	}

	if (ch == ':' && strncmp(line, "*%", 2) != 0)
	  colon = 1;

	if (ch == '\"' && colon)
	  endquote = !endquote;
      }
    }

    if (endquote)
    {
     /*
      * Didn't finish this quoted string...
      */

      while ((ch = getc(fp)) != EOF)
        if (ch == '\"')
	  break;
	else if (ch == '\r' || ch == '\n')
	{
	  ppd_line ++;
	  col = 0;

	  if (ch == '\r')
	  {
	   /*
            * Check for a trailing line feed...
	    */

	    if ((ch = getc(fp)) == EOF)
	      break;
	    if (ch != 0x0a)
	      ungetc(ch, fp);
	  }

	  ch = '\n';
	}
	else if (ch < ' ' && ch != '\t' && ppd_conform == PPD_CONFORM_STRICT)
	{
	 /*
          * Other control characters...
	  */

          ppd_line   = startline;
          ppd_status = PPD_ILLEGAL_CHARACTER;

          return (0);
	}
	else if (ch != 0x1a)
	{
	  col ++;

	  if (col > (PPD_MAX_LINE - 1))
	  {
	   /*
            * Line is too long...
	    */

            ppd_line   = startline;
            ppd_status = PPD_LINE_TOO_LONG;

            return (0);
	  }
	}
    }

    if (ch != '\n')
    {
     /*
      * Didn't finish this line...
      */

      while ((ch = getc(fp)) != EOF)
	if (ch == '\r' || ch == '\n')
	{
	 /*
	  * Line feed or carriage return...
	  */

          ppd_line ++;
	  col = 0;

	  if (ch == '\r')
	  {
	   /*
            * Check for a trailing line feed...
	    */

	    if ((ch = getc(fp)) == EOF)
	      break;
	    if (ch != 0x0a)
	      ungetc(ch, fp);
	  }

	  break;
	}
	else if (ch < ' ' && ch != '\t' && ppd_conform == PPD_CONFORM_STRICT)
	{
	 /*
          * Other control characters...
	  */

          ppd_line   = startline;
          ppd_status = PPD_ILLEGAL_CHARACTER;

          return (0);
	}
	else if (ch != 0x1a)
	{
	  col ++;

	  if (col > (PPD_MAX_LINE - 1))
	  {
	   /*
            * Line is too long...
	    */

            ppd_line   = startline;
            ppd_status = PPD_LINE_TOO_LONG;

            return (0);
	  }
	}
    }

    if (lineptr > line && lineptr[-1] == '\n')
      lineptr --;

    *lineptr = '\0';

    if (ch == EOF && lineptr == line)
      return (0);

   /*
    * Now parse it...
    */

    mask    = 0;
    lineptr = line + 1;

    keyword[0] = '\0';
    option[0]  = '\0';
    text[0]    = '\0';
    *string    = NULL;

    if ((!line[0] ||			/* Blank line */
         strncmp(line, "*%", 2) == 0 ||	/* Comment line */
         strcmp(line, "*End") == 0) &&	/* End of multi-line string */
        ignoreblank)			/* Ignore these? */
    {
      startline = ppd_line + 1;
      continue;
    }

    if (strcmp(line, "*") == 0)		/* (Bad) comment line */
    {
      if (ppd_conform == PPD_CONFORM_RELAXED)
      {
	startline = ppd_line + 1;
	continue;
      }
      else
      {
        ppd_line   = startline;
        ppd_status = PPD_ILLEGAL_MAIN_KEYWORD;

        return (0);
      }
    }

    if (line[0] != '*')			/* All lines start with an asterisk */
    {
      if (ppd_conform == PPD_CONFORM_STRICT)
      {
        ppd_status = PPD_MISSING_ASTERISK;
        return (0);
      }

     /*
      * Allow lines consisting of just whitespace...
      */

      for (lineptr = line; *lineptr; lineptr ++)
        if (!isspace(*lineptr & 255))
	  break;

      if (*lineptr)
      {
        ppd_status = PPD_MISSING_ASTERISK;
        return (0);
      }
      else if (ignoreblank)
        continue;
      else
        return (0);
    }

   /*
    * Get a keyword...
    */

    keyptr = keyword;

    while (*lineptr != '\0' && *lineptr != ':' && !isspace(*lineptr & 255))
    {
      if (*lineptr <= ' ' || *lineptr > 126 || *lineptr == '/' ||
          (keyptr - keyword) >= (PPD_MAX_NAME - 1))
      {
        ppd_status = PPD_ILLEGAL_MAIN_KEYWORD;
	return (0);
      }

      *keyptr++ = *lineptr++;
    }

    *keyptr = '\0';

    if (strcmp(keyword, "End") == 0)
      continue;

    mask |= PPD_KEYWORD;

    if (isspace(*lineptr & 255))
    {
     /*
      * Get an option name...
      */

      while (isspace(*lineptr & 255))
        lineptr ++;

      optptr = option;

      while (*lineptr != '\0' && !isspace(*lineptr & 255) && *lineptr != ':' &&
             *lineptr != '/')
      {
	if (*lineptr <= ' ' || *lineptr > 126 ||
	    (optptr - option) >= (PPD_MAX_NAME - 1))
        {
          ppd_status = PPD_ILLEGAL_OPTION_KEYWORD;
	  return (0);
	}

        *optptr++ = *lineptr++;
      }

      *optptr = '\0';

      if (isspace(*lineptr & 255) && ppd_conform == PPD_CONFORM_STRICT)
      {
        ppd_status = PPD_ILLEGAL_WHITESPACE;
	return (0);
      }

      while (isspace(*lineptr & 255))
	lineptr ++;

      mask |= PPD_OPTION;

      if (*lineptr == '/')
      {
       /*
        * Get human-readable text...
	*/

        lineptr ++;
	
	textptr = text;

	while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':')
	{
	  if (((unsigned char)*lineptr < ' ' && *lineptr != '\t') ||
	      (textptr - text) >= (PPD_MAX_LINE - 1))
	  {
	    ppd_status = PPD_ILLEGAL_TRANSLATION;
	    return (0);
	  }

	  *textptr++ = *lineptr++;
        }

	*textptr = '\0';
	textlen  = ppd_decode(text);

	if (textlen > PPD_MAX_TEXT && ppd_conform == PPD_CONFORM_STRICT)
	{
	  ppd_status = PPD_ILLEGAL_TRANSLATION;
	  return (0);
	}
	    
	mask |= PPD_TEXT;
      }

    }

    if (isspace(*lineptr & 255) && ppd_conform == PPD_CONFORM_STRICT)
    {
      ppd_status = PPD_ILLEGAL_WHITESPACE;
      return (0);
    }

    while (isspace(*lineptr & 255))
      lineptr ++;

    if (*lineptr == ':')
    {
     /*
      * Get string after triming leading and trailing whitespace...
      */

      lineptr ++;
      while (isspace(*lineptr & 255))
        lineptr ++;

      strptr = lineptr + strlen(lineptr) - 1;
      while (strptr >= lineptr && isspace(*strptr & 255))
        *strptr-- = '\0';

      if (*strptr == '\"')
      {
       /*
        * Quoted string by itself...
	*/

	*string = (char*)malloc(strlen(lineptr) + 1);

	strptr = *string;

	for (; *lineptr != '\0'; lineptr ++)
	  if (*lineptr != '\"')
	    *strptr++ = *lineptr;

	*strptr = '\0';
      }
      else
        *string = strdup(lineptr);


      mask |= PPD_STRING;
    }
  }
  while (mask == 0);

  return (mask);
}


/*
 * 'ppd_sort()' - Sort options by ordering numbers...
 */

static int			/* O - -1 if c1 < c2, 0 if equal, 1 otherwise */
ppd_sort(ppd_choice_t **c1,	/* I - First choice */
         ppd_choice_t **c2)	/* I - Second choice */
{
  if (((ppd_option_t *)(*c1)->option)->order < ((ppd_option_t *)(*c2)->option)->order)
    return (-1);
  else if (((ppd_option_t *)(*c1)->option)->order > ((ppd_option_t *)(*c2)->option)->order)
    return (1);
  else
    return (0);
}

/*
 * 'ppdCollect()' - Collect all marked options that reside in the specified
 *                  section.
 */

int					/* O - Number of options marked */
ppdCollect(ppd_file_t    *ppd,		/* I - PPD file data */
           ppd_section_t section,	/* I - Section to collect */
           ppd_choice_t  ***choices)	/* O - Pointers to choices */
{
  int		i, j, k, m;		/* Looping vars */
  ppd_group_t	*g,			/* Current group */
		*sg;			/* Current sub-group */
  ppd_option_t	*o;			/* Current option */
  ppd_choice_t	*c;			/* Current choice */
  int		count;			/* Number of choices collected */
  ppd_choice_t	**collect;		/* Collected choices */


  if (ppd == NULL)
    return (0);

 /*
  * Allocate memory for up to 1000 selected choices...
  */

  count   = 0;
  collect = (ppd_choice_t **)calloc(sizeof(ppd_choice_t *), 1000);

 /*
  * Loop through all options and add choices as needed...
  */

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o = g->options; j > 0; j --, o ++)
      if (o->section == section)
	for (k = o->num_choices, c = o->choices; k > 0; k --, c ++)
	  if (c->marked && count < 1000)
	  {
            collect[count] = c;
	    count ++;
	  }

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o = sg->options; k > 0; k --, o ++)
	if (o->section == section)
	  for (m = o->num_choices, c = o->choices; m > 0; m --, c ++)
	    if (c->marked && count < 1000)
	    {
              collect[count] = c;
	      count ++;
	    }
  }

 /*
  * If we have more than 1 marked choice, sort them...
  */

  if (count > 1)
    qsort(collect, count, sizeof(ppd_choice_t *),
          (int (*)(const void *, const void *))ppd_sort);

 /*
  * Return the array and number of choices; if 0, free the array since
  * it isn't needed.
  */

  if (count > 0)
  {
    *choices = collect;
    return (count);
  }
  else
  {
    *choices = NULL;
    free(collect);
    return (0);
  }
}

/*
 * 'ppdFindMarkedChoice()' - Return the marked choice for the specified option.
 */

ppd_choice_t *				/* O - Pointer to choice or NULL */
ppdFindMarkedChoice(ppd_file_t *ppd,	/* I - PPD file */
                    const char *option)	/* I - Keyword/option name */
{
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Pointer to option */
  ppd_choice_t	*c;		/* Pointer to choice */


  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (NULL);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (c->marked)
      return (c);

  return (NULL);
}

/*
 * End of "$Id: ppd.c,v 1.6 2007-11-23 07:24:52 jpzhang Exp $".
 */
