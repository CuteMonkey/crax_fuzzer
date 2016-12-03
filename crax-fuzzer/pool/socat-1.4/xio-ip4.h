/* $Id: xio-ip4.h,v 1.5 2001/11/04 17:19:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ip4_h_included
#define __xio_ip4_h_included 1

extern int checkrange(struct sockaddr *pa, byte4_t *netaddr_in, byte4_t *netmask_in);

#endif /* !defined(__xio_ip4_h_included) */
