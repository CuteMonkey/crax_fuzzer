/* $Id: xioexit.c,v 1.9 2003/05/21 05:16:38 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for the extended exit function */

#include "xiosysincludes.h"
#include "xio.h"


/* this function closes socket1 and socket2 on exit, if they are still open.
   It must be registered with atexit(). */ 
void xioexit(void) {
   if (sock1 != NULL && sock1->tag != XIO_TAG_INVALID) {
      xioclose(sock1);
   }
   if (sock2 != NULL && sock2->tag != XIO_TAG_INVALID) {
      xioclose(sock2);
   }
}
