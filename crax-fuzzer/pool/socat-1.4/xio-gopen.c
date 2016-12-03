/* $Id: xio-gopen.c,v 1.18 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of generic open type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"
#include "xio-gopen.h"


#if WITH_GOPEN

static int xioopen_gopen(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_gopen  = { "gopen",  3, xioopen_gopen, GROUP_FD|GROUP_NAMED|GROUP_OPEN|GROUP_FILE|GROUP_SOCKET|GROUP_SOCK_UNIX, 0, 0, NULL HELP(":<filename>") };


static int xioopen_gopen(char *a1, int rw, xiofile_t *fd, unsigned groups, int dummy1, int dummy2, void *dummyp1) {
   char *a2;
   struct opt *opts = NULL;
   flags_t flags = rw;
   mode_t st_mode;
   bool exists;
   int result;

   if ((a2 = strchr(a1, ',')) != NULL) {
      *a2++ = '\0';
   }

   if ((result =
	  _xioopen_named_early(a1, a2, GROUP_NAMED|groups, &exists, &opts)) < 0) {
      return result;
   }
   st_mode = result;

   if (exists) {
      /* file (or at least named entry) exists */
      if (rw != O_RDONLY) {
	 flags |= O_APPEND;
      }
   } else {
      flags |= O_CREAT;
   }

   /* note: when S_ISSOCK was undefined, it always gives 0 */
   if (exists && S_ISSOCK(st_mode)) {
#if WITH_UNIX
      int socktype = SOCK_STREAM;
      int optsotype = -1;
      struct sockaddr_un sa, us;
      bool needbind = false;
      socklen_t uslen;
      char infobuff[256];
      struct opt *opts2;

      socket_un_init(&sa);
      socket_un_init(&us);

      Info1("\"%s\" is a socket, connecting to it", a1);
      if (retropt_int(opts, OPT_SO_TYPE, &optsotype) == 0) {
	 socktype = optsotype;
      }

      if (retropt_bind(opts, AF_UNIX, NULL, (struct sockaddr *)&us, 0) >= 0) {
	 needbind = true;
      }

      /* save options, because we might have to start again with Socket() */
      opts2 = copyopts(opts, GROUP_ALL);

      if ((fd->stream.fd = Socket(PF_UNIX, socktype, 0)) < 0) {
	 Error2("socket(PF_UNIX, %d, 0): %s", socktype, strerror(errno));
	 return STAT_RETRYLATER;
      }
      /*0 Info2("socket(PF_UNIX, %d, 0) -> %d", socktype, fd->stream.fd);*/
      applyopts(fd->stream.fd, opts, PH_PASTSOCKET);
      applyopts(fd->stream.fd, opts, PH_FD);

      applyopts_cloexec(fd->stream.fd, opts);

      sa.sun_family = AF_UNIX;
      strncpy(sa.sun_path, a1, UNIX_PATH_MAX);

#if 0
      applyopts(fd->stream.fd, opts, PH_PREBIND);
      applyopts(fd->stream.fd, opts, PH_BIND);
      if (us) {
	 if (Bind(fd->stream.fd, us, uslen) < 0) {
	    Error4("bind(%d, {%s}, "F_Zd"): %s",
		   fd->fd, sockaddr_info(us, infobuff, sizeof(infobuff)),
		   uslen, strerror(errno));
	    if (fd->forever || --fd->retry) {
	       Nanosleep(&fd->intervall, NULL);
	       continue;
	    } else
	       return STAT_RETRYLATER;
	 }
      }
      applyopts(fd->stream.fd, opts, PH_PASTBIND);
#endif /* 0 */

      applyopts(fd->stream.fd, opts, PH_CONNECT);
      if ((result = Connect(fd->stream.fd, (struct sockaddr *)&sa, sizeof(sa))) < 0) {
	 if (errno == EINPROGRESS) {
	    Warn4("connect(%d, %s, "F_Zd"): %s",
		  fd->stream.fd, sockaddr_unix_info(&sa, infobuff, sizeof(infobuff)),
		  sizeof(sa), strerror(errno));
	 } else if (errno == EPROTOTYPE && optsotype != SOCK_STREAM) {
	    Warn4("connect(%d, %s, "F_Zd"): %s",
		  fd->stream.fd, sockaddr_unix_info(&sa, infobuff, sizeof(infobuff)),
		  sizeof(sa), strerror(errno));
	    Info("assuming datagram socket");
	    Close(fd->stream.fd);

	    opts = opts2;
	    if ((fd->stream.fd = Socket(PF_UNIX, SOCK_DGRAM, 0)) < 0) {
	       Error1("socket(PF_UNIX, SOCK_DGRAM, 0): %s", strerror(errno));
	       return STAT_RETRYLATER;
	    }
	    /*0 Info1("socket(PF_UNIX, SOCK_DGRAM, 0) -> %d", fd->stream.fd);*/

	    applyopts(fd->stream.fd, opts, PH_PASTSOCKET);
	    applyopts(fd->stream.fd, opts, PH_FD);

	    applyopts_cloexec(fd->stream.fd, opts);

	    sa.sun_family = AF_UNIX;
	    strncpy(sa.sun_path, a1, UNIX_PATH_MAX);
	    
	    fd->stream.dtype = DATA_RECV;
	    fd->stream.salen = sizeof(sa);
	    memcpy(&fd->stream.peersa.soa, &sa, fd->stream.salen);
	 } else {
	    Error4("connect(%d, %s, "F_Zd"): %s",
		   fd->stream.fd, sockaddr_unix_info(&sa, infobuff, sizeof(infobuff)),
		   sizeof(sa), strerror(errno));
	    return STAT_RETRYLATER;
	 }
      }
      fd->stream.howtoend = END_SHUTDOWN;

      applyopts_fchown(fd->stream.fd, opts);
      applyopts(fd->stream.fd, opts, PH_CONNECTED);
      applyopts(fd->stream.fd, opts, PH_LATE);
      applyopts_named(a1, opts, PH_PASTOPEN);	/* unlink-late */

      if (Getsockname(fd->stream.fd, (struct sockaddr *)&us, &uslen) < 0) {
	 Warn4("getsockname(%d, %p, {%d}): %s",
	       fd->stream.fd, &us, uslen, strerror(errno));
      } else {
	 Notice1("successfully connected via %s",
		 sockaddr_unix_info(&us, infobuff, sizeof(infobuff)));
      }
#else
      Error("\"%s\" is a socket, but UNIX socket support is not compiled in");
      return -1;
#endif /* WITH_UNIX */

   } else {
      /* a file name */

      /*applyopts_early(a1, opts);*/
      applyopts_named(a1, opts, PH_EARLY);

      Debug1("\"%s\" is not a socket, opening it", a1);
      Notice3("opening %s \"%s\" for %s",
	      filetypenames[(st_mode&S_IFMT)>>12], a1, ddirection[rw]);
      if ((result = _xioopen_open(a1, flags, opts)) < 0)
	 return result;
#if __unix && __sun /* solaris */
      if (S_ISCHR(st_mode)) {
	 Ioctl(result, I_PUSH, "ptem");
	 Ioctl(result, I_PUSH, "ldterm");
	 Ioctl(result, I_PUSH, "ttcompat");
      }
#endif
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
   }

   if ((result = applyopts2(fd->stream.fd, opts, PH_PASTSOCKET, PH_CONNECTED)) < 0) 
      return result;

   if ((result = _xio_openlate(&fd->stream, opts)) < 0)
      return result;
   return 0;
}

#endif /* WITH_GOPEN */
