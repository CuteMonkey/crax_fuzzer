/* $Id: sysincludes.h,v 1.14 2004/06/20 21:37:27 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __sysincludes_h_included
#define __sysincludes_h_included 1

#if HAVE_LIMITS_H
#include <limits.h>	/* USHRT_MAX */
#endif
#include <math.h>	/* HUGE_VAL */
#include <assert.h>
#include <stdarg.h>	/* for msg() */
#include <string.h>	/* strerror(), strchr() */
#if HAVE_STRINGS_H
#include <strings.h>	/* strcasecmp(), bzero() for FD_ZERO */
#endif
#include <stdlib.h>	/* malloc(), free() */
#include <ctype.h>	/* isdigit() */
#include <stdio.h>	/* FILE */
#include <errno.h>	/* errno */
#if HAVE_SYSLOG_H
#include <syslog.h>	/* openlog(), syslog(), closelog() */
#endif
#include <signal.h>	/* signal(), SIGPIPE, SIG_IGN */
#include <time.h>	/* struct timeval, strftime() */
#include <sys/timeb.h>	/* struct timeb */
#if HAVE_UNISTD_H
#include <unistd.h>	/* select(), read(), write(), stat(), fork() */
#endif
#if HAVE_PWD_H
#include <pwd.h>	/* getpwnam() */
#endif
#if HAVE_GRP_H
#include <grp.h>	/* getgrnam() */
#endif
#if HAVE_PTY_H
#include <pty.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>	/* Linux 2.4 NGROUPS */
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>	/* select(); OpenBSD: struct timespec */
#endif
#if HAVE_STDINT_H
#include <stdint.h>	/* uint8_t */
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>	/* pid_t, select(), socket(), connect(), open(), u_short */
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>	/* struct sockaddr, struct linger, socket(), connect() */
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>	/* struct stat, stat(), open() */
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>	/* WNOHANG */
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>	/* open(), O_RDWR */
#endif
#if HAVE_NETDB_H && (WITH_IP4 || WITH_IP6)
#include <netdb.h>	/* struct hostent, gethostbyname() */
#endif
#if HAVE_SYS_UN_H && WITH_UNIX
#include <sys/un.h>	/* struct sockaddr_un, unix domain sockets */
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>	/* ioctl() */
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>	/* select(), fdset on AIX 4.1 */
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>	/* LOCK_EX, on AIX directly included */
#endif
#if WITH_SOCKET
#  if HAVE_NETINET_IN_H
#include <netinet/in.h>	/* struct sockaddr_in, htonl() */
#  endif
#endif /* WITH_SOCKET */
#if WITH_SOCKET && (WITH_IP4 || WITH_IP6)
#  if HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>	/* Solaris, FreeBSD: n_long */
#  endif
#  if HAVE_NETINET_IP_H
#include <netinet/ip.h>	/* struct ip - past netinet/in.h on AIX! */
#  endif
#  if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>	/* TCP_RFC1323 */
#  endif
#  if HAVE_NETINET_IP6_H && WITH_IP6
#include <netinet/ip6.h>
#  endif
#  if HAVE_NETINET6_IN6_H && WITH_IP6
#include <netinet6/in6.h>
#  endif
#include <arpa/inet.h>	/* Linux: inet_aton() */
#endif /* WITH_IP4 || WITH_IP6 */
#if HAVE_TERMIOS_H && WITH_TERMIOS
#include <termios.h>
#endif
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>	/* uname(), struct utsname */
#endif
#if HAVE_UTIL_H
#include <util.h>		/* NetBSD, OpenBSD openpty() */
#endif
#if HAVE_LIBUTIL_H
#include <libutil.h>		/* FreeBSD openpty() */
#endif
#if HAVE_SYS_STROPTS_H
#include <sys/stropts.h>	/* SunOS I_PUSH ... */
#endif
#if HAVE_REGEX_H
#include <regex.h>
#endif
#if WITH_READLINE
#  if HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#  endif
#  if HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#  endif
#endif /* WITH_READLINE */
#if WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#endif /* !defined(__sysincludes_h_included) */
