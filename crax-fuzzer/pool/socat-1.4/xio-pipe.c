/* $Id: xio-pipe.c,v 1.16 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of pipe type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"


#if WITH_PIPE

static int xioopen_fifo(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_pipe   = { "pipe",   3, xioopen_fifo,  GROUP_FD|GROUP_NAMED|GROUP_OPEN|GROUP_FIFO, 0, 0, NULL HELP(":<filename>") };


/* process an unnamed bidirectional "pipe" or "fifo" or "echo" argument with
   options */
int xioopen_fifo_unnamed(char *arg, xiofile_t *sock) {
   struct opt *opts = NULL, *opts2;
   int filedes[2];
   char *b;
   int numleft;
   int result;

   /* keep groups consistent with help! */
   if (parseopts(arg, GROUP_FD|GROUP_FIFO, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);
   if (Pipe(filedes) != 0) {
      Error2("pipe(%p): %s", filedes, strerror(errno));
      return -1;
   }
   /*0 Info2("pipe({%d,%d})", filedes[0], filedes[1]);*/

   sock->common.tag               = XIO_TAG_RDWR;
   sock->stream.dtype             = DATA_PIPE;
   sock->stream.fd                = filedes[0];
   sock->stream.para.bipipe.fdout = filedes[1];
   if ((b = strdup(arg)) == NULL) {
      Error1("strdup("F_Zu"): out of memory", strlen(arg)+1);
      return -1;
   }
   applyopts_cloexec(sock->stream.fd,                opts);
   applyopts_cloexec(sock->stream.para.bipipe.fdout, opts);

   /* one-time and input-direction options, no second application */
   retropt_bool(opts, OPT_IGNOREEOF, &sock->stream.ignoreeof);

   /* here we copy opts! */
   if ((opts2 = copyopts(opts, GROUP_FIFO)) == NULL) {
      return STAT_NORETRY;
   }

   /* apply options to first FD */
   if ((result = applyopts(sock->stream.fd, opts, PH_ALL)) < 0) {
      return result;
   }
   if ((result = applyopts_single(&sock->stream, opts, PH_ALL)) < 0) {
      return result;
   }

   /* apply options to second FD */
   if ((result = applyopts(sock->stream.para.bipipe.fdout, opts2, PH_ALL)) < 0)
   {
      return result;
   }

   if ((numleft = leftopts(opts)) > 0) {
      Error1("%d option(s) could not be used", numleft);
      showleft(opts);
   }
   Notice("writing to and reading from unnamed pipe");
   return 0;
}


/* open a named pipe/fifo */
static int xioopen_fifo(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   struct opt *opts = NULL;
#if HAVE_STAT64
   struct stat64 pipstat;
#else
   struct stat pipstat;
#endif /* !HAVE_STAT64 */
   bool do_unlink_early = false;
   mode_t mode = 0666;
   int result;
   char *a2;

   a2 = strchr(a1, ',');
   if (a2)  *a2++ = '\0';

   if ((result = parseopts(a2, groups, &opts)) < 0)
      return result;
   applyopts(-1, opts, PH_INIT);

   retropt_bool(opts, OPT_UNLINK_EARLY, &do_unlink_early);
   if (do_unlink_early) {
      if (Unlink(a1) < 0) {
	 if (errno == ENOENT) {
	    Warn2("unlink(%s): %s", a1, strerror(errno));
	 } else {
	    Error2("unlink(%s): %s", a1, strerror(errno));
	    return STAT_RETRYLATER;
	 }
      }
   }

   retropt_modet(opts, OPT_PERM, &mode);
   if (applyopts_named(a1, opts, PH_EARLY) < 0) {
      return STAT_RETRYLATER;
   }
   if (applyopts_named(a1, opts, PH_PREOPEN) < 0) {
      return STAT_RETRYLATER;
   }
   if (
#if HAVE_STAT64
       Stat64(a1, &pipstat) < 0
#else
       Stat(a1, &pipstat) < 0
#endif /* !HAVE_STAT64 */
      ) {
      if (errno != ENOENT) {
	 Error3("stat(\"%s\", %p): %s", a1, &pipstat, strerror(errno));
      } else {
	 Debug1("xioopen_fifo(\"%s\"): does not exist, creating fifo", a1);
#if 0
	 result = Mknod(a1, S_IFIFO|mode, 0);
	 if (result < 0) {
	    Error3("mknod(%s, %d, 0): %s", a1, mode, strerror(errno));
	    return STAT_RETRYLATER;
	 }
#else
	 result = Mkfifo(a1, mode);
	 if (result < 0) {
	    Error3("mkfifo(%s, %d): %s", a1, mode, strerror(errno));
	    return STAT_RETRYLATER;
	 }
#endif
	 Notice2("created named pipe \"%s\" for %s", a1, ddirection[rw]);
      }
   } else {
      /* exists */
      Debug1("xioopen_fifo(\"%s\"): already exist, opening it", a1);
      Notice3("opening %s \"%s\" for %s",
	      filetypenames[(pipstat.st_mode&S_IFMT)>>12],
	      a1, ddirection[rw]);
      /*applyopts_early(a1, opts);*/
      applyopts_named(a1, opts, PH_EARLY);
   }

   if ((result = _xioopen_open(a1, rw, opts)) < 0) {
      return result;
   }
   fd->stream.fd = result;

   applyopts_named(a1, opts, PH_FD);
   applyopts(fd->stream.fd, opts, PH_FD);
   applyopts_cloexec(fd->stream.fd, opts);
   return _xio_openlate(&fd->stream, opts);
}

#endif /* WITH_PIPE */
