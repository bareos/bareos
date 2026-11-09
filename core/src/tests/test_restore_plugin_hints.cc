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

#include <algorithm>

#include "gtest/gtest.h"
#include "include/bareos.h"

namespace {

using directordaemon::restore_plugin_hints::AllPluginOptionsBlocksAuthorized;
using directordaemon::restore_plugin_hints::AllPluginRestoreHints;
using directordaemon::restore_plugin_hints::BuildInitialPluginOptionsBlock;
using directordaemon::restore_plugin_hints::BuildPluginOptionExample;
using directordaemon::restore_plugin_hints::BuildPluginOptionsBlock;
using directordaemon::restore_plugin_hints::BuildPluginOptionsDocument;
using directordaemon::restore_plugin_hints::CategorizedPluginOptions;
using directordaemon::restore_plugin_hints::CategorizePluginOptions;
using directordaemon::restore_plugin_hints::ExtractFileSetPluginDefinitions;
using directordaemon::restore_plugin_hints::FileSetPluginDefinition;
using directordaemon::restore_plugin_hints::FindMatchingPluginDefinition;
using directordaemon::restore_plugin_hints::FindPluginRestoreHint;
using directordaemon::restore_plugin_hints::HintProvidesDefaultsElsewhere;
using directordaemon::restore_plugin_hints::ParsePluginOptionsBlock;
using directordaemon::restore_plugin_hints::ParsePluginOptionsDocument;
using directordaemon::restore_plugin_hints::PluginOptionsBlock;
using directordaemon::restore_plugin_hints::PluginOptionType;
using directordaemon::restore_plugin_hints::PluginOptionTypeName;
using directordaemon::restore_plugin_hints::PluginRestoreHint;
using directordaemon::restore_plugin_hints::ResolvePluginOptionsBlockHint;
using directordaemon::restore_plugin_hints::ResolvePluginRestoreHint;
using directordaemon::restore_plugin_hints::SortedPluginRestoreHints;

TEST(RestorePluginHints, KnowsAllCuratedPlugins)
{
  EXPECT_EQ(AllPluginRestoreHints().size(), 20u);
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

TEST(RestorePluginHints, BarriIncludesRequiredRestoreTarget)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("barri");
  ASSERT_NE(hint, nullptr);
  ASSERT_FALSE(hint->options.empty());
  EXPECT_EQ(hint->options.front().name, "files");
  EXPECT_EQ(hint->options.front().status, "required");
  EXPECT_EQ(hint->options.front().type, PluginOptionType::kPath);
  EXPECT_EQ(BuildPluginOptionExample(*hint), "files=...");
}

TEST(RestorePluginHints, IncusIncludesRestoreOptions)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("bareos-fd-incus");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "incus");

  auto restore_path = std::find_if(
      hint->options.begin(), hint->options.end(),
      [](const auto& option) { return option.name == "restore_path"; });
  ASSERT_NE(restore_path, hint->options.end());
  EXPECT_EQ(restore_path->status, "optional");
  EXPECT_EQ(restore_path->type, PluginOptionType::kPath);

  auto restore_buffer_depth = std::find_if(
      hint->options.begin(), hint->options.end(),
      [](const auto& option) { return option.name == "restore_buffer_depth"; });
  ASSERT_NE(restore_buffer_depth, hint->options.end());
  EXPECT_EQ(restore_buffer_depth->type, PluginOptionType::kInteger);
}

TEST(RestorePluginHints, QumuloOptionsMatchPluginSource)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("yuzuy-qumulo");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "qumulo");

  auto host
      = std::find_if(hint->options.begin(), hint->options.end(),
                     [](const auto& option) { return option.name == "host"; });
  ASSERT_NE(host, hint->options.end());
  EXPECT_EQ(host->status, "required");
  EXPECT_EQ(host->source, "plugin-source");

  auto port
      = std::find_if(hint->options.begin(), hint->options.end(),
                     [](const auto& option) { return option.name == "port"; });
  ASSERT_NE(port, hint->options.end());
  EXPECT_EQ(port->type, PluginOptionType::kInteger);

  auto config_file = std::find_if(
      hint->options.begin(), hint->options.end(),
      [](const auto& option) { return option.name == "config_file"; });
  ASSERT_NE(config_file, hint->options.end());
  EXPECT_EQ(config_file->type, PluginOptionType::kPath);
  EXPECT_TRUE(config_file->provides_defaults);

  EXPECT_EQ(
      std::find_if(hint->options.begin(), hint->options.end(),
                   [](const auto& option) { return option.name == "cluster"; }),
      hint->options.end());
}

TEST(RestorePluginHints, BuildsExampleFromKnownOptionsWhenNoneRequired)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("bpipe");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(BuildPluginOptionExample(*hint), "file=...:reader=...");
}

TEST(RestorePluginHints, BuildsInitialBlockWithModuleNamePrefilled)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = "
      "\"python:module_name=bareos-fd-vmware:vcserver=host\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  PluginOptionsBlock block = BuildInitialPluginOptionsBlock(definitions[0]);
  EXPECT_EQ(block.plugin_name, "python");
  ASSERT_EQ(block.options.size(), 1u);
  EXPECT_EQ(block.options[0].first, "module_name");
  EXPECT_EQ(block.options[0].second, "bareos-fd-vmware");
}

TEST(RestorePluginHints, BuildsInitialBlockWithoutModuleNameOption)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"bpipe:file=/a:reader=cat /a\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  PluginOptionsBlock block = BuildInitialPluginOptionsBlock(definitions[0]);
  EXPECT_EQ(block.plugin_name, "bpipe");
  EXPECT_TRUE(block.options.empty());
}

TEST(RestorePluginHints, ResolvesBlockHintPreferringModuleNameOption)
{
  PluginOptionsBlock block;
  block.plugin_name = "python";
  block.options.emplace_back("module_name", "bareos-fd-vmware");
  block.options.emplace_back("vcserver", "host");

  const PluginRestoreHint* hint = ResolvePluginOptionsBlockHint(block);
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "vmware");
}

TEST(RestorePluginHints, ResolvesBlockHintByDirectPluginNameFallback)
{
  PluginOptionsBlock block;
  block.plugin_name = "bpipe";
  block.options.emplace_back("file", "/a");

  const PluginRestoreHint* hint = ResolvePluginOptionsBlockHint(block);
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(hint->id, "bpipe");
}

TEST(RestorePluginHints, FindsMatchingDefinitionByResolvedHintId)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = "
      "\"python:module_name=bareos-fd-vmware:vcserver=host\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  const PluginRestoreHint* hint = FindPluginRestoreHint("vmware");
  ASSERT_NE(hint, nullptr);

  const FileSetPluginDefinition* match
      = FindMatchingPluginDefinition(*hint, definitions);
  ASSERT_NE(match, nullptr);
  EXPECT_EQ(match->plugin_name, "python");
}

TEST(RestorePluginHints, FindMatchingDefinitionReturnsNullptrWhenNoMatch)
{
  auto definitions = ExtractFileSetPluginDefinitions(
      "  Plugin = \"bpipe:file=/a:reader=cat /a\"\n");
  ASSERT_EQ(definitions.size(), 1u);

  const PluginRestoreHint* hint = FindPluginRestoreHint("vmware");
  ASSERT_NE(hint, nullptr);
  EXPECT_EQ(FindMatchingPluginDefinition(*hint, definitions), nullptr);
}

TEST(RestorePluginHints, CategorizesOptionsIntoFilesetRequiredOptional)
{
  const PluginRestoreHint* hint = FindPluginRestoreHint("vmware");
  ASSERT_NE(hint, nullptr);

  CategorizedPluginOptions categorized
      = CategorizePluginOptions(*hint, {"vcserver"});

  EXPECT_EQ(categorized.already_in_fileset.size(), 1u);
  EXPECT_EQ(categorized.already_in_fileset[0]->name, "vcserver");
  for (const auto* option : categorized.required) {
    EXPECT_EQ(option->status, "required");
    EXPECT_NE(option->name, "vcserver");
  }
  for (const auto* option : categorized.optional) {
    EXPECT_NE(option->status, "required");
    EXPECT_NE(option->name, "vcserver");
  }
}

TEST(RestorePluginHints, HintProvidesDefaultsElsewhereDetectsConfigFileOption)
{
  const PluginRestoreHint* vmware_hint = FindPluginRestoreHint("vmware");
  ASSERT_NE(vmware_hint, nullptr);
  EXPECT_TRUE(HintProvidesDefaultsElsewhere(*vmware_hint));

  const PluginRestoreHint* bpipe_hint = FindPluginRestoreHint("bpipe");
  ASSERT_NE(bpipe_hint, nullptr);
  EXPECT_FALSE(HintProvidesDefaultsElsewhere(*bpipe_hint));
}

TEST(RestorePluginHints, PluginOptionTypeNameReturnsExpectedStrings)
{
  EXPECT_EQ(PluginOptionTypeName(PluginOptionType::kString), "string");
  EXPECT_EQ(PluginOptionTypeName(PluginOptionType::kBoolean), "boolean");
  EXPECT_EQ(PluginOptionTypeName(PluginOptionType::kInteger), "integer");
  EXPECT_EQ(PluginOptionTypeName(PluginOptionType::kPath), "path");
}

TEST(PluginOptionsModel, ParsesBlockWithNameOnly)
{
  PluginOptionsBlock block = ParsePluginOptionsBlock("bpipe");
  EXPECT_EQ(block.plugin_name, "bpipe");
  EXPECT_TRUE(block.options.empty());
}

TEST(PluginOptionsModel, ParsesBlockWithKeyValueOptions)
{
  PluginOptionsBlock block
      = ParsePluginOptionsBlock("bpipe:file=/tmp/x:reader=cat %f");
  EXPECT_EQ(block.plugin_name, "bpipe");
  ASSERT_EQ(block.options.size(), 2u);
  EXPECT_EQ(block.options[0].first, "file");
  EXPECT_EQ(block.options[0].second, "/tmp/x");
  EXPECT_EQ(block.options[1].first, "reader");
  EXPECT_EQ(block.options[1].second, "cat %f");
}

TEST(PluginOptionsModel, ParsesFlagStyleOptionsWithoutEquals)
{
  PluginOptionsBlock block = ParsePluginOptionsBlock("barri:verbose");
  ASSERT_EQ(block.options.size(), 1u);
  EXPECT_EQ(block.options[0].first, "verbose");
  EXPECT_EQ(block.options[0].second, "");
}

TEST(PluginOptionsModel, SkipsEmptySegmentsBetweenColons)
{
  PluginOptionsBlock block = ParsePluginOptionsBlock("bpipe::file=x:");
  ASSERT_EQ(block.options.size(), 1u);
  EXPECT_EQ(block.options[0].first, "file");
}

TEST(PluginOptionsModel, RoundTripsBlockThroughBuild)
{
  const std::string original = "bpipe:file=/tmp/x:reader=cat %f";
  PluginOptionsBlock block = ParsePluginOptionsBlock(original);
  EXPECT_EQ(BuildPluginOptionsBlock(block), original);
}

TEST(PluginOptionsModel, BuildsFlagOptionWithoutEquals)
{
  PluginOptionsBlock block;
  block.plugin_name = "barri";
  block.options.emplace_back("verbose", "");
  EXPECT_EQ(BuildPluginOptionsBlock(block), "barri:verbose");
}

TEST(PluginOptionsModel, ParsesMultiBlockDocumentSeparatedByNewline)
{
  auto blocks
      = ParsePluginOptionsDocument("bpipe:file=/tmp/x\n\nbarri:verbose\n");
  ASSERT_EQ(blocks.size(), 2u);
  EXPECT_EQ(blocks[0].plugin_name, "bpipe");
  EXPECT_EQ(blocks[1].plugin_name, "barri");
}

TEST(PluginOptionsModel, RoundTripsDocumentThroughBuild)
{
  std::vector<PluginOptionsBlock> blocks
      = ParsePluginOptionsDocument("bpipe:file=/tmp/x\nbarri:verbose");
  EXPECT_EQ(BuildPluginOptionsDocument(blocks),
            "bpipe:file=/tmp/x\nbarri:verbose");
}

TEST(PluginOptionsModel, BuildsEmptyDocumentForNoBlocks)
{
  EXPECT_EQ(BuildPluginOptionsDocument({}), "");
}

TEST(PluginOptionsModel, AllBlocksAuthorizedWhenEveryBlockPasses)
{
  EXPECT_TRUE(AllPluginOptionsBlocksAuthorized(
      "bpipe:file=/tmp/x\nbarri:verbose",
      [](const std::string&) { return true; }));
}

TEST(PluginOptionsModel, AllBlocksAuthorizedIsFalseIfAnyBlockDenied)
{
  // A single denied block must fail the whole document, even if an
  // earlier block in the same (attacker-controlled) multi-line string
  // would individually be authorized -- an allowed block must not be
  // able to "smuggle" a denied one past a whole-string ACL check.
  EXPECT_FALSE(AllPluginOptionsBlocksAuthorized(
      "allowed:opt=1\ndenied:opt=2",
      [](const std::string& block) { return block.starts_with("allowed:"); }));
}

TEST(PluginOptionsModel, AllBlocksAuthorizedForEmptyDocument)
{
  EXPECT_TRUE(AllPluginOptionsBlocksAuthorized(
      "", [](const std::string&) { return false; }));
}

}  // namespace
