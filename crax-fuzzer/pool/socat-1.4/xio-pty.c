/* $Id: xio-pty.c,v 1.9 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for creating pty addresses */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"
#include "xio-termios.h"


#if WITH_PTY

#define MAXPTYNAMELEN 64

static int xioopen_pty(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);

const struct addrdesc addr_pty = { "pty",   3, xioopen_pty, GROUP_NAMED|GROUP_FD|GROUP_TERMIOS|GROUP_PTY, 0, 0, NULL HELP("") };

const struct optdesc opt_symbolic_link = { "symbolic-link", "link", OPT_SYMBOLIC_LINK, GROUP_PTY, PH_LATE, TYPE_FILENAME, OFUNC_SPEC, 0, 0 };

static int xioopen_pty(char *a1, int rw, xiofile_t *xfd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   /* we expect the form: filename */
   struct opt *opts = NULL;
   int ptyfd = -1, ttyfd = -1;
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   bool useptmx = false;	/* use /dev/ptmx or equivalent */
#endif
#if HAVE_OPENPTY
   bool useopenpty = false;	/* try only openpty */
#endif	/* HAVE_OPENPTY */
   char ptyname[MAXPTYNAMELEN];
   char *linkname = NULL;

   if (parseopts(a1, groups, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   /* trying to set user-early, perm-early etc. here might be useless because
      file system entry is eventually available only past pty creation */
   /* name not yet known; umask should not be handled with this function! */
   /* umask does not affect resulting mode, on Linux 2.4 */
   applyopts_named("", opts, PH_EARLY);	/* umask! */

   applyopts2(-1, opts, PH_INIT, PH_EARLY);

#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   retropt_bool(opts, OPT_PTMX, &useptmx);
#endif
#if HAVE_OPENPTY
   retropt_bool(opts, OPT_OPENPTY, &useopenpty);
#elif defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   useptmx = true;
#endif

#if HAVE_OPENPTY && (defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC))
   if (useptmx)
      useopenpty = false;
#endif

   applyopts(-1, opts, PH_PREBIGEN);

#if defined(HAVE_DEV_PTMX)
#  define PTMX "/dev/ptmx"	/* Linux */
#elif HAVE_DEV_PTC
#  define PTMX "/dev/ptc"	/* AIX 4.3.3 */
#endif
#if HAVE_DEV_PTMX || HAVE_DEV_PTC
   if (useptmx) {
      if ((ptyfd = Open(PTMX, O_RDWR|O_NOCTTY, 0620)) < 0) {
	 Warn1("open(\""PTMX"\", O_RDWR|O_NOCTTY, 0620): %s",
	       strerror(errno));
	 /*!*/
      } else {
	 ;/*0 Info1("open(\""PTMX"\",  O_RDWR|O_NOCTTY, 0620) -> %d", ptyfd);*/
      }
      if (ptyfd >= 0 && ttyfd < 0) {
	 char *tn = NULL;
	 /* we used PTMX before forking */
	 extern char *ptsname(int);
#if HAVE_GRANTPT	/* AIX, not Linux */
	 if (Grantpt(ptyfd)/*!*/ < 0) {
	    Warn2("grantpt(%d): %s", ptyfd, strerror(errno));
	 }
#endif /* HAVE_GRANTPT */
#if HAVE_UNLOCKPT
	 if (Unlockpt(ptyfd)/*!*/ < 0) {
	    Warn2("unlockpt(%d): %s", ptyfd, strerror(errno));
	 }
#endif /* HAVE_UNLOCKPT */
#if HAVE_PTSNAME	/* AIX, not Linux */
	 if ((tn = Ptsname(ptyfd)) == NULL) {
	    Warn2("ptsname(%d): %s", ptyfd, strerror(errno));
	 } else {
	    Notice1("PTY is %s", tn);
	 }
#endif /* HAVE_PTSNAME */
	 if (tn == NULL) {
	    if ((tn = Ttyname(ptyfd)) == NULL) {
	       Warn2("ttyname(%d): %s", ptyfd, strerror(errno));
	    }
	 }
	 strncpy(ptyname, tn, MAXPTYNAMELEN);
      }
   }
#endif /* HAVE_DEV_PTMX || HAVE_DEV_PTC */
#if HAVE_OPENPTY
   if (ptyfd < 0) {
      int result;
      if ((result = Openpty(&ptyfd, &ttyfd, ptyname, NULL, NULL)) < 0) {
	 Error4("openpty(%p, %p, %p, NULL, NULL): %s",
		&ptyfd, &ttyfd, ptyname, strerror(errno));
	 return -1;
      }
      Notice1("PTY is %s", ptyname);
   }
#endif /* HAVE_OPENPTY */

   applyopts_named(ptyname, opts, PH_PASTOPEN);
   applyopts_named(ptyname, opts, PH_FD);

   applyopts_cloexec(ptyfd, opts);/*!*/
   xfd->stream.dtype    = DATA_PTY;
   xfd->stream.howtoend = END_CLOSE;

   applyopts(ptyfd, opts, PH_FD);

   if (!retropt_string(opts, OPT_SYMBOLIC_LINK, &linkname)) {
      if (Unlink(linkname) < 0 && errno != ENOENT) {
	 Error2("unlink(\"%s\"): %s", linkname, strerror(errno));
      }
      if (Symlink(ptyname, linkname) < 0) {
	 Error3("symlink(\"%s\", \"%s\"): %s",
		ptyname, linkname, strerror(errno));
      }
   }
   
   applyopts(ptyfd, opts, PH_LATE);
   applyopts_single(&xfd->stream, opts, PH_LATE);

   xfd->stream.fd = ptyfd;
   return STAT_OK;
}
#endif /* WITH_PTY */
