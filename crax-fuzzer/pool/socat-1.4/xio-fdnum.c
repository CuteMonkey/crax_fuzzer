/* $Id: xio-fdnum.c,v 1.8 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of fdnum type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-fdnum.h"


#if WITH_FDNUM

static int xioopen_fdnum(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp3);


const struct addrdesc addr_fd     = { "fd",     3, xioopen_fdnum, GROUP_FD, 0, 0, NULL HELP(":<num>") };


/* use some file descriptor and apply the options. Set the FD_CLOEXEC flag. */
static int xioopen_fdnum(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp3) {
   int numfd;
   int result;

   numfd = strtoul(a1, &a1, 0);
   if (*a1 != '\0' && *a1 != ',') {
      Error1("error in FD number \"%s\"", a1);
   }
   /* we dont want to see these fds in child processes */
   if (Fcntl_l(numfd, F_SETFD, FD_CLOEXEC) < 0) {
      Warn2("fcntl(%d, F_SETFD, FD_CLOEXEC): %s", numfd, strerror(errno));
   }
   Notice2("using file descriptor %d for %s", numfd, ddirection[rw]);
   if ((result = xioopen_fd(a1, rw, fd, groups, numfd, dummy2, dummyp3)) < 0) {
      return result;
   }
   return 0;
}

#endif /* WITH_FDNUM */

#if WITH_FD

/* retrieve and apply options to a standard file descriptor.
   Do not set FD_CLOEXEC flag. */
int xioopen_fd(char *arg, int rw, xiofile_t *xfd, unsigned groups, int numfd, int dummy2, void *dummyp1) {
   struct opt *opts = NULL;

   xfd->stream.fd = numfd;
   xfd->stream.howtoend = END_NONE;

#if WITH_TERMIOS
   if (Isatty(xfd->stream.fd)) {
      if (Tcgetattr(xfd->stream.fd, &xfd->stream.savetty) < 0) {
	 Warn2("cannot query current terminal settings on fd %d: %s",
	       xfd->stream.fd, strerror(errno));
      } else {
	 xfd->stream.ttyvalid = true;
      }
   }
#endif /* WITH_TERMIOS */
   if (parseopts(arg, groups|groupbits(xfd->stream.fd), &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   applyopts2(xfd->stream.fd, opts, PH_INIT, PH_FD);

   return _xio_openlate(&xfd->stream, opts);
}

#endif /* WITH_FD */
