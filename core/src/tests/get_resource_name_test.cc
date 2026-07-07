/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"
#include "include/bareos.h"

#include "lib/util.h"

#include <string>

/*
 * This test validates the GetSafeResourceName() utility function that safely
 * extracts resource names with defensive null checks.
 *
 * The function prevents crashes from null resource_name_ pointers
 * (e.g., during multi-threaded resource deallocation).
 */

// Mock resource structure matching bareos_resource.h
struct MockResource {
  char* resource_name_;

  MockResource() : resource_name_(nullptr) {}
  ~MockResource() {}
};

// Test with null resource pointer
TEST(GetSafeResourceName, NullResourcePointerReturnsDefault)
{
  MockResource* resource = nullptr;
  std::string result = GetSafeResourceName(resource);

  EXPECT_EQ(result, "**None**");
}

// Test with null resource pointer and custom fallback
TEST(GetSafeResourceName, NullResourcePointerReturnsCustomFallback)
{
  MockResource* resource = nullptr;
  std::string result = GetSafeResourceName(resource, "**Unknown**");

  EXPECT_EQ(result, "**Unknown**");
}

// Test with null resource_name_ pointer
TEST(GetSafeResourceName, NullResourceNameReturnsDefault)
{
  MockResource resource;
  resource.resource_name_ = nullptr;

  std::string result = GetSafeResourceName(&resource);

  EXPECT_EQ(result, "**None**");
}

// Test with valid resource_name_
TEST(GetSafeResourceName, ValidResourceNameReturnsValue)
{
  MockResource resource;
  resource.resource_name_ = const_cast<char*>("test_client");

  std::string result = GetSafeResourceName(&resource);

  EXPECT_EQ(result, "test_client");
}

// Test with valid resource_name_ and custom fallback (fallback unused)
TEST(GetSafeResourceName, ValidResourceNameIgnoresCustomFallback)
{
  MockResource resource;
  resource.resource_name_ = const_cast<char*>("my_resource");

  std::string result = GetSafeResourceName(&resource, "**Unknown**");

  EXPECT_EQ(result, "my_resource");
}

// Test with different fallback values
TEST(GetSafeResourceName, DifferentFallbackValues)
{
  MockResource resource;
  resource.resource_name_ = nullptr;

  EXPECT_EQ(GetSafeResourceName(&resource, "**None**"), "**None**");
  EXPECT_EQ(GetSafeResourceName(&resource, "**Unknown**"), "**Unknown**");
  EXPECT_EQ(GetSafeResourceName(&resource, ""), "");
  EXPECT_EQ(GetSafeResourceName(&resource, "default"), "default");
}

// Test case matching StartStorageDaemonJob scenario: job resource
TEST(GetSafeResourceName, JobResourceScenario)
{
  MockResource job;
  job.resource_name_ = const_cast<char*>("backup_job");

  std::string job_name = GetSafeResourceName(&job, "**Unknown**");

  EXPECT_EQ(job_name, "backup_job");
}

// Test case matching StartStorageDaemonJob scenario: client without name
TEST(GetSafeResourceName, ClientResourceWithoutNameScenario)
{
  MockResource client;
  client.resource_name_ = nullptr;

  std::string client_name = GetSafeResourceName(&client, "**None**");

  EXPECT_EQ(client_name, "**None**");
}

// Test case matching StartStorageDaemonJob scenario: missing fileset
TEST(GetSafeResourceName, MissingFilesetResourceScenario)
{
  MockResource* fileset = nullptr;

  std::string fileset_name = GetSafeResourceName(fileset, "**None**");

  EXPECT_EQ(fileset_name, "**None**");
}

// Comprehensive scenario: multiple resources with mixed states
TEST(GetSafeResourceName, MultipleResourcesWithMixedStates)
{
  MockResource job;
  job.resource_name_ = const_cast<char*>("test_job");

  MockResource client;
  client.resource_name_ = nullptr;

  MockResource* fileset = nullptr;

  // Verify all three are handled correctly
  EXPECT_EQ(GetSafeResourceName(&job, "**Unknown**"), "test_job");
  EXPECT_EQ(GetSafeResourceName(&client, "**None**"), "**None**");
  EXPECT_EQ(GetSafeResourceName(fileset, "**None**"), "**None**");
}

// Test that the function works with const pointers
TEST(GetSafeResourceName, ConstResourcePointer)
{
  MockResource resource;
  resource.resource_name_ = const_cast<char*>("const_test");

  const MockResource* const_resource = &resource;
  std::string result = GetSafeResourceName(const_resource);

  EXPECT_EQ(result, "const_test");
}

// Test edge case: empty string as resource_name_
TEST(GetSafeResourceName, EmptyStringResourceName)
{
  MockResource resource;
  resource.resource_name_ = const_cast<char*>("");

  std::string result = GetSafeResourceName(&resource);

  // Empty string is still valid (not null)
  EXPECT_EQ(result, "");
}
