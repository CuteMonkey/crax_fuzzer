/* $Id: xioconfig.h,v 1.25 2004/06/20 21:25:17 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xioconfig_h_included
#define __xioconfig_h_included 1

/* ensure some dependencies  between configure WITH defines. must be included
   past config.h */

#if WITH_STDIO || WITH_FDNUM
#  define WITH_FD 1
#endif

#if WITH_FILE || WITH_GOPEN || WITH_CREAT || WITH_PIPE
#  define WITH_OPEN 1
#endif

#if WITH_OPEN || WITH_PIPE || WITH_UNIX || WITH_PTY
#  define WITH_NAMED 1
#endif

#if WITH_SOCKS4A
#  define WITH_SOCKS4 1
#endif

#if WITH_SOCKS4 || WITH_PROXY
#  define WITH_TCP 1
#  define WITH_IP4 1	/* currently this socks implementation does not work
			   with IP6 */
#endif

#if WITH_OPENSSL
#  define WITH_TCP 1
#  define WITH_IP4 1
#endif

#if WITH_IP6
#  if !defined(HAVE_NETINET_IP6_H)
#    undef WITH_IP6
#  endif
#endif

#if !WITH_IP4 && !WITH_IP6
#  if WITH_TCP || WITH_UDP
#    define WITH_IP4 1
#  endif
#endif

#if WITH_UNIX || WITH_IP4 || WITH_IP6 || WITH_SOCKS4 || WITH_RAWIP
#  define WITH_SOCKET 1
#else
#  undef WITH_SOCKET
#endif

#if !WITH_SOCKET
#  undef WITH_LISTEN
#endif

#if !WITH_LISTEN
#  undef WITH_LIBWRAP
#endif

#if HAVE_DEV_PTMX && HAVE_GRANTPT && HAVE_UNLOCKPT && HAVE_PTSNAME
#else
#  undef HAVE_DEV_PTMX
#endif

#if HAVE_DEV_PTC /* && HAVE_GRANTPT && HAVE_UNLOCKPT && HAVE_PTSNAME */
#else
#  undef HAVE_DEV_PTC
#endif


/* MacOS does not seem to have any pty implementation */
#if WITH_PTY && (HAVE_DEV_PTC || HAVE_DEV_PTMX || HAVE_OPENPTY)
#  define HAVE_PTY 1
#else
#  undef HAVE_PTY
#endif

#ifndef HAVE_TYPE_SOCKLEN
   typedef int socklen_t;
#endif /* !defined(HAVE_TYPE_SOCKLEN) */

#ifndef HAVE_TYPE_UINT8
   typedef unsigned char uint8_t;
#endif

#ifndef HAVE_TYPE_UINT16
   typedef unsigned short uint16_t;
#endif

#ifndef HAVE_TYPE_UINT32
   typedef unsigned int uint32_t;
#endif

#endif /* !defined(__xioconfig_h_included) */
