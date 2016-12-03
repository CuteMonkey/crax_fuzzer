/* $Id: xio-openssl.h,v 1.5 2004/06/20 21:31:02 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_openssl_included
#define __xio_openssl_included 1

#if WITH_OPENSSL	/* make this address configure dependend */

#define SSLIO_BASE 0x53530000	/* "SSxx" */
#define SSLIO_MASK 0xffff0000

extern const struct addrdesc addr_openssl;
extern const struct addrdesc addr_openssl_listen;

extern const struct optdesc opt_openssl_cipherlist;
extern const struct optdesc opt_openssl_method;
extern const struct optdesc opt_openssl_verify;
extern const struct optdesc opt_openssl_certificate;
extern const struct optdesc opt_openssl_key;
extern const struct optdesc opt_openssl_cafile;
extern const struct optdesc opt_openssl_capath;
extern const struct optdesc opt_openssl_egd;
extern const struct optdesc opt_openssl_pseudo;

extern int
   _xioopen_openssl_prepare(struct opt *opts, struct single *xfd,
			    bool server, bool *opt_ver, const char *opt_cert,
			    SSL_CTX **ctx);
extern int
   _xioopen_openssl_connect(struct single *xfd,  bool opt_ver,
			    SSL_CTX *ctx, int level);
extern int
   _xioopen_openssl_listen(struct single *xfd, bool opt_ver,
			   SSL_CTX *ctx, int level);
extern int xioclose_openssl(xiofile_t *xfd);
extern int xioshutdown_openssl(xiofile_t *xfd, int how);
extern ssize_t xioread_openssl(struct single *file, void *buff, size_t bufsiz);
extern ssize_t xiowrite_openssl(struct single *file, const void *buff, size_t bufsiz);

#endif /* WITH_OPENSSL */

#endif /* !defined(__xio_openssl_included) */
