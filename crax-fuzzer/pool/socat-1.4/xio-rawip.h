/* $Id: xio-rawip.h,v 1.6 2001/11/04 17:19:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_rawip_h_included
#define __xio_rawip_h_included 1

extern const struct addrdesc addr_rawip4;
extern const struct addrdesc addr_rawip6;

extern int xioopen_rawip(char *a1, int rw, xiofile_t *fd, unsigned groups, int pf, int dummy2, void *dummyp1);

#endif /* !defined(__xio_rawip_h_included) */
