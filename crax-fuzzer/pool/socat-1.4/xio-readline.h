/* $Id: xio-readline.h,v 1.4 2003/12/23 21:38:59 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_readline_h_included
#define __xio_readline_h_included 1

extern const struct addrdesc addr_readline;

extern const struct optdesc opt_history_file;
extern const struct optdesc opt_prompt;
extern const struct optdesc opt_noprompt;
extern const struct optdesc opt_noecho;

extern ssize_t xioread_readline(struct single *pipe, void *buff, size_t bufsiz);extern void xioscan_readline(struct single *pipe, const void *buff, size_t bytes);

#endif /* !defined(__xio_readline_h_included) */