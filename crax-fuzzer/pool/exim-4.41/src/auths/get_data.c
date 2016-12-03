/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

#include "../exim.h"


/*************************************************
*      Issue a challenge and get a response      *
*************************************************/

/* This function is used by authentication drivers to output a challenge
to the SMTP client and read the response line.

Arguments:
   aptr       set to point to the response (which is in big_buffer)
   challenge  the challenge text (unencoded)

Returns:      OK on success
              BAD64 if response too large for buffer
              CANCELLED if response is "*"
*/

int
auth_get_data(uschar **aptr, uschar *challenge)
{
int c;
int p = 0;
smtp_printf("334 %s\r\n", auth_b64encode(challenge, Ustrlen(challenge)));
while ((c = receive_getc()) != '\n' && c != EOF)
  {
  if (p >= big_buffer_size - 1) return BAD64;
  big_buffer[p++] = c;
  }
if (p > 0 && big_buffer[p-1] == '\r') p--;
big_buffer[p] = 0;
if (Ustrcmp(big_buffer, "*") == 0) return CANCELLED;
*aptr = big_buffer;
return OK;
}

/* End of get_data.c */
