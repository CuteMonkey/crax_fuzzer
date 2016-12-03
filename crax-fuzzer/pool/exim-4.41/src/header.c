/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */


#include "exim.h"


/*************************************************
*            Add new header on end of chain      *
*************************************************/

/* The header_last variable points to the last header during message reception
and delivery; otherwise it is NULL. We add new headers only when header_last is
not NULL. The function may get called sometimes when it is NULL (e.g. during
address verification where rewriting options exist). When called from a filter,
there may be multiple header lines in a single string.

Arguments:
  type      Exim header type character
  format    sprintf format
  ...       arguments for the format

Returns:    nothing
*/

void
header_add(int type, char *format, ...)
{
header_line *new;
va_list ap;
uschar *p, *q;
uschar buffer[HEADER_ADD_BUFFER_SIZE];

if (header_last == NULL) return;

va_start(ap, format);
if (!string_vformat(buffer, sizeof(buffer), format, ap))
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "string too long in header_add: %.100s ...",
    buffer);
va_end(ap);

/* Loop for multiple header lines, taking care about continuations */

for (p = q = buffer; *p != 0; )
  {
  for (;;)
    {
    q = Ustrchr(q, '\n');
    if (q == NULL) q = p + Ustrlen(p);
    if (*(++q) != ' ' && *q != '\t') break;
    }

  new = store_get(sizeof(header_line));
  new->text = string_copyn(p, q - p);

  new->slen = q - p;
  new->next = NULL;
  new->type = type;

  header_last->next = new;
  header_last = new;

  p = q;
  }
}


/*************************************************
*          Check the name of a header            *
*************************************************/

/* This function scans a table of header field names that Exim recognizes, and
returns the identification of a match. If "resent" is true, the header is known
to start with "resent-". In that case, the function matches only those fields
that are allowed to appear with resent- in front of them.

Arguments:
  h             points to the header line
  is_resent     TRUE if the name starts "Resent-"

Returns:        One of the htype_ enum values, identifying the header
*/

int
header_checkname(header_line *h, BOOL is_resent)
{
uschar *text = h->text;
header_name *bot = header_names;
header_name *top = header_names + header_names_size;

if (is_resent) text += 7;

while (bot < top)
  {
  header_name *mid = bot + (top - bot)/2;
  int c = strncmpic(text, mid->name, mid->len);

  if (c == 0)
    {
    uschar *s = text + mid->len;
    while (isspace(*s)) s++;
    if (*s == ':')
      return (!is_resent || mid->allow_resent)? mid->htype : htype_other;
    c = 1;
    }

  if (c > 0) bot = mid + 1; else top = mid;
  }

return htype_other;
}

/* End of header.c */
