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

#include "dird/restore_plugin_hints.h"

#include "gtest/gtest.h"
#include "include/bareos.h"

namespace {

using directordaemon::restore_plugin_hints::AllPluginRestoreHints;
using directordaemon::restore_plugin_hints::BuildPluginOptionExample;
using directordaemon::restore_plugin_hints::ExtractFileSetPluginDefinitions;
using directordaemon::restore_plugin_hints::FindPluginRestoreHint;
using directordaemon::restore_plugin_hints::PluginRestoreHint;
using directordaemon::restore_plugin_hints::ResolvePluginRestoreHint;
using directordaemon::restore_plugin_hints::SortedPluginRestoreHints;

TEST(RestorePluginHints, KnowsAllCuratedPlugins)
{
  EXPECT_EQ(AllPluginRestoreHints().size(), 19u);
}

TEST(RestorePluginHints, FindsHintByIdAndAliasCaseInsensitively)
{
  const PluginRestoreHint* by_id = FindPluginRestoreHint("vmware");
  ASSERT_NE(by_id, nullptr);
  EXPECT_EQ(by_id->display_name, "VMware");

  const PluginRestoreHint* by_alias = FindPluginRestoreHint("BAREOS-FD-VMWARE");
  ASSERT_NE(by_alias, nullptr);
  EXPECT_EQ(by_alias->id, "vmware");

  EXPECT_EQ(FindPluginRestoreHint("does-not-exist"), nullptr);
}

TEST(RestorePluginHints, SortsHintsByDisplayName)
{
  auto sorted = SortedPluginRestoreHints();
  ASSERT_EQ(sorted.size(), AllPluginRestoreHints().size());
  for (size_t i = 1; i < sorted.size(); ++i) {
    EXPECT_LE(sorted[i - 1]->display_name, sorted[i]->display_name);
  }
}

TEST(RestorePluginHints, ExtractsSingleLinePluginDefinition)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  File = /etc\n"
      "  Plugin = \"vmware:vcserver=host:vcuser=admin\"\n");
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].raw, "vmware:vcserver=host:vcuser=admin");
  EXPECT_EQ(definitions[0].plugin_name, "vmware");
  EXPECT_EQ(definitions[0].option_keys,
            (std::vector<std::string>{"vcserver", "vcuser"}));
}

TEST(RestorePluginHints, JoinsMultiLineContinuationLines)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"python\"\n"
      "           \":module_name=bareos-fd-mssql\"\n"
      "           \":instance=SQLEXPRESS\"\n");
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].raw,
            "python:module_name=bareos-fd-mssql:instance=SQLEXPRESS");
}

TEST(RestorePluginHints, ExtractsMultiplePluginDefinitions)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"bpipe:file=/a:reader=cat /a\"\n"
      "  Plugin = \"bpipe:file=/b:reader=cat /b\"\n");
  ASSERT_EQ(definitions.size(), 2u);
  EXPECT_EQ(definitions[0].plugin_name, "bpipe");
  EXPECT_EQ(definitions[1].plugin_name, "bpipe");
}

TEST(RestorePluginHints, ResolvesHintPreferringModuleNameOverPluginName)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"python:module_name=vmware:vcserver=host\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  const PluginRestoreHint* hint = ResolvePluginRestoreHint(definitions[0]);
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "vmware");
}

TEST(RestorePluginHints, ResolvesHintByDirectPluginNameFallback)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"bpipe:file=/a:reader=cat /a\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  const PluginRestoreHint* hint = ResolvePluginRestoreHint(definitions[0]);
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "bpipe");
}

TEST(RestorePluginHints, ReturnsNullptrForUnknownPlugin)
{
  auto definitions
      = ExtractFileSetPluginDefinitions("  Plugin = \"unknown-thing:x=1\"\n");
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(ResolvePluginRestoreHint(definitions[0]), nullptr);
}

TEST(RestorePluginHints, BuildsExamplePreferringRequiredOptions)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("vmware");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(BuildPluginOptionExample(*hint), "vcserver=...:vcuser=...");
}

TEST(RestorePluginHints, BuildsExampleFromKnownOptionsWhenNoneRequired)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("bpipe");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(BuildPluginOptionExample(*hint), "file=...:reader=...");
}

}  // namespace
