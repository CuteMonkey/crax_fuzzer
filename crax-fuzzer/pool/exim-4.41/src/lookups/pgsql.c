/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* Thanks to Petr Cech for contributing the original code for these
functions. Thanks to Joachim Wieland for the initial patch for the Unix domain
socket extension. */

#include "../exim.h"
#include "lf_functions.h"
#include "pgsql.h"       /* The local header */

/* We can't just compile this code and allow the library mechanism to omit the
functions if they are not wanted, because we need to have the PGSQL header
available for compiling. Therefore, compile these functions only if
LOOKUP_PGSQL is defined. However, some compilers don't like compiling empty
modules, so keep them happy with a dummy when skipping the rest. Make it
reference itself to stop picky compilers complaining that it is unused, and put
in a dummy argument to stop even pickier compilers complaining about infinite
loops. */

#ifndef LOOKUP_PGSQL
static void dummy(int x) { dummy(x-1); }
#else


#include <libpq-fe.h>       /* The system header */

/* Structure and anchor for caching connections. */

typedef struct pgsql_connection {
  struct pgsql_connection *next;
  uschar *server;
  PGconn *handle;
} pgsql_connection;

static pgsql_connection *pgsql_connections = NULL;



/*************************************************
*              Open entry point                  *
*************************************************/

/* See local README for interface description. */

void *
pgsql_open(uschar *filename, uschar **errmsg)
{
return (void *)(1);    /* Just return something non-null */
}



/*************************************************
*               Tidy entry point                 *
*************************************************/

/* See local README for interface description. */

void
pgsql_tidy(void)
{
pgsql_connection *cn;
while ((cn = pgsql_connections) != NULL)
  {
  pgsql_connections = cn->next;
  DEBUG(D_lookup) debug_printf("close PGSQL connection: %s\n", cn->server);
  PQfinish(cn->handle);
  }
}



/*************************************************
*        Internal search function                *
*************************************************/

/* This function is called from the find entry point to do the search for a
single server. The server string is of the form "server/dbname/user/password".

PostgreSQL supports connections through Unix domain sockets. This is usually
faster and costs less cpu time than a TCP/IP connection. However it can only be
used if the mail server runs on the same machine as the database server. A
configuration line for PostgreSQL via Unix domain sockets looks like this:

hide pgsql_servers = (/tmp/.s.PGSQL.5432)/db/user/password[:<nextserver>]

We enclose the path name in parentheses so that its slashes aren't visually
confused with the delimeters for the other pgsql_server settings.

For TCP/IP connections, the server is a host name and optional port (with a
colon separator).

NOTE:
 1) All three '/' must be present.
 2) If host is omitted the local unix socket is used.

Arguments:
  query        the query string
  server       the server string; this is in dynamic memory and can be updated
  resultptr    where to store the result
  errmsg       where to point an error message
  defer_break  TRUE if no more servers are to be tried after DEFER
  do_cache     set FALSE if data is changed

Returns:       OK, FAIL, or DEFER
*/

static int
perform_pgsql_search(uschar *query, uschar *server, uschar **resultptr,
  uschar **errmsg, BOOL *defer_break, BOOL *do_cache)
{
PGconn *pg_conn = NULL;
PGresult *pg_result = NULL;

int i;
int ssize = 0;
int offset = 0;
int yield = DEFER;
unsigned int num_fields, num_tuples;
uschar *result = NULL;
pgsql_connection *cn;
uschar *server_copy = NULL;
uschar *sdata[3];

/* Disaggregate the parameters from the server argument. The order is host or
path, database, user, password. We can write to the string, since it is in a
nextinlist temporary buffer. The copy of the string that is used for caching
has the password removed. This copy is also used for debugging output. */

for (i = 2; i >= 0; i--)
  {
  uschar *pp = Ustrrchr(server, '/');
  if (pp == NULL)
    {
    *errmsg = string_sprintf("incomplete pgSQL server data: %s",
      (i == 2)? server : server_copy);
    *defer_break = TRUE;
    return DEFER;
    }
  *pp++ = 0;
  sdata[i] = pp;
  if (i == 2) server_copy = string_copy(server);  /* sans password */
  }

/* The total server string has now been truncated so that what is left at the
start is the identification of the server (host or path). See if we have a
cached connection to the server. */

for (cn = pgsql_connections; cn != NULL; cn = cn->next)
  {
  if (Ustrcmp(cn->server, server_copy) == 0)
    {
    pg_conn = cn->handle;
    break;
    }
  }

/* If there is no cached connection, we must set one up. */

if (cn == NULL)
  {
  uschar *port = US"";

  /* For a Unix domain socket connection, the path is in parentheses */

  if (*server == '(')
    {
    uschar *last_slash, *last_dot, *p;

    p = ++server;
    while (*p != 0 && *p != ')') p++;
    *p = 0;

    last_slash = Ustrrchr(server, '/');
    last_dot = Ustrrchr(server, '.');

    DEBUG(D_lookup) debug_printf("PGSQL new connection: socket=%s "
      "database=%s user=%s\n", server, sdata[0], sdata[1]);

    /* A valid socket name looks like this: /var/run/postgresql/.s.PGSQL.5432
    We have to call PQsetdbLogin with '/var/run/postgresql' as the hostname
    argument and put '5432' into the port variable. */

    if (last_slash == NULL || last_dot == NULL)
      {
      *errmsg = string_sprintf("PGSQL invalid filename for socket: %s",
        server);
      *defer_break = TRUE;
      return DEFER;
      }

    /* Terminate the path name and set up the port: we'll have something like
    server = "/var/run/postgresql" and port = "5432". */

    *last_slash = 0;
    port = last_dot + 1;
    }

  /* Host connection; sort out the port */

  else
    {
    uschar *p;
    if ((p = Ustrchr(server, ':')) != NULL)
      {
      *p++ = 0;
      port = p;
      }

    if (Ustrchr(server, '/') != NULL)
      {
      *errmsg = string_sprintf("unexpected slash in pgSQL server hostname: %s",
        server);
      *defer_break = TRUE;
      return DEFER;
      }

    DEBUG(D_lookup) debug_printf("PGSQL new connection: host=%s port=%s "
      "database=%s user=%s\n", server, port, sdata[0], sdata[1]);
    }

  /* If the database is the empty string, set it NULL - the query must then
  define it. */

  if (sdata[0][0] == 0) sdata[0] = NULL;

  /* Get store for a new handle, initialize it, and connect to the server */

  pg_conn=PQsetdbLogin(
    /*  host      port  options tty   database       user       passwd */
    CS server, CS port,  NULL, NULL, CS sdata[0], CS sdata[1], CS sdata[2]);

  if(PQstatus(pg_conn) == CONNECTION_BAD)
    {
    store_reset(server_copy);
    *errmsg = string_sprintf("PGSQL connection failed: %s",
      PQerrorMessage(pg_conn));
    PQfinish(pg_conn);
    *defer_break = FALSE;
    goto PGSQL_EXIT;
    }

  /* Add the connection to the cache */

  cn = store_get(sizeof(pgsql_connection));
  cn->server = server_copy;
  cn->handle = pg_conn;
  cn->next = pgsql_connections;
  pgsql_connections = cn;
  }

/* Else use a previously cached connection */

else
  {
  DEBUG(D_lookup) debug_printf("PGSQL using cached connection for %s\n",
    server_copy);
  }

/* Run the query */

  pg_result = PQexec(pg_conn, CS query);
  switch(PQresultStatus(pg_result))
    {
    case PGRES_EMPTY_QUERY:
    case PGRES_COMMAND_OK:
    /* The command was successful but did not return any data since it was
     * not SELECT but either an INSERT, UPDATE or DELETE statement. Tell the
     * high level code to not cache this query, and clean the current cache for
     * this handle by setting *do_cache FALSE. */
    result = string_copy(US PQcmdTuples(pg_result));
    offset = Ustrlen(result);
    *do_cache = FALSE;
    DEBUG(D_lookup) debug_printf("PGSQL: command does not return any data "
      "but was successful. Rows affected: %s\n", result);

    case PGRES_TUPLES_OK:
    break;

    default:
    *errmsg = string_sprintf("PGSQL: query failed: %s\n",
                             PQresultErrorMessage(pg_result));
    *defer_break = FALSE;
    goto PGSQL_EXIT;
    }

/* Result is in pg_result. Find the number of fields returned. If this is one,
we don't add field names to the data. Otherwise we do. If the query did not
return anything we skip the for loop; this also applies to the case
PGRES_COMMAND_OK. */

num_fields = PQnfields(pg_result);
num_tuples = PQntuples(pg_result);

/* Get the fields and construct the result string. If there is more than one
row, we insert '\n' between them. */

for (i = 0; i < num_tuples; i++)
  {
  if (result != NULL)
    result = string_cat(result, &ssize, &offset, US"\n", 1);

   if (num_fields == 1)
    {
    result = string_cat(result, &ssize, &offset,
      US PQgetvalue(pg_result, i, 0), PQgetlength(pg_result, i, 0));
    }

   else
    {
    int j;
    for (j = 0; j < num_fields; j++)
      {
      uschar *tmp = US PQfname(pg_result, j);
      result = string_cat(result, &ssize, &offset, tmp, Ustrlen(tmp));
      result = string_cat(result, &ssize, &offset, US"=", 1);

      /* Quote the value if it contains spaces or is empty */

      tmp = US PQgetvalue(pg_result, i, j);
      if (*tmp == 0 || (Ustrchr(tmp, ' ') != NULL))
        {
        int k, t_len = Ustrlen(tmp);
        result = string_cat(result, &ssize, &offset, US"\"", 1);
        for(k = 0; k < t_len; k++)
          {
          if (tmp[k] == '\"' || tmp[k] == '\\')
             result = string_cat(result, &ssize, &offset, US"\\", 1);
          result = string_cat(result, &ssize, &offset, tmp+k, 1);
          }
        result = string_cat(result, &ssize, &offset, US"\"", 1);
        }
      else
        {
        result = string_cat(result, &ssize, &offset, tmp, Ustrlen(tmp));
        }
      result = string_cat(result, &ssize, &offset, US" ", 1);
      }
    }
  }

/* If result is NULL then no data has been found and so we return FAIL.
Otherwise, we must terminate the string which has been built; string_cat()
always leaves enough room for a terminating zero. */

if (result == NULL)
  {
  yield = FAIL;
  *errmsg = US"PGSQL: no data found";
  }
else
  {
  result[offset] = 0;
  store_reset(result + offset + 1);
  }

/* Get here by goto from various error checks. */

PGSQL_EXIT:

/* Free store for any result that was got; don't close the connection, as
it is cached. */

if (pg_result != NULL) PQclear(pg_result);

/* Non-NULL result indicates a sucessful result */

if (result != NULL)
  {
  *resultptr = result;
  return OK;
  }
else
  {
  DEBUG(D_lookup) debug_printf("%s\n", *errmsg);
  return yield;      /* FAIL or DEFER */
  }
}




/*************************************************
*               Find entry point                 *
*************************************************/

/* See local README for interface description. The handle and filename
arguments are not used. Loop through a list of servers while the query is
deferred with a retryable error. */

int
pgsql_find(void *handle, uschar *filename, uschar *query, int length,
  uschar **result, uschar **errmsg, BOOL *do_cache)
{
int sep = 0;
uschar *server;
uschar *list = pgsql_servers;
uschar buffer[512];

DEBUG(D_lookup) debug_printf("PGSQL query: %s\n", query);

while ((server = string_nextinlist(&list, &sep, buffer, sizeof(buffer)))
        != NULL)
  {
  BOOL defer_break;
  int rc = perform_pgsql_search(query, server, result, errmsg, &defer_break,
    do_cache);
  if (rc != DEFER || defer_break) return rc;
  }

if (pgsql_servers == NULL)
  *errmsg = US"no PGSQL servers defined (pgsql_servers option)";

return DEFER;
}



/*************************************************
*               Quote entry point                *
*************************************************/

/* The characters that always need to be quoted (with backslash) are newline,
tab, carriage return, backspace, backslash itself, and the quote characters.
Percent and underscore are only special in contexts where they can be wild
cards, and this isn't usually the case for data inserted from messages, since
that isn't likely to be treated as a pattern of any kind. However, pgsql seems
to allow escaping "on spec". If you use something like "where id="ab\%cd" it
does treat the string as "ab%cd". So we can safely quote percent and
underscore. [This is different to MySQL, where you can't do this.]

Arguments:
  s          the string to be quoted
  opt        additional option text or NULL if none

Returns:     the processed string or NULL for a bad option
*/

uschar *
pgsql_quote(uschar *s, uschar *opt)
{
register int c;
int count = 0;
uschar *t = s;
uschar *quoted;

if (opt != NULL) return NULL;     /* No options recognized */

while ((c = *t++) != 0)
  if (Ustrchr("\n\t\r\b\'\"\\%_", c) != NULL) count++;

if (count == 0) return s;
t = quoted = store_get(Ustrlen(s) + count + 1);

while ((c = *s++) != 0)
  {
  if (Ustrchr("\n\t\r\b\'\"\\%_", c) != NULL)
    {
    *t++ = '\\';
    switch(c)
      {
      case '\n': *t++ = 'n';
      break;
      case '\t': *t++ = 't';
      break;
      case '\r': *t++ = 'r';
      break;
      case '\b': *t++ = 'b';
      break;
      default:   *t++ = c;
      break;
      }
    }
  else *t++ = c;
  }

*t = 0;
return quoted;
}

#endif  /* PGSQL_LOOKUP */

/* End of lookups/pgsql.c */
