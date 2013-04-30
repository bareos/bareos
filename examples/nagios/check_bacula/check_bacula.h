/*
 * Includes specific to the tray monitor
 *
 *     Nicolas Boichat, August MMIV
 *
 *    Version $Id: tray-monitor.h,v 1.6 2004/08/25 12:20:01 nboichat Exp $
 */
/*
   Copyright (C) 2004 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

/*
 * Resource codes -- they must be sequential for indexing
 */
enum rescode {
   R_MONITOR = 1001,
   R_DIRECTOR,
   R_CLIENT,
   R_STORAGE,
   R_FIRST = R_MONITOR,
   R_LAST  = R_STORAGE                /* keep this updated */
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

/* Director */
struct DIRRES {
   RES   hdr;
   int   DIRport;                     /* UA server port */
   char *address;                     /* UA server address */
   char *password;                    /* UA server password */
   int enable_ssl;                    /* Use SSL */
};

/*
 *   Tray Monitor Resource
 *
 */
struct MONITOR {
   RES   hdr;
   int require_ssl;                   /* Require SSL for all connections */
   MSGS *messages;                    /* Daemon message handler */
   char *password;                    /* UA server password */
   utime_t RefreshInterval;           /* Status refresh interval */
   utime_t FDConnectTimeout;          /* timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* timeout in seconds */
};


/*
 *   Client Resource
 *
 */
struct CLIENT {
   RES   hdr;

   int   FDport;                      /* Where File daemon listens */
   char *address;
   char *password;
   int enable_ssl;                    /* Use SSL */
};

/*
 *   Store Resource
 *
 */
struct STORE {
   RES   hdr;

   int   SDport;                      /* port where Directors connect */
   char *address;
   char *password;
   int enable_ssl;                    /* Use SSL */
};



/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   MONITOR    res_monitor;
   DIRRES     res_dir;
   CLIENT     res_client;
   STORE      res_store;
   RES        hdr;
};



struct monitoritem {
   rescode type; /* R_DIRECTOR, R_CLIENT or R_STORAGE */
   void* resource; /* DIRRES*, CLIENT* or STORE* */
   BSOCK *D_sock;
};
