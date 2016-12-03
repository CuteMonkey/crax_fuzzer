/* $Id: xio-ip.c,v 1.14 2003/06/26 13:17:32 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for IP related functions */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ip6.h"

#if WITH_IP4 || WITH_IP6


#ifdef IP_OPTIONS
const struct optdesc opt_ip_options = { "ip-options", "ipoptions", OPT_IP_OPTIONS, GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_BIN, OFUNC_SOCKOPT_APPEND, SOL_IP, IP_OPTIONS };
#endif
#ifdef IP_PKTINFO
const struct optdesc opt_ip_pktinfo = { "ip-pktinfo", "pktinfo",   OPT_IP_PKTINFO, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_PKTINFO };
#endif
#ifdef IP_RECVTOS
const struct optdesc opt_ip_recvtos = { "ip-recvtos", "recvtos",   OPT_IP_RECVTOS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVTOS };
#endif
#ifdef IP_RECVTTL
const struct optdesc opt_ip_recvttl = { "ip-recvttl", "recvttl",   OPT_IP_RECVTTL, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVTTL };
#endif
#ifdef IP_RECVOPTS
const struct optdesc opt_ip_recvopts= { "ip-recvopts","recvopts",  OPT_IP_RECVOPTS,GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVOPTS };
#endif
#ifdef IP_RETOPTS
const struct optdesc opt_ip_retopts = { "ip-retopts", "retopts",   OPT_IP_RETOPTS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RETOPTS };
#endif
const struct optdesc opt_ip_tos     = { "ip-tos",     "tos",       OPT_IP_TOS,     GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_TOS };
const struct optdesc opt_ip_ttl     = { "ip-ttl",     "ttl",       OPT_IP_TTL,     GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_TTL };
#ifdef IP_HDRINCL
const struct optdesc opt_ip_hdrincl = { "ip-hdrincl", "hdrincl",   OPT_IP_HDRINCL, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_HDRINCL };
#endif
#ifdef IP_RECVERR
const struct optdesc opt_ip_recverr = { "ip-recverr", "recverr",   OPT_IP_RECVERR, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVERR };
#endif
#ifdef IP_MTU_DISCOVER
const struct optdesc opt_ip_mtu_discover={"ip-mtu-discover","mtudiscover",OPT_IP_MTU_DISCOVER,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_MTU_DISCOVER };
#endif
#ifdef IP_MTU
const struct optdesc opt_ip_mtu     = { "ip-mtu",     "mtu",       OPT_IP_MTU,     GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_MTU };
#endif
#ifdef IP_FREEBIND
const struct optdesc opt_ip_freebind= { "ip-freebind","freebind",  OPT_IP_FREEBIND,GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_FREEBIND };
#endif
#ifdef IP_ROUTER_ALERT
const struct optdesc opt_ip_router_alert={"ip-router-alert","routeralert",OPT_IP_ROUTER_ALERT,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_ROUTER_ALERT};
#endif
const struct optdesc opt_ip_multicast_ttl={"ip-multicast-ttl","multicastttl",OPT_IP_MULTICAST_TTL,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_MULTICAST_TTL};
const struct optdesc opt_ip_multicast_loop={"ip-multicast-loop","multicastloop",OPT_IP_MULTICAST_LOOP,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_MULTICAST_LOOP};
#ifdef IP_PKTOPTIONS
const struct optdesc opt_ip_pktoptions = { "ip-pktoptions", "pktopts", OPT_IP_PKTOPTIONS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_PKTOPTIONS };
#endif


#if WITH_IP4 && WITH_IP6
int gethostaddr(const char *name, struct sockaddr *sin) {
   struct hostent *hent;

   if ((hent = Gethostbyname(name)) == NULL) {
      Error2("gethostbyname(\"%s\"): %s", name,
	     h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
      return -1;
   }
   switch (hent->h_addrtype) {
   case AF_INET: 
      memcpy(&((struct sockaddr_in *)sin)->sin_addr, hent->h_addr_list[0], 4);
      break;
#if WITH_IP6
   case AF_INET6:
      memcpy(&((struct sockaddr_in6 *)sin)->sin6_addr,hent->h_addr_list[0], 16);
      break;
#endif
   default:
      Error1("gethostbyname(\"%s\"): unsupported address type", name);
      return -1;
   }
   return 0;
}

/* detect following address types, all with or without port:
   short-hostname
   fq-hostname
   IP4addr
   IP4addr.port

   fq-hostname:port
   IP4addr:port
   short-hostname:port
   IP6addrA
   IP6addrA:port
   IP6addrB
   IP6addrB:port
   IP6addrC
   IP6addrC:port

   where hostname may resolve to IP4 or IP6 address
   where IP6addrA is x1:x2:x3:x4:x5:x6:x7:x8
   where IP6addrB is x1::x8
   where IP6addrC is x1::IP4addr

   conflict: IP6addrB vs. IP6addrB:port when withport == 0;
      resolves to first form
 */
/* function currently not in use */
int parseipaddr(char *addr,
		struct sockaddr *out,
		const char *protoname,
		int withaddr,
		int withport	/* -1: no port;  0: port allowed;
				   1: port required */
		) {
   struct sockaddr_in  *sin4 = (struct sockaddr_in  *)out;
   struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)out;
   char *colon, *point, *a1;
   bool hasport;

   colon = strchr(addr, ':');
   point = strchr(addr, '.');
   if (colon == NULL) {
      if (point == NULL) {
	 /* short hostname, no port */
	 gethostaddr(addr, out);
	 hasport = false;
      } else {
	 /* IP4 address with or without port, or fq hostname w/o port */
	 if (isdigit(addr[0]&0xff)) {
	    /* IP4 address with or without port; resolving manually because
	       gethostbyname() does not allow appended ".port" */
	    sin4->sin_family = AF_INET;
	    a1 = addr;
	    ((unsigned char *)&sin4->sin_addr)[0] = strtoul(a1, &a1, 10);
	    if (*a1++ != '.')  {
	       Error1("error in IP4 address \"%s\"", a1); return -1; }
	    ((unsigned char *)&sin4->sin_addr)[1] = strtoul(a1, &a1, 10);
	    if (*a1++ != '.')  {
	       Error1("error in IP4 address \"%s\"", a1); return -1; }
	    ((unsigned char *)&sin4->sin_addr)[2] = strtoul(a1, &a1, 10);
	    if (*a1++ != '.')  {
	       Error1("error in IP4 address \"%s\"", a1); return -1; }
	    ((unsigned char *)&sin4->sin_addr)[3] = strtoul(a1, &a1, 10);
	    if (*a1++ == '.')  {
	       sin4->sin_port = htons(strtoul(a1, &a1, 10));
	       hasport = true;
	    } else {
	       hasport = false;
	    }
	    if (*a1 != '\0') {
	       Error1("trailing data in IP4 address \"%s\"", a1); }
	 } else {
	     gethostaddr(addr, out);
	     hasport = false;
	 }
      }
   } else /* colon != NULL */ {
      if (point == NULL) {
	 /* short hostname with port or IP6 address with or without port */
	 char *colon2 = strchr(colon+1, ':');
	 if (colon2 == NULL) {
	    /* only one colon, no point: short hostname with port */
	    char *ap = colon2 + 1;
	    *colon2 = '\0';
	    gethostaddr(addr, out);
	    switch (out->sa_family) {
	    case AF_INET:
	       sin4->sin_port = htons(strtoul(ap, &ap, 10));
	       break;
	    case AF_INET6:
	       sin6->sin6_port = htons(strtoul(ap, &ap, 10));
	       break;
	    default:
	       return -1;
	    }
	    hasport = true;
	 } else {
	    /* no point, two or more colons: IP6 address with or without port*/
	    parseip6addr(addr, sin6, protoname, withaddr, withport);
	 }
      } else {
	 /* colon(s) and point(s) */
	 if (point < colon) {
	    /* hostname or IP4 address, with port */
	    char *ap = colon + 1;
	    *colon = '\0';
	    gethostaddr(addr, out);
	    sin4->sin_port = htons(strtoul(ap, &ap, 10));
	    hasport = true;
	 } else /* colon < point */ {
	    /* IP6 address with included IP4 address, with or without port */
	    parseip6addr(addr, sin6, protoname, withaddr, withport);
	 }
      }
   }
   return 0;
}
#endif /* WITH_IP */


int parserange(char *rangename, int pf, byte4_t *netaddr_in, byte4_t *netmask_in) {
   struct hostent *maskaddr;
   char *delim;
   int bits;

   if (delim = strchr(rangename, '/')) {
      int i;
      bits = strtoul(delim+1, NULL, 10);
      *netmask_in = 0xffffffff;
      for (i = 32; i > bits; --i)
         *netmask_in <<= 1;
   } else if (delim = strchr(rangename, ':')) {
      if ((maskaddr = Gethostbyname(delim+1)) == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", delim+1,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	 return STAT_NORETRY;
      }
      *netmask_in = *(unsigned long *)maskaddr->h_addr_list[0];
   } else {
      Error1("parserange(\"%s\",,): missing netmask delimiter", rangename);
      return -1;
   }
   *delim = 0;
   switch (pf) {
      struct hostent *nameaddr;
#if WITH_IP4
   case PF_INET:
      if ((nameaddr = Gethostbyname(rangename)) == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", rangename,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	 return STAT_NORETRY;
      }
      *netaddr_in = ntohl(*(unsigned long *)nameaddr->h_addr_list[0]);
      *netaddr_in &= *netmask_in;
#endif /* WITH_IP4 */
   }
   return 0;
}

#endif /* WITH_IP4 || WITH_IP6 */
