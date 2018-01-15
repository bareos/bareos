/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

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
 * Tray Monitor specific configuration and defines
 *
 * Adapted from dird_conf.c
 *
 * Nicolas Boichat, August MMIV
 */

/* NOTE:  #includes at the end of this file */

/*
 * Resource codes -- they must be sequential for indexing
 */

#ifndef TRAY_CONF_H_INCLUDED
#define TRAY_CONF_H_INCLUDED

extern CONFIG *my_config;

enum Rescode {
   R_UNKNOWN = 0,
   R_MONITOR = 1001,
   R_DIRECTOR,
   R_CLIENT,
   R_STORAGE,
   R_CONSOLE_FONT,
   R_FIRST = R_MONITOR,
   R_LAST = R_CONSOLE_FONT                 /* keep this updated */
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

/*
 * Director Resource
 */
class DIRRES : public BRSRES {
public:
   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   bool enable_ssl;                   /* Use SSL */

   DIRRES() : BRSRES() {}
};

/*
 * Tray Monitor Resource
 */
class MONITORRES : public BRSRES {
public:
   bool require_ssl;                  /* Require SSL for all connections */
   MSGSRES *messages;                 /* Daemon message handler */
   s_password password;               /* UA server password */
   utime_t RefreshInterval;           /* Status refresh interval */
   utime_t FDConnectTimeout;          /* timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* timeout in seconds */
   utime_t DIRConnectTimeout;         /* timeout in seconds */

   MONITORRES() : TLSRES() {}
};

/*
 * Client Resource
 */
class CLIENTRES : public BRSRES {
public:
   uint32_t FDport;                   /* Where File daemon listens */
   char *address;
   s_password password;
   bool enable_ssl;                   /* Use SSL */

   CLIENTRES() : TLSRES() {}
};

/*
 * Store Resource
 */
class STORERES : public BRSRES {
public:
   uint32_t SDport;                   /* port where Directors connect */
   char *address;
   s_password password;
   bool enable_ssl;                   /* Use SSL */

   STORERES() : TLSRES() {}
};

class CONFONTRES : public BRSRES {
public:
   char *fontface;                    /* Console Font specification */
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   MONITORRES res_monitor;
   DIRRES res_dir;
   CLIENTRES res_client;
   STORERES res_store;
   CONFONTRES con_font;
   RES hdr;
};

void init_tmon_config(CONFIG *config, const char *configfile, int exit_code);
bool print_config_schema_json(POOL_MEM &buffer);

#endif /* TRAY_CONF_H_INCLUDED */
