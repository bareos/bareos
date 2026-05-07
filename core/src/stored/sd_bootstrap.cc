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

#include "stored/sd_bootstrap.h"

#include <cstdio>

#include "include/exit_codes.h"
#include "stored/sd_discovery_probe.h"

namespace storagedaemon {

BootstrapModeOptionHandles AddBootstrapOptions(CLI::App& app,
                                               BootstrapModeOptions& options)
{
  BootstrapModeOptionHandles handles;

  handles.discovery
      = app.add_flag("--discovery", options.enabled,
                     "Run in restricted storage discovery bootstrap mode.");
  handles.bootstrap_url
      = app.add_option("--bootstrap-url", options.bootstrap_url,
                       "Bootstrap service URL for discovery mode.");
  handles.bootstrap_token
      = app.add_option("--bootstrap-token", options.bootstrap_token,
                       "One-time bootstrap token for discovery mode.");
  handles.bootstrap_session
      = app.add_option("--bootstrap-session", options.bootstrap_session,
                       "Bootstrap session identifier for discovery mode.");

  handles.bootstrap_url->type_name("<url>");
  handles.bootstrap_token->type_name("<token>");
  handles.bootstrap_session->type_name("<id>");

  return handles;
}

std::optional<std::string> ValidateBootstrapOptions(
    const BootstrapModeOptions& options)
{
  const bool has_url = !options.bootstrap_url.empty();
  const bool has_token = !options.bootstrap_token.empty();
  const bool has_session = !options.bootstrap_session.empty();
  const bool has_bootstrap_details = has_url || has_token || has_session;

  if (!options.enabled && has_bootstrap_details) {
    return "--bootstrap-url, --bootstrap-token, and --bootstrap-session "
           "require --discovery.";
  }

  if (options.enabled && (!has_url || !has_token || !has_session)) {
    return "--discovery requires --bootstrap-url, --bootstrap-token, and "
           "--bootstrap-session.";
  }

  return std::nullopt;
}

int RunStorageDaemonBootstrap(const BootstrapModeOptions& options)
{
  const auto report = ProbeStorageDiscoveryReport();

  std::fprintf(stderr,
               "bareos-sd: starting restricted discovery bootstrap mode for "
               "session '%s' via %s\n",
               options.bootstrap_session.c_str(),
               options.bootstrap_url.c_str());
  std::fprintf(stderr,
               "bareos-sd: detected %zu filesystem candidate(s) on %s\n",
               report.filesystems.size(),
               report.fqdn.empty() ? "<unknown-host>" : report.fqdn.c_str());
  std::fprintf(stderr,
               "bareos-sd: discovery bootstrap mode is not implemented yet\n");
  return BEXIT_FAILURE;
}

}  // namespace storagedaemon
