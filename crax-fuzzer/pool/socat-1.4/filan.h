/* $Id: filan.h,v 1.5 2003/06/21 17:08:13 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */

/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __filan_h_included
#define __filan_h_included 1

struct sockaddr;	/* prevent gcc from spitting silly warning */
struct sockaddr_un;	/* prevent gcc from spitting silly warning */
struct sockaddr_in;	/* prevent gcc from spitting silly warning */
struct sockaddr_in6;	/* prevent gcc from spitting silly warning */

extern int filan(int fd, FILE *outfile);
extern int cdevan(int fd, FILE *outfile);

#if WITH_SOCKET
extern int isasocket(int fd);
extern int sockan(int fd, FILE *outfile);
extern int ipan(int fd, FILE *outfile);
#endif /* WITH_SOCKET */

extern int fdname(const char *file, int fd, FILE *outfile);

#endif /* !defined(__filan_h_included) */
