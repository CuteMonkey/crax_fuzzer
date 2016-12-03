/* $Id: xioopen.c,v 1.102 2004/06/06 17:33:23 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source file of the extended open function */

#include "xiosysincludes.h"

#include "xioopen.h"
#include "xiomodes.h"


static xiofile_t *xioallocfd(void);

const struct addrname addressnames[] = {
#if 1
#if WITH_STDIO
   { "-",		&addr_stdio },
#endif
#if WITH_CREAT
   { "creat",	&addr_creat },
   { "create",	&addr_creat },
#endif
#if WITH_EXEC
   { "exec",		&addr_exec },
#endif
#if WITH_FDNUM
   { "fd",		&addr_fd },
#endif
#if WITH_PIPE
   { "fifo",		&addr_pipe },
#endif
#if WITH_FILE
   { "file",		&addr_open },
#endif
#if WITH_GOPEN
   { "gopen",	&addr_gopen },
#endif
#if WITH_IP4 && WITH_TCP
   { "inet",		&addr_tcp4_connect },
#endif
#if WITH_IP4 && WITH_TCP && WITH_LISTEN
   { "inet-l",	&addr_tcp4_listen },
   { "inet-listen",	&addr_tcp4_listen },
#endif
#if WITH_IP6 && WITH_TCP
   { "inet6",	&addr_tcp6_connect },
#endif
#if WITH_IP6 && WITH_TCP && WITH_LISTEN
   { "inet6-l",	&addr_tcp6_listen },
   { "inet6-listen",	&addr_tcp6_listen },
#endif
#if WITH_IP4 && WITH_RAWIP
   { "ip",		&addr_rawip4 },
   { "ip4",		&addr_rawip4 },
#endif
#if WITH_IP6 && WITH_RAWIP
   { "ip6",		&addr_rawip6 },
#endif
#if WITH_UNIX
   { "local",	&addr_unix_connect },
#endif
#if WITH_FILE
   { "open",		&addr_open },
#endif
#if WITH_OPENSSL
   { "openssl",		&addr_openssl },
   { "openssl-connect",		&addr_openssl },
#if WITH_LISTEN
   { "openssl-listen",		&addr_openssl_listen },
#endif
#endif
#if WITH_PIPE
   { "pipe",		&addr_pipe },
#endif
#if WITH_PROXY
   { "proxy",		&addr_proxy_connect },
   { "proxy-connect",	&addr_proxy_connect },
#endif
#if WITH_PTY
   { "pty",		&addr_pty },
#endif
#if WITH_READLINE
   { "readline",	&addr_readline },
#endif
#if WITH_SOCKS4
   { "socks",	&addr_socks4_connect },
   { "socks4",	&addr_socks4_connect },
#endif
#if WITH_SOCKS4A
   { "socks4a",	&addr_socks4a_connect },
#endif
#if WITH_OPENSSL
   { "ssl",		&addr_openssl },
#if WITH_LISTEN
   { "ssl-l",		&addr_openssl_listen },
#endif
#endif
#if WITH_STDIO
   { "stderr",	&addr_stderr },
   { "stdin",	&addr_stdin },
   { "stdio",	&addr_stdio },
   { "stdout",	&addr_stdout },
#endif
#if WITH_SYSTEM
   { "system",	&addr_system },
#endif
#if WITH_IP4 && WITH_TCP
   { "tcp",		&addr_tcp4_connect },
#endif
#if WITH_IP4 && WITH_TCP && WITH_LISTEN
   { "tcp-l",	&addr_tcp4_listen },
   { "tcp-listen",	&addr_tcp4_listen },
#endif
#if WITH_IP4 && WITH_TCP
   { "tcp4",		&addr_tcp4_connect },
#endif
#if WITH_IP4 && WITH_TCP && WITH_LISTEN
   { "tcp4-l",	&addr_tcp4_listen },
   { "tcp4-listen",	&addr_tcp4_listen },
#endif
#if WITH_IP6 && WITH_TCP
   { "tcp6",		&addr_tcp6_connect },
#endif
#if WITH_IP6 && WITH_TCP && WITH_LISTEN
   { "tcp6-l",	&addr_tcp6_listen },
   { "tcp6-listen",	&addr_tcp6_listen },
#endif
#if WITH_IP4 && WITH_UDP
   { "udp",		&addr_udp4_connect },
#endif
#if WITH_IP4 && WITH_UDP && WITH_LISTEN
   { "udp-l",	&addr_udp4_listen },
   { "udp-listen",	&addr_udp4_listen },
#endif
#if WITH_IP4 && WITH_UDP
   { "udp4",		&addr_udp4_connect },
#endif
#if WITH_IP4 && WITH_UDP && WITH_LISTEN
   { "udp4-l",	&addr_udp4_listen },
   { "udp4-listen",	&addr_udp4_listen },
#endif
#if WITH_IP6 && WITH_UDP
   { "udp6",		&addr_udp6_connect },
#endif
#if WITH_IP6 && WITH_UDP && WITH_LISTEN
   { "udp6-l",	&addr_udp6_listen },
   { "udp6-listen",	&addr_udp6_listen },
#endif
#if WITH_UNIX
   { "unix",		&addr_unix_connect },
   { "unix-connect",	&addr_unix_connect },
#endif
#if WITH_UNIX && WITH_LISTEN
   { "unix-l",	&addr_unix_listen },
   { "unix-listen",	&addr_unix_listen },
#endif
#else /* !0 */
#  if WITH_INTEGRATE
#    include "xiointegrate.c"
#  else
#    include "xioaddrtab.c"
#  endif
#endif /* !0 */
   { NULL }	/* end marker */
} ;


/* prepares a xiofile_t record for dual address type:
   sets the tag and allocates memory for the substreams.
   returns 0 on success, of <0 if an error occurred.
*/
int xioopen_makedual(xiofile_t *file) {
   file->tag = XIO_TAG_DUAL;
   file->common.flags = O_RDWR;
   if ((file->dual.stream[0] = xioallocfd()) == NULL)
      return -1;
   file->dual.stream[0]->common.flags = O_RDONLY;
   if ((file->dual.stream[1] = xioallocfd()) == NULL)
      return -1;
   file->dual.stream[1]->common.flags = O_WRONLY;
   return 0;
}

static xiofile_t *xioallocfd(void) {
   xiofile_t *fd;

   if ((fd = Calloc(1, sizeof(xiofile_t))) == NULL) {
      return NULL;
   }
   /* some default values; 0's and NULL's need not be applied (calloc'ed) */
   fd->common.tag       = XIO_TAG_INVALID;
/* fd->common.addr      = NULL; */
   fd->common.flags     = O_RDWR;

#if WITH_RETRY
/* fd->stream.retry	= 0; */
/* fd->stream.forever	= false; */
   fd->stream.intervall.tv_sec  = 1;
/* fd->stream.intervall.tv_nsec = 0; */
#endif /* WITH_RETRY */
/* fd->common.ignoreeof = false; */
/* fd->common.eof       = 0; */

   fd->stream.fd        = -1;
   fd->stream.dtype     = DATA_STREAM;
#if WITH_SOCKET
/* fd->stream.salen     = 0; */
#endif /* WITH_SOCKET */
   fd->stream.howtoend  = END_CLOSE;
/* fd->stream.name      = NULL; */
/* fd->stream.para.exec.pid = 0; */
   fd->stream.lineterm  = LINETERM_RAW;
   return fd;
}

/* parse the argument that specifies a two-directional data stream */
xiofile_t *xioopen(const char *addr,	/* address specification */
		   int flags) {
   xiofile_t *fd;
   char *a1, *a2;

   if (xioinitialize() < 0) {
      return NULL;
   }

   if ((a1 = strdup(addr)) == NULL) {
      Error1("strdup("F_Zu"): out of memory", strlen(addr)+1);
      return NULL;
   }
   if (a2 = strstr(a1, xioopts.pipesep)) {
      /* in- and out-pipe are specified separately */
      /* flags argument: */
      if ((flags&3) != O_RDWR) {
	 Error("unidirectional open with dual address");
	 return NULL;
      }
      if ((fd = xioallocfd()) == NULL) {
	 return NULL;
      }
      if (xioopen_makedual(fd) < 0)
	 return NULL;
      *a2 = '\0';
      a2 += strlen(xioopts.pipesep);
      if (xioopensingle(a1, fd->dual.stream[0], O_RDONLY) != 0)
	 return NULL;
      if (xioopensingle(a2, fd->dual.stream[1], O_WRONLY) != 0)
	 return NULL;
#if 0
      fd->common.ignoreeof = fd->dual.stream[0]->common.ignoreeof;
#endif

   /* there are some specific addresses that we must handle specially if they 
      occur as single address, because copying of the first fd to the second 
      does not do what we want: STDIO, -, PIPE */
      /* but only for the bidirectional case */
#if WITH_STDIO
   } else if ((flags&3) == O_RDWR &&
	      !strncasecmp("STDIO", a1, 5) &&
	      (a1[5] == ',' || a1[5] == '\0')) {
      if ((fd = xioallocfd()) == NULL) {
	 return NULL;
      }
      if (xioopen_stdio_bi(a1+5, fd) < 0) {
	 free(fd);
	 return NULL;
      }
#endif /* WITH_STDIO */

#if WITH_STDIO
    } else if ((flags&3) == O_RDWR &&
	      !strncmp("-", a1, 1) &&
	      (a1[1] == ',' || a1[1] == '\0')) {
      if ((fd = xioallocfd()) == NULL) {
	 return NULL;
      }
      if (xioopen_stdio_bi(a1+1, fd) < 0) {
	 free(fd);
	 return NULL;
      }
#endif /* WITH_STDIO */

#if WITH_PIPE
    } else if ((flags&3) == O_RDWR &&
	       (!strncasecmp("PIPE", a1, 4) ||
	       !strncasecmp("FIFO", a1, 4) ||
	       !strncasecmp("ECHO", a1, 4)) &&
	      (a1[4] == ',' || a1[4] == '\0')) {
      /*! flags argument */
      if ((fd = xioallocfd()) == NULL) {
	 return NULL;
      }
      if (xioopen_fifo_unnamed(a1+4, fd) < 0) {
	 free(fd);
	 return NULL;
      }
#endif /* WITH_PIPE */

   } else {
      if ((fd = xioallocfd()) == NULL) {
	 return NULL;
      }
      /* a "usual" bidirectional stream specification, one address */
      if (xioopensingle(a1, fd, flags) != 0) {
	 free(fd);
	 return 0;
      }
      fd->common.tag       = XIO_TAG_RDWR;
#if 0
      /* some extended default values */
      fd->stream.fd        = -1;
      fd->stream.dtype     = DATA_STREAM;
#if WITH_SOCKET
      fd->stream.salen     = 0;
#endif /* WITH_SOCKET */
      fd->stream.howtoend  = END_CLOSE;
      fd->stream.name      = NULL;
      fd->stream.para.exec.pid = 0;
      fd->stream.lineterm  = LINETERM_RAW;
#endif
#if 0
	 if (fd->stream[1].fd < 0) {
	    fd->stream[1] = sock->fd[0];
	    fd->stream[1].fd = Dup(fd->stream[0].fd);
	    /*0 Info2("dup(%d) -> %d", fd->stream[0].fd, fd->stream[1].fd);*/
	    Fcntl_l(fd->fd[1].fd, F_SETFD,
		    Fcntl(fd->fd[0].fd, F_GETFD) ? FD_CLOEXEC : 0);
	 }
#endif
   }

   fd->common.flags     = flags;
   free(a1);
   return fd;
}


/* rw is O_RDONLY, O_WRONLY, or O_RDWR */
int xioopensingle(char *addr, union bipipe *fd, int rw) {
   struct addrname *ae;
   const struct addrdesc *addrdesc;
   char *colon, *comma, *a1, *akey;
   int result;

   /* identify where address keyword ends */
   colon = strchr(addr, ':');
   comma = strchr(addr, ',');
   if (colon) {
      if (comma) {
	 a1 = Min(colon, comma);
      } else {
	 a1 = colon;
      }
   } else {
      if (comma) {
	 a1 = comma;
      } else {
	 a1 = strchr(addr, '\0');
      }
   }
   if ((akey = Malloc(a1-addr+1)) == NULL)
      return -1;
   strncpy(akey, addr, a1-addr);
   akey[a1-addr] = '\0';

   ae = (struct addrname *)
      keyw((struct wordent *)&addressnames, akey,
	   sizeof(addressnames)/sizeof(struct addrname)-1);
   free(akey);
   if (ae) {
      addrdesc = ae->desc;
      if (*a1) { *a1++ = '\0'; }
   } else {
      char *a2;
#if WITH_FDNUM
      if (isdigit(addr[0]&0xff)) {
	 a1 = addr;
	 addrdesc = &addr_fd;
      } else 
#endif /* WITH_FDNUM */
#if WITH_GOPEN
      if ((a2 = strchr(addr, '/')) && (a2 < a1 || a1 == NULL)) {
	 a1 = addr;
	 addrdesc = &addr_gopen;
      } else
#endif /* WITH_GOPEN */
      {
	 Error1("unknown device/address \"%s\"", addr);
	 return STAT_NORETRY;
      }
   }

   fd->common.tag  = XIO_TAG_RDWR;
   fd->common.addr = addrdesc;
   result = (*addrdesc->func)(a1, rw, fd, addrdesc->groups, addrdesc->arg1,
			      addrdesc->arg2, addrdesc->argp1);
   return result;
}
