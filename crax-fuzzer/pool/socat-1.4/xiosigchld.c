/* $Id: xiosigchld.c,v 1.17 2003/07/14 17:00:17 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended child signal handler */


#include "xiosysincludes.h"
#include "xioopen.h"


/*! */
extern int closing;
extern int sleepon;

/*! with socat, at most 4 exec children exist */
pid_t diedunknown1;	/* child died before it is registered */
pid_t diedunknown2;
pid_t diedunknown3;
pid_t diedunknown4;


/* exec'd child has died, perform appropriate changes to descriptor */
static int sigchld_stream(struct single *file) {
   file->para.exec.pid = 0;
   if (file->ignoreeof && !closing) {
      sleepon = 1;
   } else {
      file->eof = 1; closing = 1;
   }
   return 0;
}

/* return 0 if socket is not responsible for deadchild */
static int xio_checkchild(xiofile_t *socket, int socknum, pid_t deadchild) {
   int retval;
   if (socket != NULL) {
      if (socket->tag != XIO_TAG_DUAL) {
	 if ((socket->stream.howtoend == END_KILL ||
	      socket->stream.howtoend == END_CLOSE_KILL ||
	      socket->stream.howtoend == END_SHUTDOWN_KILL) &&
	     socket->stream.para.exec.pid == deadchild) {
	    Info2("exec'd process %d on socket %d terminated",
		  socket->stream.para.exec.pid, socknum);
	    sigchld_stream(&socket->stream);
	    return 1;
	 }
      } else {
	 if (retval = xio_checkchild(socket->dual.stream[0], socknum, deadchild))
	    return retval;
	 else
	    return xio_checkchild(socket->dual.stream[1], socknum, deadchild);
      }
   }
   return 0;
}

/* this is the signal handler for SIGCHLD */
/* the current socat/xio implementation knows two kinds of children:
   exec/system addresses perform a fork: these children are registered and
   there death influences the parents flow;
   listen-socket with fork children: these children are "anonymous" and their
   death does not effect the parent process (now; maybe we have a child
   process counter later) */
void childdied(int signum) {
   pid_t pid;
   int _errno;
   int status;
   bool wassig = false;

   _errno = errno;	/* save current value; e.g., select() on Cygwin seems
			   to set it to EINTR _before_ handling the signal, and
			   then passes the value left by the signal handler to
			   the caller of select(), accept() etc. */
   Info1("childdied(signum=%d)", signum);
   do {
      pid = Waitpid(-1, &status, WNOHANG);
      if (pid == 0 || (pid < 0 && errno == ECHILD)) {
	 if (!wassig) {
	    Warn("waitpid(-1, {}, WNOHANG): no child process");
	 }
	 Info("childdied() finished");
	 errno = _errno;
	 return;
      }
      wassig = true;
      if (pid < 0) {
	 Warn2("waitpid(-1, {%d}, WNOHANG): %s", status, strerror(errno));
	 Info("childdied() finished");
	 errno = _errno;
	 return;
      }
   if (!xio_checkchild(sock1, 1, pid)) {
      if (!xio_checkchild(sock2, 2, pid)) {
	 Info2("childdied(%d): cannot identify child %d", signum, pid);
	 if (diedunknown1 == 0) {
	    diedunknown1 = pid;
	    Debug("saving pid in diedunknown1");
	 } else if (diedunknown2 == 0) {
	    diedunknown2 = pid;
	    Debug("saving pid in diedunknown2");
	 } else if (diedunknown3 == 0) {
	     diedunknown3 = pid;
	    Debug("saving pid in diedunknown3");
	 } else {
	    diedunknown4 = pid;
	    Debug("saving pid in diedunknown4");
	 }
      }
   }

   if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
	 Info2("waitpid(): child %d exited with status %d",
	       pid, WEXITSTATUS(status));
      } else {
	 Warn2("waitpid(): child %d exited with status %d",
	       pid, WEXITSTATUS(status));
      }
   } else if (WIFSIGNALED(status)) {
      Info2("waitpid(): child %d exited on signal %d",
	    pid, WTERMSIG(status));
   } else if (WIFSTOPPED(status)) {
      Info2("waitpid(): child %d stopped on signal %d",
	    pid, WSTOPSIG(status));
   } else {
      Warn1("waitpid(): cannot determine status of child %d", pid);
   }

#if !HAVE_SIGACTION
   if (Signal(SIGCHLD, childdied) == SIG_ERR) {
      Warn2("signal(SIGCHLD, %p): %s", childdied, strerror(errno));
   }
#endif /* !HAVE_SIGACTION */
  } while (1);
   Info("childdied() finished");
   errno = _errno;
}
