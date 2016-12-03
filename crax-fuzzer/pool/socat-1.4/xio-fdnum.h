/* $Id: xio-fdnum.h,v 1.4 2001/11/04 17:19:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_fdnum_h_included
#define __xio_fdnum_h_included 1

extern const struct addrdesc addr_fd;

extern int xioopen_fd(char *arg, int rw, xiofile_t *fd, unsigned groups, int numfd, int dummy2, void *dummyp1);

#endif /* !defined(__xio_fdnum_h_included) */
