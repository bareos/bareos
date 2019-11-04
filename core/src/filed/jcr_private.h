/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_FILED_JCR_H_
#define BAREOS_SRC_FILED_JCR_H_

#include "include/bareos.h"

struct acl_data_t;
struct xattr_data_t;

/* clang-format off */
struct CryptoContext {
  bool pki_sign = false;               /**< Enable PKI Signatures? */
  bool pki_encrypt = false;            /**< Enable PKI Encryption? */
  DIGEST* digest = nullptr;            /**< Last file's digest context */
  X509_KEYPAIR* pki_keypair = nullptr; /**< Encryption key pair */
  alist* pki_signers = nullptr;        /**< Trusted Signers */
  alist* pki_recipients = nullptr;     /**< Trusted Recipients */
  CRYPTO_SESSION* pki_session = nullptr; /**< PKE Public Keys + Symmetric Session Keys */
  POOLMEM* crypto_buf = nullptr;       /**< Encryption/Decryption buffer */
  POOLMEM* pki_session_encoded = nullptr; /**< Cached DER-encoded copy of pki_session */
  int32_t pki_session_encoded_size = 0; /**< Size of DER-encoded pki_session */
};
/* clang-format on */

struct JobControlRecordPrivate {
  uint32_t num_files_examined = 0; /**< Files examined this job */
  POOLMEM* last_fname = nullptr;   /**< Last file saved/verified */
  POOLMEM* job_metadata = nullptr; /**< VSS job metadata */
  acl_data_t* acl_data = nullptr;  /**< ACLs for backup/restore */
  xattr_data_t* xattr_data =
      nullptr;                   /**< Extended Attributes for backup/restore */
  int32_t last_type = 0;         /**< Type of last file saved/verified */
  bool incremental = false;      /**< Set if incremental for SINCE */
  utime_t mtime = 0;             /**< Begin time for SINCE */
  int listing = 0;               /**< Job listing in estimate */
  int32_t Ticket = 0;            /**< Ticket */
  char* big_buf = nullptr;       /**< I/O buffer */
  int32_t replace = 0;           /**< Replace options */
  FindFilesPacket* ff = nullptr; /**< Find Files packet */
  char PrevJob[MAX_NAME_LENGTH]{
      0}; /**< Previous job name assiciated with since time */
  uint32_t ExpectedFiles = 0; /**< Expected restore files */
  uint32_t StartFile = 0;
  uint32_t EndFile = 0;
  uint32_t StartBlock = 0;
  uint32_t EndBlock = 0;
  pthread_t heartbeat_id = 0;                 /**< Id of heartbeat thread */
  volatile bool hb_started = false;           /**< Heartbeat running */
  std::shared_ptr<BareosSocket> hb_bsock;     /**< Duped SD socket */
  std::shared_ptr<BareosSocket> hb_dir_bsock; /**< Duped DIR socket */
  alist* RunScripts = nullptr; /**< Commands to run before and after job */
  CryptoContext crypto;        /**< Crypto ctx */
  filedaemon::DirectorResource* director = nullptr; /**< Director resource */
  bool enable_vss = false;                          /**< VSS used by FD */
  bool got_metadata = false;  /**< Set when found job_metadata */
  bool multi_restore = false; /**< Dir can do multiple storage restore */
  filedaemon::BareosAccurateFilelist* file_list =
      nullptr;            /**< Previous file list (accurate mode) */
  uint64_t base_size = 0; /**< Compute space saved with base job */
  filedaemon::save_pkt* plugin_sp = nullptr; /**< Plugin save packet */
#ifdef HAVE_WIN32
  VSSClient* pVSSClient = nullptr; /**< VSS Client Instance */
#endif
};

#endif  // BAREOS_SRC_FILED_JCR_H_
