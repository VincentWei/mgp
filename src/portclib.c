
#include "mgpconfig.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "portclib.h"

#ifdef NOUNIX
int strncasecmp(const char * s1, const char * s2, size_t n)
{
    int c1, c2;

    while (n && *s1 && *s2)
    {
        n -= 1;
        c1 = isupper ((unsigned char)*s1) ? tolower ((unsigned char)*s1) : *s1;
        c2 = isupper ((unsigned char)*s2) ? tolower ((unsigned char)*s2) : *s2;
        if (c1 != c2)
            return (c1 - c2);
        s1++; s2++;
    }

    if (n)
        return (((int) (unsigned char) *s1) - ((int) (unsigned char) *s2));
    else
        return 0;
}

#endif
