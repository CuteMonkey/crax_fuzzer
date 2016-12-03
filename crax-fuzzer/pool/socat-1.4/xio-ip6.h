/* $Id: xio-ip6.h,v 1.6 2001/11/04 17:19:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ip6_h_included
#define __xio_ip6_h_included 1

extern int parseip6addr(char *addr, struct sockaddr_in6 *out, const char *protoname, int withaddr, int withport);

#endif /* !defined(__xio_ip6_h_included) */
