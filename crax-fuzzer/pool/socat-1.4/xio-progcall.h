/* $Id: xio-progcall.h,v 1.12 2003/12/23 21:22:38 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_progcall_h_included
#define __xio_progcall_h_included 1

extern const struct optdesc opt_fdin;
extern const struct optdesc opt_fdout;
extern const struct optdesc opt_path;
extern const struct optdesc opt_pipes;
extern const struct optdesc opt_pty;
extern const struct optdesc opt_openpty;
extern const struct optdesc opt_ptmx;
extern const struct optdesc opt_stderr;
extern const struct optdesc opt_nofork;
extern const struct optdesc opt_sighup;
extern const struct optdesc opt_sigint;
extern const struct optdesc opt_sigquit;

extern int _xioopen_foxec(char *comma,
		int rw,	/* O_RDONLY etc. */
		struct single *fd,
		unsigned groups,
		struct opt **opts
		);
extern int setopt_path(struct opt *opts, char **path);

#endif /* !defined(__xio_progcall_h_included) */
