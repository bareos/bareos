/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2025 Bareos GmbH & Co. KG

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

#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"


namespace directordaemon {

JobControlRecord* NewDirectorJcr(JCR_free_HANDLER* DirdFreeJcr)
{
  JobControlRecord* jcr = new_jcr(DirdFreeJcr);
  jcr->dir_impl = new DirectorJcrImpl(my_config->config_resources_container_);
  register_jcr(jcr);
  Dmsg1(10, "NewDirectorJcr: configuration_resources_ is at %p %s\n",
        my_config->config_resources_container_->configuration_resources_.data(),
        my_config->config_resources_container_->TimeStampAsString().c_str());
  return jcr;
}

} /* namespace directordaemon */
