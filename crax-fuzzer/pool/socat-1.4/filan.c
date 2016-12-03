/* $Id: filan.c,v 1.28 2003/09/27 19:03:05 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* the subroutine filan makes a "FILe descriptor ANalysis". It checks the
   type of file descriptor and tries to retrieve as much info as possible
   about it as it cat get without modifying its state.
   NOTE: it works on UNIX (kernel) file descriptors, not on libc files! */

#include "config.h"
#include "xioconfig.h"	/* what features are enabled */

#include "sysincludes.h"

#include "mytypes.h"
#include "compat.h"
#include "error.h"
#include "sycls.h"
#include "sysutils.h"

#include "filan.h"


struct sockopt {
   int so;
   char *name;
};

/* dirty workaround so we dont get an error on AIX when getting linked with
   libwrap */
int allow_severity, deny_severity;


int sockoptan(int fd, const struct sockopt *optname, int socklay, FILE *outfile);
int tcpan(int fd, FILE *outfile);


int filan(int fd, FILE *outfile) {
   static int headprinted;
   int result, cloexec, flags;
#if defined(F_GETOWN)
   int sigown;
#endif
#if defined(F_GETSIG)
   int sigio;
#endif /* defined(F_GETSIG) */
#if HAVE_STAT64
   struct stat64 buf = {0};
#else
   struct stat buf = {0};
#endif /* !HAVE_STAT64 */
   int filetype;
   static const char *filetypenames[] = {
      "undef","pipe","chrdev","undef","blkdev","undef","dir","undef",
      "file","undef","symlink","undef","socket","undef","undef","\"MT\"?"} ;
   Debug1("checking file descriptor %u", fd);
#if HAVE_STAT64
   result = Fstat64(fd, &buf);
#else
   result = Fstat(fd, &buf);
#endif /* !HAVE_STAT64 */
   if (result < 0) {
      if (errno == EBADF) {
         Debug2("fstat(%d): %s", fd, strerror(errno));
      } else {
         Error2("fstat(%d): %s", fd, strerror(errno));
      }
      return -1;
   }
   filetype = (buf.st_mode&S_IFMT)>>12;
   Debug2("fd %d is a %s", fd, filetypenames[filetype]);
   cloexec = Fcntl(fd, F_GETFD);
   flags   = Fcntl(fd, F_GETFL);
#if defined(F_GETOWN)
   sigown  = Fcntl(fd, F_GETOWN);
#endif
#if defined(F_GETSIG)
   sigio   = Fcntl(fd, F_GETSIG);
#endif /* defined(F_GETSIG) */
   if (!headprinted) {
      fputs("  FD  type\tdevice\tinode\tmode\tlinks\tuid\tgid"
#if HAVE_ST_RDEV
	    "\trdev"
#endif
	    "\tsize"
#if HAVE_ST_BLKSIZE
	    "\tblksize"
#endif
#if HAVE_ST_BLOCKS
	    "\tblocks"
#endif
	    "\tatime\t\tmtime\t\tctime\t\tcloexec\tflags"
#if defined(F_GETOWN)
	    "\tsigown"
#endif
	    , outfile);
#if defined(F_GETSIG)
      fputs("\tsigio", outfile);
#endif /* defined(F_GETSIG) */
      fputc('\n', outfile);
      headprinted = 1;
   }
   fprintf(outfile, "%4d: %s\t"F_st_dev"\t"
#if HAVE_STAT64
	   F_st64_ino
#else
	   F_st_ino
#endif /* HAVE_STAT64 */
	   "\t"F_mode"\t"F_st_nlink"\t"F_uid"\t"F_gid
#if HAVE_ST_RDEV
	   "\t%hu,%hu"
#endif
	   "\t"
#if HAVE_STAT64
	   F_st64_size
#else
	   F_st_size
#endif /* HAVE_STAT64 */
#if HAVE_ST_BLKSIZE
	   "\t"F_st_blksize
#endif
#if HAVE_ST_BLOCKS
#if HAVE_STAT64
	   "\t"F_st64_blocks
#else
	   "\t"F_st_blocks
#endif /* HAVE_STAT64 */
#endif
	   "\t"F_time"\t"F_time"\t"F_time,
	   fd, filetypenames[filetype],
	   buf.st_dev,
	   buf.st_ino,
	   buf.st_mode, buf.st_nlink, buf.st_uid,
	   buf.st_gid,
#if HAVE_ST_RDEV
	   (unsigned short)buf.st_rdev>>8, (unsigned short)buf.st_rdev&0xff,
#endif
	   buf.st_size,
#if HAVE_ST_BLKSIZE
	   buf.st_blksize,
#endif
#if HAVE_ST_BLOCKS
	   buf.st_blocks,	/* on Linux, this applies to stat and stat64 */
#endif
	   buf.st_atime, buf.st_mtime, buf.st_ctime);
   fprintf(outfile, "\t%d\tx%06x", cloexec, flags);
#if defined(F_GETOWN)
   fprintf(outfile, "\t%d", sigown);
#endif
#if defined(F_GETSIG)
   fprintf(outfile, "\t%d", sigio);
#endif /* defined(F_GETSIG) */

   /* now see for type specific infos */
   switch (filetype) {
   case (S_IFIFO>>12):	/* 1, FIFO */
      break;
   case (S_IFCHR>>12):	/* 2, character device */
      result = cdevan(fd, outfile);
      break;
   case (S_IFDIR>>12):	/* 4, directory */
      break;
   case (S_IFBLK>>12):	/* 6, block device */
      break;
   case (S_IFREG>>12):	/* 8, regular file */
      break;
   case (S_IFLNK>>12):	/* 10, symbolic link */
      break;
#ifdef S_IFSOCK
   case (S_IFSOCK>>12): /* 12, socket */
#if WITH_SOCKET
      result = sockan(fd, outfile);
#else
      Error("SOCKET support not compiled in");
      return -1;
#endif /* !WITH_SOCKET */
      break;
#endif /* S_IFSOCK */
   }
   /* ioctl() */
   fputc('\n', outfile);
   return 0;
}


#if LATER
int fdinfo(int fd) {
   int result;

   result = Fcntl(fd, F_GETFD);
   fcntl(fd, F_GETFL, );
   fcntl(fd, F_GETLK, );
#ifdef F_GETOWN
   fcntl(fd, F_GETOWN, );
#endif
#ifdef F_GETSIG
   fcntl(fd, F_GETSIG, );
#endif
}


int devinfo(int fd) {
  ioctl();
}
#endif


/* character device analysis */
int cdevan(int fd, FILE *outfile) {
   int ret;

   if ((ret = Isatty(fd)) < 0) {
      Error2("isatty(%d): %s", fd, strerror(errno));
      return -1;
   }
   if (ret > 0) {
      struct termios termarg;
      char *name;

      if ((name = Ttyname(fd)) == NULL) {
	 /*Error2("ttyname(%d): %s", fd, strerror(errno));*/
	 fputs("\tNULL", outfile);
      } else {
	 fprintf(outfile, "\t%s", name);
      }
      if (Tcgetattr(fd, &termarg) < 0) {
	 Error3("tcgetattr(%d, %p): %s", fd, &termarg, strerror(errno));
	 return -1;
      }
      fprintf(outfile, " \tIFLAGS=%08x OFLAGS=%08x CFLAGS=%08x LFLAGS=%08x",
	      termarg.c_iflag, termarg.c_oflag, termarg.c_cflag, termarg.c_lflag);
   }
   return 0;
}


#if WITH_SOCKET
int sockan(int fd, FILE *outfile) {
#define FILAN_OPTLEN 256
#define FILAN_NAMELEN 256
   socklen_t optlen;
   int result /*0, i*/;
   static const char *socktypes[] = {
      "undef", "STREAM", "DGRAM", "RAW", "RDM",
      "SEQPACKET", "undef", "undef", "undef", "undef", 
      "PACKET", "undef" } ;
   char nambuff[FILAN_NAMELEN];
   /* in Linux these optcodes are 'enum', but on AIX they are bits! */
   static const struct sockopt sockopts[] = {
      {SO_DEBUG, "DEBUG"},
      {SO_REUSEADDR, "REUSEADDR"},
      {SO_TYPE, "TYPE"},
      {SO_ERROR, "ERROR"},
      {SO_DONTROUTE, "DONTROUTE"},
      {SO_BROADCAST, "BROADCAST"},
      {SO_SNDBUF, "SNDBUF"},
      {SO_RCVBUF, "RCVBUF"},
      {SO_KEEPALIVE, "KEEPALIVE"},
      {SO_OOBINLINE, "OOBINLINE"},
#ifdef SO_NO_CHECK
      {SO_NO_CHECK, "NO_CHECK"},
#endif
#ifdef SO_PRIORITY
      {SO_PRIORITY, "PRIORITY"},
#endif
      {SO_LINGER, "LINGER"},
#ifdef SO_BSDCOMPAT
      {SO_BSDCOMPAT, "BSDCOMPAT"},
#endif
#ifdef SO_REUSEPORT
      {SO_REUSEPORT, "REUSEPORT"},
#endif /* defined(SO_REUSEPORT) */
#ifdef SO_PASSCRED
      {SO_PASSCRED, "PASSCRED"},
#endif
#ifdef SO_PEERCRED
      {SO_PEERCRED, "PEERCRED"},
#endif
#ifdef SO_RCVLOWAT
      {SO_RCVLOWAT, "RCVLOWAT"},
#endif
#ifdef SO_SNDLOWAT
      {SO_SNDLOWAT, "SNDLOWAT"},
#endif
#ifdef SO_RCVTIMEO
      {SO_RCVTIMEO, "RCVTIMEO"},
#endif
#ifdef SO_SNDTIMEO
      {SO_SNDTIMEO, "SNDTIMEO"},
#endif
#ifdef SO_SECURITY_AUTHENTICATION
      {SO_SECURITY_AUTHENTICATION, "SECURITY_AUTHENTICATION"},
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
      {SO_SECURITY_ENCRYPTION_TRANSPORT, "SECURITY_ENCRYPTION_TRANSPORT"},
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
      {SO_SECURITY_ENCRYPTION_NETWORK, "SECURITY_ENCRYPTION_NETWORK"},
#endif
#ifdef SO_BINDTODEVICE
      {SO_BINDTODEVICE, "BINDTODEVICE"},
#endif
#ifdef SO_ATTACH_FILTER
      {SO_ATTACH_FILTER, "ATTACH_FILTER"},
#endif
#ifdef SO_DETACH_FILTER
      {SO_DETACH_FILTER, "DETACH_FILTER"},
#endif
      {0, NULL} } ;
   char optval[FILAN_OPTLEN];
   const struct sockopt *optname;
   union sockaddr_union sockname, peername;	/* the longest I know of */
   socklen_t namelen;
#if 0 && defined(SIOCGIFNAME)
   /*Linux struct ifreq ifc = {{{ 0 }}};*/
   struct ifreq ifc = {{ 0 }};
#endif

   optlen = FILAN_OPTLEN;
   result = Getsockopt(fd, SOL_SOCKET, SO_TYPE, optval, &optlen);
   if (result < 0) {
      Debug4("getsockopt(%d, SOL_SOCKET, SO_TYPE, %p, {"F_socklen"}): %s",
	     fd, optval, optlen, strerror(errno));
   } else {
      Debug3("fd %d: socket of type %d (\"%s\")", fd, *(int *)optval,
	  socktypes[*(int *)optval]);
   }

   optname = sockopts; while (optname->so) {
      optlen = FILAN_OPTLEN;
      result =
	 Getsockopt(fd, SOL_SOCKET, optname->so, (void *)optval, &optlen);
      if (result < 0) {
	 Debug5("getsockopt(%d, SOL_SOCKET, %d, %p, {"F_socklen"}): %s",
		fd, optname->so, optval, optlen, strerror(errno));
	 fputc('\t', outfile);
      } else if (optlen == sizeof(int)) {
	 Debug2("getsockopt(,,, {%d}, %d)",
	     *(int *)optval, optlen);
	 /*Info2("%s: %d", optname->name, *(int *)optval);*/
	 fprintf(outfile, "%s=%d\t", optname->name, *(int *)optval);
      } else {
	 Debug3("getsockopt(,,, {%d,%d}, %d)",
	     ((int *)optval)[0], ((int *)optval)[1], optlen);
	 fprintf(outfile, "%s={%d,%d}\t", optname->name,
		 ((int *)optval)[0], ((int *)optval)[1]);
      }
      ++optname;
   }

   namelen = sizeof(sockname);
   result = Getsockname(fd, (struct sockaddr *)&sockname, &namelen);
   if (result < 0) {
      Error2("getsockname(%d): %s", fd, strerror(errno));
      return -1;
   }
   fputc('\t', outfile);
   fputs(sockaddr_info((struct sockaddr *)&sockname, nambuff, sizeof(nambuff)),
	 outfile);

   namelen = sizeof(peername);
   result = Getpeername(fd, (struct sockaddr *)&peername, &namelen);
   if (result < 0) {
      Error2("getpeername(%d): %s", fd, strerror(errno));
   }
   fputs(" <-> ", outfile);
   fprintf(outfile, "%s\t",
	   sockaddr_info((struct sockaddr *)&peername,
			 nambuff, sizeof(nambuff)));

#if 0 && defined(SIOCGIFNAME)
   if ((result = Ioctl(fd, SIOCGIFNAME, &ifc)) < 0) {
      Error3("ioctl(%d, SIOCGIFNAME, %p): %s", fd, &ifc, strerror(errno));
   } else {
      fprintf(outfile, "IFNAME=\"%s\"\t", ifc.ifr_name);
   }
#endif /* SIOCGIFNAME */

   switch (((struct sockaddr *)&sockname)->sa_family) {
#if WITH_UNIX
   case AF_UNIX:
      /* no options for unix domain sockets known yet -> no unixan() */
      result = 0;
      break;
#endif
#if WITH_IP4 || WITH_IP6
#if WITH_IP4
   case AF_INET:
#endif
#if WITH_IP6
   case AF_INET6:
#endif
      result = ipan(fd, outfile);
      break;
#endif
   default:
      fputs("**** NO FURTHER ANALYSIS FOR THIS SOCKET TYPE IMPLEMENTED", outfile);
      result = 0;
   }
   return result;
#undef FILAN_OPTLEN
#undef FILAN_NAMELEN
}
#endif /* WITH_SOCKET */


#if WITH_IP4 || WITH_IP6
/* prints the option values for the IP protocol and the IP based protocols */
/* no distinction between IP4 and IP6 yet */
int ipan(int fd, FILE *outfile) {
   /* in Linux these optcodes are 'enum', but on AIX they are bits! */
   static const struct sockopt ipopts[] = {
      {IP_TOS,          "IP_TOS"},
      {IP_TTL,          "IP_TTL"},
#ifdef IP_HDRINCL
      {IP_HDRINCL,      "IP_HDRINCL"},
#endif
#ifdef IP_OPTIONS
      {IP_OPTIONS,      "IP_OPTIONS"},
#endif
#ifdef IP_ROUTER_ALERT
      {IP_ROUTER_ALERT, "IP_ROUTER_ALERT"},
#endif
#ifdef IP_RECVOPTS
      {IP_RECVOPTS,     "IP_RECVOPTS"},
#endif
#ifdef IP_RETOPTS
      {IP_RETOPTS,      "IP_RETOPTS"},
#endif
#ifdef IP_PKTINFO
      {IP_PKTINFO,      "IP_PKTINFO"},
#endif
#ifdef IP_PKTOPTIONS
      {IP_PKTOPTIONS,   "IP_PKTOPTIONS"},
#endif
#ifdef IP_MTU_DISCOVER
      {IP_MTU_DISCOVER, "IP_MTU_DISCOVER"},
#endif
#ifdef IP_RECVERR
      {IP_RECVERR,      "IP_RECVERR"},
#endif
#ifdef IP_RECVTTL
      {IP_RECVTTL,      "IP_RECVTTL"},
#endif
#ifdef IP_RECVTOS
      {IP_RECVTOS,      "IP_RECVTOS"},
#endif
#ifdef IP_MTU
      {IP_MTU,          "IP_MTU"},
#endif
#ifdef IP_FREEBIND
      {IP_FREEBIND,       "IP_FREEBIND"},
#endif
#ifdef IP_MULTICAST_TTL
      {IP_MULTICAST_TTL,  "IP_MULTICAST_TTL"},
#endif
#ifdef IP_MULTICAST_LOOP
      {IP_MULTICAST_LOOP, "IP_MULTICAST_LOOP"},
#endif
      {0, NULL} } ;
   const struct sockopt *optname;
   int opttype;
   socklen_t optlen = sizeof(opttype);
   
   optname = ipopts; while (optname->so) {
      sockoptan(fd, optname, SOL_IP, outfile);
      ++optname;
   }
   /* want to pass the fd to the next layer protocol. dont know how to get the
      protocol number from the fd? use TYPE to identify TCP. */
   if (Getsockopt(fd, SOL_SOCKET, SO_TYPE, &opttype, &optlen) >= 0) {
      switch (opttype) {
#if WITH_TCP
      case SOCK_STREAM: tcpan(fd, outfile); break;
#endif
      }
   }
   return 0;
}
#endif /* WITH_IP */


#if WITH_TCP
int tcpan(int fd, FILE *outfile) {
   static const struct sockopt tcpopts[] = {
#ifdef TCP_NODELAY
      { TCP_NODELAY, "TCP_NODELAY" },
#endif
#ifdef TCP_MAXSEG
      { TCP_MAXSEG,  "TCP_MAXSEG" },
#endif
#ifdef TCP_STDURG
      { TCP_STDURG,  "TCP_STDURG" },
#endif
#ifdef TCP_RFC1323
      { TCP_RFC1323, "TCP_RFC1323" },
#endif
#ifdef TCP_CORK
      { TCP_CORK,    "TCP_CORK" },
#endif
#ifdef TCP_KEEPIDLE
      { TCP_KEEPIDLE, "TCP_KEEPIDLE" },
#endif
#ifdef TCP_KEEPINTVL
      { TCP_KEEPINTVL, "TCP_KEEPINTVL" },
#endif
#ifdef TCP_KEEPCNT
      { TCP_KEEPCNT, "TCP_KEEPCNT" },
#endif
#ifdef TCP_SYNCNT
      { TCP_SYNCNT, "TCP_SYNCNT" },
#endif
#ifdef TCP_LINGER2
      { TCP_LINGER2, "TCP_LINGER2" },
#endif
#ifdef TCP_DEFER_ACCEPT
      { TCP_DEFER_ACCEPT, "TCP_ACCEPT" },
#endif
#ifdef TCP_WINDOW_CLAMP
      { TCP_WINDOW_CLAMP, "TCP_WINDOW_CLAMP" },
#endif
#ifdef TCP_INFO
      { TCP_INFO, "TCP_INFO" },
#endif
#ifdef TCP_QUICKACK
      { TCP_QUICKACK, "TCP_QUICKACK" },
#endif
      {0, NULL}
   } ;
   const struct sockopt *optname;

   optname = tcpopts; while (optname->so) {
      sockoptan(fd, optname, SOL_TCP, outfile);
      ++optname;
   }
   return 0;
}
#endif /* WITH_TCP */


#if WITH_SOCKET
int sockoptan(int fd, const struct sockopt *optname, int socklay, FILE *outfile) {
#define FILAN_OPTLEN 256
   char optval[FILAN_OPTLEN];
   socklen_t optlen;
   int result;

   optlen = FILAN_OPTLEN;
   result =
      Getsockopt(fd, socklay, optname->so, (void *)optval, &optlen);
   if (result < 0) {
      Debug6("getsockopt(%d, %d, %d, %p, {"F_socklen"}): %s",
	     fd, socklay, optname->so, optval, optlen, strerror(errno));
      fputc('\t', outfile);
      return -1;
   } else if (optlen == 0) {
      Debug1("getsockopt(,,, {}, %d)", optlen);
      fprintf(outfile, "%s=\"\"\t", optname->name);
   } else if (optlen == sizeof(int)) {
      Debug2("getsockopt(,,, {%d}, %d)",
	     *(int *)optval, optlen);
      fprintf(outfile, "%s=%d\t", optname->name, *(int *)optval);
   } else {
      char outbuf[512], *cp = outbuf;
      int i;
      for (i = 0; i < optlen/sizeof(unsigned int); ++i) {
	 cp += sprintf(cp, "%08x ", ((unsigned int *)optval)[i]);
      }
      *--cp = '\0';	/* delete trailing space */
      Debug2("getsockopt(,,, {%s}, %d)", outbuf, optlen);
      fprintf(outfile, "%s={%s}\t", optname->name, outbuf);
   }
   return 0;
#undef FILAN_OPTLEN
}
#endif /* WITH_SOCKET */


#if WITH_SOCKET
int isasocket(int fd) {
   int retval;
#if HAVE_STAT64
   struct stat64 props;
#else
   struct stat props;
#endif /* HAVE_STAT64 */
   retval =
#if HAVE_STAT64
      Fstat64
#else
      Fstat
#endif
	  (fd, &props);
   if (retval < 0) {
      Info3("fstat(%d, %p): %s", fd, &props, strerror(errno));
      return 0;
   }
   /* note: when S_ISSOCK was undefined, it always gives 0 */
   return S_ISSOCK(props.st_mode);
}
#endif /* WITH_SOCKET */


