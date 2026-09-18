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

#include <gtest/gtest.h>

namespace directordaemon {

static VolumeUsageSegment MakeSegment(uint64_t firstindex,
                                      uint64_t lastindex,
                                      uint64_t jobbytes = 0,
                                      uint64_t jobid = 0,
                                      uint64_t startblock = 0,
                                      uint64_t endblock = 0)
{
  VolumeUsageSegment segment;
  segment.jobid = jobid;
  segment.firstindex = firstindex;
  segment.lastindex = lastindex;
  segment.startblock = startblock;
  segment.endblock = endblock;
  segment.jobbytes = jobbytes;
  return segment;
}

TEST(VolumeUsage, AssignsByteWeightsWhenAvailable)
{
  std::vector<VolumeUsageSegment> segments{
      MakeSegment(1, 100, 25),
      MakeSegment(1, 5, 75),
  };

  AssignVolumeUsageWeights(segments);

  EXPECT_EQ(segments[0].weight, 25);
  EXPECT_EQ(segments[1].weight, 75);
  EXPECT_EQ(VolumeUsageSegmentWidths(segments, 20),
            (std::vector<size_t>{5, 15}));
  EXPECT_EQ(BuildVolumeUsageTapeBar(segments, 20), "11111222222222222222");
}

TEST(VolumeUsage, FallsBackToFileIndexSpans)
{
  std::vector<VolumeUsageSegment> segments{
      MakeSegment(1, 10),
      MakeSegment(1, 30),
  };

  AssignVolumeUsageWeights(segments);

  EXPECT_EQ(segments[0].weight, 10);
  EXPECT_EQ(segments[1].weight, 30);
  EXPECT_EQ(VolumeUsageSegmentWidths(segments, 20),
            (std::vector<size_t>{5, 15}));
}

TEST(VolumeUsage, HandlesEmptyAndTinyBars)
{
  EXPECT_TRUE(VolumeUsageSegmentWidths({}, 20).empty());
  EXPECT_EQ(BuildVolumeUsageTapeBar({}, 20), "");

  std::vector<VolumeUsageSegment> segments{
      MakeSegment(0, 0, 1),
      MakeSegment(0, 0, 1),
      MakeSegment(0, 0, 1),
  };
  AssignVolumeUsageWeights(segments);

  EXPECT_EQ(VolumeUsageSegmentWidths(segments, 2),
            (std::vector<size_t>{1, 1, 0}));
  EXPECT_EQ(BuildVolumeUsageTapeBar(segments, 2), "12");
}

TEST(VolumeUsage, RotatesReadableMarkers)
{
  EXPECT_EQ(VolumeUsageSegmentMarker(0), "1");
  EXPECT_EQ(VolumeUsageSegmentMarker(8), "9");
  EXPECT_EQ(VolumeUsageSegmentMarker(9), "A");
}

TEST(VolumeUsage, DetectsFiveParallelOverlappingJobs)
{
  std::vector<VolumeUsageSegment> segments{
      MakeSegment(1, 10, 10, 101, 100, 500),
      MakeSegment(1, 10, 10, 102, 200, 600),
      MakeSegment(1, 10, 10, 103, 300, 700),
      MakeSegment(1, 10, 10, 104, 400, 800),
      MakeSegment(1, 10, 10, 105, 500, 900),
  };
  AssignVolumeUsageMarkersByJob(segments);

  EXPECT_TRUE(HasOverlappingVolumeUsageRanges(segments));
  EXPECT_EQ(segments[0].marker, '1');
  EXPECT_EQ(segments[4].marker, '5');
  EXPECT_EQ(BuildVolumeUsageRangeBar(segments[0], segments, 9), "11111    ");
  EXPECT_EQ(BuildVolumeUsageRangeBar(segments[4], segments, 9), "    55555");
}

TEST(VolumeUsage, ReusesMarkersForRepeatedJobRanges)
{
  std::vector<VolumeUsageSegment> segments{
      MakeSegment(1, 10, 10, 101, 100, 200),
      MakeSegment(1, 10, 10, 102, 300, 400),
      MakeSegment(11, 20, 10, 101, 500, 600),
  };
  AssignVolumeUsageMarkersByJob(segments);

  EXPECT_FALSE(HasOverlappingVolumeUsageRanges(segments));
  EXPECT_EQ(segments[0].marker, '1');
  EXPECT_EQ(segments[1].marker, '2');
  EXPECT_EQ(segments[2].marker, '1');
}

} /* namespace directordaemon */
