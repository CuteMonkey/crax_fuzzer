/* $Id: xio-creat.c,v 1.9 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of create type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"
#include "xio-creat.h"


#if WITH_CREAT

static int xioopen_creat(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_creat  = { "create", 3, xioopen_creat, GROUP_FD|GROUP_NAMED|GROUP_FILE, 0, 0, NULL HELP(":<filename>") };


/* retrieve the mode option and perform the creat() call.
   returns the file descriptor or a negative value. */
static int _xioopen_creat(const char *path, int rw, struct opt *opts) {
   mode_t mode = 0666;
   int fd;

   retropt_modet(opts, OPT_PERM,      &mode);

   if ((fd = Creat(path, mode)) < 0) {
      Error3("creat(\"%s\", 0%03o): %s",
	     path, mode, strerror(errno));
      return STAT_RETRYLATER;
   }
   return fd;
}


static int xioopen_creat(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
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

   Notice2("creating regular file \"%s\" for %s", a1, ddirection[rw]);
   if ((result = _xioopen_creat(a1, rw, opts)) < 0)
      return result;
   fd->stream.fd = result;

   applyopts_named(a1, opts, PH_PASTOPEN);
   if ((result = applyopts2(fd->stream.fd, opts, PH_PASTOPEN, PH_LATE2)) < 0)
      return result;

   applyopts_cloexec(fd->stream.fd, opts);

   applyopts_fchown(fd->stream.fd, opts);

   if ((result = _xio_openlate(&fd->stream, opts)) < 0)
      return result;

   return 0;
}

#endif /* WITH_CREAT */
