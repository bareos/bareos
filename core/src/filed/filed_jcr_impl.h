/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_FILED_JCR_IMPL_H_
#define BAREOS_FILED_FILED_JCR_IMPL_H_

#include "include/bareos.h"
#include "lib/crypto.h"
#include "lib/thread_pool.h"

#include <atomic>

struct FindFilesPacket;
class AclData;
class XattrData;
class RunScript;

namespace filedaemon {
class BareosAccurateFilelist;
class DirectorResource;
struct save_pkt;
}  // namespace filedaemon

/* clang-format off */
struct CryptoContext {
  bool pki_sign{};                /**< Enable PKI Signatures? */
  bool pki_encrypt{};             /**< Enable PKI Encryption? */
  DIGEST* digest{};               /**< Last file's digest context */
  X509_KEYPAIR* pki_keypair{};    /**< Encryption key pair */
  alist<X509_KEYPAIR*>* pki_signers{};           /**< Trusted Signers */
  alist<X509_KEYPAIR*>* pki_recipients{};        /**< Trusted Recipients */
  CRYPTO_SESSION* pki_session{};  /**< PKE Public Keys + Symmetric Session Keys */
  POOLMEM* crypto_buf{};          /**< Encryption/Decryption buffer */
  POOLMEM* pki_session_encoded{}; /**< Cached DER-encoded copy of pki_session */
  int32_t pki_session_encoded_size{}; /**< Size of DER-encoded pki_session */
};

struct FiledJcrImpl {
  uint32_t num_files_examined{};  /**< Files examined this job */
  POOLMEM* last_fname{};          /**< Last file saved/verified */
  POOLMEM* job_metadata{};        /**< VSS job metadata */
  std::unique_ptr<AclData> acl_data{};         /**< ACLs for backup/restore */
  std::unique_ptr<XattrData> xattr_data{};     /**< Extended Attributes for backup/restore */
  int32_t last_type{};            /**< Type of last file saved/verified */
  bool incremental{};             /**< Set if incremental for SINCE */
  utime_t since_time{};           /**< Begin time for SINCE */
  int listing{};                  /**< Job listing in estimate */
  int32_t Ticket{};               /**< Ticket */
  char* big_buf{};                /**< I/O buffer */
  int32_t replace{};              /**< Replace options */
  FindFilesPacket* ff{};          /**< Find Files packet */
  char PrevJob[MAX_NAME_LENGTH]{};/**< Previous job name assiciated with since time */
  uint32_t ExpectedFiles{};       /**< Expected restore files */
  uint32_t StartFile{};
  uint32_t EndFile{};
  uint32_t StartBlock{};
  uint32_t EndBlock{};
  pthread_t heartbeat_id{};       /**< Id of heartbeat thread */
  alist<RunScript*>* RunScripts{};            /**< Commands to run before and after job */
  CryptoContext crypto;           /**< Crypto ctx */
  filedaemon::DirectorResource* director{}; /**< Director resource */
  bool enable_vss{};              /**< VSS used by FD */
  bool got_metadata{};            /**< Set when found job_metadata */
  bool multi_restore{};           /**< Dir can do multiple storage restore */
  filedaemon::BareosAccurateFilelist* file_list{}; /**< Previous file list (accurate mode) */
  uint64_t base_size{};           /**< Compute space saved with base job */
  filedaemon::save_pkt* plugin_sp{}; /**< Plugin save packet */
#ifdef HAVE_WIN32
  VSSClient* pVSSClient{};        /**< VSS Client Instance */
#endif
  thread_pool threads;
};
/* clang-format on */

#endif  // BAREOS_FILED_FILED_JCR_IMPL_H_
