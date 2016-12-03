/* $Id: xio-file.c,v 1.14 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of open type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"
#include "xio-file.h"


static int xioopen_open(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


#if WITH_OPEN

/****** OPEN addresses ******/
const struct optdesc opt_o_rdonly    = { "o-rdonly",    "rdonly", OPT_O_RDONLY,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG_PATTERN, O_RDONLY, O_ACCMODE };
const struct optdesc opt_o_wronly    = { "o-wronly",    "wronly", OPT_O_WRONLY,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG_PATTERN, O_WRONLY, O_ACCMODE };
const struct optdesc opt_o_rdwr      = { "o-rdwr",      "rdwr",   OPT_O_RDWR,      GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG_PATTERN, O_RDWR,   O_ACCMODE };
const struct optdesc opt_o_create    = { "o-create",    "creat",  OPT_O_CREATE,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_CREAT };
const struct optdesc opt_o_excl      = { "o-excl",      "excl",   OPT_O_EXCL,      GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_EXCL };
const struct optdesc opt_o_noctty    = { "o-noctty",    "noctty", OPT_O_NOCTTY,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_NOCTTY };
#ifdef O_SYNC
const struct optdesc opt_o_sync      = { "o-sync",      "sync",   OPT_O_SYNC,      GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_SYNC };
#endif
#ifdef O_NOFOLLOW
const struct optdesc opt_o_nofollow  = { "o-nofollow",  "nofollow",OPT_O_NOFOLLOW, GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_NOFOLLOW };
#endif
#ifdef O_DIRECTORY
const struct optdesc opt_o_directory = { "o-directory", "directory",OPT_O_DIRECTORY, GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_DIRECTORY };
#endif
#ifdef O_LARGEFILE
const struct optdesc opt_o_largefile = { "o-largefile", "largefile",OPT_O_LARGEFILE, GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_LARGEFILE };
#endif
#ifdef O_NSHARE
const struct optdesc opt_o_nshare    = { "o-nshare",    "nshare", OPT_O_NSHARE,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_NSHARE };
#endif
#ifdef O_RSHARE
const struct optdesc opt_o_rshare    = { "o-rshare",    "rshare", OPT_O_RSHARE,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_RSHARE };
#endif
#ifdef O_DEFER
const struct optdesc opt_o_defer     = { "o-defer",     "defer",  OPT_O_DEFER,     GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_DEFER };
#endif
#ifdef O_DIRECT
const struct optdesc opt_o_direct    = { "o-direct",    "direct", OPT_O_DIRECT,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_DIRECT };
#endif
#ifdef O_DSYNC
const struct optdesc opt_o_dsync     = { "o-dsync",     "dsync",  OPT_O_DSYNC,     GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_DSYNC };
#endif
#ifdef O_RSYNC
const struct optdesc opt_o_rsync     = { "o-rsync",     "rsync",  OPT_O_RSYNC,     GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_RSYNC };
#endif
#ifdef O_DELAY
const struct optdesc opt_o_delay     = { "o-delay",     "delay",  OPT_O_DELAY,     GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_DELAY };
#endif
#ifdef O_PRIV
const struct optdesc opt_o_priv      = { "o-priv",      "priv",   OPT_O_PRIV,      GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FLAG, O_PRIV };
#endif
const struct optdesc opt_o_trunc     = { "o-trunc",     "trunc",  OPT_O_TRUNC,     GROUP_OPEN, PH_LATE, TYPE_BOOL, OFUNC_FLAG, O_TRUNC };

#endif /* WITH_OPEN */


#if WITH_FILE	/*! inconsistent name FILE vs. OPEN */

const struct addrdesc addr_open   = { "open",   3, xioopen_open, GROUP_FD|GROUP_NAMED|GROUP_OPEN|GROUP_FILE, 0, 0, NULL HELP(":<filename>") };


/* open for writing:
   if the filesystem entry already exists, the data is appended
   if it does not exist, a file is created and the data is appended
*/
static int xioopen_open(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   char *a2;
   struct opt *opts = NULL;
   bool exists;
   int result;

   if ((a2 = strchr(a1, ',')) != NULL) {
      *a2++ = '\0';
   }

   /* remove old file, or set user/permissions on old file; parse options */
   if ((result = _xioopen_named_early(a1, a2, groups, &exists, &opts)) < 0) {
      return result;
   }

   Notice3("opening %s \"%s\" for %s",
	   filetypenames[(result&S_IFMT)>>12], a1, ddirection[rw]);
   if ((result = _xioopen_open(a1, rw, opts)) < 0)
      return result;
   fd->stream.fd = result;

#if WITH_TERMIOS
   if (Isatty(fd->stream.fd)) {
      if (Tcgetattr(fd->stream.fd, &fd->stream.savetty) < 0) {
	 Warn2("cannot query current terminal settings on fd %d: %s",
	       fd->stream.fd, strerror(errno));
      } else {
	 fd->stream.ttyvalid = true;
      }
   }
#endif /* WITH_TERMIOS */

   applyopts_named(a1, opts, PH_FD);
   applyopts(fd->stream.fd, opts, PH_FD);
   applyopts_cloexec(fd->stream.fd, opts);

   applyopts_fchown(fd->stream.fd, opts);

   if ((result = _xio_openlate(&fd->stream, opts)) < 0)
      return result;

   return 0;
}

#endif /* WITH_FILE */
