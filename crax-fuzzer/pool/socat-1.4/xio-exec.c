/* $Id: xio-exec.c,v 1.13 2003/12/07 19:58:00 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of exec type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-progcall.h"
#include "xio-exec.h"

#if WITH_EXEC

static int xioopen_exec(char *arg,
		int rw,	/* O_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, void *dummyp1
		);

const struct addrdesc addr_exec = { "exec",   3, xioopen_exec, GROUP_FD|GROUP_FORK|GROUP_EXEC|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_TERMIOS|GROUP_FIFO|GROUP_PTY|GROUP_PARENT, 0, 0, NULL HELP(":<command-line>") };

const struct optdesc opt_dash = { "dash", "login", OPT_DASH, GROUP_EXEC, PH_PREEXEC, TYPE_BOOL, OFUNC_SPEC };

static int xioopen_exec(char *arg,
		int rw,	/* O_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, void *dummyp1
		) {
   char *comma;
   int status;
   struct opt *opts = NULL;
   bool dash = false;

   if (comma = strchr(arg, ',')) {
      *comma++ = '\0';
   }

   if (parseopts(comma, groups, &opts) < 0)
      return STAT_NORETRY;

   retropt_bool(opts, OPT_DASH, &dash);

   status = _xioopen_foxec(comma, rw, &fd->stream, groups, &opts);
   if (status < 0)  return status;
   if (status == 0) {	/* child */
      char *a1;
      char **argv = NULL;
      int argc, i;
      char *path = NULL;
      char *tmp;
      int numleft;
      int result;

      /*! Close(something) */
      /* parse command line */
      a1 = arg;
      Debug1("child: args = \"%s\"", a1);
      argv = Malloc(8*sizeof(char *));
      if (argv == NULL)  return STAT_RETRYLATER;
      i = 0;
      while (a1[i] && a1[i] != ' ') {
	 ++i;
      }
      if (a1[i]) {
	 a1[i] = '\0';
	 ++i;
      }
      argv[0] = strrchr(a1, '/');
      if (argv[0] == NULL)  argv[0] = a1;  else  ++argv[0];
      for (argc=1; a1[i]; ++argc) {
	 /*Debug3("child: argc = %d, a1 = %s", argc, i, a1);*/
	 if ((argc & 0x07) == 0) {
	    argv = Realloc(argv, (argc+8)*sizeof(char *));
	    if (argv == NULL)  return STAT_RETRYLATER;
	 }
	 argv[argc] = a1+i;
	 while (a1[i] && a1[i] != ' ') {
	    /*fprintf(stderr, "i = %d\n", i);*/
	    ++i;
	 }
	 if (a1[i]) {
	    a1[i] = '\0';
	    ++i;
	 }
      }
      argv[argc] = NULL;

      if ((tmp = Malloc(strlen(argv[0])+2)) == NULL) {
	 return STAT_RETRYLATER;
      }
      if (dash) {
	 tmp[0] = '-';
	 strcpy(tmp+1, argv[0]);
      } else {
	 strcpy(tmp, argv[0]);
      }
      argv[0] = tmp;

      if (setopt_path(opts, &path) < 0) {
	 /* this could be dangerous, so let us abort this child... */
	 Exit(1);
      }

      if ((numleft = leftopts(opts)) > 0) {
	 Error1("%d option(s) could not be used", numleft);
	 showleft(opts);
	 return STAT_NORETRY;
      }

      Notice1("execvp'ing \"%s\"", a1);
      result = Execvp(a1, argv);
      Error2("execvp(\"%s\", ...): %s", argv[0], strerror(errno));
      Exit(1);	/* this child process */
   }

   /* parent */
   return 0;
}
#endif /* WITH_EXEC */
