/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
// Kern Sibbald, March MM
/**
 * @file
 * Configuration file parser for Bareos Storage daemon
 */

#include "include/bareos.h"
#include <fmt/core.h>

#include "stored/stored_conf.h"
#include "stored/autochanger_resource.h"
#include "stored/device_resource.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/sd_backends.h"
#include "lib/address_conf.h"
#include "lib/bareos_resource.h"
#include "lib/berrno.h"
#include "lib/messages_resource.h"
#include "lib/resource_item.h"
#include "lib/parse_conf.h"
#include "lib/tls_resource_items.h"
#define NEED_JANSSON_NAMESPACE 1
#include "lib/output_formatter.h"
#include "lib/output_formatter_resource.h"
#include "lib/implementation_factory.h"
#include "lib/version.h"
#include "include/auth_types.h"
#include "include/jcr.h"

#include <algorithm>

namespace storagedaemon {

static void FreeResource(BareosResource* sres, int type);
static bool SaveResource(int type, const ResourceItem* items, int pass);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/* the first configuration pass uses this static memory */
static DirectorResource* res_dir;
static NdmpResource* res_ndmp;
static StorageResource* res_store;
static DeviceResource* res_dev;
static AutochangerResource* res_changer;

static MessagesResource* res_msgs;
#include "lib/messages_resource_items.h"

/* clang-format off */

static const ResourceItem store_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_store, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_store, description_), {}},
  { "Port", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store, SDaddrs), {config::DefaultValue{SD_DEFAULT_PORT}, config::Alias{"SdPort"}}},
  { "Address", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, SDaddrs), {config::DefaultValue{SD_DEFAULT_PORT}, config::Alias{"SdAddress"}}},
  { "Addresses", CFG_TYPE_ADDRESSES, ITEM(res_store, SDaddrs), {config::DefaultValue{SD_DEFAULT_PORT}, config::Alias{"SdAddresses"}}},
  { "SourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, SDsrc_addr), {config::DefaultValue{"0"}, config::Alias{"SdSourceAddress"}}},
  { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_store, working_directory), {config::DefaultValue{PATH_BAREOS_WORKINGDIR}, config::PlatformSpecific{}}},
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  { "BackendDirectory", CFG_TYPE_STR_VECTOR_OF_DIRS, ITEM(res_store, backend_directories), {config::DefaultValue{PATH_BAREOS_BACKENDDIR}, config::PlatformSpecific{},config::Description{"This is the directory where the storage daemon will look for dynamic backends (file, tape, etc.) when it needs to load them."}}},
#endif
  { "JustInTimeReservation", CFG_TYPE_BOOL, ITEM(res_store, just_in_time_reservation), {config::IntroducedIn{23, 1, 0}, config::DefaultValue{"Yes"}, config::Description{"The storage daemon will only reserve devices when it receives data that needs to be written.  This option also means that no session label gets written if the job is empty."}}},
  { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_store, plugin_directory), {}},
  { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_store, plugin_names), {}},
  { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_store, scripts_directory), {config::DefaultValue{PATH_BAREOS_SCRIPTDIR}, config::Description{"Path to directory containing script files"}, config::PlatformSpecific{}}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentJobs), {config::DeprecatedSince{24, 0, 0}, config::DefaultValue{"1000"}}},
  { "Messages", CFG_TYPE_RES, ITEM(res_store, messages), {config::Code{R_MSGS}}},
  { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store, SDConnectTimeout), {config::DefaultValue{"1800"}}},
  { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store, FDConnectTimeout), {config::DefaultValue{"1800"}}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store, heartbeat_interval), {config::DefaultValue{"0"}}},
  { "CheckpointInterval", CFG_TYPE_TIME, ITEM(res_store, checkpoint_interval), {config::DefaultValue{"0"}}},
  { "MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_store, max_network_buffer_size), {}},
  { "ClientConnectWait", CFG_TYPE_TIME, ITEM(res_store, client_wait), {config::DefaultValue{"1800"}}},
  { "VerId", CFG_TYPE_STR, ITEM(res_store, verid), {}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store, max_bandwidth_per_job), {}},
  { "AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_store, allow_bw_bursting), {config::DefaultValue{"false"}}},
  { "NdmpEnable", CFG_TYPE_BOOL, ITEM(res_store, ndmp_enable), {config::DefaultValue{"false"}}},
  { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_store, ndmp_snooping), {config::DefaultValue{"false"}}},
  { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_store, ndmploglevel), {config::DefaultValue{"4"}}},
  { "NdmpAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, NDMPaddrs), {config::DefaultValue{"10000"}}},
  { "NdmpAddresses", CFG_TYPE_ADDRESSES, ITEM(res_store, NDMPaddrs), {config::DefaultValue{"10000"}}},
  { "NdmpPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store, NDMPaddrs), {config::DefaultValue{"10000"}}},
  { "AutoXFlateOnReplication", CFG_TYPE_BOOL, ITEM(res_store, autoxflateonreplication), {config::IntroducedIn{13, 4, 0}, config::DefaultValue{"false"}}},
  { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_store, jcr_watchdog_time), {config::IntroducedIn{14, 2, 0}, config::Description{"Absolute time after which a Job gets terminated regardless of its progress"}}},
  { "CollectDeviceStatistics", CFG_TYPE_BOOL, ITEM(res_store, collect_dev_stats), {config::DeprecatedSince{22, 0, 0}, config::DefaultValue{"false"}}},
  { "CollectJobStatistics", CFG_TYPE_BOOL, ITEM(res_store, collect_job_stats), {config::DeprecatedSince{22, 0, 0}, config::DefaultValue{"false"}}},
  { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_store, stats_collect_interval), {config::DeprecatedSince{22, 0, 0}, config::DefaultValue{"0"}}},
  { "DeviceReserveByMediaType", CFG_TYPE_BOOL, ITEM(res_store, device_reserve_by_mediatype), {config::DefaultValue{"false"}}},
  { "FileDeviceConcurrentRead", CFG_TYPE_BOOL, ITEM(res_store, filedevice_concurrent_read), {config::DefaultValue{"false"}}},
  { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_store, secure_erase_cmdline), {config::IntroducedIn{15, 2, 1}, config::Description{"Specify command that will be called when bareos unlinks files."}}},
  { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_store, log_timestamp_format), {config::IntroducedIn{15, 2, 3}, config::DefaultValue{"%d-%b %H:%M"}}},
  { "EnableKtls", CFG_TYPE_BOOL, ITEM(res_store, enable_ktls), {config::DefaultValue{"false"}, config::Description{"If set to \"yes\", Bareos will allow the SSL implementation to use Kernel TLS."}, config::IntroducedIn{23, 0, 0}}},
    TLS_COMMON_CONFIG(res_store),
    TLS_CERT_CONFIG(res_store),
  {}
};

static const ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_dir, description_), {}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, password_), {config::Required{}}},
  { "Monitor", CFG_TYPE_BOOL, ITEM(res_dir, monitor), {}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir, max_bandwidth_per_job), {}},
  { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, keyencrkey), {config::Code{1}}},
    TLS_COMMON_CONFIG(res_dir),
    TLS_CERT_CONFIG(res_dir),
  {}
};

static const ResourceItem ndmp_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_ndmp, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_ndmp, description_), {}},
  { "Username", CFG_TYPE_STR, ITEM(res_ndmp, username), {config::Required{}}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_ndmp, password), {config::Required{}}},
  { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_ndmp, AuthType), {config::DefaultValue{"None"}}},
  { "LogLevel", CFG_TYPE_PINT32, ITEM(res_ndmp, LogLevel), {config::DefaultValue{"4"}}},
  {}
};

static const ResourceItem dev_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dev, resource_name_), {config::Required{}, config::Description{"Unique identifier of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_dev, description_), {config::Description{"The Description directive provides easier human recognition, but is not used by Bareos directly."}}},
  { "MediaType", CFG_TYPE_STRNAME, ITEM(res_dev, media_type), {config::Required{}}},
  { "DeviceType", CFG_TYPE_STDSTR, ITEM(res_dev, device_type), {config::DefaultValue{""}}},
  { "ArchiveDevice", CFG_TYPE_STRNAME, ITEM(res_dev, archive_device_string), {config::Required{}}},
  { "DeviceOptions", CFG_TYPE_STR, ITEM(res_dev, device_options), {config::IntroducedIn{15, 2, 0}}},
  { "DiagnosticDevice", CFG_TYPE_STRNAME, ITEM(res_dev, diag_device_name), {}},
  { "HardwareEndOfFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_EOF}}},
  { "HardwareEndOfMedium", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_EOM}}},
  { "BackwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_BSR}}},
  { "BackwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_BSF}}},
  { "BsfAtEom", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_BSFATEOM}}},
  { "TwoEof", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_TWOEOF}}},
  { "ForwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_FSR}}},
  { "ForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_FSF}}},
  { "FastForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_FASTFSF}}},
  { "RemovableMedia", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_REM}}},
  { "RandomAccess", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_RACCESS}}},
  { "AutomaticMount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_AUTOMOUNT}}},
  { "LabelMedia", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_LABEL}}},
  { "AlwaysOpen", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_ALWAYSOPEN}}},
  { "Autochanger", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_ATTACHED_TO_AUTOCHANGER}}},
  { "CloseOnPoll", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_CLOSEONPOLL}}},
  { "BlockPositioning", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_POSITIONBLOCKS}}},
  { "UseMtiocget", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_MTIOCGET}}},
  { "CheckLabels", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DeprecatedSince{23, 0, 0}, config::DefaultValue{"off"}, config::Code{CAP_CHECKLABELS}}},
  { "RequiresMount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_REQMOUNT}}},
  { "OfflineOnUnmount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"off"}, config::Code{CAP_OFFLINEUNMOUNT}}},
  { "BlockChecksum", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), {config::DefaultValue{"on"}, config::Code{CAP_BLOCKCHECKSUM}}},
  { "AccessMode", CFG_TYPE_IODIRECTION, ITEM(res_dev, access_mode), {config::DefaultValue{"readwrite"}, config::Description{"Access mode specifies whether this device can be reserved for reading, writing or for both modes (default)."}}},
  { "AutoSelect", CFG_TYPE_BOOL, ITEM(res_dev, autoselect), {config::DefaultValue{"true"}}},
  { "ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_dev, changer_name), {}},
  { "ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_dev, changer_command), {}},
  { "AlertCommand", CFG_TYPE_STRNAME, ITEM(res_dev, alert_command), {}},
  { "MaximumChangerWait", CFG_TYPE_TIME, ITEM(res_dev, max_changer_wait), {config::DefaultValue{"300"}}},
  { "MaximumOpenWait", CFG_TYPE_TIME, ITEM(res_dev, max_open_wait), {config::DefaultValue{"300"}}},
  { "MaximumOpenVolumes", CFG_TYPE_PINT32, ITEM(res_dev, max_open_vols), {config::DefaultValue{"1"}}},
  { "MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_dev, max_network_buffer_size), {config::DeprecatedSince{24, 0, 0}, config::Description{"Replaced by MaximumNetworkBufferSize (SD->Storage)."}}},
  { "VolumePollInterval", CFG_TYPE_TIME, ITEM(res_dev, vol_poll_interval), {config::DefaultValue{"300"}}},
  { "MaximumRewindWait", CFG_TYPE_TIME, ITEM(res_dev, max_rewind_wait), {config::DefaultValue{"300"}}},
  { "LabelBlockSize", CFG_TYPE_PINT32, ITEM(res_dev, label_block_size), {config::DefaultValue{"64512"}}},
  { "MinimumBlockSize", CFG_TYPE_PINT32, ITEM(res_dev, min_block_size), {}},
  { "MaximumBlockSize", CFG_TYPE_MAXBLOCKSIZE, ITEM(res_dev, max_block_size), {config::DefaultValue{"1048576"}}},
  { "MaximumFileSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_file_size), {config::DefaultValue{"1000000000"}}},
  { "VolumeCapacity", CFG_TYPE_SIZE64, ITEM(res_dev, volume_capacity), {}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dev, max_concurrent_jobs), {config::DefaultValue{"1"}}},
  { "SpoolDirectory", CFG_TYPE_DIR, ITEM(res_dev, spool_directory), {}},
  { "MaximumSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_spool_size), {}},
  { "MaximumJobSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_job_spool_size), {}},
  { "DriveIndex", CFG_TYPE_PINT16, ITEM(res_dev, drive_index), {}},
  { "MountPoint", CFG_TYPE_STRNAME, ITEM(res_dev, mount_point), {}},
  { "MountCommand", CFG_TYPE_STRNAME, ITEM(res_dev, mount_command), {}},
  { "UnmountCommand", CFG_TYPE_STRNAME, ITEM(res_dev, unmount_command), {}},
  { "LabelType", CFG_TYPE_LABEL, ITEM(res_dev, label_type), {config::DeprecatedSince{23, 0, 0}}},
  { "NoRewindOnClose", CFG_TYPE_BOOL, ITEM(res_dev, norewindonclose), {config::DefaultValue{"true"}}},
  { "DriveTapeAlertEnabled", CFG_TYPE_BOOL, ITEM(res_dev, drive_tapealert_enabled), {}},
  { "DriveCryptoEnabled", CFG_TYPE_BOOL, ITEM(res_dev, drive_crypto_enabled), {}},
  { "QueryCryptoStatus", CFG_TYPE_BOOL, ITEM(res_dev, query_crypto_status), {}},
  { "AutoDeflate", CFG_TYPE_IODIRECTION, ITEM(res_dev, autodeflate), {config::IntroducedIn{13, 4, 0}}},
  { "AutoDeflateAlgorithm", CFG_TYPE_CMPRSALGO, ITEM(res_dev, autodeflate_algorithm), {config::IntroducedIn{13, 4, 0}}},
  { "AutoDeflateLevel", CFG_TYPE_PINT16, ITEM(res_dev, autodeflate_level), {config::IntroducedIn{13, 4, 0}, config::DefaultValue{"6"}}},
  { "AutoInflate", CFG_TYPE_IODIRECTION, ITEM(res_dev, autoinflate), {config::IntroducedIn{13, 4, 0}}},
  { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_dev, collectstats), {config::DefaultValue{"true"}}},
  { "EofOnErrorIsEot", CFG_TYPE_BOOL, ITEM(res_dev, eof_on_error_is_eot), {config::IntroducedIn{18, 2, 4}, config::Description{"If Yes, Bareos will treat any read error at an end-of-file mark as end-of-tape. You should only set this option if your tape-drive fails to detect end-of-tape while reading."}}},
  { "Count", CFG_TYPE_PINT32, ITEM(res_dev, count), {config::DefaultValue{"1"}, config::Description{"If Count is set to (1 < Count < 10000), duplicated devices will be created post-fixed with serial numbers 0000 up to Count. The 0000 device is automatically assigned 'Autoselect=No'. Additionally, an autochanger resource is created with the name of the device the Count directive is specified for. The duplicated devices will be assigned to this autochanger unless they are used in another autochanger already."}}},
  {}
};

static const ResourceItem autochanger_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_changer, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_changer, description_), {}},
  { "Device", CFG_TYPE_ALIST_RES, ITEM(res_changer, device_resources), {config::Required{}, config::Code{R_DEVICE}}},
  { "ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_changer, changer_name), {config::Required{}}},
  { "ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_changer, changer_command), {config::Required{}}},
  {}
};

static ResourceTable resources[] = {
  {"Director", "Directors", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      []() { res_dir = new DirectorResource(); }, reinterpret_cast<BareosResource**>(&res_dir)},
  {"Ndmp", "Ndmp", ndmp_items, R_NDMP, sizeof(NdmpResource),
      []() { res_ndmp = new NdmpResource(); }, reinterpret_cast<BareosResource**>(&res_ndmp)},
  {"Storage", "Storages", store_items, R_STORAGE, sizeof(StorageResource),
      []() { res_store = new StorageResource(); }, reinterpret_cast<BareosResource**>(&res_store)},
  {"Device", "Devices", dev_items, R_DEVICE, sizeof(DeviceResource),
      []() { res_dev = new DeviceResource(); }, reinterpret_cast<BareosResource**>(&res_dev)},
  {"Messages", "Messages", msgs_items, R_MSGS, sizeof(MessagesResource),
      []() { res_msgs = new MessagesResource(); }, reinterpret_cast<BareosResource**>(&res_msgs)},
  {"Autochanger", "Autochangers", autochanger_items, R_AUTOCHANGER, sizeof(AutochangerResource),
      []() { res_changer = new AutochangerResource(); }, reinterpret_cast<BareosResource**>(&res_changer)},
  {nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr}};

/* clang-format on */

static struct s_kw authentication_methods[]
    = {{"None", AT_NONE}, {"Clear", AT_CLEAR}, {"MD5", AT_MD5}, {NULL, 0}};

struct s_io_kw {
  const char* name;
  IODirection token;
};

static s_io_kw io_directions[] = {
    {"in", IODirection::READ},         {"read", IODirection::READ},
    {"readonly", IODirection::READ},   {"out", IODirection::WRITE},
    {"write", IODirection::WRITE},     {"writeonly", IODirection::WRITE},
    {"both", IODirection::READ_WRITE}, {"readwrite", IODirection::READ_WRITE},
    {nullptr, IODirection::READ_WRITE}};

static s_kw compression_algorithms[]
    = {{"gzip", COMPRESS_GZIP},   {"lzo", COMPRESS_LZO1X},
       {"lzfast", COMPRESS_FZFZ}, {"lz4", COMPRESS_FZ4L},
       {"lz4hc", COMPRESS_FZ4H},  {NULL, 0}};

static void StoreAuthenticationType(ConfigurationParser* conf,
                                    lexer* lc,
                                    const ResourceItem* item,
                                    int index,
                                    int)
{
  auto found = ReadKeyword(conf, lc, authentication_methods);

  if (!found) {
    scan_err1(lc, T_("Expected a Authentication Type keyword, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

// Store password either clear if for NDMP or MD5 hashed for native.
static void StoreAutopassword(ConfigurationParser* conf,
                              lexer* lc,
                              const ResourceItem* item,
                              int index,
                              int pass)
{
  switch ((*item->allocated_resource)->rcode_) {
    case R_DIRECTOR:
      /* As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1. */
      switch (item->code) {
        case 1:
          StoreResource(conf, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          StoreResource(conf, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_NDMP:
      StoreResource(conf, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
    default:
      StoreResource(conf, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
  }
}

// Store Maximum Block Size, and check it is not greater than MAX_BLOCK_LENGTH
static void StoreMaxblocksize(ConfigurationParser* conf,
                              lexer* lc,
                              const ResourceItem* item,
                              int index,
                              int pass)
{
  StoreResource(conf, CFG_TYPE_SIZE32, lc, item, index, pass);
  if (GetItemVariable<uint32_t>(*item) > MAX_BLOCK_LENGTH) {
    scan_err2(lc,
              T_("Maximum Block Size configured value %u is greater than "
                 "allowed maximum: %u"),
              GetItemVariable<uint32_t>(*item), MAX_BLOCK_LENGTH);
  }
}

// Store the IO direction on a certain device.
static void StoreIoDirection(ConfigurationParser* conf,
                             lexer* lc,
                             const ResourceItem* item,
                             int index,
                             int)
{
  auto found = ReadKeyword(conf, lc, io_directions);
  if (!found) {
    scan_err1(lc, T_("Expected a IO direction keyword, got: %s"), lc->str());
  } else {
    SetItemVariable<IODirection>(*item, found->token);
  }
  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

// Store the compression algorithm to use on a certain device.
static void StoreCompressionalgorithm(ConfigurationParser* conf,
                                      lexer* lc,
                                      const ResourceItem* item,
                                      int index,
                                      int)
{
  auto found = ReadKeyword(conf, lc, compression_algorithms);

  if (!found) {
    scan_err1(lc, T_("Expected a Compression algorithm keyword, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token & 0xffffffff);
  }
  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.c, function InitResource, for more generic handling.
 */
static void InitResourceCb(const ResourceItem* item, int pass)
{
  switch (pass) {
    case 1:
      switch (item->type) {
        case CFG_TYPE_AUTHTYPE:
          for (int i = 0; authentication_methods[i].name; i++) {
            if (Bstrcasecmp(item->default_value,
                            authentication_methods[i].name)) {
              SetItemVariable<uint32_t>(*item, authentication_methods[i].token);
            }
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

static void ParseConfigCb(ConfigurationParser* conf,
                          lexer* lc,
                          const ResourceItem* item,
                          int index,
                          int pass)
{
  switch (item->type) {
    case CFG_TYPE_AUTOPASSWORD:
      StoreAutopassword(conf, lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHTYPE:
      StoreAuthenticationType(conf, lc, item, index, pass);
      break;
    case CFG_TYPE_MAXBLOCKSIZE:
      StoreMaxblocksize(conf, lc, item, index, pass);
      break;
    case CFG_TYPE_IODIRECTION:
      StoreIoDirection(conf, lc, item, index, pass);
      break;
    case CFG_TYPE_CMPRSALGO:
      StoreCompressionalgorithm(conf, lc, item, index, pass);
      break;
    default:
      break;
  }
}

static void MultiplyDevice(DeviceResource& original,
                           ConfigurationParser& config)
{
  // create autochanger
  if (!original.changer_res) {
    original.changer_res = AutochangerResource::CreateImplicitAutochanger(
                               std::string(original.resource_name_))
                               .release();
    original.changer_command = strdup("");
    if (!config.GetResWithName(R_AUTOCHANGER,
                               original.changer_res->resource_name_)) {
      config.AppendToResourcesChain(original.changer_res, R_AUTOCHANGER);
    } else {
      Emsg0(M_INFO, 0,
            "Device resource \"%s\" will not implicitly create an autochanger "
            "resource \"%s\" since an autochanger resource with that name "
            "already exists.\n",
            original.resource_name_, original.changer_res->resource_name_);
    }
  } else {
    auto& devices = *original.changer_res->device_resources;
    auto it = std::find(devices.begin(), devices.end(), &original);
    if (it != devices.end()) { devices.remove(it - devices.begin()); }
    Emsg0(M_INFO, 0,
          "Device resource \"%s\" will not implicitly create autochanger "
          "\"%s\" resource since it is already used by autochanger "
          "resource \"%s\".\n",
          original.resource_name_, original.resource_name_,
          original.changer_res->resource_name_);
  }

  // create devices
  for (uint32_t i = 0; i <= original.count; i++) {
    std::string serial_number = std::string("0000") + std::to_string(i);
    std::string device_name = original.resource_name_
                              + serial_number.substr(serial_number.size() - 4);
    DeviceResource* device = dynamic_cast<DeviceResource*>(
        config.GetResWithName(R_DEVICE, device_name.c_str()));
    if (!device) {
      device = original.CreateCopy(device_name).release();
      if (i == 0) { device->autoselect = false; }
      config.AppendToResourcesChain(device, R_DEVICE);
    } else {
      Emsg0(M_INFO, 0,
            "Device resource \"%s\" will not implicitly create device \"%s\" "
            "resource since there already exists a device resource with that "
            "name.\n",
            original.resource_name_, device_name.c_str());
    }

    auto& devices = *original.changer_res->device_resources;
    auto it = std::find(devices.begin(), devices.end(), device);
    if (it == devices.end()) { devices.append(device); }
  }
  // The original device name is prefixed with "$" in order to prevent a naming
  // collision with the implicitly created autochanger. If both the original
  // device and the implicit autochanger have the same name, both will be used
  // if specified in a Director -> Storage resource. However, we only want the
  // implicit autochanger to be used so we give the device a different name.
  char* prefixed_resource_name
      = strdup(("$" + std::string{original.resource_name_}).c_str());
  free(original.resource_name_);
  original.resource_name_ = prefixed_resource_name;
}

static void MultiplyConfiguredDevices(ConfigurationParser& config)
{
  BareosResource* p = nullptr;
  while ((p = config.GetNextRes(R_DEVICE, p))) {
    DeviceResource& d = dynamic_cast<DeviceResource&>(*p);
    if (d.count > 1 && d.count < 10000) {
      MultiplyDevice(d, config);
    } else if (d.count >= 10000) {
      Emsg0(M_CONFIG_ERROR, 0,
            "Count directive in device \"%s\" is %u, but must be in range 1 < "
            "Count < 10000.\n",
            d.resource_name_, d.count);
    }
  }
}

static void ConfigBeforeCallback(ConfigurationParser& config)
{
  std::map<int, std::string> map{
      {R_DIRECTOR, "R_DIRECTOR"},
      {R_JOB, "R_JOB"}, /* needed for client name conversion */
      {R_NDMP, "R_NDMP"},
      {R_STORAGE, "R_STORAGE"},
      {R_MSGS, "R_MSGS"},
      {R_DEVICE, "R_DEVICE"},
      {R_AUTOCHANGER, "R_AUTOCHANGER"},
      {R_CLIENT, "R_CLIENT"}}; /* needed for network dump */
  config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void CheckDropletDevices(ConfigurationParser& config)
{
  BareosResource* p = nullptr;

  while ((p = config.GetNextRes(R_DEVICE, p)) != nullptr) {
    DeviceResource* d = dynamic_cast<DeviceResource*>(p);
    if (d && d->device_type == DeviceType::B_DROPLET_DEV) {
      my_config->AddWarning(
          fmt::format("device {} uses the droplet backend, "
                      "please consider using the newer dplcompat backend.",
                      d->archive_device_string));
      if (d->max_concurrent_jobs == 0) {
        /* 0 is the general default. However, for this device_type, only 1
         * works. So we set it to this value. */
        Jmsg1(nullptr, M_WARNING, 0,
              T_("device %s is set to the default 'Maximum Concurrent Jobs' = "
                 "1.\n"),
              d->archive_device_string);
        d->max_concurrent_jobs = 1;
      } else if (d->max_concurrent_jobs > 1) {
        Jmsg2(nullptr, M_ERROR_TERM, 0,
              T_("device %s is configured with 'Maximum Concurrent Jobs' = %d, "
                 "however only 1 is supported.\n"),
              d->archive_device_string, d->max_concurrent_jobs);
      }
    }
  }
}

static void GuessMissingDeviceTypes(ConfigurationParser& config)
{
  BareosResource* p = nullptr;

  while ((p = config.GetNextRes(R_DEVICE, p)) != nullptr) {
    DeviceResource* d = dynamic_cast<DeviceResource*>(p);
    if (d && d->device_type == DeviceType::B_UNKNOWN_DEV) {
      struct stat statp;
      // Check that device is available
      if (stat(d->archive_device_string, &statp) < 0) {
        BErrNo be;
        Jmsg2(nullptr, M_ERROR_TERM, 0,
              T_("Unable to stat path '%s' for device %s: ERR=%s\n"
                 "Consider setting Device Type if device is not available when "
                 "daemon starts.\n"),
              d->archive_device_string, d->resource_name_, be.bstrerror());
        return;
      }
      if (S_ISDIR(statp.st_mode)) {
        d->device_type = DeviceType::B_FILE_DEV;
      } else if (S_ISCHR(statp.st_mode)) {
        d->device_type = DeviceType::B_TAPE_DEV;
      } else if (S_ISFIFO(statp.st_mode)) {
        d->device_type = DeviceType::B_FIFO_DEV;
      } else if (!BitIsSet(CAP_REQMOUNT, d->cap_bits)) {
        Jmsg2(nullptr, M_ERROR_TERM, 0,
              "cannot deduce Device Type from '%s'. Must be tape or directory, "
              "st_mode=%04o\n",
              d->archive_device_string, (statp.st_mode & ~S_IFMT));
        return;
      }
    }
  }
}

static void CheckAndLoadDeviceBackends(ConfigurationParser& config)
{
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  auto storage_res
      = dynamic_cast<StorageResource*>(config.GetNextRes(R_STORAGE, NULL));
#endif

  BareosResource* p = nullptr;
  while ((p = config.GetNextRes(R_DEVICE, p)) != nullptr) {
    DeviceResource* d = dynamic_cast<DeviceResource*>(p);
    if (d) {
      to_lower(d->device_type);
      if (!ImplementationFactory<Device>::IsRegistered(d->device_type)) {
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
        if (!storage_res || storage_res->backend_directories.empty()) {
          Jmsg2(nullptr, M_ERROR_TERM, 0,
                "Backend Directory not set. Cannot load dynamic backend %s\n",
                d->device_type.c_str());
        }
        if (!LoadStorageBackend(d->device_type,
                                storage_res->backend_directories)) {
          Jmsg2(nullptr, M_ERROR_TERM, 0,
                "Could not load storage backend %s for device %s.\n",
                d->device_type.c_str(), d->resource_name_);
        }
#else
        Jmsg2(nullptr, M_ERROR_TERM, 0,
              "Backend %s for device %s not available.\n",
              d->device_type.c_str(), d->resource_name_);
#endif
      }
    }
  }
}

static void ConfigReadyCallback(ConfigurationParser& config)
{
  MultiplyConfiguredDevices(config);
  GuessMissingDeviceTypes(config);
  CheckAndLoadDeviceBackends(config);
  CheckDropletDevices(config);
}

ConfigurationParser* InitSdConfig(const char* t_configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      t_configfile, InitResourceCb, ParseConfigCb, nullptr, exit_code, R_NUM,
      resources, default_config_filename.c_str(), "bareos-sd.d",
      ConfigBeforeCallback, ConfigReadyCallback, SaveResource, DumpResource,
      FreeResource);
  if (config) { config->r_own_ = R_STORAGE; }
  return config;
}

bool ParseSdConfig(const char* t_configfile, int exit_code)
{
  bool retval;

  retval = my_config->ParseConfig();

  if (retval) {
    me = (StorageResource*)my_config->GetNextRes(R_STORAGE, NULL);
    my_config->own_resource_ = me;
    if (!me) {
      Emsg1(exit_code, 0,
            T_("No Storage resource defined in %s. Cannot continue.\n"),
            t_configfile);
      return retval;
    }
  }

  return retval;
}

// Print configuration file schema in json format
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-sd"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  // Resources
  json_t* resource = json_object();
  json_object_set_new(json, "resource", resource);
  json_t* bareos_sd = json_object();
  json_object_set_new(resource, "bareos-sd", bareos_sd);

  for (int r = 0; my_config->resource_definitions_[r].name; r++) {
    const ResourceTable& resource_table = my_config->resource_definitions_[r];
    json_object_set_new(bareos_sd, resource_table.name,
                        json_items(resource_table.items));
  }

  char* const json_str = json_dumps(json, JSON_INDENT(2));
  PmStrcat(buffer, json_str);
  free(json_str);
  json_decref(json);

  return true;
}
#else
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  PmStrcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");
  return false;
}
#endif

#include <cassert>

static bool DumpResource_(int type,
                          BareosResource* res,
                          ConfigurationParser::sender* sendit,
                          void* sock,
                          bool hide_sensitive_data,
                          bool verbose)
{
  PoolMem buf;
  bool recurse = true;
  OutputFormatter output_formatter
      = OutputFormatter(sendit, sock, nullptr, nullptr);
  OutputFormatterResource output_formatter_resource
      = OutputFormatterResource(&output_formatter);

  if (!res) {
    sendit(sock, T_("Warning: no \"%s\" resource (%d) defined.\n"),
           my_config->ResToStr(type), type);
    return false;
  }

  if (type < 0) { /* no recursion */
    type = -type;
    recurse = false;
  }

  switch (type) {
    case R_MSGS: {
      MessagesResource* resclass = dynamic_cast<MessagesResource*>(res);
      assert(resclass);
      resclass->PrintConfig(output_formatter_resource, *my_config,
                            hide_sensitive_data, verbose);
      break;
    }
    case R_DEVICE: {
      DeviceResource* d = dynamic_cast<DeviceResource*>(res);
      assert(d);
      d->PrintConfig(output_formatter_resource, *my_config, hide_sensitive_data,
                     verbose);
      break;
    }
    case R_AUTOCHANGER: {
      AutochangerResource* autochanger
          = dynamic_cast<AutochangerResource*>(res);
      assert(autochanger);
      autochanger->PrintConfig(output_formatter_resource, *my_config,
                               hide_sensitive_data, verbose);
      break;
    }
    default:
      BareosResource* p = dynamic_cast<BareosResource*>(res);
      assert(p);
      p->PrintConfig(output_formatter_resource, *my_config, hide_sensitive_data,
                     verbose);
      break;
  }

  return recurse;
}

static void DumpResource(int type,
                         BareosResource* res,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  bool recurse = true;
  BareosResource* p = res;

  while (recurse && p) {
    recurse
        = DumpResource_(type, p, sendit, sock, hide_sensitive_data, verbose);
    p = p->next_;
  }
}

static bool SaveResource(int type, const ResourceItem* items, int pass)
{
  int i;
  int error = 0;

  // Ensure that all required items are present
  for (i = 0; items[i].name; i++) {
    if (items[i].is_required) {
      if (!items[i].IsPresent()) {
        Emsg2(
            M_ERROR_TERM, 0,
            T_("\"%s\" item is required in \"%s\" resource, but not found.\n"),
            items[i].name, resources[type].name);
      }
    }

    if (i >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR_TERM, 0, T_("Too many items in \"%s\" resource\n"),
            resources[type].name);
    }
  }

  // save previously discovered pointers into dynamic memory
  if (pass == 2) {
    BareosResource* allocated_resource = my_config->GetResWithName(
        type, (*items->allocated_resource)->resource_name_);
    if (allocated_resource && !allocated_resource->Validate()) { return false; }
    switch (type) {
      case R_DEVICE:
      case R_MSGS:
      case R_NDMP:
        // Resources not containing a resource
        break;
      case R_DIRECTOR: {
        DirectorResource* p
            = dynamic_cast<DirectorResource*>(allocated_resource);
        if (!p) {
          Emsg1(M_ERROR_TERM, 0, T_("Cannot find Director resource %s\n"),
                res_dir->resource_name_);
        } else {
          p->tls_cert_.allowed_certificate_common_names_
              = std::move(res_dir->tls_cert_.allowed_certificate_common_names_);
        }
        break;
      }
      case R_STORAGE: {
        StorageResource* p = dynamic_cast<StorageResource*>(allocated_resource);
        if (!p) {
          Emsg1(M_ERROR_TERM, 0, T_("Cannot find Storage resource %s\n"),
                res_store->resource_name_);
        } else {
          p->plugin_names = res_store->plugin_names;
          p->messages = res_store->messages;
          p->backend_directories = res_store->backend_directories;
          p->tls_cert_.allowed_certificate_common_names_ = std::move(
              res_store->tls_cert_.allowed_certificate_common_names_);
        }
        break;
      }
      case R_AUTOCHANGER: {
        AutochangerResource* p
            = dynamic_cast<AutochangerResource*>(allocated_resource);
        if (!p) {
          Emsg1(M_ERROR_TERM, 0, T_("Cannot find AutoChanger resource %s\n"),
                res_changer->resource_name_);
        } else {
          p->device_resources = res_changer->device_resources;

          for (auto* q : p->device_resources) { q->changer_res = p; }

          int errstat;
          if ((errstat = RwlInit(&p->changer_lock, PRIO_SD_ACH_ACCESS)) != 0) {
            BErrNo be;
            Jmsg1(NULL, M_ERROR_TERM, 0, T_("Unable to init lock: ERR=%s\n"),
                  be.bstrerror(errstat));
          }
        }
        break;
      }
      default:
        printf(T_("Unknown resource type %d\n"), type);
        error = 1;
        break;
    }

    /* resource_name_ was already deep copied during 1. pass
     * as matter of fact the remaining allocated memory is
     * redundant and would not be freed in the dynamic resources;
     *
     * currently, this is the best place to free that */

    BareosResource* res = *items[0].allocated_resource;

    if (res) {
      if (res->resource_name_) {
        free(res->resource_name_);
        res->resource_name_ = nullptr;
      }
      if (res->description_) {
        free(res->description_);
        res->description_ = nullptr;
      }
    }
    return (error == 0);
  }

  if (!error) {
    BareosResource* new_resource = nullptr;
    switch (resources[type].rcode) {
      case R_DIRECTOR:
        new_resource = res_dir;
        res_dir = nullptr;
        break;
      case R_NDMP:
        new_resource = res_ndmp;
        res_ndmp = nullptr;
        break;
      case R_STORAGE:
        new_resource = res_store;
        res_store = nullptr;
        break;
      case R_DEVICE:
        new_resource = res_dev;
        res_dev = nullptr;
        break;
      case R_MSGS:
        new_resource = res_msgs;
        res_msgs = nullptr;
        break;
      case R_AUTOCHANGER:
        new_resource = res_changer;
        res_changer = nullptr;
        break;
      default:
        ASSERT(false);
        break;
    }
    error = my_config->AppendToResourcesChain(new_resource, type) ? 0 : 1;
  }
  return (error == 0);
}  // namespace storagedaemon

static void FreeResource(BareosResource* res, int type)
{
  if (!res) return;

  if (res->resource_name_) {
    free(res->resource_name_);
    res->resource_name_ = nullptr;
  }
  if (res->description_) {
    free(res->description_);
    res->description_ = nullptr;
  }

  BareosResource* next_ressource = (BareosResource*)res->next_;

  switch (type) {
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      assert(p);
      if (p->password_.value) { free(p->password_.value); }
      if (p->address) { free(p->address); }
      if (p->keyencrkey.value) { free(p->keyencrkey.value); }
      delete p;
      break;
    }
    case R_NDMP: {
      NdmpResource* p = dynamic_cast<NdmpResource*>(res);
      assert(p);
      if (p->username) { free(p->username); }
      if (p->password.value) { free(p->password.value); }
      delete p;
      break;
    }
    case R_AUTOCHANGER: {
      AutochangerResource* p = dynamic_cast<AutochangerResource*>(res);
      assert(p);
      if (p->changer_name) { free(p->changer_name); }
      if (p->changer_command) { free(p->changer_command); }
      if (p->device_resources) { delete p->device_resources; }
      RwlDestroy(&p->changer_lock);
      delete p;
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(res);
      assert(p);
      if (p->SDaddrs) { FreeAddresses(p->SDaddrs); }
      if (p->SDsrc_addr) { FreeAddresses(p->SDsrc_addr); }
      if (p->NDMPaddrs) { FreeAddresses(p->NDMPaddrs); }
      if (p->working_directory) { free(p->working_directory); }
      if (p->plugin_directory) { free(p->plugin_directory); }
      if (p->plugin_names) { delete p->plugin_names; }
      if (p->scripts_directory) { free(p->scripts_directory); }
      if (p->verid) { free(p->verid); }
      if (p->secure_erase_cmdline) { free(p->secure_erase_cmdline); }
      if (p->log_timestamp_format) { free(p->log_timestamp_format); }
      delete p;
      break;
    }
    case R_DEVICE: {
      DeviceResource* p = dynamic_cast<DeviceResource*>(res);
      assert(p);
      if (p->media_type) { free(p->media_type); }
      if (p->archive_device_string) { free(p->archive_device_string); }
      if (p->device_options) { free(p->device_options); }
      if (p->diag_device_name) { free(p->diag_device_name); }
      if (p->changer_name) { free(p->changer_name); }
      if (p->changer_command) { free(p->changer_command); }
      if (p->alert_command) { free(p->alert_command); }
      if (p->spool_directory) { free(p->spool_directory); }
      if (p->mount_point) { free(p->mount_point); }
      if (p->mount_command) { free(p->mount_command); }
      if (p->unmount_command) { free(p->unmount_command); }
      delete p;
      break;
    }
    case R_MSGS: {
      MessagesResource* p = dynamic_cast<MessagesResource*>(res);
      assert(p);
      delete p;
      break;
    }
    default:
      Dmsg1(0, T_("Unknown resource type %d\n"), type);
      break;
  }
  if (next_ressource) { my_config->FreeResourceCb_(next_ressource, type); }
}

} /* namespace storagedaemon  */
