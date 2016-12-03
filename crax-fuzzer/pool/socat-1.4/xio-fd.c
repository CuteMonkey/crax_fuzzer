/* $Id: xio-fd.c,v 1.21 2003/07/07 08:58:34 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains common file descriptor related option definitions */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-fd.h"

/****** for ALL addresses - with open() or fcntl(F_SETFL) ******/
const struct optdesc opt_append    = { "append",    NULL, OPT_O_APPEND,    GROUP_OPEN|GROUP_FD, PH_LATE, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_APPEND };
const struct optdesc opt_nonblock  = { "nonblock",  NULL, OPT_O_NONBLOCK,  GROUP_OPEN|GROUP_FD, PH_FD, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_NONBLOCK };
#if defined(O_NDELAY) && (!defined(O_NONBLOCK) || O_NDELAY != O_NONBLOCK)
const struct optdesc opt_o_ndelay  = { "o-ndelay",  NULL, OPT_O_NDELAY,  GROUP_OPEN|GROUP_FD, PH_LATE, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_NDELAY };
#endif
#ifdef O_ASYNC
const struct optdesc opt_async     = { "async",     NULL, OPT_O_ASYNC,     GROUP_OPEN|GROUP_FD, PH_LATE, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_ASYNC };
#endif
#ifdef O_BINARY
const struct optdesc opt_o_binary    = { "o-binary",    "binary",    OPT_O_BINARY,    GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_BINARY };
#endif
#ifdef O_TEXT
const struct optdesc opt_o_text      = { "o-text",      "text",      OPT_O_TEXT,      GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_TEXT };
#endif
#ifdef O_NOINHERIT
const struct optdesc opt_o_noinherit = { "o-noinherit", "noinherit", OPT_O_NOINHERIT, GROUP_OPEN, PH_OPEN, TYPE_BOOL, OFUNC_FCNTL, F_SETFL, O_NOINHERIT };
#endif
/****** for ALL addresses - with fcntl(F_SETFD) ******/
const struct optdesc opt_cloexec   = { "cloexec",   NULL, OPT_CLOEXEC,   GROUP_FD, PH_LATE, TYPE_BOOL, OFUNC_FCNTL, F_SETFD, FD_CLOEXEC };
/****** ftruncate() ******/
/* this record is good for ftruncate() or ftruncate64() if available */
#if HAVE_FTRUNCATE64
const struct optdesc opt_ftruncate32  = { "ftruncate32",  NULL,       OPT_FTRUNCATE32,  GROUP_REG, PH_LATE, TYPE_OFF32, OFUNC_SPEC };
const struct optdesc opt_ftruncate64  = { "ftruncate64",  "truncate", OPT_FTRUNCATE64,  GROUP_REG, PH_LATE, TYPE_OFF64, OFUNC_SPEC };
#else
const struct optdesc opt_ftruncate32  = { "ftruncate32",  "truncate", OPT_FTRUNCATE32,  GROUP_REG, PH_LATE, TYPE_OFF32, OFUNC_SPEC };
#endif /* !HAVE_FTRUNCATE64 */
/****** for ALL addresses - permissions, ownership, and positioning ******/
const struct optdesc opt_group     = { "group",     "gid",   OPT_GROUP,     GROUP_FD|GROUP_NAMED,PH_FD,TYPE_GIDT,OFUNC_SPEC };
const struct optdesc opt_group_late= { "group-late","gid-l", OPT_GROUP_LATE,GROUP_FD, PH_LATE,  TYPE_GIDT, OFUNC_SPEC };
const struct optdesc opt_perm      = { "perm",      "mode",  OPT_PERM,      GROUP_FD|GROUP_NAMED, PH_FD,    TYPE_MODET,OFUNC_SPEC };
const struct optdesc opt_perm_late = { "perm-late", NULL,    OPT_PERM_LATE, GROUP_FD, PH_LATE,  TYPE_MODET,OFUNC_SPEC };
const struct optdesc opt_user      = { "user",      "uid",   OPT_USER,      GROUP_FD|GROUP_NAMED, PH_FD,    TYPE_UIDT, OFUNC_SPEC };
const struct optdesc opt_user_late = { "user-late", "uid-l", OPT_USER_LATE, GROUP_FD, PH_LATE,  TYPE_UIDT, OFUNC_SPEC };
/* for something like random access files */
#if HAVE_LSEEK64
const struct optdesc opt_lseek32_cur  = { "lseek32-cur",  NULL,       OPT_SEEK32_CUR,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_CUR };
const struct optdesc opt_lseek32_end  = { "lseek32-end",  NULL,       OPT_SEEK32_END,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_END };
const struct optdesc opt_lseek32_set  = { "lseek32-set",  NULL,       OPT_SEEK32_SET,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_SET };
const struct optdesc opt_lseek64_cur  = { "lseek64-cur",  "seek-cur", OPT_SEEK64_CUR,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF64, OFUNC_SEEK64, SEEK_CUR };
const struct optdesc opt_lseek64_end  = { "lseek64-end",  "seek-end", OPT_SEEK64_END,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF64, OFUNC_SEEK64, SEEK_END };
const struct optdesc opt_lseek64_set  = { "lseek64-set",  "seek",     OPT_SEEK64_SET,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF64, OFUNC_SEEK64, SEEK_SET };
#else
const struct optdesc opt_lseek32_cur  = { "lseek32-cur",  "seek-cur", OPT_SEEK32_CUR,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_CUR };
const struct optdesc opt_lseek32_end  = { "lseek32-end",  "seek-end", OPT_SEEK32_END,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_END };
const struct optdesc opt_lseek32_set  = { "lseek32-set",  "seek",     OPT_SEEK32_SET,  GROUP_REG|GROUP_BLK, PH_LATE,  TYPE_OFF32, OFUNC_SEEK32, SEEK_SET };
#endif /* !HAVE_LSEEK64 */
/* for all addresses (?) */
const struct optdesc opt_f_setlk   = { "f-setlk",   "setlk", OPT_F_SETLK,   GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_SPEC, F_SETLK };
const struct optdesc opt_f_setlkw  = { "f-setlkw",  "setlkw",OPT_F_SETLKW,  GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_SPEC, F_SETLKW };
#if HAVE_FLOCK
const struct optdesc opt_flock_sh     = { "flock-sh",    NULL,    OPT_FLOCK_SH,    GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_FLOCK, LOCK_SH };
const struct optdesc opt_flock_sh_nb  = { "flock-sh-nb", NULL,    OPT_FLOCK_SH_NB, GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_FLOCK, LOCK_SH|LOCK_NB };
const struct optdesc opt_flock_ex     = { "flock-ex",    "flock", OPT_FLOCK_EX,    GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_FLOCK, LOCK_EX };
const struct optdesc opt_flock_ex_nb  = { "flock-ex-nb", "flock-nb", OPT_FLOCK_EX_NB, GROUP_FD, PH_FD,TYPE_BOOL, OFUNC_FLOCK, LOCK_EX|LOCK_NB };
#endif /* HAVE_FLOCK */
