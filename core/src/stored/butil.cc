/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Utility routines for "tool" programs such as bscan, bls,
 * bextract, ...  Some routines also used by Bareos.
 *
 * Normally nothing in this file is called by the Storage
 * daemon because we interact more directly with the user
 * i.e. printf, ...
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/bsr.h"
#include "stored/sd_backends.h"
#include "stored/stored_conf.h"
#include "stored/stored_jcr_impl.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"

#include <algorithm>
#include <cstdio>
#include <limits.h>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <vector>
#if defined(HAVE_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#endif

extern char* exepath;

namespace storagedaemon {

namespace {

struct DeviceResolutionResult {
  DeviceResource* device_resource{nullptr};
  std::string detail;
  std::string inferred_volume_name;
};

static std::vector<std::unique_ptr<DeviceResource>> synthetic_device_resources;

static bool DeviceSupportsAccess(const DeviceResource* device_resource,
                                 bool readonly)
{
  switch (device_resource->access_mode) {
    case IODirection::READ_WRITE:
      return true;
    case IODirection::READ:
      return readonly;
    case IODirection::WRITE:
      return !readonly;
    case IODirection::NONE:
    default:
      return false;
  }
}

static std::string StripQuotes(std::string_view resource_name)
{
  if (resource_name.size() >= 2 && resource_name.front() == '"'
      && resource_name.back() == '"') {
    resource_name.remove_prefix(1);
    resource_name.remove_suffix(1);
  }

  return std::string(resource_name);
}

static const char* AccessDescription(bool readonly)
{
  return readonly ? "reading" : "writing";
}

static bool TryStatPath(const std::string& path, struct stat& statp)
{
  return !path.empty() && stat(path.c_str(), &statp) == 0;
}

static std::string DirectoryName(const std::string& path)
{
  // Return the parent path component. A trailing separator is ignored, and the
  // root path stays rooted instead of becoming empty.
  auto separator = path.find_last_of("/\\");
  if (separator == std::string::npos) { return "."; }
  if (separator == 0) { return path.substr(0, 1); }
  return path.substr(0, separator);
}

static std::string BaseName(const std::string& path)
{
  // Return the final path component. If the input ends in a separator or is
  // the root path, the basename is empty.
  auto separator = path.find_last_of("/\\");
  if (separator == std::string::npos) { return path; }
  return path.substr(separator + 1);
}

static std::string GetExecutableDirectory()
{
#if defined(HAVE_WIN32)
  char path[MAX_PATH];
  auto len = GetModuleFileNameA(nullptr, path, sizeof(path));
  if (len > 0 && len < sizeof(path)) {
    return DirectoryName(std::string(path, len));
  }
#elif defined(HAVE_LINUX_OS)
  char path[PATH_MAX];
  auto len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len > 0) {
    path[len] = '\0';
    return DirectoryName(path);
  }
#endif

  // Non-Linux platforms use the path captured during startup by MyNameIs().
  if (exepath && exepath[0]) { return exepath; }
  return {};
}

static bool EnsureFileBackendLoaded()
{
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  const std::string device_type{DeviceType::B_FILE_DEV};
  if (ImplementationFactory<Device>::IsRegistered(device_type)) { return true; }

  std::vector<std::string> backend_directories{PATH_BAREOS_BACKENDDIR};
  auto executable_directory = GetExecutableDirectory();
  if (!executable_directory.empty()) {
    std::string build_tree_backend_dir{std::move(executable_directory)};
    if (!IsPathSeparator(build_tree_backend_dir.back())) {
      build_tree_backend_dir += PathSeparator;
    }
    build_tree_backend_dir += "backends";
    backend_directories.emplace_back(std::move(build_tree_backend_dir));
  }

  return LoadStorageBackend(device_type, backend_directories);
#else
  return true;
#endif
}

static DeviceResolutionResult ResolveDirectLocalVolumePath(std::string path)
{
  DeviceResolutionResult result;
  struct stat statp;
  if (!TryStatPath(path, statp)
      || (!S_ISREG(statp.st_mode) && !S_ISDIR(statp.st_mode))) {
    return result;
  }

  if (!EnsureFileBackendLoaded()) {
    result.detail = T_(
        "Cannot load the file storage backend needed for direct "
        "local volume access.\n");
    return result;
  }

  auto device_resource = std::make_unique<DeviceResource>();
  auto display_path = path;
  if (S_ISREG(statp.st_mode)) {
    result.inferred_volume_name = BaseName(path);
    path = DirectoryName(path);
  }

  device_resource->resource_name_ = strdup(display_path.c_str());
  device_resource->archive_device_string = strdup(path.c_str());
  device_resource->media_type = strdup("");
  device_resource->device_type = DeviceType::B_FILE_DEV;

  result.device_resource = device_resource.get();
  synthetic_device_resources.emplace_back(std::move(device_resource));

  return result;
}

static std::string FormatDeviceChoice(const DeviceResource* device_resource)
{
  return "\"" + std::string(device_resource->resource_name_) + "\" ("
         + device_resource->archive_device_string + ")";
}

static std::string FormatAutochangerSelectionPrompt(
    const AutochangerResource* autochanger_resource,
    const std::vector<DeviceResource*>& devices)
{
  std::string prompt = T_("Autochanger \"");
  prompt += autochanger_resource->resource_name_;
  prompt += T_("\" has multiple suitable devices. Choose one:\n");

  for (std::size_t i = 0; i < devices.size(); ++i) {
    prompt += "  ";
    prompt += std::to_string(i + 1);
    prompt += ") ";
    prompt += FormatDeviceChoice(devices[i]);
    if (devices[i]->autoselect) { prompt += " [autoselect]"; }
    prompt += '\n';
  }

  prompt += T_("Enter selection number: ");
  return prompt;
}

static std::string FormatAutochangerCandidates(
    const AutochangerResource* autochanger_resource,
    const std::vector<DeviceResource*>& devices,
    bool readonly)
{
  std::string detail = T_("Autochanger \"");
  detail += autochanger_resource->resource_name_;
  detail += T_("\" has multiple devices suitable for ");
  detail += AccessDescription(readonly);
  detail += T_(". Please specify one of these Device resources explicitly:\n");

  for (const auto* device_resource : devices) {
    detail += "  ";
    detail += FormatDeviceChoice(device_resource);
    if (device_resource->autoselect) { detail += " [autoselect]"; }
    detail += '\n';
  }

  return detail;
}

static DeviceResource* PromptForAutochangerDevice(
    const AutochangerResource* autochanger_resource,
    const std::vector<DeviceResource*>& devices)
{
  if (!isatty(fileno(stdin))) { return nullptr; }

  while (true) {
    auto prompt
        = FormatAutochangerSelectionPrompt(autochanger_resource, devices);
    if (fputs(prompt.c_str(), stderr) < 0) { return nullptr; }
    fflush(stderr);

    char buffer[64];
    if (!fgets(buffer, sizeof(buffer), stdin)) { return nullptr; }

    char* end = nullptr;
    errno = 0;
    auto selection = strtol(buffer, &end, 10);
    if (errno == 0 && end != buffer && selection >= 1
        && static_cast<std::size_t>(selection) <= devices.size()) {
      return devices[selection - 1];
    }

    if (fputs(T_("Invalid selection.\n"), stderr) < 0) { return nullptr; }
  }
}

static DeviceResolutionResult ResolveAutochangerDevice(
    const AutochangerResource* autochanger_resource,
    bool readonly)
{
  DeviceResolutionResult result;
  std::vector<DeviceResource*> eligible_devices;
  std::vector<DeviceResource*> eligible_autoselect_devices;

  DeviceResource* device_resource = nullptr;
  int i;
  foreach_alist_index (i, device_resource,
                       autochanger_resource->device_resources) {
    if (!DeviceSupportsAccess(device_resource, readonly)) { continue; }

    eligible_devices.emplace_back(device_resource);
    if (device_resource->autoselect) {
      eligible_autoselect_devices.emplace_back(device_resource);
    }
  }

  if (eligible_autoselect_devices.size() == 1) {
    result.device_resource = eligible_autoselect_devices.front();
    return result;
  }

  if (eligible_devices.size() == 1) {
    result.device_resource = eligible_devices.front();
    return result;
  }

  if (eligible_devices.empty()) {
    result.detail = T_("Autochanger \"");
    result.detail += autochanger_resource->resource_name_;
    result.detail += T_("\" has no devices suitable for ");
    result.detail += AccessDescription(readonly);
    result.detail += ".\n";
    return result;
  }

  result.device_resource
      = PromptForAutochangerDevice(autochanger_resource, eligible_devices);
  if (!result.device_resource) {
    result.detail = FormatAutochangerCandidates(autochanger_resource,
                                                eligible_devices, readonly);
  }

  return result;
}

}  // namespace

/* Forward referenced functions */
static bool setup_to_access_device(DeviceControlRecord* dcr,
                                   JobControlRecord* jcr,
                                   char* dev_name,
                                   const std::string& VolumeName,
                                   bool readonly);
static DeviceResolutionResult find_device_res(char* archive_device_string,
                                              bool readonly);
static void MyFreeJcr(JobControlRecord* jcr);

bool IsLocalFilesystemVolumePath(const char* device_name)
{
  if (!device_name) { return false; }

  auto normalized_path = StripQuotes(device_name);
  struct stat statp;
  return TryStatPath(normalized_path, statp)
      && (S_ISREG(statp.st_mode) || S_ISDIR(statp.st_mode));
}

void LoadSdConfigForDeviceListingIfAvailable(const char* config_path)
{
  if (my_config || !config_path || config_path[0] == '\0') { return; }

  struct stat statp;
  if (stat(config_path, &statp) != 0) { return; }

  my_config = InitSdConfig(config_path, M_CONFIG_ERROR);
  ParseSdConfig(config_path, M_CONFIG_ERROR);
}


JobControlRecord* SetupDummyJcr(const char* name,
                                BootStrapRecord* bsr,
                                DirectorResource* director)
{
  JobControlRecord* jcr = new_jcr(&MyFreeJcr);
  jcr->sd_impl = new StoredJcrImpl;
  register_jcr(jcr);

  jcr->sd_impl->read_session.bsr = bsr;
  jcr->sd_impl->director = director;
  jcr->VolSessionId = 1;
  jcr->VolSessionTime = (uint32_t)time(NULL);
  jcr->sd_impl->NumReadVolumes = 0;
  jcr->sd_impl->NumWriteVolumes = 0;
  jcr->JobId = 0;
  jcr->setJobType(JT_CONSOLE);
  jcr->setJobLevel(L_FULL);
  jcr->setJobStatus(JS_Terminated);
  jcr->where = strdup("");
  jcr->sd_impl->job_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->job_name, "Dummy.Job.Name");
  jcr->client_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->client_name, "Dummy.Client.Name");
  bstrncpy(jcr->Job, name, sizeof(jcr->Job));
  jcr->sd_impl->fileset_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->fileset_name, "Dummy.fileset.name");
  jcr->sd_impl->fileset_md5 = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->fileset_md5, "Dummy.fileset.md5");

  NewPlugins(jcr); /* instantiate plugins */

  return jcr;
}


/**
 * Setup a "daemon" JobControlRecord for the various standalone tools (e.g. bls,
 * bextract, bscan, ...)
 */
JobControlRecord* SetupJcr(const char* name,
                           char* dev_name,
                           BootStrapRecord* bsr,
                           DirectorResource* director,
                           DeviceControlRecord* dcr,
                           const std::string& VolumeName,
                           bool readonly)
{
  JobControlRecord* jcr = SetupDummyJcr(name, bsr, director);

  if (my_config) { InitAutochangers(); }
  CreateVolumeLists();

  if (!setup_to_access_device(dcr, jcr, dev_name, VolumeName, readonly)) {
    return NULL;
  }

  if (!bsr && !VolumeName.empty()) {
    bstrncpy(dcr->VolumeName, VolumeName.c_str(), sizeof(dcr->VolumeName));
  }

  bstrncpy(dcr->pool_name, "Default", sizeof(dcr->pool_name));
  bstrncpy(dcr->pool_type, "Backup", sizeof(dcr->pool_type));

  return jcr;
}

static std::string ListDevicesInConfig(const ConfigurationParser* config)
{
  std::vector<std::string> devices;
  for (BareosResource* resource = nullptr;
       (resource = config->GetNextRes(R_DEVICE, resource));) {
    DeviceResource* device = dynamic_cast<DeviceResource*>(resource);
    std::string device_str;
    device_str += " \"";
    device_str += device->resource_name_;
    device_str += "\" (";
    device_str += device->archive_device_string;
    device_str += ")\n";
    devices.emplace_back(std::move(device_str));
  }

  std::vector<std::string> autochangers;
  for (BareosResource* resource = nullptr;
       (resource = config->GetNextRes(R_AUTOCHANGER, resource));) {
    auto* autochanger = dynamic_cast<AutochangerResource*>(resource);
    std::string autochanger_str;
    autochanger_str += " \"";
    autochanger_str += autochanger->resource_name_;
    autochanger_str += "\"";

    std::vector<std::string> members;
    for (auto* device : autochanger->device_resources) {
      members.emplace_back(FormatDeviceChoice(device));
    }
    std::sort(members.begin(), members.end());

    if (!members.empty()) {
      autochanger_str += " ->";
      for (const auto& member : members) {
        autochanger_str += "\n    ";
        autochanger_str += member;
      }
    }
    autochanger_str += '\n';
    autochangers.emplace_back(std::move(autochanger_str));
  }

  std::sort(devices.begin(), devices.end());
  std::sort(autochangers.begin(), autochangers.end());
  std::string devices_str = "Available Devices:\n";
  for (const std::string& device : devices) { devices_str += device; }
  if (!autochangers.empty()) {
    devices_str += "Available Autochangers:\n";
    for (const std::string& autochanger : autochangers) {
      devices_str += autochanger;
    }
  }
  return devices_str;
}

std::string AvailableDevicesListing()
{
  if (!my_config) {
    return T_(
        "Direct local file volume paths may be used without an SD "
        "configuration.\n");
  }

  return ListDevicesInConfig(my_config);
}
/**
 * Setup device, jcr, and prepare to access device.
 *   If the caller wants read access, acquire the device, otherwise,
 *     the caller will do it.
 */
static bool setup_to_access_device(DeviceControlRecord* dcr,
                                   JobControlRecord* jcr,
                                   char* dev_name,
                                   const std::string& VolumeName,
                                   bool readonly)
{
  Device* dev;
  DeviceResource* device_resource;
  char VolName[MAX_NAME_LENGTH];

  InitReservationsLock();

  /* If no volume name already given and no bsr, and it is a file,
   * try getting name from Filename */
  if (!VolumeName.empty()) {
    bstrncpy(VolName, VolumeName.c_str(), sizeof(VolName));
    if (VolumeName.size() >= MAX_NAME_LENGTH) {
      /* We do not handle this case gracefully, so its best to just
       * abort the job here. */
      Jmsg0(jcr, M_FATAL, 0,
            T_("Volume name or names is too long. Please use a .bsr file.\n"));
      return false;
    }
  } else {
    VolName[0] = 0;
  }

  auto resolved_device = find_device_res(dev_name, readonly);
  device_resource = resolved_device.device_resource;
  if (device_resource == NULL) {
    if (!resolved_device.detail.empty()) {
      if (configfile) {
        Jmsg3(jcr, M_FATAL, 0,
              T_("Cannot use device \"%s\" in config file %s.\n%s"), dev_name,
              configfile, resolved_device.detail.c_str());
      } else {
        Jmsg2(jcr, M_FATAL, 0, T_("Cannot use device \"%s\".\n%s"), dev_name,
              resolved_device.detail.c_str());
      }
    } else {
      if (configfile) {
        Jmsg2(jcr, M_FATAL, 0,
              T_("Cannot find device \"%s\" in config file %s.\n%s"), dev_name,
              configfile, AvailableDevicesListing().c_str());
      } else {
        Jmsg2(jcr, M_FATAL, 0, T_("Cannot use device \"%s\".\n%s"), dev_name,
              AvailableDevicesListing().c_str());
      }
    }
    return false;
  }

  if (!jcr->sd_impl->read_session.bsr && VolName[0] == 0) {
    if (!resolved_device.inferred_volume_name.empty()) {
      bstrncpy(VolName, resolved_device.inferred_volume_name.c_str(),
               sizeof(VolName));
    } else if (!bstrncmp(dev_name, "/dev/", 5)) {
      auto* p = dev_name + strlen(dev_name);
      while (p > dev_name && !IsPathSeparator(*p)) p--;
      if (IsPathSeparator(*p)) { bstrncpy(VolName, p + 1, sizeof(VolName)); }
    }
  }

  dev = FactoryCreateDevice(jcr, device_resource);
  if (!dev) {
    Jmsg1(jcr, M_FATAL, 0, T_("Cannot init device %s\n"), dev_name);
    return false;
  }
  device_resource->dev = dev;
  jcr->sd_impl->dcr = dcr;
  SetupNewDcrDevice(jcr, dcr, dev, NULL);
  if (!readonly) { dcr->SetWillWrite(); }

  if (VolName[0]) {
    bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
  }
  bstrncpy(dcr->dev_name, device_resource->archive_device_string,
           sizeof(dcr->dev_name));

  CreateRestoreVolumeList(jcr);

  if (readonly) { /* read only access? */
    Dmsg0(100, "Acquire device for read\n");
    if (!AcquireDeviceForRead(dcr)) { return false; }
    jcr->sd_impl->read_dcr = dcr;
  } else {
    if (!FirstOpenDevice(dcr)) {
      Jmsg1(jcr, M_FATAL, 0, T_("Cannot open %s\n"), dev->print_name());
      return false;
    }
  }

  return true;
}

/**
 * Called here when freeing JobControlRecord so that we can get rid
 *  of "daemon" specific memory allocated.
 */
static void MyFreeJcr(JobControlRecord* jcr)
{
  if (jcr->sd_impl->job_name) {
    FreePoolMemory(jcr->sd_impl->job_name);
    jcr->sd_impl->job_name = NULL;
  }

  if (jcr->client_name) {
    FreePoolMemory(jcr->client_name);
    jcr->client_name = NULL;
  }

  if (jcr->sd_impl->fileset_name) {
    FreePoolMemory(jcr->sd_impl->fileset_name);
    jcr->sd_impl->fileset_name = NULL;
  }

  if (jcr->sd_impl->fileset_md5) {
    FreePoolMemory(jcr->sd_impl->fileset_md5);
    jcr->sd_impl->fileset_md5 = NULL;
  }

  if (jcr->comment) {
    FreePoolMemory(jcr->comment);
    jcr->comment = NULL;
  }

  if (jcr->sd_impl->VolList) { FreeRestoreVolumeList(jcr); }

  if (jcr->sd_impl->dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->dcr);
    jcr->sd_impl->dcr = NULL;
  }

  if (jcr->sd_impl) {
    delete jcr->sd_impl;
    jcr->sd_impl = nullptr;
  }

  return;
}

/**
 * Search for device resource that corresponds to
 * device name on command line (or default).
 *
 * Returns: NULL on failure
 *          Device resource pointer on success
 */
static DeviceResolutionResult find_device_res(char* archive_device_string,
                                              bool readonly)
{
  DeviceResolutionResult result;
  bool found = false;
  DeviceResource* device_resource;
  auto normalized_resource_name = StripQuotes(archive_device_string);

  Dmsg0(900, "Enter find_device_res\n");
  if (my_config) {
    foreach_res (device_resource, R_DEVICE) {
      Dmsg2(900, "Compare %s and %s\n", device_resource->archive_device_string,
            archive_device_string);
      if (bstrcmp(device_resource->archive_device_string,
                  archive_device_string)) {
        found = true;
        break;
      }
    }

    if (!found) {
      /* Search for name of Device resource rather than archive name */
      foreach_res (device_resource, R_DEVICE) {
        Dmsg2(900, "Compare %s and %s\n", device_resource->resource_name_,
              normalized_resource_name.c_str());
        if (bstrcmp(device_resource->resource_name_,
                    normalized_resource_name.c_str())) {
          found = true;
          break;
        }
      }
    }

    if (!found) {
      AutochangerResource* autochanger_resource;
      foreach_res (autochanger_resource, R_AUTOCHANGER) {
        Dmsg2(900, "Compare %s and %s\n", autochanger_resource->resource_name_,
              normalized_resource_name.c_str());
        if (bstrcmp(autochanger_resource->resource_name_,
                    normalized_resource_name.c_str())) {
          result = ResolveAutochangerDevice(autochanger_resource, readonly);
          device_resource = result.device_resource;
          found = (device_resource != nullptr);
          break;
        }
      }
    }
  }

  if (!found && result.detail.empty()) {
    result = ResolveDirectLocalVolumePath(normalized_resource_name);
    device_resource = result.device_resource;
    found = (device_resource != nullptr);
  }

  if (!found) {
    if (result.detail.empty() && configfile) {
      Pmsg2(0, T_("Could not find device \"%s\" in config file %s.\n"),
            archive_device_string, configfile);
    }
    return result;
  }

  if (readonly) {
    Pmsg1(0, T_("Using device: \"%s\" for reading.\n"),
          device_resource->resource_name_);
  } else {
    Pmsg1(0, T_("Using device: \"%s\" for writing.\n"),
          device_resource->resource_name_);
  }

  result.device_resource = device_resource;
  return result;
}

// Device got an error, attempt to analyse it
void DisplayTapeErrorStatus(JobControlRecord* jcr, Device* dev)
{
  char* status;

  status = dev->StatusDev();

  if (BitIsSet(BMT_EOD, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of Data\n"));
  else if (BitIsSet(BMT_EOT, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of Tape\n"));
  else if (BitIsSet(BMT_EOF, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of File\n"));
  else if (BitIsSet(BMT_DR_OPEN, status))
    Jmsg(jcr, M_ERROR, 0, T_("Tape Door is Open\n"));
  else if (!BitIsSet(BMT_ONLINE, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected Tape is Off-line\n"));

  free(status);
}

} /* namespace storagedaemon */
