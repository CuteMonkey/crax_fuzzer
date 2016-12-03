/* $Id: xio-listen.h,v 1.7 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_listen_h_included
#define __xio_listen_h_included 1

extern const struct optdesc opt_backlog;
extern const struct optdesc opt_fork;
extern const struct optdesc opt_range;
extern const struct optdesc opt_tcpwrappers;

int _xioopen_listen(struct single *fd, struct sockaddr *us, socklen_t uslen,
		 struct opt *opts, int pf, int socktype, int proto, int level);

#endif /* !defined(__xio_listen_h_included) */
