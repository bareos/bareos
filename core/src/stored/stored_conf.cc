/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, March MM
 */
/**
 * @file
 * Configuration file parser for Bareos Storage daemon
 */

#include "include/bareos.h"
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
#include "lib/json.h"
#include "include/auth_types.h"
#include "include/jcr.h"

namespace storagedaemon {

static BareosResource* sres_head[R_LAST - R_FIRST + 1];
static BareosResource** res_head = sres_head;

static void FreeResource(BareosResource* sres, int type);
static bool SaveResource(int type, ResourceItem* items, int pass);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         bool sendit(void* sock, const char* fmt, ...),
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

static ResourceItem store_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_store, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_store, description_), 0, 0, NULL, NULL, NULL},
  {"SdPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store, SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL},
  {"SdAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL},
  {"SdAddresses", CFG_TYPE_ADDRESSES, ITEM(res_store, SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL},
  {"SdSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, SDsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"WorkingDirectory", CFG_TYPE_DIR, ITEM(res_store, working_directory), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, PATH_BAREOS_WORKINGDIR, NULL, NULL},
  {"PidDirectory", CFG_TYPE_DIR, ITEM(res_store, pid_directory), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, PATH_BAREOS_PIDDIR, NULL, NULL},
  {"SubSysDirectory", CFG_TYPE_DIR, ITEM(res_store, subsys_directory), CFG_ITEM_DEPRECATED, 0, NULL, NULL, NULL},
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  {"BackendDirectory", CFG_TYPE_STR_VECTOR_OF_DIRS, ITEM(res_store, backend_directories), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, PATH_BAREOS_BACKENDDIR, NULL, NULL},
#endif
  {"PluginDirectory", CFG_TYPE_DIR, ITEM(res_store, plugin_directory), 0, 0, NULL, NULL, NULL},
  {"PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_store, plugin_names), 0, 0, NULL, NULL, NULL},
  {"ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_store, scripts_directory), 0, 0, NULL, NULL, NULL},
  {"MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL},
  {"MaximumConnections", CFG_TYPE_PINT32, ITEM(res_store, MaxConnections), 0, CFG_ITEM_DEFAULT, "42", "15.2.3-", NULL},
  {"Messages", CFG_TYPE_RES, ITEM(res_store, messages), R_MSGS, 0, NULL, NULL, NULL},
  {"SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store, SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL},
  {"FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store, FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL},
  {"HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store, heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_store, max_network_buffer_size), 0, 0, NULL, NULL, NULL},
  {"ClientConnectWait", CFG_TYPE_TIME, ITEM(res_store, client_wait), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL},
  {"VerId", CFG_TYPE_STR, ITEM(res_store, verid), 0, 0, NULL, NULL, NULL},
  {"Compatible", CFG_TYPE_BOOL, ITEM(res_store, compatible), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store, max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_store, allow_bw_bursting), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"NdmpEnable", CFG_TYPE_BOOL, ITEM(res_store, ndmp_enable), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_store, ndmp_snooping), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_store, ndmploglevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL},
  {"NdmpAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store, NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL},
  {"NdmpAddresses", CFG_TYPE_ADDRESSES, ITEM(res_store, NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL},
  {"NdmpPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store, NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL},
  {"AutoXFlateOnReplication", CFG_TYPE_BOOL, ITEM(res_store, autoxflateonreplication), 0, CFG_ITEM_DEFAULT, "false", "13.4.0-", NULL},
  {"AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_store, jcr_watchdog_time), 0, 0, NULL, NULL, NULL},
  {"CollectDeviceStatistics", CFG_TYPE_BOOL, ITEM(res_store, collect_dev_stats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"CollectJobStatistics", CFG_TYPE_BOOL, ITEM(res_store, collect_job_stats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_store, stats_collect_interval), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL},
  {"DeviceReserveByMediaType", CFG_TYPE_BOOL, ITEM(res_store, device_reserve_by_mediatype), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"FileDeviceConcurrentRead", CFG_TYPE_BOOL, ITEM(res_store, filedevice_concurrent_read), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"SecureEraseCommand", CFG_TYPE_STR, ITEM(res_store, secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
      "Specify command that will be called when bareos unlinks files."},
  {"LogTimestampFormat", CFG_TYPE_STR, ITEM(res_store, log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL},
    TLS_COMMON_CONFIG(res_store),
    TLS_CERT_CONFIG(res_store),
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem dir_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_dir, description_), 0, 0, NULL, NULL, NULL},
  {"Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Monitor", CFG_TYPE_BOOL, ITEM(res_dir, monitor), 0, 0, NULL, NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir, max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, keyencrkey), 1, 0, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_dir),
    TLS_CERT_CONFIG(res_dir),
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem ndmp_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_ndmp, resource_name_), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_ndmp, description_), 0, 0, 0, NULL, NULL},
  {"Username", CFG_TYPE_STR, ITEM(res_ndmp, username), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL},
  {"Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_ndmp, password), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL},
  {"AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_ndmp, AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL},
  {"LogLevel", CFG_TYPE_PINT32, ITEM(res_ndmp, LogLevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL},
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem dev_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_dev, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, "Unique identifier of the resource."},
  {"Description", CFG_TYPE_STR, ITEM(res_dev, description_), 0, 0, NULL, NULL,
      "The Description directive provides easier human recognition, but is not used by Bareos directly."},
  {"MediaType", CFG_TYPE_STRNAME, ITEM(res_dev, media_type), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"DeviceType", CFG_TYPE_DEVTYPE, ITEM(res_dev, dev_type), 0, 0, NULL, NULL, NULL},
  {"ArchiveDevice", CFG_TYPE_STRNAME, ITEM(res_dev, device_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"DeviceOptions", CFG_TYPE_STR, ITEM(res_dev, device_options), 0, 0, NULL, "15.2.0-", NULL},
  {"DiagnosticDevice", CFG_TYPE_STRNAME, ITEM(res_dev, diag_device_name), 0, 0, NULL, NULL, NULL},
  {"HardwareEndOfFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_EOF, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"HardwareEndOfMedium", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_EOM, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"BackwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_BSR, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"BackwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_BSF, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"BsfAtEom", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_BSFATEOM, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"TwoEof", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_TWOEOF, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"ForwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_FSR, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"ForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_FSF, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"FastForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_FASTFSF, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"RemovableMedia", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_REM, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"RandomAccess", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_RACCESS, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"AutomaticMount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_AUTOMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"LabelMedia", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_LABEL, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"AlwaysOpen", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_ALWAYSOPEN, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"Autochanger", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_AUTOCHANGER, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"CloseOnPoll", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_CLOSEONPOLL, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"BlockPositioning", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_POSITIONBLOCKS, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"UseMtiocget", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_MTIOCGET, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"CheckLabels", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_CHECKLABELS, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"RequiresMount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_REQMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"OfflineOnUnmount", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_OFFLINEUNMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL},
  {"BlockChecksum", CFG_TYPE_BIT, ITEM(res_dev, cap_bits), CAP_BLOCKCHECKSUM, CFG_ITEM_DEFAULT, "on", NULL, NULL},
  {"AutoSelect", CFG_TYPE_BOOL, ITEM(res_dev, autoselect), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL},
  {"ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_dev, changer_name), 0, 0, NULL, NULL, NULL},
  {"ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_dev, changer_command), 0, 0, NULL, NULL, NULL},
  {"AlertCommand", CFG_TYPE_STRNAME, ITEM(res_dev, alert_command), 0, 0, NULL, NULL, NULL},
  {"MaximumChangerWait", CFG_TYPE_TIME, ITEM(res_dev, max_changer_wait), 0, CFG_ITEM_DEFAULT,
      "300" /* 5 minutes */, NULL, NULL},
  {"MaximumOpenWait", CFG_TYPE_TIME, ITEM(res_dev, max_open_wait), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL},
  {"MaximumOpenVolumes", CFG_TYPE_PINT32, ITEM(res_dev, max_open_vols), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL},
  {"MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_dev, max_network_buffer_size), 0, 0, NULL, NULL, NULL},
  {"VolumePollInterval", CFG_TYPE_TIME, ITEM(res_dev, vol_poll_interval), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL},
  {"MaximumRewindWait", CFG_TYPE_TIME, ITEM(res_dev, max_rewind_wait), 0, CFG_ITEM_DEFAULT,
      "300" /* 5 minutes */, NULL, NULL},
  {"LabelBlockSize", CFG_TYPE_PINT32, ITEM(res_dev, label_block_size), 0, CFG_ITEM_DEFAULT,
      "64512" /* DEFAULT_BLOCK_SIZE */, NULL, NULL},
  {"MinimumBlockSize", CFG_TYPE_PINT32, ITEM(res_dev, min_block_size), 0, 0, NULL, NULL, NULL},
  {"MaximumBlockSize", CFG_TYPE_MAXBLOCKSIZE, ITEM(res_dev, max_block_size), 0, 0, NULL, NULL, NULL},
  {"MaximumVolumeSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_volume_size), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"MaximumFileSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_file_size), 0, CFG_ITEM_DEFAULT, "1000000000", NULL, NULL},
  {"VolumeCapacity", CFG_TYPE_SIZE64, ITEM(res_dev, volume_capacity), 0, 0, NULL, NULL, NULL},
  {"MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dev, max_concurrent_jobs), 0, 0, NULL, NULL, NULL},
  {"SpoolDirectory", CFG_TYPE_DIR, ITEM(res_dev, spool_directory), 0, 0, NULL, NULL, NULL},
  {"MaximumSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_spool_size), 0, 0, NULL, NULL, NULL},
  {"MaximumJobSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_job_spool_size), 0, 0, NULL, NULL, NULL},
  {"DriveIndex", CFG_TYPE_PINT16, ITEM(res_dev, drive_index), 0, 0, NULL, NULL, NULL},
  {"MaximumPartSize", CFG_TYPE_SIZE64, ITEM(res_dev, max_part_size), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"MountPoint", CFG_TYPE_STRNAME, ITEM(res_dev, mount_point), 0, 0, NULL, NULL, NULL},
  {"MountCommand", CFG_TYPE_STRNAME, ITEM(res_dev, mount_command), 0, 0, NULL, NULL, NULL},
  {"UnmountCommand", CFG_TYPE_STRNAME, ITEM(res_dev, unmount_command), 0, 0, NULL, NULL, NULL},
  {"WritePartCommand", CFG_TYPE_STRNAME, ITEM(res_dev, write_part_command), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"FreeSpaceCommand", CFG_TYPE_STRNAME, ITEM(res_dev, free_space_command), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"LabelType", CFG_TYPE_LABEL, ITEM(res_dev, label_type), 0, 0, NULL, NULL, NULL},
  {"NoRewindOnClose", CFG_TYPE_BOOL, ITEM(res_dev, norewindonclose), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL},
  {"DriveTapeAlertEnabled", CFG_TYPE_BOOL, ITEM(res_dev, drive_tapealert_enabled), 0, 0, NULL, NULL, NULL},
  {"DriveCryptoEnabled", CFG_TYPE_BOOL, ITEM(res_dev, drive_crypto_enabled), 0, 0, NULL, NULL, NULL},
  {"QueryCryptoStatus", CFG_TYPE_BOOL, ITEM(res_dev, query_crypto_status), 0, 0, NULL, NULL, NULL},
  {"AutoDeflate", CFG_TYPE_IODIRECTION, ITEM(res_dev, autodeflate), 0, 0, NULL, "13.4.0-", NULL},
  {"AutoDeflateAlgorithm", CFG_TYPE_CMPRSALGO, ITEM(res_dev, autodeflate_algorithm), 0, 0, NULL, "13.4.0-", NULL},
  {"AutoDeflateLevel", CFG_TYPE_PINT16, ITEM(res_dev, autodeflate_level), 0, CFG_ITEM_DEFAULT, "6", "13.4.0-",NULL},
  {"AutoInflate", CFG_TYPE_IODIRECTION, ITEM(res_dev, autoinflate), 0, 0, NULL, "13.4.0-", NULL},
  {"CollectStatistics", CFG_TYPE_BOOL, ITEM(res_dev, collectstats), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL},
  {"EofOnErrorIsEot", CFG_TYPE_BOOL, ITEM(res_dev, eof_on_error_is_eot), 0, CFG_ITEM_DEFAULT, NULL, "18.2.4-",
      "If Yes, Bareos will treat any read error at an end-of-file mark as end-of-tape. You should only set "
      "this option if your tape-drive fails to detect end-of-tape while reading."},
  {"Count", CFG_TYPE_PINT32, ITEM(res_dev, count), 0, CFG_ITEM_DEFAULT, "1", NULL, "If Count is set to (1 < Count < 10000), "
  "this resource will be multiplied Count times. The names of multiplied resources will have a serial number (0001, 0002, ...) attached. "
  "If set to 1 only this single resource will be used and its name will not be altered."},
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem autochanger_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_changer, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_changer, description_), 0, 0, NULL, NULL, NULL},
  {"Device", CFG_TYPE_ALIST_RES, ITEM(res_changer, device_resources), R_DEVICE, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_changer, changer_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_changer, changer_command), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
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

static struct s_kw authentication_methods[] = {{"None", AT_NONE},
                                               {"Clear", AT_CLEAR},
                                               {"MD5", AT_MD5},
                                               {NULL, 0}};

struct s_dvt_kw {
  const char* name;
  DeviceType token;
};

static s_dvt_kw device_types[] = {
    {"file", DeviceType::B_FILE_DEV},
    {"tape", DeviceType::B_TAPE_DEV},
    {"fifo", DeviceType::B_FIFO_DEV},
    {"vtl", DeviceType::B_VTL_DEV},
    {"gfapi", DeviceType::B_GFAPI_DEV},
    /* compatibility: object have been renamed to droplet */
    {"object", DeviceType::B_DROPLET_DEV},
    {"droplet", DeviceType::B_DROPLET_DEV},
    {"rados", DeviceType::B_RADOS_DEV},
    {"cephfs", DeviceType::B_CEPHFS_DEV},
    {nullptr, DeviceType::B_UNKNOWN_DEV}};

struct s_io_kw {
  const char* name;
  AutoXflateMode token;
};

static s_io_kw io_directions[] = {{"in", AutoXflateMode::IO_DIRECTION_IN},
                                  {"out", AutoXflateMode::IO_DIRECTION_OUT},
                                  {"both", AutoXflateMode::IO_DIRECTION_INOUT},
                                  {nullptr, AutoXflateMode::IO_DIRECTION_NONE}};

static s_kw compression_algorithms[] = {
    {"gzip", COMPRESS_GZIP},   {"lzo", COMPRESS_LZO1X},
    {"lzfast", COMPRESS_FZFZ}, {"lz4", COMPRESS_FZ4L},
    {"lz4hc", COMPRESS_FZ4H},  {NULL, 0}};

static void StoreAuthenticationType(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both pass 1 and pass 2
   */
  for (i = 0; authentication_methods[i].name; i++) {
    if (Bstrcasecmp(lc->str, authentication_methods[i].name)) {
      SetItemVariable<uint32_t>(*item, authentication_methods[i].token);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Authentication Type keyword, got: %s"),
              lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * Store password either clear if for NDMP or MD5 hashed for native.
 */
static void StoreAutopassword(LEX* lc, ResourceItem* item, int index, int pass)
{
  switch ((*item->allocated_resource)->rcode_) {
    case R_DIRECTOR:
      /*
       * As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1.
       */
      switch (item->code) {
        case 1:
          my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index,
                                   pass);
          break;
        default:
          my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_NDMP:
      my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
    default:
      my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
  }
}

static void StoreDeviceType(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the label pass 2 so that type is defined
   */
  for (i = 0; device_types[i].name; i++) {
    if (Bstrcasecmp(lc->str, device_types[i].name)) {
      SetItemVariable<DeviceType>(*item, device_types[i].token);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Device Type keyword, got: %s"), lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * Store Maximum Block Size, and check it is not greater than MAX_BLOCK_LENGTH
 */
static void StoreMaxblocksize(LEX* lc, ResourceItem* item, int index, int pass)
{
  my_config->StoreResource(CFG_TYPE_SIZE32, lc, item, index, pass);
  if (GetItemVariable<uint32_t>(*item) > MAX_BLOCK_LENGTH) {
    scan_err2(lc,
              _("Maximum Block Size configured value %u is greater than "
                "allowed maximum: %u"),
              GetItemVariable<uint32_t>(*item), MAX_BLOCK_LENGTH);
  }
}

/**
 * Store the IO direction on a certain device.
 */
static void StoreIoDirection(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  for (i = 0; io_directions[i].name; i++) {
    if (Bstrcasecmp(lc->str, io_directions[i].name)) {
      SetItemVariable<AutoXflateMode>(*item, io_directions[i].token);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a IO direction keyword, got: %s"), lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * Store the compression algorithm to use on a certain device.
 */
static void StoreCompressionalgorithm(LEX* lc,
                                      ResourceItem* item,
                                      int index,
                                      int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  for (i = 0; compression_algorithms[i].name; i++) {
    if (Bstrcasecmp(lc->str, compression_algorithms[i].name)) {
      SetItemVariable<uint32_t>(*item,
                                compression_algorithms[i].token & 0xffffffff);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Compression algorithm keyword, got: %s"),
              lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.c, function InitResource, for more generic handling.
 */
static void InitResourceCb(ResourceItem* item, int pass)
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

static void ParseConfigCb(LEX* lc, ResourceItem* item, int index, int pass)
{
  switch (item->type) {
    case CFG_TYPE_AUTOPASSWORD:
      StoreAutopassword(lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHTYPE:
      StoreAuthenticationType(lc, item, index, pass);
      break;
    case CFG_TYPE_DEVTYPE:
      StoreDeviceType(lc, item, index, pass);
      break;
    case CFG_TYPE_MAXBLOCKSIZE:
      StoreMaxblocksize(lc, item, index, pass);
      break;
    case CFG_TYPE_IODIRECTION:
      StoreIoDirection(lc, item, index, pass);
      break;
    case CFG_TYPE_CMPRSALGO:
      StoreCompressionalgorithm(lc, item, index, pass);
      break;
    default:
      break;
  }
}

static void MultiplyDevice(DeviceResource& multiplied_device_resource)
{
  /* append 0001 to the name of the existing resource */
  multiplied_device_resource.CreateAndAssignSerialNumber(1);

  multiplied_device_resource.multiplied_device_resource =
      std::addressof(multiplied_device_resource);

  uint32_t count = multiplied_device_resource.count - 1;

  /* create the copied devices */
  for (uint32_t i = 0; i < count; i++) {
    DeviceResource* copied_device_resource =
        new DeviceResource(multiplied_device_resource);

    /* append 0002, 0003, ... */
    copied_device_resource->CreateAndAssignSerialNumber(i + 2);

    copied_device_resource->multiplied_device_resource =
        std::addressof(multiplied_device_resource);
    copied_device_resource->count = 0;

    my_config->AppendToResourcesChain(copied_device_resource,
                                      copied_device_resource->rcode_);

    if (copied_device_resource->changer_res) {
      if (copied_device_resource->changer_res->device_resources) {
        copied_device_resource->changer_res->device_resources->append(
            copied_device_resource);
      }
    }
  }
}

static void MultiplyConfiguredDevices(ConfigurationParser& my_config)
{
  BareosResource* p = nullptr;
  while ((p = my_config.GetNextRes(R_DEVICE, p))) {
    DeviceResource& d = dynamic_cast<DeviceResource&>(*p);
    if (d.count > 1) { MultiplyDevice(d); }
  }
}

static void ConfigBeforeCallback(ConfigurationParser& my_config)
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
  my_config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void CheckDropletDevices(ConfigurationParser& my_config)
{
  BareosResource* p = nullptr;

  while ((p = my_config.GetNextRes(R_DEVICE, p)) != nullptr) {
    DeviceResource* d = dynamic_cast<DeviceResource*>(p);
    if (d && d->dev_type == DeviceType::B_DROPLET_DEV) {
      if (d->max_concurrent_jobs == 0) {
        /*
         * 0 is the general default. However, for this dev_type, only 1 works.
         * So we set it to this value.
         */
        Jmsg1(nullptr, M_WARNING, 0,
              _("device %s is set to the default 'Maximum Concurrent Jobs' = "
                "1.\n"),
              d->device_name);
        d->max_concurrent_jobs = 1;
      } else if (d->max_concurrent_jobs > 1) {
        Jmsg2(nullptr, M_ERROR_TERM, 0,
              _("device %s is configured with 'Maximum Concurrent Jobs' = %d, "
                "however only 1 is supported.\n"),
              d->device_name, d->max_concurrent_jobs);
      }
    }
  }
}

static void ConfigReadyCallback(ConfigurationParser& my_config)
{
  MultiplyConfiguredDevices(my_config);
  CheckDropletDevices(my_config);
}

ConfigurationParser* InitSdConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, InitResourceCb, ParseConfigCb, nullptr,
      exit_code, R_FIRST, R_LAST, resources, res_head,
      default_config_filename.c_str(), "bareos-sd.d", ConfigBeforeCallback,
      ConfigReadyCallback, SaveResource, DumpResource, FreeResource);
  if (config) { config->r_own_ = R_STORAGE; }
  return config;
}

bool ParseSdConfig(const char* configfile, int exit_code)
{
  bool retval;

  retval = my_config->ParseConfig();

  if (retval) {
    me = (StorageResource*)my_config->GetNextRes(R_STORAGE, NULL);
    my_config->own_resource_ = me;
    if (!me) {
      Emsg1(exit_code, 0,
            _("No Storage resource defined in %s. Cannot continue.\n"),
            configfile);
      return retval;
    }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
    SetBackendDeviceDirectories(std::move(me->backend_directories));
#endif
  }

  return retval;
}

/**
 * Print configuration file schema in json format
 */
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  ResourceTable* resources = my_config->resources_;

  InitializeJson();

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-sd"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  /*
   * Resources
   */
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bareos_sd = json_object();
  json_object_set(resource, "bareos-sd", bareos_sd);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resources_[r];
    json_object_set(bareos_sd, resource.name, json_items(resource.items));
  }

  PmStrcat(buffer, json_dumps(json, JSON_INDENT(2)));
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
                          bool sendit(void* sock, const char* fmt, ...),
                          void* sock,
                          bool hide_sensitive_data,
                          bool verbose)
{
  PoolMem buf;
  bool recurse = true;
  OutputFormatter output_formatter =
      OutputFormatter(sendit, sock, nullptr, nullptr);
  OutputFormatterResource output_formatter_resource =
      OutputFormatterResource(&output_formatter);

  if (!res) {
    sendit(sock, _("Warning: no \"%s\" resource (%d) defined.\n"),
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
      AutochangerResource* autochanger =
          dynamic_cast<AutochangerResource*>(res);
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
                         bool sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  bool recurse = true;
  BareosResource* p = res;

  while (recurse && p) {
    recurse =
        DumpResource_(type, p, sendit, sock, hide_sensitive_data, verbose);
    p = p->next_;
  }
}

static bool SaveResource(int type, ResourceItem* items, int pass)
{
  int rindex = type - R_FIRST;
  int i;
  int error = 0;

  BareosResource* allocated_resource = *resources[rindex].allocated_resource_;
  if (pass == 2 && !allocated_resource->Validate()) {
    return false;
  }

  // Ensure that all required items are present
  for (i = 0; items[i].name; i++) {
    if (items[i].flags & CFG_ITEM_REQUIRED) {
      if (!BitIsSet(i, (*items[i].allocated_resource)->item_present_)) {
        Emsg2(M_ERROR_TERM, 0,
              _("\"%s\" item is required in \"%s\" resource, but not found.\n"),
              items[i].name, resources[rindex].name);
      }
    }

    if (i >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR_TERM, 0, _("Too many items in \"%s\" resource\n"),
            resources[rindex].name);
    }
  }

  // save previously discovered pointers into dynamic memory
  if (pass == 2) {
    switch (type) {
      case R_DEVICE:
      case R_MSGS:
      case R_NDMP:
        // Resources not containing a resource
        break;
      case R_DIRECTOR: {
        DirectorResource* p = dynamic_cast<DirectorResource*>(
            my_config->GetResWithName(R_DIRECTOR, res_dir->resource_name_));
        if (!p) {
          Emsg1(M_ERROR_TERM, 0, _("Cannot find Director resource %s\n"),
                res_dir->resource_name_);
        } else {
          p->tls_cert_.allowed_certificate_common_names_ =
              std::move(res_dir->tls_cert_.allowed_certificate_common_names_);
        }
        break;
      }
      case R_STORAGE: {
        StorageResource* p = dynamic_cast<StorageResource*>(
            my_config->GetResWithName(R_STORAGE, res_store->resource_name_));
        if (!p) {
          Emsg1(M_ERROR_TERM, 0, _("Cannot find Storage resource %s\n"),
                res_store->resource_name_);
        } else {
          p->plugin_names = res_store->plugin_names;
          p->messages = res_store->messages;
          p->backend_directories = res_store->backend_directories;
          p->tls_cert_.allowed_certificate_common_names_ =
              std::move(res_store->tls_cert_.allowed_certificate_common_names_);
        }
        break;
      }
      case R_AUTOCHANGER: {
        AutochangerResource* p =
            dynamic_cast<AutochangerResource*>(my_config->GetResWithName(
                R_AUTOCHANGER, res_changer->resource_name_));

        if (!p) {
          Emsg1(M_ERROR_TERM, 0, _("Cannot find AutoChanger resource %s\n"),
                res_changer->resource_name_);
        } else {
          p->device_resources = res_changer->device_resources;

          DeviceResource* q = nullptr;
          foreach_alist (q, p->device_resources) {
            q->changer_res = p;
          }

          int errstat;
          if ((errstat = RwlInit(&p->changer_lock, PRIO_SD_ACH_ACCESS)) != 0) {
            BErrNo be;
            Jmsg1(NULL, M_ERROR_TERM, 0, _("Unable to init lock: ERR=%s\n"),
                  be.bstrerror(errstat));
          }
        }
        break;
      }
      default:
        printf(_("Unknown resource type %d\n"), type);
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
    switch (resources[rindex].rcode) {
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
      if (p->pid_directory) { free(p->pid_directory); }
      if (p->subsys_directory) { free(p->subsys_directory); }
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
      if (p->device_name) { free(p->device_name); }
      if (p->device_options) { free(p->device_options); }
      if (p->diag_device_name) { free(p->diag_device_name); }
      if (p->changer_name) { free(p->changer_name); }
      if (p->changer_command) { free(p->changer_command); }
      if (p->alert_command) { free(p->alert_command); }
      if (p->spool_directory) { free(p->spool_directory); }
      if (p->mount_point) { free(p->mount_point); }
      if (p->mount_command) { free(p->mount_command); }
      if (p->unmount_command) { free(p->unmount_command); }
      if (p->write_part_command) { free(p->write_part_command); }
      if (p->free_space_command) { free(p->free_space_command); }
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
      Dmsg1(0, _("Unknown resource type %d\n"), type);
      break;
  }
  if (next_ressource) { my_config->FreeResourceCb_(next_ressource, type); }
}

} /* namespace storagedaemon  */
