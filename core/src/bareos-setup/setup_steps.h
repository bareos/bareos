/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
/**
 * @file
 * Shared command builders for the setup wizard steps.
 * Used by both the WebSocket session handler and the TUI wizard.
 */
#ifndef BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
#define BAREOS_BAREOS_SETUP_SETUP_STEPS_H_

#include <string>
#include <vector>

#include "os_detector.h"

/** Validate the small set of values accepted from the wizard. */
bool IsSupportedSetupPlatform(const std::string& distro,
                              const std::string& package_manager);

/**
 * Check whether the package manager is one the wizard can drive.
 *
 * The distribution ID only selects the repository path; every other setup
 * step is driven by the package manager. A system with an unknown package
 * manager therefore cannot be installed at all, while an unknown
 * distribution can still be handled through a manual repository choice.
 */
bool IsSupportedPackageManager(const std::string& package_manager);

bool IsSafeSetupIdentifier(const std::string& value);

/** Generate a cryptographically random secret for the initial admin account. */
std::string GenerateSetupSecret(size_t length = 32);

/** Remove credentials and bearer tokens from command output. */
std::string RedactSetupSecrets(std::string value,
                               const std::vector<std::string>& secrets);

/**
 * Render a command argv vector as a single, readable display string (e.g.
 * for showing "$ <command>" in the install log before running it). Any
 * argument containing whitespace or shell metacharacters is wrapped in
 * single quotes. This is for display purposes only: the result is never
 * re-parsed or executed.
 */
std::string JoinCommandForDisplay(const std::vector<std::string>& argv);

/** Accept only requests whose Origin header matches the Host header of the
 * same request (i.e. same-origin), regardless of which address/port the
 * wizard is listening on. */
bool IsValidSetupOrigin(const std::string& origin, const std::string& host);

/** Capitalize the first letter (Bareos repo paths use e.g. "Fedora_43"). */
std::string CapFirst(std::string s);

/** Trim leading and trailing whitespace. */
std::string Trim(std::string value);

/** Build the fixed package list used by the setup wizard. */
std::vector<std::string> BuildDefaultPackageList(const std::string& pkg_mgr);

/** Build the package list without tape support packages. */
std::vector<std::string> BuildPackageListWithoutTapeStorage(
    const std::string& pkg_mgr);

/** Build the Bareos package list without the local PostgreSQL server package.
 */
std::vector<std::string> BuildPackageListWithoutPostgresServer(
    const std::string& pkg_mgr);

/**
 * Return the Bareos catalog scripts that must be run manually. Debian/Ubuntu
 * packages initialize the catalog through dbconfig-common during package
 * configuration, so apt does not need the manual scripts.
 */
std::vector<std::string> BuildCatalogInitScripts(const std::string& pkg_mgr);

/**
 * Build the command needed to initialize a fresh PostgreSQL data directory,
 * if the package manager's PostgreSQL package requires one (e.g. Red
 * Hat/SUSE family "postgresql-setup --initdb"). Returns an empty vector when
 * no separate initialization step is required (e.g. Debian/Ubuntu, whose
 * postgresql package initializes a default cluster automatically on
 * install).
 */
std::vector<std::string> BuildPostgresInitCmd();

/**
 * Wrap a Bareos catalog script (create_bareos_database,
 * make_bareos_tables, grant_bareos_privileges, ...) so it runs as the
 * "postgres" OS user via "su postgres -c <script>", matching the
 * official Bareos installation documentation. These scripts must run as
 * the postgres user, not root, since they rely on PostgreSQL peer
 * authentication as that OS user.
 */
std::vector<std::string> BuildRunAsPostgresCmd(const std::string& script);

/** Build the repository OS path segment for the detected distribution. */
std::string BuildRepoOsPath(const std::string& distro,
                            const std::string& version);

/**
 * The repository OS paths published by Bareos that this installer supports.
 *
 * Offered as a manual choice when the distribution is not recognised. This
 * is the single source of truth: it is sent to the web wizard in the state
 * message and printed by the TUI, so the two cannot drift apart.
 */
const std::vector<std::string>& KnownRepoOsPaths();

/**
 * Validate a repository OS path segment.
 *
 * Stricter than IsSafeSetupIdentifier(): that helper accepts "..", which as
 * a repository path would escape the release directory in the constructed
 * download URL.
 */
bool IsValidRepoOsPath(const std::string& value);

/**
 * Suggest repository OS paths for a platform the wizard does not recognise.
 *
 * ID_LIKE identifies the family a derivative is compatible with, but not the
 * version the Bareos repository is named after (Amazon Linux 2023 declares
 * ID_LIKE=fedora with VERSION_ID=2023, Linux Mint 22 declares ID_LIKE=ubuntu
 * with VERSION_ID=22). The result is therefore only ever a suggestion to
 * preselect in the picker; it is never used to install without the user
 * confirming it. The best guess comes first, followed by the remaining paths
 * of the same family.
 */
std::vector<std::string> SuggestRepoOsPaths(const OsInfo& info);

// Build the command to download and run add_bareos_repositories.sh.
std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         bool read_curl_config_from_stdin
                                         = false,
                                         const std::string& release = {});

/** Build the add-repository command for an explicit repository OS path. */
std::vector<std::string> BuildAddRepoCmdForPath(const std::string& repo_os_path,
                                                const std::string& repo_type,
                                                bool read_curl_config_from_stdin
                                                = false,
                                                const std::string& release
                                                = {});

/**
 * Build a probe that checks whether a repository OS path actually exists.
 *
 * Run before adding the repository so that a wrong manual choice fails
 * immediately with a clear message instead of part-way through the install.
 */
std::vector<std::string> BuildRepoPathProbeCmd(const std::string& repo_os_path,
                                               const std::string& repo_type,
                                               bool read_curl_config_from_stdin
                                               = false,
                                               const std::string& release = {});

/** Build the command that retrieves the Subscription release directory index.
 */
std::vector<std::string> BuildSubscriptionReleaseIndexCmd(
    bool read_curl_config_from_stdin = false);

/**
 * Extract the highest numeric release directory version from a Subscription
 * repository index. Returns an empty string when no valid version is found.
 */
std::string ParseLatestSubscriptionRelease(const std::string& index);

/** Whether a repository OS path belongs to the SUSE family. */
bool IsSuseRepoOsPath(const std::string& repo_os_path);

/** Build curl config content containing subscription credentials. */
std::string BuildCurlUserConfig(const std::string& login,
                                const std::string& password);

/**
 * Build a lightweight pre-flight reachability probe for the public community
 * download server. Subscription repositories intentionally return an empty
 * command: their authenticated repository-script download is the only
 * connectivity and credential check, avoiding an unauthenticated 401 probe.
 */
std::vector<std::string> BuildNetworkCheckCmd(const std::string& repo_type);

/** Path of the admin console resource created by setup. */
std::string SetupAdminConfigPath();

/**
 * Configuration files the wizard creates and therefore owns. Setup must
 * never silently overwrite them when they already exist.
 */
std::vector<std::string> SetupOwnedConfigPaths();

/**
 * Build the privileged check that only succeeds when the given path does
 * not exist yet.
 */
std::vector<std::string> BuildFileAbsentCheckCmd(const std::string& path);

/** Build the user-facing error listing pre-existing configuration files. */
std::string BuildExistingSetupConfigError(
    const std::vector<std::string>& existing_paths);

/** Build a zypper command that checks whether the mtx package is available. */
std::vector<std::string> BuildMtxAvailabilityCheckCmd();

/** Return the web server service name used by the distribution packages. */
std::string BuildWebServerServiceName(const std::string& pkg_mgr);

/** Return commands that activate HTTPS for the packaged WebUI web server. */
std::vector<std::vector<std::string>> BuildWebServerHttpsSetupCmds(
    const std::string& pkg_mgr);

/**
 * Build the conditional SELinux setup required by HTTPD to reach the WebUI
 * proxy. The command is a no-op unless SELinux is enforcing.
 */
std::vector<std::string> BuildWebUiSelinuxSetupCmd();

/** Return canonical Bareos daemon unit names for enable/start/status checks. */
std::vector<std::string> BuildBareosDaemonServiceNames(
    const std::string& pkg_mgr);

/**
 * Build the command to refresh package metadata after adding the Bareos
 * repository. Debian/Ubuntu need this before packages from the new repo are
 * installable; repository helpers for other package managers refresh metadata
 * themselves when packages are installed.
 */
std::vector<std::string> BuildPackageCacheUpdateCmd(const std::string& pkg_mgr);

// Build the package install command for the detected package manager.
std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages);

#endif  // BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
