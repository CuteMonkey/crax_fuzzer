/* $Id: xioclose.c,v 1.18 2004/06/06 17:33:23 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended close function */


#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-termios.h"


/* close the xio fd; must be valid and "simple" */
int xioclose1(struct single *pipe) {

   if (pipe->tag == XIO_TAG_INVALID) {
      Error("xioclose1(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

#if WITH_READLINE
   if (pipe->dtype == DATA_READLINE) {
      Write_history(pipe->para.readline.history_file);
      /*xiotermios_setflag(pipe->fd, 3, ECHO|ICANON);*/	/* error when pty closed */
   }
#endif /* WITH_READLINE */
#if WITH_OPENSSL
   if (pipe->dtype == DATA_OPENSSL) {
      sycSSL_shutdown(pipe->para.openssl.ssl);
      Close(pipe->fd);
      sycSSL_free(pipe->para.openssl.ssl);
      sycSSL_CTX_free(pipe->para.openssl.ctx);
   } else
#endif /* WITH_OPENSSL */
#if WITH_TERMIOS
   if (pipe->ttyvalid) {
      if (Tcsetattr(pipe->fd, 0, &pipe->savetty) < 0) {
	 Warn2("cannot restore terminal settings on fd %d: %s",
	       pipe->fd, strerror(errno));
      }
   }
#endif /* WITH_TERMIOS */
   if (pipe->fd >= 0) {
      switch (pipe->howtoend) {
      case END_KILL: case END_SHUTDOWN_KILL: case END_CLOSE_KILL:
	 if (pipe->para.exec.pid > 0) {
	    if (Kill(pipe->para.exec.pid, SIGTERM) < 0) {
	       Warn2("kill(%d, SIGTERM): %s",
		      pipe->para.exec.pid, strerror(errno));
	    }
	 }
      default:
	 break;
      }
      switch (pipe->howtoend) {
      case END_CLOSE: case END_CLOSE_KILL:
	 if (Close(pipe->fd) < 0) {
	 Warn2("close(%d): %s", pipe->fd, strerror(errno)); } break;
#if WITH_SOCKET
      case END_SHUTDOWN: case END_SHUTDOWN_KILL:
	 if (Shutdown(pipe->fd, 2) < 0) {
	     Warn3("shutdown(%d, %d): %s", pipe->fd, 2, strerror(errno)); }
         break;
#endif /* WITH_SOCKET */
      case END_UNLINK: if (Unlink((const char *)pipe->name) < 0) {
	    Warn2("unlink(\"%s\"): %s", pipe->name, strerror(errno)); }
         break;
      case END_NONE: default: break;
      }
   }
   pipe->tag = XIO_TAG_INVALID;
   return 0;	/*! */
}

/* close the xio fd */
int xioclose(xiofile_t *file) {
   int result;

   if (file->tag == XIO_TAG_INVALID) {
      Error("xioclose(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

   if (file->tag == XIO_TAG_DUAL) {
      result  = xioclose1(&file->dual.stream[0]->stream);
      result |= xioclose1(&file->dual.stream[1]->stream);
      file->tag = XIO_TAG_INVALID;
   } else {
      result = xioclose1(&file->stream);
   }
   return result;
}

