/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#include "create_resource.h"
#include "bsock_test.h"
#include "bareos_test_sockets.h"

#include "include/bareos.h"
#include "console/console_conf.h"
#include "stored/stored_conf.h"
#include "dird/dird_conf.h"

namespace console {
console::DirectorResource* CreateAndInitializeNewDirectorResource()
{
  console::DirectorResource* dir = new (console::DirectorResource);
  dir->address = (char*)HOST;
  dir->DIRport = htons(create_unique_socket_number());
  dir->tls_enable_ = false;
  dir->tls_require_ = false;
  dir->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  dir->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  dir->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  dir->tls_cert_.verify_peer_ = false;
  dir->resource_name_ = (char*)"director";
  dir->password_.encoding = p_encoding_md5;
  dir->password_.value = (char*)"verysecretpassword";
  return dir;
}

console::ConsoleResource* CreateAndInitializeNewConsoleResource()
{
  console::ConsoleResource* cons = new (console::ConsoleResource);
  cons->tls_enable_ = false;
  cons->tls_require_ = false;
  cons->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  cons->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  cons->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  cons->tls_cert_.verify_peer_ = false;
  cons->resource_name_ = (char*)"clientname";
  cons->password.encoding = p_encoding_md5;
  cons->password.value = (char*)"verysecretpassword";
  return cons;
}
} /* namespace console */

namespace directordaemon {
directordaemon::ConsoleResource* CreateAndInitializeNewConsoleResource()
{
  directordaemon::ConsoleResource* cons = new (directordaemon::ConsoleResource);
  cons->tls_enable_ = false;
  cons->tls_require_ = false;
  cons->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/console.bareos.org-cert.pem");
  cons->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/console.bareos.org-key.pem");
  cons->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  cons->tls_cert_.verify_peer_ = false;
  cons->resource_name_ = (char*)"clientname";
  cons->password_.encoding = p_encoding_md5;
  cons->password_.value = (char*)"verysecretpassword";
  return cons;
}

directordaemon::StorageResource* CreateAndInitializeNewStorageResource()
{
  directordaemon::StorageResource* store =
      new (directordaemon::StorageResource);
  store->address = (char*)HOST;
  store->SDport = htons(create_unique_socket_number());
  store->tls_enable_ = false;
  store->tls_require_ = false;
  store->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  store->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  store->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  store->tls_cert_.verify_peer_ = false;
  store->resource_name_ = (char*)"storage";
  return store;
}

directordaemon::DirectorResource* CreateAndInitializeNewDirectorResource()
{
  directordaemon::DirectorResource* dir =
      new (directordaemon::DirectorResource);
  dir->tls_enable_ = false;
  dir->tls_require_ = false;
  dir->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  dir->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  dir->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  dir->tls_cert_.verify_peer_ = false;
  dir->DIRsrc_addr = 0;
  dir->resource_name_ = (char*)"director";
  dir->password_.encoding = p_encoding_md5;
  dir->password_.value = (char*)"verysecretpassword";
  return dir;
}
} /* namespace directordaemon */

namespace storagedaemon {
storagedaemon::DirectorResource* CreateAndInitializeNewDirectorResource()
{
  storagedaemon::DirectorResource* dir = new (storagedaemon::DirectorResource);
  dir->tls_enable_ = false;
  dir->tls_require_ = false;
  dir->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  dir->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  dir->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  dir->tls_cert_.verify_peer_ = false;
  dir->resource_name_ = (char*)"director";
  return dir;
}

storagedaemon::StorageResource* CreateAndInitializeNewStorageResource()
{
  storagedaemon::StorageResource* store = new (storagedaemon::StorageResource);
  store->tls_enable_ = false;
  store->tls_require_ = false;
  store->tls_cert_.certfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-cert.pem");
  store->tls_cert_.keyfile_ =
      new (std::string)(CERTDIR "/bareos-dir.bareos.org-key.pem");
  store->tls_cert_.ca_certfile_ = new (std::string)(CERTDIR "/bareos-ca.pem");
  store->tls_cert_.verify_peer_ = false;
  store->resource_name_ = (char*)"storage";
  return store;
}
} /* namespace storagedaemon */
