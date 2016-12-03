/* $Id: utils.h,v 1.6 2003/11/18 22:11:15 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __utils_h_included
#define __utils_h_included 1

/* a generic name table entry */
struct wordent {
   const char *name;
   void *desc;
} ;

#if !HAVE_MEMRCHR
extern void *memrchr(const void *s, int c, size_t n);
#endif
extern void *memdup(const void *src, size_t n);
#if !HAVE_SETENV
extern int setenv(const char *name, const char *value, int overwrite);
#endif /* !HAVE_SETENV */

extern const struct wordent *keyw(const struct wordent *keywds, const char *name, unsigned int nkeys);

#endif /* !defined(__utils_h_included) */

