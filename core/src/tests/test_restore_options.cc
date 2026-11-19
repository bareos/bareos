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

#include "dird/restore_options.h"

#include "gtest/gtest.h"

namespace {

using directordaemon::restore_options::BuildLiteralPrefixRegexWhere;
using directordaemon::restore_options::BuildRestoreRunCommand;
using directordaemon::restore_options::kDefaultCustomRegexWhere;
using directordaemon::restore_options::kDefaultRelocationExamplePath;
using directordaemon::restore_options::ParseReplacePolicy;
using directordaemon::restore_options::PreviewRegexWhere;
using directordaemon::restore_options::QuoteDirectorString;
using directordaemon::restore_options::ReplacePolicy;
using directordaemon::restore_options::ReplacePolicyName;
using directordaemon::restore_options::RestoreRunOptions;

TEST(RestoreOptions, QuotesDirectorStrings)
{
  EXPECT_EQ(QuoteDirectorString(R"(/tmp/a "quoted" \ path)"),
            R"("/tmp/a \"quoted\" \\ path")");
}

TEST(RestoreOptions, ParsesReplacePoliciesCaseInsensitively)
{
  EXPECT_EQ(ParseReplacePolicy("always"), ReplacePolicy::kAlways);
  EXPECT_EQ(ParseReplacePolicy("IFNEWER"), ReplacePolicy::kIfNewer);
  EXPECT_EQ(ParseReplacePolicy("IfOlder"), ReplacePolicy::kIfOlder);
  EXPECT_EQ(ParseReplacePolicy("never"), ReplacePolicy::kNever);
  EXPECT_EQ(ParseReplacePolicy("sometimes"), std::nullopt);
}

TEST(RestoreOptions, NamesReplacePolicies)
{
  EXPECT_EQ(ReplacePolicyName(ReplacePolicy::kAlways), "Always");
  EXPECT_EQ(ReplacePolicyName(ReplacePolicy::kIfNewer), "IfNewer");
  EXPECT_EQ(ReplacePolicyName(ReplacePolicy::kIfOlder), "IfOlder");
  EXPECT_EQ(ReplacePolicyName(ReplacePolicy::kNever), "Never");
}

TEST(RestoreOptions, BuildsBasicRestoreRunCommand)
{
  RestoreRunOptions options;
  options.restore_job = "RestoreFiles";
  options.backup_client = "backup-fd";
  options.restore_client = "target-fd";
  options.storage = "File";
  options.bootstrap = "/tmp/restore.bsr";
  options.files = 2;
  options.catalog = "MyCatalog";
  options.where = "/tmp/bareos-restores";
  options.replace = ReplacePolicyName(ReplacePolicy::kIfNewer);
  options.yes = true;

  EXPECT_EQ(BuildRestoreRunCommand(options),
            "run job=\"RestoreFiles\" client=\"backup-fd\" "
            "restoreclient=\"target-fd\" storage=\"File\" "
            "bootstrap=\"/tmp/restore.bsr\" files=2 catalog=\"MyCatalog\" "
            "where=\"/tmp/bareos-restores\" replace=\"IfNewer\" yes");
}

TEST(RestoreOptions, PrefersRegexWhereOverWhere)
{
  RestoreRunOptions options;
  options.restore_job = "RestoreFiles";
  options.where = "/tmp/out";
  options.regex_where = R"(!^/home/!/restore/home/!)";

  EXPECT_EQ(BuildRestoreRunCommand(options),
            R"(run job="RestoreFiles" regexwhere="!^/home/!/restore/home/!")");
}

TEST(RestoreOptions, DistinguishesEmptyWhereFromOmittedWhere)
{
  RestoreRunOptions options;
  options.restore_job = "RestoreFiles";
  EXPECT_EQ(BuildRestoreRunCommand(options), R"(run job="RestoreFiles")");

  options.where = "";
  EXPECT_EQ(BuildRestoreRunCommand(options),
            R"(run job="RestoreFiles" where="")");
}

TEST(RestoreOptions, DefaultRegexWhereExampleShowsPathRewrite)
{
  auto preview = PreviewRegexWhere(kDefaultCustomRegexWhere,
                                   kDefaultRelocationExamplePath);

  ASSERT_TRUE(preview);
  EXPECT_EQ(*preview, "/restore/home/alice/documents/report.pdf");
  EXPECT_NE(*preview, kDefaultRelocationExamplePath);
}

TEST(RestoreOptions, LiteralSourcePrefixEscapesRegexMetacharacters)
{
  const std::string regex_where
      = BuildLiteralPrefixRegexWhere("/home/a.b", "/restore/home");

  auto matching_path = PreviewRegexWhere(regex_where, "/home/a.b/report.pdf");
  ASSERT_TRUE(matching_path);
  EXPECT_EQ(*matching_path, "/restore/home/report.pdf");

  auto similar_path = PreviewRegexWhere(regex_where, "/home/axb/report.pdf");
  ASSERT_TRUE(similar_path);
  EXPECT_EQ(*similar_path, "/home/axb/report.pdf");

  const std::string strip_regex_where
      = BuildLiteralPrefixRegexWhere("/home/a.b", "");

  auto stripped_path
      = PreviewRegexWhere(strip_regex_where, "/home/a.b/report.pdf");
  ASSERT_TRUE(stripped_path);
  EXPECT_EQ(*stripped_path, "/report.pdf");

  auto similar_strip_path
      = PreviewRegexWhere(strip_regex_where, "/home/axb/report.pdf");
  ASSERT_TRUE(similar_strip_path);
  EXPECT_EQ(*similar_strip_path, "/home/axb/report.pdf");
  EXPECT_EQ(PreviewRegexWhere(strip_regex_where, "/mnt/home/a.b/report.pdf"),
            "/mnt/home/a.b/report.pdf");
}

TEST(RestoreOptions, LiteralSourcePrefixHandlesDelimiterAndBackslash)
{
  constexpr std::string_view prefix = R"(/home/a!\)";
  const std::string replace
      = BuildLiteralPrefixRegexWhere(prefix, "/restore/home");
  const std::string strip = BuildLiteralPrefixRegexWhere(prefix, "");

  EXPECT_EQ(PreviewRegexWhere(replace, R"(/home/a!\/report.pdf)"),
            "/restore/home/report.pdf");
  EXPECT_EQ(PreviewRegexWhere(strip, R"(/home/a!\/report.pdf)"), "/report.pdf");
  EXPECT_EQ(PreviewRegexWhere(strip, "/home/a!/report.pdf"),
            "/home/a!/report.pdf");
}

TEST(RestoreOptions, LiteralReplacementHandlesTrailingBackslash)
{
  const auto rule = BuildLiteralPrefixRegexWhere("/source/", R"(D:\restore\)");
  EXPECT_EQ(PreviewRegexWhere(rule, "/source/report.pdf"),
            R"(D:\restore\report.pdf)");
}

TEST(RestoreOptions, LiteralReplacementDoesNotExpandReferences)
{
  const auto rule = BuildLiteralPrefixRegexWhere("/source/", R"(D:\1st/$1/)");
  EXPECT_EQ(PreviewRegexWhere(rule, "/source/report.pdf"),
            R"(D:\1st/$1/report.pdf)");
}

TEST(RestoreOptions, LiteralReplacementDecodesEscapesOnlyOnce)
{
  const auto rule
      = BuildLiteralPrefixRegexWhere("/source@/", R"(/out\!@%5C%40/)");
  EXPECT_EQ(PreviewRegexWhere(rule, "/source@/report.pdf"),
            R"(/out\!@%5C%40/report.pdf)");
}

TEST(RestoreOptions, LiteralReplacementCanStartWithDelimiter)
{
  const auto rule = BuildLiteralPrefixRegexWhere("/source/", "!out/");
  EXPECT_EQ(PreviewRegexWhere(rule, "/source/report.pdf"), "!out/report.pdf");
}

TEST(RestoreOptions, CustomRulesStillExpandReferencesAndChain)
{
  EXPECT_EQ(PreviewRegexWhere(R"(!^/source/(a\.b)/!/$1/\1/!,!^/a\.b/!/out/!)",
                              "/source/a.b/report.pdf"),
            "/out/a.b/report.pdf");
}

TEST(RestoreOptions, BuildsAdvancedRestoreRunCommand)
{
  RestoreRunOptions options;
  options.restore_job = "RestoreFiles";
  options.backup_format = "Native";
  options.plugin_options
      = "python:module_name=bareos-fd-vmware\nbarri:files=/dev/null";
  options.comment = R"(operator said "restore")";
  options.when = "2026-09-17 18:30:00";
  options.priority = 42;

  EXPECT_EQ(BuildRestoreRunCommand(options),
            "run job=\"RestoreFiles\" backupformat=\"Native\" "
            "pluginoptions=\"python:module_name=bareos-fd-vmware\n"
            "barri:files=/dev/null\" comment=\"operator said \\\"restore\\\"\" "
            "when=\"2026-09-17 18:30:00\" priority=42");
}

}  // namespace
