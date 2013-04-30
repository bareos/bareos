/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.

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
 * Includes specific to the tray monitor
 *
 *     Nicolas Boichat, August MMIV
 *
 *    Version $Id$
 */

#include <gtk/gtk.h>

#include "tray_conf.h"

#include "jcr.h"

enum stateenum {
   idle = 0,
   running = 1,
   warn = 2,
   error = 3
};

struct monitoritem {
   rescode type; /* R_DIRECTOR, R_CLIENT or R_STORAGE */
   void* resource; /* DIRRES*, CLIENT* or STORE* */
   BSOCK *D_sock;
   stateenum state;
   stateenum oldstate;
   GtkWidget* image;
   GtkWidget* label;
};
