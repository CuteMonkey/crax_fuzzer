/* $Id: sysutils.c,v 1.33 2004/06/20 21:38:44 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* translate socket addresses into human readable form */

#include "config.h"
#include "xioconfig.h"

#include "sysincludes.h"

#include "compat.h"	/* socklen_t */
#include "mytypes.h"
#include "error.h"
#include "sycls.h"
#include "sysutils.h"


#if WITH_UNIX
void socket_un_init(struct sockaddr_un *sa) {
   sa->sun_family      = AF_UNIX;
   memset(sa->sun_path, '\0', UNIX_PATH_MAX);
}
#endif /* WITH_UNIX */

#if WITH_IP4
void socket_in_init(struct sockaddr_in *sa) {
#if HAVE_SOCKADDR_SA_LEN
   sa->sin_len         = sizeof(struct sockaddr_in);
#endif /* HAVE_SOCKADDR_SA_LEN */
   sa->sin_family      = AF_INET;
   sa->sin_port        = 0;
   sa->sin_addr.s_addr = 0;
   sa->sin_zero[0]     = 0;
   sa->sin_zero[1]     = 0;
   sa->sin_zero[2]     = 0;
   sa->sin_zero[3]     = 0;
   sa->sin_zero[4]     = 0;
   sa->sin_zero[5]     = 0;
   sa->sin_zero[6]     = 0;
   sa->sin_zero[7]     = 0;
}
#endif /* WITH_IP4 */

#if WITH_IP6
void socket_in6_init(struct sockaddr_in6 *sa) {
#if HAVE_SOCKADDR_SA_LEN
   sa->sin6_len        = sizeof(struct sockaddr_in6);
#endif /* HAVE_SOCKADDR_SA_LEN */
   sa->sin6_family     = AF_INET6;
   sa->sin6_port       = 0;
   sa->sin6_flowinfo   = 0;
#if HAVE_IP6_SOCKADDR==0
   sa->sin6_addr.s6_addr[0] = 0;
   sa->sin6_addr.s6_addr[1] = 0;
   sa->sin6_addr.s6_addr[2] = 0;
   sa->sin6_addr.s6_addr[3] = 0;
   sa->sin6_addr.s6_addr[4] = 0;
   sa->sin6_addr.s6_addr[5] = 0;
   sa->sin6_addr.s6_addr[6] = 0;
   sa->sin6_addr.s6_addr[7] = 0;
   sa->sin6_addr.s6_addr[8] = 0;
   sa->sin6_addr.s6_addr[9] = 0;
   sa->sin6_addr.s6_addr[10] = 0;
   sa->sin6_addr.s6_addr[11] = 0;
   sa->sin6_addr.s6_addr[12] = 0;
   sa->sin6_addr.s6_addr[13] = 0;
   sa->sin6_addr.s6_addr[14] = 0;
   sa->sin6_addr.s6_addr[15] = 0;
#elif HAVE_IP6_SOCKADDR==1
   sa->sin6_addr.u6_addr.u6_addr32[0] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[1] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[2] = 0;
   sa->sin6_addr.u6_addr.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==2
   sa->sin6_addr.u6_addr32[0] = 0;
   sa->sin6_addr.u6_addr32[1] = 0;
   sa->sin6_addr.u6_addr32[2] = 0;
   sa->sin6_addr.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==3
   sa->sin6_addr.in6_u.u6_addr32[0] = 0;
   sa->sin6_addr.in6_u.u6_addr32[1] = 0;
   sa->sin6_addr.in6_u.u6_addr32[2] = 0;
   sa->sin6_addr.in6_u.u6_addr32[3] = 0;
#elif HAVE_IP6_SOCKADDR==4
   sa->sin6_addr._S6_un._S6_u32[0] = 0;
   sa->sin6_addr._S6_un._S6_u32[1] = 0;
   sa->sin6_addr._S6_un._S6_u32[2] = 0;
   sa->sin6_addr._S6_un._S6_u32[3] = 0;
#elif HAVE_IP6_SOCKADDR==5
   sa->sin6_addr.__u6_addr.__u6_addr32[0] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[1] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[2] = 0;
   sa->sin6_addr.__u6_addr.__u6_addr32[3] = 0;
#endif
}
#endif /* WITH_IP6 */


#if WITH_SOCKET
/* initializes the socket address of the specified address family. Returns the
   length of the specific socket address, or 0 on error. */
socklen_t socket_init(int af, union sockaddr_union *sa) {
   switch (af) {
#if WITH_UNIX
   case AF_UNIX:   socket_un_init(&sa->un);   return sizeof(sa->un);
#endif
#if WITH_IP4
   case AF_INET:   socket_in_init(&sa->ip4);  return sizeof(sa->ip4);
#endif
#if WITH_IP6
   case AF_INET6:  socket_in6_init(&sa->ip6); return sizeof(sa->ip6);
#endif
   default: Error1("unknown address family %d", af);
      memset(sa, 0, sizeof(union sockaddr_union));
      sa->soa.sa_family = af;
      return 0;
   }
}
#endif /* WITH_SOCKET */


#if WITH_SOCKET
char *sockaddr_info(const struct sockaddr *sa, char *buff, size_t blen) {
   char *lbuff = buff;
   char *cp = lbuff;
   int n;

   if ((n = snprintf(cp, blen, "AF=%d ", sa->sa_family)) < 0) {
      Warn1("sockaddr_info(): buffer too short ("F_Zu")", blen);
      *buff = '\0';
      return buff;
   }
   cp += n,  blen -= n;

   switch (sa->sa_family) {
#if WITH_UNIX
   case 0:
   case AF_UNIX:
      snprintf(cp, blen, "\"%s\"", (char *)&sa->sa_data);
      break;
#endif
#if WITH_IP4
   case AF_INET: sockaddr_inet4_info((struct sockaddr_in *)sa, cp, blen);
      break;
#endif
#if WITH_IP6
   case AF_INET6: sockaddr_inet6_info((struct sockaddr_in6 *)sa, cp, blen);
      break;
#endif
   default:
      if ((snprintf(cp, blen,
		    "0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		    sa->sa_data[0], sa->sa_data[1], sa->sa_data[2],
		    sa->sa_data[3], sa->sa_data[4], sa->sa_data[5],
		    sa->sa_data[6], sa->sa_data[7], sa->sa_data[8],
		    sa->sa_data[9], sa->sa_data[10], sa->sa_data[11],
		    sa->sa_data[12], sa->sa_data[13])) < 0) {
	 Warn("sockaddr_info(): buffer too short");
	 *buff = '\0';
	 return buff;
      }
   }
   return lbuff;
}
#endif /* WITH_SOCKET */


#if WITH_UNIX
char *sockaddr_unix_info(const struct sockaddr_un *sa, char *buff, size_t blen) {
   blen = Min(blen, UNIX_PATH_MAX);
   strncpy(buff, sa->sun_path, blen);
   if (strlen(buff) >= blen) {
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_UNIX */

#if WITH_IP4
char *inet4addr_info(byte4_t addr, char *buff, size_t blen) {
   if (snprintf(buff, blen, "%lu.%lu.%lu.%lu",
		addr >> 24, (addr >> 16) & 0xff,
		(addr >> 8) & 0xff, addr & 0xff) < 0) {
      Warn("inet4addr_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_IP4 */

#if WITH_IP4
char *sockaddr_inet4_info(const struct sockaddr_in *sa, char *buff, size_t blen) {
   if (snprintf(buff, blen, "%u.%u.%u.%u:%hu",
		((unsigned char *)&sa->sin_addr.s_addr)[0],
		((unsigned char *)&sa->sin_addr.s_addr)[1],
		((unsigned char *)&sa->sin_addr.s_addr)[2],
		((unsigned char *)&sa->sin_addr.s_addr)[3],
		htons(sa->sin_port)) < 0) {
      Warn("sockaddr_inet4_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_IP4 */

#if WITH_IP6
/* convert the IP6 socket address to human readable form. buff should be at
   least 50 chars long */
char *sockaddr_inet6_info(const struct sockaddr_in6 *sa, char *buff, size_t blen) {
   if (snprintf(buff, blen, "[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]:%hu",
#if HAVE_IP6_SOCKADDR==0
		(sa->sin6_addr.s6_addr[0]<<8)+
		sa->sin6_addr.s6_addr[1],
		(sa->sin6_addr.s6_addr[2]<<8)+
		sa->sin6_addr.s6_addr[3],
		(sa->sin6_addr.s6_addr[4]<<8)+
		sa->sin6_addr.s6_addr[5],
		(sa->sin6_addr.s6_addr[6]<<8)+
		sa->sin6_addr.s6_addr[7],
		(sa->sin6_addr.s6_addr[8]<<8)+
		sa->sin6_addr.s6_addr[9],
		(sa->sin6_addr.s6_addr[10]<<8)+
		sa->sin6_addr.s6_addr[11],
		(sa->sin6_addr.s6_addr[12]<<8)+
		sa->sin6_addr.s6_addr[13],
		(sa->sin6_addr.s6_addr[14]<<8)+
		sa->sin6_addr.s6_addr[15],
#elif HAVE_IP6_SOCKADDR==1
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==2
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==3
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.in6_u.u6_addr16)[7]),
#elif HAVE_IP6_SOCKADDR==4
		(sa->sin6_addr._S6_un._S6_u8[0]<<8)|(sa->sin6_addr._S6_un._S6_u8[1]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[2]<<8)|(sa->sin6_addr._S6_un._S6_u8[3]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[4]<<8)|(sa->sin6_addr._S6_un._S6_u8[5]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[6]<<8)|(sa->sin6_addr._S6_un._S6_u8[7]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[8]<<8)|(sa->sin6_addr._S6_un._S6_u8[9]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[10]<<8)|(sa->sin6_addr._S6_un._S6_u8[11]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[12]<<8)|(sa->sin6_addr._S6_un._S6_u8[13]&0xff),
		(sa->sin6_addr._S6_un._S6_u8[14]<<8)|(sa->sin6_addr._S6_un._S6_u8[15]&0xff),
#elif HAVE_IP6_SOCKADDR==5
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->sin6_addr.__u6_addr.__u6_addr16)[7]),
#endif
		ntohs(sa->sin6_port)) < 0) {
      Warn("sockaddr_inet6_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}
#endif /* WITH_IP6 */

/* fill the list with the supplementary group ids of user.
   caller passes size of list in ngroups, function returns number of groups in
   ngroups.
   function returns 0 if 0 or more groups were found, or 1 if the list is too
   short. */
int getusergroups(const char *user, gid_t *list, size_t *ngroups) {
   struct group *grp;
   size_t i = 0;

   setgrent();
   while (grp = getgrent()) {
      char **gusr = grp->gr_mem;
      while (*gusr) {
	 if (!strcmp(*gusr, user)) {
	    if (i == *ngroups)
	       return 1;
	    list[i++] = grp->gr_gid;
	    break;
	 }
	 ++gusr;
      }
   }
   endgrent();
   *ngroups = i;
   return 0;
}

#if !HAVE_HSTRERROR
const char *hstrerror(int err) {
   static const char *h_messages[] = {
      "success",
      "authoritative answer not found",
      "non-authoritative, host not found, or serverfail",
      "Host name lookup failure",	/* "non recoverable error" */
      "valid name, no data record of requested type" };

   assert(HOST_NOT_FOUND==1);
   assert(TRY_AGAIN==2);
   assert(NO_RECOVERY==3);
   assert(NO_DATA==4);
   if ((err < 0) || err > sizeof(h_messages)/sizeof(const char *)) {
      return "";
   }
   return h_messages[err];
}
#endif /* !HAVE_HSTRERROR */
   

#if WITH_IP4
/* a1 must point to a '\0' terminated string of the forms
   short-hostname
   short-hostname:port
   hostname
   hostname:port
   IP4addr
   IP4addr:port
   Returns 0 on success, -1 on error
 */
int parseip4addr(char *addr,	/* "hostname:port\0" */
		 struct sockaddr_in *sa,	/* must be initialized */
		 const char *protoname,	/* "tcp" or "udp", for service */
		 int withaddr,	/* 0: address allowed;  1: address required */
		 int withport	/* -1: no port;  0: port allowed;
				   1: port required */) {
   char *colon, *ap=NULL;

   /* try to find and split port */
   if ((colon = strchr(addr, ':')) != NULL) {
      *colon = '\0'; ap = colon+1;
   }
   return _parseip4addr(addr, ap, sa, protoname, withaddr, withport);
}

/* parses a tcp4 address that may consist of hostname and/or servicename.
   hostname may also be an IP address, servicename may also be numerical.
   might block, especially during hostname resolution, or with NIS for service
   resolution.
   returns -1 on error or 0 in success. */
int _parseip4addr(const char *hostname,	/* or IP address string 0 terminated */
		  const char *portname,	/* port number or service or NULL */
		  struct sockaddr_in *sa,	/* must be initialized */
		  const char *protoname,	/* "tcp" or "udp", for service */
		  int withaddr,	/* 0: address allowed;  1: address required */
		  int withport	/* -1: no port;  0: port allowed;
				   1: port required */) {
   struct hostent *host;
   
   if ((hostname == NULL || hostname[0] == '\0')) {
      if (withaddr > 0)  Error("hostname required in IP4 address");
   } else {
      struct in_addr in_addr;
#if HAVE_INET_ATON
      if (inet_aton(hostname, &in_addr) != 0) {
	 memcpy(&sa->sin_addr, &in_addr, 4);
      } else
#endif /* HAVE_INET_ATON */
      {
	 if ((host = Gethostbyname(hostname)) == NULL) {
	    Error2("gethostbyname(\"%s\"): %s", hostname,
		   h_errno == NETDB_INTERNAL ? strerror(errno) :
		   hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	    return -1;
	 }
	 if (host->h_addrtype != AF_INET) {
	    Error1("parseip4addr(): \"%s\" does not resolve to IP4", hostname);
	 } else {
	    memcpy(&sa->sin_addr, host->h_addr_list[0], 4);
	 }
      }
   }

   if (portname != NULL && portname[0]) {
#if WITH_TCP || WITH_UDP
      /* port */
      if (withport < 0) {
         Error1("unexpected port specification \"%s\"", portname);
      } else {
	 sa->sin_port = parseport(portname, protoname);
      }
#else
       Error1("unexpected port specification \"%s\"", portname);
#endif /* WITH_TCP || WITH_UDP */
   } else {
     if (withport > 0) {
        Error("missing port specification");
     }
   }
   return 0;
}
#endif /* WITH_IP4 */


#if WITH_TCP || WITH_UDP
/* returns port in network byte order */
int parseport(const char *portname, const char *protoname) {
   struct servent *se;
   char *extra;
   int result;

   if (isdigit(portname[0]&0xff)) {
      result = htons(strtoul(portname, &extra, 0));
      if (*extra != '\0') {
	 Error3("parseport(\"%s\", \"%s\"): extra trailing data \"%s\"",
		portname, protoname, extra);
      }
      return result;
   }

   if ((se = getservbyname(portname, protoname)) == NULL) {
      Error2("cannot resolve service \"%s/%s\"", portname, protoname);
      return 0;
   }

   return se->s_port;
}
#endif /* WITH_TCP || WITH_UDP */
