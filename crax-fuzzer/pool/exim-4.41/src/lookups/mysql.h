/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* Header for the mysql lookup functions */

extern void *mysql_open(uschar *, uschar **);
extern int   mysql_find(void *, uschar *, uschar *, int, uschar **, uschar **,
               BOOL *);
extern void  mysql_tidy(void);
extern uschar *mysql_quote(uschar *, uschar *);

/* End of lookups/mysql.h */
