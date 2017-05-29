/**
 * file nparser.h
 * author Zhang Jipeng <jpzhang@minigui.org>
 * date 2005/12/22
 *
 * This file include page range parser. 
 */

/*
 * $Id: nparser.h,v 1.2 2006-01-10 11:11:31 jpzhang Exp $
 *          Printing support system for linux is a component of MiniGUI.
 *
 *          MiniGUI is a compact cross-platform Graphics User Interface 
 *         (GUI) support system for real-time embedded systems.
 *                  
 *             Copyright (C) 2002-2005 Feynman Software.
 *             Copyright (C) 1998-2002 Wei Yongming.
 */

#ifndef _NPARSER_H_
#define _NPARSER_H_

#define RANGE_SINGLE  0
#define RANGE_START   1
#define RANGE_END     2

struct list_table {
    unsigned int type;
    unsigned int val;
    struct list_table * next;
};

int _create_range_table (const char * str, struct list_table ** table);

void _destroy_range_table (struct list_table ** table);

#endif /* _NPARSER_H_ */

