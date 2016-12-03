/* $Id: filan_main.c,v 1.11 2003/06/26 12:03:40 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

const char copyright[] = "filan by Gerhard Rieger - send bug reports to socat@dest-unreach.org";

#include "config.h"
#include "xioconfig.h"
#include "sysincludes.h"

#include "mytypes.h"
#include "compat.h"
#include "error.h"
#include "sycls.h"
#include "filan.h"


#define WITH_HELP 1

static void filan_usage(FILE *fd);


int main(int argc, const char *argv[]) {
   const char **arg1, *a;
   const char *filename = NULL;
   unsigned int m = 0, n = 1024;	/* this is default on my Linux */
   unsigned int i;
   int style = 0;

   diag_set('p', strchr(argv[0], '/') ? strrchr(argv[0], '/')+1 : argv[0]);

   arg1 = argv+1;  --argc;
   while (arg1[0] && (arg1[0][0] == '-')) {
      switch (arg1[0][1]) {
#if WITH_HELP
      case '?': filan_usage(stdout); exit(0);
#endif
#if LATER
      case 'V': filan_version(stdout); exit(0);
      case 'l': diag_set(arg1[0][2], &arg1[0][3]); break;
#endif
      case 'd': diag_set('d', NULL); break;
      case 's': style = 1; break;
      case 'i':  if (arg1[0][2]) {
	    a = *arg1+2;
         } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -i requires an argument");
	       filan_usage(stderr); exit(1);
	    }
	 }
         m = strtoul(a, (char **)&a, 0);
	 n = m+1;
	 break;
      case 'n': if (arg1[0][2]) {
	    a = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((a = *arg1) == NULL) {
	       Error("option -n requires an argument");
	       filan_usage(stderr); exit(1);
	    }
	 }
         n = strtoul(a, (char **)&a, 0);
	 break;
      case 'f': if (arg1[0][2]) {
	    filename = *arg1+2;
	 } else {
	    ++arg1, --argc;
	    if ((filename = *arg1) == NULL) {
	       Error("option -f requires an argument");
	       filan_usage(stderr); exit(1);
	    }
	 }
	 break;
      case '\0': break;
      default:
	 {
	    char l[2];
	    l[0] = '0'+E_FATAL; l[1] = '\0';
	    diag_set('e', l);
	    Error1("unknown option %s", arg1[0]);
#if WITH_HELP
	    filan_usage(stderr);
#endif
	 }
	 exit(1);
      }
#if 0
      if (arg1[0][1] == '\0')
	 break;
#endif
      ++arg1; --argc;
   }
   if (argc != 0) {
      Error1("%d superfluous arguments", argc);
      filan_usage(stderr);
      exit(1);
   }
   if (style == 0) {
      /* this style gives detailled infos, but requires a file descriptor */
      if (filename) {
	 struct stat buf;
	 int fd;

	 if (Stat(filename, &buf) < 0) {
	    Error3("stat(\"%s\", %p): %s", filename, &buf, strerror(errno));
	 }
	 /* note: when S_ISSOCK was undefined, it always gives 0 */
	 if (S_ISSOCK(buf.st_mode)) {
	    Error("cannot analyze UNIX domain socket");
	 }
	 if ((fd =
	      Open(filename, O_RDONLY|O_NOCTTY|O_NONBLOCK
#ifdef O_LARGEFILE
		   |O_LARGEFILE
#endif
		   , 0700))
	     < 0) {
	    Error2("open(\"%s\", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_LARGEFILE, 0700): %s",
		   filename, strerror(errno));
	 }
	 filan(fd, stdout);
      } else {
	 for (i = m; i < n; ++i) {
	    filan(i, stdout);
	 }
      }
   } else {
      /* this style gives only type and path / socket addresses, and works from
	 file descriptor or filename (with restrictions) */
      if (filename) {
	 /* filename: NULL means yet unknown; "" means no name at all */
	 int fd;
	 if ((fd =
	      Open(filename, O_RDONLY|O_NOCTTY|O_NONBLOCK
#ifdef O_LARGEFILE
		   |O_LARGEFILE
#endif
		   , 0700))
	     < 0) {
	    Debug2("open(\"%s\", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_LARGEFILE, 0700): %s",
		   filename, strerror(errno));
	 }
	 fdname(filename, fd, stdout);
      } else {
	 for (i = m; i < n; ++i) {
	    fdname("", i, stdout);
	 }
      }
   }
   return 0;
}


#if WITH_HELP
static void filan_usage(FILE *fd) {
   fputs(copyright, fd); fputc('\n', fd);
   fputs("Analyze file descriptors of the process\n", fd);
   fputs("Usage:\n", fd);
   fputs("filan [options]\n", fd);
   fputs("   options:\n", fd);
#if LATER
   fputs("      -V     print version information to stdout, and exit\n", fd);
#endif
#if WITH_HELP
   fputs("      -?             print this help text\n", fd);
   fputs("      -d             increase verbosity (use up to 4 times)\n", fd);
#endif
#if 0
   fputs("      -ly[facility]  log to syslog, using facility (default is daemon)\n", fd);
   fputs("      -lf<logfile>   log to file\n", fd);
   fputs("      -ls            log to stderr (default if no other log)\n", fd);
#endif
   fputs("      -i<fdnum>      only analyze this fd\n", fd);
   fputs("      -n<fdnum>      analyze all fds from 0 up to fdnum-1 (default: 1024)\n", fd);
   fputs("      -s             simple output with just type and socket address or path\n", fd);
   fputs("      -f<filename>   analyze file system entry\n", fd);
}
#endif /* WITH_HELP */
