/* $Id: xioshutdown.c,v 1.16 2003/12/23 16:51:08 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended shutdown function */


#include "xiosysincludes.h"
#include "xioopen.h"


int xioshutdown(xiofile_t *sock, int how) {
   int result = 0;

   if (sock->tag == XIO_TAG_INVALID) {
      Error("xioshutdown(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

   if (sock->tag == XIO_TAG_DUAL) {
      if ((how+1)&1) {
	 result = xioshutdown(sock->dual.stream[0], 0);
      }
      if ((how+1)&2) {
	 result |= xioshutdown(sock->dual.stream[1], 1);
      }

#if WITH_OPENSSL
   } else if (sock->stream.dtype == DATA_OPENSSL) {
      sycSSL_shutdown (sock->stream.para.openssl.ssl);
      /*! */
#endif /* WITH_OPENSSL */

   } else if (sock->stream.dtype == DATA_PIPE) {
      if ((how+1)&1) {
	 if (Close(sock->stream.fd) < 0) {
	    Warn2("close(%d): %s",
		  sock->stream.fd, strerror(errno));
	 }
      }
      if ((how+1)&2) {
	 if (Close(sock->stream.para.bipipe.fdout) < 0) {
	    Warn2("close(%d): %s",
		  sock->stream.para.bipipe.fdout, strerror(errno));
	 } 
      }
      
   } else if (sock->stream.dtype == DATA_2PIPE) {
      if ((how+1)&1) {
	 if (Close(sock->stream.fd) < 0) {
	    Warn2("close(%d): %s",
		  sock->stream.fd, strerror(errno));
	 }
      }
      if ((how+1)&2) {
	 if (Close(sock->stream.para.exec.fdout) < 0) {
	    Warn2("close(%d): %s",
		  sock->stream.para.exec.fdout, strerror(errno));
	 } 
      }
      
#if WITH_SOCKET
   } else if (sock->stream.howtoend == END_SHUTDOWN ||
	      sock->stream.howtoend == END_SHUTDOWN_KILL) {
      if ((result = Shutdown(sock->stream.fd, how)) < 0) {
	 Warn3("shutdown(%d, %d): %s",
	       sock->stream.fd, how, strerror(errno));
      }
   } else if (sock->stream.dtype == DATA_RECV) {
      if (how >= 1) {
	 if (Close(sock->stream.fd) < 0) {
	    Warn2("close(%d): %s",
		  sock->stream.fd, strerror(errno));
	 }
	 sock->stream.eof = 1;
	 sock->stream.fd = -1;
      }
#endif /* WITH_SOCKET */
#if 0
   } else {
      Error1("xioshutdown(): bad data type specification %d", sock->stream.dtype);
      return -1;
#endif

   }
#if 0
   else if (sock->stream.howtoend == END_CLOSE &&
	      sock->stream.dtype == DATA_STREAM) {
      return result;
   }
#if WITH_TERMIOS
   if (sock->stream.ttyvalid) {
      if (Tcsetattr(sock->stream.fd, 0, &sock->stream.savetty) < 0) {
	 Warn2("cannot restore terminal settings on fd %d: %s",
	       sock->stream.fd, strerror(errno));
      }
   }
#endif /* WITH_TERMIOS */
#endif

   return result;
}
