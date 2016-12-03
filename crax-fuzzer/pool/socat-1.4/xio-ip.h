/* $Id: xio-ip.h,v 1.5 2003/02/23 08:33:41 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001, 2003 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ip_h_included
#define __xio_ip_h_included 1

extern const struct optdesc opt_ip_options;
extern const struct optdesc opt_ip_pktinfo;
extern const struct optdesc opt_ip_recvtos;
extern const struct optdesc opt_ip_recvttl;
extern const struct optdesc opt_ip_recvopts;
extern const struct optdesc opt_ip_retopts;
extern const struct optdesc opt_ip_tos;
extern const struct optdesc opt_ip_ttl;
extern const struct optdesc opt_ip_hdrincl;
extern const struct optdesc opt_ip_recverr;
extern const struct optdesc opt_ip_mtu_discover;
extern const struct optdesc opt_ip_mtu;
extern const struct optdesc opt_ip_freebind;
extern const struct optdesc opt_ip_router_alert;
extern const struct optdesc opt_ip_multicast_ttl;
extern const struct optdesc opt_ip_multicast_loop;
extern const struct optdesc opt_ip_pktoptions;

extern int parseipaddr(char *addr, struct sockaddr *out, const char *protoname,
		int withaddr, int withport);

#endif /* !defined(__xio_ip_h_included) */
