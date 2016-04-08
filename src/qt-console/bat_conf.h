/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2007 Free Software Foundation Europe e.V.

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
 * Bareos Adminstration Tool (bat)
 *
 * Kern Sibbald, March 2002
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
class DIRRES : public BRSRES {
public:
   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   s_password password;               /* UA server password */
   utime_t heartbeat_interval;        /* Dir heartbeat interval */
   tls_t tls;                         /* TLS structure */
};

class CONFONTRES : public BRSRES {
public:
   char *fontface;                    /* Console Font specification */
};

class CONRES : public BRSRES {
public:
   s_password password;               /* UA server password */
   utime_t heartbeat_interval;        /* Cons heartbeat interval */
   tls_t tls;                         /* TLS structure */
};

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
