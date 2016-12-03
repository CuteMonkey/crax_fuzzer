/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

#include "../exim.h"
#include "rf_functions.h"


/*************************************************
*      Get additional headers for a router       *
*************************************************/

/* This function is called by both routers to sort out the additional headers
and header remove list for a particular address.

Arguments:
  addr           the input address
  rblock         the router instance
  extra_headers  points to where to hang the header chain
  remove_headers points to where to hang the remove list

Returns:         OK if no problem
                 DEFER if expanding a string caused a deferment
                   or a big disaster (e.g. expansion failure)
*/

int
rf_get_munge_headers(address_item *addr, router_instance *rblock,
  header_line **extra_headers, uschar **remove_headers)
{
/* Default is to retain existing headers */

*extra_headers = addr->p.extra_headers;

if (rblock->extra_headers != NULL)
  {
  header_line *h;
  uschar *s = expand_string(rblock->extra_headers);

  if (s == NULL)
    {
    if (!expand_string_forcedfail)
      {
      addr->message = string_sprintf("%s router failed to expand \"%s\": %s",
        rblock->name, rblock->extra_headers, expand_string_message);
      return DEFER;
      }
    }

  /* Expand succeeded. Put extra header at the start of the chain because
  further down it may point to headers from other routers, which may be
  shared with other addresses. The output function outputs them in reverse
  order. */

  else
    {
    uschar *addnl = US"";
    int slen = Ustrlen(s);
    if (slen > 0)
      {
      if (s[slen-1] != '\n')
        {
        addnl = US"\n";
        slen++;
        }
      h = store_get(sizeof(header_line));
      h->text = string_sprintf("%s%s", s, addnl);
      h->next = addr->p.extra_headers;
      h->type = htype_other;
      h->slen = slen;
      *extra_headers = h;
      }
    }
  }

/* Default is to retain existing removes */

*remove_headers = addr->p.remove_headers;

if (rblock->remove_headers != NULL)
  {
  uschar *s = expand_string(rblock->remove_headers);
  if (s == NULL)
    {
    if (!expand_string_forcedfail)
      {
      addr->message = string_sprintf("%s router failed to expand \"%s\": %s",
        rblock->name, rblock->remove_headers, expand_string_message);
      return DEFER;
      }
    }
  else if (*s != 0)
    {
    if (addr->p.remove_headers == NULL)
      *remove_headers = s;
    else
      *remove_headers = string_sprintf("%s : %s", addr->p.remove_headers, s);
    }
  }

return OK;
}

/* End of rf_get_munge_headers.c */
