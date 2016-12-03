/* $Id: xioread.c,v 1.21 2004/06/20 21:19:02 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended read function */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-termios.h"
#include "xio-readline.h"
#include "xio-openssl.h"

 
/* xioread() performs read() or recvfrom() */
ssize_t xioread(xiofile_t *file, void *buff, size_t bufsiz) {
   ssize_t bytes;
#if WITH_IP6 && 0
   int nexthead;
#endif
   struct single *pipe;

   if (file->tag == XIO_TAG_INVALID) {
      Error("xioread(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

   if (file->tag == XIO_TAG_DUAL) {
      pipe = &file->dual.stream[0]->stream;
      if (pipe->tag == XIO_TAG_INVALID) {
	 Error("xioread(): invalid file descriptor");
	 errno = EINVAL;
	 return -1;
      }
   } else {
      pipe = &file->stream;
   }

   if (pipe->dtype == DATA_STREAM || pipe->dtype == DATA_PIPE ||
       pipe->dtype == DATA_2PIPE) {
      do {
	 bytes = Read(pipe->fd, buff, bufsiz);
      } while (bytes < 0 && errno == EINTR);
      if (bytes < 0) {
	 Error4("read(%d, %p, "F_Zu"): %s",
		pipe->fd, buff, bufsiz, strerror(errno));
	 return -1;
      }
   } else if (pipe->dtype == DATA_PTY) {
      do {
	 bytes = Read(pipe->fd, buff, bufsiz);
      } while (bytes < 0 && errno == EINTR);
      if (bytes < 0) {
	 if (errno == EIO) {
	    Notice4("read(%d, %p, "F_Zu"): %s (probably PTY closed)",
		    pipe->fd, buff, bufsiz, strerror(errno));
	    return 0;
	 } else {
	    Error4("read(%d, %p, "F_Zu"): %s",
		   pipe->fd, buff, bufsiz, strerror(errno));
	 }
	 return -1;
      }
#if WITH_READLINE
   } else if (pipe->dtype == DATA_READLINE) {
      return xioread_readline(pipe, buff, bufsiz);
#endif /* WITH_READLINE */
#if WITH_OPENSSL
   } else if (pipe->dtype == DATA_OPENSSL) {
      /* this function prints its error messages */
      return xioread_openssl(pipe, buff, bufsiz);
#endif /* WITH_OPENSSL */
   } else {
#if WITH_RAWIP
      union sockaddr_union from;
      socklen_t fromlen = sizeof(from);

      do {
	 bytes =
	    Recvfrom(pipe->fd, buff, bufsiz, 0, &from.soa, &fromlen);
      } while (bytes < 0 && errno == EINTR);
      if (bytes < 0) {
	 char infobuff[256];
	 Error6("recvfrom(%d, %p, "F_Zu", 0, %s, "F_socklen"): %s",
		pipe->fd, buff, bufsiz,
		sockaddr_info(&from.soa, infobuff, sizeof(infobuff)),
		&fromlen, strerror(errno));
	 return -1;
      }
      switch(from.soa.sa_family) {
	 int headlen;
#if WITH_IP4
      case AF_INET:
	 headlen = 4*((struct ip *)buff)->ip_hl;
	 if (headlen > bytes) {
	    Warn1("xioread(%d, ...)/IP4: short packet", pipe->fd);
	    bytes = 0;
	 } else {
	    memmove(buff, ((char *)buff)+headlen, bytes-headlen);
	    bytes -= headlen;
	 }
	 break;
#endif
#if WITH_IP6
      case AF_INET6: /* does not seem to include header... */
	 break;
#endif
      default:
	 /* do nothing, for now */
	 break;
      }
#if 0
      if (fromlen != pipe->fd[0].salen) {
	 Debug("recvfrom(): wrong peer address length, ignoring packet");
	 continue;
      }
      if (memcmp(&from, &pipe->fd[0].peersa.sa, fromlen)) {
	 Debug("recvfrom(): other peer address, ignoring packet");
	 Debug16("peer: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		 pipe->fd[0].peersa.space[0],
		 pipe->fd[0].peersa.space[1],
		 pipe->fd[0].peersa.space[2],
		 pipe->fd[0].peersa.space[3],
		 pipe->fd[0].peersa.space[4],
		 pipe->fd[0].peersa.space[5],
		 pipe->fd[0].peersa.space[6],
		 pipe->fd[0].peersa.space[7],
		 pipe->fd[0].peersa.space[8],
		 pipe->fd[0].peersa.space[9],
		 pipe->fd[0].peersa.space[10],
		 pipe->fd[0].peersa.space[11],
		 pipe->fd[0].peersa.space[12],
		 pipe->fd[0].peersa.space[13],
		 pipe->fd[0].peersa.space[14],
		 pipe->fd[0].peersa.space[15]);
	 Debug16("from: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		 from.space[0], from.space[1],
		 from.space[2], from.space[3],
		 from.space[4], from.space[5],
		 from.space[6], from.space[7],
		 from.space[8], from.space[9],
		 from.space[10], from.space[11],
		 from.space[12], from.space[13],
		 from.space[14], from.space[15]);
	 continue;
      }
#endif
#else /* !WITH_RAWIP */
      Fatal("address requires raw sockets, but they are not compiled in");
      return -1;
#endif /* !WITH_RAWIP */
   }
   return bytes;
}
