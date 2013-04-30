/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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
 *  Written by James Harper, October 2008
 */

#include "exchange-fd.h"

root_node_t::root_node_t(char *name) : node_t(name, NODE_TYPE_ROOT)
{
   service_node = NULL;
}

root_node_t::~root_node_t()
{
}

bRC
root_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   bRC retval = bRC_OK;
   time_t now;

   _DebugMessage(100, "startBackupNode_ROOT state = %d\n", state);
   switch(state)
   {
   case 0:
      if (strcmp(PLUGIN_PATH_PREFIX_BASE, name) != 0)
      {
         _JobMessage(M_FATAL, "Invalid backup path specified, must start with '/" PLUGIN_PATH_PREFIX_BASE "/'\n");
         state = 999;
         return bRC_Error;
      }
      // check that service_node == NULL
      service_node = new service_node_t(bstrdup(context->path_bits[level + 1]), this);
      state = 1;
      // fall through
   case 1:
      context->current_node = service_node;
      break;
   case 2:
      now = time(NULL);
      sp->fname = full_path;
      sp->link = full_path;
      sp->statp.st_mode = 0700 | S_IFDIR;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = 0;
      sp->type = FT_DIREND;
      break;
   case 999:
      return bRC_Error;
   default:
      _JobMessage(M_FATAL, "startBackupFile: invalid internal state %d", state);
      state = 999;
   }
   return retval;
}

bRC
root_node_t::endBackupFile(exchange_fd_context_t *context)
{
   bRC retval = bRC_OK;

   _DebugMessage(100, "endBackupNode_ROOT state = %d\n", state);
   switch(state)
   {
   case 1:
      state = 2;
      retval = bRC_More;
      // free service_node here?
      break;
   case 2:
      retval = bRC_OK;
      break;
   case 999:
      retval = bRC_Error;
   default:
      _JobMessage(M_FATAL, "endBackupFile: invalid internal state %d", state);
      state = 999;
      return bRC_Error;
   }
   return retval;
}

bRC
root_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   _DebugMessage(100, "createFile_ROOT state = %d\n", state);
   switch (state) {
   case 0:
      if (strcmp(name, PLUGIN_PATH_PREFIX_BASE) != 0) {
         _JobMessage(M_FATAL, "Invalid restore path specified, must start with '/" PLUGIN_PATH_PREFIX_BASE "/'\n");
         state = 999;
         return bRC_Error;
      }
      service_node = new service_node_t(bstrdup(context->path_bits[level + 1]), this);
      context->current_node = service_node;
      return bRC_OK;
   case 1:
      rp->create_status = CF_CREATED;
      return bRC_OK;

   /* Skip this file */
   case 900:
      rp->create_status = CF_SKIP;
      return bRC_OK;
   /* Error */
   case 999:
      return bRC_Error;
   default:
      _JobMessage(M_FATAL, "createFile: invalid internal state %d", state);
      state = 999;
   }
   return bRC_Error;
}

bRC
root_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endRestoreFile_ROOT state = %d\n", state);
   switch (state) {
   case 0:
      delete service_node;
      state = 1;
      return bRC_OK;
   case 1:
      return bRC_OK;
   case 900:
      return bRC_OK;
   default:
      _JobMessage(M_FATAL, "endRestore: invalid internal state %d", state);
      state = 999;
   }
   return bRC_Error;
}
