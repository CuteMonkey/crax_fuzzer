/* $Id: xio-ipapp.c,v 1.18 2004/06/20 21:35:01 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for TCP and UDP related options */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-listen.h"
#include "xio-ip6.h"
#include "xio-ipapp.h"

#if WITH_TCP || WITH_UDP

const struct optdesc opt_sourceport = { "sourceport", "sp",       OPT_SOURCEPORT,  GROUP_IPAPP,     PH_LATE,TYPE_2BYTE,	OFUNC_SPEC };
/*const struct optdesc opt_port = { "port",  NULL,    OPT_PORT,        GROUP_IPAPP, PH_BIND,    TYPE_USHORT,	OFUNC_SPEC };*/
const struct optdesc opt_lowport = { "lowport", NULL, OPT_LOWPORT, GROUP_IPAPP, PH_LATE, TYPE_BOOL, OFUNC_SPEC };

#if WITH_IP4
/* we expect the form "host:port" */
int xioopen_ip4app_connect(char *a1, int rw, xiofile_t *xxfd,
			 unsigned groups, int socktype,
			 int ipproto, void *protname) {
   struct single *xfd = &xxfd->stream;
   struct opt *opts = NULL, *opts0 = NULL;
   char *hostname = a1, *portname, *a3;
   bool dofork = false;
   struct sockaddr_in us_sa,  *us = &us_sa;
   struct sockaddr_in themsa, *them = &themsa;
   bool needbind = false;
   bool lowport = false;
   int level;
   int result;

   if (portname = strchr(a1, ':')) {
      *portname++ = '\0';
   } else {
      Error1("xioopen_ip4app_connect(\"%s\", ...): missing parameter(s)", a1);
      return -1;
   }

   if (a3 = strchr(portname, ',')) {
      *a3++ = '\0';
   }
   if (parseopts(a3, groups, &opts) < 0) {
      return STAT_NORETRY;
   }

   applyopts(-1, opts, PH_INIT);
   applyopts_single(xfd, opts, PH_INIT);

   retropt_bool(opts, OPT_FORK, &dofork);

   if (_xioopen_ip4app_prepare(opts, &opts0, hostname, portname, protname,
			       them, us, &needbind, &lowport,
			       &socktype) != STAT_OK) {
      return STAT_NORETRY;
   }

   if (xioopts.logopt == 'm') {
      Info("starting connect loop, switching to syslog");
      diag_set('y', xioopts.syslogfac);  xioopts.logopt = 'y';
   } else {
      Info("starting connect loop");
   }

   do {	/* loop over retries and forks */

#if WITH_RETRY
      if (xfd->forever || xfd->retry) {
	 level = E_INFO;
      } else
#endif /* WITH_RETRY */
	 level = E_ERROR;

      result =
	 _xioopen_connect(xfd,
			  needbind?(struct sockaddr *)us:NULL, sizeof(*us),
			  (struct sockaddr *)them, sizeof(struct sockaddr_in),
			  opts, PF_INET, socktype, ipproto, lowport, level);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
      case STAT_RETRYNOW:
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
   } while (true);

   if ((result = _xio_openlate(xfd, opts)) < 0) {
      return result;
   }
   return 0;
}


/* returns STAT_OK on success or some other value on failure */
int
   _xioopen_ip4app_prepare(struct opt *opts, struct opt **opts0,
			   const char *hostname,
			   const char *portname, const char *protname,
			   struct sockaddr_in *them,
			   struct sockaddr_in *us,
			   bool *needbind, bool *lowport,
			   int *socktype) {
   byte2_t port;
   char infobuff[256];
   int result;

   /* initialization of local vars */
   socket_in_init(them);
   socket_in_init(us);

   if ((result =
	_parseip4addr(hostname, portname, them, (char *)protname, 1, 1))
       < 0)
      return STAT_RETRYLATER;

   applyopts(-1, opts, PH_INIT);

   if (retropt_2bytes(opts, OPT_SOURCEPORT, &port) >= 0) {
      us->sin_port = htons(port);
      *needbind = true;
   }

   /* 3 means: IP address AND port accepted */
   if (retropt_bind(opts, AF_INET, (char *)protname, (struct sockaddr *)us, 3) >= 0)
      *needbind = true;

   retropt_bool(opts, OPT_LOWPORT, lowport);
   retropt_int(opts, OPT_SO_TYPE, socktype);

   *opts0 = copyopts(opts, GROUP_ALL);

   Notice1("opening connection to %s",
	   sockaddr_info((struct sockaddr *)them, infobuff, sizeof(infobuff)));
   return STAT_OK;
}


#if 0
/* this function is the common subset for addresses tcp4, udp4, socks4, proxy,
   openssl */
int _xioopen_ip4app_connect(struct single *xfd,
			    int socktype, int ipproto,
			    struct opt *opts, struct sockaddr_in *us,
			    struct sockaddr_in *them) {
   bool needbind = false;
   byte2_t port;
   bool lowport = false;
   bool dofork = false;
   int level;
   int result;

   if (retropt_2bytes(opts, OPT_SOURCEPORT, &port) >= 0) {
      us->sin_port = htons(port);
      needbind = true;
   }

   /* 3 means: IP address AND port accepted */
   if (retropt_bind(opts, AF_INET, (char *)protname, (struct sockaddr *)us, 3) >= 0)
      needbind = true;

   retropt_bool(opts, OPT_LOWPORT, &lowport);
   retropt_int(opts, OPT_SO_TYPE, &socktype);

   Notice1("opening connection to %s",
	   sockaddr_info((struct sockaddr *)them, infobuff, sizeof(infobuff)));

   result =
      _xioopen_connect(xfd,
		       needbind?(struct sockaddr *)us:NULL, sizeof(*us),
		       (struct sockaddr *)them, sizeof(struct sockaddr_in),
		       opts, PF_INET, socktype, ipproto, lowport, level);
   return result;
}
#endif /* 0 */
#endif /* WITH_IP4 */


#if WITH_IP4 && WITH_TCP && WITH_LISTEN
int _xioopen_ip4app_listen_prepare(struct opt *opts, struct opt **opts0,
				   const char *portname, const char *protname,
				   struct sockaddr_in *us,
				   int *socktype) {
   char *bindname;
   struct hostent *usaddr;

   socket_in_init(us);
   us->sin_port = parseport(portname, protname);

   if (retropt_string(opts, OPT_BIND, &bindname) >= 0) {
      if (xioGethostbyname(bindname, &usaddr, E_ERROR) != STAT_OK) {
	 return STAT_RETRYLATER;
      }
      memcpy(&us->sin_addr, usaddr->h_addr_list[0], 4);
   }

   retropt_int(opts, OPT_SO_TYPE, socktype);

   *opts0 = copyopts(opts, GROUP_ALL);

   return STAT_OK;
}


/* we expect the form: port */
/* currently only used for TCP4 */
int xioopen_ip4app_listen(char *a1, int rw, xiofile_t *fd,
			 unsigned groups, int socktype,
			 int ipproto, void *protname) {
   char *a2;
   struct opt *opts = NULL, *opts0 = NULL;
   struct sockaddr_in us_sa, *us = &us_sa;
   int result;

   if (a2 = strchr(a1, ','))  *a2++ = '\0';

   if (parseopts(a2, groups, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);
   applyopts_single(&fd->stream, opts, PH_INIT);

   if (_xioopen_ip4app_listen_prepare(opts, &opts0, a1, protname, us, &socktype)
       != STAT_OK) {
      return STAT_NORETRY;
   }

   if ((result =
	_xioopen_listen(&fd->stream,
		     (struct sockaddr *)us, sizeof(struct sockaddr_in),
		     opts, PF_INET, socktype, ipproto, E_ERROR))
       != 0)
      return result;
   return 0;
}
#endif /* WITH_IP4 && WITH_TCP && WITH_LISTEN */


#if WITH_IP6
/* we expect the form: host:port */
int xioopen_ip6app_connect(char *a1, int rw, xiofile_t *xxfd,
			 unsigned groups, int socktype, int ipproto,
			 void *protname) {
   struct single *xfd = &xxfd->stream;
   struct opt *opts = NULL;
   char *a2;
   bool needbind = false;
   struct sockaddr_in6 us_sa, *us = &us_sa;
   struct sockaddr_in6 themsa, *them = &themsa;
   byte2_t port;
   bool lowport = false;
   bool dofork = false;
   struct opt *opts0;
   char infobuff[256];
   int level, result;

   if (a2 = strchr(a1, ',')) {
      *a2++ = '\0';
   }

   /* initialization of local vars */
   socket_in6_init(us);
   socket_in6_init(them);

   /* expect destination name/address */
   if ((result = parseip6addr(a1, them, (char *)protname, 1, 1)) < 0) {
      return result;
   }

   if (parseopts(a2, groups, &opts) < 0) {
      return STAT_NORETRY;
   }
   applyopts(-1, opts, PH_INIT);
   applyopts_single(xfd, opts, PH_INIT);
#if WITH_RETRY
   ++xfd->retry;
#endif /* WITH_RETRY */

   if (retropt_2bytes(opts, OPT_SOURCEPORT, &port) >= 0) {
      us->sin6_port = htons(port);
      needbind = true;
   }

   retropt_bool(opts, OPT_LOWPORT, &lowport);

   /* 3 means: IP address AND port accepted */
   if (retropt_bind(opts, AF_INET6, (char *)protname, (struct sockaddr *)us, 3) >= 0)
      needbind = true;

   retropt_bool(opts, OPT_FORK, &dofork);
   retropt_int(opts, OPT_SO_TYPE, &socktype);

   opts0 = copyopts(opts, GROUP_ALL);

   Notice1("opening connection to %s",
	   sockaddr_info((struct sockaddr *)them, infobuff, sizeof(infobuff)));

   do {	/* loop over retries and forks */

#if WITH_RETRY
      if (xfd->forever || xfd->retry) {
	 level = E_INFO;
      } else
#endif /* WITH_RETRY */
	 level = E_ERROR;
      result =
	 _xioopen_connect(xfd, needbind?(struct sockaddr *)us:NULL,sizeof(*us),
			  (struct sockaddr *)them,sizeof(struct sockaddr_in6),
			  opts, PF_INET6, socktype, ipproto, lowport, level);
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

      if ((result = _xio_openlate(xfd, opts)) < 0) {
	 return result;
      }
   } while (true);

   return 0;
}
#endif /* WITH_IP6 */

int xioGethostbyname(const char *hostname, struct hostent **host, int level) {
   *host = Gethostbyname(hostname);
   if (*host == NULL) {
      Msg2(level, "gethostbyname(\"%s\"): %s", hostname,
	   h_errno == NETDB_INTERNAL ? strerror(errno) :
	   hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
      return STAT_RETRYLATER;
   }
   return STAT_OK;
}

#endif /* WITH_TCP || WITH_UDP */
