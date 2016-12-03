/* $Id: xio-socket.c,v 1.26 2004/06/20 21:29:36 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for socket related functions */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-ipapp.h"	/*! not clean */

#if WITH_SOCKET

const struct optdesc opt_so_debug    = { "so-debug",    "debug", OPT_SO_DEBUG,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_DEBUG };
#ifdef SO_ACCEPTCONN /* AIX433 */
const struct optdesc opt_so_acceptconn={ "so-acceptconn","acceptconn",OPT_SO_ACCEPTCONN,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_ACCEPTCONN};
#endif /* SO_ACCEPTCONN */
const struct optdesc opt_so_broadcast= { "so-broadcast", "broadcast", OPT_SO_BROADCAST,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_BROADCAST};
const struct optdesc opt_so_reuseaddr= { "so-reuseaddr", "reuseaddr", OPT_SO_REUSEADDR,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_REUSEADDR};
const struct optdesc opt_so_keepalive= { "so-keepalive", "keepalive", OPT_SO_KEEPALIVE,GROUP_SOCKET, PH_FD, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_KEEPALIVE};
#if HAVE_STRUCT_LINGER
const struct optdesc opt_so_linger   = { "so-linger",    "linger",    OPT_SO_LINGER,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_LINGER,OFUNC_SOCKOPT,SOL_SOCKET, SO_LINGER };
#else /* !HAVE_STRUCT_LINGER */
const struct optdesc opt_so_linger   = { "so-linger",    "linger",    OPT_SO_LINGER,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_LINGER };
#endif /* !HAVE_STRUCT_LINGER */
const struct optdesc opt_so_oobinline= { "so-oobinline", "oobinline", OPT_SO_OOBINLINE,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_OOBINLINE};
const struct optdesc opt_so_sndbuf   = { "so-sndbuf",    "sndbuf",    OPT_SO_SNDBUF,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_SNDBUF};
const struct optdesc opt_so_sndbuf_late={ "so-sndbuf-late","sndbuf-late",OPT_SO_SNDBUF_LATE,GROUP_SOCKET,PH_LATE,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SNDBUF };
const struct optdesc opt_so_rcvbuf   = { "so-rcvbuf",    "rcvbuf",    OPT_SO_RCVBUF,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_RCVBUF};
const struct optdesc opt_so_rcvbuf_late={"so-rcvbuf-late","rcvbuf-late",OPT_SO_RCVBUF_LATE,GROUP_SOCKET,PH_LATE,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_RCVBUF };
const struct optdesc opt_so_error    = { "so-error",     "error",     OPT_SO_ERROR,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_ERROR};
const struct optdesc opt_so_type     = { "so-type",      "type",      OPT_SO_TYPE,     GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_TYPE };
const struct optdesc opt_so_dontroute= { "so-dontroute", "dontroute", OPT_SO_DONTROUTE,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_DONTROUTE };
#ifdef SO_RCVLOWAT
const struct optdesc opt_so_rcvlowat = { "so-rcvlowat",  "rcvlowat", OPT_SO_RCVLOWAT, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_RCVLOWAT };
#endif
#ifdef SO_RCVTIMEO
const struct optdesc opt_so_rcvtimeo = { "so-rcvtimeo",  "rcvtimeo", OPT_SO_RCVTIMEO, GROUP_SOCKET, PH_PASTSOCKET, TYPE_TIMEVAL,OFUNC_SOCKOPT,SOL_SOCKET,SO_RCVTIMEO };
#endif
#ifdef SO_SNDLOWAT
const struct optdesc opt_so_sndlowat = { "so-sndlowat",  "sndlowat", OPT_SO_SNDLOWAT, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_SNDLOWAT };
#endif
#ifdef SO_SNDTIMEO
const struct optdesc opt_so_sndtimeo = { "so-sndtimeo",  "sndtimeo", OPT_SO_SNDTIMEO, GROUP_SOCKET, PH_PASTSOCKET, TYPE_TIMEVAL,OFUNC_SOCKOPT,SOL_SOCKET,SO_SNDTIMEO };
#endif
/* end of setsockopt options of UNIX98 standard */

#ifdef SO_AUDIT	/* AIX 4.3.3 */
const struct optdesc opt_so_audit    = { "so-audit",     "audit",    OPT_SO_AUDIT,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_AUDIT };
#endif /* SO_AUDIT */
#ifdef SO_ATTACH_FILTER
const struct optdesc opt_so_attach_filter={"so-attach-filter","attachfilter",OPT_SO_ATTACH_FILTER,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_ATTACH_FILTER};
#endif
#ifdef SO_DETACH_FILTER
const struct optdesc opt_so_detach_filter={"so-detach-filter","detachfilter",OPT_SO_DETACH_FILTER,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DETACH_FILTER};
#endif
#ifdef SO_BINDTODEVICE	/* Linux: man 7 socket */
const struct optdesc opt_so_bindtodevice={"so-bindtodevice","if",OPT_SO_BINDTODEVICE,GROUP_SOCKET,PH_PASTSOCKET,TYPE_NAME,OFUNC_SOCKOPT,SOL_SOCKET,SO_BINDTODEVICE};
#endif
#ifdef SO_BSDCOMPAT
const struct optdesc opt_so_bsdcompat= { "so-bsdcompat","bsdcompat",OPT_SO_BSDCOMPAT,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_BSDCOMPAT };
#endif
#ifdef SO_CKSUMRECV
const struct optdesc opt_so_cksumrecv= { "so-cksumrecv","cksumrecv",OPT_SO_CKSUMRECV,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_CKSUMRECV };
#endif /* SO_CKSUMRECV */
#ifdef SO_KERNACCEPT	/* AIX 4.3.3 */
const struct optdesc opt_so_kernaccept={ "so-kernaccept","kernaccept",OPT_SO_KERNACCEPT,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_KERNACCEPT};
#endif /* SO_KERNACCEPT */
#ifdef SO_NO_CHECK
const struct optdesc opt_so_no_check = { "so-no-check", "nocheck",OPT_SO_NO_CHECK, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_NO_CHECK };
#endif
#ifdef SO_NOREUSEADDR	/* AIX 4.3.3 */
const struct optdesc opt_so_noreuseaddr={"so-noreuseaddr","noreuseaddr",OPT_SO_NOREUSEADDR,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET, SO_NOREUSEADDR};
#endif /* SO_NOREUSEADDR */
#ifdef SO_PASSCRED
const struct optdesc opt_so_passcred = { "so-passcred", "passcred", OPT_SO_PASSCRED, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_PASSCRED};
#endif
#ifdef SO_PEERCRED
const struct optdesc opt_so_peercred = { "so-peercred", "peercred", OPT_SO_PEERCRED, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT3,OFUNC_SOCKOPT, SOL_SOCKET, SO_PEERCRED};
#endif
#ifdef SO_PRIORITY
const struct optdesc opt_so_priority = { "so-priority", "priority", OPT_SO_PRIORITY, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_PRIORITY};
#endif
#ifdef SO_REUSEPORT	/* AIX 4.3.3, BSD */
const struct optdesc opt_so_reuseport= { "so-reuseport","reuseport",OPT_SO_REUSEPORT,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_REUSEPORT };
#endif /* defined(SO_REUSEPORT) */
#ifdef SO_SECURITY_AUTHENTICATION
const struct optdesc opt_so_security_authentication={"so-security-authentication","securityauthentication",OPT_SO_SECURITY_AUTHENTICATION,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_AUTHENTICATION};
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
const struct optdesc opt_so_security_encryption_network={"so-security-encryption-network","securityencryptionnetwork",OPT_SO_SECURITY_ENCRYPTION_NETWORK,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_ENCRYPTION_NETWORK};
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
const struct optdesc opt_so_security_encryption_transport={"so-security-encryption-transport","securityencryptiontransport",OPT_SO_SECURITY_ENCRYPTION_TRANSPORT,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_ENCRYPTION_TRANSPORT};
#endif
#ifdef SO_USE_IFBUFS
const struct optdesc opt_so_use_ifbufs={ "so-use-ifbufs","useifbufs",OPT_SO_USE_IFBUFS,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_USE_IFBUFS};
#endif /* SO_USE_IFBUFS */
#ifdef SO_USELOOPBACK /* AIX433, Solaris */
const struct optdesc opt_so_useloopback={"so-useloopback","useloopback",OPT_SO_USELOOPBACK,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT, SOL_SOCKET, SO_USELOOPBACK};
#endif /* SO_USELOOPBACK */
#ifdef SO_DGRAM_ERRIND	/* Solaris */
const struct optdesc opt_so_dgram_errind={"so-dgram-errind","dgramerrind",OPT_SO_DGRAM_ERRIND,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DGRAM_ERRIND};
#endif /* SO_DGRAM_ERRIND */
#ifdef SO_DONTLINGER	/* Solaris */
const struct optdesc opt_so_dontlinger = {"so-dontlinger", "dontlinger",  OPT_SO_DONTLINGER,  GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DONTLINGER };
#endif
#ifdef SO_PROTOTYPE	/* Solaris */
const struct optdesc opt_so_prototype  = {"so-prototype",  "prototype",   OPT_SO_PROTOTYPE,   GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_PROTOTYPE };
#endif
#ifdef FIOSETOWN
const struct optdesc opt_fiosetown   = { "fiosetown", NULL, OPT_FIOSETOWN,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_IOCTL,  FIOSETOWN };
#endif
#ifdef SIOCSPGRP
const struct optdesc opt_siocspgrp   = { "siocspgrp", NULL, OPT_SIOCSPGRP,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_IOCTL,  SIOCSPGRP };
#endif
const struct optdesc opt_bind        = { "bind",      NULL, OPT_BIND,        GROUP_SOCKET, PH_BIND, TYPE_STRING,OFUNC_SPEC };



/* a subroutine that is common to all socket addresses that want to connect
   to a peer address.
   might fork.
   returns 0 on success.
*/
int _xioopen_connect(struct single *xfd, struct sockaddr *us, size_t uslen,
		     struct sockaddr *them, size_t themlen,
		     struct opt *opts, int pf, int stype, int proto,
		     bool alt, int level) {
   char infobuff[256];
   union sockaddr_union la;
   socklen_t lalen = sizeof(la);
   int result;

   if ((xfd->fd = Socket(pf, stype, proto)) < 0) {
      Msg4(level,
	   "socket(%d, %d, %d): %s", pf, stype, proto, strerror(errno));
      return STAT_RETRYLATER;	    
   }

   applyopts(xfd->fd, opts, PH_PASTSOCKET);
   applyopts(xfd->fd, opts, PH_FD);

   applyopts_cloexec(xfd->fd, opts);

   applyopts(xfd->fd, opts, PH_PREBIND);
   applyopts(xfd->fd, opts, PH_BIND);
#if WITH_TCP || WITH_UDP
   if (alt) {
      union sockaddr_union sin, *sinp;
      unsigned short *port, i, N;
      div_t dv;
      bool problem;

      /* prepare sockaddr for bind probing */
      if (us) {
	 sinp = (union sockaddr_union *)us;
      } else {
	 if (them->sa_family == AF_INET) {
	    socket_in_init(&sin.ip4);
#if WITH_IP6
	 } else {
	    socket_in6_init(&sin.ip6);
#endif
	 }
	 sinp = &sin;
      }
      if (them->sa_family == AF_INET) {
	 port = &sin.ip4.sin_port;
#if WITH_IP6
      } else if (them->sa_family == AF_INET6) {
	 port = &sin.ip6.sin6_port;
#endif
      } else {
	 port = 0;	/* just to make compiler happy */
      }
      /* combine random+step variant to quickly find a free port when only
	 few are in use, and certainly find a free port in defined time even
	 if there are almost all in use */
      /* dirt 1: having tcp/udp code in socket function */
      /* dirt 2: using a time related system call for init of random */
      {
	 /* generate a random port, with millisecond random init */
#if 0
	 struct timeb tb;
	 ftime(&tb);
	 srandom(tb.time*1000+tb.millitm);
#else
	 struct timeval tv;
	 struct timezone tz;
	 tz.tz_minuteswest = 0;
	 tz.tz_dsttime = 0;
	 if ((result = Gettimeofday(&tv, &tz)) < 0) {
	    Warn2("gettimeofday(%p, {0,0}): %s", &tv, strerror(errno));
	 }
	 srandom(tv.tv_sec*1000000+tv.tv_usec);
#endif
      }
      dv = div(random(), IPPORT_RESERVED-XIO_IPPORT_LOWER);
      i = N = XIO_IPPORT_LOWER + dv.rem;
      problem = false;
      do {	/* loop over lowport bind() attempts */
	 *port = htons(i);
	 if (Bind(xfd->fd, (struct sockaddr *)sinp, sizeof(*sinp)) < 0) {
	    Msg4(errno==EADDRINUSE?E_INFO:level,
		 "bind(%d, {%s}, "F_Zd"): %s", xfd->fd,
		 sockaddr_info(&sinp->soa, infobuff, sizeof(infobuff)),
		 sizeof(*sinp), strerror(errno));
	    if (errno != EADDRINUSE) {
	       Close(xfd->fd);
	       return STAT_RETRYLATER;
	    }
	 } else {
	    break;	/* could bind to port, good, continue past loop */
	 }
	 --i;  if (i < XIO_IPPORT_LOWER)  i = IPPORT_RESERVED-1;
	 if (i == N) {
	    Msg(level, "no low port available");
	    /*errno = EADDRINUSE; still assigned */
	    Close(xfd->fd);
	    return STAT_RETRYLATER;
	 }
      } while (i != N);
   } else
#endif /* WITH_TCP || WITH_UDP */

   if (us) {
      if (Bind(xfd->fd, us, uslen) < 0) {
	 Msg4(level, "bind(%d, {%s}, "F_Zd"): %s",
	      xfd->fd, sockaddr_info(us, infobuff, sizeof(infobuff)),
	      uslen, strerror(errno));
	 Close(xfd->fd);
	 return STAT_RETRYLATER;
      }
   }

   applyopts(xfd->fd, opts, PH_PASTBIND);

   applyopts(xfd->fd, opts, PH_CONNECT);

   if ((result = Connect(xfd->fd, (struct sockaddr *)them, themlen)) < 0) {
      if (errno == EINPROGRESS) {
	 Warn4("connect(%d, %s, "F_Zd"): %s",
	       xfd->fd, sockaddr_info(them, infobuff, sizeof(infobuff)),
	       themlen, strerror(errno));
      } else if (errno == EPROTOTYPE) {
	 /* this is for UNIX domain sockets: a connect attempt seems to be
	    the only way to distinguish stream and datagram sockets */
	 Warn4("connect(%d, %s, "F_Zd"): %s",
	       xfd->fd, sockaddr_info(them, infobuff, sizeof(infobuff)),
	       themlen, strerror(errno));
	 Info("assuming datagram socket");
	 xfd->dtype = DATA_RECV;
	 xfd->salen = themlen;
	 memcpy(&xfd->peersa.soa, them, xfd->salen);
      } else {
	 Msg4(level, "connect(%d, %s, "F_Zd"): %s",
	      xfd->fd, sockaddr_info(them, infobuff, sizeof(infobuff)),
	      themlen, strerror(errno));
	 Close(xfd->fd);
	 return STAT_RETRYLATER;
      }
   }

   if (Getsockname(xfd->fd, &la.soa, &lalen) < 0) {
      Warn4("getsockname(%d, %p, {%d}): %s",
	    xfd->fd, &la.soa, lalen, strerror(errno));
   }

   xfd->howtoend = END_SHUTDOWN;

   applyopts_fchown(xfd->fd, opts);
   applyopts(xfd->fd, opts, PH_CONNECTED);
   applyopts(xfd->fd, opts, PH_LATE);

   xfd->howtoend = END_SHUTDOWN;
   Notice1("successfully connected from local address %s",
	   sockaddr_info(&la.soa, infobuff, sizeof(infobuff)));

   return STAT_OK;
}


/* a subroutine that is common to all socket addresses that want to connect
   to a peer address.
   might fork.
   returns 0 on success.
*/
int xioopen_connect(struct single *xfd, struct sockaddr *us, size_t uslen,
		    struct sockaddr *them, size_t themlen,
		    struct opt *opts, int pf, int stype, int proto,
		    bool alt) {
   bool dofork = false;
   struct opt *opts0;
   char infobuff[256];
   int level;
   int result;

   retropt_bool(opts, OPT_FORK, &dofork);
   retropt_int(opts, OPT_SO_TYPE, &stype);

   opts0 = copyopts(opts, GROUP_ALL);

   Notice1("opening connection to %s",
	   sockaddr_info(them, infobuff, sizeof(infobuff)));

   do {	/* loop over retries and forks */

#if WITH_RETRY
      if (xfd->forever || xfd->retry) {
	 level = E_INFO;
      } else
#endif /* WITH_RETRY */
	 level = E_ERROR;
      result =
	 _xioopen_connect(xfd, us, uslen, them, themlen, opts,
			  pf, stype, proto, alt, level);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
	 if (xfd->forever || xfd->retry) {
	    --xfd->retry;
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&xfd->intervall, NULL);
	    }
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    continue;
	 }
	 return STAT_NORETRY;
#endif /* WITH_RETRY */
      default:
	 return result;
      }

#if WITH_RETRY
      if (dofork) {
	 pid_t pid;
	 while ((pid = Fork()) < 0) {
	    int level = E_ERROR;
	    if (xfd->forever || --xfd->retry) {
	       level = E_WARN;	/* most users won't expect a problem here,
				   so Notice is too weak */
	    }
	    Msg1(level, "fork(): %s", strerror(errno));
	    if (xfd->forever || xfd->retry) {
	       dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	       Nanosleep(&xfd->intervall, NULL); continue;
	    }
	    return STAT_RETRYLATER;
	 }
	 if (pid == 0) {	/* child process */
	    Info1("just born: TCP client process "F_pid, Getpid());
	    break;
	 }
	 /* parent process */
	 Notice1("forked off child process "F_pid, pid);
	 Close(xfd->fd);
	 /* with and without retry */
	 Nanosleep(&xfd->intervall, NULL);
	 dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	 continue;	/* with next socket() bind() connect() */
      } else
#endif /* WITH_RETRY */
      {
	 break;
      }
#if 0
      if ((result = _xio_openlate(fd, opts)) < 0)
	 return result;
#endif
   } while (true);

   return 0;
}
#endif /* WITH_SOCKET */
