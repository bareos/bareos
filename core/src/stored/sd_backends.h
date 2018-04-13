/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2017 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, June 2014
 */
/**
 * @file
 * Dynamic loading of SD backend plugins.
 */

#ifndef __SD_BACKENDS_H_
#define __SD_BACKENDS_H_ 1

extern "C" {
typedef Device *(*t_backend_instantiate)(JobControlRecord *jcr, int device_type);
typedef void (*t_flush_backend)(void);
}

/**
 * Loaded shared library with a certain backend interface type.
 */
struct backend_shared_library_t {
   int interface_type_id;
   void *handle;
   /*
    * Entry points into loaded shared library.
    */
   t_backend_instantiate backend_instantiate;
   t_flush_backend flush_backend;
};

#if defined(HAVE_WIN32)
#define DYN_LIB_EXTENSION ".dll"
#elif defined(HAVE_DARWIN_OS)
#define DYN_LIB_EXTENSION ".dylib"
#else
#define DYN_LIB_EXTENSION ".so"
#endif

/**
 * Known backend to interface mappings.
 */
static struct backend_interface_mapping_t {
   int interface_type_id;
   const char *interface_name;
} backend_interface_mappings[] = {
   { B_FIFO_DEV, "fifo" },
   { B_TAPE_DEV, "tape" },
   { B_GFAPI_DEV, "gfapi" },
   { B_DROPLET_DEV, "droplet" },
   { B_RADOS_DEV, "rados" },
   { B_CEPHFS_DEV, "cephfs" },
   { B_ELASTO_DEV, "elasto" },
   { 0, NULL }
};
#endif /* __SD_DYNAMIC_H_ */
