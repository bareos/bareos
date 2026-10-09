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
#include "dird/ua_output.h"
#include "include/jcr.h"
#include "dird/ua_configure.cc"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
// Stand-in for DoReloadConfig() (installed via ConfigureReloadConfig by the
// fixture below): just the configuration swap/rollback, without the
// catalog/statistics/scheduler side effects a unit test can't provide.
bool TestReloadConfig()
{
  using namespace directordaemon;

  ResLocker _{my_config};

  auto backup = my_config->BackupCurrentConfiguration();

  my_config->err_type_ = M_ERROR;
  my_config->ClearWarnings();

  bool ok = my_config->ParseConfig();
  if (ok) {
    backup->SetNext(my_config->GetCurrentConfiguration());
  } else {
    my_config->RestoreConfiguration(std::move(backup));
  }

  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;

  return ok;
}

// Copies the configure_delete fixture into a fresh temporary directory, so
// that each test gets its own on-disk copy and can safely unlink files
// without affecting other tests or later runs of the same test.
class TempConfigureDeleteConfig {
 public:
  TempConfigureDeleteConfig()
  {
    // Restored on destruction so a test that installs its own stand-in
    // (see RestoresConfigFileWhenReloadFails) doesn't leak it to the next one.
    saved_reload_config_ = directordaemon::ConfigureReloadConfig;
    directordaemon::ConfigureReloadConfig = TestReloadConfig;

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
    directordaemon::ConfigureReloadConfig = saved_reload_config_;

    if (!path_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(path_, ec);
    }
  }

  TempConfigureDeleteConfig(const TempConfigureDeleteConfig&) = delete;
  TempConfigureDeleteConfig& operator=(const TempConfigureDeleteConfig&)
      = delete;

  const std::string& path() const { return path_; }

 private:
  std::string path_;
  bool (*saved_reload_config_)() = nullptr;
};

// Fills in ua->cmd/argc/argk/argv the way bconsole would, so tests can
// exercise ConfigureCmd()'s own argument parsing.
void FakeConfigureCmd(directordaemon::UaContext* ua, const std::string& cmd)
{
  PmStrcpy(ua->cmd, cmd.c_str());
  ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}

std::string ReadWholeFile(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

bool CaptureSend(void* ctx, const char* fmt, ...)
{
  PoolMem msg;
  va_list ap;
  va_start(ap, fmt);
  msg.Bvsprintf(fmt, ap);
  va_end(ap);
  static_cast<std::string*>(ctx)->append(msg.c_str());
  return true;
}

// Swaps in an output formatter that appends to a string, so a test can
// assert on console output where the on-disk effect alone isn't enough.
class CapturedConsole {
 public:
  explicit CapturedConsole(directordaemon::UaContext* ua) : ua_{ua}
  {
    ua_->send = std::make_unique<OutputFormatter>(CaptureSend, &text_,
                                                  directordaemon::filterit, ua);
  }

  // The formatter buffers; flush before inspecting.
  const std::string& text()
  {
    ua_->send->SendBuffer();
    return text_;
  }

  CapturedConsole(const CapturedConsole&) = delete;
  CapturedConsole& operator=(const CapturedConsole&) = delete;

 private:
  directordaemon::UaContext* ua_;
  std::string text_;
};
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

TEST(ConfigureDelete, KeepsDeletedResourceAliveForRunningJob)
{
  // Holds a resource pointer the way a running job does
  // (DirectorJcrImpl::used_config_for_job); a regression to removing it in
  // place fails EXPECT_EQ outright, and EXPECT_STREQ catches the
  // use-after-free under AddressSanitizer.
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

  // What a running job keeps hold of.
  std::shared_ptr<LoadedConfiguration> used_config_for_job
      = directordaemon::my_config->GetCurrentConfiguration();
  BareosResource* client_in_use = directordaemon::my_config->GetResWithName(
      directordaemon::R_CLIENT, "unreferenced-fd");
  ASSERT_NE(client_in_use, nullptr);

  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "unreferenced-fd"));

  // Gone from the configuration that new jobs will be started from ...
  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "unreferenced-fd"),
            nullptr);

  /* ... but the generation the job was started with still has it, and the
   * pointer the job holds is still valid. */
  EXPECT_EQ(used_config_for_job->GetResWithName(directordaemon::R_CLIENT,
                                                "unreferenced-fd"),
            client_in_use);
  EXPECT_STREQ(client_in_use->resource_name_, "unreferenced-fd");

  delete ua;
}

TEST(ConfigureDelete, RestoresConfigFileWhenReloadFails)
{
  // A failed reload must restore the resource file exactly, mode included --
  // moved aside and back rather than read and rewritten. The mode is set to
  // something other than configure_write_resource()'s 0640, so a regression
  // to rewriting the contents fails here.
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

  using std::filesystem::perms;
  constexpr perms owner_only = perms::owner_read | perms::owner_write;
  std::filesystem::permissions(path.c_str(), owner_only);

  const std::string content_before = ReadWholeFile(path.c_str());
  const std::filesystem::perms perms_before
      = std::filesystem::status(path.c_str()).permissions();
  ASSERT_FALSE(content_before.empty());

  static int reload_calls;
  reload_calls = 0;
  directordaemon::ConfigureReloadConfig = [] {
    reload_calls++;
    return false;
  };

  EXPECT_FALSE(directordaemon::ConfigureDeleteResource(ua, res_table,
                                                       "unreferenced-fd"));

  /* The file really was moved aside and the reload really was attempted --
   * without this the checks below would also pass if the delete had bailed
   * out before touching anything. */
  EXPECT_EQ(reload_calls, 1);

  // The resource is still there, in the configuration and on disk ...
  EXPECT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "unreferenced-fd"),
            nullptr);
  ASSERT_TRUE(std::filesystem::exists(path.c_str()));
  EXPECT_EQ(ReadWholeFile(path.c_str()), content_before);
  EXPECT_EQ(std::filesystem::status(path.c_str()).permissions(), perms_before);

  // ... and the rollback did not leave the stashed copy behind.
  EXPECT_FALSE(std::filesystem::exists(std::string(path.c_str()) + ".deleted"));

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

TEST(ConfigureDelete, GetPathOfResourceRejectsTraversalInName)
{
  // IsNameValid() permits '.' and '/' in a resource name; a name that
  // embeds ".." must not let GetPathOfResource() escape config_dir_.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  ASSERT_TRUE(client_config);

  PoolMem path(PM_FNAME);
  EXPECT_FALSE(directordaemon::my_config->GetPathOfResource(
      path, NULL, "client", "../../../../../../../../etc/cron.d/evil",
      false));
}

TEST(ConfigureDelete, RefusesToDeleteResourceNameAliasingAnotherResource)
{
  // "a/../unreferenced-fd" is a distinct loaded resource (its own literal
  // name), but as the last component of config_include_naming_format_ it
  // resolves to the same file as the real "unreferenced-fd" -- still inside
  // config_dir_, so GetPathOfResource()'s containment check alone would not
  // catch it. ConfigureDeleteResource() must refuse a name containing '/'
  // before ever computing that path.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  ASSERT_TRUE(client_config);

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  ASSERT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                       "a/../unreferenced-fd"),
           nullptr);

  EXPECT_FALSE(
      ConfigureDeleteResource(ua, res_table, "a/../unreferenced-fd"));

  // The aliased, real resource and its file must be untouched.
  EXPECT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                       "unreferenced-fd"),
           nullptr);
  PoolMem path(PM_FNAME);
  ASSERT_TRUE(directordaemon::my_config->GetPathOfResource(
      path, NULL, res_table->name, "unreferenced-fd", false));
  EXPECT_TRUE(std::filesystem::exists(path.c_str()));

  delete ua;
}

TEST(ConfigureDelete, GetFdExportBasedirRejectsDotAndDotDot)
{
  // clientname sits as its own path segment here, so a bare ".." isn't
  // caught by GetPathOfResource()'s containment check (it never leaves
  // config_dir_); GetFdExportBasedir() must refuse it directly.
  PoolMem basedir(PM_FNAME);
  EXPECT_FALSE(directordaemon::GetFdExportBasedir(basedir, ".."));
  EXPECT_FALSE(directordaemon::GetFdExportBasedir(basedir, "."));

  EXPECT_TRUE(directordaemon::GetFdExportBasedir(basedir, "some-client"));
  EXPECT_STREQ(basedir.c_str(),
              "bareos-dir-export/client/some-client/bareos-fd.d");
}

TEST(ConfigureDelete, GetFdExportBasedirRejectsEmbeddedSlash)
{
  // "a/../victim" stays inside config_dir_ (containment check passes) but
  // aliases the real "victim" client's export directory. clientname must be
  // rejected outright, not just checked for escaping.
  PoolMem basedir(PM_FNAME);
  EXPECT_FALSE(directordaemon::GetFdExportBasedir(basedir, "a/../victim"));
}

TEST(ConfigureDelete, GetPathOfResourceRejectsTraversalInComponent)
{
  // GetFdExportBasedir() now rejects any '/' in clientname outright, so a
  // component built from it can no longer carry an escape in practice, but
  // GetPathOfResource() must still refuse one on its own component
  // argument, as a caller-independent backstop.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  ASSERT_TRUE(client_config);

  PoolMem path(PM_FNAME);
  EXPECT_FALSE(directordaemon::my_config->GetPathOfResource(
      path, "bareos-dir-export/client/../../../../../../../../etc/cron.d",
      "director", "bareos-dir", false));
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
  ASSERT_NE(references[0].item, nullptr);
  EXPECT_STREQ(references[0].item->name, "Client");

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
  // Deleting a client must also remove the FD export file "configure add
  // client" writes (see ConfigureCreateFdResource()), not leave it behind.
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

  // All three levels that existed only for this client's export go too;
  // the shared parent must survive.
  const std::filesystem::path director_dir
      = std::filesystem::path{export_path.c_str()}.parent_path();
  const std::filesystem::path fd_dir = director_dir.parent_path();
  const std::filesystem::path client_dir = fd_dir.parent_path();

  EXPECT_FALSE(std::filesystem::exists(director_dir));
  EXPECT_FALSE(std::filesystem::exists(fd_dir));
  EXPECT_FALSE(std::filesystem::exists(client_dir));
  EXPECT_TRUE(std::filesystem::exists(client_dir.parent_path()));

  delete ua;
}

TEST(ConfigureDelete, KeepsForeignFileInFiledaemonExportDirectory)
{
  // A stale export left under a previous Director name (plaintext password)
  // must stop the prune and warn -- the warning is what needs asserting,
  // since the on-disk state alone doesn't distinguish that from silently
  // walking away.
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

  // An export left behind under a Director resource name we no longer use.
  const std::filesystem::path director_dir
      = std::filesystem::path{export_path.c_str()}.parent_path();
  const std::filesystem::path stale = director_dir / "old-dir-name.conf";
  std::ofstream{stale} << "Director {\n  Name = old\n}\n";
  ASSERT_TRUE(std::filesystem::exists(stale));

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  CapturedConsole console(ua);
  EXPECT_TRUE(directordaemon::ConfigureDeleteResource(ua, res_table,
                                                      "unreferenced-fd"));

  // Our own export is gone ...
  EXPECT_FALSE(std::filesystem::exists(export_path.c_str()));
  // ... but the one we cannot account for, and its directory, are intact.
  EXPECT_TRUE(std::filesystem::exists(stale));
  EXPECT_TRUE(std::filesystem::exists(director_dir));

  // ... and the user was told, rather than left with a stray password file.
  EXPECT_NE(console.text().find("is not empty"), std::string::npos)
      << "expected a warning about the leftover export, got:\n"
      << console.text();

  delete ua;
}

TEST(ConfigureDelete, NamesTheJobdefsThatHoldsAnInheritedReference)
{
  // A Job inheriting Client from a JobDefs has no Client directive of its
  // own to remove; the JobDefs that holds it must be reported instead.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());

  const std::filesystem::path config_dir
      = std::filesystem::path{config.path()} / "bareos-dir.d";
  std::filesystem::create_directory(config_dir / "jobdefs");
  std::ofstream{config_dir / "jobdefs" / "inheriting-defs.conf"}
      << "JobDefs {\n"
         "  Name = inheriting-defs\n"
         "  Type = Backup\n"
         "  Level = Full\n"
         "  Client = unreferenced-fd\n"
         "  Messages = standard\n"
         "  Pool = default\n"
         "}\n";
  std::ofstream{config_dir / "job" / "inheriting-job.conf"}
      << "Job {\n"
         "  Name = inheriting-job\n"
         "  JobDefs = inheriting-defs\n"
         "}\n";

  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  /* CheckResources() -> PopulateJobdefaults() does this in the running
   * director; the test harness only parses. */
  directordaemon::JobResource* inheriting_job
      = (directordaemon::JobResource*)directordaemon::my_config->GetResWithName(
          directordaemon::R_JOB, "inheriting-job");
  ASSERT_NE(inheriting_job, nullptr);
  ASSERT_TRUE(directordaemon::PropagateJobdefs(directordaemon::R_JOB,
                                               inheriting_job));

  BareosResource* client = directordaemon::my_config->GetResWithName(
      directordaemon::R_CLIENT, "unreferenced-fd");
  ASSERT_NE(client, nullptr);
  // The Job really does point at the Client, inherited or not.
  ASSERT_EQ((BareosResource*)inheriting_job->client, client);

  std::vector<ResourceReference> references
      = directordaemon::my_config->FindResourceReferences(
          directordaemon::R_CLIENT, client);

  ASSERT_EQ(references.size(), 1u);
  EXPECT_EQ(references[0].rcode, directordaemon::R_JOBDEFS);
  EXPECT_STREQ(references[0].resource_name.c_str(), "inheriting-defs");
  EXPECT_STREQ(references[0].directive_name.c_str(), "Client");
}

TEST(ConfigureDelete, ReportsResourcesRemovedAsCollateral)
{
  // A hand-written file can define more than one resource, unlike
  // "configure add"'s one-per-file convention; deleting one takes the
  // others with it, and that must be reported, not lost silently.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());

  const std::filesystem::path shared_file
      = std::filesystem::path{config.path()} / "bareos-dir.d" / "client"
        / "collateral-fd.conf";
  std::ofstream{shared_file} << "Client {\n"
                                "  Name = collateral-fd\n"
                                "  Address = localhost\n"
                                "  Password = \"secret\"\n"
                                "}\n"
                                "\n"
                                "Client {\n"
                                "  Name = tagalong-fd\n"
                                "  Address = localhost\n"
                                "  Password = \"secret\"\n"
                                "}\n";

  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  // Both are loaded, and neither is referenced by anything.
  ASSERT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "collateral-fd"),
            nullptr);
  ASSERT_NE(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "tagalong-fd"),
            nullptr);

  CapturedConsole console(ua);
  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "collateral-fd"));

  // The delete went through, and took the other resource in the file too.
  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "collateral-fd"),
            nullptr);
  EXPECT_EQ(directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                      "tagalong-fd"),
            nullptr);
  EXPECT_FALSE(std::filesystem::exists(shared_file));

  // The user was told which resource went with it ...
  EXPECT_NE(console.text().find("tagalong-fd"), std::string::npos)
      << "expected the collaterally removed resource to be named, got:\n"
      << console.text();

  /* ... and the file was kept rather than unlinked, so its definitions are
   * still recoverable. */
  const std::filesystem::path stashed
      = shared_file.string() + std::string{".deleted"};
  ASSERT_TRUE(std::filesystem::exists(stashed));
  std::ifstream stashed_stream{stashed};
  const std::string stashed_content{
      std::istreambuf_iterator<char>{stashed_stream},
      std::istreambuf_iterator<char>{}};
  EXPECT_NE(stashed_content.find("tagalong-fd"), std::string::npos);
  EXPECT_NE(console.text().find(stashed.string()), std::string::npos)
      << "expected the kept file to be named, got:\n"
      << console.text();

  delete ua;
}

TEST(ConfigureDelete, RemovesFiledaemonExportOfCollaterallyRemovedClient)
{
  // A client collaterally removed is as gone as the one that was named, so
  // its export (plaintext Director password, CWE-459) must go too.
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());

  const std::filesystem::path shared_file
      = std::filesystem::path{config.path()} / "bareos-dir.d" / "client"
        / "collateral-fd.conf";
  std::ofstream{shared_file} << "Client {\n"
                                "  Name = collateral-fd\n"
                                "  Address = localhost\n"
                                "  Password = \"secret\"\n"
                                "}\n"
                                "\n"
                                "Client {\n"
                                "  Name = tagalong-fd\n"
                                "  Address = localhost\n"
                                "  Password = \"secret\"\n"
                                "}\n";

  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);

  // Both clients were added the way "configure add client" would.
  ASSERT_TRUE(directordaemon::ConfigureCreateFdResource(ua, "collateral-fd"));
  ASSERT_TRUE(directordaemon::ConfigureCreateFdResource(ua, "tagalong-fd"));

  const char* dirname = directordaemon::me->resource_name_;
  auto export_path_of = [&](const char* clientname) {
    PoolMem basedir(PM_FNAME);
    basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
    PoolMem export_path(PM_FNAME);
    EXPECT_TRUE(directordaemon::my_config->GetPathOfResource(
        export_path, basedir.c_str(), "director", dirname, false));
    return std::filesystem::path{export_path.c_str()};
  };

  const std::filesystem::path target_export = export_path_of("collateral-fd");
  const std::filesystem::path tagalong_export = export_path_of("tagalong-fd");
  ASSERT_TRUE(std::filesystem::exists(target_export));
  ASSERT_TRUE(std::filesystem::exists(tagalong_export));

  const ResourceTable* res_table
      = directordaemon::my_config->GetResourceTable("client");
  ASSERT_NE(res_table, nullptr);

  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "collateral-fd"));

  // Neither client is configured any longer, so neither export may remain.
  EXPECT_FALSE(std::filesystem::exists(target_export));
  EXPECT_FALSE(std::filesystem::exists(tagalong_export));
  EXPECT_FALSE(std::filesystem::exists(tagalong_export.parent_path()));

  delete ua;
}

TEST(ConfigureDelete, RemovesStashedFileWhenNothingElseWasLost)
{
  // Counterpart to the test above: nothing else lost means no ".deleted"
  // copy should be left behind either.
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

  EXPECT_TRUE(ConfigureDeleteResource(ua, res_table, "unreferenced-fd"));

  EXPECT_FALSE(std::filesystem::exists(path.c_str()));
  EXPECT_FALSE(std::filesystem::exists(std::string{path.c_str()} + ".deleted"));

  delete ua;
}

// The tests below go through ConfigureCmd() rather than
// ConfigureDeleteResource() directly, to also cover its argument parsing
// and "configure" output wrapping.

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

TEST(ConfigureDelete, CmdAcceptsNameAsResourceTypeValue)
{
  // "configure delete client=foo" must be the same request as
  // "configure delete client name=foo".
  InitDirGlobals();
  TempConfigureDeleteConfig config;
  ASSERT_FALSE(config.path().empty());
  PConfigParser client_config(DirectorPrepareResources(config.path()));
  if (!client_config) { return; }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua = new directordaemon::UaContext(&jcr);
  FakeConfigureCmd(ua, "configure delete client=unreferenced-fd");

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
  // Checked in ConfigureDelete() itself, so only reachable through
  // ConfigureCmd(), never through ConfigureDeleteResource() directly.
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
