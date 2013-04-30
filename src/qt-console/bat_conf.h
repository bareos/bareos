/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Bacula Adminstration Tool (bat) 
 *
 *     Kern Sibbald, March 2002
 *
 *     Version $Id$
 */

#ifndef _BAT_CONF_H_
#define _BAT_CONF_H_

/*
 * Resource codes -- they must be sequential for indexing
 */

enum {
   R_DIRECTOR = 1001,
   R_CONSOLE,
   R_CONSOLE_FONT,
   R_FIRST = R_DIRECTOR,
   R_LAST = R_CONSOLE_FONT            /* Keep this updated */
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
class DIRRES {
public:
   RES   hdr;
   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   char *password;                    /* UA server password */
   bool tls_authenticate;             /* Authenticate with tls */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   utime_t heartbeat_interval;        /* Dir heartbeat interval */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */

   /* Methods */
   char *name() const;
};

inline char *DIRRES::name() const { return hdr.name; }


struct CONFONTRES {
   RES   hdr;
   char *fontface;                    /* Console Font specification */
};

class CONRES {
public:
   RES   hdr;
   char *password;                    /* UA server password */
   bool tls_authenticate;             /* Authenticate with tls */
   bool tls_enable;                   /* Enable TLS on all connections */
   bool tls_require;                  /* Require TLS on all connections */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   utime_t heartbeat_interval;        /* Cons heartbeat interval */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */

   /* Methods */
   char *name() const;
};

inline char *CONRES::name() const { return hdr.name; }


/* Define the Union of all the above
 * resource structure definitions.
 */
union u_res {
   DIRRES dir_res;
   CONRES con_res;
   CONFONTRES con_font;
   RES hdr;
};

typedef union u_res URES;

#define GetConsoleResWithName(x) ((CONRES *)GetResWithName(R_CONSOLE, (x)))
#define GetDirResWithName(x) ((DIRRES *)GetResWithName(R_DIRECTOR, (x)))


#endif /* _BAT_CONF_H_ */
