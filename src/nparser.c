/*
** $Id: nparser.c,v 1.11 2007-11-23 07:24:52 jpzhang Exp $
**
** Author:  Zhang Jipeng <jpzhang@minigui.org>
**
** Copyright (C) 2006 Feynman Software.
** Copyright (C) 1999 ~ 2002 Wei Yongming.
**
** Create date: 2005/11/10
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mgpconfig.h"
#include "nparser.h"

static struct list_table ** table_end = NULL; 

static void list_add_items (struct list_table ** table, unsigned int val, unsigned int type)
{
    table = table_end;

    *table = (struct list_table*)calloc(1, sizeof(struct list_table));
    
    if (*table)
    {
        (*table)->type = type;
        (*table)->val = val;
        (*table)->next = NULL;
    }

    table_end = &(*table)->next;
}


static void list_clear_items (struct list_table ** table)
{
    struct list_table ** ptable = NULL;
    while (*table)
    {
        ptable = table;
        table = &(*table)->next;

        free (*ptable);
        *ptable = NULL;
    }
    table_end = NULL;
}


int _create_range_table (const char * str, struct list_table ** table)
{
    int i , s = -1 , e = -1;
    char ch = 0;
    int numw = 0;
    int stat = 0;
    int nr_pno = 0; 
    char tbuf[16] = {0};
    int len = 0;

    if ((NULL == str) || (NULL == table))
        return -3;  /* error!*/

    len = strlen(str)+1;
    table_end = table;

    for (i = 0; i < len; i++)
    {
        ch = *(str++);

        switch (ch)
        {
            case 'a':
                return  -1;  /* Print all pages*/
            case 'c':
                return  0;  /* Current page*/
            case ' ':
                continue; /* Ignore*/
            case '-':
                stat = 2;
                break;
            case '\0':
            case ',':
                stat = 0;        
                break;
            default:
                if (!isdigit((int)ch)) {
                    stat = 0;
                } else {
                    stat = 1;
                }
                break;
        }
    
        switch (stat)
        {
            case 0:
                if (tbuf[0] != 0)
                {
                    if (s >= 0) {
                        e = atoi (tbuf);

                        if (s <= e) {
                            list_add_items (table, s, RANGE_START);
                            list_add_items (table, e, RANGE_END);
                            
                            nr_pno += (e-s)+1;

                            memset (tbuf , 0 , 16);
                            numw = 0;
                            s = e = -1;
                        } else {

                            memset (tbuf , 0 , 16);
                            numw = 0;
                            s = e = -1;
                            
                            return -3; /* Invalid area*/
                        }
                            
                    } else {
                        list_add_items (table, atoi(tbuf), RANGE_SINGLE);
                        nr_pno++;
                        memset (tbuf , 0 , 16);
                        numw = 0;
                        s = e = -1;
                    }
                }
                else
                {
                    memset (tbuf , 0 , 16);
                    numw = 0;
                    s = e = -1;
                }
                break;
            case 1:
                if (numw > 9)
                    return -4; /* Invalid input*/

                tbuf[numw++] = ch; 
                break; 
            case 2:
                if (tbuf[0] != 0)
                {
                    s = atoi(tbuf);
                    memset (tbuf , 0 , 16);
                    numw = 0;
                }
                break;
        }

    }

    return nr_pno;
}

void _destroy_range_table (struct list_table ** table)
{
    if (NULL == table)
        return;

    list_clear_items (table);
}


