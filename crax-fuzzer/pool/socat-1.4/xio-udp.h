/* $Id: xio-udp.h,v 1.6 2001/11/28 10:02:17 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_udp_h_included
#define __xio_udp_h_included 1

extern const struct addrdesc addr_udp4_connect;
extern const struct addrdesc addr_udp4_listen;
extern const struct addrdesc addr_udp6_connect;
extern const struct addrdesc addr_udp6_listen;

extern int xioopen_ipdgram_listen(char *a1, int rw, xiofile_t *fd,
			  unsigned groups, int af, int ipproto,
			  void *protname);

#endif /* !defined(__xio_udp_h_included) */
