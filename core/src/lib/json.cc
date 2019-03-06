/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

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
 * JSON Glue layer.
 *
 * This file is the glue between JANSSON and BAREOS.
 *
 * Joerg Steffens, April 2015
 */

#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "lib/output_formatter.h"

#if HAVE_JANSSON
static pthread_once_t json_setup = PTHREAD_ONCE_INIT;

static void* json_malloc(size_t size) { return bmalloc(size); }

static void json_free(void* ptr) { bfree(ptr); }

static void set_alloc_funcs() { json_set_alloc_funcs(json_malloc, json_free); }

void InitializeJson() { pthread_once(&json_setup, set_alloc_funcs); }
#else
void InitializeJson() {}
#endif /* HAVE_JANSSON */
