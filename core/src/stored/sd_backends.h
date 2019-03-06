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

#ifndef BAREOS_STORED_SD_BACKENDS_H_
#define BAREOS_STORED_SD_BACKENDS_H_ 1

class alist;

namespace storagedaemon {

extern "C" {
typedef Device* (*t_backend_instantiate)(JobControlRecord* jcr,
                                         int device_type);
typedef void (*t_flush_backend)(void);
}

/**
 * Loaded shared library with a certain backend interface type.
 */
struct backend_shared_library_t {
  int interface_type_id;
  void* handle;
  /*
   * Entry points into loaded shared library.
   */
  t_backend_instantiate backend_instantiate;
  t_flush_backend flush_backend;
};

#if defined(HAVE_WIN32)
#define DYN_LIB_EXTENSION ".dll"
#elif defined(HAVE_DARWIN_OS)
/* cmake MODULE creates a .so files; cmake SHARED creates .dylib */
// #define DYN_LIB_EXTENSION ".dylib"
#define DYN_LIB_EXTENSION ".so"
#else
#define DYN_LIB_EXTENSION ".so"
#endif


#if defined(HAVE_DYNAMIC_SD_BACKENDS)
void SdSetBackendDirs(alist* new_backend_dirs);
Device* init_backend_dev(JobControlRecord* jcr, int device_type);
void DevFlushBackends();
#endif

} /* namespace storagedaemon */

#endif /* __SD_DYNAMIC_H_ */
