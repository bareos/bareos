/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Dynamic loading of catalog plugins.
 *
 * Marco van Wieringen, November 2010
 */

#include "bareos.h"

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)

#include "cats.h"
#include "cats_dynamic.h"
#include <dlfcn.h>

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

/*
 * All loaded backends.
 */
static alist *loaded_backends = NULL;

static inline backend_interface_mapping_t *lookup_backend_interface_mapping(const char *interface_name)
{
   backend_interface_mapping_t *backend_interface_mapping;

   for (backend_interface_mapping = backend_interface_mappings;
        backend_interface_mapping->interface_name != NULL;
        backend_interface_mapping++) {

      Dmsg3(100, "db_init_database: Trying to find mapping of given interfacename %s to mapping interfacename %s, partly_compare = %s\n",
            interface_name, backend_interface_mapping->interface_name, (backend_interface_mapping->partly_compare) ? "true" : "false");

      /*
       * See if this is a match.
       */
      if (backend_interface_mapping->partly_compare) {
         if (bstrncasecmp(interface_name, backend_interface_mapping->interface_name,
                              strlen(backend_interface_mapping->interface_name))) {
            return backend_interface_mapping;
         }
      } else {
         if (bstrcasecmp(interface_name, backend_interface_mapping->interface_name)) {
            return backend_interface_mapping;
         }
      }
   }

   return NULL;
}

B_DB *db_init_database(JCR *jcr,
                       const char *db_driver,
                       const char *db_name,
                       const char *db_user,
                       const char *db_password,
                       const char *db_address,
                       int db_port,
                       const char *db_socket,
                       bool mult_db_connections,
                       bool disable_batch_insert,
                       bool need_private)
{
   void *dl_handle;
   char shared_library_name[1024];
   backend_interface_mapping_t *backend_interface_mapping;
   backend_shared_library_t *backend_shared_library;
   t_backend_instantiate backend_instantiate;
   t_flush_backend flush_backend;

   /*
    * A db_driver is mandatory for dynamic loading of backends to work.
    */
   if (!db_driver) {
      Jmsg(jcr, M_ABORT, 0, _("Driver type not specified in Catalog resource.\n"));
   }

   /*
    * If we didn't find a mapping its fatal because we don't know what database backend to use.
    */
   backend_interface_mapping = lookup_backend_interface_mapping(db_driver);
   if (backend_interface_mapping == NULL) {
      Jmsg(jcr, M_ABORT, 0, _("Unknown database type: %s\n"), db_driver);
      return (B_DB *)NULL;
   }

   /*
    * See if the backend is already loaded.
    */
   if (loaded_backends) {
      foreach_alist(backend_shared_library, loaded_backends) {
         if (backend_shared_library->interface_type_id ==
             backend_interface_mapping->interface_type_id) {
            return backend_shared_library->backend_instantiate(jcr,
                                                               db_driver,
                                                               db_name,
                                                               db_user,
                                                               db_password,
                                                               db_address,
                                                               db_port,
                                                               db_socket,
                                                               mult_db_connections,
                                                               disable_batch_insert,
                                                               need_private);
         }
      }
   }

   /*
    * This is a new backend try to use dynamic loading to load the backend library.
    */
   bsnprintf(shared_library_name, sizeof(shared_library_name), "%s/libbareoscats-%s%s",
             LIBDIR, backend_interface_mapping->interface_name, DYN_LIB_EXTENSION);

   dl_handle = dlopen(shared_library_name, RTLD_NOW);
   if (!dl_handle) {
      Jmsg(jcr, M_ABORT, 0, _("Unable to load shared library: %s ERR=%s\n"),
           shared_library_name, NPRT(dlerror()));
      return (B_DB *)NULL;
   }

   /*
    * Lookup the backend_instantiate function.
    */
   backend_instantiate = (t_backend_instantiate)dlsym(dl_handle, "backend_instantiate");
   if (backend_instantiate == NULL) {
      Jmsg(jcr, M_ABORT, 0, _("Lookup of backend_instantiate in shared library %s failed: ERR=%s\n"),
           shared_library_name, NPRT(dlerror()));
      dlclose(dl_handle);
      return (B_DB *)NULL;
   }

   /*
    * Lookup the flush_backend function.
    */
   flush_backend = (t_flush_backend)dlsym(dl_handle, "flush_backend");
   if (flush_backend == NULL) {
      Jmsg(jcr, M_ABORT, 0, _("Lookup of flush_backend in shared library %s failed: ERR=%s\n"),
           shared_library_name, NPRT(dlerror()));
      dlclose(dl_handle);
      return (B_DB *)NULL;
   }

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

   return backend_shared_library->backend_instantiate(jcr,
                                                      db_driver,
                                                      db_name,
                                                      db_user,
                                                      db_password,
                                                      db_address,
                                                      db_port,
                                                      db_socket,
                                                      mult_db_connections,
                                                      disable_batch_insert,
                                                      need_private);
}

void db_flush_backends(void)
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

#endif /* HAVE_DYNAMIC_CATS_BACKENDS */
