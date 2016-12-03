/* $Id: xio-socket.h,v 1.9 2004/06/06 17:33:22 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_socket_h_included
#define __xio_socket_h_included 1

extern const struct optdesc opt_so_debug;
extern const struct optdesc opt_so_acceptconn;
extern const struct optdesc opt_so_broadcast;
extern const struct optdesc opt_so_reuseaddr;
extern const struct optdesc opt_so_keepalive;
extern const struct optdesc opt_so_linger;
extern const struct optdesc opt_so_linger;
extern const struct optdesc opt_so_oobinline;
extern const struct optdesc opt_so_sndbuf;
extern const struct optdesc opt_so_sndbuf_late;
extern const struct optdesc opt_so_rcvbuf;
extern const struct optdesc opt_so_rcvbuf_late;
extern const struct optdesc opt_so_error;
extern const struct optdesc opt_so_type;
extern const struct optdesc opt_so_dontroute;
extern const struct optdesc opt_so_rcvlowat;
extern const struct optdesc opt_so_rcvtimeo;
extern const struct optdesc opt_so_sndlowat;
extern const struct optdesc opt_so_sndtimeo;
extern const struct optdesc opt_so_audit;
extern const struct optdesc opt_so_attach_filter;
extern const struct optdesc opt_so_detach_filter;
extern const struct optdesc opt_so_bindtodevice;
extern const struct optdesc opt_so_bsdcompat;
extern const struct optdesc opt_so_cksumrecv;
extern const struct optdesc opt_so_kernaccept;
extern const struct optdesc opt_so_no_check;
extern const struct optdesc opt_so_noreuseaddr;
extern const struct optdesc opt_so_passcred;
extern const struct optdesc opt_so_peercred;
extern const struct optdesc opt_so_priority;
extern const struct optdesc opt_so_reuseport;
extern const struct optdesc opt_so_security_authentication;
extern const struct optdesc opt_so_security_encryption_network;
extern const struct optdesc opt_so_security_encryption_transport;
extern const struct optdesc opt_so_use_ifbufs;
extern const struct optdesc opt_so_useloopback;
extern const struct optdesc opt_so_dgram_errind;
extern const struct optdesc opt_so_dontlinger;
extern const struct optdesc opt_so_prototype;
extern const struct optdesc opt_fiosetown;
extern const struct optdesc opt_siocspgrp;
extern const struct optdesc opt_bind;

extern int xioopen_connect(struct single *fd,
			    struct sockaddr *us, size_t uslen,
			    struct sockaddr *them, size_t themlen,
			    struct opt *opts, int pf, int stype, int proto,
			    bool alt);
extern int _xioopen_connect(struct single *fd,
			    struct sockaddr *us, size_t uslen,
			    struct sockaddr *them, size_t themlen,
			    struct opt *opts, int pf, int stype, int proto,
			    bool alt, int level);

#endif /* !defined(__xio_socket_h_included) */
