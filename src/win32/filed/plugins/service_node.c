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

service_node_t::service_node_t(char *name, node_t *parent_node) : node_t(name, NODE_TYPE_SERVICE, parent_node)
{
   current_ibi = 0;
   hccx = NULL;
   ibi = NULL;
   ibi_count = 0;
   first_storage_group_node = NULL;
}

service_node_t::~service_node_t()
{
}

bRC
service_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   HRESULT result;
   char aname[256];

   _DebugMessage(100, "startBackupNode_SERVICE state = %d\n", state);
   switch(state)
   {
   case 0:
      if (strcmp(PLUGIN_PATH_PREFIX_SERVICE, name) != 0)
      {
         _JobMessage(M_FATAL, "Invalid restore path specified, must start with /" PLUGIN_PATH_PREFIX_BASE "/" PLUGIN_PATH_PREFIX_SERVICE "/\n");
         return bRC_Error;
      }
      // convert name to a wide string

      _DebugMessage(100, "Calling HrESEBackupPrepare\n");
      wcstombs(aname, context->computer_name, 256);
      _JobMessage(M_INFO, "Preparing Exchange Backup for %s\n", aname);
      result = HrESEBackupPrepare(context->computer_name, PLUGIN_PATH_PREFIX_SERVICE_W, &ibi_count, &ibi, &hccx);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESEBackupPrepare failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         return bRC_Error;
      }
      state = 1;
      // fall through
   case 1:
      if (context->path_bits[level + 1] == NULL)
      {
         _DebugMessage(100, "No specific storage group specified - backing them all up\n");
         char *tmp = new char[wcslen(ibi[current_ibi].wszInstanceName) + 1];
         wcstombs(tmp, ibi[current_ibi].wszInstanceName, wcslen(ibi[current_ibi].wszInstanceName) + 1);
         first_storage_group_node = new storage_group_node_t(tmp, this);
         delete tmp;
         _DebugMessage(100, "storage group name = %s\n", first_storage_group_node->name);
         first_storage_group_node->ibi = &ibi[current_ibi];
         first_storage_group_node->hccx = hccx;
         context->current_node = first_storage_group_node;
      }
      else
      {
         char *tmp = NULL;
         for (current_ibi = 0; current_ibi < ibi_count; current_ibi++)
         {
            tmp = new char[wcslen(ibi[current_ibi].wszInstanceName) + 1];
            wcstombs(tmp, ibi[current_ibi].wszInstanceName, wcslen(ibi[current_ibi].wszInstanceName) + 1);
            if (stricmp(tmp, context->path_bits[level + 1]) == 0)
               break;
         }
         first_storage_group_node = new storage_group_node_t(tmp, this);
         delete tmp;
         if (current_ibi == ibi_count)
         {
            _JobMessage(M_FATAL, "Invalid Storage Group '%s'\n", context->path_bits[level + 1]);
            return bRC_Error;
         }
         _DebugMessage(100, "storage group name = %s\n", first_storage_group_node->name);
         first_storage_group_node->ibi = &ibi[current_ibi];
         first_storage_group_node->hccx = hccx;
         context->current_node = first_storage_group_node;
      }
      break;
   case 2:
      time_t now = time(NULL);
      sp->fname = full_path;
      sp->link = full_path;
      sp->statp.st_mode = 0700 | S_IFDIR;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = 0;
      sp->statp.st_nlink = 1;
      //sp->statp.st_blocks = 0;
      sp->type = FT_DIREND;
      break;
   }
   _DebugMessage(100, "ending startBackupNode_SERVICE state = %d\n", state);
   return bRC_OK;
}

bRC
service_node_t::endBackupFile(exchange_fd_context_t *context)
{
   HRESULT result;
   bRC retval = bRC_OK;

   _DebugMessage(100, "endBackupNode_SERVICE state = %d\n", state);
   switch(state)
   {
   case 0:
      // should never happen
      break;
   case 1:
      // free node->storage_group_node
      if (context->path_bits[level + 1] == NULL)
      {
         current_ibi++;
         if (current_ibi == ibi_count)
            state = 2;
      }
      else
         state = 2;
      retval = bRC_More;
      break;
   case 2:
      _DebugMessage(100, "calling HrESEBackupEnd\n");
      result = HrESEBackupEnd(hccx);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESEBackupEnd failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         return bRC_Error;
      }

      context->current_node = parent;
      retval = bRC_OK;
      break;
   }
   return retval;
}

bRC
service_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   storage_group_node_t *curr_sg, *prev_sg;

   _DebugMessage(100, "createFile_SERVICE state = %d\n", state);
   if (strcmp(name, "Microsoft Information Store") != 0)
   {
      _JobMessage(M_FATAL, "Invalid restore path specified, must start with '/" PLUGIN_PATH_PREFIX_BASE "/" PLUGIN_PATH_PREFIX_SERVICE "/'\n", state);
      return bRC_Error;
   }
   for(;;)
   {
      switch (state)
      {
      case 0:
         if (context->path_bits[level + 1] == NULL)
         {
            state = 1;
            break;
         }
         for (prev_sg = NULL, curr_sg = first_storage_group_node; curr_sg != NULL; prev_sg = curr_sg, curr_sg = curr_sg->next)
         {
            if (strcmp(curr_sg->name, context->path_bits[level + 1]) == 0)
            {
               break;
            }
         }
         if (curr_sg == NULL)
         {
            curr_sg = new storage_group_node_t(bstrdup(context->path_bits[level + 1]), this);
            if (prev_sg == NULL)
               first_storage_group_node = curr_sg;
            else
               prev_sg->next = curr_sg;
         }
         context->current_node = curr_sg;
         return bRC_OK;
      case 1:
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
   }
   return bRC_Error;
}

bRC
service_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endRestoreFile_SERVICE state = %d\n", state);
   switch(state)
   {
   case 0:
      return bRC_Error;
   case 1:
      context->current_node = parent;
      return bRC_OK;
   }

   return bRC_Error;
}
