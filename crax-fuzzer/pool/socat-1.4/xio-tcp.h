/* $Id: xio-tcp.h,v 1.7 2002/05/19 08:04:26 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2002 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_tcp_h_included
#define __xio_tcp_h_included 1

extern const struct addrdesc addr_tcp4_connect;
extern const struct addrdesc addr_tcp4_listen;
extern const struct addrdesc addr_tcp6_connect;
extern const struct addrdesc addr_tcp6_listen;

extern const struct optdesc opt_tcp_nodelay;
extern const struct optdesc opt_tcp_maxseg;
extern const struct optdesc opt_tcp_maxseg_late;
extern const struct optdesc opt_tcp_cork;
extern const struct optdesc opt_tcp_stdurg;
extern const struct optdesc opt_tcp_rfc1323;
extern const struct optdesc opt_tcp_keepidle;
extern const struct optdesc opt_tcp_keepintvl;
extern const struct optdesc opt_tcp_keepcnt;
extern const struct optdesc opt_tcp_syncnt;
extern const struct optdesc opt_tcp_linger2;
extern const struct optdesc opt_tcp_defer_accept;
extern const struct optdesc opt_tcp_window_clamp;
extern const struct optdesc opt_tcp_info;
extern const struct optdesc opt_tcp_quickack;

extern int xioopen_tcp6_listen(char *a1, int rw, xiofile_t *fd,
			     unsigned groups, int dummy1, int dummy2,
			     void *dummyp1);

#endif /* !defined(__xio_tcp_h_included) */
