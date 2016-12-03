/* $Id: xio-fd.h,v 1.9 2003/07/07 08:58:34 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_fd_h_included
#define __xio_fd_h_included 1

extern const struct optdesc opt_append;
extern const struct optdesc opt_nonblock;
extern const struct optdesc opt_o_ndelay;
extern const struct optdesc opt_async;
extern const struct optdesc opt_o_binary;
extern const struct optdesc opt_o_text;
extern const struct optdesc opt_o_noinherit;
extern const struct optdesc opt_cloexec;
extern const struct optdesc opt_ftruncate32;
extern const struct optdesc opt_ftruncate64;
extern const struct optdesc opt_group;
extern const struct optdesc opt_group_late;
extern const struct optdesc opt_perm;
extern const struct optdesc opt_perm_late;
extern const struct optdesc opt_user;
extern const struct optdesc opt_user_late;
extern const struct optdesc opt_lseek32_cur;
extern const struct optdesc opt_lseek32_end;
extern const struct optdesc opt_lseek32_set;
extern const struct optdesc opt_lseek64_cur;
extern const struct optdesc opt_lseek64_end;
extern const struct optdesc opt_lseek64_set;
extern const struct optdesc opt_flock_sh;
extern const struct optdesc opt_flock_sh_nb;
extern const struct optdesc opt_flock_ex;
extern const struct optdesc opt_flock_ex_nb;
extern const struct optdesc opt_f_setlk;
extern const struct optdesc opt_f_setlkw;

#endif /* !defined(__xio_fd_h_included) */
