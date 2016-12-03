/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* General functions concerned with transportation, and generic options for all
transports. */


#include "exim.h"


/* Structure for keeping list of addresses that have been added to
Envelope-To:, in order to avoid duplication. */

struct aci {
  struct aci *next;
  address_item *ptr;
  };


/* Static data for write_chunk() */

static uschar *chunk_ptr;           /* chunk pointer */
static uschar *nl_check;            /* string to look for at line start */
static int     nl_check_length;     /* length of same */
static uschar *nl_escape;           /* string to insert */
static int     nl_escape_length;    /* length of same */
static int     nl_partial_match;    /* length matched at chunk end */


/* Generic options for transports, all of which live inside transport_instance
data blocks and which therefore have the opt_public flag set. Note that there
are other options living inside this structure which can be set only from
certain transports. */

optionlist optionlist_transports[] = {
  { "*expand_group",    opt_stringptr|opt_hidden|opt_public,
                 (void *)offsetof(transport_instance, expand_gid) },
  { "*expand_user",     opt_stringptr|opt_hidden|opt_public,
                 (void *)offsetof(transport_instance, expand_uid) },
  { "*headers_rewrite_flags", opt_int|opt_public|opt_hidden,
                 (void *)offsetof(transport_instance, rewrite_existflags) },
  { "*headers_rewrite_rules", opt_void|opt_public|opt_hidden,
                 (void *)offsetof(transport_instance, rewrite_rules) },
  { "*set_group",       opt_bool|opt_hidden|opt_public,
                 (void *)offsetof(transport_instance, gid_set) },
  { "*set_user",        opt_bool|opt_hidden|opt_public,
                 (void *)offsetof(transport_instance, uid_set) },
  { "body_only",        opt_bool|opt_public,
                 (void *)offsetof(transport_instance, body_only) },
  { "current_directory", opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, current_dir) },
  { "debug_print",      opt_stringptr | opt_public,
                 (void *)offsetof(transport_instance, debug_string) },
  { "delivery_date_add", opt_bool|opt_public,
                 (void *)(offsetof(transport_instance, delivery_date_add)) },
  { "disable_logging",  opt_bool|opt_public,
                 (void *)(offsetof(transport_instance, disable_logging)) },
  { "driver",           opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, driver_name) },
  { "envelope_to_add",   opt_bool|opt_public,
                 (void *)(offsetof(transport_instance, envelope_to_add)) },
  { "group",             opt_expand_gid|opt_public,
                 (void *)offsetof(transport_instance, gid) },
  { "headers_add",      opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, add_headers) },
  { "headers_only",     opt_bool|opt_public,
                 (void *)offsetof(transport_instance, headers_only) },
  { "headers_remove",   opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, remove_headers) },
  { "headers_rewrite",  opt_rewrite|opt_public,
                 (void *)offsetof(transport_instance, headers_rewrite) },
  { "home_directory",   opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, home_dir) },
  { "initgroups",       opt_bool|opt_public,
                 (void *)offsetof(transport_instance, initgroups) },
  { "message_size_limit", opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, message_size_limit) },
  { "rcpt_include_affixes", opt_bool|opt_public,
                 (void *)offsetof(transport_instance, rcpt_include_affixes) },
  { "retry_use_local_part", opt_bool|opt_public,
                 (void *)offsetof(transport_instance, retry_use_local_part) },
  { "return_path",      opt_stringptr|opt_public,
                 (void *)(offsetof(transport_instance, return_path)) },
  { "return_path_add",   opt_bool|opt_public,
                 (void *)(offsetof(transport_instance, return_path_add)) },
  { "shadow_condition", opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, shadow_condition) },
  { "shadow_transport", opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, shadow) },
  { "transport_filter", opt_stringptr|opt_public,
                 (void *)offsetof(transport_instance, filter_command) },
  { "transport_filter_timeout", opt_time|opt_public,
                 (void *)offsetof(transport_instance, filter_timeout) },
  { "user",             opt_expand_uid|opt_public,
                 (void *)offsetof(transport_instance, uid) }
};

int optionlist_transports_size =
  sizeof(optionlist_transports)/sizeof(optionlist);


/*************************************************
*             Initialize transport list           *
*************************************************/

/* Read the transports section of the configuration file, and set up a chain of
transport instances according to its contents. Each transport has generic
options and may also have its own private options. This function is only ever
called when transports == NULL. We use generic code in readconf to do most of
the work. */

void
transport_init(void)
{
transport_instance *t;

readconf_driver_init(US"transport",
  (driver_instance **)(&transports),     /* chain anchor */
  (driver_info *)transports_available,   /* available drivers */
  sizeof(transport_info),                /* size of info block */
  &transport_defaults,                   /* default values for generic options */
  sizeof(transport_instance),            /* size of instance block */
  optionlist_transports,                 /* generic options */
  optionlist_transports_size);

/* Now scan the configured transports and check inconsistencies. A shadow
transport is permitted only for local transports. */

for (t = transports; t != NULL; t = t->next)
  {
  if (!t->info->local)
    {
    if (t->shadow != NULL)
      log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
        "shadow transport not allowed on non-local transport %s", t->name);
    }

  if (t->body_only && t->headers_only)
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "%s transport: body_only and headers_only are mutually exclusive",
      t->name);
  }
}



/*************************************************
*             Write block of data                *
*************************************************/

/* Subroutine called by write_chunk() and at the end of the message actually
to write a data block. Also called directly by some transports to write
additional data to the file descriptor (e.g. prefix, suffix).

If a transport wants data transfers to be timed, it sets a non-zero value in
transport_write_timeout. A non-zero transport_write_timeout causes a timer to
be set for each block of data written from here. If time runs out, then write()
fails and provokes an error return. The caller can then inspect sigalrm_seen to
check for a timeout.

On some systems, if a quota is exceeded during the write, the yield is the
number of bytes written rather than an immediate error code. This also happens
on some systems in other cases, for example a pipe that goes away because the
other end's process terminates (Linux). On other systems, (e.g. Solaris 2) you
get the error codes the first time.

The write() function is also interruptible; the Solaris 2.6 man page says:

     If write() is interrupted by a signal before it writes any
     data, it will return -1 with errno set to EINTR.

     If write() is interrupted by a signal after it successfully
     writes some data, it will return the number of bytes written.

To handle these cases, we want to restart the write() to output the remainder
of the data after a non-negative return from write(), except after a timeout.
In the error cases (EDQUOT, EPIPE) no bytes get written the second time, and a
proper error then occurs. In principle, after an interruption, the second
write() could suffer the same fate, but we do not want to continue for
evermore, so stick a maximum repetition count on the loop to act as a
longstop.

Arguments:
  fd        file descriptor to write to
  block     block of bytes to write
  len       number of bytes to write

Returns:    TRUE on success, FALSE on failure (with errno preserved);
              transport_count is incremented by the number of bytes written
*/

BOOL
transport_write_block(int fd, uschar *block, int len)
{
int i, rc, save_errno;

for (i = 0; i < 100; i++)
  {
  DEBUG(D_transport)
    debug_printf("writing data block fd=%d size=%d timeout=%d\n",
      fd, len, transport_write_timeout);
  if (transport_write_timeout > 0) alarm(transport_write_timeout);

  #ifdef SUPPORT_TLS
  if (tls_active == fd) rc = tls_write(block, len); else
  #endif

  rc = write(fd, block, len);
  save_errno = errno;

  /* Cancel the alarm and deal with a timeout */

  if (transport_write_timeout > 0)
    {
    alarm(0);
    if (sigalrm_seen)
      {
      errno = ETIMEDOUT;
      return FALSE;
      }
    }

  /* Hopefully, the most common case is success, so test that first. */

  if (rc == len) { transport_count += len; return TRUE; }

  /* A non-negative return code is an incomplete write. Try again. */

  if (rc >= 0)
    {
    len -= rc;
    block += rc;
    transport_count += rc;
    DEBUG(D_transport) debug_printf("write incomplete (%d)\n", rc);
    continue;
    }

  /* A negative return code with an EINTR error is another form of
  incomplete write, zero bytes having been written */

  if (save_errno == EINTR)
    {
    DEBUG(D_transport)
      debug_printf("write interrupted before anything written\n");
    continue;
    }

  /* A response of EAGAIN from write() is likely only in the case of writing
  to a FIFO that is not swallowing the data as fast as Exim is writing it. */

  if (save_errno == EAGAIN)
    {
    DEBUG(D_transport)
      debug_printf("write temporarily locked out, waiting 1 sec\n");
    sleep(1);
    continue;
    }

  /* Otherwise there's been an error */

  DEBUG(D_transport) debug_printf("writing error %d: %s\n", save_errno,
    strerror(save_errno));
  errno = save_errno;
  return FALSE;
  }

/* We've tried and tried and tried but still failed */

errno = ERRNO_WRITEINCOMPLETE;
return FALSE;
}




/*************************************************
*             Write formatted string             *
*************************************************/

/* This is called by various transports. It is a convenience function.

Arguments:
  fd          file descriptor
  format      string format
  ...         arguments for format

Returns:      the yield of transport_write_block()
*/

BOOL
transport_write_string(int fd, char *format, ...)
{
va_list ap;
va_start(ap, format);
if (!string_vformat(big_buffer, big_buffer_size, format, ap))
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "overlong formatted string in transport");
va_end(ap);
return transport_write_block(fd, big_buffer, Ustrlen(big_buffer));
}




/*************************************************
*              Write character chunk             *
*************************************************/

/* Subroutine used by transport_write_message() to scan character chunks for
newlines and act appropriately. The object is to minimise the number of writes.
The output byte stream is buffered up in deliver_out_buffer, which is written
only when it gets full, thus minimizing write operations and TCP packets.

Static data is used to handle the case when the last character of the previous
chunk was NL, or matched part of the data that has to be escaped.

Arguments:
  fd         file descript to write to
  chunk      pointer to data to write
  len        length of data to write
  usr_crlf   TRUE if CR LF is wanted at the end of each line

In addition, the static nl_xxx variables must be set as required.

Returns:     TRUE on success, FALSE on failure (with errno preserved)
*/

static BOOL
write_chunk(int fd, uschar *chunk, int len, BOOL use_crlf)
{
uschar *start = chunk;
uschar *end = chunk + len;
register uschar *ptr;
int mlen = DELIVER_OUT_BUFFER_SIZE - nl_escape_length - 2;

/* The assumption is made that the check string will never stretch over move
than one chunk since the only time there are partial matches is when copying
the body in large buffers. There is always enough room in the buffer for an
escape string, since the loop below ensures this for each character it
processes, and it won't have stuck in the escape string if it left a partial
match. */

if (nl_partial_match >= 0)
  {
  if (nl_check_length > 0 && len >= nl_check_length &&
      Ustrncmp(start, nl_check + nl_partial_match,
        nl_check_length - nl_partial_match) == 0)
    {
    Ustrncpy(chunk_ptr, nl_escape, nl_escape_length);
    chunk_ptr += nl_escape_length;
    start += nl_check_length - nl_partial_match;
    }

  /* The partial match was a false one. Insert the characters carried over
  from the previous chunk. */

  else if (nl_partial_match > 0)
    {
    Ustrncpy(chunk_ptr, nl_check, nl_partial_match);
    chunk_ptr += nl_partial_match;
    }

  nl_partial_match = -1;
  }

/* Now process the characters in the chunk. Whenever we hit a newline we check
for possible escaping. The code for the non-NL route should be as fast as
possible. */

for (ptr = start; ptr < end; ptr++)
  {
  register int ch;

  /* Flush the buffer if it has reached the threshold - we want to leave enough
  room for the next uschar, plus a possible extra CR for an LF, plus the escape
  string. */

  if (chunk_ptr - deliver_out_buffer > mlen)
    {
    if (!transport_write_block(fd, deliver_out_buffer,
          chunk_ptr - deliver_out_buffer))
      return FALSE;
    chunk_ptr = deliver_out_buffer;
    }

  if ((ch = *ptr) == '\n')
    {
    int left = end - ptr - 1;  /* count of chars left after NL */

    /* Insert CR before NL if required */

    if (use_crlf) *chunk_ptr++ = '\r';
    *chunk_ptr++ = '\n';

    /* The check_string test (formerly "from hack") replaces the specific
    string at the start of a line with an escape string (e.g. "From " becomes
    ">From " or "." becomes "..". It is a case-sensitive test. The length
    check above ensures there is always enough room to insert this string. */

    if (nl_check_length > 0)
      {
      if (left >= nl_check_length &&
          Ustrncmp(ptr+1, nl_check, nl_check_length) == 0)
        {
        Ustrncpy(chunk_ptr, nl_escape, nl_escape_length);
        chunk_ptr += nl_escape_length;
        ptr += nl_check_length;
        }

      /* Handle the case when there isn't enough left to match the whole
      check string, but there may be a partial match. We remember how many
      characters matched, and finish processing this chunk. */

      else if (left <= 0) nl_partial_match = 0;

      else if (Ustrncmp(ptr+1, nl_check, left) == 0)
        {
        nl_partial_match = left;
        ptr = end;
        }
      }
    }

  /* Not a NL character */

  else *chunk_ptr++ = ch;
  }

return TRUE;
}




/*************************************************
*        Generate address for RCPT TO            *
*************************************************/

/* This function puts together an address for RCPT to, using the caseful
version of the local part and the caseful version of the domain. If there is no
prefix or suffix, or if affixes are to be retained, we can just use the
original address. Otherwise, if there is a prefix but no suffix we can use a
pointer into the original address. If there is a suffix, however, we have to
build a new string.

Arguments:
  addr              the address item
  include_affixes   TRUE if affixes are to be included

Returns:            a string
*/

uschar *
transport_rcpt_address(address_item *addr, BOOL include_affixes)
{
uschar *at;
int plen, slen;

if (include_affixes)
  {
  setflag(addr, af_include_affixes);  /* Affects logged => line */
  return addr->address;
  }

if (addr->suffix == NULL)
  {
  if (addr->prefix == NULL) return addr->address;
  return addr->address + Ustrlen(addr->prefix);
  }

at = Ustrrchr(addr->address, '@');
plen = (addr->prefix == NULL)? 0 : Ustrlen(addr->prefix);
slen = Ustrlen(addr->suffix);

return string_sprintf("%.*s@%s", (at - addr->address - plen - slen),
   addr->address + plen, at + 1);
}


/*************************************************
*  Output Envelope-To: address & scan duplicates *
*************************************************/

/* This function is called from internal_transport_write_message() below, when
generating an Envelope-To: header line. It checks for duplicates of the given
address and its ancestors. When one is found, this function calls itself
recursively, to output the envelope address of the duplicate.

We want to avoid duplication in the list, which can arise for example when
A->B,C and then both B and C alias to D. This can also happen when there are
unseen drivers in use. So a list of addresses that have been output is kept in
the plist variable.

It is also possible to have loops in the address ancestry/duplication graph,
for example if there are two top level addresses A and B and we have A->B,C and
B->A. To break the loop, we use a list of processed addresses in the dlist
variable.

After handling duplication, this function outputs the progenitor of the given
address.

Arguments:
  p         the address we are interested in
  pplist    address of anchor of the list of addresses not to output
  pdlist    address of anchor of the list of processed addresses
  first     TRUE if this is the first address; set it FALSE afterwards
  fd        the file descriptor to write to
  use_crlf  to be passed on to write_chunk()

Returns:    FALSE if writing failed
*/

static BOOL
write_env_to(address_item *p, struct aci **pplist, struct aci **pdlist,
  BOOL *first, int fd, BOOL use_crlf)
{
address_item *pp;
struct aci *ppp;

/* Do nothing if we have already handled this address. If not, remember it
so that we don't handle it again. */

for (ppp = *pdlist; ppp != NULL; ppp = ppp->next)
  { if (p == ppp->ptr) return TRUE; }

ppp = store_get(sizeof(struct aci));
ppp->next = *pdlist;
*pdlist = ppp;
ppp->ptr = p;

/* Now scan up the ancestry, checking for duplicates at each generation. */

for (pp = p;; pp = pp->parent)
  {
  address_item *dup;
  for (dup = addr_duplicate; dup != NULL; dup = dup->next)
    {
    if (dup->dupof != pp) continue;   /* Not a dup of our address */
    if (!write_env_to(dup, pplist, pdlist, first, fd, use_crlf)) return FALSE;
    }
  if (pp->parent == NULL) break;
  }

/* Check to see if we have already output the progenitor. */

for (ppp = *pplist; ppp != NULL; ppp = ppp->next)
  { if (pp == ppp->ptr) break; }
if (ppp != NULL) return TRUE;

/* Remember what we have output, and output it. */

ppp = store_get(sizeof(struct aci));
ppp->next = *pplist;
*pplist = ppp;
ppp->ptr = pp;

if (!(*first) && !write_chunk(fd, US",\n ", 3, use_crlf)) return FALSE;
*first = FALSE;
return write_chunk(fd, pp->address, Ustrlen(pp->address), use_crlf);
}




/*************************************************
*                Write the message               *
*************************************************/

/* This function writes the message to the given file descriptor. The headers
are in the in-store data structure, and the rest of the message is in the open
file descriptor deliver_datafile. Make sure we start it at the beginning.

. If add_return_path is TRUE, a "return-path:" header is added to the message,
  containing the envelope sender's address.

. If add_envelope_to is TRUE, a "envelope-to:" header is added to the message,
  giving the top-level envelope address that caused this delivery to happen.

. If add_delivery_date is TRUE, a "delivery-date:" header is added to the
  message. It gives the time and date that delivery took place.

. If check_string is not null, the start of each line is checked for that
  string. If it is found, it is replaced by escape_string. This used to be
  the "from hack" for files, and "smtp_dots" for escaping SMTP dots.

. If use_crlf is true, newlines are turned into CRLF (SMTP output).

The yield is TRUE if all went well, and FALSE if not. Exit *immediately* after
any writing or reading error, leaving the code in errno intact. Error exits
can include timeouts for certain transports, which are requested by setting
transport_write_timeout non-zero.

Arguments:
  addr                  (chain of) addresses (for extra headers), or NULL;
                          only the first address is used
  fd                    file descriptor to write the message to
  options               bit-wise options:
    add_return_path       if TRUE, add a "return-path" header
    add_envelope_to       if TRUE, add a "envelope-to" header
    add_delivery_date     if TRUE, add a "delivery-date" header
    use_crlf              if TRUE, turn NL into CR LF
    end_dot               if TRUE, send a terminating "." line at the end
    no_headers            if TRUE, omit the headers
    no_body               if TRUE, omit the body
  size_limit            if > 0, this is a limit to the size of message written;
                          it is used when returning messages to their senders,
                          and is approximate rather than exact, owing to chunk
                          buffering
  add_headers           a string containing one or more headers to add; it is
                          expanded, and must be in correct RFC 822 format as
                          it is transmitted verbatim; NULL => no additions,
                          and so does empty string or forced expansion fail
  remove_headers        a colon-separated list of headers to remove, or NULL
  check_string          a string to check for at the start of lines, or NULL
  escape_string         a string to insert in front of any check string
  rewrite_rules         chain of header rewriting rules
  rewrite_existflags    flags for the rewriting rules

Returns:                TRUE on success; FALSE (with errno) on failure.
                        In addition, the global variable transport_count
                        is incremented by the number of bytes written.
*/

static BOOL
internal_transport_write_message(address_item *addr, int fd, int options,
  int size_limit, uschar *add_headers, uschar *remove_headers, uschar *check_string,
  uschar *escape_string, rewrite_rule *rewrite_rules, int rewrite_existflags)
{
int written = 0;
int len;
header_line *h;
BOOL use_crlf  = (options & topt_use_crlf)  != 0;

/* Initialize pointer in output buffer. */

chunk_ptr = deliver_out_buffer;

/* Set up the data for start-of-line data checking and escaping */

nl_partial_match = -1;
if (check_string != NULL && escape_string != NULL)
  {
  nl_check = check_string;
  nl_check_length = Ustrlen(nl_check);
  nl_escape = escape_string;
  nl_escape_length = Ustrlen(nl_escape);
  }
else nl_check_length = nl_escape_length = 0;

/* Whether the escaping mechanism is applied to headers or not is controlled by
an option (set for SMTP, not otherwise). Negate the length if not wanted till
after the headers. */

if ((options & topt_escape_headers) == 0) nl_check_length = -nl_check_length;

/* Write the headers if required, including any that have to be added. If there
are header rewriting rules, apply them. */

if ((options & topt_no_headers) == 0)
  {
  /* Add return-path: if requested. */

  if ((options & topt_add_return_path) != 0)
    {
    uschar buffer[ADDRESS_MAXLENGTH + 20];
    sprintf(CS buffer, "Return-path: <%.*s>\n", ADDRESS_MAXLENGTH,
      return_path);
    if (!write_chunk(fd, buffer, Ustrlen(buffer), use_crlf)) return FALSE;
    }

  /* Add envelope-to: if requested */

  if ((options & topt_add_envelope_to) != 0)
    {
    BOOL first = TRUE;
    address_item *p;
    struct aci *plist = NULL;
    struct aci *dlist = NULL;
    void *reset_point = store_get(0);

    if (!write_chunk(fd, US"Envelope-to: ", 13, use_crlf)) return FALSE;

    /* Pick up from all the addresses. The plist and dlist variables are
    anchors for lists of addresses already handled; they have to be defined at
    this level becuase write_env_to() calls itself recursively. */

    for (p = addr; p != NULL; p = p->next)
      {
      if (!write_env_to(p, &plist, &dlist, &first, fd, use_crlf)) return FALSE;
      }

    /* Add a final newline and reset the store used for tracking duplicates */

    if (!write_chunk(fd, US"\n", 1, use_crlf)) return FALSE;
    store_reset(reset_point);
    }

  /* Add delivery-date: if requested. */

  if ((options & topt_add_delivery_date) != 0)
    {
    uschar buffer[100];
    sprintf(CS buffer, "Delivery-date: %s\n", tod_stamp(tod_full));
    if (!write_chunk(fd, buffer, Ustrlen(buffer), use_crlf)) return FALSE;
    }

  /* Then the message's headers. Don't write any that are flagged as "old";
  that means they were rewritten, or are a record of envelope rewriting, or
  were removed (e.g. Bcc). If remove_headers is not null, skip any headers that
  match any entries therein. Then check addr->p.remove_headers too, provided that
  addr is not NULL. */

  if (remove_headers != NULL)
    {
    uschar *s = expand_string(remove_headers);
    if (s == NULL && !expand_string_forcedfail)
      {
      errno = ERRNO_CHHEADER_FAIL;
      return FALSE;
      }
    remove_headers = s;
    }

  for (h = header_list; h != NULL; h = h->next)
    {
    int i;
    uschar *list = NULL;
    BOOL include_header;

    if (h->type == htype_old) continue;

    include_header = TRUE;
    list = remove_headers;

    for (i = 0; i < 2; i++)    /* For remove_headers && addr->p.remove_headers */
      {
      if (list != NULL)
        {
        int sep = ':';         /* This is specified as a colon-separated list */
        uschar *s, *ss;
        uschar buffer[128];
        while ((s = string_nextinlist(&list, &sep, buffer, sizeof(buffer)))
                != NULL)
          {
          int len = Ustrlen(s);
          if (strncmpic(h->text, s, len) != 0) continue;
          ss = h->text + len;
          while (*ss == ' ' || *ss == '\t') ss++;
          if (*ss == ':') break;
          }
        if (s != NULL) { include_header = FALSE; break; }
        }
      if (addr != NULL) list = addr->p.remove_headers;
      }

    /* If this header is to be output, try to rewrite it if there are rewriting
    rules. */

    if (include_header)
      {
      if (rewrite_rules != NULL)
        {
        void *reset_point = store_get(0);
        header_line *hh =
          rewrite_header(h, NULL, NULL, rewrite_rules, rewrite_existflags,
            FALSE);
        if (hh != NULL)
          {
          if (!write_chunk(fd, hh->text, hh->slen, use_crlf)) return FALSE;
          store_reset(reset_point);
          continue;     /* With the next header line */
          }
        }

      /* Either no rewriting rules, or it didn't get rewritten */

      if (!write_chunk(fd, h->text, h->slen, use_crlf)) return FALSE;
      }

    /* Header removed */

    else
      {
      DEBUG(D_transport) debug_printf("removed header line:\n%s---\n",
        h->text);
      }
    }

  /* Add on any address-specific headers. If there are multiple addresses,
  they will all have the same headers in order to be batched. The headers
  are chained in reverse order of adding (so several addresses from the
  same alias might share some of them) but we want to output them in the
  opposite order. This is a bit tedious, but there shouldn't be very many
  of them. We just walk the list twice, reversing the pointers each time,
  but on the second time, write out the items. */

  if (addr != NULL)
    {
    int i;
    header_line *hprev = addr->p.extra_headers;
    header_line *hnext;
    for (i = 0; i < 2; i++)
      {
      for (h = hprev, hprev = NULL; h != NULL; h = hnext)
        {
        hnext = h->next;
        h->next = hprev;
        hprev = h;
        if (i == 1)
          {
          if (!write_chunk(fd, h->text, h->slen, use_crlf)) return FALSE;
          DEBUG(D_transport)
            debug_printf("added header line(s):\n%s---\n", h->text);
          }
        }
      }
    }

  /* If a string containing additional headers exists, expand it and write
  out the result. This is done last so that if it (deliberately or accidentally)
  isn't in header format, it won't mess up any other headers. An empty string
  or a forced expansion failure are noops. */

  if (add_headers != NULL)
    {
    uschar *s = expand_string(add_headers);
    if (s == NULL)
      {
      if (!expand_string_forcedfail)
        {
        errno = ERRNO_CHHEADER_FAIL;
        return FALSE;
        }
      }
    else
      {
      int len = Ustrlen(s);
      if (len > 0)
        {
        if (!write_chunk(fd, s, len, use_crlf)) return FALSE;
        if (s[len-1] != '\n' && !write_chunk(fd, US"\n", 1, use_crlf))
          return FALSE;
        DEBUG(D_transport)
          debug_printf("added header line(s):\n%s---\n", s);
        }
      }
    }

  /* Separate headers from body with a blank line */

  if (!write_chunk(fd, US"\n", 1, use_crlf)) return FALSE;
  }

/* If the body is required, ensure that the data for check strings (formerly
the "from hack") is enabled by negating the length if necessary. (It will be
negative in cases where it isn't to apply to the headers). Then ensure the body
is positioned at the start of its file (following the message id), then write
it, applying the size limit if required. */

if ((options & topt_no_body) == 0)
  {
  nl_check_length = abs(nl_check_length);
  nl_partial_match = 0;
  lseek(deliver_datafile, SPOOL_DATA_START_OFFSET, SEEK_SET);
  while ((len = read(deliver_datafile, deliver_in_buffer,
           DELIVER_IN_BUFFER_SIZE)) > 0)
    {
    if (!write_chunk(fd, deliver_in_buffer, len, use_crlf)) return FALSE;
    if (size_limit > 0)
      {
      written += len;
      if (written > size_limit)
        {
        len = 0;    /* Pretend EOF */
        break;
        }
      }
    }

  /* Finished with the check string */

  nl_check_length = nl_escape_length = 0;

  /* A read error on the body will have left len == -1 and errno set. */

  if (len != 0) return FALSE;

  /* If requested, add a terminating "." line (SMTP output). */

  if ((options & topt_end_dot) != 0 && !write_chunk(fd, US".\n", 2, use_crlf))
    return FALSE;
  }

/* Write out any remaining data in the buffer before returning. */

return (len = chunk_ptr - deliver_out_buffer) <= 0 ||
  transport_write_block(fd, deliver_out_buffer, len);
}




/*************************************************
*    External interface to write the message     *
*************************************************/

/* If there is no filtering required, call the internal function above to do
the real work, passing over all the arguments from this function. Otherwise,
set up a filtering process, fork another process to call the internal function
to write to the filter, and in this process just suck from the filter and write
down the given fd. At the end, tidy up the pipes and the processes.

Arguments:     as for internal_transport_write_message() above

Returns:       TRUE on success; FALSE (with errno) for any failure
               transport_count is incremented by the number of bytes written
*/

BOOL
transport_write_message(address_item *addr, int fd, int options,
  int size_limit, uschar *add_headers, uschar *remove_headers,
  uschar *check_string, uschar *escape_string, rewrite_rule *rewrite_rules,
  int rewrite_existflags)
{
BOOL use_crlf;
BOOL last_filter_was_NL = TRUE;
int rc, len, yield, fd_read, fd_write, save_errno;
int pfd[2];
pid_t filter_pid, write_pid;

/* If there is no filter command set up, call the internal function that does
the actual work, passing it the incoming fd, and return its result. */

if (transport_filter_argv == NULL)
  return internal_transport_write_message(addr, fd, options, size_limit,
    add_headers, remove_headers, check_string, escape_string,
    rewrite_rules, rewrite_existflags);

/* Otherwise the message must be written to a filter process and read back
before being written to the incoming fd. First set up the special processing to
be done during the copying. */

use_crlf  = (options & topt_use_crlf) != 0;
nl_partial_match = -1;

if (check_string != NULL && escape_string != NULL)
  {
  nl_check = check_string;
  nl_check_length = Ustrlen(nl_check);
  nl_escape = escape_string;
  nl_escape_length = Ustrlen(nl_escape);
  }
else nl_check_length = nl_escape_length = 0;

/* Start up a subprocess to run the command. Ensure that our main fd will
be closed when the subprocess execs, but remove the flag afterwards.
(Otherwise, if this is a TCP/IP socket, it can't get passed on to another
process to deliver another message.) We get back stdin/stdout file descriptors.
If the process creation failed, give an error return. */

fd_read = -1;
fd_write = -1;
save_errno = 0;
yield = FALSE;
write_pid = (pid_t)(-1);

fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
filter_pid = child_open(transport_filter_argv, NULL, 077, &fd_write, &fd_read,
  FALSE);
fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) & ~FD_CLOEXEC);
if (filter_pid < 0) goto TIDY_UP;      /* errno set */

DEBUG(D_transport)
  debug_printf("process %d running as transport filter: write=%d read=%d\n",
    (int)filter_pid, fd_write, fd_read);

/* Fork subprocess to write the message to the filter, and return the result
via a(nother) pipe. While writing to the filter, we do not do the CRLF,
smtp dots, or check string processing. */

if (pipe(pfd) != 0) goto TIDY_UP;      /* errno set */
if ((write_pid = fork()) == 0)
  {
  BOOL rc;
  close(fd_read);
  close(pfd[pipe_read]);
  nl_check_length = nl_escape_length = 0;
  rc = internal_transport_write_message(addr, fd_write,
    (options & ~(topt_use_crlf | topt_end_dot)),
    size_limit, add_headers, remove_headers, NULL, NULL,
    rewrite_rules, rewrite_existflags);
  save_errno = errno;
  write(pfd[pipe_write], (void *)&rc, sizeof(BOOL));
  write(pfd[pipe_write], (void *)&save_errno, sizeof(int));
  write(pfd[pipe_write], (void *)&(addr->more_errno), sizeof(int));
  _exit(0);
  }
save_errno = errno;

/* Parent process: close our copy of the writing subprocess' pipes. */

close(pfd[pipe_write]);
close(fd_write);
fd_write = -1;

/* Writing process creation failed */

if (write_pid < 0)
  {
  errno = save_errno;    /* restore */
  goto TIDY_UP;
  }

/* When testing, let the subprocess get going */

if (running_in_test_harness) millisleep(250);

DEBUG(D_transport)
  debug_printf("process %d writing to transport filter\n", (int)write_pid);

/* Copy the message from the filter to the output fd. A read error leaves len
== -1 and errno set. We need to apply a timeout to the read, to cope with
the case when the filter gets stuck, but it can be quite a long one. The
default is 5m, but this is now configurable. */

DEBUG(D_transport) debug_printf("copying from the filter\n");

/* Copy the output of the filter, remembering if the last character was NL. If
no data is returned, that counts as "ended with NL" (default setting of the
variable is TRUE). */

chunk_ptr = deliver_out_buffer;

for (;;)
  {
  sigalrm_seen = FALSE;
  alarm(transport_filter_timeout);
  len = read(fd_read, deliver_in_buffer, DELIVER_IN_BUFFER_SIZE);
  alarm(0);
  if (sigalrm_seen)
    {
    errno = ETIMEDOUT;
    goto TIDY_UP;
    }

  /* If the read was successful, write the block down the original fd,
  remembering whether it ends in \n or not. */

  if (len > 0)
    {
    if (!write_chunk(fd, deliver_in_buffer, len, use_crlf)) goto TIDY_UP;
    last_filter_was_NL = (deliver_in_buffer[len-1] == '\n');
    }

  /* Otherwise, break the loop. If we have hit EOF, set yield = TRUE. */

  else
    {
    if (len == 0) yield = TRUE;
    break;
    }
  }

/* Tidying up code. If yield = FALSE there has been an error and errno is set
to something. Ensure the pipes are all closed and the processes are removed. If
there has been an error, kill the processes before waiting for them, just to be
sure. Also apply a paranoia timeout. */

TIDY_UP:
save_errno = errno;

close(fd_read);
if (fd_write > 0) close(fd_write);

if (!yield)
  {
  if (filter_pid > 0) kill(filter_pid, SIGKILL);
  if (write_pid > 0)  kill(write_pid, SIGKILL);
  }

/* Wait for the filter process to complete. */

DEBUG(D_transport) debug_printf("waiting for filter process\n");
if (filter_pid > 0 && (rc = child_close(filter_pid, 30)) != 0 && yield)
  {
  yield = FALSE;
  save_errno = ERRNO_FILTER_FAIL;
  addr->more_errno = rc;
  DEBUG(D_transport) debug_printf("filter process returned %d\n", rc);
  }

/* Wait for the writing process to complete. If it ends successfully,
read the results from its pipe. */

DEBUG(D_transport) debug_printf("waiting for writing process\n");
if (write_pid > 0)
  {
  if ((rc = child_close(write_pid, 30)) == 0)
    {
    BOOL ok;
    read(pfd[pipe_read], (void *)&ok, sizeof(BOOL));
    if (!ok)
      {
      read(pfd[pipe_read], (void *)&save_errno, sizeof(int));
      read(pfd[pipe_read], (void *)&(addr->more_errno), sizeof(int));
      yield = FALSE;
      }
    }
  else if (yield)
    {
    yield = FALSE;
    save_errno = ERRNO_FILTER_FAIL;
    addr->more_errno = rc;
    DEBUG(D_transport) debug_printf("writing process returned %d\n", rc);
    }
  }
close(pfd[pipe_read]);

/* If there have been no problems we can now add the terminating "." if this is
SMTP output, turning off escaping beforehand. If the last character from the
filter was not NL, insert a NL to make the SMTP protocol work. */

if (yield)
  {
  nl_check_length = nl_escape_length = 0;
  if ((options & topt_end_dot) != 0 && (last_filter_was_NL?
        !write_chunk(fd, US".\n", 2, use_crlf) :
        !write_chunk(fd, US"\n.\n", 3, use_crlf)))
    {
    yield = FALSE;
    }

  /* Write out any remaining data in the buffer. */

  else
    {
    yield = (len = chunk_ptr - deliver_out_buffer) <= 0 ||
      transport_write_block(fd, deliver_out_buffer, len);
    }
  }
else errno = save_errno;      /* From some earlier error */

DEBUG(D_transport)
  {
  debug_printf("end of filtering transport writing: yield=%d\n", yield);
  if (!yield)
    debug_printf("errno=%d more_errno=%d\n", errno, addr->more_errno);
  }

return yield;
}





/*************************************************
*            Update waiting database             *
*************************************************/

/* This is called when an address is deferred by remote transports that are
capable of sending more than one message over one connection. A database is
maintained for each transport, keeping track of which messages are waiting for
which hosts. The transport can then consult this when eventually a successful
delivery happens, and if it finds that another message is waiting for the same
host, it can fire up a new process to deal with it using the same connection.

The database records are keyed by host name. They can get full if there are
lots of messages waiting, and so there is a continuation mechanism for them.

Each record contains a list of message ids, packed end to end without any
zeros. Each one is MESSAGE_ID_LENGTH bytes long. The count field says how many
in this record, and the sequence field says if there are any other records for
this host. If the sequence field is 0, there are none. If it is 1, then another
record with the name <hostname>:0 exists; if it is 2, then two other records
with sequence numbers 0 and 1 exist, and so on.

Currently, an exhaustive search of all continuation records has to be done to
determine whether to add a message id to a given record. This shouldn't be
too bad except in extreme cases. I can't figure out a *simple* way of doing
better.

Old records should eventually get swept up by the exim_tidydb utility.

Arguments:
  hostlist  list of hosts that this message could be sent to;
              the update_waiting flag is set if a host is to be noted
  tpname    name of the transport

Returns:    nothing
*/

void
transport_update_waiting(host_item *hostlist, uschar *tpname)
{
uschar buffer[256];
uschar *prevname = US"";
host_item *host;
open_db dbblock;
open_db *dbm_file;

/* Open the database for this transport */

sprintf(CS buffer, "wait-%.200s", tpname);
dbm_file = dbfn_open(buffer, O_RDWR, &dbblock, TRUE);
if (dbm_file == NULL) return;

/* Scan the list of hosts for which this message is waiting, and ensure
that the message id is in each host record for those that have the
update_waiting flag set. */

for (host = hostlist; host!= NULL; host = host->next)
  {
  BOOL already = FALSE;
  dbdata_wait *host_record;
  uschar *s;
  int i, host_length;

  /* Skip if the update_waiting flag is not set. */

  if (!host->update_waiting) continue;

  /* Skip if this is the same host as we just processed; otherwise remember
  the name for next time. */

  if (Ustrcmp(prevname, host->name) == 0) continue;
  prevname = host->name;

  /* Look up the host record; if there isn't one, make an empty one. */

  host_record = dbfn_read(dbm_file, host->name);
  if (host_record == NULL)
    {
    host_record = store_get(sizeof(dbdata_wait) + MESSAGE_ID_LENGTH);
    host_record->count = host_record->sequence = 0;
    }

  /* Compute the current length */

  host_length = host_record->count * MESSAGE_ID_LENGTH;

  /* Search the record to see if the current message is already in it. */

  for (s = host_record->text; s < host_record->text + host_length;
       s += MESSAGE_ID_LENGTH)
    {
    if (Ustrncmp(s, message_id, MESSAGE_ID_LENGTH) == 0)
      { already = TRUE; break; }
    }

  /* If we haven't found this message in the main record, search any
  continuation records that exist. */

  for (i = host_record->sequence - 1; i >= 0 && !already; i--)
    {
    dbdata_wait *cont;
    sprintf(CS buffer, "%.200s:%d", host->name, i);
    cont = dbfn_read(dbm_file, buffer);
    if (cont != NULL)
      {
      int clen = cont->count * MESSAGE_ID_LENGTH;
      for (s = cont->text; s < cont->text + clen; s += MESSAGE_ID_LENGTH)
        {
        if (Ustrncmp(s, message_id, MESSAGE_ID_LENGTH) == 0)
          { already = TRUE; break; }
        }
      }
    }

  /* If this message is already in a record, no need to update. */

  if (already) continue;


  /* If this record is full, write it out with a new name constructed
  from the sequence number, increase the sequence number, and empty
  the record. */

  if (host_record->count >= WAIT_NAME_MAX)
    {
    sprintf(CS buffer, "%.200s:%d", host->name, host_record->sequence);
    dbfn_write(dbm_file, buffer, host_record, sizeof(dbdata_wait) + host_length);
    host_record->sequence++;
    host_record->count = 0;
    host_length = 0;
    }

  /* If this record is not full, increase the size of the record to
  allow for one new message id. */

  else
    {
    dbdata_wait *newr =
      store_get(sizeof(dbdata_wait) + host_length + MESSAGE_ID_LENGTH);
    memcpy(newr, host_record, sizeof(dbdata_wait) + host_length);
    host_record = newr;
    }

  /* Now add the new name on the end */

  memcpy(host_record->text + host_length, message_id, MESSAGE_ID_LENGTH);
  host_record->count++;
  host_length += MESSAGE_ID_LENGTH;

  /* Update the database */

  dbfn_write(dbm_file, host->name, host_record, sizeof(dbdata_wait) + host_length);
  }

/* All now done */

dbfn_close(dbm_file);
}




/*************************************************
*         Test for waiting messages              *
*************************************************/

/* This function is called by a remote transport which uses the previous
function to remember which messages are waiting for which remote hosts. It's
called after a successful delivery and its job is to check whether there is
another message waiting for the same host. However, it doesn't do this if the
current continue sequence is greater than the maximum supplied as an argument,
or greater than the global connection_max_messages, which, if set, overrides.

Arguments:
  transport_name     name of the transport
  hostname           name of the host
  local_message_max  maximum number of messages down one connection
                       as set by the caller transport
  new_message_id     set to the message id of a waiting message
  more               set TRUE if there are yet more messages waiting

Returns:             TRUE if new_message_id set; FALSE otherwise
*/

BOOL
transport_check_waiting(uschar *transport_name, uschar *hostname,
  int local_message_max, uschar *new_message_id, BOOL *more)
{
dbdata_wait *host_record;
int host_length, path_len;
open_db dbblock;
open_db *dbm_file;
uschar buffer[256];

*more = FALSE;

DEBUG(D_transport)
  {
  debug_printf("transport_check_waiting entered\n");
  debug_printf("  sequence=%d local_max=%d global_max=%d\n",
    continue_sequence, local_message_max, connection_max_messages);
  }

/* Do nothing if we have hit the maximum number that can be send down one
connection. */

if (connection_max_messages >= 0) local_message_max = connection_max_messages;
if (local_message_max > 0 && continue_sequence >= local_message_max)
  {
  DEBUG(D_transport)
    debug_printf("max messages for one connection reached: returning\n");
  return FALSE;
  }

/* Open the waiting information database. */

sprintf(CS buffer, "wait-%.200s", transport_name);
dbm_file = dbfn_open(buffer, O_RDWR, &dbblock, TRUE);
if (dbm_file == NULL) return FALSE;

/* See if there is a record for this host; if not, there's nothing to do. */

host_record = dbfn_read(dbm_file, hostname);
if (host_record == NULL)
  {
  dbfn_close(dbm_file);
  DEBUG(D_transport) debug_printf("no messages waiting for %s\n", hostname);
  return FALSE;
  }

/* If the data in the record looks corrupt, just log something and
don't try to use it. */

if (host_record->count > WAIT_NAME_MAX)
  {
  dbfn_close(dbm_file);
  log_write(0, LOG_MAIN|LOG_PANIC, "smtp-wait database entry for %s has bad "
    "count=%d (max=%d)", hostname, host_record->count, WAIT_NAME_MAX);
  return FALSE;
  }

/* Scan the message ids in the record from the end towards the beginning,
until one is found for which a spool file actually exists. If the record gets
emptied, delete it and continue with any continuation records that may exist.
*/

host_length = host_record->count * MESSAGE_ID_LENGTH;

/* Loop to handle continuation host records in the database */

for (;;)
  {
  BOOL found = FALSE;

  sprintf(CS buffer, "%s/input/", spool_directory);
  path_len = Ustrlen(buffer);

  for (host_length -= MESSAGE_ID_LENGTH; host_length >= 0;
       host_length -= MESSAGE_ID_LENGTH)
    {
    struct stat statbuf;
    Ustrncpy(new_message_id, host_record->text + host_length,
      MESSAGE_ID_LENGTH);
    new_message_id[MESSAGE_ID_LENGTH] = 0;

    if (split_spool_directory)
      sprintf(CS(buffer + path_len), "%c/%s-D", new_message_id[5], new_message_id);
    else
      sprintf(CS(buffer + path_len), "%s-D", new_message_id);

    /* The listed message may be the one we are currently processing. If
    so, we want to remove it from the list without doing anything else.
    If not, do a stat to see if it is an existing message. If it is, break
    the loop to handle it. No need to bother about locks; as this is all
    "hint" processing, it won't matter if it doesn't exist by the time exim
    actually tries to deliver it. */

    if (Ustrcmp(new_message_id, message_id) != 0 &&
        Ustat(buffer, &statbuf) == 0)
      {
      found = TRUE;
      break;
      }
    }

  /* If we have removed all the message ids from the record delete the record.
  If there is a continuation record, fetch it and remove it from the file,
  as it will be rewritten as the main record. Repeat in the case of an
  empty continuation. */

  while (host_length <= 0)
    {
    int i;
    dbdata_wait *newr = NULL;

    /* Search for a continuation */

    for (i = host_record->sequence - 1; i >= 0 && newr == NULL; i--)
      {
      sprintf(CS buffer, "%.200s:%d", hostname, i);
      newr = dbfn_read(dbm_file, buffer);
      }

    /* If no continuation, delete the current and break the loop */

    if (newr == NULL)
      {
      dbfn_delete(dbm_file, hostname);
      break;
      }

    /* Else replace the current with the continuation */

    dbfn_delete(dbm_file, buffer);
    host_record = newr;
    host_length = host_record->count * MESSAGE_ID_LENGTH;
    }

  /* If we found an existing message, break the continuation loop. */

  if (found) break;

  /* If host_length <= 0 we have emptied a record and not found a good message,
  and there are no continuation records. Otherwise there is a continuation
  record to process. */

  if (host_length <= 0)
    {
    dbfn_close(dbm_file);
    DEBUG(D_transport) debug_printf("waiting messages already delivered\n");
    return FALSE;
    }
  }

/* Control gets here when an existing message has been encountered; its
id is in new_message_id, and host_length is the revised length of the
host record. If it is zero, the record has been removed. Update the
record if required, close the database, and return TRUE. */

if (host_length > 0)
  {
  host_record->count = host_length/MESSAGE_ID_LENGTH;
  dbfn_write(dbm_file, hostname, host_record, (int)sizeof(dbdata_wait) + host_length);
  *more = TRUE;
  }

dbfn_close(dbm_file);
return TRUE;
}



/*************************************************
*    Deliver waiting message down same socket    *
*************************************************/

/* Fork a new exim process to deliver the message, and do a re-exec, both to
get a clean delivery process, and to regain root privilege in cases where it
has been given away.

Arguments:
  transport_name  to pass to the new process
  hostname        ditto
  hostaddress     ditto
  id              the new message to process
  socket_fd       the connected socket

Returns:          FALSE if fork fails; TRUE otherwise
*/

BOOL
transport_pass_socket(uschar *transport_name, uschar *hostname,
  uschar *hostaddress, uschar *id, int socket_fd)
{
pid_t pid;
int status;

DEBUG(D_transport) debug_printf("transport_pass_socket entered\n");

if ((pid = fork()) == 0)
  {
  int i = 16;
  uschar **argv;

  /* Disconnect entirely from the parent process. If we are running in the
  test harness, wait for a bit to allow the previous process time to finish,
  write the log, etc., so that the output is always in the same order for
  automatic comparison. */

  if ((pid = fork()) != 0) _exit(EXIT_SUCCESS);
  if (running_in_test_harness) millisleep(500);

  /* Set up the calling arguments; use the standard function for the basics,
  but we have a number of extras that may be added. */

  argv = child_exec_exim(CEE_RETURN_ARGV, TRUE, &i, FALSE, 0);

  if (smtp_authenticated) argv[i++] = US"-MCA";

  #ifdef SUPPORT_TLS
  if (tls_offered) argv[i++] = US"-MCT";
  #endif

  if (smtp_use_size) argv[i++] = US"-MCS";
  if (smtp_use_pipelining) argv[i++] = US"-MCP";

  if (queue_run_pid != (pid_t)0)
    {
    argv[i++] = US"-MCQ";
    argv[i++] = string_sprintf("%d", queue_run_pid);
    argv[i++] = string_sprintf("%d", queue_run_pipe);
    }

  argv[i++] = US"-MC";
  argv[i++] = transport_name;
  argv[i++] = hostname;
  argv[i++] = hostaddress;
  argv[i++] = string_sprintf("%d", continue_sequence + 1);
  argv[i++] = id;
  argv[i++] = NULL;

  /* Arrange for the channel to be on stdin. */

  if (socket_fd != 0)
    {
    dup2(socket_fd, 0);
    close(socket_fd);
    }

  DEBUG(D_exec) debug_print_argv(argv);
  exim_nullstd();                          /* Ensure std{out,err} exist */
  execv(CS argv[0], (char *const *)argv);

  DEBUG(D_any) debug_printf("execv failed: %s\n", strerror(errno));
  _exit(errno);         /* Note: must be _exit(), NOT exit() */
  }

/* If the process creation succeeded, wait for the first-level child, which
immediately exits, leaving the second level process entirely disconnected from
this one. */

if (pid > 0)
  {
  int rc;
  while ((rc = wait(&status)) != pid && (rc >= 0 || errno != ECHILD));
  DEBUG(D_transport) debug_printf("transport_pass_socket succeeded\n");
  return TRUE;
  }
else
  {
  DEBUG(D_transport) debug_printf("transport_pass_socket failed to fork: %s\n",
    strerror(errno));
  return FALSE;
  }
}



/*************************************************
*          Set up direct (non-shell) command     *
*************************************************/

/* This function is called when a command line is to be parsed and executed
directly, without the use of /bin/sh. It is called by the pipe transport,
the queryprogram router, and also from the main delivery code when setting up a
transport filter process. The code for ETRN also makes use of this; in that
case, no addresses are passed.

Arguments:
  argvptr            pointer to anchor for argv vector
  cmd                points to the command string
  expand_arguments   true if expansion is to occur
  expand_failed      error value to set if expansion fails; not relevant if
                     addr == NULL
  addr               chain of addresses, or NULL
  etext              text for use in error messages
  errptr             where to put error message if addr is NULL;
                     otherwise it is put in the first address

Returns:             TRUE if all went well; otherwise an error will be
                     set in the first address and FALSE returned
*/

BOOL
transport_set_up_command(uschar ***argvptr, uschar *cmd, BOOL expand_arguments,
  int expand_failed, address_item *addr, uschar *etext, uschar **errptr)
{
address_item *ad;
uschar **argv;
uschar *s, *ss;
int address_count = 0;
int argcount = 0;
int i, max_args;

/* Get store in which to build an argument list. Count the number of addresses
supplied, and allow for that many arguments, plus an additional 60, which
should be enough for anybody. Multiple addresses happen only when the local
delivery batch option is set. */

for (ad = addr; ad != NULL; ad = ad->next) address_count++;
max_args = address_count + 60;
*argvptr = argv = store_get((max_args+1)*sizeof(uschar *));

/* Split the command up into arguments terminated by white space. Lose
trailing space at the start and end. Double-quoted arguments can contain \\ and
\" escapes and so can be handled by the standard function; single-quoted
arguments are verbatim. Copy each argument into a new string. */

s = cmd;
while (isspace(*s)) s++;

while (*s != 0 && argcount < max_args)
  {
  if (*s == '\'')
    {
    ss = s + 1;
    while (*ss != 0 && *ss != '\'') ss++;
    argv[argcount++] = ss = store_get(ss - s++);
    while (*s != 0 && *s != '\'') *ss++ = *s++;
    if (*s != 0) s++;
    *ss++ = 0;
    }
  else argv[argcount++] = string_dequote(&s);
  while (isspace(*s)) s++;
  }

argv[argcount] = (uschar *)0;

/* If *s != 0 we have run out of argument slots. */

if (*s != 0)
  {
  uschar *msg = string_sprintf("Too many arguments in command \"%s\" in "
    "%s", cmd, etext);
  if (addr != NULL)
    {
    addr->transport_return = FAIL;
    addr->message = msg;
    }
  else *errptr = msg;
  return FALSE;
  }

/* Expand each individual argument if required. Expansion happens for pipes set
up in filter files and with directly-supplied commands. It does not happen if
the pipe comes from a traditional .forward file. A failing expansion is a big
disaster if the command came from Exim's configuration; if it came from a user
it is just a normal failure. The expand_failed value is used as the error value
to cater for these two cases.

An argument consisting just of the text "$pipe_addresses" is treated specially.
It is not passed to the general expansion function. Instead, it is replaced by
a number of arguments, one for each address. This avoids problems with shell
metacharacters and spaces in addresses.

If the parent of the top address has an original part of "system-filter", this
pipe was set up by the system filter, and we can permit the expansion of
$recipients. */

DEBUG(D_transport)
  {
  debug_printf("direct command:\n");
  for (i = 0; argv[i] != (uschar *)0; i++)
    debug_printf("  argv[%d] = %s\n", i, string_printing(argv[i]));
  }

if (expand_arguments)
  {
  BOOL allow_dollar_recipients = addr != NULL &&
    addr->parent != NULL &&
    Ustrcmp(addr->parent->address, "system-filter") == 0;

  for (i = 0; argv[i] != (uschar *)0; i++)
    {

    /* Handle special fudge for passing an address list */

    if (addr != NULL &&
        (Ustrcmp(argv[i], "$pipe_addresses") == 0 ||
         Ustrcmp(argv[i], "${pipe_addresses}") == 0))
      {
      int additional;

      if (argcount + address_count - 1 > max_args)
        {
        addr->transport_return = FAIL;
        addr->message = string_sprintf("Too many arguments to command \"%s\" "
          "in %s", cmd, etext);
        return FALSE;
        }

      additional = address_count - 1;
      if (additional > 0)
        memmove(argv + i + 1 + additional, argv + i + 1,
          (argcount - i)*sizeof(uschar *));

      for (ad = addr; ad != NULL; ad = ad->next) argv[i++] = ad->address;
      i--;
      }

    /* Handle normal expansion string */

    else
      {
      uschar *expanded_arg;
      enable_dollar_recipients = allow_dollar_recipients;
      expanded_arg = expand_string(argv[i]);
      enable_dollar_recipients = FALSE;

      if (expanded_arg == NULL)
        {
        uschar *msg = string_sprintf("Expansion of \"%s\" "
          "from command \"%s\" in %s failed: %s",
          argv[i], cmd, etext, expand_string_message);
        if (addr != NULL)
          {
          addr->transport_return = expand_failed;
          addr->message = msg;
          }
        else *errptr = msg;
        return FALSE;
        }
      argv[i] = expanded_arg;
      }
    }

  DEBUG(D_transport)
    {
    debug_printf("direct command after expansion:\n");
    for (i = 0; argv[i] != (uschar *)0; i++)
      debug_printf("  argv[%d] = %s\n", i, string_printing(argv[i]));
    }
  }

return TRUE;
}

/* End of transport.c */
