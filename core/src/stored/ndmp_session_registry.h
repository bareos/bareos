/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_NDMP_SESSION_REGISTRY_H_
#define BAREOS_STORED_NDMP_SESSION_REGISTRY_H_

class JobControlRecord;

namespace storagedaemon {

/* Make the job reachable for the NDMP data mover that is expected to connect
 * with this token. Only jobs using the NDMP_BAREOS protocol are registered,
 * so an NDMP session can never bind to a job of another protocol. */
void RegisterNdmpSessionToken(const char* token, JobControlRecord* jcr);

// Withdraw a token, so it can no longer be used to bind to the job.
void UnregisterNdmpSessionToken(const char* token);

/* Return the job registered for token with its use count incremented, or
 * nullptr if the token is unknown or the job has already ended. The caller
 * has to FreeJcr() the result. */
JobControlRecord* AcquireJcrByNdmpSessionToken(const char* token);

}  // namespace storagedaemon

#endif  // BAREOS_STORED_NDMP_SESSION_REGISTRY_H_
