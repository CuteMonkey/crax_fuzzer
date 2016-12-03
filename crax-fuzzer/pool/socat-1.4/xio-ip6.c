/* $Id: xio-ip6.c,v 1.20 2004/06/20 21:34:42 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for IP6 related functions */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-ip6.h"

#if WITH_IP6


/* addr must be 0 terminated; withport tells if the number past the last colon
   is expected to be a (TCP or UDP) port number */
int parseip6addr(char *addr,	/* socket address, 0 terminated */
		 struct sockaddr_in6 *sa,	/* must be initialized */
		 const char *protoname,	/* "tcp" or "udp", for service */
		 int withaddr,	/* 0: address allowed;  1: address required */
		 int withport	/* -1: no port;  0: port allowed;
				   1: port required */
		 ) {
   char *a = addr;
   int i = 0, j = -1, k;
   unsigned short words[9];	/* last might be port */
   int n;
#if !HAVE_GETADDRINFO
   struct hostent *host;
#endif
   char *colon, *ap=NULL;

   while (*a) {
      if (*a == ':')  ++i;
      ++a;
   }
   a = addr;

   if (i == 0) {
      /* symbolic hostname, no port */
#if HAVE_GETADDRINFO
      struct addrinfo hints;
      struct addrinfo *res;
      int error_num;
      hints.ai_flags = 0;
      hints.ai_family = AF_INET6;
      if (!strcmp("tcp", protoname)) {
	 hints.ai_socktype = SOCK_STREAM;
	 hints.ai_protocol = IPPROTO_TCP;
      } else {
	 hints.ai_socktype = SOCK_DGRAM;
	 hints.ai_protocol = IPPROTO_UDP;
      }
      hints.ai_addrlen = 0;
      hints.ai_addr = NULL;
      hints.ai_canonname = NULL;
      hints.ai_next = NULL;
      if ((error_num = Getaddrinfo(addr, NULL, &hints, &res)) != 0) {
	 Error7("getaddrinfo(\"%s\", NULL, {%d,%d,%d,%d,"F_Zu",...}, {}): %s",
		addr, hints.ai_flags, hints.ai_family, hints.ai_socktype,
		hints.ai_protocol, hints.ai_addrlen, gai_strerror(error_num));
	 if (res != NULL)  freeaddrinfo(res);
	 return STAT_RETRYLATER;
      }

      memcpy(sa, res->ai_addr, res->ai_addrlen);
      freeaddrinfo(res);

#elif HAVE_GETIPNODEBYNAME

      /* first fallback is getipnodebyname() */
      host = Getipnodebyname(addr, AF_INET6, AI_V4MAPPED, &error_num);
      if (host == NULL) {
	 const static char ai_host_not_found = "Host not found";
	 const static char ai_no_address     = "No address";
	 const static char ai_no_recovery    = "No recovery";
	 const static char ai_try_again      = "Try again";
	 char *error_msg = "Unknown error";
	 switch (error_num) {
	 case HOST_NOT_FOUND: error_msg = ai_host_not_found; break;
	 case NO_ADDRESS:     error_msg = ai_no_address;
	 case NO_RECOVERY:    error_msg = ai_no_recovery;
	 case TRY_AGAIN:      error_msg = ai_try_again;
	 }
	 Error2("getipnodebyname(\"%s\", ...): %s", addr, error_msg);
      } else {
	 memcpy(&sa->sin6_addr, host->h_addr_list[0], 16);
      }
      freehostent(host);

#else /* !HAVE_GETIPNODEBYNAME */

      /* this is not a typical IP6 resolver function - but Linux
	 "man gethostbyname" says that the only supported address type with
	 this function is AF_INET _at present_, so maybe this fallback will
	 be useful somewhere sometimesin a future */
      if ((host = Gethostbyname(addr)) == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", addr,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	 return STAT_RETRYLATER;
      }
      if (host->h_addrtype != AF_INET6) {
	 Error1("parseip6addr(): \"%s\" does not resolve to IP6", addr);
      } else {
	 memcpy(&sa->sin6_addr, host->h_addr_list[0], 16);
      }
#endif /* !HAVE_GETADDRINFO */

   } else if (i == 1) {
      /* symbolic hostname, with port */

#if HAVE_GETADDRINFO
      struct addrinfo hints;
      struct addrinfo *res = NULL;
      int error_num;
#endif /* HAVE_GETADDRINFO */

      if (colon = strchr(addr, ':')) {
	 *colon = '\0';
      }

#if HAVE_GETADDRINFO
      hints.ai_flags = 0;
      hints.ai_family = AF_INET6;
      if (!strcmp("tcp", protoname)) {
	 hints.ai_socktype = SOCK_STREAM;
	 hints.ai_protocol = IPPROTO_TCP;
      } else {
	 hints.ai_socktype = SOCK_DGRAM;
	 hints.ai_protocol = IPPROTO_UDP;
      }
      hints.ai_addrlen = 0;
      hints.ai_addr = NULL;
      hints.ai_canonname = NULL;
      hints.ai_next = NULL;
      /* symbolic hostname, no port */
      if ((error_num = Getaddrinfo(addr, ap, &hints, &res)) != 0) {
	 Error7("getaddrinfo(\"%s\", NULL, {%d,%d,%d,%d,"F_Zu",...}, {}): %s",
		addr, hints.ai_flags, hints.ai_family, hints.ai_socktype,
		hints.ai_protocol, hints.ai_addrlen, gai_strerror(error_num));
	 if (res != NULL)  freeaddrinfo(res);
	 return STAT_RETRYLATER;
      }

      memcpy(sa, res->ai_addr, res->ai_addrlen);
      freeaddrinfo(res);

#elif HAVE_GETIPNODEBYNAME

      /* first fallback is getipnodebyname() */
      host = Getipnodebyname(addr, AF_INET6, AI_V4MAPPED, &error_num);
      if (host == NULL) {
	 const static char ai_host_not_found = "Host not found";
	 const static char ai_no_address     = "No address";
	 const static char ai_no_recovery    = "No recovery";
	 const static char ai_try_again      = "Try again";
	 char *error_msg = "Unknown error";
	 switch (error_num) {
	 case HOST_NOT_FOUND: error_msg = ai_host_not_found; break;
	 case NO_ADDRESS:     error_msg = ai_no_address;
	 case NO_RECOVERY:    error_msg = ai_no_recovery;
	 case TRY_AGAIN:      error_msg = ai_try_again;
	 }
	 Error2("getipnodebyname(\"%s\", ...): %s", addr, error_msg);
      } else {
	 memcpy(&sa->sin6_addr, host->h_addr_list[0], 16);
      }
      freehostent(host);

#else /* !HAVE_GETIPODEBYNAME */

      /*! first fallback should be getipnodebyname() */

      if ((host = Gethostbyname(addr)) == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", addr,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno)/*0 h_messages[h_errno-1]*/);
	 return STAT_RETRYLATER;
      }
      if (host->h_addrtype != AF_INET6) {
	 Error1("parseip6addr(): \"%s\" does not resolve to IP6", addr);
      } else {
	 memcpy(&sa->sin6_addr, host->h_addr_list[0], 16);
      }
#endif /* !HAVE_GETADDRINFO */

      if (colon) {
	 ap = colon + 1;
	 if (withport < 0) {
	    Error1("unexpected port specification \"%s\"", ap);
#if WITH_TCP || WITH_UDP
	 } else {
	    sa->sin6_port = parseport(ap, protoname);
#endif
	 }
      }

   } else {
      /* numeric address specification */
      if (i == 8 || withport == 1) {
	 n = i;		/* hex words we get for address */
	 if ((ap = strrchr(a, ':')) == NULL) {
	    Error1("missing port specification in IP6 address \"%s\"", a);
	    return -1;
	 }
	 *ap++ = 0;
      } else {
	 n = 8;		/* we want all words for hex address */
	 ap = NULL;
      }

      i = 0;
      while (i < n) {
	 if (*a == ':') {
	    if (*(a+1) == ':') {	/* it is "::" */
	       if (j >= 0) {	/* there was already a "::" */
		  addr = a;
		  Error("found second \"::\" in IP6 address");
		  return -1;
	       }
	       j = i;
	       if (i == 0)  --n;
	       ++a, --n;
	    }
	    ++a;
	 } else if (isxdigit(*(unsigned char *)a)) {
	    words[i++] = strtoul(a, (char **)&a, 16);
	 } else if (!*a) {
	    break;
	 } else {
	    addr = a;
	    Error1("unsupported character '0%o' in IP6 address", *a);
	    return -1;
	 }
      }

      if (ap && withport) {
	 sa->sin6_port = htons(strtoul(ap, NULL, 0));
      }
      if (*a) {
	 Error1("extra characters \"%s\" in IP6 address", a);
	 return -1;
      }
      if (j < 0 && i < n) {
	 addr = a;	/* too few words... */
	 Error("too few words in IP6 address");
	 return -1;
      }
      if (withport == 0 && i == 9) {
	 withport = 1;
      }
#if 0
      if (withport > 0) {
	 if (i == 0) {
	    Error("missing port specification in IP6 address");
	    return -1;
	 }
	 sa->sin6_port = htons(words[i-1]);
	 --i; n = 8;
      }
#endif

      for (k=0; k<j; ++k) {
#if HAVE_IP6_SOCKADDR==0
	 sa->sin6_addr.s6_addr[k<<1] = words[k]>>8;
	 sa->sin6_addr.s6_addr[(k<<1)+1] = words[k]&0xff;
#elif HAVE_IP6_SOCKADDR==1
	 sa->sin6_addr.u6_addr.u6_addr16[k] = htons(words[k]);
#elif HAVE_IP6_SOCKADDR==2
	 sa->sin6_addr.u6_addr16[k] = htons(words[k]);
#elif HAVE_IP6_SOCKADDR==3
	 sa->sin6_addr.in6_u.u6_addr16[k] = htons(words[k]);
#elif HAVE_IP6_SOCKADDR==4
	 sa->sin6_addr._S6_un._S6_u8[k<<1] = (words[k]>>8);
	 sa->sin6_addr._S6_un._S6_u8[(k<<1)+1] = (words[k]&0xff);
#elif HAVE_IP6_SOCKADDR==5
	 sa->sin6_addr.__u6_addr.__u6_addr16[k] = htons(words[k]);
#endif
      }
      for (; k<8-(i-j); ++k) {
#if HAVE_IP6_SOCKADDR==0
	 sa->sin6_addr.s6_addr[k<<1] = 0;
	 sa->sin6_addr.s6_addr[(k<<1)+1] = 0;
#elif HAVE_IP6_SOCKADDR==1
	 sa->sin6_addr.u6_addr.u6_addr16[k] = 0;
#elif HAVE_IP6_SOCKADDR==2
	 sa->sin6_addr.u6_addr16[k] = 0;
#elif HAVE_IP6_SOCKADDR==3
	 sa->sin6_addr.in6_u.u6_addr16[k] = 0;
#elif HAVE_IP6_SOCKADDR==4
	 sa->sin6_addr._S6_un._S6_u32[k] = 0;
#elif HAVE_IP6_SOCKADDR==5
	 sa->sin6_addr.__u6_addr.__u6_addr16[k] = 0;
#endif
      }
      for (; k<8; ++k) {
#if HAVE_IP6_SOCKADDR==0
	 sa->sin6_addr.s6_addr[k<<1] = words[k-(8-i)]>>8;
	 sa->sin6_addr.s6_addr[(k<<1)+1] = words[k-(8-i)]&0xff;
#elif HAVE_IP6_SOCKADDR==1
	 sa->sin6_addr.u6_addr.u6_addr16[k] = htons(words[k-(8-i)]);
#elif HAVE_IP6_SOCKADDR==2
	 sa->sin6_addr.u6_addr16[k] = htons(words[k-(8-i)]);
#elif HAVE_IP6_SOCKADDR==3
	 sa->sin6_addr.in6_u.u6_addr16[k] = htons(words[k-(8-i)]);
#elif HAVE_IP6_SOCKADDR==4
	 sa->sin6_addr._S6_un._S6_u8[k<<1]     = (words[k-(8-i)]>>8);
	 sa->sin6_addr._S6_un._S6_u8[(k<<1)+1] = (words[k-(8-i)]&0xff);
#elif HAVE_IP6_SOCKADDR==5
	 sa->sin6_addr.__u6_addr.__u6_addr16[k] = htons(words[k-(8-i)]);
#endif
      }
   }
   return 0;
}

#endif /* WITH_IP6 */
