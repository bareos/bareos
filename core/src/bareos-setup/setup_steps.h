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

/** Validate the small set of values accepted from the wizard. */
bool IsSupportedSetupPlatform(const std::string& distro,
                              const std::string& package_manager);
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

// Build the command to download and run add_bareos_repositories.sh.
std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         bool read_curl_config_from_stdin
                                         = false);

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

/** Path of the WebUI proxy configuration created by setup. */
std::string SetupProxyConfigPath();

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
