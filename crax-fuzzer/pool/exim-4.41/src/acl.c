/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* Code for handling Access Control Lists (ACLs) */

#include "exim.h"


/* Default callout timeout */

#define CALLOUT_TIMEOUT_DEFAULT 30

/* ACL verb codes - keep in step with the table of verbs that follows */

enum { ACL_ACCEPT, ACL_DEFER, ACL_DENY, ACL_DISCARD, ACL_DROP, ACL_REQUIRE,
       ACL_WARN };

/* ACL verbs */

static uschar *verbs[] =
  { US"accept", US"defer", US"deny", US"discard", US"drop", US"require",
    US"warn" };

/* For each verb, the condition for which "message" is used */

static int msgcond[] = { FAIL, OK, OK, FAIL, OK, FAIL, OK };

/* ACL condition and modifier codes - keep in step with the table that
follows. */

enum { ACLC_ACL, ACLC_AUTHENTICATED, ACLC_CONDITION, ACLC_CONTROL, ACLC_DELAY,
  ACLC_DNSLISTS, ACLC_DOMAINS, ACLC_ENCRYPTED, ACLC_ENDPASS, ACLC_HOSTS,
  ACLC_LOCAL_PARTS, ACLC_LOG_MESSAGE, ACLC_LOGWRITE, ACLC_MESSAGE,
  ACLC_RECIPIENTS, ACLC_SENDER_DOMAINS, ACLC_SENDERS, ACLC_SET, ACLC_VERIFY };

/* ACL conditions/modifiers: "delay", "control", "endpass", "message",
"log_message", "logwrite", and "set" are modifiers that look like conditions
but always return TRUE. They are used for their side effects. */

static uschar *conditions[] = { US"acl", US"authenticated", US"condition",
  US"control", US"delay", US"dnslists", US"domains", US"encrypted",
  US"endpass", US"hosts", US"local_parts", US"log_message", US"logwrite",
  US"message", US"recipients", US"sender_domains", US"senders", US"set",
  US"verify" };

/* Flags to indicate for which conditions /modifiers a string expansion is done
at the outer level. In the other cases, expansion already occurs in the
checking functions. */

static uschar cond_expand_at_top[] = {
  TRUE,    /* acl */
  FALSE,   /* authenticated */
  TRUE,    /* condition */
  TRUE,    /* control */
  TRUE,    /* delay */
  TRUE,    /* dnslists */
  FALSE,   /* domains */
  FALSE,   /* encrypted */
  TRUE,    /* endpass */
  FALSE,   /* hosts */
  FALSE,   /* local_parts */
  TRUE,    /* log_message */
  TRUE,    /* logwrite */
  TRUE,    /* message */
  FALSE,   /* recipients */
  FALSE,   /* sender_domains */
  FALSE,   /* senders */
  TRUE,    /* set */
  TRUE     /* verify */
};

/* Flags to identify the modifiers */

static uschar cond_modifiers[] = {
  FALSE,   /* acl */
  FALSE,   /* authenticated */
  FALSE,   /* condition */
  TRUE,    /* control */
  TRUE,    /* delay */
  FALSE,   /* dnslists */
  FALSE,   /* domains */
  FALSE,   /* encrypted */
  TRUE,    /* endpass */
  FALSE,   /* hosts */
  FALSE,   /* local_parts */
  TRUE,    /* log_message */
  TRUE,    /* log_write */
  TRUE,    /* message */
  FALSE,   /* recipients */
  FALSE,   /* sender_domains */
  FALSE,   /* senders */
  TRUE,    /* set */
  FALSE    /* verify */
};

/* Bit map of which conditions are not allowed at certain times. For each
condition, there's a bitmap of dis-allowed times. */

static unsigned int cond_forbids[] = {
  0,                                               /* acl */
  (1<<ACL_WHERE_NOTSMTP)|(1<<ACL_WHERE_CONNECT)|   /* authenticated */
    (1<<ACL_WHERE_HELO),
  0,                                               /* condition */

  (1<<ACL_WHERE_AUTH)|(1<<ACL_WHERE_CONNECT)|      /* control */
    (1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_STARTTLS)|(ACL_WHERE_VRFY),

  0,                                               /* delay */
  (1<<ACL_WHERE_NOTSMTP),                          /* dnslists */

  (1<<ACL_WHERE_NOTSMTP)|(1<<ACL_WHERE_AUTH)|      /* domains */
    (1<<ACL_WHERE_CONNECT)|(1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_DATA)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_MAIL)|(1<<ACL_WHERE_STARTTLS)|
    (1<<ACL_WHERE_VRFY),

  (1<<ACL_WHERE_NOTSMTP)|(1<<ACL_WHERE_CONNECT)|   /* encrypted */
    (1<<ACL_WHERE_HELO),
  0,                                               /* endpass */
  (1<<ACL_WHERE_NOTSMTP),                          /* hosts */

  (1<<ACL_WHERE_NOTSMTP)|(1<<ACL_WHERE_AUTH)|      /* local_parts */
    (1<<ACL_WHERE_CONNECT)|(1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_DATA)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_MAIL)|(1<<ACL_WHERE_STARTTLS)|
    (1<<ACL_WHERE_VRFY),

  0,                                               /* log_message */
  0,                                               /* logwrite */
  0,                                               /* message */

  (1<<ACL_WHERE_NOTSMTP)|(1<<ACL_WHERE_AUTH)|      /* recipients */
    (1<<ACL_WHERE_CONNECT)|(1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_DATA)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_MAIL)|(1<<ACL_WHERE_STARTTLS)|
    (1<<ACL_WHERE_VRFY),

  (1<<ACL_WHERE_AUTH)|(1<<ACL_WHERE_CONNECT)|      /* sender_domains */
    (1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_STARTTLS)|(1<<ACL_WHERE_VRFY),

  (1<<ACL_WHERE_AUTH)|(1<<ACL_WHERE_CONNECT)|      /* senders */
    (1<<ACL_WHERE_HELO)|
    (1<<ACL_WHERE_MAILAUTH)|
    (1<<ACL_WHERE_ETRN)|(1<<ACL_WHERE_EXPN)|
    (1<<ACL_WHERE_STARTTLS)|(1<<ACL_WHERE_VRFY),

  0,                                               /* set */

  /* Certain types of verify are always allowed, so we let it through
  always and check in the verify function itself */

  0                                                /* verify */

};


/* Enable recursion between acl_check_internal() and acl_check_condition() */

static int acl_check_internal(int, address_item *, uschar *, int, uschar **,
         uschar **);


/*************************************************
*         Pick out name from list                *
*************************************************/

/* Use a binary chop method

Arguments:
  name        name to find
  list        list of names
  end         size of list

Returns:      offset in list, or -1 if not found
*/

static int
acl_checkname(uschar *name, uschar **list, int end)
{
int start = 0;

while (start < end)
  {
  int mid = (start + end)/2;
  int c = Ustrcmp(name, list[mid]);
  if (c == 0) return mid;
  if (c < 0) end = mid; else start = mid + 1;
  }

return -1;
}


/*************************************************
*            Read and parse one ACL              *
*************************************************/

/* This function is called both from readconf in order to parse the ACLs in the
configuration file, and also when an ACL is encountered dynamically (e.g. as
the result of an expansion). It is given a function to call in order to
retrieve the lines of the ACL. This function handles skipping comments and
blank lines (where relevant).

Arguments:
  func        function to get next line of ACL
  error       where to put an error message

Returns:      pointer to ACL, or NULL
              NULL can be legal (empty ACL); in this case error will be NULL
*/

acl_block *
acl_read(uschar *(*func)(void), uschar **error)
{
acl_block *yield = NULL;
acl_block **lastp = &yield;
acl_block *this = NULL;
acl_condition_block *cond;
acl_condition_block **condp = NULL;
uschar *s;

*error = NULL;

while ((s = (*func)()) != NULL)
  {
  int v, c;
  BOOL negated = FALSE;
  uschar *saveline = s;
  uschar name[64];

  /* Conditions (but not verbs) are allowed to be negated by an initial
  exclamation mark. */

  while (isspace(*s)) s++;
  if (*s == '!')
    {
    negated = TRUE;
    s++;
    }

  /* Read the name of a verb or a condition, or the start of a new ACL */

  s = readconf_readname(name, sizeof(name), s);
  if (*s == ':')
    {
    if (negated || name[0] == 0)
      {
      *error = string_sprintf("malformed ACL name in \"%s\"", saveline);
      return NULL;
      }
    break;
    }

  /* If a verb is unrecognized, it may be another condition or modifier that
  continues the previous verb. */

  v = acl_checkname(name, verbs, sizeof(verbs)/sizeof(char *));
  if (v < 0)
    {
    if (this == NULL)
      {
      *error = string_sprintf("unknown ACL verb in \"%s\"", saveline);
      return NULL;
      }
    }

  /* New verb */

  else
    {
    if (negated)
      {
      *error = string_sprintf("malformed ACL line \"%s\"", saveline);
      return NULL;
      }
    this = store_get(sizeof(acl_block));
    *lastp = this;
    lastp = &(this->next);
    this->next = NULL;
    this->verb = v;
    this->condition = NULL;
    condp = &(this->condition);
    if (*s == 0) continue;               /* No condition on this line */
    if (*s == '!')
      {
      negated = TRUE;
      s++;
      }
    s = readconf_readname(name, sizeof(name), s);  /* Condition name */
    }

  /* Handle a condition or modifier. */

  c = acl_checkname(name, conditions, sizeof(conditions)/sizeof(char *));
  if (c < 0)
    {
    *error = string_sprintf("unknown ACL condition/modifier in \"%s\"",
      saveline);
    return NULL;
    }

  /* The modifiers may not be negated */

  if (negated && cond_modifiers[c])
    {
    *error = string_sprintf("ACL error: negation is not allowed with "
      "\"%s\"", conditions[c]);
    return NULL;
    }

  /* ENDPASS may occur only with ACCEPT or DISCARD. */

  if (c == ACLC_ENDPASS &&
      this->verb != ACL_ACCEPT &&
      this->verb != ACL_DISCARD)
    {
    *error = string_sprintf("ACL error: \"%s\" is not allowed with \"%s\"",
      conditions[c], verbs[this->verb]);
    return NULL;
    }

  cond = store_get(sizeof(acl_condition_block));
  cond->next = NULL;
  cond->type = c;
  cond->u.negated = negated;

  *condp = cond;
  condp = &(cond->next);

  /* The "set" modifier is different in that its argument is "name=value"
  rather than just a value, and we can check the validity of the name, which
  gives us a variable number to insert into the data block. */

  if (c == ACLC_SET)
    {
    if (Ustrncmp(s, "acl_", 4) != 0 || (s[4] != 'c' && s[4] != 'm') ||
        !isdigit(s[5]) || (!isspace(s[6]) && s[6] != '='))
      {
      *error = string_sprintf("unrecognized name after \"set\" in ACL "
        "modifier \"set %s\"", s);
      return NULL;
      }

    cond->u.varnumber = s[5] - '0';
    if (s[4] == 'm') cond->u.varnumber += ACL_C_MAX;
    s += 6;
    while (isspace(*s)) s++;
    }

  /* For "set", we are now positioned for the data. For the others, only
  "endpass" has no data */

  if (c != ACLC_ENDPASS)
    {
    if (*s++ != '=')
      {
      *error = string_sprintf("\"=\" missing after ACL \"%s\" %s", name,
        cond_modifiers[c]? US"modifier" : US"condition");
      return NULL;
      }
    while (isspace(*s)) s++;
    cond->arg = string_copy(s);
    }
  }

return yield;
}



/*************************************************
*               Handle warnings                  *
*************************************************/

/* This function is called when a WARN verb's conditions are true. It adds to
the message's headers, and/or writes information to the log. In each case, this
only happens once (per message for headers, per connection for log).

Arguments:
  where          ACL_WHERE_xxxx indicating which ACL this is
  user_message   message for adding to headers
  log_message    message for logging, if different

Returns:         nothing
*/

static void
acl_warn(int where, uschar *user_message, uschar *log_message)
{
int hlen;
uschar *s;

if (log_message != NULL && log_message != user_message)
  {
  uschar *text;
  string_item *logged;

  text = string_sprintf("%s Warning: %s",  host_and_ident(TRUE),
    string_printing(log_message));

  /* If a sender verification has failed, and the log message is "sender verify
  failed", add the failure message. */

  if (sender_verified_failed != NULL &&
      sender_verified_failed->message != NULL &&
      strcmpic(log_message, US"sender verify failed") == 0)
    text = string_sprintf("%s: %s", text, sender_verified_failed->message);

  /* Search previously logged warnings. They are kept in malloc store so they
  can be freed at the start of a new message. */

  for (logged = acl_warn_logged; logged != NULL; logged = logged->next)
    if (Ustrcmp(logged->text, text) == 0) break;

  if (logged == NULL)
    {
    int length = Ustrlen(text) + 1;
    log_write(0, LOG_MAIN, "%s", text);
    logged = store_malloc(sizeof(string_item) + length);
    logged->text = (uschar *)logged + sizeof(string_item);
    memcpy(logged->text, text, length);
    logged->next = acl_warn_logged;
    acl_warn_logged = logged;
    }
  }

/* If there's no user message, we are done. */

if (user_message == NULL) return;

/* If this isn't a message ACL, we can't do anything with a user message.
Log an error. */

if (where != ACL_WHERE_MAIL && where != ACL_WHERE_RCPT &&
    where != ACL_WHERE_DATA && where != ACL_WHERE_NOTSMTP)
  {
  log_write(0, LOG_MAIN|LOG_PANIC, "ACL \"warn\" with \"message\" setting "
    "found in a non-message (%s) ACL: cannot specify header lines here: "
    "message ignored", acl_wherenames[where]);
  return;
  }

/* If the user message isn't a valid header, turn it into one by adding
X-ACL-Warn: at the front. */

s = user_message;

while (*s != 0)
  {
  if (*s == ':' || !isgraph(*s)) break;
  s++;
  }

if (*s != ':')
  user_message = string_sprintf("X-ACL-Warn: %s", user_message);

hlen = Ustrlen(user_message);
if (hlen > 0)
  {
  uschar *text, *p, *q;

  /* Add newline if not present */

  text = ((user_message)[hlen-1] == '\n')? user_message :
    string_sprintf("%s\n", user_message);

  /* Loop for multiple header lines, taking care about continuations */

  for (p = q = text; *p != 0; )
    {
    header_line **hptr = &acl_warn_headers;

    /* Find next header line within the string */

    for (;;)
      {
      q = Ustrchr(q, '\n');
      if (*(++q) != ' ' && *q != '\t') break;
      }

    hlen = q - p;

    /* See if this line has already been added */

    while (*hptr != NULL)
      {
      if (Ustrncmp((*hptr)->text, p, hlen) == 0) break;
      hptr = &((*hptr)->next);
      }

    /* Add if not previously present */

    if (*hptr == NULL)
      {
      header_line *h = store_get(sizeof(header_line));
      h->text = string_copyn(p, hlen);
      h->next = NULL;
      h->type = htype_other;
      h->slen = hlen;

      *hptr = h;
      hptr = &(h->next);
      }

    /* Advance for next header line within the string */

    p = q;
    }
  }
}



/*************************************************
*         Verify and check reverse DNS           *
*************************************************/

/* Called from acl_verify() below. We look up the host name(s) of the client IP
address if this has not yet been done. The host_name_lookup() function checks
that one of these names resolves to an address list that contains the client IP
address, so we don't actually have to do the check here.

Arguments:
  user_msgptr  pointer for user message
  log_msgptr   pointer for log message

Returns:       OK        verification condition succeeded
               FAIL      verification failed
               DEFER     there was a problem verifying
*/

static int
acl_verify_reverse(uschar **user_msgptr, uschar **log_msgptr)
{
int rc;

user_msgptr = user_msgptr;  /* stop compiler warning */

/* Previous success */

if (sender_host_name != NULL) return OK;

/* Previous failure */

if (host_lookup_failed)
  {
  *log_msgptr = string_sprintf("host lookup failed%s", host_lookup_msg);
  return FAIL;
  }

/* Need to do a lookup */

HDEBUG(D_acl)
  debug_printf("looking up host name to force name/address consistency check\n");

if ((rc = host_name_lookup()) != OK)
  {
  *log_msgptr = (rc == DEFER)?
    US"host lookup deferred for reverse lookup check"
    :
    string_sprintf("host lookup failed for reverse lookup check%s",
      host_lookup_msg);
  return rc;    /* DEFER or FAIL */
  }

host_build_sender_fullhost();
return OK;
}



/*************************************************
*     Handle verification (address & other)      *
*************************************************/

/* This function implements the "verify" condition. It is called when
encountered in any ACL, because some tests are almost always permitted. Some
just don't make sense, and always fail (for example, an attempt to test a host
lookup for a non-TCP/IP message). Others are restricted to certain ACLs.

Arguments:
  where        where called from
  addr         the recipient address that the ACL is handling, or NULL
  arg          the argument of "verify"
  user_msgptr  pointer for user message
  log_msgptr   pointer for log message
  basic_errno  where to put verify errno

Returns:       OK        verification condition succeeded
               FAIL      verification failed
               DEFER     there was a problem verifying
               ERROR     syntax error
*/

static int
acl_verify(int where, address_item *addr, uschar *arg,
  uschar **user_msgptr, uschar **log_msgptr, int *basic_errno)
{
int sep = '/';
int callout = -1;
int verify_options = 0;
int rc;
BOOL verify_header_sender = FALSE;
BOOL defer_ok = FALSE;
BOOL callout_defer_ok = FALSE;
BOOL no_details = FALSE;
address_item *sender_vaddr = NULL;
uschar *verify_sender_address = NULL;
uschar *list = arg;
uschar *ss = string_nextinlist(&list, &sep, big_buffer, big_buffer_size);

if (ss == NULL) goto BAD_VERIFY;

/* Handle name/address consistency verification in a separate function. */

if (strcmpic(ss, US"reverse_host_lookup") == 0)
  {
  if (sender_host_address == NULL) return OK;
  return acl_verify_reverse(user_msgptr, log_msgptr);
  }

/* TLS certificate verification is done at STARTTLS time; here we just
test whether it was successful or not. (This is for optional verification; for
mandatory verification, the connection doesn't last this long.) */

if (strcmpic(ss, US"certificate") == 0)
  {
  if (tls_certificate_verified) return OK;
  *user_msgptr = US"no verified certificate";
  return FAIL;
  }

/* We can test the result of optional HELO verification */

if (strcmpic(ss, US"helo") == 0) return helo_verified? OK : FAIL;

/* Handle header verification options - permitted only after DATA or a non-SMTP
message. */

if (strncmpic(ss, US"header_", 7) == 0)
  {
  if (where != ACL_WHERE_DATA && where != ACL_WHERE_NOTSMTP)
    {
    *log_msgptr = string_sprintf("cannot check header contents in ACL for %s "
      "(only possible in ACL for DATA)", acl_wherenames[where]);
    return ERROR;
    }

  /* Check that all relevant header lines have the correct syntax. If there is
  a syntax error, we return details of the error to the sender if configured to
  send out full details. (But a "message" setting on the ACL can override, as
  always). */

  if (strcmpic(ss+7, US"syntax") == 0)
    {
    int rc = verify_check_headers(log_msgptr);
    if (rc != OK && smtp_return_error_details && *log_msgptr != NULL)
      *user_msgptr = string_sprintf("Rejected after DATA: %s", *log_msgptr);
    return rc;
    }

  /* Check that there is at least one verifiable sender address in the relevant
  header lines. This can be followed by callout and defer options, just like
  sender and recipient. */

  else if (strcmpic(ss+7, US"sender") == 0) verify_header_sender = TRUE;

  /* Unknown verify argument starting with "header_" */

  else goto BAD_VERIFY;
  }

/* Otherwise, first item in verify argument must be "sender" or "recipient".
In the case of a sender, this can optionally be followed by an address to use
in place of the actual sender (rare special-case requirement). */

else if (strncmpic(ss, US"sender", 6) == 0)
  {
  uschar *s = ss + 6;
  if (where != ACL_WHERE_NOTSMTP &&
      where != ACL_WHERE_MAIL &&
      where != ACL_WHERE_RCPT &&
      where != ACL_WHERE_DATA)
    {
    *log_msgptr = string_sprintf("cannot verify sender in ACL for %s "
      "(only possible for MAIL, RCPT, or DATA)", acl_wherenames[where]);
    return ERROR;
    }
  if (*s == 0)
    verify_sender_address = sender_address;
  else
    {
    while (isspace(*s)) s++;
    if (*s++ != '=') goto BAD_VERIFY;
    while (isspace(*s)) s++;
    verify_sender_address = string_copy(s);
    }
  }
else
  {
  if (strcmpic(ss, US"recipient") != 0) goto BAD_VERIFY;
  if (addr == NULL)
    {
    *log_msgptr = string_sprintf("cannot verify recipient in ACL for %s "
      "(only possible for RCPT)", acl_wherenames[where]);
    return ERROR;
    }
  }

/* Remaining items are optional */

while ((ss = string_nextinlist(&list, &sep, big_buffer, big_buffer_size))
      != NULL)
  {
  if (strcmpic(ss, US"defer_ok") == 0) defer_ok = TRUE;
  else if (strcmpic(ss, US"no_details") == 0) no_details = TRUE;

  /* These two old options are left for backwards compatibility */

  else if (strcmpic(ss, US"callout_defer_ok") == 0)
    {
    callout_defer_ok = TRUE;
    if (callout == -1) callout = CALLOUT_TIMEOUT_DEFAULT;
    }

  else if (strcmpic(ss, US"check_postmaster") == 0)
     {
     verify_options |= vopt_callout_postmaster;
     if (callout == -1) callout = CALLOUT_TIMEOUT_DEFAULT;
     }

  /* The callout option has a number of sub-options, comma separated */

  else if (strncmpic(ss, US"callout", 7) == 0)
    {
    callout = CALLOUT_TIMEOUT_DEFAULT;
    ss += 7;
    if (*ss != 0)
      {
      while (isspace(*ss)) ss++;
      if (*ss++ == '=')
        {
        int optsep = ',';
        uschar *opt;
        uschar buffer[256];
        while (isspace(*ss)) ss++;
        while ((opt = string_nextinlist(&ss, &optsep, buffer, sizeof(buffer)))
              != NULL)
          {
          if (strcmpic(opt, US"defer_ok") == 0) callout_defer_ok = TRUE;
          else if (strcmpic(opt, US"no_cache") == 0)
             verify_options |= vopt_callout_no_cache;
          else if (strcmpic(opt, US"random") == 0)
             verify_options |= vopt_callout_random;
          else if (strcmpic(opt, US"postmaster") == 0)
             verify_options |= vopt_callout_postmaster;
          else if (strcmpic(opt, US"use_sender") == 0)
             verify_options |= vopt_callout_recipsender;
          else if (strcmpic(opt, US"use_postmaster") == 0)
             verify_options |= vopt_callout_recippmaster;
          else
            {
            callout = readconf_readtime(opt, 0, FALSE);
            if (callout < 0)
              {
              *log_msgptr = string_sprintf("bad time value in ACL condition "
                "\"verify %s\"", arg);
              return ERROR;
              }
            }
          }
        }
      else
        {
        *log_msgptr = string_sprintf("'=' expected after \"callout\" in "
          "ACL condition \"%s\"", arg);
        return ERROR;
        }
      }
    }

  /* Option not recognized */

  else
    {
    *log_msgptr = string_sprintf("unknown option \"%s\" in ACL "
      "condition \"verify %s\"", ss, arg);
    return ERROR;
    }
  }

if ((verify_options & (vopt_callout_recipsender|vopt_callout_recippmaster)) ==
      (vopt_callout_recipsender|vopt_callout_recippmaster))
  {
  *log_msgptr = US"only one of use_sender and use_postmaster can be set "
    "for a recipient callout";
  return ERROR;
  }

/* Handle sender-in-header verification. Default the user message to the log
message if giving out verification details. */

if (verify_header_sender)
  {
  rc = verify_check_header_address(user_msgptr, log_msgptr, callout);
  if (smtp_return_error_details)
    {
    if (*user_msgptr == NULL && *log_msgptr != NULL)
      *user_msgptr = string_sprintf("Rejected after DATA: %s", *log_msgptr);
    if (rc == DEFER) acl_temp_details = TRUE;
    }
  }

/* Handle a sender address. The default is to verify *the* sender address, but
optionally a different address can be given, for special requirements. If the
address is empty, we are dealing with a bounce message that has no sender, so
we cannot do any checking. If the real sender address gets rewritten during
verification (e.g. DNS widening), set the flag to stop it being rewritten again
during message reception.

A list of verified "sender" addresses is kept to try to avoid doing to much
work repetitively when there are multiple recipients in a message and they all
require sender verification. However, when callouts are involved, it gets too
complicated because different recipients may require different callout options.
Therefore, we always do a full sender verify when any kind of callout is
specified. Caching elsewhere, for instance in the DNS resolver and in the
callout handling, should ensure that this is not terribly inefficient. */

else if (verify_sender_address != NULL)
  {
  if ((verify_options & (vopt_callout_recipsender|vopt_callout_recippmaster))
       != 0)
    {
    *log_msgptr = US"use_sender or use_postmaster cannot be used for a "
      "sender verify callout";
    return ERROR;
    }

  sender_vaddr = verify_checked_sender(verify_sender_address);
  if (sender_vaddr != NULL &&               /* Previously checked */
      callout <= 0)                         /* No callout needed this time */
    {
    /* If the "routed" flag is set, it means that routing worked before, so
    this check can give OK (the saved return code value, if set, belongs to a
    callout that was done previously). If the "routed" flag is not set, routing
    must have failed, so we use the saved return code. */

    if (testflag(sender_vaddr, af_verify_routed)) rc = OK; else
      {
      rc = sender_vaddr->special_action;
      *basic_errno = sender_vaddr->basic_errno;
      }
    HDEBUG(D_acl) debug_printf("using cached sender verify result\n");
    }

  /* Do a new verification, and cache the result. The cache is used to avoid
  verifying the sender multiple times for multiple RCPTs when callouts are not
  specified (see comments above).

  The cache is also used on failure to give details in response to the first
  RCPT that gets bounced for this reason. However, this can be suppressed by
  the no_details option, which sets the flag that says "this detail has already
  been sent". The cache normally contains just one address, but there may be
  more in esoteric circumstances. */

  else
    {
    BOOL routed = TRUE;
    sender_vaddr = deliver_make_addr(verify_sender_address, TRUE);
    if (no_details) setflag(sender_vaddr, af_sverify_told);
    if (verify_sender_address[0] != 0)
      {
      /* If this is the real sender address, save the unrewritten version
      for use later in receive. Otherwise, set a flag so that rewriting the
      sender in verify_address() does not update sender_address. */

      if (verify_sender_address == sender_address)
        sender_address_unrewritten = sender_address;
      else
        verify_options |= vopt_fake_sender;

      /* The recipient, qualify, and expn options are never set in
      verify_options. */

      rc = verify_address(sender_vaddr, NULL, verify_options, callout, &routed);

      HDEBUG(D_acl) debug_printf("----------- end verify ------------\n");

      if (rc == OK)
        {
        if (Ustrcmp(sender_vaddr->address, verify_sender_address) != 0)
          {
          DEBUG(D_acl) debug_printf("sender %s verified ok as %s\n",
            verify_sender_address, sender_vaddr->address);
          }
        else
          {
          DEBUG(D_acl) debug_printf("sender %s verified ok\n",
            verify_sender_address);
          }
        }
      else *basic_errno = sender_vaddr->basic_errno;
      }
    else rc = OK;  /* Null sender */

    /* Cache the result code */

    if (routed) setflag(sender_vaddr, af_verify_routed);
    if (callout > 0) setflag(sender_vaddr, af_verify_callout);
    sender_vaddr->special_action = rc;
    sender_vaddr->next = sender_verified_list;
    sender_verified_list = sender_vaddr;
    }
  }

/* A recipient address just gets a straightforward verify; again we must handle
the DEFER overrides. */

else
  {
  address_item addr2;

  /* We must use a copy of the address for verification, because it might
  get rewritten. */

  addr2 = *addr;
  rc = verify_address(&addr2, NULL, verify_options|vopt_is_recipient, callout,
    NULL);
  HDEBUG(D_acl) debug_printf("----------- end verify ------------\n");
  *log_msgptr = addr2.message;
  *user_msgptr = addr2.user_message;
  *basic_errno = addr2.basic_errno;

  /* Make $address_data visible */
  deliver_address_data = addr2.p.address_data;
  }

/* We have a result from the relevant test. Handle defer overrides first. */

if (rc == DEFER && (defer_ok ||
   (callout_defer_ok && *basic_errno == ERRNO_CALLOUTDEFER)))
  {
  HDEBUG(D_acl) debug_printf("verify defer overridden by %s\n",
    defer_ok? "defer_ok" : "callout_defer_ok");
  rc = OK;
  }

/* If we've failed a sender, set up a recipient message, and point
sender_verified_failed to the address item that actually failed. */

if (rc != OK && verify_sender_address != NULL)
  {
  if (rc != DEFER)
    {
    *log_msgptr = *user_msgptr = US"Sender verify failed";
    }
  else if (*basic_errno != ERRNO_CALLOUTDEFER)
    {
    *log_msgptr = *user_msgptr = US"Could not complete sender verify";
    }
  else
    {
    *log_msgptr = US"Could not complete sender verify callout";
    *user_msgptr = smtp_return_error_details? sender_vaddr->user_message :
      *log_msgptr;
    }

  sender_verified_failed = sender_vaddr;
  }

/* Verifying an address messes up the values of $domain and $local_part,
so reset them before returning if this is a RCPT ACL. */

if (addr != NULL)
  {
  deliver_domain = addr->domain;
  deliver_localpart = addr->local_part;
  }
return rc;

/* Syntax errors in the verify argument come here. */

BAD_VERIFY:
*log_msgptr = string_sprintf("expected \"sender[=address]\", \"recipient\", "
  "\"header_syntax\" or \"header_sender\" at start of ACL condition "
  "\"verify %s\"", arg);
return ERROR;
}




/*************************************************
*   Handle conditions/modifiers on an ACL item   *
*************************************************/

/* Called from acl_check() below.

Arguments:
  verb         ACL verb
  cb           ACL condition block - if NULL, result is OK
  where        where called from
  addr         the address being checked for RCPT, or NULL
  level        the nesting level
  epp          pointer to pass back TRUE if "endpass" encountered
                 (applies only to "accept" and "discard")
  user_msgptr  user message pointer
  log_msgptr   log message pointer
  basic_errno  pointer to where to put verify error

Returns:       OK        - all conditions are met
               DISCARD   - an "acl" condition returned DISCARD - only allowed
                             for "accept" or "discard" verbs
               FAIL      - at least one condition fails
               FAIL_DROP - an "acl" condition returned FAIL_DROP
               DEFER     - can't tell at the moment (typically, lookup defer,
                             but can be temporary callout problem)
               ERROR     - ERROR from nested ACL or expansion failure or other
                             error
*/

static int
acl_check_condition(int verb, acl_condition_block *cb, int where,
  address_item *addr, int level, BOOL *epp, uschar **user_msgptr,
  uschar **log_msgptr, int *basic_errno)
{
uschar *user_message = NULL;
uschar *log_message = NULL;
int rc = OK;

for (; cb != NULL; cb = cb->next)
  {
  uschar *arg;

  /* The message and log_message items set up messages to be used in
  case of rejection. They are expanded later. */

  if (cb->type == ACLC_MESSAGE)
    {
    user_message = cb->arg;
    continue;
    }

  if (cb->type == ACLC_LOG_MESSAGE)
    {
    log_message = cb->arg;
    continue;
    }

  /* The endpass "condition" just sets a flag to show it occurred. This is
  checked at compile time to be on an "accept" or "discard" item. */

  if (cb->type == ACLC_ENDPASS)
    {
    *epp = TRUE;
    continue;
    }

  /* For other conditions and modifiers, the argument is expanded now for some
  of them, but not for all, because expansion happens down in some lower level
  checking functions in some cases. */

  if (cond_expand_at_top[cb->type])
    {
    arg = expand_string(cb->arg);
    if (arg == NULL)
      {
      if (expand_string_forcedfail) continue;
      *log_msgptr = string_sprintf("failed to expand ACL string \"%s\": %s",
        cb->arg, expand_string_message);
      return search_find_defer? DEFER : ERROR;
      }
    }
  else arg = cb->arg;

  /* Show condition, and expanded condition if it's different */

  HDEBUG(D_acl)
    {
    int lhswidth = 0;
    debug_printf("check %s%s %n",
      (!cond_modifiers[cb->type] && cb->u.negated)? "!":"",
      conditions[cb->type], &lhswidth);

    if (cb->type == ACLC_SET)
      {
      int n = cb->u.varnumber;
      int t = (n < ACL_C_MAX)? 'c' : 'm';
      if (n >= ACL_C_MAX) n -= ACL_C_MAX;
      debug_printf("acl_%c%d ", t, n);
      lhswidth += 7;
      }

    debug_printf("= %s\n", cb->arg);

    if (arg != cb->arg)
      debug_printf("%.*s= %s\n", lhswidth,
      US"                             ", CS arg);
    }

  /* Check that this condition makes sense at this time */

  if ((cond_forbids[cb->type] & (1 << where)) != 0)
    {
    *log_msgptr = string_sprintf("cannot %s %s condition in %s ACL",
      cond_modifiers[cb->type]? "use" : "test",
      conditions[cb->type], acl_wherenames[where]);
    return ERROR;
    }

  /* Run the appropriate test for each condition, or take the appropriate
  action for the remaining modifiers. */

  switch(cb->type)
    {
    /* A nested ACL that returns "discard" makes sense only for an "accept" or
    "discard" verb. */

    case ACLC_ACL:
    rc = acl_check_internal(where, addr, arg, level+1, user_msgptr, log_msgptr);
    if (rc == DISCARD && verb != ACL_ACCEPT && verb != ACL_DISCARD)
      {
      *log_msgptr = string_sprintf("nested ACL returned \"discard\" for "
        "\"%s\" command (only allowed with \"accept\" or \"discard\")",
        verbs[verb]);
      return ERROR;
      }
    break;

    case ACLC_AUTHENTICATED:
    rc = (sender_host_authenticated == NULL)? FAIL :
      match_isinlist(sender_host_authenticated, &arg, 0, NULL, NULL, MCL_STRING,
        TRUE, NULL);
    break;

    case ACLC_CONDITION:
    if (Ustrspn(arg, "0123456789") == Ustrlen(arg))     /* Digits, or empty */
      rc = (Uatoi(arg) == 0)? FAIL : OK;
    else
      rc = (strcmpic(arg, US"no") == 0 ||
            strcmpic(arg, US"false") == 0)? FAIL :
           (strcmpic(arg, US"yes") == 0 ||
            strcmpic(arg, US"true") == 0)? OK : DEFER;
    if (rc == DEFER)
      *log_msgptr = string_sprintf("invalid \"condition\" value \"%s\"", arg);
    break;

    case ACLC_CONTROL:
    if (where != ACL_WHERE_MAIL && where != ACL_WHERE_RCPT &&
        where != ACL_WHERE_DATA && where != ACL_WHERE_NOTSMTP)
      {
      *log_msgptr = string_sprintf("cannot use \"control\" in %s ACL",
        acl_wherenames[where]);
      return ERROR;
      }
    if (Ustrcmp(arg, "freeze") == 0)
      {
      deliver_freeze = TRUE;
      deliver_frozen_at = time(NULL);
      }
    else if (Ustrcmp(arg, "queue_only") == 0)
      {
      queue_only_policy = TRUE;
      }
    else if (Ustrncmp(arg, "submission", 10) == 0)
      {

debug_printf("+++submission arg=%s\n", arg);

      submission_mode = TRUE;
      if (Ustrncmp(arg+10, "/domain=", 8) == 0)
        {
        submission_domain = string_copy(arg+18);
        }
      else if (arg[10] != 0)
        {
        *log_msgptr = string_sprintf("syntax error in argument for "
          "\"control\" modifier \"%s\"", arg);
        return ERROR;
        }
      }
    else
      {
      *log_msgptr = string_sprintf("syntax error in argument for \"control\" "
        "modifier: \"%s\" is unknown", arg);
      return ERROR;
      }
    break;

    case ACLC_DELAY:
      {
      int delay = readconf_readtime(arg, 0, FALSE);
      if (delay < 0)
        {
        *log_msgptr = string_sprintf("syntax error in argument for \"delay\" "
          "modifier: \"%s\" is not a time value", arg);
        return ERROR;
        }
      else
        {
        HDEBUG(D_acl) debug_printf("delay modifier requests %d-second delay\n",
          delay);
        if (host_checking)
          {
          HDEBUG(D_acl)
            debug_printf("delay skipped in -bh checking mode\n");
          }
        else sleep(delay);
        }
      }
    break;

    case ACLC_DNSLISTS:
    rc = verify_check_dnsbl(&arg);
    break;

    case ACLC_DOMAINS:
    rc = match_isinlist(addr->domain, &arg, 0, &domainlist_anchor,
      addr->domain_cache, MCL_DOMAIN, TRUE, &deliver_domain_data);
    break;

    /* The value in tls_cipher is the full cipher name, for example,
    TLSv1:DES-CBC3-SHA:168, whereas the values to test for are just the
    cipher names such as DES-CBC3-SHA. But program defensively. We don't know
    what may in practice come out of the SSL library - which at the time of
    writing is poorly documented. */

    case ACLC_ENCRYPTED:
    if (tls_cipher == NULL) rc = FAIL; else
      {
      uschar *endcipher = NULL;
      uschar *cipher = Ustrchr(tls_cipher, ':');
      if (cipher == NULL) cipher = tls_cipher; else
        {
        endcipher = Ustrchr(++cipher, ':');
        if (endcipher != NULL) *endcipher = 0;
        }
      rc = match_isinlist(cipher, &arg, 0, NULL, NULL, MCL_STRING, TRUE, NULL);
      if (endcipher != NULL) *endcipher = ':';
      }
    break;

    /* Use verify_check_this_host() instead of verify_check_host() so that
    we can pass over &host_data to catch any looked up data. Once it has been
    set, it retains its value so that it's still there if another ACL verb
    comes through here and uses the cache. However, we must put it into
    permanent store in case it is also expected to be used in a subsequent
    message in the same SMTP connection. */

    case ACLC_HOSTS:
    rc = verify_check_this_host(&arg, sender_host_cache, NULL,
      (sender_host_address == NULL)? US"" : sender_host_address, &host_data);
    if (host_data != NULL) host_data = string_copy_malloc(host_data);
    break;

    case ACLC_LOCAL_PARTS:
    rc = match_isinlist(addr->cc_local_part, &arg, 0,
      &localpartlist_anchor, addr->localpart_cache, MCL_LOCALPART, TRUE,
      &deliver_localpart_data);
    break;

    case ACLC_LOGWRITE:
      {
      int logbits = 0;
      uschar *s = arg;
      if (*s == ':')
        {
        s++;
        while (*s != ':')
          {
          if (Ustrncmp(s, "main", 4) == 0)
            { logbits |= LOG_MAIN; s += 4; }
          else if (Ustrncmp(s, "panic", 5) == 0)
            { logbits |= LOG_PANIC; s += 5; }
          else if (Ustrncmp(s, "reject", 6) == 0)
            { logbits |= LOG_REJECT; s += 6; }
          else
            {
            logbits = LOG_MAIN|LOG_PANIC;
            s = string_sprintf(":unknown log name in \"%s\" in "
              "\"logwrite\" in %s ACL", arg, acl_wherenames[where]);
            }
          if (*s == ',') s++;
          }
        s++;
        }
      while (isspace(*s)) s++;
      if (logbits == 0) logbits = LOG_MAIN;
      log_write(0, logbits, "%s", string_printing(s));
      }
    break;

    case ACLC_RECIPIENTS:
    rc = match_address_list(addr->address, TRUE, TRUE, &arg, NULL, -1, 0,
      &recipient_data);
    break;

    case ACLC_SENDER_DOMAINS:
      {
      uschar *sdomain;
      sdomain = Ustrrchr(sender_address, '@');
      sdomain = (sdomain == NULL)? US"" : sdomain + 1;
      rc = match_isinlist(sdomain, &arg, 0, &domainlist_anchor,
        sender_domain_cache, MCL_DOMAIN, TRUE, NULL);
      }
    break;

    case ACLC_SENDERS:
    rc = match_address_list(sender_address, TRUE, TRUE, &arg,
      sender_address_cache, -1, 0, &sender_data);
    break;

    /* Connection variables must persist forever */

    case ACLC_SET:
      {
      int old_pool = store_pool;
      if (cb->u.varnumber < ACL_C_MAX) store_pool = POOL_PERM;
      acl_var[cb->u.varnumber] = string_copy(arg);
      store_pool = old_pool;
      }
    break;

    /* If the verb is WARN, discard any user message from verification, because
    such messages are SMTP responses, not header additions. The latter come
    only from explicit "message" modifiers. */

    case ACLC_VERIFY:
    rc = acl_verify(where, addr, arg, user_msgptr, log_msgptr, basic_errno);
    if (verb == ACL_WARN) *user_msgptr = NULL;
    break;

    default:
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "internal ACL error: unknown "
      "condition %d", cb->type);
    break;
    }

  /* If a condition was negated, invert OK/FAIL. */

  if (!cond_modifiers[cb->type] && cb->u.negated)
    {
    if (rc == OK) rc = FAIL;
      else if (rc == FAIL || rc == FAIL_DROP) rc = OK;
    }

  if (rc != OK) break;   /* Conditions loop */
  }


/* If the result is the one for which "message" and/or "log_message" are used,
handle the values of these options. Most verbs have but a single return for
which the messages are relevant, but for "discard", it's useful to have the log
message both when it succeeds and when it fails.

These modifiers act in different ways:

"message" is a user message that will be included in an SMTP response. Unless
it is empty, it overrides any previously set user message.

"log_message" is a non-user message, and it adds to any existing non-user
message that is already set.

If there isn't a log message set, we make it the same as the user message. */

if (((rc == FAIL_DROP)? FAIL : rc) == msgcond[verb] ||
    (verb == ACL_DISCARD && rc == OK))
  {
  uschar *expmessage;

  /* If the verb is "warn", messages generated by conditions (verification or
  nested ACLs) are discarded. Only messages specified at this level are used.
  However, the value of an existing message is available in $acl_verify_message
  during expansions. */

  uschar *old_user_msgptr = *user_msgptr;
  uschar *old_log_msgptr = (*log_msgptr != NULL)? *log_msgptr : old_user_msgptr;

  if (verb == ACL_WARN) *log_msgptr = *user_msgptr = NULL;

  if (user_message != NULL)
    {
    acl_verify_message = old_user_msgptr;
    expmessage = expand_string(user_message);
    if (expmessage == NULL)
      {
      if (!expand_string_forcedfail)
        log_write(0, LOG_MAIN|LOG_PANIC, "failed to expand ACL message \"%s\": %s",
          user_message, expand_string_message);
      }
    else if (expmessage[0] != 0) *user_msgptr = expmessage;
    }

  if (log_message != NULL)
    {
    acl_verify_message = old_log_msgptr;
    expmessage = expand_string(log_message);
    if (expmessage == NULL)
      {
      if (!expand_string_forcedfail)
        log_write(0, LOG_MAIN|LOG_PANIC, "failed to expand ACL message \"%s\": %s",
          log_message, expand_string_message);
      }
    else if (expmessage[0] != 0)
      {
      *log_msgptr = (*log_msgptr == NULL)? expmessage :
        string_sprintf("%s: %s", expmessage, *log_msgptr);
      }
    }

  /* If no log message, default it to the user message */

  if (*log_msgptr == NULL) *log_msgptr = *user_msgptr;
  }

acl_verify_message = NULL;
return rc;
}





/*************************************************
*        Get line from a literal ACL             *
*************************************************/

/* This function is passed to acl_read() in order to extract individual lines
of a literal ACL, which we access via static pointers. We can destroy the
contents because this is called only once (the compiled ACL is remembered).

This code is intended to treat the data in the same way as lines in the main
Exim configuration file. That is:

  . Leading spaces are ignored.

  . A \ at the end of a line is a continuation - trailing spaces after the \
    are permitted (this is because I don't believe in making invisible things
    significant). Leading spaces on the continued part of a line are ignored.

  . Physical lines starting (significantly) with # are totally ignored, and
    may appear within a sequence of backslash-continued lines.

  . Blank lines are ignored, but will end a sequence of continuations.

Arguments: none
Returns:   a pointer to the next line
*/


static uschar *acl_text;          /* Current pointer in the text */
static uschar *acl_text_end;      /* Points one past the terminating '0' */


static uschar *
acl_getline(void)
{
uschar *yield;

/* This loop handles leading blank lines and comments. */

for(;;)
  {
  while (isspace(*acl_text)) acl_text++;   /* Leading spaces/empty lines */
  if (*acl_text == 0) return NULL;         /* No more data */
  yield = acl_text;                        /* Potential data line */

  while (*acl_text != 0 && *acl_text != '\n') acl_text++;

  /* If we hit the end before a newline, we have the whole logical line. If
  it's a comment, there's no more data to be given. Otherwise, yield it. */

  if (*acl_text == 0) return (*yield == '#')? NULL : yield;

  /* After reaching a newline, end this loop if the physical line does not
  start with '#'. If it does, it's a comment, and the loop continues. */

  if (*yield != '#') break;
  }

/* This loop handles continuations. We know we have some real data, ending in
newline. See if there is a continuation marker at the end (ignoring trailing
white space). We know that *yield is not white space, so no need to test for
cont > yield in the backwards scanning loop. */

for(;;)
  {
  uschar *cont;
  for (cont = acl_text - 1; isspace(*cont); cont--);

  /* If no continuation follows, we are done. Mark the end of the line and
  return it. */

  if (*cont != '\\')
    {
    *acl_text++ = 0;
    return yield;
    }

  /* We have encountered a continuation. Skip over whitespace at the start of
  the next line, and indeed the whole of the next line or lines if they are
  comment lines. */

  for (;;)
    {
    while (*(++acl_text) == ' ' || *acl_text == '\t');
    if (*acl_text != '#') break;
    while (*(++acl_text) != 0 && *acl_text != '\n');
    }

  /* We have the start of a continuation line. Move all the rest of the data
  to join onto the previous line, and then find its end. If the end is not a
  newline, we are done. Otherwise loop to look for another continuation. */

  memmove(cont, acl_text, acl_text_end - acl_text);
  acl_text_end -= acl_text - cont;
  acl_text = cont;
  while (*acl_text != 0 && *acl_text != '\n') acl_text++;
  if (*acl_text == 0) return yield;
  }

/* Control does not reach here */
}





/*************************************************
*        Check access using an ACL               *
*************************************************/

/* This function is called from address_check. It may recurse via
acl_check_condition() - hence the use of a level to stop looping. The ACL is
passed as a string which is expanded. A forced failure implies no access check
is required. If the result is a single word, it is taken as the name of an ACL
which is sought in the global ACL tree. Otherwise, it is taken as literal ACL
text, complete with newlines, and parsed as such. In both cases, the ACL check
is then run. This function uses an auxiliary function for acl_read() to call
for reading individual lines of a literal ACL. This is acl_getline(), which
appears immediately above.

Arguments:
  where        where called from
  addr         address item when called from RCPT; otherwise NULL
  s            the input string; NULL is the same as an empty ACL => DENY
  level        the nesting level
  user_msgptr  where to put a user error (for SMTP response)
  log_msgptr   where to put a logging message (not for SMTP response)

Returns:       OK         access is granted
               DISCARD    access is apparently granted...
               FAIL       access is denied
               FAIL_DROP  access is denied; drop the connection
               DEFER      can't tell at the moment
               ERROR      disaster
*/

static int
acl_check_internal(int where, address_item *addr, uschar *s, int level,
  uschar **user_msgptr, uschar **log_msgptr)
{
int fd = -1;
acl_block *acl = NULL;
uschar *acl_name = US"inline ACL";
uschar *ss;

/* Catch configuration loops */

if (level > 20)
  {
  *log_msgptr = US"ACL nested too deep: possible loop";
  return ERROR;
  }

if (s == NULL)
  {
  HDEBUG(D_acl) debug_printf("ACL is NULL: implicit DENY\n");
  return FAIL;
  }

/* At top level, we expand the incoming string. At lower levels, it has already
been expanded as part of condition processing. */

if (level == 0)
  {
  ss = expand_string(s);
  if (ss == NULL)
    {
    if (expand_string_forcedfail) return OK;
    *log_msgptr = string_sprintf("failed to expand ACL string \"%s\": %s", s,
      expand_string_message);
    return ERROR;
    }
  }
else ss = s;

while (isspace(*ss))ss++;

/* If we can't find a named ACL, the default is to parse it as an inline one.
(Unless it begins with a slash; non-existent files give rise to an error.) */

acl_text = ss;

/* Handle the case of a string that does not contain any spaces. Look for a
named ACL among those read from the configuration, or a previously read file.
It is possible that the pointer to the ACL is NULL if the configuration
contains a name with no data. If not found, and the text begins with '/',
read an ACL from a file, and save it so it can be re-used. */

if (Ustrchr(ss, ' ') == NULL)
  {
  tree_node *t = tree_search(acl_anchor, ss);
  if (t != NULL)
    {
    acl = (acl_block *)(t->data.ptr);
    if (acl == NULL)
      {
      HDEBUG(D_acl) debug_printf("ACL \"%s\" is empty: implicit DENY\n", ss);
      return FAIL;
      }
    acl_name = string_sprintf("ACL \"%s\"", ss);
    HDEBUG(D_acl) debug_printf("using ACL \"%s\"\n", ss);
    }

  else if (*ss == '/')
    {
    struct stat statbuf;
    fd = Uopen(ss, O_RDONLY, 0);
    if (fd < 0)
      {
      *log_msgptr = string_sprintf("failed to open ACL file \"%s\": %s", ss,
        strerror(errno));
      return ERROR;
      }

    if (fstat(fd, &statbuf) != 0)
      {
      *log_msgptr = string_sprintf("failed to fstat ACL file \"%s\": %s", ss,
        strerror(errno));
      return ERROR;
      }

    acl_text = store_get(statbuf.st_size + 1);
    acl_text_end = acl_text + statbuf.st_size + 1;

    if (read(fd, acl_text, statbuf.st_size) != statbuf.st_size)
      {
      *log_msgptr = string_sprintf("failed to read ACL file \"%s\": %s",
        ss, strerror(errno));
      return ERROR;
      }
    acl_text[statbuf.st_size] = 0;
    close(fd);

    acl_name = string_sprintf("ACL \"%s\"", ss);
    HDEBUG(D_acl) debug_printf("read ACL from file %s\n", ss);
    }
  }

/* Parse an ACL that is still in text form. If it came from a file, remember it
in the ACL tree, having read it into the POOL_PERM store pool so that it
persists between multiple messages. */

if (acl == NULL)
  {
  int old_pool = store_pool;
  if (fd >= 0) store_pool = POOL_PERM;
  acl = acl_read(acl_getline, log_msgptr);
  store_pool = old_pool;
  if (acl == NULL && *log_msgptr != NULL) return ERROR;
  if (fd >= 0)
    {
    tree_node *t = store_get_perm(sizeof(tree_node) + Ustrlen(ss));
    Ustrcpy(t->name, ss);
    t->data.ptr = acl;
    (void)tree_insertnode(&acl_anchor, t);
    }
  }

/* Now we have an ACL to use. It's possible it may be NULL. */

while (acl != NULL)
  {
  int cond;
  int basic_errno = 0;
  BOOL endpass_seen = FALSE;

  HDEBUG(D_acl) debug_printf("processing \"%s\"\n", verbs[acl->verb]);

  *log_msgptr = *user_msgptr = NULL;
  acl_temp_details = FALSE;

  /* Clear out any search error message from a previous check before testing
  this condition. */

  search_error_message = NULL;
  cond = acl_check_condition(acl->verb, acl->condition, where, addr, level,
    &endpass_seen, user_msgptr, log_msgptr, &basic_errno);

  /* Handle special returns: DEFER causes a return except on a WARN verb;
  ERROR always causes a return. */

  switch (cond)
    {
    case DEFER:
    HDEBUG(D_acl) debug_printf("%s: condition test deferred\n", verbs[acl->verb]);
    if (basic_errno != ERRNO_CALLOUTDEFER)
      {
      if (search_error_message != NULL && *search_error_message != 0)
        *log_msgptr = search_error_message;
      if (smtp_return_error_details) acl_temp_details = TRUE;
      }
    else
      {
      acl_temp_details = TRUE;
      }
    if (acl->verb != ACL_WARN) return DEFER;
    break;

    default:      /* Paranoia */
    case ERROR:
    HDEBUG(D_acl) debug_printf("%s: condition test error\n", verbs[acl->verb]);
    return ERROR;

    case OK:
    HDEBUG(D_acl) debug_printf("%s: condition test succeeded\n",
      verbs[acl->verb]);
    break;

    case FAIL:
    HDEBUG(D_acl) debug_printf("%s: condition test failed\n", verbs[acl->verb]);
    break;

    /* DISCARD and DROP can happen only from a nested ACL condition, and
    DISCARD can happen only for an "accept" or "discard" verb. */

    case DISCARD:
    HDEBUG(D_acl) debug_printf("%s: condition test yielded \"discard\"\n",
      verbs[acl->verb]);
    break;

    case FAIL_DROP:
    HDEBUG(D_acl) debug_printf("%s: condition test yielded \"drop\"\n",
      verbs[acl->verb]);
    break;
    }

  /* At this point, cond for most verbs is either OK or FAIL or (as a result of
  a nested ACL condition) FAIL_DROP. However, for WARN, cond may be DEFER, and
  for ACCEPT and DISCARD, it may be DISCARD after a nested ACL call. */

  switch(acl->verb)
    {
    case ACL_ACCEPT:
    if (cond == OK || cond == DISCARD) return cond;
    if (endpass_seen)
      {
      HDEBUG(D_acl) debug_printf("accept: endpass encountered - denying access\n");
      return cond;
      }
    break;

    case ACL_DEFER:
    if (cond == OK)
      {
      acl_temp_details = TRUE;
      return DEFER;
      }
    break;

    case ACL_DENY:
    if (cond == OK) return FAIL;
    break;

    case ACL_DISCARD:
    if (cond == OK || cond == DISCARD) return DISCARD;
    if (endpass_seen)
      {
      HDEBUG(D_acl) debug_printf("discard: endpass encountered - denying access\n");
      return cond;
      }
    break;

    case ACL_DROP:
    if (cond == OK) return FAIL_DROP;
    break;

    case ACL_REQUIRE:
    if (cond != OK) return cond;
    break;

    case ACL_WARN:
    if (cond == OK)
      acl_warn(where, *user_msgptr, *log_msgptr);
    else if (cond == DEFER)
      acl_warn(where, NULL, string_sprintf("ACL \"warn\" statement skipped: "
        "condition test deferred: %s",
        (*log_msgptr == NULL)? US"" : *log_msgptr));
    *log_msgptr = *user_msgptr = NULL;  /* In case implicit DENY follows */
    break;

    default:
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "internal ACL error: unknown verb %d",
      acl->verb);
    break;
    }

  /* Pass to the next ACL item */

  acl = acl->next;
  }

/* We have reached the end of the ACL. This is an implicit DENY. */

HDEBUG(D_acl) debug_printf("end of %s: implicit DENY\n", acl_name);
return FAIL;
}


/*************************************************
*        Check access using an ACL               *
*************************************************/

/* This is the external interface for ACL checks. It sets up an address and the
expansions for $domain and $local_part when called after RCPT, then calls
acl_check_internal() to do the actual work.

Arguments:
  where        ACL_WHERE_xxxx indicating where called from
  data_string  RCPT address, or SMTP command argument, or NULL
  s            the input string; NULL is the same as an empty ACL => DENY
  user_msgptr  where to put a user error (for SMTP response)
  log_msgptr   where to put a logging message (not for SMTP response)

Returns:       OK         access is granted by an ACCEPT verb
               DISCARD    access is granted by a DISCARD verb
               FAIL       access is denied
               FAIL_DROP  access is denied; drop the connection
               DEFER      can't tell at the moment
               ERROR      disaster
*/

int
acl_check(int where, uschar *data_string, uschar *s, uschar **user_msgptr,
  uschar **log_msgptr)
{
int rc;
address_item adb;
address_item *addr;

*user_msgptr = *log_msgptr = NULL;
sender_verified_failed = NULL;

if (where == ACL_WHERE_RCPT)
  {
  adb = address_defaults;
  addr = &adb;
  addr->address = data_string;
  if (deliver_split_address(addr) == DEFER)
    {
    *log_msgptr = US"defer in percent_hack_domains check";
    return DEFER;
    }
  deliver_domain = addr->domain;
  deliver_localpart = addr->local_part;
  }
else
  {
  addr = NULL;
  smtp_command_argument = data_string;
  }

rc = acl_check_internal(where, addr, s, 0, user_msgptr, log_msgptr);

smtp_command_argument = deliver_domain =
  deliver_localpart = deliver_address_data = NULL;

/* A DISCARD response is permitted only for message ACLs */

if (rc == DISCARD)
  {
  if (where != ACL_WHERE_MAIL &&
      where != ACL_WHERE_RCPT &&
      where != ACL_WHERE_DATA &&
      where != ACL_WHERE_NOTSMTP)
    {
    log_write(0, LOG_MAIN|LOG_PANIC, "\"discard\" verb not allowed in %s "
      "ACL", acl_wherenames[where]);
    return ERROR;
    }
  return DISCARD;
  }

/* A DROP response is not permitted from MAILAUTH */

if (rc == FAIL_DROP && where == ACL_WHERE_MAILAUTH)
  {
  log_write(0, LOG_MAIN|LOG_PANIC, "\"drop\" verb not allowed in %s "
    "ACL", acl_wherenames[where]);
  return ERROR;
  }

/* Before giving an error response, take a look at the length of any user
message, and split it up into multiple lines if possible. */

if (rc != OK && *user_msgptr != NULL && Ustrlen(*user_msgptr) > 75)
  {
  uschar *s = *user_msgptr = string_copy(*user_msgptr);
  uschar *ss = s;

  for (;;)
    {
    int i = 0;
    while (i < 75 && *ss != 0 && *ss != '\n') ss++, i++;
    if (*ss == 0) break;
    if (*ss == '\n')
      s = ++ss;
    else
      {
      uschar *t = ss + 1;
      uschar *tt = NULL;
      while (--t > s + 35)
        {
        if (*t == ' ')
          {
          if (t[-1] == ':') { tt = t; break; }
          if (tt == NULL) tt = t;
          }
        }

      if (tt == NULL)          /* Can't split behind - try ahead */
        {
        t = ss + 1;
        while (*t != 0)
          {
          if (*t == ' ' || *t == '\n')
            { tt = t; break; }
          t++;
          }
        }

      if (tt == NULL) break;   /* Can't find anywhere to split */
      *tt = '\n';
      s = ss = tt+1;
      }
    }
  }

return rc;
}

/* End of acl.c */
