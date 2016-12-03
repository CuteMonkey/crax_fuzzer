/* $Id: xio-rawip.c,v 1.14 2004/06/20 21:28:12 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of raw IP type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-ip.h"
#include "xio-ip6.h"
#include "xio-rawip.h"


#if (WITH_IP4 || WITH_IP6) && WITH_RAWIP

#if WITH_IP4
const struct addrdesc addr_rawip4 = { "ip4",     3, xioopen_rawip, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4, PF_INET, 0, NULL HELP(":<host>:<protocol>") };
#endif

#if WITH_IP6
const struct addrdesc addr_rawip6 = { "ip6",     3, xioopen_rawip, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6, PF_INET6, 0, NULL HELP(":<host>:<protocol>") };
#endif

/* we expect the form: host:protocol */
/* struct sockaddr_in sa;*/
/* socklen_t salen;*/
int xioopen_rawip(char *a1, int rw, xiofile_t *fd, unsigned groups, int pf, int dummy2, void *dummyp1) {
   char *a2, *a3;
   struct opt *opts = NULL;
   union sockaddr_union us;
   socklen_t uslen;
   int feats = 1;
   int socktype = SOCK_RAW;
   int proto;
   int af = pf;
   char infobuff[256];

   fd->stream.salen = socket_init(af, &fd->stream.peersa);
   uslen = socket_init(af, &us);
   fd->stream.howtoend = END_SHUTDOWN;

   if (a3 = strchr(a1, ','))  *a3++ = '\0';	/* start of options */
   if (a2 = strrchr(a1, xioopts.ip4portsep)) {
      *a2++ = '\0';
   } else {
      Error1("xioopen_rawip(\"%s\",,): missing protocol specification", a1);
      return STAT_NORETRY;
   }
   switch (af) {
#if WITH_IP4
   case AF_INET:
      parseip4addr(a1, &fd->stream.peersa.ip4, NULL, 1, -1);
      break;
#endif
#if WITH_IP6
   case AF_INET6:
      parseip6addr(a1, &fd->stream.peersa.ip6, NULL, 1, -1);
      break;
#endif
   }
   /*0 fd->stream.salen = sizeof(struct sockaddr_in);*/
   fd->stream.dtype = DATA_RECV;
   if (parseopts(a3, groups, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   if ((proto = strtoul(a2, &a2, 0)) >= 256) {
      Error3("xioopen_rawip(\"%s:%s\",,): protocol number exceeds 255 (%u)",
	     a1, a2, proto);
      return STAT_NORETRY;
   }
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   fd->stream.fd = Socket(pf, socktype, proto);
   if (fd->stream.fd < 0) {
      Error4("socket(%d, %d, %d): %s", pf, socktype, proto, strerror(errno));
      return STAT_RETRYLATER;
   }
   /*0 Info4("socket(%d, %d, %d) -> %d", pf, socktype, proto, fd->stream.fd);*/
   applyopts(fd->stream.fd, opts, PH_PASTSOCKET);
   applyopts(fd->stream.fd, opts, PH_FD);

   if (retropt_bind(opts, af, NULL, &us.soa, feats) >= 0) {
      if (Bind(fd->stream.fd, &us.soa, uslen) < 0) {
	 char infobuff[256];
	 Error4("bind(%d, {%s}, "F_Zu"): %s",
		fd->stream.fd,
		sockaddr_info(&us.soa,
			      infobuff, sizeof(infobuff)),
		uslen, strerror(errno));
      }
   }

   Notice2("raw IP, protocol %d, with peer %s",
	   proto,
	   sockaddr_info(&fd->stream.peersa.soa, infobuff, sizeof(infobuff)));
   applyopts_cloexec(fd->stream.fd, opts);
   return _xio_openlate(&fd->stream, opts);
}

#endif /* (WITH_IP4 || WITH_IP6) && WITH_RAWIP */
