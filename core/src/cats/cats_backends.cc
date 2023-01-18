/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
// Marco van Wieringen, November 2010
/**
 * @file
 * Dynamic loading of catalog plugins.
 */

#include "include/bareos.h"
#include "lib/edit.h"

#if HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_DBI

#  include "cats.h"

#  if defined(HAVE_DYNAMIC_CATS_BACKENDS)

static struct backend_interface_mapping_t {
  const char* interface_name;
  bool partly_compare;
  int interface_type_id;
} backend_interface_mappings[]
    = {{"dbi", TRUE, SQL_INTERFACE_TYPE_DBI},
       {"mysql", FALSE, SQL_INTERFACE_TYPE_MYSQL},
       {"postgresql", FALSE, SQL_INTERFACE_TYPE_POSTGRESQL},
       {NULL, FALSE, 0}};

#    include "cats_backends.h"
#    include <dlfcn.h>

#    ifndef RTLD_NOW
#      define RTLD_NOW 2
#    endif

static alist<backend_shared_library_t*>* loaded_backends = NULL;
static std::vector<std::string> backend_dirs;

void DbSetBackendDirs(std::vector<std::string>& new_backend_dirs)
{
  backend_dirs = new_backend_dirs;
}

static inline backend_interface_mapping_t* lookup_backend_interface_mapping(
    const char* interface_name)
{
  backend_interface_mapping_t* backend_interface_mapping;

  for (backend_interface_mapping = backend_interface_mappings;
       backend_interface_mapping->interface_name != NULL;
       backend_interface_mapping++) {
    Dmsg3(100,
          "db_init_database: Trying to find mapping of given interfacename %s "
          "to mapping interfacename %s, partly_compare = %s\n",
          interface_name, backend_interface_mapping->interface_name,
          (backend_interface_mapping->partly_compare) ? "true" : "false");

    // See if this is a match.
    if (backend_interface_mapping->partly_compare) {
      if (bstrncasecmp(interface_name,
                       backend_interface_mapping->interface_name,
                       strlen(backend_interface_mapping->interface_name))) {
        return backend_interface_mapping;
      }
    } else {
      if (Bstrcasecmp(interface_name,
                      backend_interface_mapping->interface_name)) {
        return backend_interface_mapping;
      }
    }
  }

  return NULL;
}

static backend_shared_library_t* load_backend(JobControlRecord* jcr,
                                              const char* library_name,
                                              int type_id)
{
#    ifndef HAVE_WIN32
  struct stat st;
  if (stat(library_name, &st) == -1) { return nullptr; }
#    endif
  void* dl_handle = dlopen(library_name, RTLD_NOW);
  if (!dl_handle) {
    std::string error{dlerror()};
    Jmsg(jcr, M_ERROR, 0, _("Unable to load shared library: %s ERR=%s\n"),
         library_name, error.c_str());
    Dmsg2(100, _("Unable to load shared library: %s ERR=%s\n"), library_name,
          error.c_str());
    return nullptr;
  }

  // Lookup the backend_instantiate function.
  t_backend_instantiate backend_instantiate
      = (t_backend_instantiate)dlsym(dl_handle, "backend_instantiate");
  if (backend_instantiate == NULL) {
    std::string error{dlerror()};
    Jmsg(jcr, M_ERROR, 0,
         _("Lookup of backend_instantiate in shared library %s failed: "
           "ERR=%s\n"),
         library_name, error.c_str());
    Dmsg2(100,
          _("Lookup of backend_instantiate in shared library %s failed: "
            "ERR=%s\n"),
          library_name, error.c_str());
    dlclose(dl_handle);
    return nullptr;
  }

  // Lookup the flush_backend function.
  t_flush_backend flush_backend
      = (t_flush_backend)dlsym(dl_handle, "flush_backend");
  if (flush_backend == NULL) {
    std::string error{dlerror()};
    Jmsg(jcr, M_ERROR, 0,
         _("Lookup of flush_backend in shared library %s failed: ERR=%s\n"),
         library_name, error.c_str());
    Dmsg2(100,
          _("Lookup of flush_backend in shared library %s failed: ERR=%s\n"),
          library_name, error.c_str());
    dlclose(dl_handle);
    return nullptr;
  }
  backend_shared_library_t* backend_shared_library
      = (backend_shared_library_t*)malloc(sizeof(backend_shared_library_t));
  backend_shared_library->interface_type_id = type_id;
  backend_shared_library->handle = dl_handle;
  backend_shared_library->backend_instantiate = backend_instantiate;
  backend_shared_library->flush_backend = flush_backend;
  return backend_shared_library;
}

BareosDb* db_init_database(JobControlRecord* jcr,
                           const char* db_driver,
                           const char* db_name,
                           const char* db_user,
                           const char* db_password,
                           const char* db_address,
                           int db_port,
                           const char* db_socket,
                           bool mult_db_connections,
                           bool disable_batch_insert,
                           bool try_reconnect,
                           bool exit_on_fatal,
                           bool need_private)
{
  PoolMem shared_library_name(PM_FNAME);
  PoolMem error(PM_FNAME);
  backend_interface_mapping_t* backend_interface_mapping;
  backend_shared_library_t* backend_shared_library = nullptr;

  if (!db_driver) {
    Jmsg(jcr, M_ERROR_TERM, 0,
         _("Driver type not specified in Catalog resource.\n"));
  }

  /*
   * If we didn't find a mapping its fatal because we don't know what database
   * backend to use.
   */
  backend_interface_mapping = lookup_backend_interface_mapping(db_driver);
  if (backend_interface_mapping == NULL) {
    Jmsg(jcr, M_ERROR_TERM, 0, _("Unknown database driver: %s\n"), db_driver);
    return (BareosDb*)NULL;
  }

  // See if the backend is already loaded.
  if (loaded_backends) {
    foreach_alist (backend_shared_library, loaded_backends) {
      if (backend_shared_library->interface_type_id
          == backend_interface_mapping->interface_type_id) {
        return backend_shared_library->backend_instantiate(
            jcr, db_driver, db_name, db_user, db_password, db_address, db_port,
            db_socket, mult_db_connections, disable_batch_insert, try_reconnect,
            exit_on_fatal, need_private);
      }
    }
  }

  /*
   * This is a new backend try to use dynamic loading to load the backend
   * library.
   */

#    if defined HAVE_WIN32
  Mmsg(shared_library_name, "libbareoscats-%s%s",
       backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);
  backend_shared_library
      = load_backend(jcr, shared_library_name.c_str(),
                     backend_interface_mapping->interface_type_id);
#    else
  if (backend_dirs.empty()) {
    Jmsg(jcr, M_ERROR_TERM, 0, _("Catalog Backends Dir not configured.\n"));
  }

  for (const auto& backend_dir : backend_dirs) {
    Mmsg(shared_library_name, "%s/libbareoscats-%s%s", backend_dir.c_str(),
         backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);
    Dmsg3(100, "db_init_database: checking backend %s/libbareoscats-%s%s\n",
          backend_dir.c_str(), backend_interface_mapping->interface_name,
          DYN_LIB_EXTENSION);

    if (nullptr
        != (backend_shared_library
            = load_backend(jcr, shared_library_name.c_str(),
                           backend_interface_mapping->interface_type_id))) {
      // We found the shared library and it has the right entry points.
      break;
    }
  }
#    endif

  if (backend_shared_library) {
    /*
     * Create a new loaded shared library entry and tack it onto the list of
     * loaded backend shared libs.
     */

    if (loaded_backends == NULL) {
      loaded_backends
          = new alist<backend_shared_library_t*>(10, not_owned_by_alist);
    }
    loaded_backends->append(backend_shared_library);

    Dmsg1(100, "db_init_database: loaded backend %s\n",
          shared_library_name.c_str());

    return backend_shared_library->backend_instantiate(
        jcr, db_driver, db_name, db_user, db_password, db_address, db_port,
        db_socket, mult_db_connections, disable_batch_insert, try_reconnect,
        exit_on_fatal, need_private);
  } else {
    Jmsg(jcr, M_ABORT, 0,
         _("Unable to load any shared library for libbareoscats-%s%s\n"),
         backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);
    return (BareosDb*)NULL;
  }
}

void DbFlushBackends(void)
{
  backend_shared_library_t* backend_shared_library = nullptr;

  if (loaded_backends) {
    foreach_alist (backend_shared_library, loaded_backends) {
      backend_shared_library->flush_backend();

      dlclose(backend_shared_library->handle);
      free(backend_shared_library);
    }

    delete loaded_backends;
    loaded_backends = NULL;
  }
}
#  else
// Dummy bareos backend function replaced with the correct one at install time.
BareosDb* db_init_database(JobControlRecord* jcr,
                           const char* db_driver,
                           const char* db_name,
                           const char* db_user,
                           const char* db_password,
                           const char* db_address,
                           int db_port,
                           const char* db_socket,
                           bool mult_db_connections,
                           bool disable_batch_insert,
                           bool try_reconnect,
                           bool exit_on_fatal,
                           bool need_private)
{
  Jmsg(jcr, M_FATAL, 0,
       _("Please replace this dummy libbareoscats library with a proper "
         "one.\n"));
  Dmsg0(0, _("Please replace this dummy libbareoscats library with a proper "
             "one.\n"));
  return NULL;
}

void DbFlushBackends(void) {}
#  endif /* HAVE_DYNAMIC_CATS_BACKENDS */
#endif   /* HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_DBI */
