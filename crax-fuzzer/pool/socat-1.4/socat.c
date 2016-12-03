/* $Id: socat.c,v 1.87 2004/06/20 21:46:10 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the main source, including command line option parsing, general
   control, and the data shuffler */

#include "config.h"
#include "xioconfig.h"	/* what features are enabled */

#include "sysincludes.h"

#include "mytypes.h"
#include "compat.h"
#include "error.h"

#include "sycls.h"
#include "sysutils.h"
#include "dalan.h"
#include "filan.h"
#include "xio.h"
#include "xioopts.h"


/* command line options */
struct {
   size_t bufsiz;
   bool verbose;
   bool verbhex;
   unsigned int timeout;	/* with ignoreeof, reread after seconds */
   struct timeval closwait;	/* after close of x, die after seconds */
   bool debug;
   bool strictopts;	/* stop on errors in address options */
   char logopt;		/* y..syslog; s..stderr; f..file; m..mixed */
   bool lefttoright;	/* first addr ro, second addr wo */
   bool righttoleft;	/* first addr wo, second addr ro */
} socat_opts = { 8192, false, false, 0, {0,500000}, 0, 0, 's', false, false };

void socat_usage(FILE *fd);
void socat_version(FILE *fd);
int socat(const char *address1, const char *address2);
int _socat(void);
int cv_newline(unsigned char **buff, ssize_t *bytes, int lineterm1, int lineterm2);
void socat_signal(int sig);

void lftocrlf(char **in, ssize_t *len, size_t bufsiz);
void crlftolf(char **in, ssize_t *len, size_t bufsiz);

static const char socatversion[] =
#include "./VERSION"
      ;
static const char timestamp[] = __DATE__" "__TIME__;

const char copyright_socat[] = "socat by Gerhard Rieger - see www.dest-unreach.org";
#if WITH_OPENSSL
const char copyright_openssl[] = "This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)";
const char copyright_ssleay[] = "This product includes software written by Tim Hudson (tjh@cryptsoft.com)";
#endif


int main(int argc, const char *argv[]) {
   const char **arg1, *a;
   char buff[10];
   double rto;
   int i, argc0, result;
   struct utsname ubuf;

   diag_set('p', strchr(argv[0], '/') ? strrchr(argv[0], '/')+1 : argv[0]);
   xiosetopt('p', "!!");
   xiosetopt('o', ":");

   argc0 = argc;	/* save for later use */
   arg1 = argv+1;  --argc;
   while (arg1[0] && (arg1[0][0] == '-')) {
      switch (arg1[0][1]) {
      case 'V': socat_version(stdout); Exit(0);
#if WITH_HELP
      case '?':
	 socat_usage(stdout);
	 xioopenhelp(stdout, arg1[0][2]=='?' ? arg1[0][3]=='?' ? 2 : 1 : 0);
	 Exit(0);
#endif /* WITH_HELP */
      case 'd': diag_set('d', NULL); break;
#if WITH_FILAN
      case 'D': socat_opts.debug = true; break;
#endif
      case 'l':
	 switch (arg1[0][2]) {
	 case 'm':
	    /* mixed mode: stderr, then switch to syslog */
	    diag_set('s', NULL);
	    xiosetopt('l', "m");
	    socat_opts.logopt = arg1[0][2];
	    xiosetopt('y', &arg1[0][3]);
	    break;
	 case 'y':
	 case 'f':
	 case 's':
	    /* >stderr, s>yslog, >file */
	    diag_set(arg1[0][2], &arg1[0][3]);
	    break;
	 case 'p':
	    diag_set('p', &arg1[0][3]);
	    break;
	 case 'u':
	    diag_set('u', NULL);
	    break;
	 default:
	    Error1("unknown log option \"%s\"; use option \"-?\" for help", arg1[0]);
	    break;
	 }
	 break;
      case 'v': socat_opts.verbose = true; break;
      case 'x': socat_opts.verbhex = true; break;
      case 'b': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -b requires an argument; use option \"-?\" for help");
	       socat_usage(stderr); Exit(1);
	    }
	 }
	 socat_opts.bufsiz = strtoul(a, (char **)&a, 0);
	 break;
      case 's':
	 {
	    char l[2];
	    l[0] = '0'+E_FATAL; l[1] = '\0';
	    diag_set('e', l); break;
	 }
      case 't': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -t requires an argument; use option \"-?\" for help");
	       socat_usage(stderr); Exit(1);
	    }
	 }
	 rto = strtod(a, (char **)&a);
	 socat_opts.closwait.tv_sec = rto;
	 socat_opts.closwait.tv_usec =
	    (rto-socat_opts.closwait.tv_sec) * 1000000; 
	 break;
      case 'u': socat_opts.lefttoright = true; break;
      case 'U': socat_opts.righttoleft = true; break;
      case 'g': xioopts_ignoregroups = true; break;
      case '\0':
      case ',':
      case ':': break;	/* this "-" is a variation of STDIO */
      default:
	 xioinqopt('p', buff, sizeof(buff));
	 if (arg1[0][1] == buff[0]) {
	    break;
	 }
	 Error1("unknown option \"%s\"; use option \"-?\" for help", arg1[0]);
	 socat_usage(stderr); Exit(1);
      }
      /* the leading "-" might be a form of the first address */
      xioinqopt('p', buff, sizeof(buff));
      if (arg1[0][0] == '-' &&
	  (arg1[0][1] == '\0' || arg1[0][1] == ':' ||
	   arg1[0][1] == ',' || arg1[0][1] == buff[0]))
	 break;
      ++arg1; --argc;
   }
   if (argc != 2) {
      Error1("exactly 2 addresses required (there are %d); use option \"-?\" for help", argc);
      /*0 socat_usage(stderr);*/
      Exit(1);
   }
   if (socat_opts.lefttoright && socat_opts.righttoleft) {
      Error("-U and -u must not be combined");
   }

   Info(copyright_socat);
#if WITH_OPENSSL
   Info(copyright_openssl);
   Info(copyright_ssleay);
#endif
   Debug2("socat version %s on %s", socatversion, timestamp);
   uname(&ubuf);	/* ! here we circumvent internal tracing (Uname) */
   Debug4("running on %s version %s, release %s, machine %s\n",
	   ubuf.sysname, ubuf.version, ubuf.release, ubuf.machine);

#if WITH_MSGLEVEL <= E_DEBUG
   for (i = 0; i < argc0; ++i) {
      Debug2("argv[%d]: \"%s\"", i, argv[i]);
   }
#endif /* WITH_MSGLEVEL <= E_DEBUG */

   /* not sure what signal should print a message */
   Signal(SIGHUP, socat_signal);
   Signal(SIGINT, socat_signal);
   Signal(SIGQUIT, socat_signal);
   Signal(SIGILL, socat_signal);
   /* SIGABRT for assert; catching caused endless recursion on assert in libc
      (tzfile.c:498: __tzfile_compute: Assertion 'num_types == 1' failed.) */
   /*Signal(SIGABRT, socat_signal);*/
   Signal(SIGBUS, socat_signal);
   Signal(SIGFPE, socat_signal);
   Signal(SIGSEGV, socat_signal);
   Signal(SIGTERM, socat_signal);
   result = socat(arg1[0], arg1[1]);
   Notice1("exiting with status %d", result);
   Exit(result);
   return 0;	/* not reached, just for gcc -Wall */
}


void socat_usage(FILE *fd) {
   fputs(copyright_socat, fd); fputc('\n', fd);
   fputs("Usage:\n", fd);
   fputs("socat [options] <bi-address> <bi-address>\n", fd);
   fputs("   options:\n", fd);
   fputs("      -V     print version and feature information to stdout, and exit\n", fd);
#if WITH_HELP
   fputs("      -?     print a help text describing command line options and addresses\n", fd);
   fputs("      -??    like -?, plus a list of all common address option names\n", fd);
   fputs("      -???    like -?, plus a list of all available address option names\n", fd);
#endif /* WITH_HELP */
   fputs("      -d     increase verbosity (use up to 4 times; 2 are recommended)\n", fd);
#if WITH_FILAN
   fputs("      -D     analyze file descriptors before loop\n", fd);
#endif
   fputs("      -ly[facility]  log to syslog, using facility (default is daemon)\n", fd);
   fputs("      -lf<logfile>   log to file\n", fd);
   fputs("      -ls            log to stderr (default if no other log)\n", fd);
   fputs("      -lm[facility]  mixed log mode (stderr during initialization, then syslog)\n", fd);
   fputs("      -lp<progname>  set the program name used for logging\n", fd);
   fputs("      -lu            use microseconds for logging timestamps\n", fd);
   fputs("      -v     verbose data traffic, text\n", fd);
   fputs("      -x     verbose data traffic, hexadecimal\n", fd);
   fputs("      -b<size_t>     set data buffer size (8192)\n", fd);
   fputs("      -s     sloppy (continue on error)\n", fd);
   fputs("      -t<timeout>    wait seconds before closing second channel\n", fd);
   fputs("      -u     unidirectional mode (left to right)\n", fd);
   fputs("      -U     unidirectional mode (right to left)\n", fd);
   fputs("      -g     do not check option groups\n", fd);
}


void socat_version(FILE *fd) {
   struct utsname ubuf;

   fputs(copyright_socat, fd); fputc('\n', fd);
   fprintf(fd, "socat version %s on %s\n", socatversion, timestamp);
   Uname(&ubuf);
   fprintf(fd, "   running on %s version %s, release %s, machine %s\n",
	   ubuf.sysname, ubuf.version, ubuf.release, ubuf.machine);
   fputs("features:\n", fd);
#ifdef WITH_STDIO
   fprintf(fd, "  #define WITH_STDIO %d\n", WITH_STDIO);
#else
   fputs("  #undef WITH_STDIO\n", fd);
#endif
#ifdef WITH_FDNUM
   fprintf(fd, "  #define WITH_FDNUM %d\n", WITH_FDNUM);
#else
   fputs("  #undef WITH_FDNUM\n", fd);
#endif
#ifdef WITH_FILE
   fprintf(fd, "  #define WITH_FILE %d\n", WITH_FILE);
#else
   fputs("  #undef WITH_FILE\n", fd);
#endif
#ifdef WITH_CREAT
   fprintf(fd, "  #define WITH_CREAT %d\n", WITH_CREAT);
#else
   fputs("  #undef WITH_CREAT\n", fd);
#endif
#ifdef WITH_GOPEN
   fprintf(fd, "  #define WITH_GOPEN %d\n", WITH_GOPEN);
#else
   fputs("  #undef WITH_GOPEN\n", fd);
#endif
#ifdef WITH_TERMIOS
   fprintf(fd, "  #define WITH_TERMIOS %d\n", WITH_TERMIOS);
#else
   fputs("  #undef WITH_TERMIOS\n", fd);
#endif
#ifdef WITH_PIPE
   fprintf(fd, "  #define WITH_PIPE %d\n", WITH_PIPE);
#else
   fputs("  #undef WITH_PIPE\n", fd);
#endif
#ifdef WITH_UNIX
   fprintf(fd, "  #define WITH_UNIX %d\n", WITH_UNIX);
#else
   fputs("  #undef WITH_UNIX\n", fd);
#endif /* WITH_UNIX */
#ifdef WITH_IP4
   fprintf(fd, "  #define WITH_IP4 %d\n", WITH_IP4);
#else
   fputs("  #undef WITH_IP4\n", fd);
#endif
#ifdef WITH_IP6
   fprintf(fd, "  #define WITH_IP6 %d\n", WITH_IP6);
#else
   fputs("  #undef WITH_IP6\n", fd);
#endif
#ifdef WITH_RAWIP
   fprintf(fd, "  #define WITH_RAWIP %d\n", WITH_RAWIP);
#else
   fputs("  #undef WITH_RAWIP\n", fd);
#endif
#ifdef WITH_TCP
   fprintf(fd, "  #define WITH_TCP %d\n", WITH_TCP);
#else
   fputs("  #undef WITH_TCP\n", fd);
#endif
#ifdef WITH_UDP
   fprintf(fd, "  #define WITH_UDP %d\n", WITH_UDP);
#else
   fputs("  #undef WITH_UDP\n", fd);
#endif
#ifdef WITH_LISTEN
   fprintf(fd, "  #define WITH_LISTEN %d\n", WITH_LISTEN);
#else
   fputs("  #undef WITH_LISTEN\n", fd);
#endif
#ifdef WITH_SOCKS4
   fprintf(fd, "  #define WITH_SOCKS4 %d\n", WITH_SOCKS4);
#else
   fputs("  #undef WITH_SOCKS4\n", fd);
#endif
#ifdef WITH_SOCKS4A
   fprintf(fd, "  #define WITH_SOCKS4A %d\n", WITH_SOCKS4A);
#else
   fputs("  #undef WITH_SOCKS4A\n", fd);
#endif
#ifdef WITH_PROXY
   fprintf(fd, "  #define WITH_PROXY %d\n", WITH_PROXY);
#else
   fputs("  #undef WITH_PROXY\n", fd);
#endif
#ifdef WITH_SYSTEM
   fprintf(fd, "  #define WITH_SYSTEM %d\n", WITH_SYSTEM);
#else
   fputs("  #undef WITH_SYSTEM\n", fd);
#endif
#ifdef WITH_EXEC
   fprintf(fd, "  #define WITH_EXEC %d\n", WITH_EXEC);
#else
   fputs("  #undef WITH_EXEC\n", fd);
#endif
#ifdef WITH_READLINE
   fprintf(fd, "  #define WITH_READLINE %d\n", WITH_READLINE);
#else
   fputs("  #undef WITH_READLINE\n", fd);
#endif
#ifdef WITH_PTY
   fprintf(fd, "  #define WITH_PTY %d\n", WITH_PTY);
#else
   fputs("  #undef WITH_PTY\n", fd);
#endif
#ifdef WITH_OPENSSL
   fprintf(fd, "  #define WITH_OPENSSL %d\n", WITH_OPENSSL);
#else
   fputs("  #undef WITH_OPENSSL\n", fd);
#endif
#ifdef WITH_LIBWRAP
   fprintf(fd, "  #define WITH_LIBWRAP %d\n", WITH_LIBWRAP);
#else
   fputs("  #undef WITH_LIBWRAP\n", fd);
#endif
#ifdef WITH_SYCLS
   fprintf(fd, "  #define WITH_SYCLS %d\n", WITH_SYCLS);
#else
   fputs("  #undef WITH_SYCLS\n", fd);
#endif
#ifdef WITH_FILAN
   fprintf(fd, "  #define WITH_FILAN %d\n", WITH_FILAN);
#else
   fputs("  #undef WITH_FILAN\n", fd);
#endif
#ifdef WITH_RETRY
   fprintf(fd, "  #define WITH_RETRY %d\n", WITH_RETRY);
#else
   fputs("  #undef WITH_RETRY\n", fd);
#endif
#ifdef WITH_MSGLEVEL
   fprintf(fd, "  #define WITH_MSGLEVEL %d /*%s*/\n", WITH_MSGLEVEL,
	   &"debug\0\0\0info\0\0\0\0notice\0\0warn\0\0\0\0error\0\0\0fatal\0\0\0"[WITH_MSGLEVEL<<3]);
#else
   fputs("  #undef WITH_MSGLEVEL\n", fd);
#endif
}


xiofile_t *sock1, *sock2;
int closing = 0;	/* 0..no eof yet, 1..first eof just occurred,
			   2..counting down closing timeout */

/* call this function when the common command line options are parsed, and the
   addresses are extracted (but not resolved). */
int socat(const char *address1, const char *address2) {

#if 1
   if (Signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
      Warn1("signal(SIGPIPE, SIG_IGN): %s", strerror(errno));
   }
#endif

   if (socat_opts.lefttoright) {
      if ((sock1 = xioopen(address1, O_RDONLY)) == NULL) {
	 return -1;
      }
   } else if (socat_opts.righttoleft) {
      if ((sock1 = xioopen(address1, O_WRONLY)) == NULL) {
	 return -1;
      }
   } else {
      if ((sock1 = xioopen(address1, O_RDWR)) == NULL) {
	 return -1;
      }
   }
#if 1	/*! */
   if (XIO_READABLE(sock1) &&
       (XIO_RDSTREAM(sock1)->howtoend == END_KILL ||
	XIO_RDSTREAM(sock1)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(sock1)->howtoend == END_SHUTDOWN_KILL)) {
      if (XIO_RDSTREAM(sock1)->para.exec.pid == diedunknown1) {
	 /* child has alread died... but it might have put regular data into
	    the communication channel, so continue */
	 Info1("child "F_pid" has already died (diedunknown1)",
	       XIO_RDSTREAM(sock1)->para.exec.pid);
	 diedunknown1 = 0;
	 XIO_RDSTREAM(sock1)->para.exec.pid = 0;
	 /* return STAT_RETRYLATER; */
      } else if (XIO_RDSTREAM(sock1)->para.exec.pid == diedunknown2) {
	 Info1("child "F_pid" has already died (diedunknown2)",
	       XIO_RDSTREAM(sock1)->para.exec.pid);
	 diedunknown2 = 0;
	 XIO_RDSTREAM(sock1)->para.exec.pid = 0;
      } else if (XIO_RDSTREAM(sock1)->para.exec.pid == diedunknown3) {
	 Info1("child "F_pid" has already died (diedunknown3)",
	       XIO_RDSTREAM(sock1)->para.exec.pid);
	 diedunknown3 = 0;
	 XIO_RDSTREAM(sock1)->para.exec.pid = 0;
      } else if (XIO_RDSTREAM(sock1)->para.exec.pid == diedunknown4) {
	 Info1("child "F_pid" has already died (diedunknown4)",
	       XIO_RDSTREAM(sock1)->para.exec.pid);
	 diedunknown4 = 0;
	 XIO_RDSTREAM(sock1)->para.exec.pid = 0;
      }
   }
#endif

   if (XIO_WRITABLE(sock1)) {
      if (XIO_READABLE(sock1)) {
	 if ((sock2 = xioopen(address2, O_RDWR)) == NULL) {
	    return -1;
	 }
      } else {
	 if ((sock2 = xioopen(address2, O_RDONLY)) == NULL) {
	    return -1;
	 }
      }
   } else {	/* assuming sock1 is readable */
      if ((sock2 = xioopen(address2, O_WRONLY)) == NULL) {
	 return -1;
      }
   }
#if 1	/*! */
   if (XIO_READABLE(sock2) &&
       (XIO_RDSTREAM(sock2)->howtoend == END_KILL ||
	XIO_RDSTREAM(sock2)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(sock2)->howtoend == END_SHUTDOWN_KILL)) {
      if (XIO_RDSTREAM(sock2)->para.exec.pid == diedunknown1) {
	 /* child has alread died... but it might have put regular data into
	    the communication channel, so continue */
	 Info1("child "F_pid" has already died (diedunknown1)",
	       XIO_RDSTREAM(sock2)->para.exec.pid);
	 diedunknown1 = 0;
	 XIO_RDSTREAM(sock2)->para.exec.pid = 0;
	 /* return STAT_RETRYLATER; */
      } else if (XIO_RDSTREAM(sock2)->para.exec.pid == diedunknown2) {
	 Info1("child "F_pid" has already died (diedunknown2)",
	       XIO_RDSTREAM(sock2)->para.exec.pid);
	 diedunknown2 = 0;
	 XIO_RDSTREAM(sock2)->para.exec.pid = 0;
      } else if (XIO_RDSTREAM(sock2)->para.exec.pid == diedunknown3) {
	 Info1("child "F_pid" has already died (diedunknown3)",
	       XIO_RDSTREAM(sock2)->para.exec.pid);
	 diedunknown3 = 0;
	 XIO_RDSTREAM(sock2)->para.exec.pid = 0;
      } else if (XIO_RDSTREAM(sock2)->para.exec.pid == diedunknown4) {
	 Info1("child "F_pid" has already died (diedunknown4)",
	       XIO_RDSTREAM(sock2)->para.exec.pid);
	 diedunknown4 = 0;
	 XIO_RDSTREAM(sock2)->para.exec.pid = 0;
      }
   }
#endif

   Info("resolved and opened all sock addresses");
   return 
      _socat();	/* nsocks, sockets are visible outside function */
}

/* checks if this is a connection to a child process, and if so, sees if the
   child already died, leaving some data for us.
   returns <0 if an error occurred;
   returns 0 if no child or not yet died or died without data (sets eof);
   returns >0 if child died and left data
*/
int childleftdata(xiofile_t *xfd) {
   fd_set in, out, expt;
   int retval;
   /* have to check if a child process died before, but left read data */
   if (XIO_READABLE(xfd) &&
       (XIO_RDSTREAM(xfd)->howtoend == END_KILL ||
	XIO_RDSTREAM(xfd)->howtoend == END_CLOSE_KILL ||
	XIO_RDSTREAM(xfd)->howtoend == END_SHUTDOWN_KILL) &&
       XIO_RDSTREAM(xfd)->para.exec.pid == 0) {
      struct timeval time0 = { 0,0 };

      FD_ZERO(&in); FD_ZERO(&out); FD_ZERO(&expt);
      if (XIO_READABLE(xfd) && !(XIO_RDSTREAM(xfd)->eof >= 2 && !XIO_RDSTREAM(xfd)->ignoreeof)) {
	 FD_SET(XIO_GETRDFD(xfd), &in);
	 /*0 FD_SET(XIO_GETRDFD(xfd), &expt);*/
      }
      do {
	 retval = Select(FOPEN_MAX, &in, &out, &expt, &time0);
      } while (retval < 0 && errno == EINTR);

      if (retval < 0) {
#if HAVE_FDS_BITS
	 Error5("select(%d, &%lu, &%lu, &%lu, {0}): %s",
		FOPEN_MAX, in.fds_bits[0], out.fds_bits[0],
		expt.fds_bits[0], strerror(errno));
#else
	 Error5("select(%d, &%lu, &%lu, &%lu, {0}): %s",
		FOPEN_MAX, in.__fds_bits[0], out.__fds_bits[0],
		expt.__fds_bits[0], strerror(errno));
#endif
	 return -1;
      } else if (retval == 0) {
	 Info("terminated child did not leave data for us");
	 XIO_RDSTREAM(xfd)->eof = 2;
	 xfd->stream.eof = 2;
	 closing = MAX(closing, 1);
      }
   }
   return 0;
}

int xiotransfer(xiofile_t *inpipe, xiofile_t *outpipe,
		unsigned char **buff, size_t bufsiz, const char *prefix);

int sleepon = 0;	/* if last select was not active, then sleep so long */
bool mayrd1;		/* sock1 has read data or eof, according to select() */
bool mayrd2;		/* sock2 has read data or eof, according to select() */
bool maywr1;		/* sock1 can be written to, according to select() */
bool maywr2;		/* sock2 can be written to, according to select() */

/* here we come when the sockets are opened (in the meaning of C language),
   and their options are set/applied */
int _socat(void) {
   fd_set in, out, expt;
   struct timeval timeout, *to;
   int retval;
   unsigned char *buff;
   ssize_t bytes1, bytes2;
   int wasaction = 1;	/* last select was active, do NOT sleep before next */

#if WITH_FILAN
   if (socat_opts.debug) {
      int fdi, fdo;
      int msglevel, exitlevel;
      char diagtxt[2];

      msglevel = diag_get_int('D');	/* save current message level */
      diag_set('D', "4");	/* only print errors and fatals in filan */
      exitlevel = diag_get_int('e');	/* save current exit level */
      diag_set('e', "5");	/* only exit on fatals */

      fdi = XIO_GETRDFD(sock1);
      fdo = XIO_GETWRFD(sock1);
      filan(fdi, stderr);
      if (fdo != fdi) {
	 filan(fdo, stderr);
      }

      fdi = XIO_GETRDFD(sock2);
      fdo = XIO_GETWRFD(sock2);
      filan(fdi, stderr);
      if (fdo != fdi) {
	 filan(fdo, stderr);
      }

      diagtxt[0] = exitlevel+'0'; diagtxt[1] = '\0';
      diag_set('e', diagtxt);	/* restore old exit level */
      diagtxt[0] = msglevel+'0'; diagtxt[1] = '\0';
      diag_set('D', diagtxt);	/* restore old message level */
   }
#endif /* WITH_FILAN */

   buff = Malloc(2*socat_opts.bufsiz+1);
   if (buff == NULL)  return -1;

   if (socat_opts.timeout) {
      /*! timeout = socat_opts.timeout;*/	/*!?*/
      to = &timeout;
   } else {
      to = NULL;
   }

   if (socat_opts.logopt == 'm' && xioinqopt('l', NULL, 0) == 'm') {
      Info("switching to syslog");
      diag_set('y', xioopts.syslogfac);
      xiosetopt('l', "\0");
   }
   Notice4("starting data transfer loop with [%d,%d] and [%d,%d]",
	   XIO_GETRDFD(sock1), XIO_GETWRFD(sock1),
	   XIO_GETRDFD(sock2), XIO_GETWRFD(sock2));
   while (XIO_RDSTREAM(sock1)->eof <= 1 ||
	  XIO_RDSTREAM(sock2)->eof <= 1) {
      Debug5("data loop: sock1->eof=%d, sock2->eof=%d, closing=%d, sleepon=%d, wasaction=%d",
	     XIO_RDSTREAM(sock1)->eof, XIO_RDSTREAM(sock2)->eof, closing, sleepon, wasaction);

      if (!wasaction) {
	 Sleep(sleepon);
      }
      wasaction = 0;

#if 0
      if (sock1->fd[0].fd >= 0 &&
	  !(/*sock1->fd[0].ignoreeof &&*/ XIO_RDSTREAM(sock1)->eof)) {
	 if (XIO_READABLE(sock1))
	    FD_SET(sock1->fd[0].fd, &in);
	 FD_SET(sock1->fd[0].fd, &expt); }
#endif
      do {
	 FD_ZERO(&in); FD_ZERO(&out); FD_ZERO(&expt);

	 childleftdata(sock1);
	 childleftdata(sock2);

	 if (XIO_READABLE(sock1) &&
	     !(XIO_RDSTREAM(sock1)->eof > 1 && !XIO_RDSTREAM(sock1)->ignoreeof) &&
	     !socat_opts.righttoleft) {
	    if (!mayrd1) {
	       FD_SET(XIO_GETRDFD(sock1), &in);
	       /*0 FD_SET(XIO_GETRDFD(sock1), &expt);*/
	    }
	    if (!maywr2) {
	       FD_SET(XIO_GETWRFD(sock2), &out);
	    }
	 }
	 if (XIO_READABLE(sock2) &&
	     !(XIO_RDSTREAM(sock2)->eof > 1 && !XIO_RDSTREAM(sock2)->ignoreeof) &&
	     !socat_opts.lefttoright) {
	    if (!mayrd2) {
	       FD_SET(XIO_GETRDFD(sock2), &in);
	       /*0 FD_SET(XIO_GETRDFD(sock2), &expt);*/
	    }
	    if (!maywr1) {
	       FD_SET(XIO_GETWRFD(sock1), &out);
	    }
	 }

	 if (!closing && to) {
	    to->tv_sec  = socat_opts.timeout;
	    to->tv_usec = 0;
	 } else if (closing==1) {
	    /* first eof just occurred, start end timer */
	    to = &timeout;
	    timeout = socat_opts.closwait;
	    closing = 2;
	 }
	 retval = Select(FOPEN_MAX, &in, &out, &expt, to);
      } while (retval < 0 && errno == EINTR);

      /* attention:
	 when an exec'd process sends data and terminates, it is unpredictable
	 whether the data or the sigchild arrives first.
	 
	 */

      if (retval < 0) {
#if HAVE_FDS_BITS
	    Error7("select(%d, &%lu, &%lu, &%lu, %s%lu): %s",
		   FOPEN_MAX, in.fds_bits[0], out.fds_bits[0],
		   expt.fds_bits[0], to?"&":"NULL/", to?to->tv_sec:0,
		   strerror(errno));
#else
	    Error7("select(%d, &%lu, &%lu, &%lu, %s%lu): %s",
		   FOPEN_MAX, in.__fds_bits[0], out.__fds_bits[0],
		   expt.__fds_bits[0], to?"&":"NULL/", to?to->tv_sec:0,
		   strerror(errno));
#endif
	    return -1;
      } else if (retval == 0) {
	 Info1("select timed out (no data within %d seconds)",
	       socat_opts.timeout); /*! wrong source for value */
	 if (closing) {
	    break;
	 }
	 /* one possibility to come here is ignoreeof on some fd, but no EOF 
	    and no data on any descriptor - this is no indication for end! */
	 continue;
      }

      if (XIO_READABLE(sock1) && XIO_GETRDFD(sock1) >= 0 &&
	  FD_ISSET(XIO_GETRDFD(sock1), &in)) {
	 mayrd1 = true;
      }
      if (XIO_READABLE(sock2) && XIO_GETRDFD(sock2) >= 0 &&
	  FD_ISSET(XIO_GETRDFD(sock2), &in)) {
	 mayrd2 = true;
      }
      if (XIO_GETWRFD(sock1) >= 0 && FD_ISSET(XIO_GETWRFD(sock1), &out)) {
	 maywr1 = true;
      }
      if (XIO_GETWRFD(sock2) >= 0 && FD_ISSET(XIO_GETWRFD(sock2), &out)) {
	 maywr2 = true;
      }

      if (mayrd1 && maywr2) {
	 mayrd1 = maywr2 = false;
	 if ((bytes1 = xiotransfer(sock1, sock2, &buff, socat_opts.bufsiz, "> "))
	     < 0) {
	    if (errno != EAGAIN) {
	       closing = MAX(closing, 1);
	    }
	 } else if (bytes1 > 0) {
	    wasaction = 1;
	 }
      } else {	/* (bytes == 0)  handled later */
	 bytes1 = -1;
      }

      if (mayrd2 && maywr1) {
	 mayrd2 = maywr1 = false;
	 if ((bytes2 = xiotransfer(sock2, sock1, &buff, socat_opts.bufsiz, "< "))
	     < 0) {
	    if (errno != EAGAIN) {
	       closing = MAX(closing, 1);
	    }
	 } else if (bytes2 > 0) {
	    wasaction = 1;
	 }
      } else {
	 bytes2 = -1;
      }

      /* NOW handle EOFs */

      if (bytes1 == 0) {
	 if (XIO_RDSTREAM(sock1)->ignoreeof && !closing) {
	    Debug1("socket 1 (fd %d) is at EOF, ignoring",
		   XIO_RDSTREAM(sock1)->fd);	/*! */
	    sleepon = 1;
	 } else {
	    Notice1("socket 1 (fd %d) is at EOF", XIO_GETRDFD(sock1));
	    xioshutdown(sock2, 1);
	    if (socat_opts.lefttoright) {
	       break;
	    }
	 }
      }

      if (bytes2 == 0) {
	 if (XIO_RDSTREAM(sock2)->ignoreeof && !closing) {
	    Debug1("socket 2 (fd %d) is at EOF, ignoring",
		   XIO_RDSTREAM(sock2)->fd);
	    sleepon = 1;
	 } else {
	    Notice1("socket 2 (fd %d) is at EOF", XIO_GETRDFD(sock2));
	    xioshutdown(sock1, 1);
	    if (socat_opts.righttoleft) {
	       break;
	    }
	 }
      }

   }

   /* close everything that's still open */
   xioclose(sock1);
   xioclose(sock2);

   return 0;
}


/* inpipe is suspected to have read data available; read at most bufsiz bytes
   and transfer them to outpipe. Perform required data conversions.
   buff should be at least twice as large as bufsiz, to allow all standard
   conversions. Returns the number of bytes written, or 0 on EOF or <0 if an
   error occurred or when data was read but none written due to conversions
   (with EAGAIN). 
   If 0 bytes were read (EOF), it does NOT shutdown or close a channel, and it
   does NOT write 0 bytes.
   */
/* inpipe, outpipe must be single descriptors (not dual!) */
int xiotransfer(xiofile_t *inpipe, xiofile_t *outpipe,
		unsigned char **buff, size_t bufsiz, const char *prefix) {
   ssize_t bytes, writt;

	 bytes = xioread(inpipe, *buff, socat_opts.bufsiz);
	 if (bytes < 0) {
	    XIO_RDSTREAM(inpipe)->eof = 2;
	    /*xioshutdown(inpipe, 0);*/
	    return -1;
	 }
	 if (bytes == 0 && XIO_RDSTREAM(inpipe)->ignoreeof && !closing) {
	    writt = 0;
	 } else if (bytes == 0) {
	    XIO_RDSTREAM(inpipe)->eof = 2;
	    closing = MAX(closing, 1);
	    writt = 0;
	 }

	 else /* if (bytes > 0)*/ {

	    if (XIO_RDSTREAM(inpipe)->lineterm !=
		XIO_WRSTREAM(outpipe)->lineterm) {
	       cv_newline(buff, &bytes,
			  XIO_RDSTREAM(inpipe)->lineterm,
			  XIO_WRSTREAM(outpipe)->lineterm);
	    }
	    if (bytes == 0) {
	       errno = EAGAIN;  return -1;
	    }

	    if (socat_opts.verbose && socat_opts.verbhex) {
	       /* Hack-o-rama */
	       size_t i = 0;
	       size_t j;
	       size_t N = 16;
	       const unsigned char *end, *s, *t;
	       s = *buff;
	       end = (*buff)+bytes;
	       while (s < end) {
		  fputs(prefix, stderr);
		  j = Min(N, (size_t)(end-s));

		  /* print hex */
		  t = s;
		  i = 0;
		  while (i < j) {
		     int c = *t++;
		     fprintf(stderr, " %02x", c);
		     ++i;
		     if (c == '\n')  break;
		  }

		  /* fill hex column */
		  while (i < N) {
		     fputs("   ", stderr);
		     ++i;
		  }
		  fputs("  ", stderr);

		  /* print acsii */
		  t = s;
		  i = 0;
		  while (i < j) {
		     int c = *t++;
		     if (c == '\n') {
			fputc('.', stderr);
			break;
		     }
		     if (!isprint(c))
			c = '.';
		     fputc(c, stderr);
		     ++i;
		  }

		  fputc('\n', stderr);
		  s = t;
	       }
	       fputs("--\n", stderr);
	    } else if (socat_opts.verbose) {
	       size_t i = 0;
	       fputs(prefix, stderr);
	       while (i < (size_t)bytes) {
		  int c = (*buff)[i];
		  if (i > 0 && (*buff)[i-1] == '\n')  fputs(prefix, stderr);
		  switch (c) {
		  case '\a' : fputs("\\a", stderr); break;
		  case '\b' : fputs("\\b", stderr); break;
		  case '\t' : fputs("\t", stderr); break;
		  case '\n' : fputs("\n", stderr); break;
		  case '\v' : fputs("\\v", stderr); break;
		  case '\f' : fputs("\\f", stderr); break;
		  case '\r' : fputs("\\r", stderr); break;
		  case '\\' : fputs("\\\\", stderr); break;
		  default:
		     if (!isprint(c))
			c = '.';
		     fputc(c, stderr);
		     break;
		  }
		  ++i;
	       }
	    } else if (socat_opts.verbhex) {
	       int i;
	       fputs(prefix, stderr);
	       for (i = 0; i < bytes; ++i) {
		  fprintf(stderr, " %02x", (*buff)[i]);
	       }
	       fputc('\n', stderr);
	    }

	    writt = xiowrite(outpipe, *buff, bytes);
	    if (writt < 0) {
	       return -1;
	    } else {
	       Info3("transferred "F_Zu" bytes from %d to %d",
		     writt, XIO_GETRDFD(inpipe), XIO_GETWRFD(outpipe));
	    }
	 }
   return writt;
}

#define CR '\r'
#define LF '\n'


int cv_newline(unsigned char **buff, ssize_t *bytes,
	       int lineterm1, int lineterm2) {
   /* must perform newline changes */
   if (lineterm1 <= LINETERM_CR && lineterm2 <= LINETERM_CR) {
      /* no change in data length */
      unsigned char from, to,  *p, *z;
      if (lineterm1 == LINETERM_RAW) {
	 from = '\n'; to = '\r';
      } else {
	 from = '\r'; to = '\n';
      }
      z = *buff + *bytes;
      p = *buff;
      while (p < z) {
	 if (*p == from)  *p = to;
	 ++p;
      }

   } else if (lineterm1 == LINETERM_CRNL) {
      /* buffer becomes shorter */
      unsigned char to,  *s, *t, *z;
      if (lineterm2 == LINETERM_RAW) {
	 to = '\n';
      } else {
	 to = '\r';
      }
      z = *buff + *bytes;
      s = t = *buff;
      while (s < z) {
	 if (*s == '\r') {
	    ++s;
	    continue;
	 }
	 if (*s == '\n') {
	    *t++ = to; ++s;
	 } else {
	    *t++ = *s++;
	 }
      }
      *bytes = t - *buff;
   } else {
      /* buffer becomes longer, must alloc another space */
      unsigned char *buf2;
      unsigned char from;  unsigned char *s, *t, *z;
      if (lineterm1 == LINETERM_RAW) {
	 from = '\n';
      } else {
	 from = '\r';
      }
      if ((buf2 = Malloc(2*socat_opts.bufsiz+1)) == NULL) {
	 return -1;
      }
      s = *buff;  t = buf2;  z = *buff + *bytes;
      while (s < z) {
	 if (*s == from) {
	    *t++ = '\r'; *t++ = '\n';
	    ++s;
	    continue;
	 } else {
	    *t++ = *s++;
	 }
      }
      free(*buff);
      *buff = buf2;
      *bytes = t - buf2;;
   }
   return 0;
}

void socat_signal(int sig) {
   switch (sig) {
   case SIGQUIT:
   case SIGILL:
   case SIGABRT:
   case SIGBUS:
   case SIGFPE:
   case SIGSEGV:
   case SIGPIPE:
      Error1("exiting on signal %d", sig); break;
   case SIGTERM:
      Warn1("exiting on signal %d", sig); break;
   case SIGHUP:  
   case SIGINT:
      Notice1("exiting on signal %d", sig); break;
   }
   Exit(1);
}
