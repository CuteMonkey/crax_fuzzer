/* $Id: procan_main.c,v 1.6 2003/05/21 05:16:38 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

const char copyright[] = "procan by Gerhard Rieger - send bug reports to socat@dest-unreach.org";

#include <stdlib.h>	/* strtoul() */
#include <string.h>
#include <stdio.h>
#include "mytypes.h"
#include "error.h"
#include "filan.h"
#include "procan.h"


#define WITH_HELP 1

static void procan_usage(FILE *fd);


int main(int argc, const char *argv[]) {
   const char **arg1;
#if 0
   unsigned int n = 1024;	/* this is default on my Linux */
#endif

   diag_set('p', strchr(argv[0], '/') ? strrchr(argv[0], '/')+1 : argv[0]);

   arg1 = argv+1;  --argc;
   while (arg1[0] && (arg1[0][0] == '-')) {
      switch (arg1[0][1]) {
      case '?': procan_usage(stdout); exit(0);
#if LATER
      case 'V': procan_version(stdout); exit(0);
      case 'l': diag_set(arg1[0][2], &arg1[0][3]); break;
      case 'd': diag_set('d', NULL); break;
#endif
#if 0
      case 'n': n = strtoul(&arg1[0][2], NULL, 0); break;
#endif
      case '\0': break;
      default:
	 {
	    char l[2];
	    l[0] = '0'+E_FATAL; l[1] = '\0';
	    diag_set('e', l);
	    Error1("unknown option \"%s\"", arg1[0]);
#if WITH_HELP
	    procan_usage(stderr);
#endif
	 }
	 exit(1);
      }
      if (arg1[0][1] == '\0')
	 break;
      ++arg1; --argc;
   }
   if (argc != 0) {
      Error1("%d superfluous arguments", argc);
#if WITH_HELP
      procan_usage(stderr);
#endif
      exit(1);
   }
   procan(stdout);
   return 0;
}


#if WITH_HELP
static void procan_usage(FILE *fd) {
   fputs(copyright, fd); fputc('\n', fd);
   fputs("Analyze system parameters of process\n", fd);
   fputs("Usage:\n", fd);
   fputs("procan [options]\n", fd);
   fputs("   options:\n", fd);
#if LATER
   fputs("      -V     print version information to stdout, and exit\n", fd);
#endif
#if WITH_HELP
   fputs("      -?     print a help text describing command line options\n", fd);
#endif
#if LATER
   fputs("      -d     increase verbosity (use up to 4 times; 2 are recommended)\n", fd);
#endif
#if 0
   fputs("      -ly[facility]  log to syslog, using facility (default is daemon)\n", fd);
   fputs("      -lf<logfile>   log to file\n", fd);
   fputs("      -ls            log to stderr (default if no other log)\n", fd);
#endif
#if 0
   fputs("      -n<fdnum>      first file descriptor number not analyzed\n", fd);
#endif
}
#endif /* WITH_HELP */
