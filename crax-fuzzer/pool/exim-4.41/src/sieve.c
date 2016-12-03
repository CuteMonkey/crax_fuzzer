/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) Michael Haardt 2003 */
/* See the file NOTICE for conditions of use and distribution. */

/* This code was contributed by Michael Haardt. */


/* Sieve mail filter. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "exim.h"

#if HAVE_ICONV
#include <iconv.h>
#endif

/* Define this for RFC compliant \r\n end-of-line terminators.      */
/* Undefine it for UNIX-style \n end-of-line terminators (default). */
#undef RFC_EOL

/* Define this for development of the subaddress Sieve extension.   */
/* The code is currently broken.                                    */
#undef SUBADDRESS

/* Keep this at 75 to accept only RFC compliant MIME words.         */
/* Increase it if you want to match headers from buggy MUAs.        */
#define MIMEWORD_LENGTH 75

struct Sieve
  {
  uschar *filter;
  const uschar *pc;
  int line;
  const uschar *errmsg;
  int keep;
  int require_envelope;
  int require_fileinto;
#ifdef SUBADDRESS
  int require_subaddress;
#endif
  };

enum Comparator { COMP_OCTET, COMP_ASCII_CASEMAP };
enum MatchType { MATCH_IS, MATCH_CONTAINS, MATCH_MATCHES };
enum AddressPart { ADDRPART_USER, ADDRPART_DETAIL, ADDRPART_LOCALPART, ADDRPART_DOMAIN, ADDRPART_ALL };

struct String
  {
  uschar *character;
  int length;
  };

static int parse_test(struct Sieve *filter, int *cond, int exec);
static int parse_commands(struct Sieve *filter, int exec, address_item **generated);

static uschar str_from_c[]="From";
static const struct String str_from={ str_from_c, 4 };
static uschar str_to_c[]="To";
static const struct String str_to={ str_to_c, 2 };
static uschar str_cc_c[]="Cc";
static const struct String str_cc={ str_cc_c, 2 };
static uschar str_bcc_c[]="Bcc";
static const struct String str_bcc={ str_bcc_c, 3 };
static uschar str_sender_c[]="Sender";
static const struct String str_sender={ str_sender_c, 6 };
static uschar str_resent_from_c[]="Resent-From";
static const struct String str_resent_from={ str_resent_from_c, 11 };
static uschar str_resent_to_c[]="Resent-To";
static const struct String str_resent_to={ str_resent_to_c, 9 };
static uschar str_fileinto_c[]="fileinto";
static const struct String str_fileinto={ str_fileinto_c, 8 };
static uschar str_envelope_c[]="envelope";
static const struct String str_envelope={ str_envelope_c, 8 };
#ifdef SUBADDRESS
static uschar str_subaddress_c[]="subaddress";
static const struct String str_subaddress={ str_subaddress_c, 10 };
#endif
static uschar str_iascii_casemap_c[]="i;ascii-casemap";
static const struct String str_iascii_casemap={ str_iascii_casemap_c, 15 };
static uschar str_ioctet_c[]="i;octet";
static const struct String str_ioctet={ str_ioctet_c, 7 };
static uschar str_comparator_iascii_casemap_c[]="comparator-i;ascii-casemap";
static const struct String str_comparator_iascii_casemap={ str_comparator_iascii_casemap_c, 26 };
static uschar str_comparator_ioctet_c[]="comparator-i;octet";
static const struct String str_comparator_ioctet={ str_comparator_ioctet_c, 18 };


/*************************************************
*          Octet-wise string comparison          *
*************************************************/

/*
Arguments:
  needle            UTF-8 string to search ...
  haystack          ... inside the haystack
  match_prefix      1 to compare if needle is a prefix of haystack

Returns:      0               needle not found in haystack
              1               needle found
*/

static int eq_octet(const struct String *needle,
  const struct String *haystack, int match_prefix)
{
size_t nl,hl;
const uschar *n,*h;

nl=needle->length;
n=needle->character;
hl=haystack->length;
h=haystack->character;
while (nl>0 && hl>0)
  {
#if !HAVE_ICONV
  if (*n&0x80) return 0;
  if (*h&0x80) return 0;
#endif
  if (*n!=*h) return 0;
  ++n;
  ++h;
  --nl;
  --hl;
  }
return (match_prefix ? nl==0 : nl==0 && hl==0);
}


/*************************************************
*    ASCII case-insensitive string comparison    *
*************************************************/

/*
Arguments:
  needle            UTF-8 string to search ...
  haystack          ... inside the haystack
  match_prefix      1 to compare if needle is a prefix of haystack

Returns:      0               needle not found in haystack
              1               needle found
*/

static int eq_asciicase(const struct String *needle,
  const struct String *haystack, int match_prefix)
{
size_t nl,hl;
const uschar *n,*h;
uschar nc,hc;

nl=needle->length;
n=needle->character;
hl=haystack->length;
h=haystack->character;
while (nl>0 && hl>0)
  {
  nc=*n;
  hc=*h;
#if !HAVE_ICONV
  if (nc&0x80) return 0;
  if (hc&0x80) return 0;
#endif
  /* tolower depends on the locale and only ASCII case must be insensitive */
  if ((nc&0x80) || (hc&0x80)) { if (nc!=hc) return 0; }
  else if ((nc>='A' && nc<='Z' ? nc|0x20 : nc) != (hc>='A' && hc<='Z' ? hc|0x20 : hc)) return 0;
  ++n;
  ++h;
  --nl;
  --hl;
  }
return (match_prefix ? nl==0 : nl==0 && hl==0);
}


/*************************************************
*        Octet-wise glob pattern search          *
*************************************************/

/*
Arguments:
  needle      pattern to search ...
  haystack    ... inside the haystack

Returns:      0               needle not found in haystack
              1               needle found
*/

static int eq_octetglob(const struct String *needle,
  const struct String *haystack)
{
struct String n,h;

n=*needle;
h=*haystack;
while (n.length)
  {
  switch (n.character[0])
    {
    case '*':
      {
      int currentLength;

      ++n.character;
      --n.length;
      /* The greedy match is not yet well tested.  Some day we may */
      /* need to refer to the matched parts, so the code is already */
      /* prepared for that. */
#if 1
      /* greedy match */
      currentLength=h.length;
      h.character+=h.length;
      h.length=0;
      while (h.length<=currentLength)
        {
        if (eq_octetglob(&n,&h)) return 1;
        else /* go back one octet */
          {
          --h.character;
          ++h.length;
          }
        }
      return 0;
#else
      /* minimal match */
      while (h.length)
        {
        if (eq_octetglob(&n,&h)) return 1;
        else /* advance one octet */
          {
          ++h.character;
          --h.length;
          }
        }
      break;
#endif
      }
    case '?':
      {
      if (h.length)
        {
        ++h.character;
        --h.length;
        ++n.character;
        --n.length;
        }
      else return 0;
      break;
      }
    case '\\':
      {
      ++n.character;
      --n.length;
      /* FALLTHROUGH */
      }
    default:
      {
      if
        (
        h.length==0 ||
#if !HAVE_ICONV
        (h.character[0]&0x80) || (n.character[0]&0x80) ||
#endif
        h.character[0]!=n.character[0]
        ) return 0;
      else
        {
        ++h.character;
        --h.length;
        ++n.character;
        --n.length;
        };
      }
    }
  }
return (h.length==0);
}


/*************************************************
*   ASCII case-insensitive glob pattern search   *
*************************************************/

/*
Arguments:
  needle      UTF-8 pattern to search ...
  haystack    ... inside the haystack

Returns:      0               needle not found in haystack
              1               needle found
*/

static int eq_asciicaseglob(const struct String *needle,
  const struct String *haystack)
{
struct String n,h;

n=*needle;
h=*haystack;
while (n.length)
  {
  switch (n.character[0])
    {
    case '*':
      {
      int currentLength;

      ++n.character;
      --n.length;
      /* The greedy match is not yet well tested.  Some day we may */
      /* need to refer to the matched parts, so the code is already */
      /* prepared for that. */
#if 1
      /* greedy match */
      currentLength=h.length;
      h.character+=h.length;
      h.length=0;
      while (h.length<=currentLength)
        {
        if (eq_asciicaseglob(&n,&h)) return 1;
        else /* go back one UTF-8 character */
          {
          if (h.length==currentLength) return 0;
          --h.character;
          ++h.length;
          if (h.character[0]&0x80)
            {
            while (h.length<currentLength && (*(h.character-1)&0x80))
              {
              --h.character;
              ++h.length;
              }
            }
          }
        }
      /* NOTREACHED */
#else
      while (h.length)
        {
        if (eq_asciicaseglob(&n,&h)) return 1;
        else /* advance one UTF-8 character */
          {
          if (h.character[0]&0x80)
            {
            while (h.length && (h.character[0]&0x80))
              {
              ++h.character;
              --h.length;
              }
            }
          else
            {
            ++h.character;
            --h.length;
            }
          }
        }
      break;
#endif
      }
    case '?':
      {
      if (h.length)
        {
        ++n.character;
        --n.length;
        /* advance one UTF-8 character */
        if (h.character[0]&0x80)
          {
          while (h.length && (h.character[0]&0x80))
            {
            ++h.character;
            --h.length;
            }
          }
        else
          {
          ++h.character;
          --h.length;
          }
        }
      else return 0;
      break;
      }
    case '\\':
      {
      ++n.character;
      --n.length;
      /* FALLTHROUGH */
      }
    default:
      {
      char nc,hc;

      if (h.length==0) return 0;
      nc=n.character[0];
      hc=h.character[0];
#if !HAVE_ICONV
      if ((hc&0x80) || (nc&0x80)) return 0;
#endif
      /* tolower depends on the locale and only ASCII case must be insensitive */
      if ((nc&0x80) || (hc&0x80)) { if (nc!=hc) return 0; }
      else if ((nc>='A' && nc<='Z' ? nc|0x20 : nc) != (hc>='A' && hc<='Z' ? hc|0x20 : hc)) return 0;
      ++h.character;
      --h.length;
      ++n.character;
      --n.length;
      }
    }
  }
return (h.length==0);
}


/*************************************************
*             Compare strings                    *
*************************************************/

/*
Arguments:
  needle      UTF-8 pattern or string to search ...
  haystack    ... inside the haystack
  co          comparator to use
  mt          match type to use

Returns:      0               needle not found in haystack
              1               needle found
*/

static int compare(const struct String *needle, const struct String *haystack,
  enum Comparator co, enum MatchType mt)
{
int r=0;

if ((filter_test != NULL && debug_selector != 0) ||
  (debug_selector & D_filter) != 0)
  {
  debug_printf("String comparison (match ");
  switch (mt)
    {
    case MATCH_IS: debug_printf(":is"); break;
    case MATCH_CONTAINS: debug_printf(":contains"); break;
    case MATCH_MATCHES: debug_printf(":matches"); break;
    }
  debug_printf(", comparison \"");
  switch (co)
    {
    case COMP_OCTET: debug_printf("i;octet"); break;
    case COMP_ASCII_CASEMAP: debug_printf("i;ascii-casemap"); break;
    }
  debug_printf("\"):\n");
  debug_printf("  Search = %s (%d chars)\n", needle->character,needle->length);
  debug_printf("  Inside = %s (%d chars)\n", haystack->character,haystack->length);
  }
switch (mt)
  {
  case MATCH_IS:
    {
    switch (co)
      {
      case COMP_OCTET:
        {
        if (eq_octet(needle,haystack,0)) r=1;
        break;
        }
      case COMP_ASCII_CASEMAP:
        {
        if (eq_asciicase(needle,haystack,0)) r=1;
        break;
        }
      }
    break;
    }
  case MATCH_CONTAINS:
    {
    struct String h;

    switch (co)
      {
      case COMP_OCTET:
        {
        for (h=*haystack; h.length; ++h.character,--h.length) if (eq_octet(needle,&h,1)) { r=1; break; }
        break;
        }
      case COMP_ASCII_CASEMAP:
        {
        for (h=*haystack; h.length; ++h.character,--h.length) if (eq_asciicase(needle,&h,1)) { r=1; break; }
        break;
        }
      }
    break;
    }
  case MATCH_MATCHES:
    {
    switch (co)
      {
      case COMP_OCTET:
        {
        if (eq_octetglob(needle,haystack)) r=1;
        break;
        }
      case COMP_ASCII_CASEMAP:
        {
        if (eq_asciicaseglob(needle,haystack)) r=1;
        break;
        }
      }
    break;
    }
  }
if ((filter_test != NULL && debug_selector != 0) ||
  (debug_selector & D_filter) != 0)
  debug_printf("  Result %s\n",r?"true":"false");
return r;
}


/*************************************************
*         Check header field syntax              *
*************************************************/

/*
RFC 2822, section 3.6.8 says:

  field-name      =       1*ftext

  ftext           =       %d33-57 /               ; Any character except
                          %d59-126                ;  controls, SP, and
                                                  ;  ":".

That forbids 8-bit header fields.  This implementation accepts them, since
all of Exim is 8-bit clean, so it adds %d128-%d255.

Arguments:
  header      header field to quote for suitable use in Exim expansions

Returns:      0               string is not a valid header field
              1               string is a value header field
*/

static int is_header(const struct String *header)
{
size_t l;
const uschar *h;

l=header->length;
h=header->character;
if (l==0) return 0;
while (l)
  {
  if (((unsigned char)*h)<33 || ((unsigned char)*h)==':' || ((unsigned char)*h)==127) return 0;
  else
    {
    ++h;
    --l;
    }
  }
return 1;
}


/*************************************************
*       Quote special characters string          *
*************************************************/

/*
Arguments:
  header      header field to quote for suitable use in Exim expansions
              or as debug output

Returns:      quoted string
*/

static const uschar *quote(const struct String *header)
{
uschar *quoted=NULL;
int size=0,ptr=0;
size_t l;
const uschar *h;

l=header->length;
h=header->character;
while (l)
  {
  switch (*h)
    {
    case '\0':
      {
      quoted=string_cat(quoted,&size,&ptr,CUS "\\0",2);
      break;
      }
    case '$':
    case '{':
    case '}':
      {
      quoted=string_cat(quoted,&size,&ptr,CUS "\\",1);
      }
    default:
      {
      quoted=string_cat(quoted,&size,&ptr,h,1);
      }
    }
  ++h;
  --l;
  }
quoted=string_cat(quoted,&size,&ptr,CUS "",1);
return quoted;
}


/*************************************************
*   Add address to list of generated addresses   *
*************************************************/

/*
According to RFC 3028, duplicate delivery to the same address must
not happen, so the list is first searched for the address.

Arguments:
  generated   list of generated addresses
  addr        new address to add
  file        address denotes a file

Returns:      nothing
*/

static void add_addr(address_item **generated, uschar *addr, int file)
{
address_item *new_addr;

for (new_addr=*generated; new_addr; new_addr=new_addr->next)
  {
  if (Ustrcmp(new_addr->address,addr)==0 && (file ? testflag(new_addr, af_pfr|af_file) : 1))
    {
    if ((filter_test != NULL && debug_selector != 0) || (debug_selector & D_filter) != 0)
      {
      debug_printf("Repeated %s `%s' ignored.\n",file ? "fileinto" : "redirect", addr);
      }
    return;
    }
  }

if ((filter_test != NULL && debug_selector != 0) || (debug_selector & D_filter) != 0)
  {
  debug_printf("%s `%s'\n",file ? "fileinto" : "redirect", addr);
  }
new_addr=deliver_make_addr(addr,TRUE);
if (file)
  {
  setflag(new_addr, af_pfr|af_file);
  new_addr->mode = 0;
  }
new_addr->p.errors_address = NULL;
new_addr->next = *generated;
*generated = new_addr;
}


/*************************************************
*         Return decoded header field            *
*************************************************/

/*
Arguments:
  value       returned value of the field
  header      name of the header field

Returns:      nothing          The expanded string is empty
                               in case there is no such header
*/

static void expand_header(struct String *value, const struct String *header)
{
uschar *s,*r,*t;
uschar *errmsg;

value->length=0;
value->character=(uschar*)0;

t=r=s=expand_string(string_sprintf("$rheader_%s",quote(header)));
while (*r==' ') ++r;
while (*r)
  {
  if (*r=='\n')
    {
    ++r;
    while (*r==' ' || *r=='\t') ++r;
    if (*r) *t++=' ';
    }
  else
    *t++=*r++;
  }
*t++='\0';
value->character=rfc2047_decode(s,TRUE,US"utf-8",'\0',&value->length,&errmsg);
}


/*************************************************
*        Parse remaining hash comment            *
*************************************************/

/*
Token definition:
  Comment up to terminating CRLF

Arguments:
  filter      points to the Sieve filter including its state

Returns:      1                success
              -1               syntax error
*/

static int parse_hashcomment(struct Sieve *filter)
{
++filter->pc;
while (*filter->pc)
  {
#ifdef RFC_EOL
  if (*filter->pc=='\r' && *(filter->pc+1)=='\n')
#else
  if (*filter->pc=='\n')
#endif
    {
#ifdef RFC_EOL
    filter->pc+=2;
#else
    ++filter->pc;
#endif
    ++filter->line;
    return 1;
    }
  else ++filter->pc;
  }
filter->errmsg=CUS "missing end of comment";
return -1;
}


/*************************************************
*       Parse remaining C-style comment          *
*************************************************/

/*
Token definition:
  Everything up to star slash

Arguments:
  filter      points to the Sieve filter including its state

Returns:      1                success
              -1               syntax error
*/

static int parse_comment(struct Sieve *filter)
{
  filter->pc+=2;
  while (*filter->pc)
  {
    if (*filter->pc=='*' && *(filter->pc+1)=='/')
    {
      filter->pc+=2;
      return 1;
    }
    else ++filter->pc;
  }
  filter->errmsg=CUS "missing end of comment";
  return -1;
}


/*************************************************
*         Parse optional white space             *
*************************************************/

/*
Token definition:
  Spaces, tabs, CRLFs, hash comments or C-style comments

Arguments:
  filter      points to the Sieve filter including its state

Returns:      1                success
              -1               syntax error
*/

static int parse_white(struct Sieve *filter)
{
while (*filter->pc)
  {
  if (*filter->pc==' ' || *filter->pc=='\t') ++filter->pc;
#ifdef RFC_EOL
  else if (*filter->pc=='\r' && *(filter->pc+1)=='\n')
#else
  else if (*filter->pc=='\n')
#endif
    {
#ifdef RFC_EOL
    filter->pc+=2;
#else
    ++filter->pc;
#endif
    ++filter->line;
    }
  else if (*filter->pc=='#')
    {
    if (parse_hashcomment(filter)==-1) return -1;
    }
  else if (*filter->pc=='/' && *(filter->pc+1)=='*')
    {
    if (parse_comment(filter)==-1) return -1;
    }
  else break;
  }
return 1;
}


/*************************************************
*          Parse a optional string               *
*************************************************/

/*
Token definition:
   quoted-string = DQUOTE *CHAR DQUOTE
           ;; in general, \ CHAR inside a string maps to CHAR
           ;; so \" maps to " and \\ maps to \
           ;; note that newlines and other characters are all allowed
           ;; in strings

   multi-line          = "text:" *(SP / HTAB) (hash-comment / CRLF)
                         *(multi-line-literal / multi-line-dotstuff)
                         "." CRLF
   multi-line-literal  = [CHAR-NOT-DOT *CHAR-NOT-CRLF] CRLF
   multi-line-dotstuff = "." 1*CHAR-NOT-CRLF CRLF
           ;; A line containing only "." ends the multi-line.
           ;; Remove a leading '.' if followed by another '.'.
  string           = quoted-string / multi-line

Arguments:
  filter      points to the Sieve filter including its state
  id          specifies identifier to match

Returns:      1                success
              -1               syntax error
              0                identifier not matched
*/

static int parse_string(struct Sieve *filter, struct String *data)
{
int dataCapacity=0;

data->length=0;
data->character=(uschar*)0;
if (*filter->pc=='"') /* quoted string */
  {
  ++filter->pc;
  while (*filter->pc)
    {
    if (*filter->pc=='"') /* end of string */
      {
      int foo=data->length;

      ++filter->pc;
      data->character=string_cat(data->character,&dataCapacity,&foo,CUS "",1);
      return 1;
      }
    else if (*filter->pc=='\\' && *(filter->pc+1)) /* quoted character */
      {
      if (*(filter->pc+1)=='0') data->character=string_cat(data->character,&dataCapacity,&data->length,CUS "",1);
      else data->character=string_cat(data->character,&dataCapacity,&data->length,filter->pc+1,1);
      filter->pc+=2;
      }
    else /* regular character */
      {
      data->character=string_cat(data->character,&dataCapacity,&data->length,filter->pc,1);
      filter->pc++;
      }
    }
  filter->errmsg=CUS "missing end of string";
  return -1;
  }
else if (Ustrncmp(filter->pc,CUS "text:",5)==0) /* multiline string */
  {
  filter->pc+=5;
  /* skip optional white space followed by hashed comment or CRLF */
  while (*filter->pc==' ' || *filter->pc=='\t') ++filter->pc;
  if (*filter->pc=='#')
    {
    if (parse_hashcomment(filter)==-1) return -1;
    }
#ifdef RFC_EOL
  else if (*filter->pc=='\r' && *(filter->pc+1)=='\n')
#else
  else if (*filter->pc=='\n')
#endif
    {
#ifdef RFC_EOL
    filter->pc+=2;
#else
    ++filter->pc;
#endif
    ++filter->line;
    }
  else
    {
    filter->errmsg=CUS "syntax error";
    return -1;
    }
  while (*filter->pc)
    {
#ifdef RFC_EOL
    if (*filter->pc=='\r' && *(filter->pc+1)=='\n') /* end of line */
#else
    if (*filter->pc=='\n') /* end of line */
#endif
      {
      data->character=string_cat(data->character,&dataCapacity,&data->length,CUS "\r\n",2);
#ifdef RFC_EOL
      filter->pc+=2;
#else
      ++filter->pc;
#endif
      ++filter->line;
#ifdef RFC_EOL
      if (*filter->pc=='.' && *(filter->pc+1)=='\r' && *(filter->pc+2)=='\n') /* end of string */
#else
      if (*filter->pc=='.' && *(filter->pc+1)=='\n') /* end of string */
#endif
        {
        data->character=string_cat(data->character,&dataCapacity,&data->length,CUS "",1);
#ifdef RFC_EOL
        filter->pc+=3;
#else
        filter->pc+=2;
#endif
        ++filter->line;
        return 1;
        }
      else if (*filter->pc=='.' && *(filter->pc+1)=='.') /* remove dot stuffing */
        {
        data->character=string_cat(data->character,&dataCapacity,&data->length,CUS ".",1);
        filter->pc+=2;
        }
      }
    else /* regular character */
      {
      data->character=string_cat(data->character,&dataCapacity,&data->length,filter->pc,1);
      filter->pc++;
      }
    }
  filter->errmsg=CUS "missing end of multi line string";
  return -1;
  }
else return 0;
}


/*************************************************
*          Parse a specific identifier           *
*************************************************/

/*
Token definition:
  identifier       = (ALPHA / "_") *(ALPHA DIGIT "_")

Arguments:
  filter      points to the Sieve filter including its state
  id          specifies identifier to match

Returns:      1                success
              0                identifier not matched
*/

static int parse_identifier(struct Sieve *filter, const uschar *id)
{
  size_t idlen=Ustrlen(id);

  if (Ustrncmp(filter->pc,id,idlen)==0)
  {
    uschar next=filter->pc[idlen];

    if ((next>='A' && next<='Z') || (next>='a' && next<='z') || next=='_' || (next>='0' && next<='9')) return 0;
    filter->pc+=idlen;
    return 1;
  }
  else return 0;
}


/*************************************************
*                 Parse a number                 *
*************************************************/

/*
Token definition:
  number           = 1*DIGIT [QUANTIFIER]
  QUANTIFIER       = "K" / "M" / "G"

Arguments:
  filter      points to the Sieve filter including its state
  data        returns value

Returns:      1                success
              -1               no string list found
*/

static int parse_number(struct Sieve *filter, unsigned long *data)
{
unsigned long d,u;

if (*filter->pc>='0' && *filter->pc<='9')
  {
  uschar *e;

  errno=0;
  d=Ustrtoul(filter->pc,&e,10);
  if (errno==ERANGE)
    {
    filter->errmsg=CUstrerror(ERANGE);
    return -1;
    }
  filter->pc=e;
  u=1;
  if (*filter->pc=='K') { u=1024; ++filter->pc; }
  else if (*filter->pc=='M') { u=1024*1024; ++filter->pc; }
  else if (*filter->pc=='G') { u=1024*1024*1024; ++filter->pc; }
  if (d>(ULONG_MAX/u))
    {
    filter->errmsg=CUstrerror(ERANGE);
    return -1;
    }
  d*=u;
  *data=d;
  return 1;
  }
else
  {
  filter->errmsg=CUS "missing number";
  return -1;
  }
}


/*************************************************
*              Parse a string list               *
*************************************************/

/*
Grammar:
  string-list      = "[" string *("," string) "]" / string

Arguments:
  filter      points to the Sieve filter including its state
  data        returns string list

Returns:      1                success
              -1               no string list found
*/

static int parse_stringlist(struct Sieve *filter, struct String **data)
{
const uschar *orig=filter->pc;
int dataCapacity=0;
int dataLength=0;
struct String *d=(struct String*)0;
int m;

if (*filter->pc=='[') /* string list */
  {
  ++filter->pc;
  for (;;)
    {
    if (parse_white(filter)==-1) goto error;
    if ((dataLength+1)>=dataCapacity) /* increase buffer */
      {
      struct String *new;
      int newCapacity;          /* Don't amalgamate with next line; some compilers grumble */
      newCapacity=dataCapacity?(dataCapacity*=2):(dataCapacity=4);
      if ((new=(struct String*)store_get(sizeof(struct String)*newCapacity))==(struct String*)0)
        {
        filter->errmsg=CUstrerror(errno);
        goto error;
        }
      if (d) memcpy(new,d,sizeof(struct String)*dataLength);
      d=new;
      dataCapacity=newCapacity;
      }
    m=parse_string(filter,&d[dataLength]);
    if (m==0)
      {
      if (dataLength==0) break;
      else
        {
        filter->errmsg=CUS "missing string";
        goto error;
        }
      }
    else if (m==-1) goto error;
    else ++dataLength;
    if (parse_white(filter)==-1) goto error;
    if (*filter->pc==',') ++filter->pc;
    else break;
    }
  if (*filter->pc==']')
    {
    d[dataLength].character=(uschar*)0;
    ++filter->pc;
    *data=d;
    return 1;
    }
  else
    {
    filter->errmsg=CUS "missing closing bracket";
    goto error;
    }
  }
else /* single string */
  {
  if ((d=store_get(sizeof(struct String)*2))==(struct String*)0)
    {
    return -1;
    }
  m=parse_string(filter,&d[0]);
  if (m==-1)
    {
    return -1;
    }
  else if (m==0)
    {
    filter->pc=orig;
    return 0;
    }
  else
    {
    d[1].character=(uschar*)0;
    *data=d;
    return 1;
    }
  }
error:
filter->errmsg=CUS "missing string list";
return -1;
}


/*************************************************
*    Parse an optional address part specifier    *
*************************************************/

/*
Grammar:
  address-part = ":user" / ":detail" / ":localpart" / ":domain" / ":all"

Arguments:
  filter      points to the Sieve filter including its state
  a           returns address part specified

Returns:      1                success
              0                no comparator found
              -1               syntax error
*/

static int parse_addresspart(struct Sieve *filter, enum AddressPart *a)
{
#ifdef SUBADDRESS
if (parse_identifier(filter,CUS ":user")==1)
  {
  if (!filter->require_subaddress)
    {
    filter->errmsg=CUS "missing previous require \"subaddress\";";
    return -1;
    }
  *a=ADDRPART_USER;
  return 1;
  }
else if (parse_identifier(filter,CUS ":detail")==1)
  {
  if (!filter->require_subaddress)
    {
    filter->errmsg=CUS "missing previous require \"subaddress\";";
    return -1;
    }
  *a=ADDRPART_DETAIL;
  return 1;
  }
else
#endif
if (parse_identifier(filter,CUS ":localpart")==1)
  {
  *a=ADDRPART_LOCALPART;
  return 1;
  }
else if (parse_identifier(filter,CUS ":domain")==1)
  {
  *a=ADDRPART_DOMAIN;
  return 1;
  }
else if (parse_identifier(filter,CUS ":all")==1)
  {
  *a=ADDRPART_ALL;
  return 1;
  }
else return 0;
}


/*************************************************
*         Parse an optional comparator           *
*************************************************/

/*
Grammar:
  comparator = ":comparator" <comparator-name: string>

Arguments:
  filter      points to the Sieve filter including its state
  c           returns comparator

Returns:      1                success
              0                no comparator found
*/

static int parse_comparator(struct Sieve *filter, enum Comparator *c)
{
struct String comparator_name;

if (parse_identifier(filter,CUS ":comparator")==0) return 0;
if (parse_white(filter)==-1) return -1;
switch (parse_string(filter,&comparator_name))
  {
  case -1: return -1;
  case 0:
    {
    filter->errmsg=CUS "missing comparator";
    return -1;
    }
  default:
    {
    int match;

    if (eq_asciicase(&comparator_name,&str_ioctet,0))
      {
      *c=COMP_OCTET;
      match=1;
      }
    else if (eq_asciicase(&comparator_name,&str_iascii_casemap,0))
      {
      *c=COMP_ASCII_CASEMAP;
      match=1;
      }
    else
      {
      filter->errmsg=CUS "invalid comparator";
      match=-1;
      }
    return match;
    }
  }
}


/*************************************************
*          Parse an optional match type          *
*************************************************/

/*
Grammar:
  match-type = ":is" / ":contains" / ":matches"

Arguments:
  filter      points to the Sieve filter including its state
  m           returns match type

Returns:      1                success
              0                no match type found
*/

static int parse_matchtype(struct Sieve *filter, enum MatchType *m)
{
  if (parse_identifier(filter,CUS ":is")==1)
  {
    *m=MATCH_IS;
    return 1;
  }
  else if (parse_identifier(filter,CUS ":contains")==1)
  {
    *m=MATCH_CONTAINS;
    return 1;
  }
  else if (parse_identifier(filter,CUS ":matches")==1)
  {
    *m=MATCH_MATCHES;
    return 1;
  }
  else return 0;
}


/*************************************************
*   Parse and interpret an optional test list    *
*************************************************/

/*
Grammar:
  test-list = "(" test *("," test) ")"

Arguments:
  filter      points to the Sieve filter including its state
  n           total number of tests
  true        number of passed tests
  exec        Execute parsed statements

Returns:      1                success
              0                no test list found
              -1               syntax or execution error
*/

static int parse_testlist(struct Sieve *filter, int *n, int *true, int exec)
{
if (parse_white(filter)==-1) return -1;
if (*filter->pc=='(')
  {
  ++filter->pc;
  *n=0;
   *true=0;
  for (;;)
    {
    int cond;

    switch (parse_test(filter,&cond,exec))
      {
      case -1: return -1;
      case 0: filter->errmsg=CUS "missing test"; return -1;
      default: ++*n; if (cond) ++*true; break;
      }
    if (parse_white(filter)==-1) return -1;
    if (*filter->pc==',') ++filter->pc;
    else break;
    }
  if (*filter->pc==')')
    {
    ++filter->pc;
    return 1;
    }
  else
    {
    filter->errmsg=CUS "missing closing paren";
    return -1;
    }
  }
else return 0;
}


/*************************************************
*     Parse and interpret an optional test       *
*************************************************/

/*
Arguments:
  filter      points to the Sieve filter including its state
  cond        returned condition status
  exec        Execute parsed statements

Returns:      1                success
              0                no test found
              -1               syntax or execution error
*/

static int parse_test(struct Sieve *filter, int *cond, int exec)
{
if (parse_white(filter)==-1) return -1;
if (parse_identifier(filter,CUS "address"))
  {
  /*
  address-test = "address" { [address-part] [comparator] [match-type] }
                 <header-list: string-list> <key-list: string-list>

  header-list From, To, Cc, Bcc, Sender, Resent-From, Resent-To
  */

  enum AddressPart addressPart=ADDRPART_ALL;
  enum Comparator comparator=COMP_ASCII_CASEMAP;
  enum MatchType matchType=MATCH_IS;
  struct String *hdr,*h,*key,*k;
  int m;
  int ap=0,co=0,mt=0;

  for (;;)
    {
    if (parse_white(filter)==-1) return -1;
    if ((m=parse_addresspart(filter,&addressPart))!=0)
      {
      if (m==-1) return -1;
      if (ap)
        {
        filter->errmsg=CUS "address part already specified";
        return -1;
        }
      else ap=1;
      }
    else if ((m=parse_comparator(filter,&comparator))!=0)
      {
      if (m==-1) return -1;
      if (co)
        {
        filter->errmsg=CUS "comparator already specified";
        return -1;
        }
      else co=1;
      }
    else if ((m=parse_matchtype(filter,&matchType))!=0)
      {
      if (m==-1) return -1;
      if (mt)
        {
        filter->errmsg=CUS "match type already specified";
        return -1;
        }
      else mt=1;
      }
    else break;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&hdr))!=1)
    {
    if (m==0) filter->errmsg=CUS "header string list expected";
    return -1;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&key))!=1)
    {
    if (m==0) filter->errmsg=CUS "key string list expected";
    return -1;
    }
  *cond=0;
  for (h=hdr; h->character && !*cond; ++h)
    {
    uschar *header_value=(uschar*)0,*extracted_addr,*end_addr;

    if
      (
      !eq_asciicase(h,&str_from,0)
      && !eq_asciicase(h,&str_to,0)
      && !eq_asciicase(h,&str_cc,0)
      && !eq_asciicase(h,&str_bcc,0)
      && !eq_asciicase(h,&str_sender,0)
      && !eq_asciicase(h,&str_resent_from,0)
      && !eq_asciicase(h,&str_resent_to,0)
      )
      {
      filter->errmsg=CUS "invalid header field";
      return -1;
      }
    if (exec)
      {
      /* We are only interested in addresses below, so no MIME decoding */
      header_value=expand_string(string_sprintf("$rheader_%s",quote(h)));
      if (header_value == NULL)
        {
        filter->errmsg=CUS "header string expansion failed";
        return -1;
        }
      parse_allow_group = TRUE;
      while (*header_value && !*cond)
        {
        uschar *error;
        int start, end, domain;
        int saveend;
        uschar *part=NULL;

        end_addr = parse_find_address_end(header_value, FALSE);
        saveend = *end_addr;
        *end_addr = 0;
        extracted_addr = parse_extract_address(header_value, &error, &start, &end, &domain, FALSE);

        /* Modification by PH: clever compilers complain when switching on an
        enumeration if not all the possible values appear. So we have to have
        them all; just do nothing when nothing need be done. */

        if (extracted_addr) switch (addressPart)
          {
          case ADDRPART_ALL: part=extracted_addr; break;

          case ADDRPART_USER:
#ifndef SUBADDRESS
          break;    /* Else fall through */
#endif
          case ADDRPART_LOCALPART: part=extracted_addr; part[domain-1]='\0'; break;
          case ADDRPART_DOMAIN: part=extracted_addr+domain; break;

          case ADDRPART_DETAIL:
#ifdef SUBADDRESS
          part=NULL;
#endif
          break;
          }

        *end_addr = saveend;
        if (part)
          {
          for (k=key; k->character; ++k)
            {
            struct String partStr;

            partStr.character=part;
            partStr.length=Ustrlen(part);
            if (extracted_addr && (*cond=compare(k,&partStr,comparator,matchType))) break;
            }
          }
        if (saveend == 0) break;
        header_value = end_addr + 1;
        }
      }
    }
  return 1;
  }
else if (parse_identifier(filter,CUS "allof"))
  {
  /*
  allof-test   = "allof" <tests: test-list>
  */

  int n,true;

  switch (parse_testlist(filter,&n,&true,exec))
    {
    case -1: return -1;
    case 0: filter->errmsg=CUS "missing test list"; return -1;
    default: *cond=(n==true); return 1;
    }
  }
else if (parse_identifier(filter,CUS "anyof"))
  {
  /*
  anyof-test   = "anyof" <tests: test-list>
  */

  int n,true;

  switch (parse_testlist(filter,&n,&true,exec))
    {
    case -1: return -1;
    case 0: filter->errmsg=CUS "missing test list"; return -1;
    default: *cond=(true>0); return 1;
    }
  }
else if (parse_identifier(filter,CUS "exists"))
  {
  /*
  exists-test = "exists" <header-names: string-list>
  */

  struct String *hdr,*h;
  int m;

  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&hdr))!=1)
    {
    if (m==0) filter->errmsg=CUS "header string list expected";
    return -1;
    }
  if (exec)
    {
    *cond=1;
    for (h=hdr; h->character && *cond; ++h)
      {
      uschar *header_def;

      header_def=expand_string(string_sprintf("${if def:header_%s {true}{false}}",quote(h)));
      if (header_def == NULL)
        {
        filter->errmsg=CUS "header string expansion failed";
        return -1;
        }
      if (Ustrcmp(header_def,"false")==0) *cond=0;
      }
    }
  return 1;
  }
else if (parse_identifier(filter,CUS "false"))
  {
  /*
  false-test = "false"
  */

  *cond=0;
  return 1;
  }
else if (parse_identifier(filter,CUS "header"))
  {
  /*
  header-test = "header" { [comparator] [match-type] }
                <header-names: string-list> <key-list: string-list>
  */

  enum Comparator comparator=COMP_ASCII_CASEMAP;
  enum MatchType matchType=MATCH_IS;
  struct String *hdr,*h,*key,*k;
  int m;
  int co=0,mt=0;

  for (;;)
    {
    if (parse_white(filter)==-1) return -1;
    if ((m=parse_comparator(filter,&comparator))!=0)
      {
      if (m==-1) return -1;
      if (co)
        {
        filter->errmsg=CUS "comparator already specified";
        return -1;
        }
      else co=1;
      }
    else if ((m=parse_matchtype(filter,&matchType))!=0)
      {
      if (m==-1) return -1;
      if (mt)
        {
        filter->errmsg=CUS "match type already specified";
        return -1;
        }
      else mt=1;
      }
    else break;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&hdr))!=1)
    {
    if (m==0) filter->errmsg=CUS "header string list expected";
    return -1;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&key))!=1)
    {
    if (m==0) filter->errmsg=CUS "key string list expected";
    return -1;
    }
  *cond=0;
  for (h=hdr; h->character && !*cond; ++h)
    {
    if (!is_header(h))
      {
      filter->errmsg=CUS "invalid header field";
      return -1;
      }
    if (exec)
      {
      struct String header_value;
      uschar *header_def;

      expand_header(&header_value,h);
      header_def=expand_string(string_sprintf("${if def:header_%s {true}{false}}",quote(h)));
      if (header_value.character == NULL || header_def == NULL)
        {
        filter->errmsg=CUS "header string expansion failed";
        return -1;
        }
      for (k=key; k->character; ++k)
        {
        if (Ustrcmp(header_def,"true")==0 && (*cond=compare(k,&header_value,comparator,matchType))) break;
        }
      }
    }
  return 1;
  }
else if (parse_identifier(filter,CUS "not"))
  {
  if (parse_white(filter)==-1) return -1;
  switch (parse_test(filter,cond,exec))
    {
    case -1: return -1;
    case 0: filter->errmsg=CUS "missing test"; return -1;
    default: *cond=!*cond; return 1;
    }
  }
else if (parse_identifier(filter,CUS "size"))
  {
  /*
  relop = ":over" / ":under"
  size-test = "size" relop <limit: number>
  */

  unsigned long limit;
  int overNotUnder;

  if (parse_white(filter)==-1) return -1;
  if (parse_identifier(filter,CUS ":over")) overNotUnder=1;
  else if (parse_identifier(filter,CUS ":under")) overNotUnder=0;
  else
    {
    filter->errmsg=CUS "missing :over or :under";
    return -1;
    }
  if (parse_white(filter)==-1) return -1;
  if (parse_number(filter,&limit)==-1) return -1;
  *cond=(overNotUnder ? (message_size>limit) : (message_size<limit));
  return 1;
  }
else if (parse_identifier(filter,CUS "true"))
  {
  *cond=1;
  return 1;
  }
else if (parse_identifier(filter,CUS "envelope"))
  {
  /*
  envelope-test = "envelope" { [comparator] [address-part] [match-type] }
                  <envelope-part: string-list> <key-list: string-list>

  envelope-part is case insensitive "from" or "to"
  */

  enum Comparator comparator=COMP_ASCII_CASEMAP;
  enum AddressPart addressPart=ADDRPART_ALL;
  enum MatchType matchType=MATCH_IS;
  struct String *env,*e,*key,*k;
  int m;
  int co=0,ap=0,mt=0;

  if (!filter->require_envelope)
    {
    filter->errmsg=CUS "missing previous require \"envelope\";";
    return -1;
    }
  for (;;)
    {
    if (parse_white(filter)==-1) return -1;
    if ((m=parse_comparator(filter,&comparator))!=0)
      {
      if (m==-1) return -1;
      if (co)
        {
        filter->errmsg=CUS "comparator already specified";
        return -1;
        }
      else co=1;
      }
    else if ((m=parse_addresspart(filter,&addressPart))!=0)
      {
      if (m==-1) return -1;
      if (ap)
        {
        filter->errmsg=CUS "address part already specified";
        return -1;
        }
      else ap=1;
      }
    else if ((m=parse_matchtype(filter,&matchType))!=0)
      {
      if (m==-1) return -1;
      if (mt)
        {
        filter->errmsg=CUS "match type already specified";
        return -1;
        }
      else mt=1;
      }
    else break;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&env))!=1)
    {
    if (m==0) filter->errmsg=CUS "envelope string list expected";
    return -1;
    }
  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&key))!=1)
    {
    if (m==0) filter->errmsg=CUS "key string list expected";
    return -1;
    }
  *cond=0;
  for (e=env; e->character; ++e)
    {
    const uschar *envelopeExpr=CUS 0;
    uschar *envelope=US 0;

    if (eq_asciicase(e,&str_from,0))
      {
      /* Modification by PH: clever compilers complain when switching on an
      enumeration if not all the possible values appear. So we have to have
      them all; just do nothing when nothing need be done. */

      switch (addressPart)
        {
        case ADDRPART_ALL: envelopeExpr=CUS "$sender_address"; break;

        case ADDRPART_USER:
#ifndef SUBADDRESS
        break;     /* Else fall through */
#endif
        case ADDRPART_LOCALPART: envelopeExpr=CUS "${local_part:$sender_address}"; break;
        case ADDRPART_DOMAIN: envelopeExpr=CUS "${domain:$sender_address}"; break;

        case ADDRPART_DETAIL:
#ifdef SUBADDRESS
        envelopeExpr=CUS 0;
#endif
        break;
        }
      }

    else if (eq_asciicase(e,&str_to,0))
      {
      /* Modification by PH: clever compilers complain when switching on an
      enumeration if not all the possible values appear. So we have to have
      them all; just do nothing when nothing need be done. */

      switch (addressPart)
        {
        case ADDRPART_ALL: envelopeExpr=CUS "$local_part_prefix$local_part$local_part_suffix@$domain"; break;

#ifdef SUBADDRESS
        case ADDRPART_USER: envelopeExpr=CUS "$local_part_prefix$local_part"; break;
        case ADDRPART_DETAIL: envelopeExpr=CUS "$local_part_suffix"; break;
#else
        case ADDRPART_USER:
        case ADDRPART_DETAIL:
        break;
#endif
        case ADDRPART_LOCALPART: envelopeExpr=CUS "$local_part_prefix$local_part$local_part_suffix"; break;
        case ADDRPART_DOMAIN: envelopeExpr=CUS "$domain"; break;
        }
      }
    else
      {
      filter->errmsg=CUS "invalid envelope string";
      return -1;
      }
    if (exec && envelopeExpr)
      {
      if ((envelope=expand_string(US envelopeExpr)) == NULL)
        {
        filter->errmsg=CUS "header string expansion failed";
        return -1;
        }
      for (k=key; k->character; ++k)
        {
        struct String envelopeStr;

        envelopeStr.character=envelope;
        envelopeStr.length=Ustrlen(envelope);
        if ((*cond=compare(&envelopeStr,k,comparator,matchType))) break;
        }
      }
    }
  return 1;
  }
else return 0;
}


/*************************************************
*     Parse and interpret an optional block      *
*************************************************/

/*
Arguments:
  filter      points to the Sieve filter including its state
  exec        Execute parsed statements
  generated   where to hang newly-generated addresses

Returns:      2                success by stop
              1                other success
              0                no block command found
              -1               syntax or execution error
*/

static int parse_block(struct Sieve *filter, int exec,
  address_item **generated)
{
int r;

if (parse_white(filter)==-1) return -1;
if (*filter->pc=='{')
  {
  ++filter->pc;
  if ((r=parse_commands(filter,exec,generated))==-1 || r==2) return r;
  if (*filter->pc=='}')
    {
    ++filter->pc;
    return 1;
    }
  else
    {
    filter->errmsg=CUS "missing closing brace";
    return -1;
    }
  }
else return 0;
}


/*************************************************
*           Match a semicolon                    *
*************************************************/

/*
Arguments:
  filter      points to the Sieve filter including its state

Returns:      1                success
              -1               syntax error
*/

static int parse_semicolon(struct Sieve *filter)
{
  if (parse_white(filter)==-1) return -1;
  if (*filter->pc==';')
  {
    ++filter->pc;
    return 1;
  }
  else
  {
    filter->errmsg=CUS "missing semicolon";
    return -1;
  }
}


/*************************************************
*     Parse and interpret a Sieve command        *
*************************************************/

/*
Arguments:
  filter      points to the Sieve filter including its state
  exec        Execute parsed statements
  generated   where to hang newly-generated addresses

Returns:      2                success by stop
              1                other success
              -1               syntax or execution error
*/
static int parse_commands(struct Sieve *filter, int exec,
  address_item **generated)
{
while (*filter->pc)
  {
  if (parse_white(filter)==-1) return -1;
  if (parse_identifier(filter,CUS "if"))
    {
    /*
    if-command = "if" test block *( "elsif" test block ) [ else block ]
    */

    int cond,m,unsuccessful;

    /* test block */
    if (parse_white(filter)==-1) return -1;
    if ((m=parse_test(filter,&cond,exec))==-1) return -1;
    if (m==0)
      {
      filter->errmsg=CUS "missing test";
      return -1;
      }
    m=parse_block(filter,exec ? cond : 0, generated);
    if (m==-1 || m==2) return m;
    if (m==0)
      {
      filter->errmsg=CUS "missing block";
      return -1;
      }
    unsuccessful = !cond;
    for (;;) /* elsif test block */
      {
      if (parse_white(filter)==-1) return -1;
      if (parse_identifier(filter,CUS "elsif"))
        {
        if (parse_white(filter)==-1) return -1;
        m=parse_test(filter,&cond,exec && unsuccessful);
        if (m==-1 || m==2) return m;
        if (m==0)
          {
          filter->errmsg=CUS "missing test";
          return -1;
          }
        m=parse_block(filter,exec && unsuccessful ? cond : 0, generated);
        if (m==-1 || m==2) return m;
        if (m==0)
          {
          filter->errmsg=CUS "missing block";
          return -1;
          }
        if (exec && unsuccessful && cond) unsuccessful = 0;
        }
      else break;
      }
    /* else block */
    if (parse_white(filter)==-1) return -1;
    if (parse_identifier(filter,CUS "else"))
      {
      m=parse_block(filter,exec && unsuccessful, generated);
      if (m==-1 || m==2) return m;
      if (m==0)
        {
        filter->errmsg=CUS "missing block";
        return -1;
        }
      }
    }
  else if (parse_identifier(filter,CUS "stop"))
    {
    /*
    stop-command = "stop" ";"
    */

    if (parse_semicolon(filter)==-1) return -1;
    if (exec)
      {
      filter->pc+=Ustrlen(filter->pc);
      return 2;
      }
    }
  else if (parse_identifier(filter,CUS "keep"))
    {
    /*
    keep-command = "keep" ";"
    */

    if (parse_semicolon(filter)==-1) return -1;
    if (exec)
      {
      add_addr(generated,US"inbox",1);
      filter->keep = 0;
      }
    }
  else if (parse_identifier(filter,CUS "discard"))
    {
    /*
    discard-command    = "discard" ";"
    */

    if (parse_semicolon(filter)==-1) return -1;
    if (exec) filter->keep=0;
    }
  else if (parse_identifier(filter,CUS "redirect"))
    {
    /*
    redirect-command = "redirect" "string" ";"
    */

    struct String recipient;
    int m;

    if (parse_white(filter)==-1) return -1;
    if ((m=parse_string(filter,&recipient))!=1)
      {
      if (m==0) filter->errmsg=CUS "missing redirect recipient string";
      return -1;
      }
    if (strchr(CCS recipient.character,'@')==(char*)0)
      {
      filter->errmsg=CUS "unqualified recipient address";
      return -1;
      }
    if (exec)
      {
      add_addr(generated,recipient.character,0);
      filter->keep = 0;
      }
    if (parse_semicolon(filter)==-1) return -1;
    }
  else if (parse_identifier(filter,CUS "fileinto"))
    {
    /*
    fileinto-command   = "fileinto" string ";"
    */

    struct String folder;
    uschar *s;
    int m;

    if (!filter->require_fileinto)
      {
      filter->errmsg=CUS "missing previous require \"fileinto\";";
      return -1;
      }
    if (parse_white(filter)==-1) return -1;
    if ((m=parse_string(filter,&folder))!=1)
      {
      if (m==0) filter->errmsg=CUS "missing fileinto folder string";
      return -1;
      }
    m=0; s=folder.character;
    if (folder.length==0) m=1;
    if (Ustrcmp(s,"..")==0 || Ustrncmp(s,"../",3)==0) m=1;
    else while (*s)
      {
      if (Ustrcmp(s,"/..")==0 || Ustrncmp(s,"/../",4)==0) { m=1; break; }
      ++s;
      }
    if (m)
      {
      filter->errmsg=CUS "invalid folder";
      return -1;
      }
    if (exec)
      {
      add_addr(generated, folder.character, 1);
      filter->keep = 0;
      }
    if (parse_semicolon(filter)==-1) return -1;
    }
  else break;
  }
return 1;
}


/*************************************************
*       Parse and interpret a sieve filter       *
*************************************************/

/*
Arguments:
  filter      points to the Sieve filter including its state
  exec        Execute parsed statements
  generated   where to hang newly-generated addresses

Returns:      1                success
              -1               syntax or execution error
*/

static int parse_start(struct Sieve *filter, int exec,
  address_item **generated)
{
filter->pc=filter->filter;
filter->line=1;
filter->keep=1;
filter->require_envelope=0;
filter->require_fileinto=0;
#ifdef SUBADDRESS
filter->require_subaddress=0;
#endif

if (parse_white(filter)==-1) return -1;
while (parse_identifier(filter,CUS "require"))
  {
  /*
  require-command = "require" <capabilities: string-list>
  */

  struct String *cap,*check;
  int m;

  if (parse_white(filter)==-1) return -1;
  if ((m=parse_stringlist(filter,&cap))!=1)
    {
    if (m==0) filter->errmsg=CUS "capability string list expected";
    return -1;
    }
  for (check=cap; check->character; ++check)
    {
    if (eq_asciicase(check,&str_envelope,0)) filter->require_envelope=1;
    else if (eq_asciicase(check,&str_fileinto,0)) filter->require_fileinto=1;
#ifdef SUBADDRESS
    else if (eq_asciicase(check,&str_subaddress,0)) filter->require_subaddress=1;
#endif
    else if (eq_asciicase(check,&str_comparator_ioctet,0)) ;
    else if (eq_asciicase(check,&str_comparator_iascii_casemap,0)) ;
    else
      {
      filter->errmsg=CUS "unknown capability";
      return -1;
      }
    }
    if (parse_semicolon(filter)==-1) return -1;
  }
  if (parse_commands(filter,exec,generated)==-1) return -1;
  if (*filter->pc)
    {
    filter->errmsg=CUS "syntax error";
    return -1;
    }
  return 1;
}


/*************************************************
*            Interpret a sieve filter file       *
*************************************************/

/*
Arguments:
  filter      points to the entire file, read into store as a single string
  options     controls whether various special things are allowed, and requests
              special actions (not currently used)
  generated   where to hang newly-generated addresses
  error       where to pass back an error text

Returns:      FF_DELIVERED     success, a significant action was taken
              FF_NOTDELIVERED  success, no significant action
              FF_DEFER         defer requested
              FF_FAIL          fail requested
              FF_FREEZE        freeze requested
              FF_ERROR         there was a problem
*/

int
sieve_interpret(uschar *filter, int options, address_item **generated,
  uschar **error)
{
struct Sieve sieve;
int r;
uschar *msg;

options = options; /* Keep picky compilers happy */
error = error;

DEBUG(D_route) debug_printf("Sieve: start of processing\n");
sieve.filter=filter;
#ifdef COMPILE_SYNTAX_CHECKER
if (parse_start(&sieve,0,generated)==1)
#else
if (parse_start(&sieve,1,generated)==1)
#endif
  {
  if (sieve.keep)
    {
    add_addr(generated,US"inbox",1);
    msg = string_sprintf("Keep");
    r = FF_DELIVERED;
    }
    else
    {
    msg = string_sprintf("No keep");
    r = FF_DELIVERED;
    }
  }
else
  {
  msg = string_sprintf("Sieve error: %s in line %d",sieve.errmsg,sieve.line);
#ifdef COMPILE_SYNTAX_CHECKER
  r = FF_ERROR;
  *error = msg;
#else
  add_addr(generated,US"inbox",1);
  r = FF_DELIVERED;
#endif
  }

#ifndef COMPILE_SYNTAX_CHECKER
if (filter_test != NULL) printf("%s\n", (const char*) msg);
  else debug_printf("%s\n", msg);
#endif

DEBUG(D_route) debug_printf("Sieve: end of processing\n");
return r;
}
