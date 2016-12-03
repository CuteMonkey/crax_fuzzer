/* $Id: utils.c,v 1.16 2004/06/20 21:35:34 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* useful additions to C library */

#include "config.h"

#include "sysincludes.h"

#include "compat.h"	/* socklen_t */
#include "mytypes.h"
#include "sycls.h"
#include "utils.h"


#if !HAVE_MEMRCHR
/* GNU extension, available since glibc 2.1.91 */
void *memrchr(const void *s, int c, size_t n) {
   const unsigned char *t = ((unsigned char *)s)+n;
   while (--t >= (unsigned char *)s) {
      if (*t == c)  break;
   }
   if (t < (unsigned char *)s)
      return NULL;
   return (void *)t;
}
#endif /* !HAVE_MEMRCHR */

void *memdup(const void *src, size_t n) {
   void *dest;

   if ((dest = Malloc(n)) == NULL) {
      return NULL;
   }

   memcpy(dest, src, n);
   return dest;
}

/* search the keyword-table for a match of the leading part of name. */
/* returns the pointer to the matching field of the keyword or NULL if no
   keyword was found. */
const struct wordent *keyw(const struct wordent *keywds, const char *name, unsigned int nkeys) {
   unsigned int lower, upper, mid;
   int r;

   lower = 0;
   upper = nkeys;

   while (upper - lower > 1)
   {
      mid = (upper + lower) >> 1;
      if (!(r = strcasecmp(keywds[mid].name, name)))
      {
	 return &keywds[mid];
      }
      if (r < 0)
	 lower = mid;
      else
	 upper = mid;
   }
   if (nkeys > 0 && !(strcasecmp(keywds[lower].name, name)))
   {
      return &keywds[lower];
   }
   return NULL;
}

/* Linux: setenv(), AIX: putenv() */
#if !HAVE_SETENV
int setenv(const char *name, const char *value, int overwrite) {
   int result;
   char *env;
   if (!overwrite) {
      if (getenv(name))  return 0;	/* already exists */
   }
   if ((env = Malloc(strlen(name)+strlen(value)+2)) != NULL) {
      return -1;
   }
   sprintf(env, "%s=%s", name, value);
   if ((result = putenv(env)) != 0) {	/* AIX docu says "... nonzero ..." */
      free(env);
      result = -1;
   }
   /* linux "man putenv" says: ...this string becomes part of the environment*/
   return result;
}
#endif /* !HAVE_SETENV */
