/* $Id: xio-tcp.c,v 1.14 2004/06/20 21:21:05 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for TCP related functions and options */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-listen.h"
#include "xio-ip4.h"
#include "xio-ipapp.h"
#include "xio-tcp.h"

#if WITH_TCP

/****** TCP addresses ******/

#if WITH_IP4
const struct addrdesc addr_tcp4_connect = { "tcp4", 3 /* O_RDWR */, xioopen_ip4app_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_TCP|GROUP_CHILD|GROUP_RETRY, SOCK_STREAM, IPPROTO_TCP, "tcp" HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc addr_tcp4_listen  = { "tcp4-listen", 3, xioopen_ip4app_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_TCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_STREAM, IPPROTO_TCP, "tcp" HELP(":<port>") };
#endif
#endif /* WITH_IP4 */

#if WITH_IP6
const struct addrdesc addr_tcp6_connect = { "tcp6",    3, xioopen_ip6app_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_CHILD|GROUP_RETRY, SOCK_STREAM, IPPROTO_TCP, "tcp" HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc addr_tcp6_listen  = { "tcp6-listen", 3, xioopen_tcp6_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_TCP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY, SOCK_STREAM, IPPROTO_TCP, "tcp" HELP(":<port>") };
#endif
#endif /* WITH_IP6 */

/****** TCP address options ******/

#ifdef TCP_NODELAY
const struct optdesc opt_tcp_nodelay = { "tcp-nodelay",   "nodelay", OPT_TCP_NODELAY, GROUP_IP_TCP, PH_PASTSOCKET, TYPE_INT,	OFUNC_SOCKOPT, SOL_TCP, TCP_NODELAY };
#endif
#ifdef TCP_MAXSEG
const struct optdesc opt_tcp_maxseg  = { "tcp-maxseg",    "mss",  OPT_TCP_MAXSEG,  GROUP_IP_TCP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_MAXSEG };
const struct optdesc opt_tcp_maxseg_late={"tcp-maxseg-late","mss-late",OPT_TCP_MAXSEG_LATE,GROUP_IP_TCP,PH_CONNECTED,TYPE_INT,OFUNC_SOCKOPT, SOL_TCP, TCP_MAXSEG};
#endif
#ifdef TCP_CORK
const struct optdesc opt_tcp_cork   = { "tcp-cork",     "cork", OPT_TCP_CORK,    GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_CORK };
#endif
#ifdef TCP_STDURG
const struct optdesc opt_tcp_stdurg = { "tcp-stdurg",   "stdurg", OPT_TCP_STDURG,  GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_STDURG };
#endif
#ifdef TCP_RFC1323
const struct optdesc opt_tcp_rfc1323= { "tcp-rfc1323",  "rfc1323", OPT_TCP_RFC1323, GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_RFC1323};
#endif
#ifdef TCP_KEEPIDLE
const struct optdesc opt_tcp_keepidle={ "tcp-keepidle", "keepidle",OPT_TCP_KEEPIDLE,GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP,TCP_KEEPIDLE};
#endif
#ifdef TCP_KEEPINTVL
const struct optdesc opt_tcp_keepintvl={"tcp-keepintvl","keepintvl",OPT_TCP_KEEPINTVL,GROUP_IP_TCP,PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP,TCP_KEEPINTVL};
#endif
#ifdef TCP_KEEPCNT
const struct optdesc opt_tcp_keepcnt= { "tcp-keepcnt",  "keepcnt",  OPT_TCP_KEEPCNT, GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_KEEPCNT };
#endif
#ifdef TCP_SYNCNT
const struct optdesc opt_tcp_syncnt = { "tcp-syncnt",   "syncnt",   OPT_TCP_SYNCNT,  GROUP_IP_TCP, PH_PASTSOCKET,    TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_SYNCNT };
#endif
#ifdef TCP_LINGER2
const struct optdesc opt_tcp_linger2= { "tcp-linger2",  "linger2",  OPT_TCP_LINGER2, GROUP_IP_TCP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_LINGER2 };
#endif
#ifdef TCP_DEFER_ACCEPT
const struct optdesc opt_tcp_defer_accept={"tcp-defer-accept","defer-accept",OPT_TCP_DEFER_ACCEPT,GROUP_IP_TCP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_TCP,TCP_DEFER_ACCEPT };
#endif
#ifdef TCP_WINDOW_CLAMP
const struct optdesc opt_tcp_window_clamp={"tcp-window-clamp","window-clamp",OPT_TCP_WINDOW_CLAMP,GROUP_IP_TCP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_TCP,TCP_WINDOW_CLAMP };
#endif
#ifdef TCP_INFO
const struct optdesc opt_tcp_info   = { "tcp-info",     "info", OPT_TCP_INFO,    GROUP_IP_TCP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_INFO };
#endif
#ifdef TCP_QUICKACK
const struct optdesc opt_tcp_quickack = { "tcp-quickack", "quickack", OPT_TCP_QUICKACK, GROUP_IP_TCP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_TCP, TCP_QUICKACK };
#endif

#if WITH_IP6 && WITH_LISTEN
/* we expect the form: port */
int xioopen_tcp6_listen(char *a1, int rw, xiofile_t *fd,
			     unsigned groups, int dummy1, int dummy2,
			     void *dummyp1) {
   char *a2;
   struct opt *opts = NULL;
   struct sockaddr_in6 us;
   struct sockaddr_in6 them;
   int needbind = 0;
   int socktype = SOCK_STREAM;
   int result;

   if (a2 = strchr(a1, ',')) {
      *a2++ = '\0';
   }

   /* initialization of local vars */
   socket_in6_init(&us);
   socket_in6_init(&them);

   if (parseopts(a2, groups, &opts) < 0) {
      return STAT_NORETRY;
   }

   /* destination port */
   /*0 us.sin6_port       = htons(strtoul(a1, &a1, 0));*/
   us.sin6_port = parseport(a1, "tcp");

   if (retropt_bind(opts, AF_INET6, "tcp", (struct sockaddr *)&us, 1) >= 0)
      needbind = true;

   applyopts(-1, opts, PH_INIT);
   applyopts_single(&fd->stream, opts, PH_INIT);
   retropt_int(opts, OPT_SO_TYPE, &socktype);

   if ((result =
	_xioopen_listen(&fd->stream,
			(struct sockaddr *)&us, sizeof(struct sockaddr_in6),
			opts, PF_INET6, socktype, IPPROTO_TCP,
#if WITH_RETRY
			(fd->stream.retry||fd->stream.forever)?E_INFO:E_ERROR
#else
			E_ERROR
#endif /* WITH_RETRY */
			))
       != 0)
      return result;
   return 0;
}
#endif /* WITH_IP6 && WITH_LISTEN */

#endif /* WITH_TCP */
