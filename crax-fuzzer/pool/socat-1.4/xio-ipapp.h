/* $Id: xio-ipapp.h,v 1.9 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ipapp_h_included
#define __xio_ipapp_h_included 1


/* when selecting a low port, this is the lowest possible */
#define XIO_IPPORT_LOWER 640


extern const struct optdesc opt_sourceport;
/*extern const struct optdesc opt_port;*/
extern const struct optdesc opt_lowport;

extern int xioopen_ip4app_connect(char *a1, int rw, xiofile_t *fd,
			 unsigned groups, int socktype,
			 int ipproto, void *protname);
extern int
   _xioopen_ip4app_prepare(struct opt *opts, struct opt **opts0,
			   const char *hostname,
			   const char *portname, const char *protname,
			   struct sockaddr_in *them,
			   struct sockaddr_in *us,
			   bool *needbind, bool *lowport,
			   int *socktype);
extern int _xioopen_ip4app_connect(const char *hostname, const char *portname,
				   struct single *xfd,
				   int socktype, int ipproto, void *protname,
				   struct opt *opts);
extern int xioopen_ip4app_listen(char *a1, int rw, xiofile_t *fd,
			 unsigned groups, int socktype,
			 int ipproto, void *protname);
extern int _xioopen_ip4app_listen_prepare(struct opt *opts, struct opt **opts0,
				   const char *portname, const char *protname,
				   struct sockaddr_in *us,
					  int *socktype);
extern int xioopen_ip6app_connect(char *a1, int rw, xiofile_t *fd,
			 unsigned groups, int socktype, int ipproto,
			 void *protname);
extern int xioGethostbyname(const char *hostname, struct hostent **host, int level);

#endif /* !defined(__xio_ipapp_h_included) */
