/* $Id: sysutils.h,v 1.12 2003/06/26 12:06:38 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2002 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __sysutils_h_included
#define __sysutils_h_included 1

#if WITH_SOCKET
union sockaddr_union {
   struct sockaddr soa;
#if WITH_UNIX
   struct sockaddr_un un;
#endif /* WITH_UNIX */
#if WITH_IP4
   struct sockaddr_in ip4;
#endif /* WITH_IP4 */
#if WITH_IP6
   struct sockaddr_in6 ip6;
#endif /* WITH_IP6 */
} ;
#endif /* WITH_SOCKET */

#if WITH_SOCKET
extern socklen_t socket_init(int af, union sockaddr_union *sa);
#endif
#if WITH_UNIX
extern void socket_un_init(struct sockaddr_un *sa);
#endif /* WITH_UNIX */
#if WITH_IP4
extern void socket_in_init(struct sockaddr_in *sa);
#endif /* WITH_IP4 */
#if WITH_IP6
extern void socket_in6_init(struct sockaddr_in6 *sa);
#endif /* WITH_IP4 */

#if WITH_SOCKET
extern char *sockaddr_info(const struct sockaddr *sa, char *buff, size_t blen);
#endif
#if WITH_UNIX
extern char *sockaddr_unix_info(const struct sockaddr_un *sa, char *buff, size_t blen);
#endif /* WITH_UNIX */
#if WITH_IP4
extern char *inet4addr_info(byte4_t addr, char *buff, size_t blen);
extern char *sockaddr_inet4_info(const struct sockaddr_in *sa, char *buff, size_t blen);
#endif /* WITH_IP4 */
#if WITH_IP6
extern char *sockaddr_inet6_info(const struct sockaddr_in6 *sa, char *buff, size_t blen);
#endif /* WITH_IP6 */

extern int getusergroups(const char *user, gid_t *list, size_t *ngroups);

#if !HAVE_HSTRERROR
extern const char *hstrerror(int err);
#endif

#if WITH_IP4
extern int parseip4addr(char *addr, struct sockaddr_in *sa,
			const char *protoname, int withaddr, int withport);
extern int _parseip4addr(const char *hostname, const char *portname,
			 struct sockaddr_in *sa, const char *protoname,
			 int withaddr, int withport);
#endif
extern int parseport(const char *portname, const char *protoname);

#endif /* !defined(__sysutils_h_included) */
