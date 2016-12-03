/* $Id: xio-ascii.h,v 1.3 2004/06/20 21:33:57 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ascii_h_included
#define __xio_ascii_h_included 1

extern char *
   xiob64encodeline(const char *data,		/* input data */
		    size_t bytes,		/* length of input data, >=0 */
		    char *coded	/* output buffer, must be long enough */
		    );
extern char *xiosanitize(const char *data,	/* input data */
		  size_t bytes,			/* length of input data, >=0 */
		  char *coded	/* output buffer, must be long enough */
			 );

#endif /* !defined(__xio_ascii_h_included) */
