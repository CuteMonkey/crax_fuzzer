/* $Id: xio-stdio.c,v 1.11 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses stdio type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-fdnum.h"
#include "xio-stdio.h"


#if WITH_STDIO

static int xioopen_stdio(char *arg, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_stdio  = { "stdio",  3, xioopen_stdio, GROUP_FD, 0, 0, NULL HELP(NULL) };
const struct addrdesc addr_stdin  = { "stdin",  1, xioopen_fd, GROUP_FD, 0, 0, NULL HELP(NULL) };
const struct addrdesc addr_stdout = { "stdout", 2, xioopen_fd, GROUP_FD, 1, 0, NULL HELP(NULL) };
const struct addrdesc addr_stderr = { "stderr", 2, xioopen_fd, GROUP_FD, 2, 0, NULL HELP(NULL) };


/* process a bidirectional "stdio" or "-" argument with options.
   generate a dual address. */
int xioopen_stdio_bi(char *arg, xiofile_t *sock) {
   struct opt *opts = NULL, *opts1, *opts2, *optspr;
   /*0 char *b;*/
   unsigned int groups1, groups2;
   int result;

   if (xioopen_makedual(sock) < 0) {
      return -1;
   }

   sock->dual.stream[0]->common.tag = XIO_TAG_RDONLY;
   sock->dual.stream[0]->stream.fd = 0 /*stdin*/;
   sock->dual.stream[1]->common.tag = XIO_TAG_WRONLY;
   sock->dual.stream[1]->stream.fd = 1 /*stdout*/;
   sock->dual.stream[0]->stream.howtoend =
      sock->dual.stream[1]->stream.howtoend = END_NONE;
#if 0
   if ((b = strdup(arg)) == NULL) {
      Error1("strdup("F_Zu"): out of memory", strlen(arg)+1);
      return -1;
   }
#endif

#if WITH_TERMIOS
   if (Isatty(sock->dual.stream[0]->stream.fd)) {
      if (Tcgetattr(sock->dual.stream[0]->stream.fd,
		    &sock->dual.stream[0]->stream.savetty)
	  < 0) {
	 Warn2("cannot query current terminal settings on fd %d: %s",
	       sock->dual.stream[0]->stream.fd, strerror(errno));
      } else {
	 sock->dual.stream[0]->stream.ttyvalid = true;
      }
   }
   if (Isatty(sock->dual.stream[1]->stream.fd)) {
      if (Tcgetattr(sock->dual.stream[1]->stream.fd,
		    &sock->dual.stream[1]->stream.savetty)
	  < 0) {
	 Warn2("cannot query current terminal settings on fd %d: %s",
	       sock->dual.stream[1]->stream.fd, strerror(errno));
      } else {
	 sock->dual.stream[1]->stream.ttyvalid = true;
      }
   }
#endif /* WITH_TERMIOS */

   groups1 = groupbits(0);
   groups2 = groupbits(1);
   if (parseopts(arg, GROUP_FD|groups1|groups2, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   /* options here are one-time and one-direction, no second use */
   retropt_bool(opts, OPT_IGNOREEOF, &sock->dual.stream[0]->stream.ignoreeof);

   /* extract opts that should be applied only once */
   if ((optspr = copyopts(opts, GROUP_PROCESS)) == NULL) {
      return -1;
   }
   if ((result = applyopts(-1, optspr, PH_PREOPEN)) < 0)
      return result;

   /* here we copy opts, because most have to be applied twice! */
   if ((opts1 = copyopts(opts, GROUP_FD|GROUP_APPL|(groups1&~GROUP_PROCESS))) == NULL) {
      return -1;
   }

   /* apply options to first FD */
   if ((result = applyopts(sock->dual.stream[0]->stream.fd, opts1, PH_ALL)) < 0) {
      return result;
   }
   if ((result = _xio_openlate(&sock->dual.stream[0]->stream, opts1)) < 0) {
      return result;
   }

   if ((opts2 = copyopts(opts, GROUP_FD|GROUP_APPL|(groups2&~GROUP_PROCESS))) == NULL) {
      return -1;
   }
   /* apply options to second FD */
   if ((result = applyopts(sock->dual.stream[1]->stream.fd, opts2, PH_ALL)) < 0) {
      return result;
   }
   if ((result = _xio_openlate(&sock->dual.stream[1]->stream, opts2)) < 0) {
      return result;
   }

   if ((result = _xio_openlate(&sock->dual.stream[1]->stream, optspr)) < 0) {
      return result;
   }

   Notice("reading from and writing to stdio");
   return 0;
}


/* wrap around unidirectional xioopensingle and xioopen_fd to automatically determine stdin or stdout fd depending on rw.
   Do not set FD_CLOEXEC flag. */
static int xioopen_stdio(char *arg, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   Notice2("using %s for %s",
	   &("stdin\0\0\0stdout\0\0stderr"[rw<<3]),
	   ddirection[rw]);
   return xioopen_fd(arg, rw, fd, groups, rw, dummy2, dummyp1);
}
#endif /* WITH_STDIO */
