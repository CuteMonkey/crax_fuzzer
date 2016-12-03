/* $Id: xio-unix.c,v 1.16 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of UNIX socket type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-listen.h"
#include "xio-unix.h"
#include "xio-named.h"


#if WITH_UNIX

static int xioopen_unix_connect(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);
static int xioopen_unix_listen(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_unix_connect = { "unix-connect",   3, xioopen_unix_connect, GROUP_FD|GROUP_NAMED|GROUP_SOCKET|GROUP_SOCK_UNIX, 0, 0, NULL HELP(":<filename>") };

#if WITH_LISTEN
const struct addrdesc addr_unix_listen  = { "unix-listen", 3, xioopen_unix_listen, GROUP_FD|GROUP_NAMED|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_LISTEN|GROUP_CHILD, 0, 0, NULL HELP(":<filename>") };
#endif /* WITH_LISTEN */


#if WITH_LISTEN
static int xioopen_unix_listen(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   /* we expect the form: filename */
   struct sockaddr_un us;
   char *name;
   int socktype = SOCK_STREAM;
   bool do_unlink_early = false;
   struct opt *opts = NULL;
   int result;

   socket_un_init(&us);

   name = a1;
   if (a1 = strchr(name, ',')) {
      if (a1-name > UNIX_PATH_MAX) {
	 Error2("socket address too long (%d, max is %d)",
		a1-name, UNIX_PATH_MAX);
	 return STAT_NORETRY;
      }
      *a1++ = '\0';
      strncpy(us.sun_path, name, Min(UNIX_PATH_MAX, a1-1-name));
      if (a1-1-name <= UNIX_PATH_MAX)
	 us.sun_path[a1-1-name] = '\0';
   } else {
      strncpy(us.sun_path, name, UNIX_PATH_MAX);
   }

   if (parseopts(a1, groups, &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   retropt_bool(opts, OPT_UNLINK_EARLY, &do_unlink_early);
   if (do_unlink_early) {
      if (Unlink(name) < 0) {
	 if (errno == ENOENT) {
	    Warn2("unlink(\"%s\"): %s", name, strerror(errno));
	 } else {
	    Error2("unlink(\"%s\"): %s", name, strerror(errno));
	 }
      }
   }

   /* trying to set user-early, perm-early etc. here is useless because
      file system entry is available only past bind() call. */

   applyopts_named(name, opts, PH_EARLY);	/* umask! */

   retropt_int(opts, OPT_SO_TYPE, &socktype);

   if ((result =
	_xioopen_listen(&fd->stream,
		     (struct sockaddr *)&us, sizeof(struct sockaddr_un),
		     opts, PF_UNIX, socktype, 0, E_ERROR))
       != 0)
      return result;
   return 0;
}
#endif /* WITH_LISTEN */


static int xioopen_unix_connect(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   /* we expect the form: filename */
   char *a2;
   bool needbind = false;
   struct sockaddr_un them, us;
   struct opt *opts = NULL;
   int result;

   socket_un_init(&us);
   socket_un_init(&them);

   if (a2 = strchr(a1, ','))  *a2++ = '\0';

   strncpy(them.sun_path, a1, UNIX_PATH_MAX);

   if (parseopts(a2, groups, &opts) < 0) {
      return STAT_NORETRY;
   }
   applyopts(-1, opts, PH_INIT);

   if (retropt_bind(opts, AF_UNIX, NULL, (struct sockaddr *)&us, 0) >= 0) {
      needbind = true;
   }

   if ((result =
	xioopen_connect(&fd->stream,
		      needbind?(struct sockaddr *)&us:NULL, sizeof(us),
		      (struct sockaddr *)&them,
		      sizeof(struct sockaddr_un),
		      opts, PF_UNIX, SOCK_STREAM, 0, false)) != 0) {
      return result;
   }
   if ((result = _xio_openlate(&fd->stream, opts)) < 0) {
      return result;
   }
   return 0;
}

#endif /* WITH_UNIX */
