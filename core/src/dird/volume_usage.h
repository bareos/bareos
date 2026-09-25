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

#ifndef BAREOS_DIRD_VOLUME_USAGE_H_
#define BAREOS_DIRD_VOLUME_USAGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace directordaemon {

struct VolumeUsageSegment {
  uint64_t jobmediaid = 0;
  uint64_t jobid = 0;
  std::string job;
  std::string client;
  std::string volumename;
  uint64_t firstindex = 0;
  uint64_t lastindex = 0;
  uint64_t startfile = 0;
  uint64_t endfile = 0;
  uint64_t startblock = 0;
  uint64_t endblock = 0;
  uint64_t jobbytes = 0;
  uint64_t weight = 1;
  char marker = '#';
};

uint64_t VolumeUsageFileIndexSpan(const VolumeUsageSegment& segment);
uint64_t VolumeUsageMediaStart(const VolumeUsageSegment& segment);
uint64_t VolumeUsageMediaEnd(const VolumeUsageSegment& segment);
std::string VolumeUsageSegmentMarker(size_t index);
void AssignVolumeUsageWeights(std::vector<VolumeUsageSegment>& segments);
void AssignVolumeUsageMarkersByJob(std::vector<VolumeUsageSegment>& segments);
bool HasOverlappingVolumeUsageRanges(
    const std::vector<VolumeUsageSegment>& segments);
std::vector<size_t> VolumeUsageSegmentWidths(
    const std::vector<VolumeUsageSegment>& segments,
    size_t bar_width);
std::string BuildVolumeUsageTapeBar(
    const std::vector<VolumeUsageSegment>& segments,
    size_t bar_width);
std::string BuildVolumeUsageRangeBar(
    const VolumeUsageSegment& segment,
    const std::vector<VolumeUsageSegment>& all_segments,
    size_t bar_width);

} /* namespace directordaemon */

#endif  // BAREOS_DIRD_VOLUME_USAGE_H_
