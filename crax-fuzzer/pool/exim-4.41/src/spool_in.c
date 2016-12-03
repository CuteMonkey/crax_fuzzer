/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* Functions for reading spool files. When compiling for a utility (eximon),
not all are needed, and some functionality can be cut out. */


#include "exim.h"



#ifndef COMPILE_UTILITY
/*************************************************
*           Open and lock data file              *
*************************************************/

/* The data file is the one that is used for locking, because the header file
can get replaced during delivery because of header rewriting. The file has
to opened with write access so that we can get an exclusive lock, but in
fact it won't be written to. Just in case there's a major disaster (e.g.
overwriting some other file descriptor with the value of this one), open it
with append.

Argument: the id of the message
Returns:  TRUE if file successfully opened and locked

Side effect: deliver_datafile is set to the fd of the open file.
*/

BOOL
spool_open_datafile(uschar *id)
{
int i;
struct stat statbuf;
flock_t lock_data;
uschar spoolname[256];

/* If split_spool_directory is set, first look for the file in the appropriate
sub-directory of the input directory. If it is not found there, try the input
directory itself, to pick up leftovers from before the splitting. If split_
spool_directory is not set, first look in the main input directory. If it is
not found there, try the split sub-directory, in case it is left over from a
splitting state. */

for (i = 0; i < 2; i++)
  {
  int save_errno;
  message_subdir[0] = (split_spool_directory == (i == 0))? id[5] : 0;
  sprintf(CS spoolname, "%s/input/%s/%s-D", spool_directory, message_subdir, id);
  deliver_datafile = Uopen(spoolname, O_RDWR | O_APPEND, 0);
  if (deliver_datafile >= 0) break;
  save_errno = errno;
  if (errno == ENOENT)
    {
    if (i == 0) continue;
    if (!queue_running)
      log_write(0, LOG_MAIN, "Spool file %s-D not found", id);
    }
  else log_write(0, LOG_MAIN, "Spool error for %s: %s", spoolname,
    strerror(errno));
  errno = save_errno;
  return FALSE;
  }

/* File is open and message_subdir is set. Set the close-on-exec flag, and lock
the file. We lock only the first line of the file (containing the message ID)
because this apparently is needed for running Exim under Cygwin. If the entire
file is locked in one process, a sub-process cannot access it, even when passed
an open file descriptor (at least, I think that's the Cygwin story). On real
Unix systems it doesn't make any difference as long as Exim is consistent in
what it locks. */

fcntl(deliver_datafile, F_SETFD, fcntl(deliver_datafile, F_GETFD) |
  FD_CLOEXEC);

lock_data.l_type = F_WRLCK;
lock_data.l_whence = SEEK_SET;
lock_data.l_start = 0;
lock_data.l_len = SPOOL_DATA_START_OFFSET;

if (fcntl(deliver_datafile, F_SETLK, &lock_data) < 0)
  {
  log_write(L_skip_delivery,
            LOG_MAIN,
            "Spool file is locked (another process is handling this message)");
  close(deliver_datafile);
  deliver_datafile = -1;
  errno = 0;
  return FALSE;
  }

/* Get the size of the data; don't include the leading filename line
in the count, but add one for the newline before the data. */

if (fstat(deliver_datafile, &statbuf) == 0)
  {
  message_body_size = statbuf.st_size - SPOOL_DATA_START_OFFSET;
  message_size = message_body_size + 1;
  }

return TRUE;
}
#endif  /* COMPILE_UTILITY */



/*************************************************
*    Read non-recipients tree from spool file    *
*************************************************/

/* The tree of non-recipients is written to the spool file in a form that
makes it easy to read back into a tree. The format is as follows:

   . Each node is preceded by two letter(Y/N) indicating whether it has left
     or right children. There's one space after the two flags, before the name.

   . The left subtree (if any) then follows, then the right subtree (if any).

This function is entered with the next input line in the buffer. Note we must
save the right flag before recursing with the same buffer.

Once the tree is read, we re-construct the balance fields by scanning the tree.
I forgot to write them out originally, and the compatible fix is to do it this
way. This initial local recursing function does the necessary.

Arguments:
  node      tree node

Returns:    maximum depth below the node, including the node itself
*/

static int
count_below(tree_node *node)
{
int nleft, nright;
if (node == NULL) return 0;
nleft = count_below(node->left);
nright = count_below(node->right);
node->balance = (nleft > nright)? 1 : ((nright > nleft)? 2 : 0);
return 1 + ((nleft > nright)? nleft : nright);
}

/* This is the real function...

Arguments:
  connect      pointer to the root of the tree
  f            FILE to read data from
  buffer       contains next input line; further lines read into it
  buffer_size  size of the buffer

Returns:       FALSE on format error
*/

static BOOL
read_nonrecipients_tree(tree_node **connect, FILE *f, uschar *buffer,
  int buffer_size)
{
tree_node *node;
int n = Ustrlen(buffer);
BOOL right = buffer[1] == 'Y';

if (n < 5) return FALSE;    /* malformed line */
buffer[n-1] = 0;            /* Remove \n */
node = store_get(sizeof(tree_node) + n - 3);
*connect = node;
Ustrcpy(node->name, buffer + 3);
node->data.ptr = NULL;

if (buffer[0] == 'Y')
  {
  if (Ufgets(buffer, buffer_size, f) == NULL ||
    !read_nonrecipients_tree(&node->left, f, buffer, buffer_size))
      return FALSE;
  }
else node->left = NULL;

if (right)
  {
  if (Ufgets(buffer, buffer_size, f) == NULL ||
    !read_nonrecipients_tree(&node->right, f, buffer, buffer_size))
      return FALSE;
  }
else node->right = NULL;

(void) count_below(*connect);
return TRUE;
}




/*************************************************
*             Read spool header file             *
*************************************************/

/* This function reads a spool header file and places the data into the
appropriate global variables. The header portion is always read, but header
structures are built only if read_headers is set true. It isn't, for example,
while generating -bp output.

It may be possible for blocks of nulls (binary zeroes) to get written on the
end of a file if there is a system crash during writing. It was observed on an
earlier version of Exim that omitted to fsync() the files - this is thought to
have been the cause of that incident, but in any case, this code must be robust
against such an event, and if such a file is encountered, it must be treated as
malformed.

Arguments:
  name          name of the header file, including the -H
  read_headers  TRUE if in-store header structures are to be built
  subdir_set    TRUE is message_subdir is already set

Returns:        spool_read_OK        success
                spool_read_notopen   open failed
                spool_read_enverror  error in the envelope portion
                spool_read_hdrdrror  error in the header portion
*/

int
spool_read_header(uschar *name, BOOL read_headers, BOOL subdir_set)
{
FILE *f = NULL;
int n;
int rcount = 0;
long int uid, gid;
BOOL inheader = FALSE;
uschar originator[64];

/* Reset all the global variables to their default values. However, there is
one exception. DO NOT change the default value of dont_deliver, because it may
be forced by an external setting. */

for (n = 0; n < ACL_C_MAX + ACL_M_MAX; n++) acl_var[n] = NULL;

authenticated_id = NULL;
authenticated_sender = NULL;
allow_unqualified_recipient = FALSE;
allow_unqualified_sender = FALSE;
body_linecount = 0;
body_zerocount = 0;
deliver_firsttime = FALSE;
deliver_freeze = FALSE;
deliver_frozen_at = 0;
deliver_manual_thaw = FALSE;
/* dont_deliver must NOT be reset */
header_list = header_last = NULL;
host_lookup_failed = FALSE;
interface_address = NULL;
interface_port = 0;
local_error_message = FALSE;
local_scan_data = NULL;
message_linecount = 0;
received_protocol = NULL;
received_count = 0;
recipients_list = NULL;
sender_address = NULL;
sender_fullhost = NULL;
sender_helo_name = NULL;
sender_host_address = NULL;
sender_host_name = NULL;
sender_host_port = 0;
sender_host_authenticated = NULL;
sender_ident = NULL;
sender_local = FALSE;
sender_set_untrusted = FALSE;
tree_nonrecipients = NULL;

#ifdef SUPPORT_TLS
tls_certificate_verified = FALSE;
tls_cipher = NULL;
tls_peerdn = NULL;
#endif

/* Generate the full name and open the file. If message_subdir is already
set, just look in the given directory. Otherwise, look in both the split
and unsplit directories, as for the data file above. */

for (n = 0; n < 2; n++)
  {
  if (!subdir_set)
    message_subdir[0] = (split_spool_directory == (n == 0))? name[5] : 0;
  sprintf(CS big_buffer, "%s/input/%s/%s", spool_directory, message_subdir,
    name);
  f = Ufopen(big_buffer, "rb");
  if (f != NULL) break;
  if (n != 0 || subdir_set || errno != ENOENT) return spool_read_notopen;
  }

errno = 0;

#ifndef COMPILE_UTILITY
DEBUG(D_deliver) debug_printf("reading spool file %s\n", name);
#endif  /* COMPILE_UTILITY */

/* The first line of a spool file contains the message id followed by -H (i.e.
the file name), in order to make the file self-identifying. */

if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
if (Ustrlen(big_buffer) != MESSAGE_ID_LENGTH + 3 ||
    Ustrncmp(big_buffer, name, MESSAGE_ID_LENGTH + 2) != 0)
  goto SPOOL_FORMAT_ERROR;

/* The next three lines in the header file are in a fixed format. The first
contains the login, uid, and gid of the user who caused the file to be written.
The second contains the mail address of the message's sender, enclosed in <>.
The third contains the time the message was received, and the number of warning
messages for delivery delays that have been sent. */

if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;

if (sscanf(CS big_buffer, "%s %ld %ld", originator, &uid, &gid) != 3)
  goto SPOOL_FORMAT_ERROR;
originator_login = string_copy(originator);
originator_uid = (uid_t)uid;
originator_gid = (gid_t)gid;

if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
n = Ustrlen(big_buffer);
if (n < 3 || big_buffer[0] != '<' || big_buffer[n-2] != '>')
  goto SPOOL_FORMAT_ERROR;

sender_address = store_get(n-2);
Ustrncpy(sender_address, big_buffer+1, n-3);
sender_address[n-3] = 0;

if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
if (sscanf(CS big_buffer, "%d %d", &received_time, &warning_count) != 2)
  goto SPOOL_FORMAT_ERROR;

message_age = time(NULL) - received_time;

#ifndef COMPILE_UTILITY
DEBUG(D_deliver) debug_printf("user=%s uid=%ld gid=%ld sender=%s\n",
  originator_login, (long int)originator_uid, (long int)originator_gid,
  sender_address);
#endif  /* COMPILE_UTILITY */

/* Now there may be a number of optional lines, each starting with "-".
If you add a new setting here, make sure you set the default above. */

for (;;)
  {
  if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
  if (big_buffer[0] != '-') break;

  big_buffer[Ustrlen(big_buffer) - 1] = 0;
  if (Ustrncmp(big_buffer, "-acl ", 5) == 0)
    {
    int index, count;
    if (sscanf(CS big_buffer + 5, "%d %d", &index, &count) != 2)
      goto SPOOL_FORMAT_ERROR;
    /* Ignore if index too big - might be if a later release with more
    variables built this spool file. */
    if (index < ACL_C_MAX + ACL_M_MAX)
      {
      acl_var[index] = store_get(count + 1);
      if (fread(acl_var[index], 1, count+1, f) < count) goto SPOOL_READ_ERROR;
      acl_var[index][count] = 0;
      }
    }
  else if (Ustrcmp(big_buffer, "-local") == 0) sender_local = TRUE;
  else if (Ustrcmp(big_buffer, "-localerror") == 0)
    local_error_message = TRUE;
  else if (Ustrncmp(big_buffer, "-local_scan ", 12) == 0)
    local_scan_data = string_copy(big_buffer + 12);
  else if (Ustrcmp(big_buffer, "-host_lookup_failed") == 0)
    host_lookup_failed = TRUE;
  else if (Ustrncmp(big_buffer, "-body_linecount", 15) == 0)
    body_linecount = Uatoi(big_buffer + 15);
  else if (Ustrncmp(big_buffer, "-body_zerocount", 15) == 0)
    body_zerocount = Uatoi(big_buffer + 15);
  else if (Ustrncmp(big_buffer, "-frozen", 7) == 0)
    {
    deliver_freeze = TRUE;
    deliver_frozen_at = Uatoi(big_buffer + 7);
    }
  else if (Ustrcmp(big_buffer, "-allow_unqualified_recipient") == 0)
    allow_unqualified_recipient = TRUE;
  else if (Ustrcmp(big_buffer, "-allow_unqualified_sender") == 0)
    allow_unqualified_sender = TRUE;
  else if (Ustrcmp(big_buffer, "-deliver_firsttime") == 0)
    deliver_firsttime = TRUE;
  else if (Ustrcmp(big_buffer, "-manual_thaw") == 0)
    deliver_manual_thaw = TRUE;
  else if (Ustrncmp(big_buffer, "-auth_id", 8) == 0)
    authenticated_id = string_copy(big_buffer + 9);
  else if (Ustrncmp(big_buffer, "-auth_sender", 12) == 0)
    authenticated_sender = string_copy(big_buffer + 13);
  else if (Ustrncmp(big_buffer, "-sender_set_untrusted", 21) == 0)
    sender_set_untrusted = TRUE;

  #ifdef SUPPORT_TLS
  else if (Ustrncmp(big_buffer, "-tls_certificate_verified", 25) == 0)
    tls_certificate_verified = TRUE;
  else if (Ustrncmp(big_buffer, "-tls_cipher", 11) == 0)
    tls_cipher = string_copy(big_buffer + 12);
  else if (Ustrncmp(big_buffer, "-tls_peerdn", 11) == 0)
    tls_peerdn = string_copy(big_buffer + 12);
  #endif

  /* We now record the port number after the address, separated by a
  dot. For compatibility during upgrading, do nothing if there
  isn't a value (it gets left at zero). */

  else if (Ustrncmp(big_buffer, "-host_address", 13) == 0)
    {
    sender_host_port = host_extract_port(big_buffer + 14);
    sender_host_address = string_copy(big_buffer + 14);
    }

  else if (Ustrncmp(big_buffer, "-interface_address", 18) == 0)
    {
    interface_port = host_extract_port(big_buffer + 19);
    interface_address = string_copy(big_buffer + 19);
    }

  else if (Ustrncmp(big_buffer, "-host_auth", 10) == 0)
    sender_host_authenticated = string_copy(big_buffer + 11);
  else if (Ustrncmp(big_buffer, "-host_name", 10) == 0)
    sender_host_name = string_copy(big_buffer + 11);
  else if (Ustrncmp(big_buffer, "-helo_name", 10) == 0)
    sender_helo_name = string_copy(big_buffer + 11);
  else if (Ustrncmp(big_buffer, "-ident", 6) == 0)
    sender_ident = string_copy(big_buffer + 7);
  else if (Ustrncmp(big_buffer, "-received_protocol", 18) == 0)
    received_protocol = string_copy(big_buffer + 19);
  else if (Ustrncmp(big_buffer, "-N", 2) == 0)
    dont_deliver = TRUE;

  /* To allow new versions of Exim that add additional flags to interwork
  with older versions that do not understand them, just ignore any flagged
  lines that we don't recognize. Otherwise it wouldn't be possible to back
  off a new version that left new-style flags written on the spool. That's
  why the following line is commented out. */

    /* else goto SPOOL_FORMAT_ERROR; */
  }

/* Build sender_fullhost if required */

#ifndef COMPILE_UTILITY
host_build_sender_fullhost();
#endif  /* COMPILE_UTILITY */

#ifndef COMPILE_UTILITY
DEBUG(D_deliver)
  debug_printf("sender_local=%d ident=%s\n", sender_local,
    (sender_ident == NULL)? US"unset" : sender_ident);
#endif  /* COMPILE_UTILITY */

/* We now have the tree of addresses NOT to deliver to, or a line
containing "XX", indicating no tree. */

if (Ustrncmp(big_buffer, "XX\n", 3) != 0 &&
  !read_nonrecipients_tree(&tree_nonrecipients, f, big_buffer, big_buffer_size))
    goto SPOOL_FORMAT_ERROR;

#ifndef COMPILE_UTILITY
DEBUG(D_deliver)
  {
  debug_printf("Non-recipients:\n");
  debug_print_tree(tree_nonrecipients);
  }
#endif  /* COMPILE_UTILITY */

/* After reading the tree, the next line has not yet been read into the
buffer. It contains the count of recipients which follow on separate lines. */

if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
if (sscanf(CS big_buffer, "%d", &rcount) != 1) goto SPOOL_FORMAT_ERROR;

#ifndef COMPILE_UTILITY
DEBUG(D_deliver) debug_printf("recipients_count=%d\n", rcount);
#endif  /* COMPILE_UTILITY */

recipients_list_max = rcount;
recipients_list = store_get(rcount * sizeof(recipient_item));

for (recipients_count = 0; recipients_count < rcount; recipients_count++)
  {
  int nn;
  int pno = -1;
  uschar *errors_to = NULL;
  uschar *p;

  if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
  nn = Ustrlen(big_buffer);
  if (nn < 2) goto SPOOL_FORMAT_ERROR;

  /* Remove the newline; this terminates the address if there is no additional
  data on the line. */

  p = big_buffer + nn - 1;
  *p-- = 0;

  /* Look back from the end of the line for digits and special terminators.
  Since an address must end with a domain, we can tell that extra data is
  present by the presence of the terminator, which is always some character
  that cannot exist in a domain. (If I'd thought of the need for additional
  data early on, I'd have put it at the start, with the address at the end. As
  it is, we have to operate backwards. Addresses are permitted to contain
  spaces, you see.)

  This code has to cope with various versions of this data that have evolved
  over time. In all cases, the line might just contain an address, with no
  additional data. Otherwise, the possibilities are as follows:

  Exim 3 type:       <address><space><digits>,<digits>,<digits>

    The second set of digits is the parent number for one_time addresses. The
    other values were remnants of earlier experiments that were abandoned.

  Exim 4 first type: <address><space><digits>

    The digits are the parent number for one_time addresses.

  Exim 4 new type:   <address><space><data>#<type bits>

    The type bits indicate what the contents of the data are.

    Bit 01 indicates that, reading from right to left, the data
      ends with <errors_to address><space><len>,<pno> where pno is
      the parent number for one_time addresses, and len is the length
      of the errors_to address (zero meaning none).
   */

  while (isdigit(*p)) p--;

  /* Handle Exim 3 spool files */

  if (*p == ',')
    {
    int dummy;
    while (isdigit(*(--p)) || *p == ',');
    if (*p == ' ')
      {
      *p++ = 0;
      sscanf(CS p, "%d,%d", &dummy, &pno);
      }
    }

  /* Handle early Exim 4 spool files */

  else if (*p == ' ')
    {
    *p++ = 0;
    sscanf(CS p, "%d", &pno);
    }

  /* Handle current format Exim 4 spool files */

  else if (*p == '#')
    {
    int flags;
    sscanf(CS p+1, "%d", &flags);

    if ((flags & 0x01) != 0)      /* one_time data exists */
      {
      int len;
      while (isdigit(*(--p)) || *p == ',' || *p == '-');
      sscanf(CS p+1, "%d,%d", &len, &pno);
      *p = 0;
      if (len > 0)
        {
        p -= len;
        errors_to = string_copy(p);
        }
      }

    *(--p) = 0;   /* Terminate address */
    }

  recipients_list[recipients_count].address = string_copy(big_buffer);
  recipients_list[recipients_count].pno = pno;
  recipients_list[recipients_count].errors_to = errors_to;
  }

/* The remainder of the spool header file contains the headers for the message,
separated off from the previous data by a blank line. Each header is preceded
by a count of its length and either a certain letter (for various identified
headers), space (for a miscellaneous live header) or an asterisk (for a header
that has been rewritten). Count the Received: headers. We read the headers
always, in order to check on the format of the file, but only create a header
list if requested to do so. */

inheader = TRUE;
if (Ufgets(big_buffer, big_buffer_size, f) == NULL) goto SPOOL_READ_ERROR;
if (big_buffer[0] != '\n') goto SPOOL_FORMAT_ERROR;

while ((n = fgetc(f)) != EOF)
  {
  header_line *h;
  uschar flag[4];
  int i;

  if (!isdigit(n)) goto SPOOL_FORMAT_ERROR;
  ungetc(n, f);
  fscanf(f, "%d%c ", &n, flag);
  if (flag[0] != '*') message_size += n;  /* Omit non-transmitted headers */

  if (read_headers)
    {
    h = store_get(sizeof(header_line));
    h->next = NULL;
    h->type = flag[0];
    h->slen = n;
    h->text = store_get(n+1);

    if (h->type == htype_received) received_count++;

    if (header_list == NULL) header_list = h;
      else header_last->next = h;
    header_last = h;

    for (i = 0; i < n; i++)
      {
      int c = fgetc(f);
      if (c == 0 || c == EOF) goto SPOOL_FORMAT_ERROR;
      if (c == '\n' && h->type != htype_old) message_linecount++;
      h->text[i] = c;
      }
    h->text[i] = 0;
    }

  /* Not requiring header data, just skip through the bytes */

  else for (i = 0; i < n; i++)
    {
    int c = fgetc(f);
    if (c == 0 || c == EOF) goto SPOOL_FORMAT_ERROR;
    }
  }

/* We have successfully read the data in the header file. Update the message
line count by adding the body linecount to the header linecount. Close the file
and give a positive response. */

#ifndef COMPILE_UTILITY
DEBUG(D_deliver) debug_printf("body_linecount=%d message_linecount=%d\n",
  body_linecount, message_linecount);
#endif  /* COMPILE_UTILITY */

message_linecount += body_linecount;

fclose(f);
return spool_read_OK;


/* There was an error reading the spool or there was missing data,
or there was a format error. A "read error" with no errno means an
unexpected EOF, which we treat as a format error. */

SPOOL_READ_ERROR:
if (errno != 0)
  {
  n = errno;

  #ifndef COMPILE_UTILITY
  DEBUG(D_any) debug_printf("Error while reading spool file %s\n", name);
  #endif  /* COMPILE_UTILITY */

  fclose(f);
  errno = n;
  return inheader? spool_read_hdrerror : spool_read_enverror;
  }

SPOOL_FORMAT_ERROR:

#ifndef COMPILE_UTILITY
DEBUG(D_any) debug_printf("Format error in spool file %s\n", name);
#endif  /* COMPILE_UTILITY */

fclose(f);
errno = ERRNO_SPOOLFORMAT;
return inheader? spool_read_hdrerror : spool_read_enverror;
}

/* End of spool_in.c */
