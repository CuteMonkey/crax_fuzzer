/* $Id: mytypes.h,v 1.3 2001/06/30 14:02:39 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __mytypes_h_included
#define __mytypes_h_included 1

/* some types and macros I miss in C89 */

typedef enum { false, true } bool;

typedef unsigned char byte_t;

/* this should work for 16, 32, and 64 bit architectures */
typedef unsigned short byte2_t;

typedef unsigned long byte4_t;

#define Min(x,y) ((x)<=(y)?(x):(y))
#define Max(x,y) ((x)>=(y)?(x):(y))

#define SOCKADDR_MAX UNIX_PATH_MAX

#endif /* __mytypes_h_included */
