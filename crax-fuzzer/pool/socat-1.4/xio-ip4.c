/* $Id: xio-ip4.c,v 1.7 2004/06/20 21:34:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for IP4 related functions */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ip4.h"

#if WITH_IP4

/* check if peer address is within permitted range.
   return >= 0 if so. */
int checkrange(struct sockaddr *pa, byte4_t *netaddr_in, byte4_t *netmask_in) {
   /* client address restriction */
   switch (pa->sa_family) {
      char addrbuf[256], maskbuf[256];
      char peername[256];
#if WITH_IP4	/* should be extended for IP6 */
   case AF_INET:
      Debug2("permitted client subnet: %s:%s",
	     inet4addr_info(*netaddr_in, addrbuf, sizeof(addrbuf)),
	     inet4addr_info(*netmask_in, maskbuf, sizeof(maskbuf)));
      Debug1("client address is 0x%08x",
	     ntohl(((struct sockaddr_in *)pa)->sin_addr.s_addr));
      Debug1("masked address is 0x%08x",
	     ntohl(((struct sockaddr_in *)pa)->sin_addr.s_addr) &
	     *netmask_in);
      if ((ntohl(((struct sockaddr_in *)pa)->sin_addr.s_addr) & *netmask_in)
	  != *netaddr_in) {
	 Debug1("client address %s is not permitted",
		sockaddr_info(pa, peername, sizeof(peername)));
	 return -1;
      }
#endif /* WITH_IP4 */
   }
   return 0;
}
#endif /* WITH_IP4 */
