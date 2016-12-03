/* $Id: xio-listen.c,v 1.30 2004/06/20 21:30:33 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for listen socket options */

#include "xiosysincludes.h"
#if WITH_LIBWRAP
#include "tcpd.h"
#endif
#include "xioopen.h"

#include "xio-named.h"
#include "xio-ip.h"
#include "xio-ip4.h"
#include "xio-listen.h"

#if WITH_LISTEN

/***** LISTEN addresses *****/
const struct optdesc opt_backlog = { "backlog",   NULL, OPT_BACKLOG,     GROUP_LISTEN, PH_LISTEN, TYPE_INT,    OFUNC_SPEC };
const struct optdesc opt_fork    = { "fork",      NULL, OPT_FORK,        GROUP_CHILD,   PH_PASTACCEPT, TYPE_BOOL,  OFUNC_SPEC };
/**/
#if (WITH_UDP || WITH_TCP)
const struct optdesc opt_range   = { "range",     NULL, OPT_RANGE,       GROUP_RANGE,  PH_ACCEPT, TYPE_STRING, OFUNC_SPEC };
#endif
#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
const struct optdesc opt_tcpwrappers = { "tcpwrappers", "tcpwrap", OPT_TCPWRAPPERS, GROUP_RANGE,  PH_ACCEPT, TYPE_STRING_NULL, OFUNC_SPEC };
#endif


#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
/* libwrap these are declared only extern and would be unresolved without these
   definitions */
int allow_severity=10, deny_severity=10;
#endif

/* waits for incoming connection, checks its source address and port. Depending
   on fork option, it may fork a subprocess.
   Returns 0 if a connection was accepted; with fork option, this is a
   subprocess!
   Other return values indicate a problem; this can happen in the master
   process or in a subprocess.
   This function does not retry itself. If you need retries, handle this is a
   loop in the calling function.
   With fork, the forever/retry of the child process are set to 0
 */
int _xioopen_listen(struct single *xfd, struct sockaddr *us, socklen_t uslen,
		 struct opt *opts, int pf, int socktype, int proto, int level) {
   struct sockaddr sa;
   socklen_t salen;
   int backlog = 5;	/* why? 1 seems to cause problems under some load */
   bool dofork = false;
   pid_t pid;	/* mostly int; only used with fork */
   char *rangename;	/* allowed client address range - text form */
   unsigned short sourceport;
   bool lowport = false;
   bool dosourceport = false;
#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   bool dolibwrap = false;
   char *libwrapname = NULL;
#endif
#if WITH_IP4
   byte4_t netaddr_in = 0;
   byte4_t netmask_in = 0;
#endif /* WITH_IP4 */
   char infobuff[256];
   char lisname[256];
   int result;

   retropt_bool(opts, OPT_FORK, &dofork);

   applyopts_single(xfd, opts, PH_INIT);

#if 1
   if (dofork) {
#if HAVE_SIGACTION
      struct sigaction act;
      memset(&act, 0, sizeof(struct sigaction));
      act.sa_flags   = SA_NOCLDSTOP|SA_RESTART
#ifdef SA_NOMASK
	 |SA_NOMASK
#endif
	 ;
      act.sa_handler = childdied;
      if (Sigaction(SIGCHLD, &act, NULL) < 0) {
	 /*! man does not say that errno is defined */
	 Warn2("sigaction(SIGCHLD, %p, NULL): %s", childdied, strerror(errno));
      }
#else /* HAVE_SIGACTION */
      if (Signal(SIGCHLD, childdied) == SIG_ERR) {
	 Warn2("signal(SIGCHLD, %p): %s", childdied, strerror(errno));
      }
#endif /* !HAVE_SIGACTION */
   }
#endif /* 1 */

   if ((xfd->fd = Socket(pf, socktype, proto)) < 0) {
      Msg4(level,
	   "socket(%d, %d, %d): %s", pf, socktype, proto, strerror(errno));
      return STAT_RETRYLATER;
   }

   applyopts(xfd->fd, opts, PH_PASTSOCKET);

   applyopts_cloexec(xfd->fd, opts);

   applyopts(xfd->fd, opts, PH_PREBIND);
   applyopts(xfd->fd, opts, PH_BIND);
   if (Bind(xfd->fd, (struct sockaddr *)us, uslen) < 0) {
      Msg4(level, "bind(%d, {%s}, "F_Zd"): %s", xfd->fd,
	   sockaddr_info(us, infobuff, sizeof(infobuff)), uslen,
	   strerror(errno));
      Close(xfd->fd);
      return STAT_RETRYLATER;
   }

#if WITH_UNIX
   if (us->sa_family == AF_UNIX) {
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_FD);
   }
#endif
   /* under some circumstances (e.g., TCP listen on port 0) bind() fills empty
      fields that we want to know. */
   salen = sizeof(sa);
   if (Getsockname(xfd->fd, us, &uslen) < 0) {
      Error4("getsockname(%d, %p, {%d}): %s",
	     xfd->fd, &us, uslen, strerror(errno));
   }

   applyopts(xfd->fd, opts, PH_PASTBIND);
#if WITH_UNIX
   if (us->sa_family == AF_UNIX) {
      /*applyopts_early(((struct sockaddr_un *)us)->sun_path, opts);*/
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_EARLY);
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_PREOPEN);
   }
#endif /* WITH_UNIX */

   retropt_int(opts, OPT_BACKLOG, &backlog);
   if (Listen(xfd->fd, backlog) < 0) {
      Error3("listen(%d, %d): %s", xfd->fd, backlog, strerror(errno));
      return STAT_RETRYLATER;
   }

#if WITH_IP4 /*|| WITH_IP6*/
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (pf == PF_INET) {
	 parserange(rangename, us->sa_family, &netaddr_in, &netmask_in);
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

   if (xioopts.logopt == 'm') {
      Info("starting accept loop, switching to syslog");
      diag_set('y', xioopts.syslogfac);  xioopts.logopt = 'y';
   } else {
      Info("starting accept loop");
   }
   while (true) {	/* but we only loop if fork option is set */
      char peername[256];
      char sockname[256];
      int ps;		/* peer socket */
      char _peername[SOCKADDR_MAX];
      union sockaddr_union _sockname;
      struct sockaddr *pa = (struct sockaddr *)_peername;	/* peer address */
      union sockaddr_union *la = &_sockname;	/* local address */
      socklen_t pas = sizeof(_peername);	/* peer address size */
      socklen_t las = sizeof(_sockname);	/* local address size */
      salen = sizeof(struct sockaddr);

      do {
	 /*? int level = E_ERROR;*/
	 Notice1("listening on %s", sockaddr_info(us, lisname, sizeof(lisname)));
	 ps = Accept(xfd->fd, (struct sockaddr *)&sa, &salen);
	 if (ps >= 0) {
	    /*0 Info4("accept(%d, %p, {"F_Zu"}) -> %d", xfd->fd, &sa, salen, ps);*/
	    break;	/* success, break out of loop */
	 }
	 if (errno == EINTR) {
	    continue;
	 }
	 if (errno == ECONNABORTED) {
	    Notice4("accept(%d, %p, {"F_Zu"}): %s",
		    xfd->fd, &sa, salen, strerror(errno));
	    continue;
	 }
	 Msg4(level, "accept(%d, %p, {"F_Zu"}): %s",
	      xfd->fd, &sa, salen, strerror(errno));
	 Close(xfd->fd);
	 return STAT_RETRYLATER;
      } while (true);
      applyopts_cloexec(ps, opts);
      if (Getpeername(ps, pa, &pas) < 0) {
	 Warn4("getpeername(%d, %p, {"F_socklen"}): %s",
	       ps, pa, pas, strerror(errno));
      }
      if (Getsockname(ps, &la->soa, &las) < 0) {
	 Warn4("getsockname(%d, %d, {"F_socklen"}): %s",
	       ps, pa, pas, strerror(errno));
      }
      Notice2("accepting connection from %s on %s",
	      sockaddr_info(pa, peername, sizeof(peername)),
	      sockaddr_info(&la->soa, sockname, sizeof(sockname)));

#if WITH_IP4
      if (rangename != NULL) {
	 if (checkrange(pa, &netaddr_in, &netmask_in) < 0) {
	    char infobuff[256];
	    Warn1("refusing connection from %s due to range option",
		  sockaddr_info((struct sockaddr *)pa,
				infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
	 Info1("permitting connection from %s due to range option",
	       sockaddr_info((struct sockaddr *)pa,
			     infobuff, sizeof(infobuff)));
      }
#endif /* WITH_IP4 */

#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
      if (dolibwrap || libwrapname) {
	 struct request_info ri;
	 int allow;

	 Debug5("request_init(%p, RQ_FILE, %d, RQ_CLIENT_SIN, %p, RQ_SERVER_SIN, %p, RQ_DAEMON, \"%s\", 0",
		&ri, xfd->fd, &la->soa, pa, libwrapname);
	 request_init(&ri, RQ_FILE, xfd->fd, RQ_CLIENT_SIN, &la->soa,
		      RQ_SERVER_SIN, pa, RQ_DAEMON, libwrapname, 0);
	 Debug("request_init() ->");

	 Debug1("hosts_access(%p)", &ri);
	 allow = hosts_access(&ri);
	 Debug1("hosts_access() -> %d", allow);
	 if (allow == 0) {
	    char infobuff[256];
	    Warn1("refusing connection from %s due to tcpwrapper option",
		  sockaddr_info((struct sockaddr *)pa,
				infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
	 Info1("permitting connection from %s due to tcpwrapper option",
	       sockaddr_info((struct sockaddr *)pa,
			     infobuff, sizeof(infobuff)));
      }
#endif /* (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */

#if WITH_TCP || WITH_UDP
      if (dosourceport) {
#if WITH_IP4
	 if (pa->sa_family == AF_INET &&
	     ntohs(((struct sockaddr_in *)pa)->sin_port) != sourceport) {
	    Warn1("refusing connection from %s due to sourceport option",
		  sockaddr_info((struct sockaddr *)pa,
				infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
#endif /* WITH_IP4 */
#if WITH_IP6
	 if (pa->sa_family == AF_INET6 &&
	     ntohs(((struct sockaddr_in6 *)pa)->sin6_port) != sourceport) {
	    Warn1("refusing connection from %s due to sourceport option",
		   sockaddr_info((struct sockaddr *)pa,
				 infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
#endif /* WITH_IP6 */
	 Info1("permitting connection from %s due to sourceport option",
	       sockaddr_info((struct sockaddr *)pa,
			     infobuff, sizeof(infobuff)));
      } else if (lowport) {
	 if (pa->sa_family == AF_INET &&
	     ntohs(((struct sockaddr_in *)pa)->sin_port) >= IPPORT_RESERVED) {
	    Warn1("refusing connection from %s due to lowport option",
		  sockaddr_info((struct sockaddr *)pa,
				infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
#if WITH_IP6
	 else if (pa->sa_family == AF_INET6 &&
		  ntohs(((struct sockaddr_in6 *)pa)->sin6_port) >=
		  IPPORT_RESERVED) {
	    Warn1("refusing connection from %s due to lowport option",
		  sockaddr_info((struct sockaddr *)pa,
				infobuff, sizeof(infobuff)));
	    if (Shutdown(ps, 2) < 0) {
	       Warn2("shutdown(%d, 2): %s", pa, strerror(errno));
	    }
	    continue;
	 }
#endif /* WITH_IP6 */
	 Info1("permitting connection from %s due to lowport option",
	       sockaddr_info((struct sockaddr *)pa,
			     infobuff, sizeof(infobuff)));
      }
#endif /* WITH_TCP || WITH_UDP */

      applyopts(xfd->fd, opts, PH_FD);

      applyopts(xfd->fd, opts, PH_CONNECTED);

      if (dofork) {
	 if ((pid = Fork()) < 0) {
	    Msg1(level, "fork(): %s", strerror(errno));
	    Close(xfd->fd);
	    return STAT_RETRYLATER;
	 }
	 if (pid == 0) {	/* child */
	    if (Close(xfd->fd) < 0) {
	       Warn2("close(%d): %s", xfd->fd, strerror(errno));
	    }
	    xfd->fd = ps;

#if WITH_RETRY
	    /* !? */
	    xfd->retry = 0;
	    xfd->forever = 0;
	    level = E_ERROR;
#endif /* WITH_RETRY */

	    break;
	 }

	 /* server: continue loop with listen */
	 /* shutdown() closes the socket even for the child process, but
	    close() does what we want */
	 if (Close(ps) < 0) {
	    Warn2("close(%d): %s", ps, strerror(errno));
	 }
	 Notice1("forked off child process "F_pid, pid);
	 Info("still listening");
      } else {
	 if (Close(xfd->fd) < 0) {
	    Warn2("close(%d): %s", xfd->fd, strerror(errno));
	 }
	 xfd->fd = ps;
	break;
      }
   }
   if ((result = _xio_openlate(xfd, opts)) < 0)
      return result;

   xfd->howtoend = END_SHUTDOWN;
   return 0;
}

#endif /* WITH_LISTEN */
