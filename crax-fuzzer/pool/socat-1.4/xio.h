/* $Id: xio.h,v 1.44 2004/06/20 21:22:59 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_h_included
#define __xio_h_included 1

#if 1 /*!*/
#include "mytypes.h"
#include "sysutils.h"
#endif

/* Linux 2.2.10 */
#define HAVE_STRUCT_LINGER 1

#define LINETERM_RAW 0
#define LINETERM_CR 1
#define LINETERM_CRNL 2

struct addrdesc;

/* these are the values allowed for the "enum xiotag  tag" flag of the "struct
   single" and "union bipipe" (xiofile_t) structures. */
enum xiotag {
   XIO_TAG_INVALID,	/* the record is not in use */
   XIO_TAG_RDONLY,	/* this is a single read-only stream */
   XIO_TAG_WRONLY,	/* this is a single write-only stream */
   XIO_TAG_RDWR,	/* this is a single read-write stream */
   XIO_TAG_DUAL		/* this is a dual stream, consisting of two single
			   streams */
} ;

typedef struct {
   bool strictopts;
   const char *pipesep;
   char ip4portsep;
   char ip6portsep;	/* do not change, is hardcoded in parseip6addr()! */
   char logopt;	/* 'm' means "switch to syslog when entering daemon mode" */
   const char *syslogfac;	/* syslog facility (only with mixed mode) */
} xioopts_t;

extern xioopts_t xioopts;

/* a non-dual file descriptor */ 
struct single {
   enum xiotag tag;	/* see  enum xiotag  */
   const struct addrdesc *addr;
   int    flags;
   /* until here, keep consistent with bipipe.common !!! */
#if WITH_RETRY
   unsigned int retry;	/* retry opening this many times */
   bool forever;	/* retry opening forever */
   struct timespec intervall;	/* wait so long between retries */
#endif /* WITH_RETRY */
   bool   ignoreeof;	/* option ignoreeof; do not pass eof condition to app*/
   int    eof;		/* 1..exec'd child has died, but no explicit eof
			   occurred 
			   2..fd0 has reached EOF (definitely; never with
			   ignoreeof! */
   size_t wsize;	/* write alway this size; 0..all available */
   /* until here, keep consistent with bipipe.dual ! */
   int    lineterm;	/* 0..dont touch; 1..CR; 2..CRNL on extern data */
   int    fd;
   enum {
      DATA_STREAM,	/* files and connected sockets (use read, write) */
      DATA_RECV,	/* datagram sockets (use recvfrom, sendto) */
      DATA_PIPE,	/* a short cut pipe */
      DATA_2PIPE,	/* communication with one process via 2 pipes */
      DATA_PTY,		/* an "internal" pty */
      DATA_READLINE,	/* read via readline(), write via fd */
      DATA_OPENSSL	/* use openssl read/write functions */
   } dtype;
   enum {
      END_NONE,		/* no action */
      END_CLOSE,	/* close() */
      END_SHUTDOWN,	/* shutdown() */
      END_UNLINK,	/* unlink() */
      END_KILL,		/* has subprocess */
      END_CLOSE_KILL,	/* first close fd, then kill subprocess */
      END_SHUTDOWN_KILL	/* first shutdown fd, then kill subprocess */
   } howtoend;
#if WITH_SOCKET
   union sockaddr_union peersa;
   socklen_t salen;
#endif /* WITH_SOCKET */
#if WITH_TERMIOS
   bool ttyvalid;		/* the following struct is valid */
   struct termios savetty;	/* save orig tty settings for later restore */
#endif /* WITH_TERMIOS */
   const char *name;		/* only with END_UNLINK */
   union {
      struct {
	 int fdout;		/* use fd for output */
      } bipipe;
      struct {
	 pid_t pid;		/* child PID, with EXEC: */
	 int fdout;		/* use fd for output if two pipes */
      } exec;
#if WITH_READLINE
      struct {
	 char *history_file;
	 char *prompt;		/* static prompt, passed to readline() */
	 size_t dynbytes;	/* length of buffer for dynamic prompt */
	 char *dynprompt;	/* the dynamic prompt */
	 char *dynend;		/* current end of dynamic prompt */
#if HAVE_REGEX_H
	 bool    hasnoecho;	/* following regex is set */
	 regex_t noecho;	/* if it matches the prompt, input is silent */
#endif
      } readline;
#endif /* WITH_READLINE */
#if WITH_OPENSSL
      struct {
	 SSL *ssl;
	 SSL_CTX* ctx;
      } openssl;
#endif /* WITH_OPENSSL */
   } para;
} ;

/* rw: 0..read, 1..write, 2..r/w */
/* when implementing a new address type take care of following topics:
   . be aware that xioopen_single is used for O_RDONLY, O_WRONLY, and O_RDWR data
   . which options are allowed (option groups)
   . implement application of all these options
   . set FD_CLOEXEC on new file descriptors BEFORE the cloexec option might be
     applied
   .
*/

typedef union bipipe {
   enum xiotag    tag;
   struct {
      enum xiotag tag;
      const struct addrdesc *addr;
      int         flags;
   } common;
   struct single  stream;
   struct {
      enum xiotag tag;
      const struct addrdesc *addr;
      int         flags;	/* compatible to fcntl(.., F_GETFL, ..) */
#if WITH_RETRY
      unsigned retry;	/* retry opening this many times */
      bool forever;	/* retry opening forever */
      struct timespec intervall;	/* wait so long between retries */
#endif /* WITH_RETRY */
      bool        ignoreeof;
      int         eof;		/* fd0 has reached EOF */
      size_t      wsize;	/* write always this size; 0..all available */
      union bipipe *stream[2];	/* input stream, output stream */
   } dual;
} xiofile_t;


struct addrdesc {
   const char *defname;	/* main (canonical) name of address */
   int directions;	/* 1..read, 2..write, 3..both */
   int (*func)(char *a1, int rw, xiofile_t *fd, unsigned groups,
	       int arg1, int arg2, void *argp1);
   unsigned groups;
   int arg1;
   int arg2;
   void *argp1;
#if WITH_HELP
   const char *syntax;
#endif
} ;

#define XIO_WRITABLE(s) (((s)->common.flags+1)&2)
#define XIO_READABLE(s) (((s)->common.flags+1)&1)
#define XIO_RDSTREAM(s) (((s)->tag==XIO_TAG_DUAL)?&(s)->dual.stream[0]->stream:&(s)->stream)
#define XIO_WRSTREAM(s) (((s)->tag==XIO_TAG_DUAL)?&(s)->dual.stream[1]->stream:&(s)->stream)
#define XIO_GETRDFD(s) (((s)->tag==XIO_TAG_DUAL)?(s)->dual.stream[0]->stream.fd:(s)->stream.fd)
#define XIO_GETWRFD(s) (((s)->tag==XIO_TAG_DUAL)?(s)->dual.stream[1]->stream.fd:((s)->stream.dtype==DATA_2PIPE)?(s)->stream.para.exec.fdout:((s)->stream.dtype==DATA_PIPE)?(s)->stream.para.bipipe.fdout:(s)->stream.fd)
#define XIO_EOF(s) (XIO_RDSTREAM(s)->eof && !XIO_RDSTREAM(s)->ignoreeof)

typedef unsigned long flags_t;

union integral {
   int            u_bool;
   byte_t         u_byte;
   gid_t          u_gidt;
   int	          u_int;
   long           u_long;
#if HAVE_TYPE_LONGLONG
   long long      u_longlong;
#endif
   double         u_double;
   mode_t         u_modet;
   short          u_short;
   size_t         u_sizet;
   char          *u_string;
   uid_t          u_uidt;
   unsigned int   u_uint;
   unsigned long  u_ulong;
   unsigned short u_ushort;
   byte2_t        u_2bytes;
   void          *u_ptr;
   flags_t        u_flag;
   struct {
      byte_t     *b_data;
      size_t      b_len;
   }              u_bin;
   struct timeval u_timeval;
#if HAVE_STRUCT_LINGER
   struct linger  u_linger;
#endif /* HAVE_STRUCT_LINGER */
#if HAVE_STRUCT_TIMESPEC	
   struct timespec u_timespec;
#endif /* HAVE_STRUCT_TIMESPEC */
} ;

/* some aliases */
#define u_off u_long	/* please report when this causes problems */

#if defined(HAVE_BASIC_OFF64_T) && HAVE_BASIC_OFF64_T
#  if HAVE_BASIC_OFF64_T==5
#     define u_off64 u_long
#  elif HAVE_BASIC_OFF64_T==7
#     define u_off64 u_longlong
#  else
#     error "unexpected size of off64_t, please report this as bug"
#  endif
#endif /* defined(HAVE_BASIC_OFF64_T) && HAVE_BASIC_OFF64_T */


/* this handles option instances, for communication between subroutines */
struct opt {
   const struct optdesc *desc;
   union integral value;
} ;

extern const char *PIPESEP;

/* return values of xioopensingle */
#define STAT_OK		0
#define STAT_WARNING	1
#define STAT_EXIT	2
#define STAT_RETRYNOW	-1	/* only after timeouts useful ? */
#define STAT_RETRYLATER	-2	/* address cannot be opened, but user might
				   change something in the filesystem etc. to
				   make this process succeed later. */
#define STAT_NORETRY	-3	/* address syntax error, not implemented etc;
				   not even by external changes correctable */

extern int xioinitialize(void);
extern int xiosetopt(char what, const char *arg);
extern int xioinqopt(char what, char *arg, size_t n);
extern xiofile_t *xioopen(const char *args, int flags);
extern int xioopensingle(char *addr, xiofile_t *fd, int rw);
extern int xioopenhelp(FILE *of, int level);

/* must be outside function for use by childdied handler */
extern xiofile_t *sock1, *sock2;
extern int xioinitialize(void);
extern pid_t diedunknown1;	/* child died before it is registered */
extern pid_t diedunknown2;
extern pid_t diedunknown3;
extern pid_t diedunknown4;

extern int xio_opt_signal(pid_t pid, int signum);
extern void childdied(int signum);

extern ssize_t xioread(xiofile_t *sock1, void *buff, size_t bufsiz);
extern ssize_t xiowrite(xiofile_t *sock1, const void *buff, size_t bufsiz);
extern int xioshutdown(xiofile_t *sock, int how);

extern int xioclose(xiofile_t *sock);
extern void xioexit(void);

#endif /* !defined(__xio_h_included) */
