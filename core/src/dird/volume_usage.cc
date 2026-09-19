/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#include "dird/volume_usage.h"

#include <algorithm>

namespace directordaemon {

uint64_t VolumeUsageFileIndexSpan(const VolumeUsageSegment& segment)
{
  if (segment.lastindex >= segment.firstindex) {
    return segment.lastindex - segment.firstindex + 1;
  }
  return 1;
}

uint64_t VolumeUsageMediaStart(const VolumeUsageSegment& segment)
{
  return (segment.startfile << 32) | segment.startblock;
}

uint64_t VolumeUsageMediaEnd(const VolumeUsageSegment& segment)
{
  const uint64_t end = (segment.endfile << 32) | segment.endblock;
  return std::max(end, VolumeUsageMediaStart(segment));
}

std::string VolumeUsageSegmentMarker(size_t index)
{
  static constexpr const char kMarkers[]
      = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  return std::string(1, kMarkers[index % (sizeof(kMarkers) - 1)]);
}

void AssignVolumeUsageMarkersByJob(std::vector<VolumeUsageSegment>& segments)
{
  std::vector<uint64_t> jobids;
  for (auto& segment : segments) {
    const auto found = std::find(jobids.begin(), jobids.end(), segment.jobid);
    if (found == jobids.end()) {
      jobids.push_back(segment.jobid);
      segment.marker = VolumeUsageSegmentMarker(jobids.size() - 1)[0];
    } else {
      segment.marker = VolumeUsageSegmentMarker(
          static_cast<size_t>(std::distance(jobids.begin(), found)))[0];
    }
  }
}

void AssignVolumeUsageWeights(std::vector<VolumeUsageSegment>& segments)
{
  const bool has_bytes = std::any_of(
      segments.begin(), segments.end(),
      [](const VolumeUsageSegment& segment) { return segment.jobbytes > 0; });

  for (auto& segment : segments) {
    segment.weight
        = has_bytes ? segment.jobbytes : VolumeUsageFileIndexSpan(segment);
    if (segment.weight == 0) { segment.weight = 1; }
  }
}

bool HasOverlappingVolumeUsageRanges(
    const std::vector<VolumeUsageSegment>& segments)
{
  if (segments.size() < 2) { return false; }

  std::vector<std::pair<uint64_t, uint64_t>> ranges;
  ranges.reserve(segments.size());
  for (const auto& segment : segments) {
    ranges.emplace_back(VolumeUsageMediaStart(segment),
                        VolumeUsageMediaEnd(segment));
  }
  std::sort(ranges.begin(), ranges.end());

  uint64_t current_end = ranges[0].second;
  for (size_t i = 1; i < ranges.size(); ++i) {
    if (ranges[i].first <= current_end) { return true; }
    current_end = std::max(current_end, ranges[i].second);
  }
  return false;
}

static uint64_t TotalVolumeUsageWeight(
    const std::vector<VolumeUsageSegment>& segments)
{
  uint64_t total = 0;
  for (const auto& segment : segments) { total += segment.weight; }
  return total ? total : 1;
}

std::vector<size_t> VolumeUsageSegmentWidths(
    const std::vector<VolumeUsageSegment>& segments,
    size_t bar_width)
{
  std::vector<size_t> widths;
  widths.reserve(segments.size());
  if (segments.empty() || bar_width == 0) { return widths; }

  const uint64_t total = TotalVolumeUsageWeight(segments);
  size_t used = 0;
  for (size_t i = 0; i < segments.size(); ++i) {
    size_t width = 1;
    if (i + 1 == segments.size()) {
      width = used < bar_width ? bar_width - used : 0;
    } else if (used < bar_width) {
      const auto ratio = static_cast<long double>(segments[i].weight)
                         / static_cast<long double>(total);
      width = std::max<size_t>(1, static_cast<size_t>(ratio * bar_width));
      width = std::min(width, bar_width - used);
    } else {
      width = 0;
    }
    widths.push_back(width);
    used += width;
  }
  return widths;
}

std::string BuildVolumeUsageTapeBar(
    const std::vector<VolumeUsageSegment>& segments,
    size_t bar_width)
{
  std::string bar;
  const auto widths = VolumeUsageSegmentWidths(segments, bar_width);
  for (size_t i = 0; i < segments.size(); ++i) {
    const char marker = segments[i].marker == '#'
                            ? VolumeUsageSegmentMarker(i)[0]
                            : segments[i].marker;
    bar.append(widths[i], marker);
  }
  return bar;
}

static size_t VolumeUsagePositionColumn(uint64_t position,
                                        uint64_t first_position,
                                        uint64_t last_position,
                                        size_t bar_width)
{
  if (bar_width <= 1 || last_position <= first_position) { return 0; }

  const auto ratio = static_cast<long double>(position - first_position)
                     / static_cast<long double>(last_position - first_position);
  return std::min(bar_width - 1, static_cast<size_t>(ratio * (bar_width - 1)));
}

std::string BuildVolumeUsageRangeBar(
    const VolumeUsageSegment& segment,
    const std::vector<VolumeUsageSegment>& all_segments,
    size_t bar_width)
{
  if (bar_width == 0) { return ""; }

  std::string bar(bar_width, ' ');
  if (all_segments.empty()) { return bar; }

  uint64_t first_position = VolumeUsageMediaStart(all_segments[0]);
  uint64_t last_position = VolumeUsageMediaEnd(all_segments[0]);
  for (const auto& candidate : all_segments) {
    first_position = std::min(first_position, VolumeUsageMediaStart(candidate));
    last_position = std::max(last_position, VolumeUsageMediaEnd(candidate));
  }

  const size_t start_column = VolumeUsagePositionColumn(
      VolumeUsageMediaStart(segment), first_position, last_position, bar_width);
  const size_t end_column = VolumeUsagePositionColumn(
      VolumeUsageMediaEnd(segment), first_position, last_position, bar_width);
  for (size_t column = start_column; column <= end_column; ++column) {
    bar[column] = segment.marker;
  }
  return bar;
}

} /* namespace directordaemon */
