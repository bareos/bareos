/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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
#ifndef BAREOS_DIRD_COMM_H_
#define BAREOS_DIRD_COMM_H_

#include <cinttypes>

namespace directordaemon {

inline constexpr const char JobMessage[]
    = "Jmsg Job=%s type=%" PRId32 " level=%" PRId64 " %s";
inline constexpr const char Find_media[]
    = "CatReq Job=%127s FindMedia=%d pool_name=%127s media_type=%127s "
      "unwanted_volumes=%s\n";
inline constexpr const char Get_Vol_Info[]
    = "CatReq Job=%127s GetVolInfo VolName=%127s write=%d\n";
inline constexpr const char Update_media[]
    = "CatReq Job=%127s UpdateMedia VolName=%s"
      " VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%lld VolMounts=%u"
      " VolErrors=%u VolWrites=%u MaxVolBytes=%lld EndTime=%lld VolStatus=%10s"
      " Slot=%d relabel=%d InChanger=%d VolReadTime=%lld VolWriteTime=%lld"
      " VolFirstWritten=%lld\n";
inline constexpr const char Create_job_media[]
    = "CatReq Job=%127s CreateJobMedia "
      " FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
      " StartBlock=%u EndBlock=%u Copy=%d Strip=%d MediaId=%lld\n";

inline constexpr const char Update_filelist[]
    = "Catreq Job=%127s UpdateFileList\n";

inline constexpr const char Update_jobrecord[]
    = "Catreq Job=%127s UpdateJobRecord JobFiles=%" PRIu32 " JobBytes=%" PRIu64
      "\n";
inline constexpr const char Delete_nulljobmediarecord[]
    = "CatReq Job=%127s DeleteNullJobmediaRecords jobid=%u\n";

// Responses sent to Storage daemon
inline constexpr const char OK_media[]
    = "1000 OK VolName=%s VolJobs=%u VolFiles=%u"
      " VolBlocks=%u VolBytes=%s VolMounts=%u VolErrors=%u VolWrites=%u"
      " MaxVolBytes=%s VolCapacityBytes=%s VolStatus=%s Slot=%d"
      " MaxVolJobs=%u MaxVolFiles=%u InChanger=%d VolReadTime=%s"
      " VolWriteTime=%s EndFile=%u EndBlock=%u LabelType=%d"
      " MediaId=%s EncryptionKey=%s MinBlocksize=%d MaxBlocksize=%d\n";
inline constexpr const char OK_create[] = "1000 OK CreateJobMedia\n";
inline constexpr const char OK_delete[] = "1000 OK DeleteNullJobmediaRecords\n";


}  // namespace directordaemon

#endif  // BAREOS_DIRD_COMM_H_
