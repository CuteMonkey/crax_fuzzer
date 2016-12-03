/*************************************************
*     Exim - an Internet mail transport agent    *
*************************************************/

/* Copyright (c) University of Cambridge 1995 - 2004 */
/* See the file NOTICE for conditions of use and distribution. */

/* This module provides the TLS (aka SSL) support for Exim using the OpenSSL
library. It is #included into the tls.c file when that library is used. The
code herein is based on a patch that was originally contributed by Steve
Haslam. It was adapted from stunnel, a GPL program by Michal Trojnara.

No cryptographic code is included in Exim. All this module does is to call
functions from the OpenSSL library. */


/* Heading stuff */

#include <openssl/lhash.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

/* Structure for collecting random data for seeding. */

typedef struct randstuff {
  time_t t;
  pid_t  p;
} randstuff;

/* Local static variables */

static BOOL verify_callback_called = FALSE;
static const uschar *sid_ctx = US"exim";

static SSL_CTX *ctx = NULL;
static SSL *ssl = NULL;

static char ssl_errstring[256];

static int  ssl_session_timeout = 200;
static BOOL verify_optional = FALSE;





/*************************************************
*               Handle TLS error                 *
*************************************************/

/* Called from lots of places when errors occur before actually starting to do
the TLS handshake, that is, while the session is still in clear. Always returns
DEFER for a server and FAIL for a client so that most calls can use "return
tls_error(...)" to do this processing and then give an appropriate return. A
single function is used for both server and client, because it is called from
some shared functions.

Argument:
  prefix    text to include in the logged error
  host      NULL if setting up a server;
            the connected host if setting up a client

Returns:    OK/DEFER/FAIL
*/

static int
tls_error(uschar *prefix, host_item *host)
{
ERR_error_string(ERR_get_error(), ssl_errstring);
if (host == NULL)
  {
  log_write(0, LOG_MAIN, "TLS error on connection from %s (%s): %s",
    (sender_fullhost != NULL)? sender_fullhost : US"local process",
    prefix, ssl_errstring);
  return DEFER;
  }
else
  {
  log_write(0, LOG_MAIN, "TLS error on connection to %s [%s] (%s): %s",
    host->name, host->address, prefix, ssl_errstring);
  return FAIL;
  }
}



/*************************************************
*        Callback to generate RSA key            *
*************************************************/

/*
Arguments:
  s          SSL connection
  export     not used
  keylength  keylength

Returns:     pointer to generated key
*/

static RSA *
rsa_callback(SSL *s, int export, int keylength)
{
RSA *rsa_key;
export = export;     /* Shut picky compilers up */
DEBUG(D_tls) debug_printf("Generating %d bit RSA key...\n", keylength);
rsa_key = RSA_generate_key(keylength, RSA_F4, NULL, NULL);
if (rsa_key == NULL)
  {
  ERR_error_string(ERR_get_error(), ssl_errstring);
  log_write(0, LOG_MAIN|LOG_PANIC, "TLS error (RSA_generate_key): %s",
    ssl_errstring);
  return NULL;
  }
return rsa_key;
}




/*************************************************
*        Callback for verification               *
*************************************************/

/* The SSL library does certificate verification if set up to do so. This
callback has the current yes/no state is in "state". If verification succeeded,
we set up the tls_peerdn string. If verification failed, what happens depends
on whether the client is required to present a verifiable certificate or not.

If verification is optional, we change the state to yes, but still log the
verification error. For some reason (it really would help to have proper
documentation of OpenSSL), this callback function then gets called again, this
time with state = 1. In fact, that's useful, because we can set up the peerdn
value, but we must take care not to set the private verified flag on the second
time through.

Note: this function is not called if the client fails to present a certificate
when asked. We get here only if a certificate has been received. Handling of
optional verification for this case is done when requesting SSL to verify, by
setting SSL_VERIFY_FAIL_IF_NO_PEER_CERT in the non-optional case.

Arguments:
  state      current yes/no state as 1/0
  x509ctx    certificate information.

Returns:     1 if verified, 0 if not
*/

static int
verify_callback(int state, X509_STORE_CTX *x509ctx)
{
static uschar txt[256];

X509_NAME_oneline(X509_get_subject_name(x509ctx->current_cert),
  CS txt, sizeof(txt));

if (state == 0)
  {
  log_write(0, LOG_MAIN, "SSL verify error: depth=%d error=%s cert=%s",
    x509ctx->error_depth,
    X509_verify_cert_error_string(x509ctx->error),
    txt);
  tls_certificate_verified = FALSE;
  verify_callback_called = TRUE;
  if (!verify_optional) return 0;    /* reject */
  DEBUG(D_tls) debug_printf("SSL verify failure overridden (host in "
    "tls_try_verify_hosts)\n");
  return 1;                          /* accept */
  }

if (x509ctx->error_depth != 0)
  {
  DEBUG(D_tls) debug_printf("SSL verify ok: depth=%d cert=%s\n",
     x509ctx->error_depth, txt);
  }
else
  {
  DEBUG(D_tls) debug_printf("SSL%s peer: %s\n",
    verify_callback_called? "" : " authenticated", txt);
  tls_peerdn = txt;
  }


debug_printf("+++verify_callback_called=%d\n", verify_callback_called);

if (!verify_callback_called) tls_certificate_verified = TRUE;
verify_callback_called = TRUE;

return 1;   /* accept */
}



/*************************************************
*           Information callback                 *
*************************************************/

/* The SSL library functions call this from time to time to indicate what they
are doing. We copy the string to the debugging output when the level is high
enough.

Arguments:
  s         the SSL connection
  where
  ret

Returns:    nothing
*/

static void
info_callback(SSL *s, int where, int ret)
{
where = where;
ret = ret;
DEBUG(D_tls) debug_printf("SSL info: %s\n", SSL_state_string_long(s));
}



/*************************************************
*                Initialize for DH               *
*************************************************/

/* If dhparam is set, expand it, and load up the parameters for DH encryption.

Arguments:
  dhparam   DH parameter file

Returns:    TRUE if OK (nothing to set up, or setup worked)
*/

static BOOL
init_dh(uschar *dhparam)
{
BOOL yield = TRUE;
BIO *bio;
DH *dh;
uschar *dhexpanded;

if (!expand_check(dhparam, US"tls_dhparam", &dhexpanded))
  return FALSE;

if (dhexpanded == NULL) return TRUE;

if ((bio = BIO_new_file(CS dhexpanded, "r")) == NULL)
  {
  log_write(0, LOG_MAIN, "DH: could not read %s: %s", dhexpanded,
    strerror(errno));
  yield = FALSE;
  }
else
  {
  if ((dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL)) == NULL)
    {
    log_write(0, LOG_MAIN, "DH: could not load params from %s",
      dhexpanded);
    yield = FALSE;
    }
  else
    {
    SSL_CTX_set_tmp_dh(ctx, dh);
    DEBUG(D_tls)
      debug_printf("Diffie-Hellman initialized from %s with %d-bit key\n",
        dhexpanded, 8*DH_size(dh));
    DH_free(dh);
    }
  BIO_free(bio);
  }

return yield;
}




/*************************************************
*            Initialize for TLS                  *
*************************************************/

/* Called from both server and client code, to do preliminary initialization of
the library.

Arguments:
  host            connected host, if client; NULL if server
  dhparam         DH parameter file
  certificate     certificate file
  privatekey      private key
  addr            address if client; NULL if server (for some randomness)

Returns:          OK/DEFER/FAIL
*/

static int
tls_init(host_item *host, uschar *dhparam, uschar *certificate, uschar *privatekey,
  address_item *addr)
{
SSL_load_error_strings();          /* basic set up */
OpenSSL_add_ssl_algorithms();

/* Create a context */

ctx = SSL_CTX_new((host == NULL)?
  SSLv23_server_method() : SSLv23_client_method());

if (ctx == NULL) return tls_error(US"SSL_CTX_new", host);

/* It turns out that we need to seed the random number generator this early in
order to get the full complement of ciphers to work. It took me roughly a day
of work to discover this by experiment.

On systems that have /dev/urandom, SSL may automatically seed itself from
there. Otherwise, we have to make something up as best we can. Double check
afterwards. */

if (!RAND_status())
  {
  randstuff r;
  r.t = time(NULL);
  r.p = getpid();

  RAND_seed((uschar *)(&r), sizeof(r));
  RAND_seed((uschar *)big_buffer, big_buffer_size);
  if (addr != NULL) RAND_seed((uschar *)addr, sizeof(addr));

  if (!RAND_status())
    {
    if (host == NULL)
      {
      log_write(0, LOG_MAIN, "TLS error on connection from %s: "
        "unable to seed random number generator",
        (sender_fullhost != NULL)? sender_fullhost : US"local process");
      return DEFER;
      }
    else
      {
      log_write(0, LOG_MAIN, "TLS error on connection to %s [%s]: "
        "unable to seed random number generator",
        host->name, host->address);
      return FAIL;
      }
    }
  }

/* Set up the information callback, which outputs if debugging is at a suitable
level. */

if (!(SSL_CTX_set_info_callback(ctx, (void (*)())info_callback)))
  return tls_error(US"SSL_CTX_set_info_callback", host);

/* The following patch was supplied by Robert Roselius */

#if OPENSSL_VERSION_NUMBER > 0x00906040L
/* Enable client-bug workaround.
   Versions of OpenSSL as of 0.9.6d include a "CBC countermeasure" feature,
   which causes problems with some clients (such as the Certicom SSL Plus
   library used by Eudora).  This option, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS,
   disables the coutermeasure allowing Eudora to connect.
   Some poppers and MTAs use SSL_OP_ALL, which enables all such bug
   workarounds. */
/* XXX (Silently?) ignore failure here? XXX*/

if (!(SSL_CTX_set_options(ctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)))
  return tls_error(US"SSL_CTX_set_option", host);
#endif

/* Initialize with DH parameters if supplied */

if (!init_dh(dhparam)) return DEFER;

/* Set up certificate and key */

if (certificate != NULL)
  {
  uschar *expanded;
  if (!expand_check(certificate, US"tls_certificate", &expanded))
    return DEFER;

  if (expanded != NULL)
    {
    DEBUG(D_tls) debug_printf("tls_certificate file %s\n", expanded);
    if (!SSL_CTX_use_certificate_chain_file(ctx, CS expanded))
      return tls_error(US"SSL_CTX_use_certificate_chain_file", host);
    }

  if (privatekey != NULL &&
      !expand_check(privatekey, US"tls_privatekey", &expanded))
    return DEFER;

  if (expanded != NULL)
    {
    DEBUG(D_tls) debug_printf("tls_privatekey file %s\n", expanded);
    if (!SSL_CTX_use_PrivateKey_file(ctx, CS expanded, SSL_FILETYPE_PEM))
      return tls_error(US"SSL_CTX_use_PrivateKey_file", host);
    }
  }

/* Set up the RSA callback */

SSL_CTX_set_tmp_rsa_callback(ctx, rsa_callback);

/* Finally, set the timeout, and we are done */

SSL_CTX_set_timeout(ctx, ssl_session_timeout);
DEBUG(D_tls) debug_printf("Initialized TLS\n");
return OK;
}




/*************************************************
*           Get name of cipher in use            *
*************************************************/

/* The answer is left in a static buffer, and tls_cipher is set to point
to it.

Argument:   pointer to an SSL structure for the connection
Returns:    nothing
*/

static void
construct_cipher_name(SSL *ssl)
{
static uschar cipherbuf[256];
SSL_CIPHER *c;
uschar *ver;
int bits;

switch (ssl->session->ssl_version)
  {
  case SSL2_VERSION:
  ver = US"SSLv2";
  break;

  case SSL3_VERSION:
  ver = US"SSLv3";
  break;

  case TLS1_VERSION:
  ver = US"TLSv1";
  break;

  default:
  ver = US"UNKNOWN";
  }

c = SSL_get_current_cipher(ssl);
SSL_CIPHER_get_bits(c, &bits);

string_format(cipherbuf, sizeof(cipherbuf), "%s:%s:%u", ver,
  SSL_CIPHER_get_name(c), bits);
tls_cipher = cipherbuf;

DEBUG(D_tls) debug_printf("Cipher: %s\n", cipherbuf);
}





/*************************************************
*        Set up for verifying certificates       *
*************************************************/

/* Called by both client and server startup

Arguments:
  certs         certs file or NULL
  crl           CRL file or NULL
  host          NULL in a server; the remote host in a client
  optional      TRUE if called from a server for a host in tls_try_verify_hosts;
                otherwise passed as FALSE

Returns:        OK/DEFER/FAIL
*/

static int
setup_certs(uschar *certs, uschar *crl, host_item *host, BOOL optional)
{
uschar *expcerts, *expcrl;

if (!expand_check(certs, US"tls_verify_certificates", &expcerts))
  return DEFER;

if (expcerts != NULL)
  {
  struct stat statbuf;
  if (!SSL_CTX_set_default_verify_paths(ctx))
    return tls_error(US"SSL_CTX_set_default_verify_paths", host);

  if (Ustat(expcerts, &statbuf) < 0)
    {
    log_write(0, LOG_MAIN|LOG_PANIC,
      "failed to stat %s for certificates", expcerts);
    return DEFER;
    }
  else
    {
    uschar *file, *dir;
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
      { file = NULL; dir = expcerts; }
    else
      { file = expcerts; dir = NULL; }

    /* If a certificate file is empty, the next function fails with an
    unhelpful error message. If we skip it, we get the correct behaviour (no
    certificates are recognized, but the error message is still misleading (it
    says no certificate was supplied.) But this is better. */

    if ((file == NULL || statbuf.st_size > 0) &&
          !SSL_CTX_load_verify_locations(ctx, CS file, CS dir))
      return tls_error(US"SSL_CTX_load_verify_locations", host);

    if (file != NULL)
      {
      SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(CS file));
      }
    }

  /* Handle a certificate revocation list. */

  #if OPENSSL_VERSION_NUMBER > 0x00907000L

  if (!expand_check(crl, US"tls_crl", &expcrl)) return DEFER;
  if (expcrl != NULL && *expcrl != 0)
    {
    BIO *crl_bio;
    X509_CRL *crl_x509;
    X509_STORE *cvstore;

    cvstore = SSL_CTX_get_cert_store(ctx);  /* cert validation store */

    crl_bio = BIO_new(BIO_s_file_internal());
    if (crl_bio != NULL)
      {
      if (BIO_read_filename(crl_bio, expcrl))
        {
        crl_x509 = PEM_read_bio_X509_CRL(crl_bio, NULL, NULL, NULL);
        BIO_free(crl_bio);
        X509_STORE_add_crl(cvstore, crl_x509);
        X509_CRL_free(crl_x509);
        X509_STORE_set_flags(cvstore,
          X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
        }
      else
        {
        BIO_free(crl_bio);
        return tls_error(US"BIO_read_filename", host);
        }
      }
    else return tls_error(US"BIO_new", host);
    }

  #endif  /* OPENSSL_VERSION_NUMBER > 0x00907000L */

  /* If verification is optional, don't fail if no certificate */

  SSL_CTX_set_verify(ctx,
    SSL_VERIFY_PEER | (optional? 0 : SSL_VERIFY_FAIL_IF_NO_PEER_CERT),
    verify_callback);
  }

return OK;
}



/*************************************************
*       Start a TLS session in a server          *
*************************************************/

/* This is called when Exim is running as a server, after having received
the STARTTLS command. It must respond to that command, and then negotiate
a TLS session.

Arguments:
  require_ciphers   allowed ciphers

Returns:            OK on success
                    DEFER for errors before the start of the negotiation
                    FAIL for errors during the negotation; the server can't
                      continue running.
*/

int
tls_server_start(uschar *require_ciphers)
{
int rc;
uschar *expciphers;

/* Check for previous activation */

if (tls_active >= 0)
  {
  log_write(0, LOG_MAIN, "STARTTLS received in already encrypted "
    "connection from %s",
    (sender_fullhost != NULL)? sender_fullhost : US"local process");
  smtp_printf("554 Already in TLS\r\n");
  return FAIL;
  }

/* Initialize the SSL library. If it fails, it will already have logged
the error. */

rc = tls_init(NULL, tls_dhparam, tls_certificate, tls_privatekey, NULL);
if (rc != OK) return rc;

if (!expand_check(require_ciphers, US"tls_require_ciphers", &expciphers))
  return FAIL;

/* In OpenSSL, cipher components are separated by hyphens. In GnuTLS, they
are separated by underscores. So that I can use either form in my tests, and
also for general convenience, we turn underscores into hyphens here. */

if (expciphers != NULL)
  {
  uschar *s = expciphers;
  while (*s != 0) { if (*s == '_') *s = '-'; s++; }
  DEBUG(D_tls) debug_printf("required ciphers: %s\n", expciphers);
  if (!SSL_CTX_set_cipher_list(ctx, CS expciphers))
    return tls_error(US"SSL_CTX_set_cipher_list", NULL);
  }

/* If this is a host for which certificate verification is mandatory or
optional, set up appropriately. */

tls_certificate_verified = FALSE;
verify_callback_called = FALSE;

if (verify_check_host(&tls_verify_hosts) == OK)
  {
  rc = setup_certs(tls_verify_certificates, tls_crl, NULL, FALSE);
  if (rc != OK) return rc;
  verify_optional = FALSE;
  }
else if (verify_check_host(&tls_try_verify_hosts) == OK)
  {
  rc = setup_certs(tls_verify_certificates, tls_crl, NULL, TRUE);
  if (rc != OK) return rc;
  verify_optional = TRUE;
  }

/* Prepare for new connection */

if ((ssl = SSL_new(ctx)) == NULL) return tls_error(US"SSL_new", NULL);
SSL_clear(ssl);

/* Set context and tell client to go ahead, except in the case of TLS startup
on connection, where outputting anything now upsets the clients and tends to
make them disconnect. We need to have an explicit fflush() here, to force out
the response. Other smtp_printf() calls do not need it, because in non-TLS
mode, the fflush() happens when smtp_getc() is called. */

SSL_set_session_id_context(ssl, sid_ctx, Ustrlen(sid_ctx));
if (!tls_on_connect)
  {
  smtp_printf("220 TLS go ahead\r\n");
  fflush(smtp_out);
  }

/* Now negotiate the TLS session. We put our own timer on it, since it seems
that the OpenSSL library doesn't. */

SSL_set_fd(ssl, fileno(smtp_out));
SSL_set_accept_state(ssl);

DEBUG(D_tls) debug_printf("Calling SSL_accept\n");

sigalrm_seen = FALSE;
if (smtp_receive_timeout > 0) alarm(smtp_receive_timeout);
rc = SSL_accept(ssl);
alarm(0);

if (rc <= 0)
  {
  if (sigalrm_seen) Ustrcpy(ssl_errstring, "timed out");
    else ERR_error_string(ERR_get_error(), ssl_errstring);
  log_write(0, LOG_MAIN, "TLS error on connection from %s (SSL_accept): %s",
    (sender_fullhost != NULL)? sender_fullhost : US"local process",
    ssl_errstring);
  return FAIL;
  }

DEBUG(D_tls) debug_printf("SSL_accept was successful\n");

/* TLS has been set up. Adjust the input functions to read via TLS,
and initialize things. */

construct_cipher_name(ssl);

DEBUG(D_tls)
  {
  uschar buf[2048];
  if (SSL_get_shared_ciphers(ssl, CS buf, sizeof(buf)) != NULL)
    debug_printf("Shared ciphers: %s\n", buf);
  }


ssl_xfer_buffer = store_malloc(ssl_xfer_buffer_size);
ssl_xfer_buffer_lwm = ssl_xfer_buffer_hwm = 0;
ssl_xfer_eof = ssl_xfer_error = 0;

receive_getc = tls_getc;
receive_ungetc = tls_ungetc;
receive_feof = tls_feof;
receive_ferror = tls_ferror;

tls_active = fileno(smtp_out);
return OK;
}





/*************************************************
*    Start a TLS session in a client             *
*************************************************/

/* Called from the smtp transport after STARTTLS has been accepted.

Argument:
  fd               the fd of the connection
  host             connected host (for messages)
  dhparam          DH parameter file
  certificate      certificate file
  privatekey       private key file
  verify_certs     file for certificate verify
  crl              file containing CRL
  require_ciphers  list of allowed ciphers

Returns:           OK on success
                   FAIL otherwise - note that tls_error() will not give DEFER
                     because this is not a server
*/

int
tls_client_start(int fd, host_item *host, address_item *addr, uschar *dhparam,
  uschar *certificate, uschar *privatekey, uschar *verify_certs, uschar *crl,
  uschar *require_ciphers, int timeout)
{
static uschar txt[256];
uschar *expciphers;
X509* server_cert;
int rc;

rc = tls_init(host, dhparam, certificate, privatekey, addr);
if (rc != OK) return rc;

tls_certificate_verified = FALSE;
verify_callback_called = FALSE;

if (!expand_check(require_ciphers, US"tls_require_ciphers", &expciphers))
  return FAIL;

/* In OpenSSL, cipher components are separated by hyphens. In GnuTLS, they
are separated by underscores. So that I can use either form in my tests, and
also for general convenience, we turn underscores into hyphens here. */

if (expciphers != NULL)
  {
  uschar *s = expciphers;
  while (*s != 0) { if (*s == '_') *s = '-'; s++; }
  DEBUG(D_tls) debug_printf("required ciphers: %s\n", expciphers);
  if (!SSL_CTX_set_cipher_list(ctx, CS expciphers))
    return tls_error(US"SSL_CTX_set_cipher_list", host);
  }

rc = setup_certs(verify_certs, crl, host, FALSE);
if (rc != OK) return rc;

if ((ssl = SSL_new(ctx)) == NULL) return tls_error(US"SSL_new", host);
SSL_set_session_id_context(ssl, sid_ctx, Ustrlen(sid_ctx));
SSL_set_fd(ssl, fd);
SSL_set_connect_state(ssl);

/* There doesn't seem to be a built-in timeout on connection. */

DEBUG(D_tls) debug_printf("Calling SSL_connect\n");
sigalrm_seen = FALSE;
alarm(timeout);
rc = SSL_connect(ssl);
alarm(0);

if (rc <= 0)
  {
  if (sigalrm_seen)
    {
    log_write(0, LOG_MAIN, "TLS error on connection to %s [%s]: "
      "SSL_connect timed out", host->name, host->address);
    return FAIL;
    }
  else return tls_error(US"SSL_connect", host);
  }

DEBUG(D_tls) debug_printf("SSL_connect succeeded\n");

server_cert = SSL_get_peer_certificate (ssl);
tls_peerdn = US X509_NAME_oneline(X509_get_subject_name(server_cert),
  CS txt, sizeof(txt));
tls_peerdn = txt;

construct_cipher_name(ssl);   /* Sets tls_cipher */

tls_active = fd;
return OK;
}





/*************************************************
*            TLS version of getc                 *
*************************************************/

/* This gets the next byte from the TLS input buffer. If the buffer is empty,
it refills the buffer via the SSL reading function.

Arguments:  none
Returns:    the next character or EOF
*/

int
tls_getc(void)
{
if (ssl_xfer_buffer_lwm >= ssl_xfer_buffer_hwm)
  {
  int error;
  int inbytes;

  DEBUG(D_tls) debug_printf("Calling SSL_read(%lx, %lx, %u)\n", (long)ssl,
    (long)ssl_xfer_buffer, ssl_xfer_buffer_size);

  if (smtp_receive_timeout > 0) alarm(smtp_receive_timeout);
  inbytes = SSL_read(ssl, CS ssl_xfer_buffer, ssl_xfer_buffer_size);
  error = SSL_get_error(ssl, inbytes);
  alarm(0);

  /* SSL_ERROR_ZERO_RETURN appears to mean that the SSL session has been
  closed down, not that the socket itself has been closed down. Revert to
  non-SSL handling. */

  if (error == SSL_ERROR_ZERO_RETURN)
    {
    DEBUG(D_tls) debug_printf("Got SSL_ERROR_ZERO_RETURN\n");

    receive_getc = smtp_getc;
    receive_ungetc = smtp_ungetc;
    receive_feof = smtp_feof;
    receive_ferror = smtp_ferror;

    SSL_free(ssl);
    ssl = NULL;
    tls_active = -1;
    tls_cipher = NULL;
    tls_peerdn = NULL;

    return smtp_getc();
    }

  /* Handle genuine errors */

  else if (error != SSL_ERROR_NONE)
    {
    DEBUG(D_tls) debug_printf("Got SSL error %d\n", error);
    ssl_xfer_error = 1;
    return EOF;
    }

  ssl_xfer_buffer_hwm = inbytes;
  ssl_xfer_buffer_lwm = 0;
  }

/* Something in the buffer; return next uschar */

return ssl_xfer_buffer[ssl_xfer_buffer_lwm++];
}



/*************************************************
*          Read bytes from TLS channel           *
*************************************************/

/*
Arguments:
  buff      buffer of data
  len       size of buffer

Returns:    the number of bytes read
            -1 after a failed read
*/

int
tls_read(uschar *buff, size_t len)
{
int inbytes;
int error;

DEBUG(D_tls) debug_printf("Calling SSL_read(%lx, %lx, %u)\n", (long)ssl,
  (long)buff, len);

inbytes = SSL_read(ssl, CS buff, len);
error = SSL_get_error(ssl, inbytes);

if (error == SSL_ERROR_ZERO_RETURN)
  {
  DEBUG(D_tls) debug_printf("Got SSL_ERROR_ZERO_RETURN\n");
  return -1;
  }
else if (error != SSL_ERROR_NONE)
  {
  return -1;
  }

return inbytes;
}





/*************************************************
*         Write bytes down TLS channel           *
*************************************************/

/*
Arguments:
  buff      buffer of data
  len       number of bytes

Returns:    the number of bytes after a successful write,
            -1 after a failed write
*/

int
tls_write(const uschar *buff, size_t len)
{
int outbytes;
int error;
int left = len;

DEBUG(D_tls) debug_printf("tls_do_write(%lx, %d)\n", (long)buff, left);
while (left > 0)
  {
  DEBUG(D_tls) debug_printf("SSL_write(SSL, %lx, %d)\n", (long)buff, left);
  outbytes = SSL_write(ssl, CS buff, left);
  error = SSL_get_error(ssl, outbytes);
  DEBUG(D_tls) debug_printf("outbytes=%d error=%d\n", outbytes, error);
  switch (error)
    {
    case SSL_ERROR_SSL:
    ERR_error_string(ERR_get_error(), ssl_errstring);
    log_write(0, LOG_MAIN, "TLS error (SSL_write): %s", ssl_errstring);
    return -1;

    case SSL_ERROR_NONE:
    left -= outbytes;
    buff += outbytes;
    break;

    case SSL_ERROR_ZERO_RETURN:
    log_write(0, LOG_MAIN, "SSL channel closed on write");
    return -1;

    default:
    log_write(0, LOG_MAIN, "SSL_write error %d", error);
    return -1;
    }
  }
return len;
}



/*************************************************
*         Close down a TLS session               *
*************************************************/

/* This is also called from within a delivery subprocess forked from the
daemon, to shut down the TLS library, without actually doing a shutdown (which
would tamper with the SSL session in the parent process).

Arguments:   TRUE if SSL_shutdown is to be called
Returns:     nothing
*/

void
tls_close(BOOL shutdown)
{
if (tls_active < 0) return;  /* TLS was not active */

if (shutdown)
  {
  DEBUG(D_tls) debug_printf("tls_close(): shutting down SSL\n");
  SSL_shutdown(ssl);
  }

SSL_free(ssl);
ssl = NULL;

tls_active = -1;
}

/* End of tls-openssl.c */
