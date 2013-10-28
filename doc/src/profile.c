/*
 * This is not real source code.  It is source for documentation which
 * will go into the Doxygen-produced documentation, in a form which is
 * convenient for Doxygen to extract, i.e. comments in what looks like C
 * source code.  Ok so maybe this is a little silly.
 */

/**
@page profile Profile File
@brief Syntax and semantics of the profile file.

@section format Syntax Overview

The profile file is in UNIX text format, with lines separated by
'\\n' (newline) characaters.

Comments begin with a '#' character and continue until the end of the line.

Most of the file is made up of strings. A string is a sequence of
characters which are not spaces or any of the other significant
characters.  C-style \\n \\r \\t and \\\\ character escape sequences are
recognised in strings.  All other uses of \\ are silently ignored, e.g.
\\x is just x. Strings can be quoted by surrounding them with '"'
(doublequote) characters; character escapes do not work inside quoted
strings but quoted strings can span whitespace including end of line.
Strings may be at most 256 characters long.

The ' ' (space) and '\\t' (tab) characters are ignored except in a quoted
string.

The file is a sequence of variable assignments, in the format `variable =
value` where both `variable` and `value` are strings.  Assignments are
terminated at the unquoted end of a line or by an unquoted ';'
(semicolon) character.

@section abnf Syntax in Detail

Here is the profile file syntax in ABNF form, which is
[defined in RFC5234](http://tools.ietf.org/html/rfc5234).

	profile = *( *LWS item ) *LWS

	item = assignment / comment

	comment = %x23 *( %x00-09 %x0b-%xff ) LF
	    ; from '#' character to newline

	assignment = variable *LWS "=" *LWS value *LWS ( LF / ";" )
	    ; assignments end at newline or semicolon

	variable = string

	value = bool / addrlist / int / inth / path

	bool = "true" / "false"
	    ; case sensitive

	addrlist = addr *( ( SP / COMMA / SEMIC ) addr )
	addr = host [ ":" port ]
	host = hostname / ipv4-addr
	hostname = dot-atom
	    ; from RFC 5322.  DNS host name, case insensitive
	ipv4-addr = 1*3DIGIT 3( "." 1*3DIGIT )
	    ; IPv4 address in dotted-quad form, see RFC 1166
	port = int

	int = 1*DIGIT

	inth = int / ( "0x" 1*XDIGIT )
	    ; integer, possibly in hexadecimal

	path = string
	    ; unix-style '/' separated pathname.  May be used
	    ; either for HTTP or as a local filesystem name.

	string = quoted-string / simple-string
	quoted-string = DQUOTE *NOTDQUOTE DQUOTE
	simple-string = 1*( STR / escaped-char )
	escaped-char = ( BSLASH "r" ) /
		       ( BSLASH "n" ) /
		       ( BSLASH "t" ) /
		       ( BSLASH BSLASH )

	TAB = %x09
	    ; tab character
	LF = %x0a
	    ; newline character
	SP = %x20
	    ; ' ' (space) character
	DQUOTE = %x22
	    ; '"' (double quote) character
	HASH = %x23
	    ; '#' (hash, pound) character
	COMMA = %x2c
	    ; ',' (comma) character
	SEMIC = %x3b
	    ; ';' (semicolon) character
	EQUAL = %x3d
	    ; '=' (equals) character
	BSLASH = %x5c
	    ; '\\' (backslash) character
	NOTDQUOTE = %x00-%x21 / %x23-%xff
	    ; anything but a double quote
	LWS = SP / TAB
	    ; linear whitespace characters
	DIGIT = %x30-39
	    ; ASCII decimal digits
	XDIGIT = %x30-39 / %x41-46 / %x61-66
	    ; ASCII hexadecimal digits
	STR = %x00-08 %x0b-1f %x21 %x24-3a %x3c %x3e-5b %x5d-ff
	    ; string characters, all characters except TAB LF SP
	    ; DQUOTE HASH SEMIC EQUAL BSLASH


@section variables Variables

This section describes the semantics of each variable which can be set
in a a profile file.

@par use_https = \<bool\>
Whether to use the https protocol (i.e. to enable TLS transport
encryption).  Default is `false`.  You should set this variable to
`true` if the cloud server is outside your internal network.

@par host = \<addrlist\>
A list of hosts to connect to.  Hosts are separated by any of the ' '
(space) ',' (comma) or ';' (semicolon) characters.  Note that the ';'
character needs to be escaped from the profile file syntax using double
quotes.  Each hostname may optionally be followed by a ':' (colon)
character and a decimal port number.  The default port is 80, or 443 if
`use_https` is `true`.

Hostnames are looked up using the operating system's name service,
which usually means that any of the following will be accepted:
hostname, FQDN, or IPv4 address literal in dotted-quad notation.
IPv6 addresses are not supported.

Connections made using the context will be spread amongst all the
listed hosts in round-robin fashion, in an order which is randomised
from the order specified in the `host` variable.

The hostnames are looked up in the operating system's name service
once when the profile is loaded, and then once more just before
each connection is made.  This may slow down connections if your
operating system does not implement a name service cache.  This may
also result in interesting behaviour if your cloud service provider
uses round-robin DNS.

There is no default host list.  All backends except `posix` require
`host` to be specified in the profile.  Examples:

	host = server
	host = server1,server2,server3
	host = server1:8080,server2:4080
	host = 192.168.100.20
	host = my.external.server.com
	host = "server1;server2"

@par port
This variable is deprecated; if specified it is ignored with a warning.

@par blacklist_expiretime = \<int\>
The number of seconds for which unresponsive hosts listed in the `host`
variable will be blacklisted.  Blacklisted hosts will not be connected
to.  The default is 10 seconds.

@par header_size = \<inth\>
The size in bytes of the buffer used to construct HTTP headers sent to
the cloud service.  The default is 8192 bytes.

@par base_path = \<path\>
This path is prepended to all HTTP resource names (or pathnames of
files, for the `posix` backend).  The default is `"/"`.

@par access_key = \<string\>
Used as the username for HTTP Basic Authorization in the `cdmi` and
`sproxy` backends, or Amazon's AWS authorization for the `s3` backend.
There is no default.

@par secret_key = \<string\>
Used as the password for HTTP Basic Authorization for the `cdmi` and
`sproxyd` backends, or Amazon's AWS authorization for the `s3` backend.
Note that with Basic the password is sent unencrypted, so you should make
sure to set `use_https` to `true` to provide any level of security at all.
There is no default.

@par ssl_cert_file = \<path\>
Specifies a pathname, either absolute or relative to the droplet
directory, to a file containing a client certificate chain which may be
sent to the server (if `use_https` is `true` and the server requests
a certificate).  The certificates in the chain must be in PEM format
and must be sorted starting with the server's certificate followed by
intermediate CA certificates if applicable, and ending at the root CA.
There is no default.

@par ssl_key_file = \<path\>
Specifies a pathname, either absolute or relative to the droplet
directory, to a file containing a client private key used in
conjunction with the client certificate chain specified by the
`ssl_cert_file` variable.  Only used when `use_https` is `true`. The
key file must be in PEM format.  There is no default.

@par ssl_password = \<string\>
Specifies a string used as the password for decrypting the SSL client
private key specified in the `ssl_key_file` variable.  Only used when
`use_https` is `true`. There is no default.

@par ssl_ca_list = \<path\>
Specifies an absolute pathname to a file containing trusted CA
certificates used when verifying a certificate chain sent by a server.
Only used when `use_https` is `true`.  The CA certificate file may
contain multiple certificates and must be in PEM format.  There is
no default value.  Note that Droplet does not automatically use the
operating system's default CA certificate stores, so if you want
any security at all you should set `ssl_ca_list` to the correct
value for your operating system.  The correct value on Ubuntu is
`"/etc/ssl/certs/ca-certificates.crt"`.

@par pricing = \<string\>
Specifies the name of a pricing model.  The pricing model is read from a
file named `<name>.pricing` in the droplet directory where `<name>` is
the name of the pricing model.  See @ref pricing "Pricing Model" for a
description of the pricing model and the syntax of the pricing file.
Pricing information is currently not used.  There is no default.

@par pricing_dir = \<path\>
Specifies the absolute path of a directory containing log files of
chargeable events.  The log file will be appended to as the library
emits requests to the server.  Note that the log file is written in
buffered mode and is not flushed until the context is closed.  The name
of the log file is `<name>.csv` where `<name>` is the name of the
profile.  See *Pricing Model* for a description of the log file.  The
default is the droplet directory, i.e. chargeable events are logged by
default.  To turn off logging specify an empty string, like this

	pricing = ""	# turns off logging of chargeable events

@par read_buf_size = \<inth\>
Size in bytes of the buffer used to read from the HTTP socket.
The default is 8192 bytes.

@par encrypt_key = \<string\>
Specifies the key which is used for the optional encryption
feature in the VFS interface.  To enable file encryption, use the
`DPL_VFILE_FLAG_ENCRYPT` flag in the `flags` parameter to either
`dpl_openread()` or `dpl_openwrite()`.  File encryption uses the
AES-256 cipher.  There is no default.

@par backend = \<string\>
Name of the protocol used to talk to the server.  The default is `s3`.  The
backends are:

- @b s3 Amazon's S3 (Simple Storage Service)
- @b cdmi The SNIA's Cloud Management Data Interface
- @b posix A backend which stores data in the local filesystem.  Useful only for testing.
- @b srws A vendor-specific RESTful interface provided by Scality's RING.
- @b sproxyd A vendor-specific RESTful interface provided by Scality's RING.

@par encode_slashes = \<bool\>
Controls whether the '/' (solidus, a.k.a. forward slash) character
is encoded when URL-encoding the resource name in HTTP requests.  Some
servers may care.  Used only when `url_encoding` is `true`.  The default
is `false`.

@par keep_alive = \<bool\>
Controls whether HTTP connection sockets are kept alive between HTTP
requests.  If set to `true`, HTTP connections are reused for multiple
requests (subject to an idle timeout).  If set to `false`, a new HTTP
connection is started for each request.  Note that this has nothing
at all to do with TCP keepalive.  The default is `true`.

@par url_encoding = \<bool\>
Controls whether the resource name in HTTP requests is URL-encoded.
Some servers may care.  The default is `true`.

@par enterprise_number = \<inth\>
Specifies an SNMP Enterprise Number, which is used in the `cdmi` backend to
generate unique ObjectIDs.  Enterprise Number assignments are published
[by IANA](http://www.iana.org/assignments/enterprise-numbers/enterprise-numbers).
For example, SGI's Enterprise Number is 59.  The default is 37489
(assigned to Scality).

 */
