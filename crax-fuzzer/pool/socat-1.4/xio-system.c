/* $Id: xio-system.c,v 1.10 2003/12/07 19:58:00 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of system type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-progcall.h"
#include "xio-system.h"


#if WITH_SYSTEM

static int xioopen_system(char *arg,
		int rw,	/* O_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, void *dummyp1
		);

const struct addrdesc addr_system = { "system", 3, xioopen_system, GROUP_FD|GROUP_FORK|GROUP_EXEC|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_TERMIOS|GROUP_FIFO|GROUP_PTY|GROUP_PARENT, 1, 0, NULL HELP(":<shell-command>") };


static int xioopen_system(char *arg,
		int rw,	/* O_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, void *dummyp1
		) {
   char *comma;
   int status;
   struct opt *opts = NULL;
   char *path = NULL;
   int result;
   char *string = arg;

   if (comma = strchr(arg, ',')) {
      *comma++ = '\0';
   }

   if (parseopts(comma, groups, &opts) < 0)
      return STAT_NORETRY;

   status = _xioopen_foxec(comma, rw, &fd->stream, groups, &opts);
   if (status < 0)  return status;
   if (status == 0) {	/* child */
      int numleft;

      if (setopt_path(opts, &path) < 0) {
	 /* this could be dangerous, so let us abort this child... */
	 Exit(1);
      }

      if ((numleft = leftopts(opts)) > 0) {
	 Error1("%d option(s) could not be used", numleft);
	 showleft(opts);
	 return STAT_NORETRY;
      }

      Info1("executing shell command \"%s\"", string);
      result = System(string);
      if (result != 0) {
	 Warn2("system(\"%s\") returned with status %d", string, result);
      }
      Exit(0);	/* this child process */
   }

   /* parent */
   return 0;
}

#endif /* WITH_SYSTEM */
