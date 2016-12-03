/* $Id: xio-progcall.c,v 1.42 2004/06/20 21:32:14 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains common code dealing with program calls (exec, system) */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-process.h"
#include "xio-progcall.h"

/* these options are used by address pty too */
#if HAVE_OPENPTY
const struct optdesc opt_openpty = { "openpty",   NULL, OPT_OPENPTY,     GROUP_PTY,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif /* HAVE_OPENPTY */
#if HAVE_DEV_PTMX || HAVE_DEV_PTC
const struct optdesc opt_ptmx    = { "ptmx",      NULL, OPT_PTMX,        GROUP_PTY,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif

#if WITH_EXEC || WITH_SYSTEM

#define MAXPTYNAMELEN 64

const struct optdesc opt_fdin    = { "fdin",      NULL, OPT_FDIN,        GROUP_FORK,   PH_PASTBIGEN,   TYPE_USHORT,	OFUNC_SPEC };
const struct optdesc opt_fdout   = { "fdout",     NULL, OPT_FDOUT,       GROUP_FORK,   PH_PASTBIGEN,   TYPE_USHORT,	OFUNC_SPEC };
const struct optdesc opt_path    = { "path",      NULL, OPT_PATH,        GROUP_EXEC,   PH_PREEXEC,     TYPE_STRING,	OFUNC_SPEC };
const struct optdesc opt_pipes   = { "pipes",     NULL, OPT_PIPES,       GROUP_FORK,   PH_BIGEN,       TYPE_BOOL, 	OFUNC_SPEC };
#if HAVE_PTY
const struct optdesc opt_pty     = { "pty",       NULL, OPT_PTY,         GROUP_FORK,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif
const struct optdesc opt_stderr  = { "stderr",    NULL, OPT_STDERR,      GROUP_FORK,   PH_PASTFORK,        TYPE_BOOL,	OFUNC_SPEC };
const struct optdesc opt_nofork  = { "nofork",    NULL, OPT_NOFORK,      GROUP_FORK,   PH_BIGEN,       TYPE_BOOL,       OFUNC_SPEC };
const struct optdesc opt_sighup  = { "sighup",    NULL, OPT_SIGHUP,      GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGHUP };
const struct optdesc opt_sigint  = { "sigint",    NULL, OPT_SIGINT,      GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGINT };
const struct optdesc opt_sigquit = { "sigquit",   NULL, OPT_SIGQUIT,     GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGQUIT };


/* fork for exec/system, but return before exec'ing.
   return=0: is child process
   return>0: is parent process
   return<0: error occurred, assume parent process and no child exists !!!
 */
int _xioopen_foxec(char *comma,
		int rw,	/* O_RDONLY etc. */
		struct single *fd,
		unsigned groups,
		   struct opt **copts	/* in: opts; out: opts for child */
		) {
   struct opt *popts;	/* parent process options */
   int numleft;
   int d, type, protocol, sv[2], rdpip[2], wrpip[2];
   bool usepipes = false;
#if HAVE_PTY
   int ptyfd = -1, ttyfd = -1;
   bool usebestpty = false;	/* use the best available way to open pty */
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   bool useptmx = false;	/* use /dev/ptmx or equivalent */
#endif
#if HAVE_OPENPTY
   bool useopenpty = false;	/* try only openpty */
#endif	/* HAVE_OPENPTY */
   bool usepty = false;		/* any of the pty options is selected */
   char ptyname[MAXPTYNAMELEN];
#endif /* HAVE_PTY */
   pid_t pid = 0;	/* mostly int */
   short fdi = 0, fdo = 1;
   short result;
   bool withstderr = false;
   bool nofork = false;
   bool withfork;

   popts = moveopts(*copts, GROUP_ALL);
   applyopts2(-1, popts, PH_INIT, PH_EARLY);

   retropt_bool(popts, OPT_NOFORK, &nofork);
   withfork = !nofork;

   retropt_bool(popts, OPT_PIPES, &usepipes);
#if HAVE_PTY
   retropt_bool(popts, OPT_PTY, &usebestpty);
#if HAVE_OPENPTY
   retropt_bool(popts, OPT_OPENPTY, &useopenpty);
#endif
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   retropt_bool(popts, OPT_PTMX, &useptmx);
#endif
   usepty = (usebestpty
#if HAVE_OPENPTY
	     || useopenpty
#endif
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
	     || useptmx
#endif
	     );
   if (usepipes && usepty) {
      Warn("_xioopen_foxec(): options \"pipes\" and \"pty\" must not be specified together; ignoring \"pipes\"");
      usepipes = false;
   }
#endif /* HAVE_PTY */
   retropt_ushort(popts, OPT_FDIN,  (unsigned short *)&fdi);
   retropt_ushort(popts, OPT_FDOUT, (unsigned short *)&fdo);

   if (withfork) {
#if HAVE_PTY
      Notice2("forking off child, using %s for %s",
	    &("socket\0\0pipes\0\0\0pty\0\0\0\0\0"[(usepipes<<3)|(usepty<<4)]),
	      ddirection[rw]);
#else
      Notice2("forking off child, using %s for %s",
	      &("socket\0\0pipes\0\0\0"[(usepipes<<3)]),
	      ddirection[rw]);
#endif /* HAVE_PTY */
   }
   applyopts(-1, popts, PH_PREBIGEN);

   if (!withfork) {
      struct single *stream1, *stream2;

      free(*copts);
      *copts = moveopts(popts, GROUP_ALL);
      /* what if WE are sock1 ? */
      if (sock1 == NULL) {
	 Fatal("nofork option must no be applied to first socat address");
      }

      if (sock1->tag == XIO_TAG_DUAL) {
	 stream1 = &sock1->dual.stream[0]->stream;
	 stream2 = &sock1->dual.stream[1]->stream;
      } else {
	 stream1 = &sock1->stream;
	 stream2 = &sock1->stream;
      }
      if (stream1->dtype == DATA_READLINE || stream2->dtype == DATA_READLINE ||
	  stream1->dtype == DATA_OPENSSL  || stream2->dtype == DATA_OPENSSL
	  ) {
	 Error("with option nofork, openssl and readline in address1 do not work");
      }
      if (stream1->lineterm != LINETERM_RAW ||
	  stream2->lineterm != LINETERM_RAW ||
	  stream1->ignoreeof || stream2->ignoreeof) {
	 Warn("due to option nofork, address1 options for lineterm and igoreeof do not apply");
      }

      /*! problem: when fdi==WRFD(sock1) or fdo==RDFD(sock1) */
      if (rw != O_WRONLY) {
	 if (XIO_GETRDFD(sock1) == fdi) {
	    if (Fcntl_l(fdi, F_SETFD, 0) < 0) {
	       Warn2("fcntl(%d, F_SETFD, 0): %s", fdi, strerror(errno));
	    }
	    if (Dup2(XIO_GETRDFD(sock1), fdi) < 0) {
	       Error3("dup2(%d, %d): %s",
		      XIO_GETRDFD(sock1), fdi, strerror(errno));
	    }
	    /*0 Info2("dup2(%d, %d)", XIO_GETRDFD(sock1), fdi);*/
	 } else {
	    if (Dup2(XIO_GETRDFD(sock1), fdi) < 0) {
	       Error3("dup2(%d, %d): %s",
		      XIO_GETRDFD(sock1), fdi, strerror(errno));
	    }
	    /*0 Info2("dup2(%d, %d)", XIO_GETRDFD(sock1), fdi);*/
	 }
      }
      if (rw != O_RDONLY) {
	 if (XIO_GETWRFD(sock1) == fdo) {
	    if (Fcntl_l(fdo, F_SETFD, 0) < 0) {
	       Warn2("fcntl(%d, F_SETFD, 0): %s", fdo, strerror(errno));
	    }
	    if (Dup2(XIO_GETWRFD(sock1), fdo) < 0) {
	       Error3("dup2(%d, %d): %s)",
		      XIO_GETWRFD(sock1), fdo, strerror(errno));
	    }
	    /*0 Info2("dup2(%d, %d)", XIO_GETWRFD(sock1), fdo);*/
	 } else {
	    if (Dup2(XIO_GETWRFD(sock1), fdo) < 0) {
	       Error3("dup2(%d, %d): %s)",
		      XIO_GETWRFD(sock1), fdo, strerror(errno));
	    }
	    /*0 Info2("dup2(%d, %d)", XIO_GETWRFD(sock1), fdo);*/
	 }
      }
   } else
#if HAVE_PTY
   if (usepty) {

#if defined(HAVE_DEV_PTMX)
#  define PTMX "/dev/ptmx"	/* Linux */
#elif HAVE_DEV_PTC
#  define PTMX "/dev/ptc"	/* AIX 4.3.3 */
#endif
      fd->dtype = DATA_PTY;
#if HAVE_DEV_PTMX || HAVE_DEV_PTC
      if (usebestpty || useptmx) {
	 if ((ptyfd = Open(PTMX, O_RDWR|O_NOCTTY, 0620)) < 0) {
	    Warn1("open(\""PTMX"\", O_RDWR|O_NOCTTY, 0620): %s",
		  strerror(errno));
	    /*!*/
	 } else {
	    /*0 Info2("open(\"%s\", O_RDWR|O_NOCTTY, 0620) -> %d", PTMX, ptyfd);*/
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
	    }
#endif /* HAVE_PTSNAME */
	    if (tn == NULL) {
	       if ((tn = Ttyname(ptyfd)) == NULL) {
		  Warn2("ttyname(%d): %s", ptyfd, strerror(errno));
	       }
	    }
	    strncpy(ptyname, tn, MAXPTYNAMELEN);
	    if ((ttyfd = Open(tn, O_RDWR|O_NOCTTY, 0620)) < 0) {
	       Warn2("open(\"%s\", O_RDWR|O_NOCTTY, 0620): %s", tn, strerror(errno));
	    } else {
	       /*0 Info2("open(\"%s\", O_RDWR|O_NOCTTY, 0620) -> %d", tn, ttyfd);*/
	    }

#if __unix && __sun
	    /*! solaris */
	    Ioctl(ttyfd, I_PUSH, "ptem");
	    Ioctl(ttyfd, I_PUSH, "ldterm");
	    Ioctl(ttyfd, I_PUSH, "ttcompat");
#endif

#if 0	    /* the following block need not work */

	    if (ttyfd >= 0 && ((tn = Ttyname(ttyfd)) == NULL)) {
	       Warn2("ttyname(%d): %s", ttyfd, strerror(errno));
	    }
	    if (tn == NULL) {
	       Error("could not open pty");
	       return STAT_NORETRY;
	    }
#endif
	    Info1("opened pseudo terminal %s", tn);
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
      }
#endif /* HAVE_OPENPTY */
      free(*copts);
      if ((*copts = moveopts(popts, GROUP_TERMIOS|GROUP_FORK|GROUP_EXEC|GROUP_PROCESS)) == NULL) {
	 return STAT_RETRYLATER;
      }
      applyopts_cloexec(ptyfd, popts);/*!*/
      fd->howtoend = END_CLOSE_KILL;

      /* this for parent, was after fork */
      applyopts(ptyfd, popts, PH_FD);
      applyopts(ptyfd, popts, PH_LATE);
      applyopts_single(fd, popts, PH_LATE);

      fd->fd = ptyfd;

      /* this for child, was after fork */
      applyopts(ttyfd, *copts, PH_FD);
   } else
#endif /* HAVE_PTY */
   if (usepipes) {
      struct opt *popts2, *copts2;

      if (rw == O_RDWR)
	 fd->dtype = DATA_2PIPE;
      if (rw != O_WRONLY) {
	 if (Pipe(rdpip) < 0) {
	    Error2("pipe(%p): %s", rdpip, strerror(errno));
	    return STAT_RETRYLATER;
	 }
      }
      /*0 Info2("pipe({%d,%d})", rdpip[0], rdpip[1]);*/
      /* rdpip[0]: read by socat; rdpip[1]: write by child */
      free(*copts);
      if ((*copts = moveopts(popts, GROUP_FORK|GROUP_EXEC|GROUP_PROCESS))
	  == NULL) {
	 return STAT_RETRYLATER;
      }

      popts2 = copyopts(popts, GROUP_ALL);
      copts2 = copyopts(*copts, GROUP_ALL);

      if (rw != O_WRONLY) {
	 applyopts_cloexec(rdpip[0], popts);
	 applyopts(rdpip[0], popts, PH_FD);
	 applyopts(rdpip[1], *copts, PH_FD);
      }

      if (rw != O_RDONLY) {
	 if (Pipe(wrpip) < 0) {
	    Error2("pipe(%p): %s", wrpip, strerror(errno));
	    return STAT_RETRYLATER;
	 }
      }
      /*0 Info2("pipe({%d,%d})", wrpip[0], wrpip[1]);*/

      /* wrpip[1]: write by socat; wrpip[0]: read by child */
      if (rw != O_RDONLY) {
	 applyopts_cloexec(wrpip[1], popts2);
	 applyopts(wrpip[1], popts2, PH_FD);
	 applyopts(wrpip[0], copts2, PH_FD);
      }
      fd->howtoend = END_CLOSE_KILL;

      /* this for parent, was after fork */
      switch (rw) {
      case O_RDONLY: fd->fd = rdpip[0]; break;
      case O_WRONLY: fd->fd = wrpip[1]; break;
      case O_RDWR:   fd->fd = rdpip[0];
	 fd->para.exec.fdout = wrpip[1];
	 break;
      }
      applyopts(fd->fd, popts, PH_FD);
      applyopts(fd->fd, popts, PH_LATE);
      applyopts_single(fd, popts, PH_LATE);
   } else {
      d = AF_UNIX;  type = SOCK_STREAM;
      protocol = 0;	/* PF_UNIX does not work on AIX */
      retropt_int(popts, OPT_SO_TYPE, &type);
      result = Socketpair(d, type, protocol, sv);
      if (result < 0) {
	 Error5("socketpair(%d, %d, %d, %p): %s",
		d, type, protocol, sv, strerror(errno));
	 return STAT_RETRYLATER;
      }
      /*0 Info5("socketpair(%d, %d, %d, {%d,%d})",
	d, type, protocol, sv[0], sv[1]);*/
      free(*copts);
      if ((*copts = moveopts(popts, GROUP_FORK|GROUP_EXEC|GROUP_PROCESS)) == NULL) {
	 return STAT_RETRYLATER;
      }
      applyopts(sv[0], *copts, PH_PASTSOCKET);
      applyopts(sv[1], popts, PH_PASTSOCKET);

      applyopts_cloexec(sv[0], *copts);
      applyopts(sv[0], *copts, PH_FD);
      applyopts(sv[1], popts, PH_FD);

      applyopts(sv[0], *copts, PH_PREBIND);
      applyopts(sv[0], *copts, PH_BIND);
      applyopts(sv[0], *copts, PH_PASTBIND);
      applyopts(sv[1], popts, PH_PREBIND);
      applyopts(sv[1], popts, PH_BIND);
      applyopts(sv[1], popts, PH_PASTBIND);

      fd->howtoend = END_SHUTDOWN_KILL;

      /* this for parent, was after fork */
      fd->fd = sv[0];
      applyopts(fd->fd, popts, PH_FD);
      applyopts(fd->fd, popts, PH_LATE);
      applyopts_single(fd, popts, PH_LATE);
   }
   /*0   if ((optpr = copyopts(*copts, GROUP_PROCESS)) == NULL)
     return STAT_RETRYLATER;*/
   retropt_bool(*copts, OPT_STDERR, &withstderr);
   if (Signal(SIGCHLD, childdied) == SIG_ERR) {
      Warn2("signal(SIGCHLD, %p): %s", childdied, strerror(errno));
   }

   if (withfork) {
      pid = Fork();
      if (pid < 0) {
	 Error1("fork(): %s", strerror(errno));
	 return STAT_RETRYLATER;
      }
   }
   if (!withfork || pid == 0) {	/* child */
      uid_t user;
      gid_t group;

      if (withfork) {
	 if (Signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
	    Warn1("signal(SIGCHLD, SIG_IGN): %s", strerror(errno));
	 }

#if HAVE_PTY
	 if (usepty) {
	    if (rw != O_RDONLY && fdi != ttyfd) {
	       if (Dup2(ttyfd, fdi) < 0) {
		  Error3("dup2(%d, %d): %s", ttyfd, fdi, strerror(errno));
		  return STAT_RETRYLATER; }
	       /*0 Info2("dup2(%d, %d)", ttyfd, fdi);*/
	    }
	    if (rw != O_WRONLY && fdo != ttyfd) {
	       if (Dup2(ttyfd, fdo) < 0) {
		  Error3("dup2(%d, %d): %s", ttyfd, fdo, strerror(errno));
		  return STAT_RETRYLATER; }
	       /*0 Info2("dup2(%d, %d)", ttyfd, fdo);*/
	    }
	    if ((rw == O_RDONLY || fdi != ttyfd) &&
		(rw == O_WRONLY || fdo != ttyfd)) {
	       applyopts_cloexec(ttyfd, *copts);
	    }

	    applyopts(ttyfd, *copts, PH_LATE);

	    applyopts(ttyfd, *copts, PH_LATE2);
	 } else
#endif /* HAVE_PTY */
	    if (usepipes) {
	       /* we might have a temporary conflict between what FDs are
		  currently allocated, and which are to be used. We try to find
		  a graceful solution via temporary descriptors */
	       int tmpi, tmpo;

	       if (fdi == rdpip[1]) {	/* a conflict here */
		  if ((tmpi = Dup(wrpip[0])) < 0) {
		     Error2("dup(%d): %s", wrpip[0], strerror(errno));
		     return STAT_RETRYLATER;
		  }
		  /*0 Info2("dup(%d) -> %d", wrpip[0], tmpi);*/
		  rdpip[1] = tmpi;
	       }
	       if (fdo == wrpip[0]) {	/* a conflict here */
		  if ((tmpo = Dup(rdpip[1])) < 0) {
		     Error2("dup(%d): %s", rdpip[1], strerror(errno));
		     return STAT_RETRYLATER;
		  }
		  /*0 Info2("dup(%d) -> %d", rdpip[1], tmpo);*/
		  wrpip[0] = tmpo;
	       }
	       
	       if (rw != O_WRONLY && rdpip[1] != fdo) {
		  if (Dup2(rdpip[1], fdo) < 0) {
		     Error3("dup2(%d, %d): %s", rdpip[1], fdo, strerror(errno));
		     return STAT_RETRYLATER;
		  }
		  Close(rdpip[1]);
		  /*0 Info2("dup2(%d, %d)", rdpip[1], fdo);*/
		  /*0 applyopts_cloexec(fdo, *copts);*/
	       }
	       if (rw != O_RDONLY && wrpip[0] != fdi) {
		  if (Dup2(wrpip[0], fdi) < 0) {
		     Error3("dup2(%d, %d): %s", wrpip[0], fdi, strerror(errno));
		     return STAT_RETRYLATER;
		  }
		  Close(wrpip[0]);
		  /*0 Info2("dup2(%d, %d)", wrpip[0], fdi);*/
		  /*0 applyopts_cloexec(wrpip[0], *copts);*/	/* option is already consumed! */
		  /* applyopts_cloexec(fdi, *copts);*/	/* option is already consumed! */
	       }

	       applyopts(fdi, *copts, PH_LATE);
	       applyopts(fdo, *copts, PH_LATE);
	       applyopts(fdi, *copts, PH_LATE2);
	       applyopts(fdo, *copts, PH_LATE2);

	    } else {	/* socketpair */
	       if (rw != O_RDONLY && fdi != sv[1]) {
		  if (Dup2(sv[1], fdi) < 0) {
		     Error3("dup2(%d, %d): %s", sv[1], fdi, strerror(errno));
		     return STAT_RETRYLATER; }
		  /*0 Info2("dup2(%d, %d)", sv[1], fdi);*/
	       }
	       if (rw != O_WRONLY && fdo != sv[1]) {
		  if (Dup2(sv[1], fdo) < 0) {
		     Error3("dup2(%d, %d): %s", sv[1], fdo, strerror(errno));
		     return STAT_RETRYLATER; }
		  /*0 Info2("dup2(%d, %d)", sv[1], fdo);*/
	       }
	       if (fdi != sv[1] && fdo != sv[1]) {
		  applyopts_cloexec(sv[1], *copts);
	       }

	       applyopts(fdi, *copts, PH_LATE);
	       applyopts(fdi, *copts, PH_LATE2);
	    }
      } /* withfork */
 
      /* what to do with stderr? */
      if (withstderr) {
	 /* handle it just like ordinary process output, i.e. copy output fd */
	 if (!withfork) {
	    if (Dup2(fdo, 2) < 0) {
	       Error2("dup2(%d, 2): %s", fdo, strerror(errno));
	    }
	    /*0 Info1("dup2(%d, 2)", fdo);*/
	 } else
#if HAVE_PTY
	 if (usepty) {
	    if (Dup2(ttyfd, 2) < 0) {
	       Error2("dup2(%d, 2): %s", ttyfd, strerror(errno));
	    }
	    /*0 Info1("dup2(%d, 2)", ttyfd);*/
	 } else
#endif /* HAVE_PTY */
	 if (usepipes) {
	    if (Dup2(rdpip[1], 2) < 0) {
	       Error2("dup2(%d, 2): %s", rdpip[1], strerror(errno));
	    }
	    /*0 Info1("dup2(%d, 2)", rdpip[1]);*/
	 } else {
	    if (Dup2(sv[1], 2) < 0) {
	       Error2("dup2(%d, 2): %s", sv[1], strerror(errno));
	    }
	    /*0 Info1("dup2(%d, 2)", sv[1]);*/
	 }
      }
      _xioopen_setdelayeduser();
      /* set group before user - maybe you are not permitted afterwards */
      if (retropt_gidt(*copts, OPT_SETGID, &group) >= 0) {
	 Setgid(group);
      }
      if (retropt_uidt(*copts, OPT_SETUID, &user) >= 0) {
	 Setuid(user);
      }
      return 0;	/* indicate child process */
   }

   /* for parent (this is our socat process) */
   Notice1("forked off child process "F_pid, pid);

#if 0
   if ((popts = copyopts(*copts,
			 GROUP_FD|GROUP_TERMIOS|GROUP_FORK|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_FIFO)) == NULL)
      return STAT_RETRYLATER;
#endif

#if HAVE_PTY
   if (usepty) {
      if (Close(ttyfd) < 0) {
	 Warn2("close(%d): %s", ttyfd, strerror(errno));
      }
   } /*0 else*/
#endif /* HAVE_PTY */
#if 0
   if (usepipes) {
   } else {
   }
#endif
   fd->para.exec.pid = pid;

   if ((result = applyopts_single(fd, popts, PH_LATE)) < 0) {
      return result;
   }
   applyopts_signal(fd, popts);
   if ((numleft = leftopts(popts)) > 0) {
      Error1("%d option(s) could not be used", numleft);
      showleft(popts);
      return STAT_NORETRY;
   }

   return pid;	/* indicate parent (main) process */
}
#endif /* WITH_EXEC || WITH_SYSTEM */


int setopt_path(struct opt *opts, char **path) {
   if (retropt_string(opts, OPT_PATH, path) >= 0) {
      if (setenv("PATH", *path, 1) < 0) {
	 Error1("setenv(\"PATH\", \"%s\", 1): insufficient space", *path);
	 return -1;
      }
   }
   return 0;
}
