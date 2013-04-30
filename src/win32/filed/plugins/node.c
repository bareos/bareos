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

node_t::node_t(char *name, int type)
{
   this->type = type;
   state = 0;
   parent = NULL;
   this->name = bstrdup(name);
   full_path = make_full_path();
   size = 0;
   level = 0;
}

node_t::node_t(char *name, int type, node_t *parent_node)
{
   this->type = type;
   state = 0;
   parent = parent_node;
   this->name = bstrdup(name);
   full_path = make_full_path();
   size = 0;
   level = parent->level + 1;
}

node_t::~node_t()
{
   delete name;
   delete full_path;
}

char *
node_t::make_full_path()
{
   node_t *curr_node;
   int len;
   char *retval;

   for (len = 0, curr_node = this; curr_node != NULL; curr_node = curr_node->parent)
   {
      len += strlen(curr_node->name) + 1;
   }
   if (type == NODE_TYPE_FILE || type == NODE_TYPE_DATABASE_INFO)
   {
      retval = new char[len + 1];
      retval[len] = 0;
   }
   else
   {
      retval = new char[len + 2];
      retval[len] = '/';
      retval[len + 1] = 0;
   }
   for (curr_node = this; curr_node != NULL; curr_node = curr_node->parent)
   {
      len -= strlen(curr_node->name);
      memcpy(retval + len, curr_node->name, strlen(curr_node->name));
      retval[--len] = '/';
   }
   return retval;
}

bRC
node_t::pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io)
{
   _DebugMessage(100, "pluginIoOpen_Node\n");
   io->status = 0;
   io->io_errno = 0;
   return bRC_OK;
}

bRC
node_t::pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io)
{
   _DebugMessage(100, "pluginIoRead_Node\n");
   io->status = 0;
   io->io_errno = 0;
   return bRC_OK;
}

bRC
node_t::pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io)
{
   _DebugMessage(100, "pluginIoWrite_Node\n");
   io->status = 0;
   io->io_errno = 1;
   return bRC_Error;
}

bRC
node_t::pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io)
{
   _DebugMessage(100, "pluginIoClose_Node\n");
   io->status = 0;
   io->io_errno = 0;
   return bRC_OK;
}
