/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* This file was originally supplied by Ian Kirk. */

#include "../exim.h"

/* This module contains functions that call the Radius authentication
mechanism.

We can't just compile this code and allow the library mechanism to omit the
functions if they are not wanted, because we need to have the Radius headers
available for compiling. Therefore, compile these functions only if
RADIUS_CONFIG_FILE is defined. However, some compilers don't like compiling
empty modules, so keep them happy with a dummy when skipping the rest. Make it
reference itself to stop picky compilers complaining that it is unused, and put
in a dummy argument to stop even pickier compilers complaining about infinite
loops. */

#ifndef RADIUS_CONFIG_FILE
static void dummy(int x) { dummy(x-1); }
#else  /* RADIUS_CONFIG_FILE */


#include <radiusclient.h>


/*************************************************
*              Perform RADIUS authentication     *
*************************************************/

/* This function calls the Radius authentication mechanism, passing over one or
more data strings.

Arguments:
  s        a colon-separated list of strings
  errptr   where to point an error message

Returns:   OK if authentication succeeded
           FAIL if authentication failed
           ERROR some other error condition
*/

int
auth_call_radius(uschar *s, uschar **errptr)
{
uschar *user;
uschar *radius_args = s;
VALUE_PAIR *send = NULL;
VALUE_PAIR *received;
unsigned int service = PW_AUTHENTICATE_ONLY;
int result;
int sep = 0;
char msg[4096];

user = string_nextinlist(&radius_args, &sep, big_buffer, big_buffer_size);
if (user == NULL) user = US"";

DEBUG(D_auth) debug_printf("Running RADIUS authentication for user \"%s\" "
               "and \"%s\"\n", user, radius_args);

*errptr = NULL;

rc_openlog("exim");

if (rc_read_config(RADIUS_CONFIG_FILE) != 0)
  *errptr = string_sprintf("RADIUS: can't open %s", RADIUS_CONFIG_FILE);

else if (rc_read_dictionary(rc_conf_str("dictionary")) != 0)
  *errptr = string_sprintf("RADIUS: can't read dictionary");

else if (rc_avpair_add(&send, PW_USER_NAME, user, 0) == NULL)
  *errptr = string_sprintf("RADIUS: add user name failed\n");

else if (rc_avpair_add(&send, PW_USER_PASSWORD, CS radius_args, 0) == NULL)
  *errptr = string_sprintf("RADIUS: add password failed\n");

else if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL)
  *errptr = string_sprintf("RADIUS: add service type failed\n");

if (*errptr != NULL)
  {
  DEBUG(D_auth) debug_printf("%s\n", *errptr);
  return ERROR;
  }

result = rc_auth(0, send, &received, msg);
DEBUG(D_auth) debug_printf("RADIUS code returned %d\n", result);

switch (result)
  {
  case OK_RC:
  return OK;

  case ERROR_RC:
  return FAIL;

  case TIMEOUT_RC:
  *errptr = US"RADIUS: timed out";
  return ERROR;

  default:
  case BADRESP_RC:
  *errptr = string_sprintf("RADIUS: unexpected response (%d)", result);
  return ERROR;
  }
}

#endif  /* RADIUS_CONFIG_FILE */

/* End of call_radius.c */
