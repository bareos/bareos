/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2024 Bareos GmbH & Co. KG

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

#include "dird/fd_sendfileset.h"
#include "filed/filed_utils.h"
#include "filed/filed.h"
#include "filed/filed_jcr_impl.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/fileset.h"
#include "lib/parse_conf.h"
#include "tools/dummysockets.h"
#include "tools/testfind_fd.h"

#include <iostream>

using namespace filedaemon;

void ProcessFileset(directordaemon::FilesetResource* director_fileset,
                    const char* t_configfile)
{
  my_config = InitFdConfig(t_configfile, M_CONFIG_ERROR);
  my_config->ParseConfigOrExit();

  me = static_cast<ClientResource*>(my_config->GetNextRes(R_CLIENT, nullptr));

  if (!CheckResources()) {
    std::cout << "Problem checking resources!" << std::endl;
    return;
  }

  struct jcr_deleter {
    void operator()(JobControlRecord* jcr) const
    {
      CleanupFileset(jcr);
      FreeJcr(jcr);
    }
  };

  std::unique_ptr<JobControlRecord, jcr_deleter> jcr(
      create_new_director_session(new EmptySocket), jcr_deleter{});
  jcr->store_bsock = new EmptySocket;
  auto filed_sock = std::make_unique<DummyFdFilesetSocket>();
  filed_sock->jcr = jcr.get();
  jcr->file_bsock = filed_sock.get();

  jcr->JobId = 1;  // helps send messages to to the log directory, instead of
                   // the director through the socket

  crypto_cipher_t cipher = CRYPTO_CIPHER_NONE;
  GetWantedCryptoCipher(jcr.get(), &cipher);

  InitFileset(jcr.get());
  directordaemon::SendIncludeExcludeItems(jcr.get(), director_fileset);
  TermFileset(jcr.get());

  BlastDataToStorageDaemon(jcr.get(), cipher);

  std::cout << "\nNumber of files examined: "
            << jcr->fd_impl->num_files_examined << "\n\n";
}
