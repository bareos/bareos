TLS
===

Written by Landon Fuller

Introduction to TLS
-------------------

This patch includes all the back-end code necessary to add complete TLS
data encryption support to Bareos. In addition, support for TLS in
Console/Director communications has been added as a proof of concept.
Adding support for the remaining daemons will be straight-forward.
Supported features of this patchset include:

-   Client/Server TLS Requirement Negotiation

-   TLSv1 Connections with Server and Client Certificate Validation

-   Forward Secrecy Support via Diffie-Hellman Ephemeral Keying

This document will refer to both “server” and “client” contexts. These
terms refer to the accepting and initiating peer, respectively.

Diffie-Hellman anonymous ciphers are not supported by this patchset. The
use of DH anonymous ciphers increases the code complexity and places
explicit trust upon the two-way Cram-MD5 implementation. Cram-MD5 is
subject to known plaintext attacks, and is should be considered
considerably less secure than PKI certificate-based authentication.


TLS API Implementation
----------------------

Appropriate autoconf macros have been added to detect and use OpenSSL.
Two additional preprocessor defines have been added: `HAVE_TLS` and
`HAVE_OPENSSL`. All changes not specific to OpenSSL rely on
`HAVE_TLS`. In turn, a generic TLS API is exported.

### Library Initialization and Cleanup

    int init_tls(void);

Performs TLS library initialization, including seeding of the PRNG. PRNG
seeding has not yet been implemented for win32.

    int cleanup_tls(void);

Performs TLS library cleanup.

### Manipulating TLS Contexts

    TLS_CONTEXT  *new_tls_context(const char *ca_certfile,
            const char *ca_certdir, const char *certfile,
            const char *keyfile, const char *dhfile, bool verify_peer);

Allocates and initalizes a new opaque *TLS\_CONTEXT* structure. The
*TLS\_CONTEXT* structure maintains default TLS settings from which
*TLS\_CONNECTION* structures are instantiated. In the future the
*TLS\_CONTEXT* structure may be used to maintain the TLS session cache.
*ca\_certfile* and *ca\_certdir* arguments are used to initialize the CA
verification stores. The *certfile* and *keyfile* arguments are used to
initialize the local certificate and private key. If *dhfile* is
non-NULL, it is used to initialize Diffie-Hellman ephemeral keying. If
*verify\_peer* is *true* , client certificate validation is enabled.

    void free_tls_context(TLS_CONTEXT *ctx);

Deallocated a previously allocated *TLS\_CONTEXT* structure.

### Performing Post-Connection Verification

    bool tls_postconnect_verify_host(TLS_CONNECTION *tls, const char *host);

Performs post-connection verification of the peer-supplied x509
certificate. Checks whether the *subjectAltName* and *commonName*
attributes match the supplied *host* string. Returns *true* if there is
a match, *false* otherwise.

    bool tls_postconnect_verify_cn(TLS_CONNECTION *tls, alist *verify_list);

Performs post-connection verification of the peer-supplied x509
certificate. Checks whether the *commonName* attribute matches any
strings supplied via the *verify\_list* parameter. Returns *true* if
there is a match, *false* otherwise.

### Manipulating TLS Connections

    TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd);

Allocates and initializes a new *TLS\_CONNECTION* structure with context
*ctx* and file descriptor *fd*.

    void free_tls_connection(TLS_CONNECTION *tls);

Deallocates memory associated with the *tls* structure.

    bool tls_bsock_connect(BSOCK *bsock);

Negotiates a a TLS client connection via *bsock*. Returns *true* if
successful, *false* otherwise. Will fail if there is a TLS protocol
error or an invalid certificate is presented

    bool tls_bsock_accept(BSOCK *bsock);

Accepts a TLS client connection via *bsock*. Returns *true* if
successful, *false* otherwise. Will fail if there is a TLS protocol
error or an invalid certificate is presented.

    bool tls_bsock_shutdown(BSOCK *bsock);

Issues a blocking TLS shutdown request to the peer via *bsock*. This
function may not wait for the peer’s reply.

    int tls_bsock_writen(BSOCK *bsock, char *ptr, int32_t nbytes);

Writes *nbytes* from *ptr* via the *TLS\_CONNECTION* associated with
*bsock*. Due to OpenSSL’s handling of *EINTR*, *bsock* is set
non-blocking at the start of the function, and restored to its original
blocking state before the function returns. Less than *nbytes* may be
written if an error occurs. The actual number of bytes written will be
returned.

    int tls_bsock_readn(BSOCK *bsock, char *ptr, int32_t nbytes);

Reads *nbytes* from the *TLS\_CONNECTION* associated with *bsock* and
stores the result in *ptr*. Due to OpenSSL’s handling of *EINTR*,
*bsock* is set non-blocking at the start of the function, and restored
to its original blocking state before the function returns. Less than
*nbytes* may be read if an error occurs. The actual number of bytes read
will be returned.

Bnet API Changes
----------------

A minimal number of changes were required in the Bnet socket API. The
BSOCK structure was expanded to include an associated TLS\_CONNECTION
structure, as well as a flag to designate the current blocking state of
the socket. The blocking state flag is required for win32, where it does
not appear possible to discern the current blocking state of a socket.

### Negotiating a TLS Connection

*bnet\_tls\_server()* and *bnet\_tls\_client()* were both implemented
using the new TLS API as follows:

    int bnet_tls_client(TLS_CONTEXT *ctx, BSOCK * bsock);

Negotiates a TLS session via *bsock* using the settings from *ctx*.
Returns 1 if successful, 0 otherwise.

    int bnet_tls_server(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list);

Accepts a TLS client session via *bsock* using the settings from *ctx*.
If *verify\_list* is non-NULL, it is passed to
*tls\_postconnect\_verify\_cn()* for client certificate verification.

### Manipulating Socket Blocking State

Three functions were added for manipulating the blocking state of a
socket on both Win32 and Unix-like systems. The Win32 code was written
according to the MSDN documentation, but has not been tested.

These functions are prototyped as follows:

    int bnet_set_nonblocking(BSOCK *bsock);

Enables non-blocking I/O on the socket associated with *bsock*. Returns
a copy of the socket flags prior to modification.

    int bnet_set_blocking(BSOCK *bsock);

Enables blocking I/O on the socket associated with *bsock*. Returns a
copy of the socket flags prior to modification.

    void bnet_restore_blocking(BSOCK *bsock, int flags);

Restores blocking or non-blocking IO setting on the socket associated
with *bsock*. The *flags* argument must be the return value of either
*bnet\_set\_blocking()* or *bnet\_restore\_blocking()*.

Authentication Negotiation
--------------------------

Backwards compatibility with the existing SSL negotiation hooks
implemented in src/lib/cram-md5.c have been maintained. The
*cram\_md5\_get\_auth()* function has been modified to accept an integer
pointer argument, tls\_remote\_need. The TLS requirement advertised by
the remote host is returned via this pointer.

After exchanging cram-md5 authentication and TLS requirements, both the
client and server independently decide whether to continue:

    if (!cram_md5_get_auth(dir, password, &tls_remote_need) ||
            !cram_md5_auth(dir, password, tls_local_need)) {
    [snip]
    /* Verify that the remote host is willing to meet our TLS requirements */
    if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK &&
            tls_remote_need != BNET_TLS_OK) {
       sendit(_("Authorization problem:"
                " Remote server did not advertise required TLS support.\n"));
       auth_success = false;
       goto auth_done;
    }

    /* Verify that we are willing to meet the remote host's requirements */
    if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK &&
            tls_remote_need != BNET_TLS_OK) {
       sendit(_("Authorization problem:"
                " Remote server requires TLS.\n"));
       auth_success = false;
       goto auth_done;
    }
