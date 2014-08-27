/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Bareos User Agent specific configuration and defines
 *
 * Kern Sibbald, Sep MM
 */

/*
 * Resource codes -- they must be sequential for indexing
 */

enum {
   R_CONSOLE = 1001,
   R_DIRECTOR,
   R_FIRST = R_CONSOLE,
   R_LAST = R_DIRECTOR                /* Keep this updated */
};

/*
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/* Definition of the contents of each Resource */

/* Console "globals" */
struct CONRES {
   RES hdr;

   char *rc_file;                     /* startup file */
   char *history_file;                /* command history file */
   s_password password;               /* UA server password */
   uint32_t history_length;           /* readline history length */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS on all connections */
   bool tls_require;                  /* Require TLS on all connections */
   bool tls_verify_peer;              /* TLS Verify Peer Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_crlfile;                 /* TLS CA Certificate Revocation List File */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   char *director;                    /* bind to director */
   utime_t heartbeat_interval;        /* Interval to send heartbeats to Dir */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};

/* Director */
struct DIRRES {
   RES hdr;

   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   s_password password;               /* UA server password */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Peer Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_crlfile;                 /* TLS CA Certificate Revocation List File */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   utime_t heartbeat_interval;        /* Interval to send heartbeats to Dir */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CONRES res_cons;
   RES hdr;
};

extern CONRES *me;                    /* "Global" Client resource */
