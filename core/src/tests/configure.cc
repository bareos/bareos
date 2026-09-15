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
/**
 * Stand-in for DoReloadConfig(), installed via ConfigureReloadConfig by the
 * fixture below. The real one also flushes and re-checks the catalog,
 * restarts the statistics thread and clears the scheduler queue, none of
 * which exists in a unit test. This keeps the part that "configure delete"
 * actually depends on and that these tests need to observe: parse into a
 * *fresh* LoadedConfiguration, swap it in, keep the previous generation
 * alive, and restore it if the parse fails.
 */
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
    /* Deleting goes through a configuration reload, which in the director is
     * DoReloadConfig(). Substitute the lightweight stand-in below for the
     * duration of the test, and put back whatever was there when the fixture
     * goes out of scope. Restoring matters: a test that installs its own
     * stand-in (see RestoresConfigFileWhenReloadFails) must not leave it
     * behind for whichever test happens to run next. */
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

// Fills in ua->cmd/argc/argk/argv the way bconsole would for a typed
// command, so tests can exercise ConfigureCmd()'s own argument parsing
// (subcommand dispatch, resource-type lookup, "name=" parsing) instead of
// calling ConfigureDeleteResource() directly.
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

/**
 * Swaps the UaContext's output formatter for one that appends to a string,
 * so a test can assert on what the command actually told the user. Needed
 * wherever the on-disk effect alone does not distinguish the branch under
 * test -- the warning about a leftover export file is the whole point of
 * that branch, and skipping past it is otherwise invisible.
 */
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
  /* Holds a configuration generation and a pointer into it the way a
   * running job does (DirectorJcrImpl::used_config_for_job), deletes the
   * resource, and checks the pointer is still usable while the live
   * configuration no longer offers it to anything new. See
   * ConfigureDeleteResource() for why that has to hold.
   *
   * A regression to removing the resource in place fails the EXPECT_EQ
   * below outright, without needing a sanitizer: RemoveResource() unlinks
   * it from the chain that GetResWithName() walks, so the lookup returns
   * nullptr. The EXPECT_STREQ additionally catches the use-after-free, but
   * only under AddressSanitizer. */
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
  /* The delete is enacted by a reload, and that reload can fail -- most
   * obviously when the configuration that remains does not parse. The
   * resource file then has to come back exactly as it was, so that the
   * on-disk configuration still matches the one the director kept running
   * with.
   *
   * "Exactly as it was" includes the file mode: the file is moved aside and
   * moved back rather than read and rewritten, so a hand-set mode survives.
   * The mode is deliberately changed to something other than the 0640 that
   * configure_write_resource() would produce, so that a regression back to
   * rewriting the contents fails here. */
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

  /* The three directory levels that existed only to hold this client's
   * export go with it: .../<client>/bareos-fd.d/director, .../bareos-fd.d
   * and .../<client>. The shared parent must survive. */
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
  /* The export directory can hold an export written under a previous
   * Director resource name, whose path this Director can no longer compute.
   * That file holds a plaintext copy of a Director password, so the pruning
   * must stop rather than take it with the rest of the tree, and the user
   * must be told it is there.
   *
   * The warning is the part that has to be asserted: stopping the prune
   * happens either way, so the on-disk state alone does not distinguish
   * warning from silently walking away. */
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
  /* A Job that takes its Client from a JobDefs ends up holding the same
   * pointer, but its own file has no Client directive. Naming the Job would
   * send whoever has to remove the reference to a file where there is
   * nothing to remove; the JobDefs, which does hold the directive, is what
   * has to be reported. */
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
  /* Deleting removes a whole configuration file, and only "configure add"
   * keeps to one resource per file -- a hand-written file can define
   * several. Deleting one of them takes the others with it, and since
   * nothing references them the reference check does not object and the
   * reload succeeds. Whether that is reported is the difference between a
   * noticed and a silent configuration loss, so it is what is asserted
   * here, along with the stashed file being kept so the definitions that
   * went with it can be recovered. */
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
  /* A Client that goes with the file is as gone from the configuration as
   * the one that was named, so its export -- which holds a plaintext copy
   * of the Director password -- must go too. Leaving it behind is the
   * CWE-459 case the export removal exists for in the first place. */
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
  /* The counterpart to the test above: deleting a resource that really does
   * have its own file must not leave a ".deleted" copy of it behind -- that
   * copy would otherwise accumulate, and for a Client it holds a password.
   */
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

TEST(ConfigureDelete, CmdAcceptsNameAsResourceTypeValue)
{
  /* "configure add" takes the name either way, so deleting has to as well:
   * "configure delete client=foo" is the same request as
   * "configure delete client name=foo". */
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
