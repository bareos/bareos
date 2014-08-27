/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
 * Dynamic loading of SD backend plugins.
 *
 * Marco van Wieringen, June 2014
 */

#include "bareos.h"
#include "stored.h"

#if defined(HAVE_DYNAMIC_SD_BACKENDS)

#include "sd_backends.h"
#include <dlfcn.h>

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

/*
 * All loaded backends.
 */
static alist *loaded_backends = NULL;
static alist *backend_dirs = NULL;

void sd_set_backend_dirs(alist *new_backend_dirs)
{
   backend_dirs = new_backend_dirs;
}

static inline backend_interface_mapping_t *lookup_backend_interface_mapping(int device_type)
{
   backend_interface_mapping_t *backend_interface_mapping;

   for (backend_interface_mapping = backend_interface_mappings;
        backend_interface_mapping->interface_name != NULL;
        backend_interface_mapping++) {

      /*
       * See if this is a match.
       */
      if (backend_interface_mapping->interface_type_id == device_type) {
         return backend_interface_mapping;
      }
   }

   return NULL;
}

DEVICE *init_backend_dev(JCR *jcr, int device_type)
{
   struct stat st;
   char *backend_dir;
   void *dl_handle = NULL;
   POOL_MEM shared_library_name(PM_FNAME);
   backend_interface_mapping_t *backend_interface_mapping;
   backend_shared_library_t *backend_shared_library;
   t_backend_instantiate backend_instantiate;
   t_flush_backend flush_backend;

   /*
    * For dynamic loading storage backends there must be a list of backend dirs set.
    */
   if (!backend_dirs) {
      Jmsg(jcr, M_ABORT, 0, _("Catalog Backends Dir not configured.\n"));
   }

   backend_interface_mapping = lookup_backend_interface_mapping(device_type);
   if (backend_interface_mapping == NULL) {
      return (DEVICE *)NULL;
   }

   /*
    * See if the backend is already loaded.
    */
   if (loaded_backends) {
      foreach_alist(backend_shared_library, loaded_backends) {
         if (backend_shared_library->interface_type_id ==
             backend_interface_mapping->interface_type_id) {
            return backend_shared_library->backend_instantiate(jcr, device_type);
         }
      }
   }

   /*
    * This is a new backend try to use dynamic loading to load the backend library.
    */
   foreach_alist(backend_dir, backend_dirs) {
      Mmsg(shared_library_name, "%s/libbareossd-%s%s", backend_dir,
           backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);
      Dmsg3(100, "init_backend_dev: testing backend %s/libbareossd-%s%s\n",
            backend_dir, backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);

      /*
       * Make sure the shared library with this name exists.
       */
      if (stat(shared_library_name.c_str(), &st) == 0) {
         dl_handle = dlopen(shared_library_name.c_str(), RTLD_NOW);
         if (!dl_handle) {
            Jmsg(jcr, M_ERROR, 0, _("Unable to load shared library: %s ERR=%s\n"),
                 shared_library_name.c_str(), NPRT(dlerror()));
            continue;
         }

         /*
          * Lookup the backend_instantiate function.
          */
         backend_instantiate = (t_backend_instantiate)dlsym(dl_handle, "backend_instantiate");
         if (backend_instantiate == NULL) {
            Jmsg(jcr, M_ERROR, 0, _("Lookup of backend_instantiate in shared library %s failed: ERR=%s\n"),
                 shared_library_name.c_str(), NPRT(dlerror()));
            dlclose(dl_handle);
            dl_handle = NULL;
            continue;
         }

         /*
          * Lookup the flush_backend function.
          */
         flush_backend = (t_flush_backend)dlsym(dl_handle, "flush_backend");
         if (flush_backend == NULL) {
            Jmsg(jcr, M_ERROR, 0, _("Lookup of flush_backend in shared library %s failed: ERR=%s\n"),
                 shared_library_name.c_str(), NPRT(dlerror()));
            dlclose(dl_handle);
            dl_handle = NULL;
            continue;
         }

         /*
          * We found the shared library and it has the right entry points.
          */
         break;
      }
   }

   if (dl_handle) {
      /*
       * Create a new loaded shared library entry and tack it onto the list of loaded backend shared libs.
       */
      backend_shared_library = (backend_shared_library_t *)malloc(sizeof(backend_shared_library_t));
      backend_shared_library->interface_type_id = backend_interface_mapping->interface_type_id;
      backend_shared_library->handle = dl_handle;
      backend_shared_library->backend_instantiate = backend_instantiate;
      backend_shared_library->flush_backend = flush_backend;

      if (loaded_backends == NULL) {
         loaded_backends = New(alist(10, not_owned_by_alist));
      }
      loaded_backends->append(backend_shared_library);

      return backend_shared_library->backend_instantiate(jcr, device_type);
   } else {
      Jmsg(jcr, M_ABORT, 0, _("Unable to load any shared library for .\n"));
      return (DEVICE *)NULL;
   }
}

void dev_flush_backends()
{
   backend_shared_library_t *backend_shared_library;

   if (loaded_backends) {
      foreach_alist(backend_shared_library, loaded_backends) {
         /*
          * Call the flush entry point in the lib.
          */
         backend_shared_library->flush_backend();

         /*
          * Close the shared library and unload it.
          */
         dlclose(backend_shared_library->handle);
         free(backend_shared_library);
      }

      delete loaded_backends;
      loaded_backends = NULL;
   }
}
#endif /* HAVE_DYNAMIC_SD_BACKENDS */
