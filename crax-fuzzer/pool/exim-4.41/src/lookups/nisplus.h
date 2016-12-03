/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* Header for the nisplus lookup */

extern void   *nisplus_open(uschar *, uschar **);
extern int     nisplus_find(void *, uschar *, uschar *, int, uschar **,
                 uschar **, BOOL *);
extern uschar *nisplus_quote(uschar *, uschar *);

/* End of lookups/nisplus.h */
