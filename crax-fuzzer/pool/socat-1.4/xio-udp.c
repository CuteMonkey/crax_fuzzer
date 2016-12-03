/* $Id: xio-udp.c,v 1.17 2004/06/20 21:22:31 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for handling UDP addresses */

#include "xiosysincludes.h"
#if WITH_LIBWRAP
#include "tcpd.h"
#endif
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-ip4.h"
#include "xio-ip6.h"
#include "xio-ip.h"
#include "xio-ipapp.h"
#include "xio-udp.h"

#if WITH_UDP && (WITH_IP4 || WITH_IP6)


#if WITH_IP4
const struct addrdesc addr_udp4_connect = { "udp4",    3, xioopen_ip4app_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP, SOCK_DGRAM, IPPROTO_UDP, "udp" HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc addr_udp4_listen  = { "udp4-listen", 3, xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET, IPPROTO_UDP, "udp" HELP(":<port>") };
#endif /* WITH_LISTEN */
#endif /* WITH_IP4 */


#if WITH_IP6
const struct addrdesc addr_udp6_connect = { "udp6",    3, xioopen_ip6app_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP, SOCK_DGRAM, IPPROTO_UDP, "udp" HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc addr_udp6_listen  = { "udp6-listen", 3, xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET6, IPPROTO_UDP, "udp" HELP(":<port>") };
#endif /* WITH_LISTEN */
#endif /* WITH_IP6 */


/* we expect the form: port */
int xioopen_ipdgram_listen(char *a1, int rw, xiofile_t *fd,
			  unsigned groups, int pf, int ipproto,
			  void *protname) {
   char *a2;
   struct opt *opts = NULL;
   union sockaddr_union us;
   union sockaddr_union themunion;
   union sockaddr_union *them = &themunion;
   struct hostent *usaddr;
   char *bindname;
   int socktype = SOCK_DGRAM;
   fd_set in, out, expt;
   bool dofork = false;
   pid_t pid;
   char *rangename;	/* allowed client address range - text form */
   unsigned short sourceport;
   bool lowport = false;
   bool dosourceport = false;
#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   bool dolibwrap = false;
   char *libwrapname = NULL;
#endif
   byte4_t netaddr_in = 0;
   byte4_t netmask_in = 0;
   char infobuff[256];
   unsigned char buff1[1];
   socklen_t uslen;
   socklen_t themlen;
   int result;

   if (a2 = strchr(a1, ','))  *a2++ = '\0';
   if (parseopts(a2, groups, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   uslen = socket_init(pf, &us);
   if (false) {
      ;
#if WITH_IP4
   } else if (pf == PF_INET) {
      us.ip4.sin_port = parseport(a1, (char *)protname);
#endif
#if WITH_IP6
   } else if (pf == PF_INET6) {
      us.ip6.sin6_port = parseport(a1, (char *)protname);
#endif
   } else {
      Error1("xioopen_ipdgram_listen(): unknown address family %d", pf);
   }

   retropt_bool(opts, OPT_FORK, &dofork);

   if (retropt_string(opts, OPT_BIND, &bindname) >= 0) {
      usaddr = Gethostbyname(bindname);
      if (usaddr == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", bindname,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	 return STAT_RETRYLATER;
      }
      /*! here we should see what address type we got from gethostbyname() */
      switch (pf) {
#if WITH_IP4
      case PF_INET:  memcpy(&us.ip4.sin_addr, usaddr->h_addr_list[0],  4);
	 break;
#endif
#if WITH_IP6
      case PF_INET6: memcpy(&us.ip6.sin6_addr, usaddr->h_addr_list[0], 16);
	 break;
#endif
      }
   }

   retropt_int(opts, OPT_SO_TYPE, &socktype);

#if WITH_IP4 /*|| WITH_IP6*/
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (pf == PF_INET) {
	 parserange(rangename, pf, &netaddr_in, &netmask_in);
      } else {
	 Error1("range option not supported with address family %d", pf);
	 return STAT_NORETRY;
      }
   }
#endif
#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   if (retropt_string(opts, OPT_TCPWRAPPERS, &libwrapname) >= 0) {
      dolibwrap = true;
      if (libwrapname == NULL) {
	 libwrapname = (char *)diag_get_string('p');
      }
   }
#endif /* && (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */
#if WITH_TCP || WITH_UDP
   if (retropt_ushort(opts, OPT_SOURCEPORT, &sourceport) >= 0) {
      dosourceport = true;
   }
   retropt_bool(opts, OPT_LOWPORT, &lowport);
#endif /* WITH_TCP || WITH_UDP */

   while (true) {	/* we loop with fork or prohibited packets */
      /* now wait for some packet on this datagram socket, get its sender
	 address, connect there, and return */
      int one = 1;
      char infobuff[256];
      /*0 char _peername[SOCKADDR_MAX];*/
      union sockaddr_union _sockname;
      /*0 struct sockaddr *pa = (struct sockaddr *)_peername;*/	/* peer address */
      union sockaddr_union *la = &_sockname;	/* local address */

      if ((fd->stream.fd = Socket(pf, socktype, ipproto)) < 0) {
	 Error4("socket(%d, %d, %d): %s", pf, socktype, ipproto, strerror(errno));
	 return STAT_RETRYLATER;
      }
      /*0 Info4("socket(%d, %d, %d) -> %d", pf, socktype, ipproto, fd->stream.fd);*/
      applyopts(fd->stream.fd, opts, PH_PASTSOCKET);
      if (Setsockopt(fd->stream.fd, opt_so_reuseaddr.major,
		     opt_so_reuseaddr.minor, &one, sizeof(one)) < 0) {
	 Warn6("setsockopt(%d, %d, %d, {%d}, %F_Zd): %s",
	       fd->stream.fd, opt_so_reuseaddr.major,
	       opt_so_reuseaddr.minor, &one, sizeof(one), strerror(errno));
      }
      applyopts_cloexec(fd->stream.fd, opts);
      applyopts(fd->stream.fd, opts, PH_PREBIND);
      applyopts(fd->stream.fd, opts, PH_BIND);
      if (Bind(fd->stream.fd, &us.soa, uslen) < 0) {
	 Error4("bind(%d, {%s}, "F_Zd"): %s", fd->stream.fd,
		sockaddr_info(&us.soa, infobuff, sizeof(infobuff)),
		uslen, strerror(errno));
	 return STAT_RETRYLATER;
      }
      /* under some circumstances bind() fills sockaddr with interesting info. */
      if (Getsockname(fd->stream.fd, &us.soa, &uslen) < 0) {
	 Error4("getsockname(%d, %p, {%d}): %s",
		fd->stream.fd, &us.soa, uslen, strerror(errno));
      }
      applyopts(fd->stream.fd, opts, PH_PASTBIND);

      Notice1("listening on UDP %s",
	      sockaddr_info(&us.soa, infobuff, sizeof(infobuff)));
      FD_ZERO(&in); FD_ZERO(&out); FD_ZERO(&expt);
      FD_SET(fd->stream.fd, &in);
      while (Select(fd->stream.fd+1, &in, &out, &expt, NULL) < 0) {
	 if (errno != EINTR)  break;
      }

      themlen = socket_init(pf, them);
      do {
	 result = Recvfrom(fd->stream.fd, buff1, 1, MSG_PEEK,
			     &them->soa, &themlen);
      } while (result < 0 && errno == EINTR);
      if (result < 0) {
	 Error5("recvfrom(%d, %p, 1, MSG_PEEK, {%s}, {"F_Zu"}): %s",
		fd->stream.fd, buff1,
		sockaddr_info(&them->soa, infobuff, sizeof(infobuff)),
		themlen, strerror(errno));
	 return STAT_RETRYLATER;
      }

#if WITH_IP4
      if (pf == PF_INET) {
	 if (checkrange(&them->soa, &netaddr_in, &netmask_in) < 0) {
	    char infobuff[256];
	    Warn1("dropping datagram from %s",
		  sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
	    /* forbidden peer address, drop the datagram */
	    Recvfrom(fd->stream.fd, buff1, 1, 0, &them->soa, &themlen); 
	    continue;
	 }
      }
#endif /* WITH_IP4 */

#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
      if (dolibwrap || libwrapname) {
	 struct request_info ri;
	 int allow;

	 Debug5("request_init(%p, RQ_FILE, %d, RQ_CLIENT_SIN, %p, RQ_SERVER_SIN, %p, RQ_DAEMON, \"%s\", 0",
		&ri, fd->stream.fd, &la->soa, them, libwrapname);
	 request_init(&ri, RQ_FILE, fd->stream.fd, RQ_CLIENT_SIN, &la->soa,
		      RQ_SERVER_SIN, them, RQ_DAEMON, libwrapname, 0);
	 Debug("request_init() ->");

	 Debug1("hosts_access(%p)", &ri);
	 allow = hosts_access(&ri);
	 Debug1("hosts_access() -> %d", allow);
	 if (allow == 0) {
	    unsigned char infobuff[256];

	    Warn1("dropping datagram from %s due to tcpwrapper option",
		  sockaddr_info(&them->soa,
				infobuff, sizeof(infobuff)));
	    /* forbidden peer address, drop the datagram */
	    Recvfrom(fd->stream.fd, buff1, 1, 0, &them->soa, &themlen); 
	    continue;
	 }
	 Info1("permitting connection from %s due to tcpwrapper option",
	       sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
      }
#endif /* (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */

#if WITH_TCP || WITH_UDP
      if (dosourceport) {
	 if (them->soa.sa_family == AF_INET &&
	     ntohs(them->ip4.sin_port) != sourceport) {
	    Warn1("dropping datagram from %s due to sourceport option",
		  sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
	    Recvfrom(fd->stream.fd, buff1, 1, 0, &them->soa, &themlen); 
	    continue;
	 }
#if WITH_IP6
	 else if (them->soa.sa_family == AF_INET6 &&
		  ntohs(them->ip6.sin6_port) != sourceport) {
	    Warn1("dropping datagram from %s due to sourceport option",
		   sockaddr_info(&them->soa,
				 infobuff, sizeof(infobuff)));
	    Recvfrom(fd->stream.fd, buff1, 1, 0, &them->soa, &themlen); 
	    continue;
	 }
#endif /* WITH_IP6 */
	 Info1("permitting connection from %s due to sourceport option",
	       sockaddr_info(&them->soa,
			     infobuff, sizeof(infobuff)));
      } else if (lowport) {
	 if ((them->soa.sa_family == AF_INET &&
	     ntohs(them->ip4.sin_port) >= IPPORT_RESERVED)
#if WITH_IP6
	     || (them->soa.sa_family == AF_INET6 &&
		 ntohs(them->ip6.sin6_port) >= IPPORT_RESERVED)
#endif
	     ) {
	    Warn1("dropping datagram from %s due to lowport option",
		  sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
	    Recvfrom(fd->stream.fd, buff1, 1, 0, &them->soa, &themlen); 
	    continue;
	 }
	 Info1("permitting UDP connection from %s due to lowport option",
	       sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
      }
#endif /* WITH_TCP || WITH_UDP */

      Notice1("accepting UDP connection from %s",
	      sockaddr_info(&them->soa, infobuff, sizeof(infobuff)));
      if (dofork) {
	 pid = Fork();
	 if (pid < 0) {
	    Error1("fork(): %s", strerror(errno));
	    return STAT_RETRYLATER;
	 }
	 if (pid == 0) {	/* child */
#if 0
	    Close(fd->fd);
	    if ((fd->fd = Socket(pf, socktype, ipproto)) < 0) {
	       Error4("socket(%d, %d, %d): %s",
		      pf, socktype, ipproto, strerror(errno));
	       return STAT_RETRYLATER;
	    }
	    /*0 Info4("socket(%d, %d, %d) -> %d", pf, socktype, ipproto, fd->fd);*/
#endif
	    break;
	 }
	 /* server: continue loop with select */
#if 1
	 /* when we dont close this we get awkward behaviour on Linux 2.4:
	    recvfrom gives 0 bytes with invalid socket address */
	 if (Close(fd->stream.fd) < 0) {
	    Warn2("close(%d): %s", fd->stream.fd, strerror(errno));
	 }
#endif
	 Notice1("forked off child process "F_pid, pid);
	 Sleep(1);	/*! give child a chance to consume the old packet */

	 continue;
      }
      break;
   }

   applyopts(fd->stream.fd, opts, PH_CONNECT);
   if ((result = Connect(fd->stream.fd, &them->soa, themlen)) < 0) {
      Error4("connect(%d, {%s}, "F_Zd"): %s",
	     fd->stream.fd,
	     sockaddr_info(&them->soa, infobuff, sizeof(infobuff)),
	     themlen, strerror(errno));
      return STAT_RETRYLATER;
   }

   fd->stream.howtoend = END_SHUTDOWN;
   applyopts_fchown(fd->stream.fd, opts);
   applyopts(fd->stream.fd, opts, PH_LATE);

   if ((result = _xio_openlate(&fd->stream, opts)) < 0)
      return result;

   return 0;
}
#endif /* WITH_UDP && (WITH_IP4 || WITH_IP6) */
