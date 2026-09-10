/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2026 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"

#include "dird/ua.h"
#include "include/jcr.h"
#include "dird/ua_configure.cc"

#include <filesystem>

namespace {
// Copies the configure_delete fixture into a fresh temporary directory, so
// that each test gets its own on-disk copy and can safely unlink files
// without affecting other tests or later runs of the same test.
class TempConfigureDeleteConfig {
 public:
  TempConfigureDeleteConfig()
  {
    char tmpl[] = "/tmp/bareos-configure-delete-XXXXXX";
    char* created = mkdtemp(tmpl);
    if (created) {
      path_ = created;
      std::error_code ec;
      std::filesystem::copy("configs/configure_delete", path_,
                            std::filesystem::copy_options::recursive, ec);
      if (ec) { path_.clear(); }
    }
  }

  ~TempConfigureDeleteConfig()
  {
    if (!path_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(path_, ec);
    }
  }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

// Fills in ua->cmd/argc/argk/argv the way bconsole would for a typed
// command, so tests can exercise ConfigureCmd()'s own argument parsing
// (subcommand dispatch, resource-type lookup, "name=" parsing) instead of
// calling ConfigureDeleteResource() directly.
void FakeConfigureCmd(directordaemon::UaContext* ua, const std::string& cmd)
{
  PmStrcpy(ua->cmd, cmd.c_str());
  ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}
}  // namespace

TEST(ConfigureExport, ReturnsQuotedNameAndPassword)
{
  InitDirGlobals();
  std::string path_to_config = std::string("configs/configure");
  PConfigParser client_config(DirectorPrepareResources(path_to_config));
  if (!client_config) { return; }


  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  PoolMem resource(PM_MESSAGE);
  ConfigureCreateFdResourceString(ua, resource, "bareos-fd");
  std::string expected_output{
      "Director {\n"
      "  Name = \"bareos director\"\n"
      "  Password = \"[md5]01234567890123456789012345678912\"\n"
      "}\n"};

  EXPECT_EQ(resource.c_str(), expected_output);

  delete ua;
}

TEST(ConfigureDelete, DeletesUnreferencedResource)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  PoolMem path(PM_FNAME);
  ASSERT_TRUE(directordaemon::my_config->GetPathOfResource(
      path, NULL, res_table->name, "unreferenced-fd", false));
  EXPECT_TRUE(std::filesystem::exists(path.c_str()));

  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "unreferenced-fd"));

  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "unreferenced-fd"),
            nullptr);
  EXPECT_FALSE(std::filesystem::exists(path.c_str()));

  delete ua;
}

TEST(ConfigureDelete, ErrorsOnUnknownResource)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  EXPECT_FALSE(ConfigureDeleteResource(ua, res_table, "does-not-exist"));

  delete ua;
}

TEST(ConfigureDelete, RefusesToDeleteReferencedResource)
{
  // There is no override: deleting the config file while a reference
  // remains would leave that reference dangling on the next reload/restart,
  // so the caller must remove or update the referencing resource first.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  EXPECT_FALSE(ConfigureDeleteResource(ua, res_table, "referenced-fd"));
  EXPECT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "referenced-fd"),
            nullptr);

  delete ua;
}

TEST(ConfigureDelete, DeletesResourceAfterReferenceRemoved)
{
  // Deleting the referencing Job resource first (exercising the same
  // ConfigureDeleteResource() for a different resource type) clears the
  // reference, after which the previously-blocked Client delete succeeds.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* job_table
      = directordaemon::my_config->GetResourceTable("job");
  ASSERT_NE(job_table, nullptr);
  ASSERT_TRUE(ConfigureDeleteResource(ua, job_table, "testjob"));

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  PoolMem path(PM_FNAME);
  ASSERT_TRUE(directordaemon::my_config->GetPathOfResource(
      path, NULL, res_table->name, "referenced-fd", false));

  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "referenced-fd"));

  EXPECT_FALSE(std::filesystem::exists(path.c_str()));
  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "referenced-fd"),
            nullptr);

  delete ua;
}

TEST(ConfigureDelete, FindsResourceReferences)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  BareosResource* referenced_client = directordaemon::my_config->GetResWithName(
      directordaemon::R_CLIENT, "referenced-fd");
  ASSERT_NE(referenced_client, nullptr);

  std::vector<ResourceReference> references
      = directordaemon::my_config->FindResourceReferences(
          directordaemon::R_CLIENT, referenced_client);
  ASSERT_EQ(references.size(), 1u);
  EXPECT_EQ(references[0].rcode, directordaemon::R_JOB);
  EXPECT_STREQ(references[0].resource_name.c_str(), "testjob");
  EXPECT_STREQ(references[0].directive_name.c_str(), "Client");

  BareosResource* unreferenced_client
      = directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                  "unreferenced-fd");
  ASSERT_NE(unreferenced_client, nullptr);
  EXPECT_TRUE(directordaemon::my_config
                  ->FindResourceReferences(directordaemon::R_CLIENT,
                                           unreferenced_client)
                  .empty());
}

TEST(ConfigureDelete, RemovesFiledaemonExportOnClientDelete)
{
  // "configure add client" also writes a File Daemon export file
  // containing a plaintext copy of the Director's password (see
  // ConfigureCreateFdResource()). Deleting the client must also remove
  // that file, so it is not left behind as a stale, credential-bearing
  // artifact.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  ASSERT_TRUE(directordaemon::ConfigureCreateFdResource(ua, "unreferenced-fd"));

  const char* dirname
      = directordaemon::my_config->GetNextRes(directordaemon::R_DIRECTOR, NULL)
            ->resource_name_;
  PoolMem basedir(PM_FNAME);
  basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d",
                   "unreferenced-fd");
  PoolMem export_path(PM_FNAME);
  ASSERT_TRUE(directordaemon::my_config->GetPathOfResource(
      export_path, basedir.c_str(), "director", dirname, false));
  ASSERT_TRUE(std::filesystem::exists(export_path.c_str()));

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  EXPECT_TRUE(directordaemon::ConfigureDeleteResource(ua, res_table,
                                                      "unreferenced-fd"));
  EXPECT_FALSE(std::filesystem::exists(export_path.c_str()));

  delete ua;
}

// The tests above call ConfigureDeleteResource() directly. The tests below
// instead go through ConfigureCmd(), the console command entry point, to
// also cover its own argument parsing: resourcetype/"name=" lookup, the
// Director-cannot-be-deleted rejection, and the "configure" output object
// wrapping that ConfigureDeleteResource() alone does not produce.

TEST(ConfigureDelete, CmdDeletesUnreferencedResource)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  FakeConfigureCmd(ua, "configure delete client name=unreferenced-fd");

  EXPECT_TRUE(directordaemon::ConfigureCmd(ua, nullptr));
  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "unreferenced-fd"),
            nullptr);

  delete ua;
}

TEST(ConfigureDelete, CmdMissingNameFailsWithUsage)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  FakeConfigureCmd(ua, "configure delete client");

  EXPECT_FALSE(directordaemon::ConfigureCmd(ua, nullptr));
  EXPECT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "unreferenced-fd"),
            nullptr);

  delete ua;
}

TEST(ConfigureDelete, CmdRefusesReferencedResource)
{
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  FakeConfigureCmd(ua, "configure delete client name=referenced-fd");

  EXPECT_FALSE(directordaemon::ConfigureCmd(ua, nullptr));
  EXPECT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "referenced-fd"),
            nullptr);

  delete ua;
}

TEST(ConfigureDelete, CmdRejectsDirectorResource)
{
  // Only one Director resource is allowed and it cannot be deleted; this is
  // checked in ConfigureDelete() itself, before any "name=" lookup, so it is
  // only reachable through ConfigureCmd()/ConfigureDelete(), never through
  // ConfigureDeleteResource() directly.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  FakeConfigureCmd(ua, "configure delete director name=bareos-dir");

  EXPECT_FALSE(directordaemon::ConfigureCmd(ua, nullptr));

  delete ua;
}
