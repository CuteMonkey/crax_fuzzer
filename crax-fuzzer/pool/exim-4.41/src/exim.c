/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */


/* The main function: entry point, initialization, and high-level control.
Also a few functions that don't naturally fit elsewhere. */


#include "exim.h"



/*************************************************
*      Function interface to store functions     *
*************************************************/

/* We need some real functions to pass to the PCRE regular expression library
for store allocation via Exim's store manager. The normal calls are actually
macros that pass over location information to make tracing easier. These
functions just interface to the standard macro calls. A good compiler will
optimize out the tail recursion and so not make them too expensive. There
are two sets of functions; one for use when we want to retain the compiled
regular expression for a long time; the other for short-term use. */

static void *
function_store_get(size_t size)
{
return store_get((int)size);
}

static void
function_dummy_free(void *block) { block = block; }

static void *
function_store_malloc(size_t size)
{
return store_malloc((int)size);
}

static void
function_store_free(void *block)
{
store_free(block);
}




/*************************************************
*  Compile regular expression and panic on fail  *
*************************************************/

/* This function is called when failure to compile a regular expression leads
to a panic exit. In other cases, pcre_compile() is called directly. In many
cases where this function is used, the results of the compilation are to be
placed in long-lived store, so we temporarily reset the store management
functions that PCRE uses if the use_malloc flag is set.

Argument:
  pattern     the pattern to compile
  caseless    TRUE if caseless matching is required
  use_malloc  TRUE if compile into malloc store

Returns:      pointer to the compiled pattern
*/

const pcre *
regex_must_compile(uschar *pattern, BOOL caseless, BOOL use_malloc)
{
int offset;
int options = PCRE_COPT;
const pcre *yield;
const uschar *error;
if (use_malloc)
  {
  pcre_malloc = function_store_malloc;
  pcre_free = function_store_free;
  }
if (caseless) options |= PCRE_CASELESS;
yield = pcre_compile(CS pattern, options, (const char **)&error, &offset, NULL);
pcre_malloc = function_store_get;
pcre_free = function_dummy_free;
if (yield == NULL)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "regular expression error: "
    "%s at offset %d while compiling %s", error, offset, pattern);
return yield;
}




/*************************************************
*   Execute regular expression and set strings   *
*************************************************/

/* This function runs a regular expression match, and sets up the pointers to
the matched substrings.

Arguments:
  re          the compiled expression
  subject     the subject string
  options     additional PCRE options
  setup       if < 0 do full setup
              if >= 0 setup from setup+1 onwards,
                excluding the full matched string

Returns:      TRUE or FALSE
*/

BOOL
regex_match_and_setup(const pcre *re, uschar *subject, int options, int setup)
{
int ovector[3*(EXPAND_MAXN+1)];
int n = pcre_exec(re, NULL, CS subject, Ustrlen(subject), 0,
  PCRE_EOPT | options, ovector, sizeof(ovector)/sizeof(int));
BOOL yield = n >= 0;
if (n == 0) n = EXPAND_MAXN + 1;
if (yield)
  {
  int nn;
  expand_nmax = (setup < 0)? 0 : setup + 1;
  for (nn = (setup < 0)? 0 : 2; nn < n*2; nn += 2)
    {
    expand_nstring[expand_nmax] = subject + ovector[nn];
    expand_nlength[expand_nmax++] = ovector[nn+1] - ovector[nn];
    }
  expand_nmax--;
  }
return yield;
}




/*************************************************
*             Handler for SIGUSR1                *
*************************************************/

/* SIGUSR1 causes any exim process to write to the process log details of
what it is currently doing. It will only be used if the OS is capable of
setting up a handler that causes automatic restarting of any system call
that is in progress at the time.

Argument: the signal number (SIGUSR1)
Returns:  nothing
*/

static void
usr1_handler(int sig)
{
sig = sig;      /* Keep picky compilers happy */
log_write(0, LOG_PROCESS, "%s", process_info);
log_close_all();
os_restarting_signal(SIGUSR1, usr1_handler);
}



/*************************************************
*             Timeout handler                    *
*************************************************/

/* This handler is enabled most of the time that Exim is running. The handler
doesn't actually get used unless alarm() has been called to set a timer, to
place a time limit on a system call of some kind. When the handler is run, it
re-enables itself.

There are some other SIGALRM handlers that are used in special cases when more
than just a flag setting is required; for example, when reading a message's
input. These are normally set up in the code module that uses them, and the
SIGALRM handler is reset to this one afterwards.

Argument: the signal value (SIGALRM)
Returns:  nothing
*/

void
sigalrm_handler(int sig)
{
sig = sig;      /* Keep picky compilers happy */
sigalrm_seen = TRUE;
os_non_restarting_signal(SIGALRM, sigalrm_handler);
}



/*************************************************
*      Sleep for a fractional time interval      *
*************************************************/

/* This function is called by millisleep() and exim_wait_tick() to wait for a
period of time that may include a fraction of a second. The coding is somewhat
tedious...

Argument:  an itimerval structure containing the interval
Returns:   nothing
*/

static void
milliwait(struct itimerval *itval)
{
sigset_t sigmask;
sigset_t old_sigmask;
(void)sigemptyset(&sigmask);                           /* Empty mask */
(void)sigaddset(&sigmask, SIGALRM);                    /* Add SIGALRM */
(void)sigprocmask(SIG_BLOCK, &sigmask, &old_sigmask);  /* Block SIGALRM */
(void)setitimer(ITIMER_REAL, itval, NULL);             /* Start timer */
(void)sigfillset(&sigmask);                            /* All signals */
(void)sigdelset(&sigmask, SIGALRM);                    /* Remove SIGALRM */
(void)sigsuspend(&sigmask);                            /* Until SIGALRM */
(void)sigprocmask(SIG_SETMASK, &old_sigmask, NULL);    /* Restore mask */
}




/*************************************************
*         Millisecond sleep function             *
*************************************************/

/* The basic sleep() function has a granularity of 1 second, which is too rough
in some cases - for example, when using an increasing delay to slow down
spammers.

Argument:    number of millseconds
Returns:     nothing
*/

void
millisleep(int msec)
{
struct itimerval itval;
itval.it_interval.tv_sec = 0;
itval.it_interval.tv_usec = 0;
itval.it_value.tv_sec = msec/1000;
itval.it_value.tv_usec = (msec % 1000) * 1000;
milliwait(&itval);
}



/*************************************************
*         Compare microsecond times              *
*************************************************/

/*
Arguments:
  tv1         the first time
  tv2         the second time

Returns:      -1, 0, or +1
*/

int
exim_tvcmp(struct timeval *t1, struct timeval *t2)
{
if (t1->tv_sec > t2->tv_sec) return +1;
if (t1->tv_sec < t2->tv_sec) return -1;
if (t1->tv_usec > t2->tv_usec) return +1;
if (t1->tv_usec < t2->tv_usec) return -1;
return 0;
}




/*************************************************
*          Clock tick wait function              *
*************************************************/

/* Exim uses a time + a pid to generate a unique identifier in two places: its
message IDs, and in file names for maildir deliveries. Because some OS now
re-use pids within the same second, sub-second times are now being used.
However, for absolute certaintly, we must ensure the clock has ticked before
allowing the relevant process to complete. At the time of implementation of
this code (February 2003), the speed of processors is such that the clock will
invariably have ticked already by the time a process has done its job. This
function prepares for the time when things are faster - and it also copes with
clocks that go backwards.

Arguments:
  then_tv      A timeval which was used to create uniqueness; its usec field
                 has been rounded down to the value of the resolution.
                 We want to be sure the current time is greater than this.
  resolution   The resolution that was used to divide the microseconds
                 (1 for maildir, larger for message ids)

Returns:       nothing
*/

void
exim_wait_tick(struct timeval *then_tv, int resolution)
{
struct timeval now_tv;
long int now_true_usec;

(void)gettimeofday(&now_tv, NULL);
now_true_usec = now_tv.tv_usec;
now_tv.tv_usec = (now_true_usec/resolution) * resolution;

if (exim_tvcmp(&now_tv, then_tv) <= 0)
  {
  struct itimerval itval;
  itval.it_interval.tv_sec = 0;
  itval.it_interval.tv_usec = 0;
  itval.it_value.tv_sec = then_tv->tv_sec - now_tv.tv_sec;
  itval.it_value.tv_usec = then_tv->tv_usec + resolution - now_true_usec;
  DEBUG(D_transport|D_receive)
    {
    if (!running_in_test_harness)
      {
      debug_printf("tick check: %lu.%06lu %lu.%06lu\n",
        then_tv->tv_sec, then_tv->tv_usec, now_tv.tv_sec, now_tv.tv_usec);
      debug_printf("waiting %lu.%06lu\n", itval.it_value.tv_sec,
        itval.it_value.tv_usec);
      }
    }
  milliwait(&itval);
  }
}




/*************************************************
*            Set up processing details           *
*************************************************/

/* Save a text string for dumping when SIGUSR1 is received.
Do checks for overruns.

Arguments: format and arguments, as for printf()
Returns:   nothing
*/

void
set_process_info(char *format, ...)
{
int len;
va_list ap;
sprintf(CS process_info, "%5d ", (int)getpid());
len = Ustrlen(process_info);
va_start(ap, format);
if (!string_vformat(process_info + len, PROCESS_INFO_SIZE - len, format, ap))
  Ustrcpy(process_info + len, "**** string overflowed buffer ****");
DEBUG(D_process_info) debug_printf("set_process_info: %s\n", process_info);
va_end(ap);
}





/*************************************************
*   Ensure stdin, stdout, and stderr exist       *
*************************************************/

/* Some operating systems grumble if an exec() happens without a standard
input, output, and error (fds 0, 1, 2) being defined. The worry is that some
file will be opened and will use these fd values, and then some other bit of
code will assume, for example, that it can write error messages to stderr.
This function ensures that fds 0, 1, and 2 are open if they do not already
exist, by connecting them to /dev/null.

This function is also used to ensure that std{in,out,err} exist at all times,
so that if any library that Exim calls tries to use them, it doesn't crash.

Arguments:  None
Returns:    Nothing
*/

void
exim_nullstd(void)
{
int i;
int devnull = -1;
struct stat statbuf;
for (i = 0; i <= 2; i++)
  {
  if (fstat(i, &statbuf) < 0 && errno == EBADF)
    {
    if (devnull < 0) devnull = open("/dev/null", O_RDWR);
    if (devnull < 0) log_write(0, LOG_MAIN|LOG_PANIC_DIE, "%s",
      string_open_failed(errno, "/dev/null"));
    if (devnull != i) dup2(devnull, i);
    }
  }
if (devnull > 2) close(devnull);
}




/*************************************************
*   Close unwanted file descriptors for delivery *
*************************************************/

/* This function is called from a new process that has been forked to deliver
an incoming message, either directly, or using exec.

We want any smtp input streams to be closed in this new process. However, it
has been observed that using fclose() here causes trouble. When reading in -bS
input, duplicate copies of messages have been seen. The files will be sharing a
file pointer with the parent process, and it seems that fclose() (at least on
some systems - I saw this on Solaris 2.5.1) messes with that file pointer, at
least sometimes. Hence we go for closing the underlying file descriptors.

If TLS is active, we want to shut down the TLS library, but without molesting
the parent's SSL connection.

For delivery of a non-SMTP message, we want to close stdin and stdout (and
stderr unless debugging) because the calling process might have set them up as
pipes and be waiting for them to close before it waits for the submission
process to terminate. If they aren't closed, they hold up the calling process
until the initial delivery process finishes, which is not what we want.

However, if D_resolver is set, implying resolver debugging, leave stdout open,
because that's where the resolver writes its debugging output.

When we close stderr (which implies we've also closed stdout), we also get rid
of any controlling terminal.

Arguments:   None
Returns:     Nothing
*/

static void
close_unwanted(void)
{
if (smtp_input)
  {
  #ifdef SUPPORT_TLS
  tls_close(FALSE);      /* Shut down the TLS library */
  #endif
  close(fileno(smtp_in));
  close(fileno(smtp_out));
  smtp_in = NULL;
  }
else
  {
  close(0);                                           /* stdin */
  if ((debug_selector & D_resolver) == 0)  close(1);  /* stdout */
  if (debug_selector == 0)                            /* stderr */
    {
    close(2);
    log_stderr = NULL;
    (void)setsid();
    }
  }
}




/*************************************************
*          Set uid and gid                       *
*************************************************/

/* This function sets a new uid and gid permanently, optionally calling
initgroups() to set auxiliary groups. There are some special cases when running
Exim in unprivileged modes. In these situations the effective uid will not be
root; if we already have the right effective uid/gid, and don't need to
initialize any groups, leave things as they are.

Arguments:
  uid        the uid
  gid        the gid
  igflag     TRUE if initgroups() wanted
  msg        text to use in debugging output and failure log

Returns:     nothing; bombs out on failure
*/

void
exim_setugid(uid_t uid, gid_t gid, BOOL igflag, uschar *msg)
{
uid_t euid = geteuid();
gid_t egid = getegid();

if (euid == root_uid || euid != uid || egid != gid || igflag)
  {
  /* At least one OS returns +1 for initgroups failure, so just check for
  non-zero. */

  if (igflag)
    {
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
      {
      if (initgroups(pw->pw_name, gid) != 0)
        log_write(0,LOG_MAIN|LOG_PANIC_DIE,"initgroups failed for uid=%ld: %s",
          (long int)uid, strerror(errno));
      }
    else log_write(0, LOG_MAIN|LOG_PANIC_DIE, "cannot run initgroups(): "
      "no passwd entry for uid=%ld", (long int)uid);
    }

  if (setgid(gid) < 0 || setuid(uid) < 0)
    {
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "unable to set gid=%ld or uid=%ld "
      "(euid=%ld): %s", (long int)gid, (long int)uid, (long int)euid, msg);
    }
  }

/* Debugging output included uid/gid and all groups */

DEBUG(D_uid)
  {
  int group_count;
  gid_t group_list[NGROUPS_MAX];
  debug_printf("changed uid/gid: %s\n  uid=%ld gid=%ld pid=%ld\n", msg,
    (long int)geteuid(), (long int)getegid(), (long int)getpid());
  group_count = getgroups(NGROUPS_MAX, group_list);
  debug_printf("  auxiliary group list:");
  if (group_count > 0)
    {
    int i;
    for (i = 0; i < group_count; i++) debug_printf(" %d", (int)group_list[i]);
    }
  else debug_printf(" <none>");
  debug_printf("\n");
  }
}




/*************************************************
*               Exit point                       *
*************************************************/

/* Exim exits via this function so that it always clears up any open
databases.

Arguments:
  rc         return code

Returns:     does not return
*/

void
exim_exit(int rc)
{
search_tidyup();
DEBUG(D_any)
  debug_printf(">>>>>>>>>>>>>>>> Exim pid=%d terminating with rc=%d "
    ">>>>>>>>>>>>>>>>\n", (int)getpid(), rc);
exit(rc);
}




/*************************************************
*         Extract port from host address         *
*************************************************/

/* Called to extract the port from the values given to -oMa and -oMi.
It also checks the syntax of the address.

Argument:
  address   the address, with possible port on the end

Returns:    the port, or zero if there isn't one
            bombs out on a syntax error
*/

static int
check_port(uschar *address)
{
int port = host_extract_port(address);
if (!string_is_ip_address(address, NULL))
  {
  fprintf(stderr, "exim abandoned: \"%s\" is not an IP address\n", address);
  exit(EXIT_FAILURE);
  }
return port;
}



/*************************************************
*              Test/verify an address            *
*************************************************/

/* This function is called by the -bv and -bt code. It extracts a working
address from a full RFC 822 address. This isn't really necessary per se, but it
has the effect of collapsing source routes.

Arguments:
  s            the address string
  flags        flag bits for verify_address()
  exit_value   to be set for failures

Returns:       nothint
*/

static void
test_address(uschar *s, int flags, int *exit_value)
{
int start, end, domain;
uschar *parse_error = NULL;
uschar *address = parse_extract_address(s, &parse_error, &start, &end, &domain,
  FALSE);
if (address == NULL)
  {
  fprintf(stdout, "syntax error: %s\n", parse_error);
  *exit_value = 2;
  }
else
  {
  int rc = verify_address(deliver_make_addr(address,TRUE), stdout, flags, -1,
    NULL);
  if (rc == FAIL) *exit_value = 2;
    else if (rc == DEFER && *exit_value == 0) *exit_value = 1;
  }
}



/*************************************************
*         Decode bit settings for log/debug      *
*************************************************/

/* This function decodes a string containing bit settings in the form of +name
and/or -name sequences, and sets/unsets bits in a bit string accordingly. It
also recognizes a numeric setting of the form =<number>, but this is not
intended for user use. It's an easy way for Exim to pass the debug settings
when it is re-exec'ed.

The log options are held in two unsigned ints (because there became too many
for one). The top bit in the table means "put in 2nd selector". This does not
yet apply to debug options, so the "=" facility sets only the first selector.

A bad value for a debug setting is treated as an unknown option - error message
to stderr and die. For log settings, which come from the configuration file,
we write to the log on the way out...

Arguments:
  selector1      address of the first bit string
  selector2      address of the second bit string, or NULL
  string         the configured string
  options        the table of option names
  count          size of table
  which          "log" or "debug"

Returns:         nothing on success - bomb out on failure
*/

static void
decode_bits(unsigned int *selector1, unsigned int *selector2, uschar *string,
  bit_table *options, int count, uschar *which)
{
uschar *errmsg;
if (string == NULL) return;

if (*string == '=')
  {
  char *end;    /* Not uschar */
  *selector1 = strtoul(CS string+1, &end, 0);
  if (*end == 0) return;
  errmsg = string_sprintf("malformed numeric %s_selector setting: %s", which,
    string);
  goto ERROR_RETURN;
  }

/* Handle symbolic setting */

else for(;;)
  {
  BOOL adding;
  uschar *s;
  int len;
  bit_table *start, *end;

  while (isspace(*string)) string++;
  if (*string == 0) return;

  if (*string != '+' && *string != '-')
    {
    errmsg = string_sprintf("malformed %s_selector setting: "
      "+ or - expected but found \"%s\"", which, string);
    goto ERROR_RETURN;
    }

  adding = *string++ == '+';
  s = string;
  while (isalnum(*string) || *string == '_') string++;
  len = string - s;

  start = options;
  end = options + count;

  while (start < end)
    {
    bit_table *middle = start + (end - start)/2;
    int c = Ustrncmp(s, middle->name, len);
    if (c == 0)
      {
      if (middle->name[len] != 0) c = -1; else
        {
        unsigned int bit = middle->bit;
        unsigned int *selector;

        /* The value with all bits set means "set all bits in both selectors"
        in the case where two are being handled. However, the top bit in the
        second selector is never set. */

        if (bit == 0xffffffff)
          {
          *selector1 = adding? bit : 0;
          if (selector2 != NULL) *selector2 = adding? 0x7fffffff : 0;
          }

        /* Otherwise, the 0x80000000 bit means "this value, without the top
        bit, belongs in the second selector". */

        else
          {
          if ((bit & 0x80000000) != 0)
            {
            selector = selector2;
            bit &= 0x7fffffff;
            }
          else selector = selector1;
          if (adding) *selector |= bit; else *selector &= ~bit;
          }
        break;  /* Out of loop to match selector name */
        }
      }
    if (c < 0) end = middle; else start = middle + 1;
    }  /* Loop to match selector name */

  if (start >= end)
    {
    errmsg = string_sprintf("unknown %s_selector setting: %c%.*s", which,
      adding? '+' : '-', len, s);
    goto ERROR_RETURN;
    }
  }    /* Loop for selector names */

/* Handle disasters */

ERROR_RETURN:
if (Ustrcmp(which, "debug") == 0)
  {
  fprintf(stderr, "exim: %s\n", errmsg);
  exit(EXIT_FAILURE);
  }
else log_write(0, LOG_CONFIG|LOG_PANIC_DIE, "%s", errmsg);
}



/*************************************************
*          Show supported features               *
*************************************************/

/* This function is called for -bV and for -d to output the optional features
of the current Exim binary.

Arguments:  a FILE for printing
Returns:    nothing
*/

static void
show_whats_supported(FILE *f)
{
#ifdef DB_VERSION_STRING
fprintf(f, "Berkeley DB: %s\n", DB_VERSION_STRING);
#elif defined(BTREEVERSION) && defined(HASHVERSION)
  #ifdef USE_DB
  fprintf(f, "Probably Berkeley DB version 1.8x (native mode)\n");
  #else
  fprintf(f, "Probably Berkeley DB version 1.8x (compatibility mode)\n");
  #endif
#elif defined(_DBM_RDONLY) || defined(dbm_dirfno)
fprintf(f, "Probably ndbm\n");
#elif defined(USE_TDB)
fprintf(f, "Using tdb\n");
#else
  #ifdef USE_GDBM
  fprintf(f, "Probably GDBM (native mode)\n");
  #else
  fprintf(f, "Probably GDBM (compatibility mode)\n");
  #endif
#endif

fprintf(f, "Support for:");
#if HAVE_ICONV
  fprintf(f, " iconv()");
#endif
#if HAVE_IPV6
  fprintf(f, " IPv6");
#endif
#ifdef SUPPORT_PAM
  fprintf(f, " PAM");
#endif
#ifdef EXIM_PERL
  fprintf(f, " Perl");
#endif
#ifdef USE_TCP_WRAPPERS
  fprintf(f, " TCPwrappers");
#endif
#ifdef SUPPORT_TLS
  #ifdef USE_GNUTLS
  fprintf(f, " GnuTLS");
  #else
  fprintf(f, " OpenSSL");
  #endif
#endif
fprintf(f, "\n");

fprintf(f, "Lookups:");
#ifdef LOOKUP_LSEARCH
  fprintf(f, " lsearch wildlsearch nwildlsearch iplsearch");
#endif
#ifdef LOOKUP_CDB
  fprintf(f, " cdb");
#endif
#ifdef LOOKUP_DBM
  fprintf(f, " dbm dbmnz");
#endif
#ifdef LOOKUP_DNSDB
  fprintf(f, " dnsdb");
#endif
#ifdef LOOKUP_DSEARCH
  fprintf(f, " dsearch");
#endif
#ifdef LOOKUP_IBASE
  fprintf(f, " ibase");
#endif
#ifdef LOOKUP_LDAP
  fprintf(f, " ldap ldapdn ldapm");
#endif
#ifdef LOOKUP_MYSQL
  fprintf(f, " mysql");
#endif
#ifdef LOOKUP_NIS
  fprintf(f, " nis nis0");
#endif
#ifdef LOOKUP_NISPLUS
  fprintf(f, " nisplus");
#endif
#ifdef LOOKUP_ORACLE
  fprintf(f, " oracle");
#endif
#ifdef LOOKUP_PASSWD
  fprintf(f, " passwd");
#endif
#ifdef LOOKUP_PGSQL
  fprintf(f, " pgsql");
#endif
#ifdef LOOKUP_TESTDB
  fprintf(f, " testdb");
#endif
#ifdef LOOKUP_WHOSON
  fprintf(f, " whoson");
#endif
fprintf(f, "\n");

fprintf(f, "Authenticators:");
#ifdef AUTH_CRAM_MD5
  fprintf(f, " cram_md5");
#endif
#ifdef AUTH_PLAINTEXT
  fprintf(f, " plaintext");
#endif
#ifdef AUTH_SPA
  fprintf(f, " spa");
#endif
fprintf(f, "\n");

fprintf(f, "Routers:");
#ifdef ROUTER_ACCEPT
  fprintf(f, " accept");
#endif
#ifdef ROUTER_DNSLOOKUP
  fprintf(f, " dnslookup");
#endif
#ifdef ROUTER_IPLITERAL
  fprintf(f, " ipliteral");
#endif
#ifdef ROUTER_IPLOOKUP
  fprintf(f, " iplookup");
#endif
#ifdef ROUTER_MANUALROUTE
  fprintf(f, " manualroute");
#endif
#ifdef ROUTER_QUERYPROGRAM
  fprintf(f, " queryprogram");
#endif
#ifdef ROUTER_REDIRECT
  fprintf(f, " redirect");
#endif
fprintf(f, "\n");

fprintf(f, "Transports:");
#ifdef TRANSPORT_APPENDFILE
  fprintf(f, " appendfile");
  #ifdef SUPPORT_MAILDIR
    fprintf(f, "/maildir");
  #endif
  #ifdef SUPPORT_MAILSTORE
    fprintf(f, "/mailstore");
  #endif
  #ifdef SUPPORT_MBX
    fprintf(f, "/mbx");
  #endif
#endif
#ifdef TRANSPORT_AUTOREPLY
  fprintf(f, " autoreply");
#endif
#ifdef TRANSPORT_LMTP
  fprintf(f, " lmtp");
#endif
#ifdef TRANSPORT_PIPE
  fprintf(f, " pipe");
#endif
#ifdef TRANSPORT_SMTP
  fprintf(f, " smtp");
#endif
fprintf(f, "\n");

if (fixed_never_users[0] > 0)
  {
  int i;
  fprintf(f, "Fixed never_users: ");
  for (i = 1; i <= (int)fixed_never_users[0] - 1; i++)
    fprintf(f, "%d:", (unsigned int)fixed_never_users[i]);
  fprintf(f, "%d\n", (unsigned int)fixed_never_users[i]);
  }
}




/*************************************************
*          Entry point and high-level code       *
*************************************************/

/* Entry point for the Exim mailer. Analyse the arguments and arrange to take
the appropriate action. All the necessary functions are present in the one
binary. I originally thought one should split it up, but it turns out that so
much of the apparatus is needed in each chunk that one might as well just have
it all available all the time, which then makes the coding easier as well.

Arguments:
  argc      count of entries in argv
  argv      argument strings, with argv[0] being the program name

Returns:    EXIT_SUCCESS if terminated successfully
            EXIT_FAILURE otherwise, except when a message has been sent
              to the sender, and -oee was given
*/

int
main(int argc, char **cargv)
{
uschar **argv = USS cargv;
int  arg_receive_timeout = -1;
int  arg_smtp_receive_timeout = -1;
int  arg_error_handling = error_handling;
int  filter_fd = -1;
int  group_count;
int  i;
int  list_queue_option = 0;
int  msg_action = 0;
int  msg_action_arg = -1;
int  namelen = (argv[0] == NULL)? 0 : Ustrlen(argv[0]);
int  queue_only_reason = 0;
#ifdef EXIM_PERL
int  perl_start_option = 0;
#endif
int  recipients_arg = argc;
int  sender_address_domain = 0;
int  test_retry_arg = -1;
int  test_rewrite_arg = -1;
BOOL arg_queue_only = FALSE;
BOOL bi_option = FALSE;
BOOL checking = FALSE;
BOOL count_queue = FALSE;
BOOL expansion_test = FALSE;
BOOL extract_recipients = FALSE;
BOOL forced_delivery = FALSE;
BOOL f_end_dot = FALSE;
BOOL deliver_give_up = FALSE;
BOOL list_queue = FALSE;
BOOL list_options = FALSE;
BOOL local_queue_only;
BOOL more = TRUE;
BOOL one_msg_action = FALSE;
BOOL queue_only_set = FALSE;
BOOL receiving_message = TRUE;
BOOL unprivileged;
BOOL removed_privilege = FALSE;
BOOL verify_address_mode = FALSE;
BOOL verify_as_sender = FALSE;
BOOL version_printed = FALSE;
uschar *alias_arg = NULL;
uschar *called_as = US"";
uschar *start_queue_run_id = NULL;
uschar *stop_queue_run_id = NULL;
uschar *ftest_domain = NULL;
uschar *ftest_localpart = NULL;
uschar *ftest_prefix = NULL;
uschar *ftest_suffix = NULL;
uschar *real_sender_address;
uschar *originator_home = US"/";
BOOL ftest_system = FALSE;
void *reset_point;

struct passwd *pw;
struct stat statbuf;
pid_t passed_qr_pid = (pid_t)0;
int passed_qr_pipe = -1;
gid_t group_list[NGROUPS_MAX];

/* Possible options for -R and -S */

static uschar *rsopts[] = { US"f", US"ff", US"r", US"rf", US"rff" };

/* Need to define this in case we need to change the environment in order
to get rid of a bogus time zone. We have to make it char rather than uschar
because some OS define it in /usr/include/unistd.h. */

extern char **environ;

/* If the Exim user and/or group and/or the configuration file owner were
defined by ref:name at build time, we must now find the actual uid/gid values.
This is a feature to make the lives of binary distributors easier. */

#ifdef EXIM_USERNAME
if (route_finduser(US EXIM_USERNAME, &pw, &exim_uid))
  {
  exim_gid = pw->pw_gid;
  }
else
  {
  fprintf(stderr, "exim: failed to find uid for user name \"%s\"\n",
    EXIM_USERNAME);
  exit(EXIT_FAILURE);
  }
#endif

#ifdef EXIM_GROUPNAME
if (!route_findgroup(US EXIM_GROUPNAME, &exim_gid))
  {
  fprintf(stderr, "exim: failed to find gid for group name \"%s\"\n",
    EXIM_GROUPNAME);
  exit(EXIT_FAILURE);
  }
#endif

#ifdef CONFIGURE_OWNERNAME
if (!route_finduser(US CONFIGURE_OWNERNAME, NULL, &config_uid))
  {
  fprintf(stderr, "exim: failed to find uid for user name \"%s\"\n",
    CONFIGURE_OWNERNAME);
  exit(EXIT_FAILURE);
  }
#endif

/* In the Cygwin environment, some initialization needs doing. It is fudged
in by means of this macro. */

#ifdef OS_INIT
OS_INIT
#endif

/* Check a field which is patched when we are running Exim within its
testing harness; do a fast initial check, and then the whole thing. */

running_in_test_harness =
  *running_status == '<' && Ustrcmp(running_status, "<<<testing>>>") == 0;

/* The C standard says that the equivalent of setlocale(LC_ALL, "C") is obeyed
at the start of a program; however, it seems that some environments do not
follow this. A "strange" locale can affect the formatting of timestamps, so we
make quite sure. */

setlocale(LC_ALL, "C");

/* Set up the default handler for timing using alarm(). */

os_non_restarting_signal(SIGALRM, sigalrm_handler);

/* Ensure we have a buffer for constructing log entries. Use malloc directly,
because store_malloc writes a log entry on failure. */

log_buffer = (uschar *)malloc(LOG_BUFFER_SIZE);
if (log_buffer == NULL)
  {
  fprintf(stderr, "exim: failed to get store for log buffer\n");
  exit(EXIT_FAILURE);
  }

/* Set log_stderr to stderr, provided that stderr exists. This gets reset to
NULL when the daemon is run and the file is closed. We have to use this
indirection, because some systems don't allow writing to the variable "stderr".
*/

if (fstat(fileno(stderr), &statbuf) >= 0) log_stderr = stderr;

/* Arrange for the PCRE regex library to use our store functions. Note that
the normal calls are actually macros that add additional arguments for
debugging purposes so we have to assign specially constructed functions here.
The default is to use store in the stacking pool, but this is overridden in the
regex_must_compile() function. */

pcre_malloc = function_store_get;
pcre_free = function_dummy_free;

/* Ensure there is a big buffer for temporary use in several places. It is put
in malloc store so that it can be freed for enlargement if necessary. */

big_buffer = store_malloc(big_buffer_size);

/* Set up the handler for the data request signal, and set the initial
descriptive text. */

set_process_info("initializing");
os_restarting_signal(SIGUSR1, usr1_handler);

/* SIGHUP is used to get the daemon to reconfigure. It gets set as appropriate
in the daemon code. For the rest of Exim's uses, we ignore it. */

signal(SIGHUP, SIG_IGN);

/* We don't want to die on pipe errors as the code is written to handle
the write error instead. */

signal(SIGPIPE, SIG_IGN);

/* Under some circumstance on some OS, Exim can get called with SIGCHLD
set to SIG_IGN. This causes subprocesses that complete before the parent
process waits for them not to hang around, so when Exim calls wait(), nothing
is there. The wait() code has been made robust against this, but let's ensure
that SIGCHLD is set to SIG_DFL, because it's tidier to wait and get a process
ending status. We use sigaction rather than plain signal() on those OS where
SA_NOCLDWAIT exists, because we want to be sure it is turned off. (There was a
problem on AIX with this.) */

#ifdef SA_NOCLDWAIT
  {
  struct sigaction act;
  act.sa_handler = SIG_DFL;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;
  sigaction(SIGCHLD, &act, NULL);
  }
#else
signal(SIGCHLD, SIG_DFL);
#endif

/* Save the arguments for use if we re-exec exim as a daemon after receiving
SIGHUP. */

sighup_argv = argv;

/* Set up the version number. Set up the leading 'E' for the external form of
message ids, set the pointer to the internal form, and initialize it to
indicate no message being processed. */

version_init();
message_id_option[0] = '-';
message_id_external = message_id_option + 1;
message_id_external[0] = 'E';
message_id = message_id_external + 1;
message_id[0] = 0;

/* Set the umask to zero so that any files that Exim creates are created
with the modes that it specifies. */

umask(0);

/* Precompile the regular expression for matching a message id. Keep this in
step with the code that generates ids in the accept.c module. We need to do
this here, because the -M options check their arguments for syntactic validity
using mac_ismsgid, which uses this. */

regex_ismsgid =
  regex_must_compile(US"^(?:[^\\W_]{6}-){2}[^\\W_]{2}$", FALSE, TRUE);

/* If the program is called as "mailq" treat it as equivalent to "exim -bp";
this seems to be a generally accepted convention, since one finds symbolic
links called "mailq" in standard OS configurations. */

if ((namelen == 5 && Ustrcmp(argv[0], "mailq") == 0) ||
    (namelen  > 5 && Ustrncmp(argv[0] + namelen - 6, "/mailq", 6) == 0))
  {
  list_queue = TRUE;
  receiving_message = FALSE;
  called_as = US"-mailq";
  }

/* If the program is called as "rmail" treat it as equivalent to
"exim -i -oee", thus allowing UUCP messages to be input using non-SMTP mode,
i.e. preventing a single dot on a line from terminating the message, and
returning with zero return code, even in cases of error (provided an error
message has been sent). */

if ((namelen == 5 && Ustrcmp(argv[0], "rmail") == 0) ||
    (namelen  > 5 && Ustrncmp(argv[0] + namelen - 6, "/rmail", 6) == 0))
  {
  dot_ends = FALSE;
  called_as = US"-rmail";
  errors_sender_rc = EXIT_SUCCESS;
  }

/* If the program is called as "rsmtp" treat it as equivalent to "exim -bS";
this is a smail convention. */

if ((namelen == 5 && Ustrcmp(argv[0], "rsmtp") == 0) ||
    (namelen  > 5 && Ustrncmp(argv[0] + namelen - 6, "/rsmtp", 6) == 0))
  {
  smtp_input = smtp_batched_input = TRUE;
  called_as = US"-rsmtp";
  }

/* If the program is called as "runq" treat it as equivalent to "exim -q";
this is a smail convention. */

if ((namelen == 4 && Ustrcmp(argv[0], "runq") == 0) ||
    (namelen  > 4 && Ustrncmp(argv[0] + namelen - 5, "/runq", 5) == 0))
  {
  queue_interval = 0;
  receiving_message = FALSE;
  called_as = US"-runq";
  }

/* If the program is called as "newaliases" treat it as equivalent to
"exim -bi"; this is a sendmail convention. */

if ((namelen == 10 && Ustrcmp(argv[0], "newaliases") == 0) ||
    (namelen  > 10 && Ustrncmp(argv[0] + namelen - 11, "/newaliases", 11) == 0))
  {
  bi_option = TRUE;
  receiving_message = FALSE;
  called_as = US"-newaliases";
  }

/* Save the original effective uid for a couple of uses later. It should
normally be root, but in some esoteric environments it may not be. */

original_euid = geteuid();

/* Get the real uid and gid. If the caller is root, force the effective uid/gid
to be the same as the real ones. This makes a difference only if Exim is setuid
(or setgid) to something other than root, which could be the case in some
special configurations. */

real_uid = getuid();
real_gid = getgid();

if (real_uid == root_uid)
  {
  setgid(real_gid);
  setuid(real_uid);
  }

/* If neither the original real uid nor the original euid was root, Exim is
running in an unprivileged state. */

unprivileged = (real_uid != root_uid && original_euid != root_uid);

/* If the first argument is --help, pretend there are no arguments. This will
cause a brief message to be given. */

if (argc > 1 && Ustrcmp(argv[1], "--help") == 0) argc = 1;

/* Scan the program's arguments. Some can be dealt with right away; others are
simply recorded for checking and handling afterwards. Do a high-level switch
on the second character (the one after '-'), to save some effort. */

for (i = 1; i < argc; i++)
  {
  BOOL badarg = FALSE;
  uschar *arg = argv[i];
  uschar *argrest;
  int switchchar;

  /* An argument not starting with '-' is the start of a recipients list;
  break out of the options-scanning loop. */

  if (arg[0] != '-')
    {
    recipients_arg = i;
    break;
    }

  /* An option consistion of -- terminates the options */

  if (Ustrcmp(arg, "--") == 0)
    {
    recipients_arg = i + 1;
    break;
    }

  /* Handle flagged options */

  switchchar = arg[1];
  argrest = arg+2;

  /* Make all -ex options synonymous with -oex arguments, since that
  is assumed by various callers. Also make -qR options synonymous with -R
  options, as that seems to be required as well. Allow for -qqR too, and
  the same for -S options. */

  if (Ustrncmp(arg+1, "oe", 2) == 0 ||
      Ustrncmp(arg+1, "qR", 2) == 0 ||
      Ustrncmp(arg+1, "qS", 2) == 0)
    {
    switchchar = arg[2];
    argrest++;
    }
  else if (Ustrncmp(arg+1, "qqR", 3) == 0 || Ustrncmp(arg+1, "qqS", 3) == 0)
    {
    switchchar = arg[3];
    argrest += 2;
    queue_2stage = TRUE;
    }

  /* Make -r synonymous with -f, since it is a documented alias */

  else if (arg[1] == 'r') switchchar = 'f';

  /* Make -ov synonymous with -v */

  else if (Ustrcmp(arg, "-ov") == 0)
    {
    switchchar = 'v';
    argrest++;
    }

  /* High-level switch on active initial letter */

  switch(switchchar)
    {
    /* -Btype is a sendmail option for 7bit/8bit setting. Exim is 8-bit clean
    so has no need of it. */

    case 'B':
    if (*argrest == 0) i++;       /* Skip over the type */
    break;


    case 'b':
    receiving_message = FALSE;    /* Reset TRUE for -bm, -bS, -bs below */

    /* -bd:  Run in daemon mode, awaiting SMTP connections.
       -bdf: Ditto, but in the foreground.
    */

    if (*argrest == 'd')
      {
      daemon_listen = TRUE;
      if (*(++argrest) == 'f') background_daemon = FALSE;
        else if (*argrest != 0) { badarg = TRUE; break; }
      }

    /* -be: Run in expansion test mode */

    else if (*argrest == 'e')
      expansion_test = checking = TRUE;

    /* -bf:  Run in mail filter testing mode
       -bF:  Ditto, but for system filters
       -bfd: Set domain for filter testing
       -bfl: Set local part for filter testing
       -bfp: Set prefix for filter testing
       -bfs: Set suffix for filter testing
    */

    else if (*argrest == 'f' || *argrest == 'F')
      {
      ftest_system = *argrest++ == 'F';
      if (*argrest == 0)
        {
        if(++i < argc) filter_test = argv[i]; else
          {
          fprintf(stderr, "exim: file name expected after %s\n", argv[i-1]);
          exit(EXIT_FAILURE);
          }
        }
      else
        {
        if (++i >= argc)
          {
          fprintf(stderr, "exim: string expected after %s\n", arg);
          exit(EXIT_FAILURE);
          }
        if (Ustrcmp(argrest, "d") == 0) ftest_domain = argv[i];
        else if (Ustrcmp(argrest, "l") == 0) ftest_localpart = argv[i];
        else if (Ustrcmp(argrest, "p") == 0) ftest_prefix = argv[i];
        else if (Ustrcmp(argrest, "s") == 0) ftest_suffix = argv[i];
        else { badarg = TRUE; break; }
        }
      }

    /* -bh: Host checking - an IP address must follow. */

    else if (Ustrcmp(argrest, "h") == 0 || Ustrcmp(argrest, "hc") == 0)
      {
      if (++i >= argc) { badarg = TRUE; break; }
      sender_host_address = argv[i];
      host_checking = checking = log_testing_mode = TRUE;
      host_checking_callout = argrest[1] == 'c';
      }

    /* -bi: This option is used by sendmail to initialize *the* alias file,
    though it has the -oA option to specify a different file. Exim has no
    concept of *the* alias file, but since Sun's YP make script calls
    sendmail this way, some support must be provided. */

    else if (Ustrcmp(argrest, "i") == 0) bi_option = TRUE;

    /* -bm: Accept and deliver message - the default option. Reinstate
    receiving_message, which got turned off for all -b options. */

    else if (Ustrcmp(argrest, "m") == 0) receiving_message = TRUE;

    /* -bnq: For locally originating messages, do not qualify unqualified
    addresses. In the envelope, this causes errors; in header lines they
    just get left. */

    else if (Ustrcmp(argrest, "nq") == 0)
      {
      allow_unqualified_sender = FALSE;
      allow_unqualified_recipient = FALSE;
      }

    /* -bpxx: List the contents of the mail queue, in various forms. If
    the option is -bpc, just a queue count is needed. Otherwise, if the
    first letter after p is r, then order is random. */

    else if (*argrest == 'p')
      {
      if (*(++argrest) == 'c')
        {
        count_queue = TRUE;
        if (*(++argrest) != 0) badarg = TRUE;
        break;
        }

      if (*argrest == 'r')
        {
        list_queue_option = 8;
        argrest++;
        }
      else list_queue_option = 0;

      list_queue = TRUE;

      /* -bp: List the contents of the mail queue, top-level only */

      if (*argrest == 0) {}

      /* -bpu: List the contents of the mail queue, top-level undelivered */

      else if (Ustrcmp(argrest, "u") == 0) list_queue_option += 1;

      /* -bpa: List the contents of the mail queue, including all delivered */

      else if (Ustrcmp(argrest, "a") == 0) list_queue_option += 2;

      /* Unknown after -bp[r] */

      else
        {
        badarg = TRUE;
        break;
        }
      }


    /* -bP: List the configuration variables given as the address list.
    Force -v, so configuration errors get displayed. */

    else if (Ustrcmp(argrest, "P") == 0)
      {
      list_options = TRUE;
      debug_selector |= D_v;
      debug_file = stderr;
      }

    /* -brt: Test retry configuration lookup */

    else if (Ustrcmp(argrest, "rt") == 0)
      {
      test_retry_arg = i + 1;
      goto END_ARG;
      }

    /* -brw: Test rewrite configuration */

    else if (Ustrcmp(argrest, "rw") == 0)
      {
      test_rewrite_arg = i + 1;
      goto END_ARG;
      }

    /* -bS: Read SMTP commands on standard input, but produce no replies -
    all errors are reported by sending messages. */

    else if (Ustrcmp(argrest, "S") == 0)
      smtp_input = smtp_batched_input = receiving_message = TRUE;

    /* -bs: Read SMTP commands on standard input and produce SMTP replies
    on standard output. */

    else if (Ustrcmp(argrest, "s") == 0) smtp_input = receiving_message = TRUE;

    /* -bt: address testing mode */

    else if (Ustrcmp(argrest, "t") == 0)
      address_test_mode = checking = log_testing_mode = TRUE;

    /* -bv: verify addresses */

    else if (Ustrcmp(argrest, "v") == 0)
      verify_address_mode = checking = log_testing_mode = TRUE;

    /* -bvs: verify sender addresses */

    else if (Ustrcmp(argrest, "vs") == 0)
      {
      verify_address_mode = checking = log_testing_mode = TRUE;
      verify_as_sender = TRUE;
      }

    /* -bV: Print version string and support details */

    else if (Ustrcmp(argrest, "V") == 0)
      {
      printf("Exim version %s #%s built %s\n", version_string,
        version_cnumber, version_date);
      printf("%s\n", CS version_copyright);
      version_printed = TRUE;
      show_whats_supported(stdout);
      }

    else badarg = TRUE;
    break;


    /* -C: change configuration file list; ignore if it isn't really
    a change! Enforce a prefix check if required. */

    case 'C':
    if (*argrest == 0)
      {
      if(++i < argc) argrest = argv[i]; else
        { badarg = TRUE; break; }
      }
    if (Ustrcmp(config_main_filelist, argrest) != 0)
      {
      #ifdef ALT_CONFIG_PREFIX
      int sep = 0;
      int len = Ustrlen(ALT_CONFIG_PREFIX);
      uschar *list = argrest;
      uschar *filename;
      while((filename = string_nextinlist(&list, &sep, big_buffer,
             big_buffer_size)) != NULL)
        {
        if ((Ustrlen(filename) < len ||
             Ustrncmp(filename, ALT_CONFIG_PREFIX, len) != 0 ||
             Ustrstr(filename, "/../") != NULL) &&
             (Ustrcmp(filename, "/dev/null") != 0 || real_uid != root_uid))
          {
          fprintf(stderr, "-C Permission denied\n");
          exit(EXIT_FAILURE);
          }
        }
      #endif

      config_main_filelist = argrest;
      config_changed = TRUE;
      }
    break;


    /* -D: set up a macro definition */

    case 'D':
    #ifdef DISABLE_D_OPTION
    fprintf(stderr, "exim: -D is not available in this Exim binary\n");
    exit(EXIT_FAILURE);
    #else
      {
      int ptr = 0;
      macro_item *mlast = NULL;
      macro_item *m;
      uschar name[24];
      uschar *s = argrest;

      while (isspace(*s)) s++;

      if (*s < 'A' || *s > 'Z')
        {
        fprintf(stderr, "exim: macro name set by -D must start with "
          "an upper case letter\n");
        exit(EXIT_FAILURE);
        }

      while (isalnum(*s) || *s == '_')
        {
        if (ptr < sizeof(name)-1) name[ptr++] = *s;
        s++;
        }
      name[ptr] = 0;
      if (ptr == 0) { badarg = TRUE; break; }
      while (isspace(*s)) s++;
      if (*s != 0)
        {
        if (*s++ != '=') { badarg = TRUE; break; }
        while (isspace(*s)) s++;
        }

      for (m = macros; m != NULL; m = m->next)
        {
        if (Ustrcmp(m->name, name) == 0)
          {
          fprintf(stderr, "exim: duplicated -D in command line\n");
          exit(EXIT_FAILURE);
          }
        mlast = m;
        }

      m = store_get(sizeof(macro_item) + Ustrlen(name));
      m->next = NULL;
      m->command_line = TRUE;
      if (mlast == NULL) macros = m; else mlast->next = m;
      Ustrcpy(m->name, name);
      m->replacement = string_copy(s);

      if (clmacro_count >= MAX_CLMACROS)
        {
        fprintf(stderr, "exim: too many -D options on command line\n");
        exit(EXIT_FAILURE);
        }
      clmacros[clmacro_count++] = string_sprintf("-D%s=%s", m->name,
        m->replacement);
      }
    #endif
    break;

    /* -d: Set debug level (see also -v below) or set the drop_cr option.
    The latter is now a no-opt, retained for compatibility only. */

    case 'd':
    if (Ustrcmp(argrest, "ropcr") == 0)
      {
      /* drop_cr = TRUE; */
      }

    /* Use an intermediate variable so that we don't set debugging while
    decoding the debugging bits. */

    else
      {
      unsigned int selector = D_default;
      debug_selector = 0;
      debug_file = NULL;
      if (*argrest != 0)
        decode_bits(&selector, NULL, argrest, debug_options,
          debug_options_count, US"debug");
      debug_selector = selector;
      }
    break;


    /* -E: This is a local error message. This option is not intended for
    external use at all, but is not restricted to trusted callers because it
    does no harm (just suppresses certain error messages) and if Exim is run
    not setuid root it won't always be trusted when it generates error
    messages using this option. If there is a message id following -E, point
    message_reference at it, for logging. */

    case 'E':
    local_error_message = TRUE;
    if (mac_ismsgid(argrest)) message_reference = argrest;
    break;


    /* -ex: The vacation program calls sendmail with the undocumented "-eq"
    option, so it looks as if historically the -oex options are also callable
    without the leading -o. So we have to accept them. Before the switch,
    anything starting -oe has been converted to -e. Exim does not support all
    of the sendmail error options. */

    case 'e':
    if (Ustrcmp(argrest, "e") == 0)
      {
      arg_error_handling = ERRORS_SENDER;
      errors_sender_rc = EXIT_SUCCESS;
      }
    else if (Ustrcmp(argrest, "m") == 0) arg_error_handling = ERRORS_SENDER;
    else if (Ustrcmp(argrest, "p") == 0) arg_error_handling = ERRORS_STDERR;
    else if (Ustrcmp(argrest, "q") == 0) arg_error_handling = ERRORS_STDERR;
    else if (Ustrcmp(argrest, "w") == 0) arg_error_handling = ERRORS_SENDER;
    else badarg = TRUE;
    break;


    /* -F: Set sender's full name, used instead of the gecos entry from
    the password file. Since users can usually alter their gecos entries,
    there's no security involved in using this instead. The data can follow
    the -F or be in the next argument. */

    case 'F':
    if (*argrest == 0)
      {
      if(++i < argc) argrest = argv[i]; else
        { badarg = TRUE; break; }
      }
    originator_name = argrest;
    break;


    /* -f: Set sender's address - this value is only actually used if Exim is
    run by a trusted user, or if untrusted_set_sender is set and matches the
    address, except that the null address can always be set by any user. The
    test for this happens later, when the value given here is ignored when not
    permitted. For an untrusted user, the actual sender is still put in Sender:
    if it doesn't match the From: header (unless no_local_from_check is set).
    The data can follow the -f or be in the next argument. The -r switch is an
    obsolete form of -f but since there appear to be programs out there that
    use anything that sendmail has ever supported, better accept it - the
    synonymizing is done before the switch above.

    At this stage, we must allow domain literal addresses, because we don't
    know what the setting of allow_domain_literals is yet. Ditto for trailing
    dots and strip_trailing_dot. */

    case 'f':
      {
      int start, end;
      uschar *errmess;
      if (*argrest == 0)
        {
        if (i+1 < argc) argrest = argv[++i]; else
          { badarg = TRUE; break; }
        }
      if (*argrest == 0)
        {
        sender_address = string_sprintf("");  /* Ensure writeable memory */
        }
      else
        {
        uschar *temp = argrest + Ustrlen(argrest) - 1;
        while (temp >= argrest && isspace(*temp)) temp--;
        if (temp >= argrest && *temp == '.') f_end_dot = TRUE;
        allow_domain_literals = TRUE;
        strip_trailing_dot = TRUE;
        sender_address = parse_extract_address(argrest, &errmess, &start, &end,
          &sender_address_domain, TRUE);
        allow_domain_literals = FALSE;
        strip_trailing_dot = FALSE;
        if (sender_address == NULL)
          {
          fprintf(stderr, "exim: bad -f address \"%s\": %s\n", argrest, errmess);
          return EXIT_FAILURE;
          }
        }
      sender_address_forced = TRUE;
      }
    break;

    /* This is some Sendmail thing which can be ignored */

    case 'G':
    break;

    /* -h: Set the hop count for an incoming message. Exim does not currently
    support this; it always computes it by counting the Received: headers.
    To put it in will require a change to the spool header file format. */

    case 'h':
    if (*argrest == 0)
      {
      if(++i < argc) argrest = argv[i]; else
        { badarg = TRUE; break; }
      }
    if (!isdigit(*argrest)) badarg = TRUE;
    break;


    /* -i: Set flag so dot doesn't end non-SMTP input (same as -oi, seems
    not to be documented for sendmail but mailx (at least) uses it) */

    case 'i':
    if (*argrest == 0) dot_ends = FALSE; else badarg = TRUE;
    break;


    case 'M':
    receiving_message = FALSE;

    /* -MC:  continue delivery of another message via an existing open
    file descriptor. This option is used for an internal call by the
    smtp transport when there is a pending message waiting to go to an
    address to which it has got a connection. Five subsequent arguments are
    required: transport name, host name, IP address, sequence number, and
    message_id. Transports may decline to create new processes if the sequence
    number gets too big. The channel is stdin. This (-MC) must be the last
    argument. There's a subsequent check that the real-uid is privileged.

    If we are running in the test harness. delay for a bit, to let the process
    that set this one up complete. This makes for repeatability of the logging,
    etc. output. */

    if (Ustrcmp(argrest, "C") == 0)
      {
      if (argc != i + 6)
        {
        fprintf(stderr, "exim: too many or too few arguments after -MC\n");
        return EXIT_FAILURE;
        }

      if (msg_action_arg >= 0)
        {
        fprintf(stderr, "exim: incompatible arguments\n");
        return EXIT_FAILURE;
        }

      continue_transport = argv[++i];
      continue_hostname = argv[++i];
      continue_host_address = argv[++i];
      continue_sequence = Uatoi(argv[++i]);
      msg_action = MSG_DELIVER;
      msg_action_arg = ++i;
      forced_delivery = TRUE;
      queue_run_pid = passed_qr_pid;
      queue_run_pipe = passed_qr_pipe;

      if (!mac_ismsgid(argv[i]))
        {
        fprintf(stderr, "exim: malformed message id %s after -MC option\n",
          argv[i]);
        return EXIT_FAILURE;
        }

      if (running_in_test_harness) millisleep(500);
      break;
      }

    /* -MCA: set the smtp_authenticated flag; this is useful only when it
    precedes -MC (see above). The flag indicates that the host to which
    Exim is connected has accepted an AUTH sequence. */

    else if (Ustrcmp(argrest, "CA") == 0)
      {
      smtp_authenticated = TRUE;
      break;
      }

    /* -MCP: set the smtp_use_pipelining flag; this is useful only when
    it preceded -MC (see above) */

    else if (Ustrcmp(argrest, "CP") == 0)
      {
      smtp_use_pipelining = TRUE;
      break;
      }

    /* -MCQ: pass on the pid of the queue-running process that started
    this chain of deliveries and the fd of its synchronizing pipe; this
    is useful only when it precedes -MC (see above) */

    else if (Ustrcmp(argrest, "CQ") == 0)
      {
      if(++i < argc) passed_qr_pid = (pid_t)(Uatol(argv[i]));
        else badarg = TRUE;
      if(++i < argc) passed_qr_pipe = (int)(Uatol(argv[i]));
        else badarg = TRUE;
      break;
      }

    /* -MCS: set the smtp_use_size flag; this is useful only when it
    precedes -MC (see above) */

    else if (Ustrcmp(argrest, "CS") == 0)
      {
      smtp_use_size = TRUE;
      break;
      }

    /* -MCT: set the tls_offered flag; this is useful only when it
    precedes -MC (see above). The flag indicates that the host to which
    Exim is connected has offered TLS support. */

    #ifdef SUPPORT_TLS
    else if (Ustrcmp(argrest, "CT") == 0)
      {
      tls_offered = TRUE;
      break;
      }
    #endif

    /* -M[x]: various operations on the following list of message ids:
       -M    deliver the messages, ignoring next retry times and thawing
       -Mc   deliver the messages, checking next retry times, no thawing
       -Mf   freeze the messages
       -Mg   give up on the messages
       -Mt   thaw the messages
       -Mrm  remove the messages
    In the above cases, this must be the last option. There are also the
    following options which are followed by a single message id, and which
    act on that message. Some of them use the "recipient" addresses as well.
       -Mar  add recipient(s)
       -Mmad mark all recipients delivered
       -Mmd  mark recipients(s) delivered
       -Mes  edit sender
       -Mvb  show body
       -Mvh  show header
       -Mvl  show log
    */

    else if (*argrest == 0)
      {
      msg_action = MSG_DELIVER;
      forced_delivery = deliver_force_thaw = TRUE;
      }
    else if (Ustrcmp(argrest, "ar") == 0)
      {
      msg_action = MSG_ADD_RECIPIENT;
      one_msg_action = TRUE;
      }
    else if (Ustrcmp(argrest, "c") == 0)  msg_action = MSG_DELIVER;
    else if (Ustrcmp(argrest, "es") == 0)
      {
      msg_action = MSG_EDIT_SENDER;
      one_msg_action = TRUE;
      }
    else if (Ustrcmp(argrest, "f") == 0)  msg_action = MSG_FREEZE;
    else if (Ustrcmp(argrest, "g") == 0)
      {
      msg_action = MSG_DELIVER;
      deliver_give_up = TRUE;
      }
    else if (Ustrcmp(argrest, "mad") == 0)
      {
      msg_action = MSG_MARK_ALL_DELIVERED;
      }
    else if (Ustrcmp(argrest, "md") == 0)
      {
      msg_action = MSG_MARK_DELIVERED;
      one_msg_action = TRUE;
      }
    else if (Ustrcmp(argrest, "rm") == 0) msg_action = MSG_REMOVE;
    else if (Ustrcmp(argrest, "t") == 0)  msg_action = MSG_THAW;
    else if (Ustrcmp(argrest, "vb") == 0)
      {
      msg_action = MSG_SHOW_BODY;
      one_msg_action = TRUE;
      }
    else if (Ustrcmp(argrest, "vh") == 0)
      {
      msg_action = MSG_SHOW_HEADER;
      one_msg_action = TRUE;
      }
    else if (Ustrcmp(argrest, "vl") == 0)
      {
      msg_action = MSG_SHOW_LOG;
      one_msg_action = TRUE;
      }
    else { badarg = TRUE; break; }

    /* All the -Mxx options require at least one message id. */

    msg_action_arg = i + 1;
    if (msg_action_arg >= argc)
      {
      fprintf(stderr, "exim: no message ids given after %s option\n", arg);
      return EXIT_FAILURE;
      }

    /* Some require only message ids to follow */

    if (!one_msg_action)
      {
      int j;
      for (j = msg_action_arg; j < argc; j++) if (!mac_ismsgid(argv[j]))
        {
        fprintf(stderr, "exim: malformed message id %s after %s option\n",
          argv[j], arg);
        return EXIT_FAILURE;
        }
      goto END_ARG;   /* Remaining args are ids */
      }

    /* Others require only one message id, possibly followed by addresses,
    which will be handled as normal arguments. */

    else
      {
      if (!mac_ismsgid(argv[msg_action_arg]))
        {
        fprintf(stderr, "exim: malformed message id %s after %s option\n",
          argv[msg_action_arg], arg);
        return EXIT_FAILURE;
        }
      i++;
      }
    break;


    /* Some programs seem to call the -om option without the leading o;
    for sendmail it askes for "me too". Exim always does this. */

    case 'm':
    if (*argrest != 0) badarg = TRUE;
    break;


    /* -N: don't do delivery - a debugging option that stops transports doing
    their thing. It implies debugging at the D_v level. */

    case 'N':
    if (*argrest == 0)
      {
      dont_deliver = TRUE;
      debug_selector |= D_v;
      debug_file = stderr;
      }
    else badarg = TRUE;
    break;


    /* -n: This means "don't alias" in sendmail, apparently. Just ignore
    it. */

    case 'n':
    break;

    /* -O: Just ignore it. In sendmail, apparently -O option=value means set
    option to the specified value. This form uses long names. We need to handle
    -O option=value and -Ooption=value. */

    case 'O':
    if (*argrest == 0)
      {
      if (++i >= argc)
        {
        fprintf(stderr, "exim: string expected after -O\n");
        exit(EXIT_FAILURE);
        }
      }
    break;

    case 'o':

    /* -oA: Set an argument for the bi command (sendmail's "alternate alias
    file" option). */

    if (*argrest == 'A')
      {
      alias_arg = argrest + 1;
      if (alias_arg[0] == 0)
        {
        if (i+1 < argc) alias_arg = argv[++i]; else
          {
          fprintf(stderr, "exim: string expected after -oA\n");
          exit(EXIT_FAILURE);
          }
        }
      }

    /* -oB: Set a connection message max value for remote deliveries */

    else if (*argrest == 'B')
      {
      uschar *p = argrest + 1;
      if (p[0] == 0)
        {
        if (i+1 < argc && isdigit((argv[i+1][0]))) p = argv[++i]; else
          {
          connection_max_messages = 1;
          p = NULL;
          }
        }

      if (p != NULL)
        {
        if (!isdigit(*p))
          {
          fprintf(stderr, "exim: number expected after -oB\n");
          exit(EXIT_FAILURE);
          }
        connection_max_messages = Uatoi(p);
        }
      }

    /* -odb: background delivery */

    else if (Ustrcmp(argrest, "db") == 0)
      {
      synchronous_delivery = FALSE;
      arg_queue_only = FALSE;
      queue_only_set = TRUE;
      }

    /* -odf: foreground delivery (smail-compatible option); same effect as
       -odi: interactive (synchronous) delivery (sendmail-compatible option)
    */

    else if (Ustrcmp(argrest, "df") == 0 || Ustrcmp(argrest, "di") == 0)
      {
      synchronous_delivery = TRUE;
      arg_queue_only = FALSE;
      queue_only_set = TRUE;
      }

    /* -odq: queue only */

    else if (Ustrcmp(argrest, "dq") == 0)
      {
      synchronous_delivery = FALSE;
      arg_queue_only = TRUE;
      queue_only_set = TRUE;
      }

    /* -odqs: queue SMTP only - do local deliveries and remote routing,
    but no remote delivery */

    else if (Ustrcmp(argrest, "dqs") == 0)
      {
      queue_smtp = TRUE;
      arg_queue_only = FALSE;
      queue_only_set = TRUE;
      }

    /* -oex: Sendmail error flags. As these are also accepted without the
    leading -o prefix, for compatibility with vacation and other callers,
    they are handled with -e above. */

    /* -oi:     Set flag so dot doesn't end non-SMTP input (same as -i)
       -oitrue: Another sendmail syntax for the same */

    else if (Ustrcmp(argrest, "i") == 0 ||
             Ustrcmp(argrest, "itrue") == 0)
      dot_ends = FALSE;

    /* -oM*: Set various characteristics for an incoming message; actually
    acted on for trusted callers only. */

    else if (*argrest == 'M')
      {
      if (i+1 >= argc)
        {
        fprintf(stderr, "exim: data expected after -o%s\n", argrest);
        exit(EXIT_FAILURE);
        }

      /* -oMa: Set sender host address */

      if (Ustrcmp(argrest, "Ma") == 0) sender_host_address = argv[++i];

      /* -oMaa: Set authenticator name */

      else if (Ustrcmp(argrest, "Maa") == 0)
        sender_host_authenticated = argv[++i];

      /* -oMas: setting authenticated sender */

      else if (Ustrcmp(argrest, "Mas") == 0) authenticated_sender = argv[++i];

      /* -oMai: setting authenticated id */

      else if (Ustrcmp(argrest, "Mai") == 0) authenticated_id = argv[++i];

      /* -oMi: Set incoming interface address */

      else if (Ustrcmp(argrest, "Mi") == 0) interface_address = argv[++i];

      /* -oMr: Received protocol */

      else if (Ustrcmp(argrest, "Mr") == 0) received_protocol = argv[++i];

      /* -oMs: Set sender host name */

      else if (Ustrcmp(argrest, "Ms") == 0) sender_host_name = argv[++i];

      /* -oMt: Set sender ident */

      else if (Ustrcmp(argrest, "Mt") == 0) sender_ident = argv[++i];

      /* Else a bad argument */

      else
        {
        badarg = TRUE;
        break;
        }
      }

    /* -om: Me-too flag for aliases. Exim always does this. Some programs
    seem to call this as -m (undocumented), so that is also accepted (see
    above). */

    else if (Ustrcmp(argrest, "m") == 0) {}

    /* -oo: An ancient flag for old-style addresses which still seems to
    crop up in some calls (see in SCO). */

    else if (Ustrcmp(argrest, "o") == 0) {}

    /* -oP <name>: set pid file path for daemon */

    else if (Ustrcmp(argrest, "P") == 0)
      override_pid_file_path = argv[++i];

    /* -or <n>: set timeout for non-SMTP acceptance
       -os <n>: set timeout for SMTP acceptance */

    else if (*argrest == 'r' || *argrest == 's')
      {
      int *tp = (*argrest == 'r')?
        &arg_receive_timeout : &arg_smtp_receive_timeout;
      if (argrest[1] == 0)
        {
        if (i+1 < argc) *tp= readconf_readtime(argv[++i], 0, FALSE);
        }
      else *tp = readconf_readtime(argrest + 1, 0, FALSE);
      if (*tp < 0)
        {
        fprintf(stderr, "exim: bad time value %s: abandoned\n", argv[i]);
        exit(EXIT_FAILURE);
        }
      }

    /* -oX <list>: Override local_interfaces and/or default daemon ports */

    else if (Ustrcmp(argrest, "X") == 0)
      override_local_interfaces = argv[++i];

    /* Unknown -o argument */

    else badarg = TRUE;
    break;


    /* -ps: force Perl startup; -pd force delayed Perl startup */

    case 'p':
    #ifdef EXIM_PERL
    if (*argrest == 's' && argrest[1] == 0)
      {
      perl_start_option = 1;
      break;
      }
    if (*argrest == 'd' && argrest[1] == 0)
      {
      perl_start_option = -1;
      break;
      }
    #endif

    /* -panythingelse is taken as the Sendmail-compatible argument -prval:sval,
    which sets the host protocol and host name */

    if (*argrest == 0)
      {
      if (i+1 < argc) argrest = argv[++i]; else
        { badarg = TRUE; break; }
      }

    if (*argrest != 0)
      {
      uschar *hn = Ustrchr(argrest, ':');
      if (hn == NULL)
        {
        received_protocol = argrest;
        }
      else
        {
        received_protocol = string_copyn(argrest, hn - argrest);
        sender_host_name = hn + 1;
        }
      }
    break;


    case 'q':
    receiving_message = FALSE;

    /* -qq...: Do queue runs in a 2-stage manner */

    if (*argrest == 'q')
      {
      queue_2stage = TRUE;
      argrest++;
      }

    /* -qi...: Do only first (initial) deliveries */

    if (*argrest == 'i')
      {
      queue_run_first_delivery = TRUE;
      argrest++;
      }

    /* -qf...: Run the queue, forcing deliveries
       -qff..: Ditto, forcing thawing as well */

    if (*argrest == 'f')
      {
      queue_run_force = TRUE;
      if (*(++argrest) == 'f')
        {
        deliver_force_thaw = TRUE;
        argrest++;
        }
      }

    /* -q[f][f]l...: Run the queue only on local deliveries */

    if (*argrest == 'l')
      {
      queue_run_local = TRUE;
      argrest++;
      }

    /* -q[f][f][l]: Run the queue, optionally forced, optionally local only,
    optionally starting from a given message id. */

    if (*argrest == 0 &&
        (i + 1 >= argc || argv[i+1][0] == '-' || mac_ismsgid(argv[i+1])))
      {
      queue_interval = 0;
      if (i+1 < argc && mac_ismsgid(argv[i+1]))
        start_queue_run_id = argv[++i];
      if (i+1 < argc && mac_ismsgid(argv[i+1]))
        stop_queue_run_id = argv[++i];
      }

    /* -q[f][f][l]<n>: Run the queue at regular intervals, optionally forced,
    optionally local only. */

    else
      {
      if (*argrest != 0)
        queue_interval = readconf_readtime(argrest, 0, FALSE);
      else
        queue_interval = readconf_readtime(argv[++i], 0, FALSE);
      if (queue_interval <= 0)
        {
        fprintf(stderr, "exim: bad time value %s: abandoned\n", argv[i]);
        exit(EXIT_FAILURE);
        }
      }
    break;


    case 'R':   /* Synonymous with -qR... */
    receiving_message = FALSE;

    /* -Rf:   As -R (below) but force all deliveries,
       -Rff:  Ditto, but also thaw all frozen messages,
       -Rr:   String is regex
       -Rrf:  Regex and force
       -Rrff: Regex and force and thaw

    in all cases provided there are no further characters in this
    argument. */

    if (*argrest != 0)
      {
      int i;
      for (i = 0; i < sizeof(rsopts)/sizeof(uschar *); i++)
        {
        if (Ustrcmp(argrest, rsopts[i]) == 0)
          {
          if (i != 2) queue_run_force = TRUE;
          if (i >= 2) deliver_selectstring_regex = TRUE;
          if (i == 1 || i == 4) deliver_force_thaw = TRUE;
          argrest += Ustrlen(rsopts[i]);
          }
        }
      }

    /* -R: Set string to match in addresses for forced queue run to
    pick out particular messages. */

    if (*argrest == 0)
      {
      if (i+1 < argc) deliver_selectstring = argv[++i]; else
        {
        fprintf(stderr, "exim: string expected after -R\n");
        exit(EXIT_FAILURE);
        }
      }
    else deliver_selectstring = argrest;
    if (queue_interval < 0) queue_interval = 0;
    break;


    /* -r: an obsolete synonym for -f (see above) */


    /* -S: Like -R but works on sender. */

    case 'S':   /* Synonymous with -qS... */
    receiving_message = FALSE;

    /* -Sf:   As -S (below) but force all deliveries,
       -Sff:  Ditto, but also thaw all frozen messages,
       -Sr:   String is regex
       -Srf:  Regex and force
       -Srff: Regex and force and thaw

    in all cases provided there are no further characters in this
    argument. */

    if (*argrest != 0)
      {
      int i;
      for (i = 0; i < sizeof(rsopts)/sizeof(uschar *); i++)
        {
        if (Ustrcmp(argrest, rsopts[i]) == 0)
          {
          if (i != 2) queue_run_force = TRUE;
          if (i >= 2) deliver_selectstring_sender_regex = TRUE;
          if (i == 1 || i == 4) deliver_force_thaw = TRUE;
          argrest += Ustrlen(rsopts[i]);
          }
        }
      }

    /* -S: Set string to match in addresses for forced queue run to
    pick out particular messages. */

    if (*argrest == 0)
      {
      if (i+1 < argc) deliver_selectstring_sender = argv[++i]; else
        {
        fprintf(stderr, "exim: string expected after -S\n");
        exit(EXIT_FAILURE);
        }
      }
    else deliver_selectstring_sender = argrest;
    if (queue_interval < 0) queue_interval = 0;
    break;

    /* -Tqt is an option that is exclusively for use by the testing suite.
    It is not recognized in other circumstances. It allows for the setting up
    of explicit "queue times" so that various warning/retry things can be
    tested. Otherwise variability of clock ticks etc. cause problems. */

    case 'T':
    if (running_in_test_harness && Ustrcmp(argrest, "qt") == 0)
      fudged_queue_times = argv[++i];
    else badarg = TRUE;
    break;


    /* -t: Set flag to extract recipients from body of message. */

    case 't':
    if (*argrest == 0) extract_recipients = TRUE;

    /* -ti: Set flag to extract recipients from body of message, and also
    specify that dot does not end the message. */

    else if (Ustrcmp(argrest, "i") == 0)
      {
      extract_recipients = TRUE;
      dot_ends = FALSE;
      }

    /* -tls-on-connect: don't wait for STARTTLS (for old clients) */

    #ifdef SUPPORT_TLS
    else if (Ustrcmp(argrest, "ls-on-connect") == 0) tls_on_connect = TRUE;
    #endif

    else badarg = TRUE;
    break;


    /* -U: This means "initial user submission" in sendmail, apparently. The
    doc claims that in future sendmail may refuse syntactically invalid
    messages instead of fixing them. For the moment, we just ignore it. */

    case 'U':
    break;


    /* -v: verify things - this is a very low-level debugging */

    case 'v':
    if (*argrest == 0)
      {
      debug_selector |= D_v;
      debug_file = stderr;
      }
    else badarg = TRUE;
    break;


    /* -x: AIX uses this to indicate some fancy 8-bit character stuff:

      The -x flag tells the sendmail command that mail from a local
      mail program has National Language Support (NLS) extended characters
      in the body of the mail item. The sendmail command can send mail with
      extended NLS characters across networks that normally corrupts these
      8-bit characters.

    As Exim is 8-bit clean, it just ignores this flag. */

    case 'x':
    if (*argrest != 0) badarg = TRUE;
    break;

    /* All other initial characters are errors */

    default:
    badarg = TRUE;
    break;
    }         /* End of high-level switch statement */

  /* Failed to recognize the option, or syntax error */

  if (badarg)
    {
    fprintf(stderr, "exim abandoned: unknown, malformed, or incomplete "
      "option %s\n", arg);
    exit(EXIT_FAILURE);
    }
  }


/* Arguments have been processed. Check for incompatibilities. */

END_ARG:
if ((
    (smtp_input || extract_recipients || recipients_arg < argc) &&
    (daemon_listen || queue_interval >= 0 || bi_option ||
      test_retry_arg >= 0 || test_rewrite_arg >= 0 ||
      filter_test != NULL || (msg_action_arg > 0 && !one_msg_action))
    ) ||
    (
    msg_action_arg > 0 &&
    (daemon_listen || queue_interval >= 0 || list_options || checking ||
     bi_option || test_retry_arg >= 0 || test_rewrite_arg >= 0)
    ) ||
    (
    (daemon_listen || queue_interval >= 0) &&
    (sender_address != NULL || list_options || list_queue || checking ||
     bi_option)
    ) ||
    (
    daemon_listen && queue_interval == 0
    ) ||
    (
    list_options &&
    (checking || smtp_input || extract_recipients ||
      filter_test != NULL || bi_option)
    ) ||
    (
    verify_address_mode &&
    (address_test_mode || smtp_input || extract_recipients ||
      filter_test != NULL || bi_option)
    ) ||
    (
    address_test_mode && (smtp_input || extract_recipients ||
      filter_test != NULL || bi_option)
    ) ||
    (
    smtp_input && (sender_address != NULL || filter_test != NULL ||
      extract_recipients)
    ) ||
    (
    deliver_selectstring != NULL && queue_interval < 0
    )
   )
  {
  fprintf(stderr, "exim: incompatible command-line options or arguments\n");
  exit(EXIT_FAILURE);
  }

/* If debugging is set up, set the file and the file descriptor to pass on to
child processes. It should, of course, be 2 for stderr. Also, force the daemon
to run in the foreground. */

if (debug_selector != 0)
  {
  debug_file = stderr;
  debug_fd = fileno(debug_file);
  background_daemon = FALSE;
  if (running_in_test_harness) millisleep(100);   /* lets caller finish */
  if (debug_selector != D_v)    /* -v only doesn't show this */
    {
    debug_printf("Exim version %s uid=%ld gid=%ld pid=%d D=%x\n",
      version_string, (long int)real_uid, (long int)real_gid, (int)getpid(),
      debug_selector);
    show_whats_supported(stderr);
    }
  }

/* When started with root privilege, ensure that the limits on the number of
open files and the number of processes (where that is accessible) are
sufficiently large, or are unset, in case Exim has been called from an
environment where the limits are screwed down. Not all OS have the ability to
change some of these limits. */

if (unprivileged)
  {
  DEBUG(D_any) debug_print_ids(US"Exim has no root privilege:");
  }
else
  {
  struct rlimit rlp;
  #ifdef RLIMIT_NOFILE
  if (getrlimit(RLIMIT_NOFILE, &rlp) < 0)
    rlp.rlim_cur = rlp.rlim_max = 0;
  if (rlp.rlim_cur < 1000)
    {
    rlp.rlim_cur = rlp.rlim_max = 1000;
    (void)setrlimit(RLIMIT_NOFILE, &rlp);
    }
  #endif
  #ifdef RLIMIT_NPROC
    #ifdef RLIM_INFINITY
    rlp.rlim_cur = rlp.rlim_max = RLIM_INFINITY;
    #else
    rlp.rlim_cur = rlp.rlim_max = 1000;
    #endif
  (void)setrlimit(RLIMIT_NPROC, &rlp);
  #endif
  }

/* Exim is normally entered as root (but some special configurations are
possible that don't do this). However, it always spins off sub-processes that
set their uid and gid as required for local delivery. We don't want to pass on
any extra groups that root may belong to, so we want to get rid of them all at
this point.

We need to obey setgroups() at this stage, before possibly giving up root
privilege for a changed configuration file, but later on we might need to
check on the additional groups for the admin user privilege - can't do that
till after reading the config, which might specify the exim gid. Therefore,
save the group list here first. */

group_count = getgroups(NGROUPS_MAX, group_list);

/* There is a fundamental difference in some BSD systems in the matter of
groups. FreeBSD and BSDI are known to be different; NetBSD and OpenBSD are
known not to be different. On the "different" systems there is a single group
list, and the first entry in it is the current group. On all other versions of
Unix there is a supplementary group list, which is in *addition* to the current
group. Consequently, to get rid of all extraneous groups on a "standard" system
you pass over 0 groups to setgroups(), while on a "different" system you pass
over a single group - the current group, which is always the first group in the
list. Calling setgroups() with zero groups on a "different" system results in
an error return. The following code should cope with both types of system.

However, if this process isn't running as root, setgroups() can't be used
since you have to be root to run it, even if throwing away groups. Not being
root here happens only in some unusual configurations. We just ignore the
error. */

if (setgroups(0, NULL) != 0)
  {
  if (setgroups(1, group_list) != 0 && !unprivileged)
    {
    fprintf(stderr, "exim: setgroups() failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
    }
  }

/* If the configuration file name has been altered by an argument on the
command line (either a new file name or a macro definition) and the caller is
not root or the exim user, or if this is a filter testing run, remove any
setuid privilege the program has, and run as the underlying user.

If ALT_CONFIG_ROOT_ONLY is defined, the exim user is locked out of this, which
severely restricts the use of -C for some purposes.

Otherwise, set the real ids to the effective values (should be root unless run
from inetd, which it can either be root or the exim uid, if one is configured).

There is a private mechanism for bypassing some of this, in order to make it
possible to test lots of configurations automatically, without having either to
recompile each time, or to patch in an actual configuration file name and other
values (such as the path name). If running in the test harness, pretend that
configuration file changes and macro definitions haven't happened. */

if ((                                            /* EITHER */
    (config_changed || macros != NULL) &&        /* Config changed, and */
    real_uid != root_uid &&                      /* Not root, and */
    #ifndef ALT_CONFIG_ROOT_ONLY                 /* (when not locked out) */
    real_uid != exim_uid &&                      /* Not exim, and */
    #endif
    !running_in_test_harness                     /* Not fudged */
    ) ||                                         /*   OR   */
    expansion_test                               /* expansion testing */
    ||                                           /*   OR   */
    filter_test != NULL)                         /* Filter testing */
  {
  setgroups(group_count, group_list);
  exim_setugid(real_uid, real_gid, FALSE,
    US"-C, -D, -be or -bf forces real uid");
  removed_privilege = TRUE;

  /* In the normal case when Exim is called like this, stderr is available
  and should be used for any logging information because attempts to write
  to the log will usually fail. To arrange this, we unset really_exim. However,
  if no stderr is available there is no point - we might as well have a go
  at the log (if it fails, syslog will be written). */

  if (log_stderr != NULL) really_exim = FALSE;
  }

/* Privilege is to be retained for the moment. It may be dropped later,
depending on the job that this Exim process has been asked to do. For now, set
the real uid to the effective so that subsequent re-execs of Exim are done by a
privileged user. */

else exim_setugid(geteuid(), getegid(), FALSE, US"forcing real = effective");

/* If testing a filter, open the file now, before wasting time doing other
setups and reading the message. */

if (filter_test != NULL)
  {
  filter_fd = Uopen(filter_test, O_RDONLY,0);
  if (filter_fd < 0)
    {
    fprintf(stderr, "exim: failed to open %s: %s\n", filter_test,
      strerror(errno));
    return EXIT_FAILURE;
    }
  }

/* Read the main runtime configuration data; this gives up if there
is a failure. It leaves the configuration file open so that the subsequent
configuration data for delivery can be read if needed. */

readconf_main();

/* Handle the decoding of logging options. */

decode_bits(&log_write_selector, &log_extra_selector, log_selector_string,
  log_options, log_options_count, US"log");

DEBUG(D_any)
  {
  debug_printf("configuration file is %s\n", config_main_filename);
  debug_printf("log selectors = %08x %08x\n", log_write_selector,
    log_extra_selector);
  }

/* If domain literals are not allowed, check the sender address that was
supplied with -f. Ditto for a stripped trailing dot. */

if (sender_address != NULL)
  {
  if (sender_address[sender_address_domain] == '[' && !allow_domain_literals)
    {
    fprintf(stderr, "exim: bad -f address \"%s\": domain literals not "
      "allowed\n", sender_address);
    return EXIT_FAILURE;
    }
  if (f_end_dot && !strip_trailing_dot)
    {
    fprintf(stderr, "exim: bad -f address \"%s.\": domain is malformed "
      "(trailing dot not allowed)\n", sender_address);
    return EXIT_FAILURE;
    }
  }

/* Paranoia check of maximum lengths of certain strings. There is a check
on the length of the log file path in log.c, which will come into effect
if there are any calls to write the log earlier than this. However, if we
get this far but the string is very long, it is better to stop now than to
carry on and (e.g.) receive a message and then have to collapse. The call to
log_write() from here will cause the ultimate panic collapse if the complete
file name exceeds the buffer length. */

if (Ustrlen(log_file_path) > 200)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE,
    "log_file_path is longer than 200 chars: aborting");

if (Ustrlen(pid_file_path) > 200)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE,
    "pid_file_path is longer than 200 chars: aborting");

if (Ustrlen(spool_directory) > 200)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE,
    "spool_directory is longer than 200 chars: aborting");

/* Length check on the process name given to syslog for its TAG field,
which is only permitted to be 32 characters or less. See RFC 3164. */

if (Ustrlen(syslog_processname) > 32)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE,
    "syslog_processname is longer than 32 chars: aborting");

/* In some operating systems, the environment variable TMPDIR controls where
temporary files are created; Exim doesn't use these (apart from when delivering
to MBX mailboxes), but called libraries such as DBM libraries may require them.
If TMPDIR is found in the environment, reset it to the value defined in the
TMPDIR macro, if this macro is defined. */

#ifdef TMPDIR
  {
  uschar **p;
  for (p = USS environ; *p != NULL; p++)
    {
    if (Ustrncmp(*p, "TMPDIR=", 7) == 0 &&
        Ustrcmp(*p+7, TMPDIR) != 0)
      {
      uschar *newp = malloc(Ustrlen(TMPDIR) + 8);
      sprintf(CS newp, "TMPDIR=%s", TMPDIR);
      *p = newp;
      DEBUG(D_any) debug_printf("reset TMPDIR=%s in environment\n", TMPDIR);
      }
    }
  }
#endif

/* Timezone handling. If timezone_string is "utc", set a flag to cause all
timestamps to be in UTC (gmtime() is used instead of localtime()). Otherwise,
we may need to get rid of a bogus timezone setting. This can arise when Exim is
called by a user who has set the TZ variable. This then affects the timestamps
in log files and in Received: headers, and any created Date: header lines. The
required timezone is settable in the configuration file, so nothing can be done
about this earlier - but hopefully nothing will normally be logged earlier than
this. We have to make a new environment if TZ is wrong, but don't bother if
timestamps_utc is set, because then all times are in UTC anyway. */

if (timezone_string != NULL && strcmpic(timezone_string, US"UTC") == 0)
  {
  timestamps_utc = TRUE;
  }
else
  {
  uschar *envtz = US getenv("TZ");
  if ((envtz == NULL && timezone_string != NULL) ||
      (envtz != NULL &&
        (timezone_string == NULL ||
         Ustrcmp(timezone_string, envtz) != 0)))
    {
    uschar **p = USS environ;
    uschar **new;
    uschar **newp;
    int count = 0;
    while (*p++ != NULL) count++;
    if (envtz == NULL) count++;
    newp = new = malloc(sizeof(uschar *) * (count + 1));
    for (p = USS environ; *p != NULL; p++)
      {
      if (Ustrncmp(*p, "TZ=", 3) == 0) continue;
      *newp++ = *p;
      }
    if (timezone_string != NULL)
      {
      *newp = malloc(Ustrlen(timezone_string) + 4);
      sprintf(CS *newp++, "TZ=%s", timezone_string);
      }
    *newp = NULL;
    environ = CSS new;
    tzset();
    DEBUG(D_any) debug_printf("Reset TZ to %s: time is %s\n", timezone_string,
      tod_stamp(tod_log));
    }
  }

/* Handle the case when we have removed the setuid privilege because of -C or
-D. This means that the caller of Exim was not root, and, provided that
ALT_CONFIG_ROOT_ONLY is not defined, was not the Exim user that is built into
the binary.

If ALT_CONFIG_ROOT_ONLY is not defined, there is a problem if it turns out we
were running as the exim user defined in the configuration file (different to
the one in the binary). The sysadmin may expect this case to retain privilege
because "the binary was called by the Exim user", but it hasn't, because of the
order in which it handles this stuff. There are two possibilities:

  (1) If deliver_drop_privilege is set, Exim is not going to re-exec in order
      to do message deliveries. Thus, the fact that it is running as a
      non-privileged user is plausible, and might be wanted in some special
      configurations. However, really_exim will have been set false when
      privilege was dropped, to stop Exim trying to write to its normal log
      files. Therefore, re-enable normal log processing, assuming the sysadmin
      has set up the log directory correctly.

  (2) If deliver_drop_privilege is not set, the configuration won't work as
      apparently intended, and so we log a panic message. In order to retain
      root for -C or -D, the caller must either be root or the Exim user
      defined in the binary (when deliver_drop_ privilege is false).

If ALT_CONFIG_ROOT_ONLY is defined, we don't know whether we were called by the
built-in exim user or one defined in the configuration. In either event,
re-enable log processing, assuming the sysadmin knows what they are doing. */

if (removed_privilege && (config_changed || macros != NULL) &&
    real_uid == exim_uid)
  {
  #ifdef ALT_CONFIG_ROOT_ONLY
  really_exim = TRUE;   /* let logging work normally */
  #else

  if (deliver_drop_privilege)
    really_exim = TRUE; /* let logging work normally */
  else
    log_write(0, LOG_MAIN|LOG_PANIC,
      "exim user (uid=%d) is defined only at runtime; privilege lost for %s",
      (int)exim_uid, config_changed? "-C" : "-D");
  #endif
  }

/* Start up Perl interpreter if Perl support is configured and there is a
perl_startup option, and the configuration or the command line specifies
initializing starting. Note that the global variables are actually called
opt_perl_xxx to avoid clashing with perl's namespace (perl_*). */

#ifdef EXIM_PERL
if (perl_start_option != 0)
  opt_perl_at_start = (perl_start_option > 0);
if (opt_perl_at_start && opt_perl_startup != NULL)
  {
  uschar *errstr;
  DEBUG(D_any) debug_printf("Starting Perl interpreter\n");
  errstr = init_perl(opt_perl_startup);
  if (errstr != NULL)
    {
    fprintf(stderr, "exim: error in perl_startup code: %s\n", errstr);
    return EXIT_FAILURE;
    }
  opt_perl_started = TRUE;
  }
#endif /* EXIM_PERL */

/* Log the arguments of the call if the configuration file said so. This is
a debugging feature for finding out what arguments certain MUAs actually use.
Don't attempt it if logging is disabled, or if listing variables or if
verifying/testing addresses or expansions. */

if ((log_extra_selector & LX_arguments) != 0 && really_exim
     && !list_options && !checking)
  {
  int i;
  uschar *p = big_buffer;
  Ustrcpy(p, "cwd=");
  (void)getcwd(CS p+4, big_buffer_size - 4);
  while (*p) p++;
  (void)string_format(p, big_buffer_size - (p - big_buffer), " %d args:", argc);
  while (*p) p++;
  for (i = 0; i < argc; i++)
    {
    int len = Ustrlen(argv[i]);
    uschar *printing;
    uschar *quote;
    if (p + len + 8 >= big_buffer + big_buffer_size)
      {
      Ustrcpy(p, " ...");
      log_write(0, LOG_MAIN, "%s", big_buffer);
      Ustrcpy(big_buffer, "...");
      p = big_buffer + 3;
      }
    printing = string_printing(argv[i]);
    if (printing[0] == 0) quote = US"\""; else
      {
      uschar *pp = printing;
      quote = US"";
      while (*pp != 0) if (isspace(*pp++)) { quote = US"\""; break; }
      }
    sprintf(CS p, " %s%.*s%s", quote, big_buffer_size - (p - big_buffer) - 4,
      printing, quote);
    while (*p) p++;
    }
  log_write(0, LOG_MAIN, "%s", big_buffer);
  }

/* Set the working directory to be the top-level spool directory. We don't rely
on this in the code, which always uses fully qualified names, but it's useful
for core dumps etc. Don't complain if it fails - the spool directory might not
be generally accessible and calls with the -C option (and others) have lost
privilege by now. */

if (Uchdir(spool_directory) != 0)
  {
  (void)directory_make(spool_directory, US"", SPOOL_DIRECTORY_MODE, TRUE);
  (void)Uchdir(spool_directory);
  }

/* Handle calls with the -bi option. This is a sendmail option to rebuild *the*
alias file. Exim doesn't have such a concept, but this call is screwed into
Sun's YP makefiles. Handle this by calling a configured script, as the real
user who called Exim. The -oA option can be used to pass an argument to the
script. */

if (bi_option)
  {
  fclose(config_file);
  if (bi_command != NULL)
    {
    int i = 0;
    uschar *argv[3];
    argv[i++] = bi_command;
    if (alias_arg != NULL) argv[i++] = alias_arg;
    argv[i++] = NULL;

    setgroups(group_count, group_list);
    exim_setugid(real_uid, real_gid, FALSE, US"running bi_command");

    DEBUG(D_exec) debug_printf("exec %.256s %.256s\n", argv[0],
      (argv[1] == NULL)? US"" : argv[1]);

    execv(CS argv[0], (char *const *)argv);
    fprintf(stderr, "exim: exec failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
    }
  else
    {
    DEBUG(D_any) debug_printf("-bi used but bi_command not set; exiting\n");
    exit(EXIT_SUCCESS);
    }
  }

/* If an action on specific messages is requested, or if a daemon or queue
runner is being started, we need to know if Exim was called by an admin user.
This is the case if the real user is root or exim, or if the real group is
exim, or if one of the supplementary groups is exim or a group listed in
admin_groups. We don't fail all message actions immediately if not admin_user,
since some actions can be performed by non-admin users. Instead, set admin_user
for later interrogation. */

if (real_uid == root_uid || real_uid == exim_uid || real_gid == exim_gid)
  admin_user = TRUE;
else
  {
  int i, j;

  for (i = 0; i < group_count; i++)
    {
    if (group_list[i] == exim_gid) admin_user = TRUE;
    else if (admin_groups != NULL)
      {
      for (j = 1; j <= (int)(admin_groups[0]); j++)
        if (admin_groups[j] == group_list[i])
          { admin_user = TRUE; break; }
      }
    if (admin_user) break;
    }
  }

/* Another group of privileged users are the trusted users. These are root,
exim, and any caller matching trusted_users or trusted_groups. Trusted callers
are permitted to specify sender_addresses with -f on the command line, and
other message parameters as well. */

if (real_uid == root_uid || real_uid == exim_uid)
  trusted_caller = TRUE;
else
  {
  int i, j;

  if (trusted_users != NULL)
    {
    for (i = 1; i <= (int)(trusted_users[0]); i++)
      if (trusted_users[i] == real_uid)
        { trusted_caller = TRUE; break; }
    }

  if (!trusted_caller && trusted_groups != NULL)
    {
    for (i = 1; i <= (int)(trusted_groups[0]); i++)
      {
      if (trusted_groups[i] == real_gid)
        trusted_caller = TRUE;
      else for (j = 0; j < group_count; j++)
        {
        if (trusted_groups[i] == group_list[j])
          { trusted_caller = TRUE; break; }
        }
      if (trusted_caller) break;
      }
    }
  }

if (trusted_caller) DEBUG(D_any) debug_printf("trusted user\n");
if (admin_user) DEBUG(D_any) debug_printf("admin user\n");

/* Only an admin user may start the daemon or force a queue run in the default
configuration, but the queue run restriction can be relaxed. Only an admin
user may request that a message be returned to its sender forthwith. Only an
admin user may specify a debug level greater than D_v (because it might show
passwords, etc. in lookup queries). Only an admin user may request a queue
count. */

if (!admin_user)
  {
  BOOL debugset = (debug_selector & ~D_v) != 0;
  if (deliver_give_up || daemon_listen ||
     (count_queue && queue_list_requires_admin) ||
     (list_queue && queue_list_requires_admin) ||
     (queue_interval >= 0 && prod_requires_admin) ||
     (debugset && !running_in_test_harness))
    {
    fprintf(stderr, "exim:%s permission denied\n", debugset? " debugging" : "");
    exit(EXIT_FAILURE);
    }
  }

/* If the real user is not root or the exim uid, the argument for passing
in an open TCP/IP connection for another message is not permitted, nor is
running with the -N option for any delivery action, unless this call to exim is
one that supplied an input message, or we are using a patched exim for
regression testing. */

if (real_uid != root_uid && real_uid != exim_uid &&
     (continue_hostname != NULL ||
       (dont_deliver &&
         (queue_interval >= 0 || daemon_listen || msg_action_arg > 0)
       )) && !running_in_test_harness)
  {
  fprintf(stderr, "exim: Permission denied\n");
  return EXIT_FAILURE;
  }

/* If the caller is not trusted, certain arguments are ignored when running for
real, but are permitted when checking things (-be, -bv, -bt, -bh, -bf). Note
that authority for performing certain actions on messages is tested in the
queue_action() function. */

if (!trusted_caller && !checking && filter_test == NULL)
  {
  sender_host_name = sender_host_address = interface_address =
    sender_ident = received_protocol = NULL;
  sender_host_port = interface_port = 0;
  sender_host_authenticated = authenticated_sender = authenticated_id = NULL;
  }

/* If a sender host address is set, extract the optional port number off the
end of it and check its syntax. Do the same thing for the interface address.
Exim exits if the syntax is bad. */

else
  {
  if (sender_host_address != NULL)
    sender_host_port = check_port(sender_host_address);
  if (interface_address != NULL)
    interface_port = check_port(interface_address);
  }

/* If an SMTP message is being received check to see if the standard input is a
TCP/IP socket. If it is, we assume that Exim was called from inetd if the
caller is root or the Exim user, or if the port is a privileged one. Otherwise,
barf. */

if (smtp_input)
  {
  union sockaddr_46 inetd_sock;
  SOCKLEN_T size = sizeof(inetd_sock);
  if (getpeername(0, (struct sockaddr *)(&inetd_sock), &size) == 0)
    {
    int family = ((struct sockaddr *)(&inetd_sock))->sa_family;
    if (family == AF_INET || family == AF_INET6)
      {
      union sockaddr_46 interface_sock;
      size = sizeof(interface_sock);

      if (getsockname(0, (struct sockaddr *)(&interface_sock), &size) == 0)
        interface_address = host_ntoa(-1, &interface_sock, NULL,
          &interface_port);

      if (real_uid == root_uid || real_uid == exim_uid || interface_port < 1024)
        {
        is_inetd = TRUE;
        sender_host_address = host_ntoa(-1, (struct sockaddr *)(&inetd_sock),
          NULL, &sender_host_port);
        }
      else
        {
        fprintf(stderr,
          "exim: Permission denied (unprivileged user, unprivileged port)\n");
        return EXIT_FAILURE;
        }
      }
    }
  }

/* If the load average is going to be needed while receiving a message, get it
now for those OS that require the first call to os_getloadavg() to be done as
root. There will be further calls later for each message received. */

#ifdef LOAD_AVG_NEEDS_ROOT
if (receiving_message &&
      (queue_only_load >= 0 ||
        (is_inetd && smtp_load_reserve >= 0)
      ))
  {
  load_average = os_getloadavg();
  }
#endif

/* The queue_only configuration option can be overridden by -odx on the command
line, except that if queue_only_override is false, queue_only cannot be unset
from the command line. */

if (queue_only_set && (queue_only_override || arg_queue_only))
  queue_only = arg_queue_only;

/* The receive_timeout and smtp_receive_timeout options can be overridden by
-or and -os. */

if (arg_receive_timeout >= 0) receive_timeout = arg_receive_timeout;
if (arg_smtp_receive_timeout >= 0)
  smtp_receive_timeout = arg_smtp_receive_timeout;

/* If Exim was started with root privilege, unless we have already removed the
root privilege above as a result of -C, -D, -be, -bf or -bF, remove it now
except when starting the daemon or doing some kind of delivery or address
testing (-bt). These are the only cases when root need to be retained. We run
as exim for -bv and -bh. However, if deliver_drop_privilege is set, root is
retained only for starting the daemon. */

if (!unprivileged &&                      /* originally had root AND */
    !removed_privilege &&                 /* still got root AND      */
    !daemon_listen &&                     /* not starting the daemon */
    queue_interval <= 0 &&                /* (either kind of daemon) */
      (                                   /*    AND EITHER           */
      deliver_drop_privilege ||           /* requested unprivileged  */
        (                                 /*       OR                */
        queue_interval < 0 &&             /* not running the queue   */
        (msg_action_arg < 0 ||            /*       and               */
          msg_action != MSG_DELIVER) &&   /* not delivering and      */
        (!checking || !address_test_mode) /* not address checking    */
        )
      ))
  {
  exim_setugid(exim_uid, exim_gid, FALSE, US"privilege not needed");
  }

/* When we are retaining a privileged uid, we still change to the exim gid. */

else setgid(exim_gid);

/* Handle a request to list the delivery queue */

if (list_queue)
  {
  set_process_info("listing the queue");
  queue_list(list_queue_option, argv + recipients_arg, argc - recipients_arg);
  exit(EXIT_SUCCESS);
  }

/* Handle a request to count the delivery queue */

if (count_queue)
  {
  set_process_info("counting the queue");
  queue_count();
  exit(EXIT_SUCCESS);
  }

/* Handle actions on specific messages, except for the force delivery action,
which is done below. Some actions take a whole list of message ids, which
are known to continue up to the end of the arguments. Others take a single
message id and then operate on the recipients list. */

if (msg_action_arg > 0 && msg_action != MSG_DELIVER)
  {
  int yield = EXIT_SUCCESS;
  set_process_info("acting on specified messages");

  if (!one_msg_action)
    {
    for (i = msg_action_arg; i < argc; i++)
      if (!queue_action(argv[i], msg_action, NULL, 0, 0))
        yield = EXIT_FAILURE;
    }

  else if (!queue_action(argv[msg_action_arg], msg_action, argv, argc,
    recipients_arg)) yield = EXIT_FAILURE;
  exit(yield);
  }

/* All the modes below here require the remaining configuration sections
to be read, except that we can skip over the ACL setting when delivering
specific messages, or doing a queue run. (For various testing cases we could
skip too, but as they are rare, it doesn't really matter.) The argument is TRUE
for skipping. */

readconf_rest(msg_action_arg > 0 || (queue_interval == 0 && !daemon_listen));

/* The configuration data will have been read into POOL_PERM because we won'd
ever want to reset back past it. Change the current pool to POOL_MAIN. In fact,
this is just a bit of pedantic tidiness. It wouldn't really matter if the
configuration were read into POOL_MAIN, because we don't do any resets till
later on. However, it seems right, and it does ensure that both pools get used.
*/

store_pool = POOL_MAIN;

/* Handle the -brt option. This is for checking out retry configurations.
The next three arguments are a domain name or a complete address, and
optionally two error numbers. All it does is to call the function that
scans the retry configuration data. */

if (test_retry_arg >= 0)
  {
  retry_config *yield;
  int basic_errno = 0;
  int more_errno = 0;
  uschar *s1, *s2;

  if (test_retry_arg >= argc)
    {
    printf("-brt needs a domain or address argument\n");
    exim_exit(EXIT_FAILURE);
    }
  s1 = argv[test_retry_arg++];
  s2 = NULL;

  /* If the first argument contains no @ and no . it might be a local user
  or it might be a single-component name. Treat as a domain. */

  if (Ustrchr(s1, '@') == NULL && Ustrchr(s1, '.') == NULL)
    {
    printf("Warning: \"%s\" contains no '@' and no '.' characters. It is "
      "being \ntreated as a one-component domain, not as a local part.\n\n",
      s1);
    }

  /* There may be an optional second domain arg. */

  if (test_retry_arg < argc && Ustrchr(argv[test_retry_arg], '.') != NULL)
    s2 = argv[test_retry_arg++];

  /* The final arg is an error name */

  if (test_retry_arg < argc)
    {
    uschar *ss = argv[test_retry_arg];
    uschar *error =
      readconf_retry_error(ss, ss + Ustrlen(ss), &basic_errno, &more_errno);
    if (error != NULL)
      {
      printf("%s\n", CS error);
      return EXIT_FAILURE;
      }
    }

  yield = retry_find_config(s1, s2, basic_errno, more_errno);
  if (yield == NULL) printf("No retry information found\n"); else
    {
    retry_rule *r;
    more_errno = yield->more_errno;
    printf("Retry rule: %s  ", yield->pattern);

    if (yield->basic_errno == ERRNO_EXIMQUOTA)
      {
      printf("quota%s%s  ",
        (more_errno > 0)? "_" : "",
        (more_errno > 0)? readconf_printtime(more_errno) : US"");
      }
    else if (yield->basic_errno == ECONNREFUSED)
      {
      printf("refused%s%s  ",
        (more_errno > 0)? "_" : "",
        (more_errno == 'M')? "MX" :
        (more_errno == 'A')? "A" : "");
      }
    else if (yield->basic_errno == ETIMEDOUT)
      {
      printf("timeout");
      if ((more_errno & RTEF_CTOUT) != 0) printf("_connect");
      more_errno &= 255;
      if (more_errno != 0) printf("_%s",
        (more_errno == 'M')? "MX" : "A");
      printf("  ");
      }
    else if (yield->basic_errno == ERRNO_AUTHFAIL)
      printf("auth_failed  ");
    else printf("*  ");

    for (r = yield->rules; r != NULL; r = r->next)
      {
      printf("%c,%s", r->rule, readconf_printtime(r->timeout)); /* Do not */
      printf(",%s", readconf_printtime(r->p1));                 /* amalgamate */
      if (r->rule == 'G')
        {
        int x = r->p2;
        int f = x % 1000;
        int d = 100;
        printf(",%d.", x/1000);
        do
          {
          printf("%d", f/d);
          f %= d;
          d /= 10;
          }
        while (f != 0);
        }
      printf("; ");
      }

    printf("\n");
    }
  exim_exit(EXIT_SUCCESS);
  }

/* Handle a request to list one or more configuration options */

if (list_options)
  {
  set_process_info("listing variables");
  if (recipients_arg >= argc) readconf_print(US"all", NULL);
    else for (i = recipients_arg; i < argc; i++)
      {
      if (i < argc - 1 &&
          (Ustrcmp(argv[i], "router") == 0 ||
           Ustrcmp(argv[i], "transport") == 0 ||
           Ustrcmp(argv[i], "authenticator") == 0))
        {
        readconf_print(argv[i+1], argv[i]);
        i++;
        }
      else readconf_print(argv[i], NULL);
      }
  exim_exit(EXIT_SUCCESS);
  }


/* Handle a request to deliver one or more messages that are already on the
queue. Values of msg_action other than MSG_DELIVER are dealt with above. This
is typically used for a small number when prodding by hand (when the option
forced_delivery will be set) or when re-execing to regain root privilege.
Each message delivery must happen in a separate process, so we fork a process
for each one, and run them sequentially so that debugging output doesn't get
intertwined, and to avoid spawning too many processes if a long list is given.
However, don't fork for the last one; this saves a process in the common case
when Exim is called to deliver just one message. */

if (msg_action_arg > 0)
  {
  if (prod_requires_admin && !admin_user)
    {
    fprintf(stderr, "exim: Permission denied\n");
    exim_exit(EXIT_FAILURE);
    }
  set_process_info("delivering specified messages");
  if (deliver_give_up) forced_delivery = deliver_force_thaw = TRUE;
  for (i = msg_action_arg; i < argc; i++)
    {
    int status;
    pid_t pid;
    if (i == argc - 1)
      (void)deliver_message(argv[i], forced_delivery, deliver_give_up);
    else if ((pid = fork()) == 0)
      {
      (void)deliver_message(argv[i], forced_delivery, deliver_give_up);
      _exit(EXIT_SUCCESS);
      }
    else if (pid < 0)
      {
      fprintf(stderr, "failed to fork delivery process for %s: %s\n", argv[i],
        strerror(errno));
      exim_exit(EXIT_FAILURE);
      }
    else wait(&status);
    }
  exim_exit(EXIT_SUCCESS);
  }


/* If only a single queue run is requested, without SMTP listening, we can just
turn into a queue runner, with an optional starting message id. */

if (queue_interval == 0 && !daemon_listen)
  {
  DEBUG(D_queue_run) debug_printf("Single queue run%s%s%s%s\n",
    (start_queue_run_id == NULL)? US"" : US" starting at ",
    (start_queue_run_id == NULL)? US"" : start_queue_run_id,
    (stop_queue_run_id == NULL)?  US"" : US" stopping at ",
    (stop_queue_run_id == NULL)?  US"" : stop_queue_run_id);
  set_process_info("running the queue (single queue run)");
  queue_run(start_queue_run_id, stop_queue_run_id, FALSE);
  exim_exit(EXIT_SUCCESS);
  }


/* Find the login name of the real user running this process. This is always
needed when receiving a message, because it is written into the spool file. It
may also be used to construct a from: or a sender: header, and in this case we
need the user's full name as well, so save a copy of it, checked for RFC822
syntax and munged if necessary, if it hasn't previously been set by the -F
argument. We try to get the passwd entry more than once, in case NIS or other
delays are in evidence. Save the home directory for use in filter testing
(only). */

for (i = 1; i <= 10; i++)
  {
  if ((pw = getpwuid(real_uid)) != NULL)
    {
    originator_login = string_copy(US pw->pw_name);
    originator_home = string_copy(US pw->pw_dir);

    /* If user name has not been set by -F, set it from the passwd entry
    unless -f has been used to set the sender address by a trusted user. */

    if (originator_name == NULL)
      {
      if (sender_address == NULL ||
           (!trusted_caller && filter_test == NULL))
        {
        uschar *name = US pw->pw_gecos;
        uschar *amp = Ustrchr(name, '&');
        uschar buffer[256];

        /* Most Unix specify that a '&' character in the gecos field is
        replaced by a copy of the login name, and some even specify that
        the first character should be upper cased, so that's what we do. */

        if (amp != NULL)
          {
          int loffset;
          string_format(buffer, sizeof(buffer), "%.*s%n%s%s",
            amp - name, name, &loffset, originator_login, amp + 1);
          buffer[loffset] = toupper(buffer[loffset]);
          name = buffer;
          }

        /* If a pattern for matching the gecos field was supplied, apply
        it and then expand the name string. */

        if (gecos_pattern != NULL && gecos_name != NULL)
          {
          const pcre *re;
          re = regex_must_compile(gecos_pattern, FALSE, TRUE); /* Use malloc */

          if (regex_match_and_setup(re, name, 0, -1))
            {
            uschar *new_name = expand_string(gecos_name);
            expand_nmax = -1;
            if (new_name != NULL)
              {
              DEBUG(D_receive) debug_printf("user name \"%s\" extracted from "
                "gecos field \"%s\"\n", new_name, name);
              name = new_name;
              }
            else DEBUG(D_receive) debug_printf("failed to expand gecos_name string "
              "\"%s\": %s\n", gecos_name, expand_string_message);
            }
          else DEBUG(D_receive) debug_printf("gecos_pattern \"%s\" did not match "
            "gecos field \"%s\"\n", gecos_pattern, name);
          store_free((void *)re);
          }
        originator_name = string_copy(name);
        }

      /* A trusted caller has used -f but not -F */

      else originator_name = US"";
      }

    /* Break the retry loop */

    break;
    }
  sleep(1);
  }

/* If we cannot get a user login, log the incident and give up, unless the
configuration specifies something to use. */

if (originator_login == NULL)
  {
  if (unknown_login != NULL)
    {
    originator_login = expand_string(unknown_login);
    if (originator_name == NULL && unknown_username != NULL)
      originator_name = expand_string(unknown_username);
    if (originator_name == NULL) originator_name = US"";
    }
  if (originator_login == NULL)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Failed to get user name for uid %d",
      (int)real_uid);
  }

/* Ensure that the user name is in a suitable form for use as a "phrase" in an
RFC822 address.*/

originator_name = string_copy(parse_fix_phrase(originator_name,
  Ustrlen(originator_name), big_buffer, big_buffer_size));

/* If a message is created by this call of Exim, the uid/gid of its originator
are those of the caller. These values are overridden if an existing message is
read in from the spool. */

originator_uid = real_uid;
originator_gid = real_gid;

DEBUG(D_receive) debug_printf("originator: uid=%d gid=%d login=%s name=%s\n",
  (int)originator_uid, (int)originator_gid, originator_login, originator_name);

/* Run in daemon and/or queue-running mode. The function daemon_go() never
returns. We leave this till here so that the originator_ fields are available
for incoming messages via the daemon. */

if (daemon_listen || queue_interval > 0) daemon_go();

/* If the sender ident has not been set (by a trusted caller) set it to
the caller. This will get overwritten below for an inetd call. If a trusted
caller has set it empty, unset it. */

if (sender_ident == NULL) sender_ident = originator_login;
  else if (sender_ident[0] == 0) sender_ident = NULL;

/* Handle the -brw option, which is for checking out rewriting rules. Cause log
writes (on errors) to go to stderr instead. Can't do this earlier, as want the
originator_* variables set. */

if (test_rewrite_arg >= 0)
  {
  really_exim = FALSE;
  if (test_rewrite_arg >= argc)
    {
    printf("-brw needs an address argument\n");
    exim_exit(EXIT_FAILURE);
    }
  rewrite_test(argv[test_rewrite_arg]);
  exim_exit(EXIT_SUCCESS);
  }

/* A locally-supplied message is considered to be coming from a local user
unless a trusted caller supplies a sender address with -f, or is passing in the
message via SMTP (inetd invocation or otherwise). */

if ((sender_address == NULL && !smtp_input) ||
    (!trusted_caller && filter_test == NULL))
  {
  sender_local = TRUE;

  /* A trusted caller can supply authenticated_sender and authenticated_id
  via -oMas and -oMai and if so, they will already be set. */

  if (authenticated_sender == NULL)
    authenticated_sender = string_sprintf("%s@%s", originator_login,
      qualify_domain_sender);
  if (authenticated_id == NULL) authenticated_id = originator_login;
  }

/* Trusted callers are always permitted to specify the sender address.
Untrusted callers may specify it if it matches untrusted_set_sender, or if what
is specified is the empty address. However, if a trusted caller does not
specify a sender address for SMTP input, we leave sender_address unset. This
causes the MAIL commands to be honoured. */

if ((!smtp_input && sender_address == NULL) ||
    !receive_check_set_sender(sender_address))
  {
  /* Either the caller is not permitted to set a general sender, or this is
  non-SMTP input and the trusted caller has not set a sender. If there is no
  sender, or if a sender other than <> is set, override with the originator's
  login (which will get qualified below), except when checking things. */

  if (sender_address == NULL             /* No sender_address set */
       ||                                /*         OR            */
       (sender_address[0] != 0 &&        /* Non-empty sender address, AND */
       !checking &&                      /* Not running tests, AND */
       filter_test == NULL))             /* Not testing a filter */
    {
    sender_address = originator_login;
    sender_address_forced = FALSE;
    sender_address_domain = 0;
    }
  }

/* Remember whether an untrusted caller set the sender address */

sender_set_untrusted = sender_address != originator_login && !trusted_caller;

/* Ensure that the sender address is fully qualified unless it is the empty
address, which indicates an error message, or doesn't exist (root caller, smtp
interface, no -f argument). */

if (sender_address != NULL && sender_address[0] != 0 &&
    sender_address_domain == 0)
  sender_address = string_sprintf("%s@%s", sender_address,
    qualify_domain_sender);

DEBUG(D_receive) debug_printf("sender address = %s\n", sender_address);

/* Handle a request to verify a list of addresses, or test them for delivery.
This must follow the setting of the sender address, since routers can be
predicated upon the sender. If no arguments are given, read addresses from
stdin. Set debug_level to at least D_v to get full output for address testing.
*/

if (verify_address_mode || address_test_mode)
  {
  int exit_value = 0;
  int flags = vopt_qualify;

  if (verify_address_mode)
    {
    if (!verify_as_sender) flags |= vopt_is_recipient;
    DEBUG(D_verify) debug_print_ids(US"Verifying:");
    }

  else
    {
    flags |= vopt_is_recipient;
    debug_selector |= D_v;
    debug_file = stderr;
    debug_fd = fileno(debug_file);
    DEBUG(D_verify) debug_print_ids(US"Address testing:");
    }

  if (recipients_arg < argc)
    {
    while (recipients_arg < argc)
      {
      uschar *s = argv[recipients_arg++];
      while (*s != 0)
        {
        BOOL finished = FALSE;
        uschar *ss = parse_find_address_end(s, FALSE);
        if (*ss == ',') *ss = 0; else finished = TRUE;
        test_address(s, flags, &exit_value);
        s = ss;
        if (!finished)
          while (*(++s) != 0 && (*s == ',' || isspace(*s)));
        }
      }
    }

  else
    {
    for (;;)
      {
      int len;
      uschar buffer[256];
      printf("> ");
      if (Ufgets(buffer, 256, stdin) == NULL) break;
      len = Ustrlen(buffer);
      while (len > 0 && isspace(buffer[len-1])) len--;
      buffer[len] = 0;
      test_address(buffer, flags, &exit_value);
      }
    printf("\n");
    }
  route_tidyup();
  exim_exit(exit_value);
  }

/* Handle expansion checking */

if (expansion_test)
  {
  if (recipients_arg < argc)
    {
    while (recipients_arg < argc)
      {
      uschar *s = argv[recipients_arg++];
      uschar *ss = expand_string(s);
      if (ss == NULL)
        printf ("Failed: %s\n", expand_string_message);
      else printf("%s\n", CS ss);
      }
    }

  /* Read stdin, allowing for continuations */

  else
    {
    uschar buffer[1024];

    for (;;)
      {
      int i;
      int size = 0;
      int ptr = 0;
      uschar *source = NULL;
      uschar *ss;

      printf("> ");
      for (i = 0;; i++)
        {
        uschar *p = buffer;
        if (Ufgets(buffer, sizeof(buffer), stdin) == NULL) break;
        ss = buffer + (int)Ustrlen(buffer);
        while (ss > buffer && isspace(ss[-1])) ss--;
        if (i > 0)
          {
          while (p < ss && isspace(*p)) p++;   /* leading space after cont */
          }
        source = string_cat(source, &size, &ptr, p, ss - p);
        if (ss == buffer || ss[-1] != '\\')
          {
          source[ptr] = 0;
          break;
          }
        source[--ptr] = 0;
        }
      if (source == NULL)
        {
        printf("\n");
        break;
        }

      ss = expand_string(source);
      if (ss == NULL)
        printf ("Failed: %s\n", expand_string_message);
      else printf("%s\n", CS ss);
      }
    }

  exim_exit(EXIT_SUCCESS);
  }


/* The active host name is normally the primary host name, but it can be varied
for hosts that want to play several parts at once. We need to ensure that it is
set for host checking, and for receiving messages. */

smtp_active_hostname = primary_hostname;
if (raw_active_hostname != NULL)
  {
  uschar *nah = expand_string(raw_active_hostname);
  if (nah == NULL)
    {
    if (!expand_string_forcedfail)
      log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to expand \"%s\" "
        "(smtp_active_hostname): %s", raw_active_hostname,
        expand_string_message);
    }
  else if (nah[0] != 0) smtp_active_hostname = nah;
  }

/* Handle host checking: this facility mocks up an incoming SMTP call from a
given IP address so that the blocking and relay configuration can be tested. An
RFC 1413 call is made only if we are running in the test harness and an
incoming interface and both ports are specified, because there is no TCP/IP
call to find the ident for. */

if (host_checking)
  {
  sender_ident = NULL;
  if (running_in_test_harness && sender_host_port != 0 &&
      interface_address != NULL && interface_port != 0)
    verify_get_ident(1413);

  host_build_sender_fullhost();
  smtp_input = TRUE;
  smtp_in = stdin;
  smtp_out = stdout;
  sender_local = FALSE;
  sender_host_notsocket = TRUE;
  debug_file = stderr;
  debug_fd = fileno(debug_file);
  fprintf(stdout, "\n**** SMTP testing session as if from host %s\n"
    "**** but without any ident (RFC 1413) callback.\n"
    "**** This is not for real!\n\n",
      sender_host_address);

  log_write(L_smtp_connection, LOG_MAIN, "%s", smtp_get_connection_info());
  if (smtp_start_session())
    {
    reset_point = store_get(0);
    for (;;)
      {
      store_reset(reset_point);
      if (smtp_setup_msg() <= 0) break;
      if (!receive_msg(FALSE)) break;
      }
    }
  exim_exit(EXIT_SUCCESS);
  }


/* Arrange for message reception if recipients or SMTP were specified;
otherwise complain unless a version print (-bV) happened or this is a filter
verification test. In the former case, show the configuration file name. */

if (recipients_arg >= argc && !extract_recipients && !smtp_input)
  {
  if (version_printed)
    {
    printf("Configuration file is %s\n", config_main_filename);
    return EXIT_SUCCESS;
    }
  if (filter_test == NULL)
    {
    fprintf(stderr,
"Exim is a Mail Transfer Agent. It is normally called by Mail User Agents,\n"
"not directly from a shell command line. Options and/or arguments control\n"
"what it does when called. For a list of options, see the Exim documentation.\n");
    return EXIT_FAILURE;
    }
  }


/* Prepare to accept one or more new messages on the standard input. When a
message has been read, its id is returned in message_id[]. If doing immediate
delivery, we fork a delivery process for each received message, except for the
last one, where we can save a process switch.

It is only in non-smtp mode that error_handling is allowed to be changed from
its default of ERRORS_SENDER by argument. (Idle thought: are any of the
sendmail error modes other than -oem ever actually used? Later: yes.) */

if (!smtp_input) error_handling = arg_error_handling;

/* If this is an inetd call, ensure that stderr is closed to prevent panic
logging being sent down the socket and make an identd call to get the
sender_ident. */

else if (is_inetd)
  {
  fclose(stderr);
  exim_nullstd();                       /* Re-open to /dev/null */
  verify_get_ident(IDENT_PORT);
  host_build_sender_fullhost();
  set_process_info("handling incoming connection from %s via inetd",
    sender_fullhost);
  }

/* If the sender host address has been set, build sender_fullhost if it hasn't
already been done (which it will have been for inetd). This caters for the
case when it is forced by -oMa. However, we must flag that is isn't a socket,
so that the test for IP options is skipped for -bs input. */

if (sender_host_address != NULL && sender_fullhost == NULL)
  {
  host_build_sender_fullhost();
  set_process_info("handling incoming connection from %s via -oMa",
    sender_fullhost);
  sender_host_notsocket = TRUE;
  }

/* Otherwise, set the sender host as unknown except for inetd calls. This
prevents host checking in the case of -bs not from inetd and also for -bS. */

else if (!is_inetd) sender_host_unknown = TRUE;

/* If stdout does not exist, then dup stdin to stdout. This can happen
if exim is started from inetd. In this case fd 0 will be set to the socket,
but fd 1 will not be set. This also happens for passed SMTP channels. */

if (fstat(1, &statbuf) < 0) dup2(0, 1);

/* Set up the incoming protocol name and the state of the program. Root
is allowed to force received protocol via the -oMr option above, and if we are
in a non-local SMTP state it means we have come via inetd and the process info
has already been set up. We don't set received_protocol here for smtp input,
as it varies according to batch/HELO/EHLO. */

if (smtp_input)
  {
  if (sender_local) set_process_info("accepting a local SMTP message from <%s>",
    sender_address);
  }
else
  {
  if (received_protocol == NULL)
    received_protocol = string_sprintf("local%s", called_as);
  set_process_info("accepting a local non-SMTP message from <%s>",
    sender_address);
  }

/* Initialize the local_queue-only flag */

queue_check_only();
local_queue_only = queue_only;

/* For non-SMTP and for batched SMTP input, check that there is enough space on
the spool if so configured. On failure, we must not attempt to send an error
message! (For interactive SMTP, the check happens at MAIL FROM and an SMTP
error code is given.) */

if ((!smtp_input || smtp_batched_input) && !receive_check_fs(0))
  {
  fprintf(stderr, "exim: insufficient disk space\n");
  return EXIT_FAILURE;
  }

/* If this is smtp input of any kind, handle the start of the SMTP
session. */

if (smtp_input)
  {
  smtp_in = stdin;
  smtp_out = stdout;
  log_write(L_smtp_connection, LOG_MAIN, "%s", smtp_get_connection_info());
  if (!smtp_start_session())
    {
    mac_smtp_fflush();
    exim_exit(EXIT_SUCCESS);
    }
  }

/* Otherwise, set up the input size limit here */

else
  {
  thismessage_size_limit = expand_string_integer(message_size_limit);
  if (thismessage_size_limit < 0)
    {
    if (thismessage_size_limit == -1)
      log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to expand "
        "message_size_limit: %s", expand_string_message);
    else
      log_write(0, LOG_MAIN|LOG_PANIC_DIE, "invalid value for "
        "message_size_limit: %s", expand_string_message);
    }
  }

/* Loop for several messages when reading SMTP input. If we fork any child
processes, we don't want to wait for them unless synchronous delivery is
requested, so set SIGCHLD to SIG_IGN in that case. This is not necessarily the
same as SIG_DFL, despite the fact that documentation often lists the default as
"ignore". This is a confusing area. This is what I know:

At least on some systems (e.g. Solaris), just setting SIG_IGN causes child
processes that complete simply to go away without ever becoming defunct. You
can't then wait for them - but we don't want to wait for them in the
non-synchronous delivery case. However, this behaviour of SIG_IGN doesn't
happen for all OS (e.g. *BSD is different).

But that's not the end of the story. Some (many? all?) systems have the
SA_NOCLDWAIT option for sigaction(). This requests the behaviour that Solaris
has by default, so it seems that the difference is merely one of default
(compare restarting vs non-restarting signals).

To cover all cases, Exim sets SIG_IGN with SA_NOCLDWAIT here if it can. If not,
it just sets SIG_IGN. To be on the safe side it also calls waitpid() at the end
of the loop below. Paranoia rules.

February 2003: That's *still* not the end of the story. There are now versions
of Linux (where SIG_IGN does work) that are picky. If, having set SIG_IGN, a
process then calls waitpid(), a grumble is written to the system log, because
this is logically inconsistent. In other words, it doesn't like the paranoia.
As a consequenc of this, the waitpid() below is now excluded if we are sure
that SIG_IGN works. */

if (!synchronous_delivery)
  {
  #ifdef SA_NOCLDWAIT
  struct sigaction act;
  act.sa_handler = SIG_IGN;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = SA_NOCLDWAIT;
  sigaction(SIGCHLD, &act, NULL);
  #else
  signal(SIGCHLD, SIG_IGN);
  #endif
  }

/* Save the current store pool point, for resetting at the start of
each message, and save the real sender address, if any. */

reset_point = store_get(0);
real_sender_address = sender_address;

/* Loop to receive messages; receive_msg() returns TRUE if there are more
messages to be read (SMTP input), or FALSE otherwise (not SMTP, or SMTP channel
collapsed). */

while (more)
  {
  store_reset(reset_point);
  message_id[0] = 0;

  /* In the SMTP case, we have to handle the initial SMTP input and build the
  recipients list, before calling receive_msg() to read the message proper.
  Whatever sender address is actually given in the SMTP transaction is
  actually ignored for local senders - we use the actual sender, which is
  normally either the underlying user running this process or a -f argument
  provided by a trusted caller. It is saved in real_sender_address.

  However, if this value is NULL, we are dealing with a trusted caller when
  -f was not used; in this case, the SMTP sender is allowed to stand.

  Also, if untrusted_set_sender is set, we permit sender addresses that match
  anything in its list.

  The variable raw_sender_address holds the sender address before rewriting. */

  if (smtp_input)
    {
    int rc;
    if ((rc = smtp_setup_msg()) > 0)
      {
      if (real_sender_address != NULL &&
          !receive_check_set_sender(sender_address))
        {
        sender_address = raw_sender = real_sender_address;
        sender_address_unrewritten = NULL;
        }
      more = receive_msg(extract_recipients);
      if (message_id[0] == 0)
        {
        if (more) continue;
        exim_exit(EXIT_FAILURE);
        }
      }
    else exim_exit((rc == 0)? EXIT_SUCCESS : EXIT_FAILURE);
    }

  /* In the non-SMTP case, we have all the information from the command
  line, but must process it in case it is in the more general RFC822
  format, and in any case, to detect syntax errors. Also, it appears that
  the use of comma-separated lists as single arguments is common, so we
  had better support them. */

  else
    {
    int i;
    int rcount = 0;
    int count = argc - recipients_arg;
    uschar **list = argv + recipients_arg;

    /* Save before any rewriting */

    raw_sender = string_copy(sender_address);

    /* Loop for each argument */

    for (i = 0; i < count; i++)
      {
      int start, end, domain;
      uschar *errmess;
      uschar *s = list[i];

      /* Loop for each comma-separated address */

      while (*s != 0)
        {
        BOOL finished = FALSE;
        uschar *recipient;
        uschar *ss = parse_find_address_end(s, FALSE);

        if (*ss == ',') *ss = 0; else finished = TRUE;

        /* Check max recipients - if -t was used, these aren't recipients */

        if (recipients_max > 0 && ++rcount > recipients_max &&
            !extract_recipients)
          {
          if (error_handling == ERRORS_STDERR)
            {
            fprintf(stderr, "exim: too many recipients\n");
            exim_exit(EXIT_FAILURE);
            }
          else
            {
            return
              moan_to_sender(ERRMESS_TOOMANYRECIP, NULL, NULL, stdin, TRUE)?
                errors_sender_rc : EXIT_FAILURE;
            }
          }

        recipient =
          parse_extract_address(s, &errmess, &start, &end, &domain, FALSE);

        if (domain == 0 && !allow_unqualified_recipient)
          {
          recipient = NULL;
          errmess = US"unqualified recipient address not allowed";
          }

        if (recipient == NULL)
          {
          if (error_handling == ERRORS_STDERR)
            {
            fprintf(stderr, "exim: bad recipient address \"%s\": %s\n",
              string_printing(list[i]), errmess);
            exim_exit(EXIT_FAILURE);
            }
          else
            {
            error_block eblock;
            eblock.next = NULL;
            eblock.text1 = string_printing(list[i]);
            eblock.text2 = errmess;
            return
              moan_to_sender(ERRMESS_BADARGADDRESS, &eblock, NULL, stdin, TRUE)?
                errors_sender_rc : EXIT_FAILURE;
            }
          }

        receive_add_recipient(recipient, -1);
        s = ss;
        if (!finished)
          while (*(++s) != 0 && (*s == ',' || isspace(*s)));
        }
      }

    /* Show the recipients when debugging */

    DEBUG(D_receive)
      {
      int i;
      if (sender_address != NULL) debug_printf("Sender: %s\n", sender_address);
      if (recipients_list != NULL)
        {
        debug_printf("Recipients:\n");
        for (i = 0; i < recipients_count; i++)
          debug_printf("  %s\n", recipients_list[i].address);
        }
      }

    /* Read the data for the message. If filter_test is true, this will
    just read the headers for the message, and not write anything onto
    the spool. */

    message_ended = END_NOTENDED;
    more = receive_msg(extract_recipients);

    /* more is always FALSE here (not SMTP message) when reading a message
    for real; when reading the headers of a message for filter testing,
    it is TRUE if the headers were terminated by '.' and FALSE otherwise. */

    if (message_id[0] == 0) exim_exit(EXIT_FAILURE);
    }  /* Non-SMTP message reception */

  /* If this is a filter testing run, there are headers in store, but
  no message on the spool. Run the filtering code in testing mode, setting
  the domain to the qualify domain and the local part to the current user,
  unless they have been set by options. The prefix and suffix are left unset
  unless specified. The the return path is set to to the sender unless it has
  already been set from a return-path header in the message. */

  if (filter_test != NULL)
    {
    deliver_domain = (ftest_domain != NULL)?
      ftest_domain : qualify_domain_recipient;
    deliver_domain_orig = deliver_domain;
    deliver_localpart = (ftest_localpart != NULL)?
      ftest_localpart : originator_login;
    deliver_localpart_orig = deliver_localpart;
    deliver_localpart_prefix = ftest_prefix;
    deliver_localpart_suffix = ftest_suffix;
    deliver_home = originator_home;

    if (return_path == NULL)
      {
      printf("Return-path copied from sender\n");
      return_path = string_copy(sender_address);
      }
    else
      {
      printf("Return-path = %s\n", (return_path[0] == 0)? US"<>" : return_path);
      }
    printf("Sender      = %s\n", (sender_address[0] == 0)? US"<>" : sender_address);

    receive_add_recipient(
      string_sprintf("%s%s%s@%s",
        (ftest_prefix == NULL)? US"" : ftest_prefix,
        deliver_localpart,
        (ftest_suffix == NULL)? US"" : ftest_suffix,
        deliver_domain), -1);

    printf("Recipient   = %s\n", recipients_list[0].address);
    if (ftest_prefix != NULL) printf("Prefix    = %s\n", ftest_prefix);
    if (ftest_suffix != NULL) printf("Suffix    = %s\n", ftest_suffix);

    chdir("/");   /* Get away from wherever the user is running this from */
    exim_exit(filter_runtest(filter_fd, ftest_system, more)?
      EXIT_SUCCESS : EXIT_FAILURE);
    }

  /* Else act on the result of message reception. We should not get here unless
  message_id[0] is non-zero. If queue_only is set, local_queue_only will be
  TRUE. If it is not, check on the number of messages received in this
  connection. If that's OK and queue_only_load is set, check that the load
  average is below it. If it is not, set local_queue_only TRUE. Note that it
  then remains this way for any subsequent messages on the same SMTP connection.
  This is a deliberate choice; even though the load average may fall, it
  doesn't seem right to deliver later messages on the same call when not
  delivering earlier ones. */

  if (!local_queue_only)
    {
    if (smtp_accept_queue_per_connection > 0 &&
        receive_messagecount > smtp_accept_queue_per_connection)
      {
      local_queue_only = TRUE;
      queue_only_reason = 2;
      }
    else if (queue_only_load >= 0)
      {
      local_queue_only = (load_average = os_getloadavg()) > queue_only_load;
      if (local_queue_only) queue_only_reason = 3;
      }
    }

  /* Log the queueing here, when it will get a message id attached, but
  not if queue_only is set (case 0). Case 1 doesn't happen here (too many
  connections). */

  if (local_queue_only) switch(queue_only_reason)
    {
    case 2:
    log_write(L_delay_delivery,
              LOG_MAIN, "no immediate delivery: more than %d messages "
      "received in one connection", smtp_accept_queue_per_connection);
    break;

    case 3:
    log_write(L_delay_delivery,
              LOG_MAIN, "no immediate delivery: load average %.2f",
              (double)load_average/1000.0);
    break;
    }

  /* Else do the delivery unless the ACL or local_scan() called for queue only
  or froze the message. Always deliver in a separate process. A fork failure is
  not a disaster, as the delivery will eventually happen on a subsequent queue
  run. */

  else if (!queue_only_policy && !deliver_freeze)
    {
    pid_t pid;
    if ((pid = fork()) == 0)
      {
      close_unwanted();      /* Close unwanted file descriptors and TLS */
      exim_nullstd();        /* Ensure std{in,out,err} exist */

      /* Re-exec Exim if we need to regain privilege */

      if (geteuid() != root_uid && !deliver_drop_privilege && !unprivileged)
        {
        (void)child_exec_exim(CEE_EXEC_EXIT, FALSE, NULL, FALSE, 2, US"-Mc",
          message_id);
        /* Control does not return here. */
        }

      /* No need to re-exec */

      (void)deliver_message(message_id, FALSE, FALSE);
      search_tidyup();
      _exit(EXIT_SUCCESS);
      }

    if (pid < 0)
      {
      log_write(0, LOG_MAIN|LOG_PANIC, "failed to fork automatic delivery "
        "process: %s", strerror(errno));
      }

    /* In the parent, wait if synchronous delivery is required. */

    else if (synchronous_delivery)
      {
      int status;
      while (wait(&status) != pid);
      }
    }

  /* The loop will repeat if more is TRUE. If we do not know know that the OS
  automatically reaps children (see comments above the loop), clear away any
  finished subprocesses here, in case there are lots of messages coming in
  from the same source. */

  #ifndef SIG_IGN_WORKS
  while (waitpid(-1, NULL, WNOHANG) > 0);
  #endif
  }

exim_exit(EXIT_SUCCESS);   /* Never returns */
return 0;                  /* To stop compiler warning */
}

/* End of exim.c */
