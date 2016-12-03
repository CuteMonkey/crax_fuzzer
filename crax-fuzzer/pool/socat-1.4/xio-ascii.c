/* $Id: xio-ascii.c,v 1.4 2004/06/20 21:36:20 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains functions for text encoding, decoding, and conversions */


#include "stddef.h"
#include "ctype.h"

#include "xio-ascii.h"

/* for each 6 bit pattern we have an ASCII character in the arry */
const static int base64chars[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/',
} ;

#define CHAR64(c) (base64chars[c])

char *
   xiob64encodeline(const char *data,		/* input data */
		    size_t bytes,		/* length of input data, >=0 */
		    char *coded	/* output buffer, must be long enough */
		 ) {
   int c1, c2, c3;

   while (bytes > 0) {
      c1 = *data++;
      *coded++ = CHAR64(c1>>2);
      if (--bytes == 0) {
	 *coded++ = CHAR64((c1&0x03)<<4);
	 *coded++ = '=';
	 *coded++ = '=';
      } else {
	 c2 = *data++;
	 *coded++ = CHAR64(((c1&0x03)<<4)|(c2>>4));
	 if (--bytes == 0) {
	    *coded++ = CHAR64((c2&0x0f)<<2);
	    *coded++ = '=';
	 } else {
	    c3 = *data++; --bytes;
	    *coded++ = CHAR64(((c2&0x0f)<<2)|(c3>>6));
	    *coded++ = CHAR64(c3&0x3f);
	 }
      }
   }
   return coded;
}



/* sanitize "untrusted" text, replacing special control characters with the C
   string version ("\x"), and replacing unprintable chars with ".".
   text can grow to double size, so keep output buffer long enough!
   returns a pointer to the first untouched byte of the output buffer.
*/
char *xiosanitize(const char *data,	/* input data */
		  size_t bytes,			/* length of input data, >=0 */
		  char *coded	/* output buffer, must be long enough */
		  ) {
   int c;

   while (bytes > 0) {
      c = *(unsigned char *)data++;
      switch (c) {
      case '\0' : *coded++ = '\\'; *coded++ = '0'; break;
      case '\a' : *coded++ = '\\'; *coded++ = 'a'; break;
      case '\b' : *coded++ = '\\'; *coded++ = 'b'; break;
      case '\t' : *coded++ = '\\'; *coded++ = 't'; break;
      case '\n' : *coded++ = '\\'; *coded++ = 'n'; break;
      case '\v' : *coded++ = '\\'; *coded++ = 'v'; break;
      case '\f' : *coded++ = '\\'; *coded++ = 'f'; break;
      case '\r' : *coded++ = '\\'; *coded++ = 'r'; break;
      case '\'' : *coded++ = '\\'; *coded++ = '\''; break;
      case '\"' : *coded++ = '\\'; *coded++ = '"'; break;
      case '\\' : *coded++ = '\\'; *coded++ = '\\'; break;
      default:
	 if (!isprint(c))
	    c = '.';
	 *coded++ = c;
	 break;
      }
      --bytes;
   }
   return coded;
}
