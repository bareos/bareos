/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Configuration file parser for Bareos Storage daemon
 *
 * Kern Sibbald, March MM
 */

#define NEED_JANSSON_NAMESPACE 1
#include "bareos.h"
#include "stored.h"

/*
 * First and last resource ids
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

/*
 * Forward referenced subroutines
 */

/*
 * We build the current resource here statically,
 * then move it to dynamic memory
 */
static URES res_all;
static int32_t res_all_size = sizeof(res_all);

/*
 * Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/*
 * Globals for the Storage daemon.
 */
static RES_ITEM store_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_store.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_store.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "SdPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store.SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL },
   { "SdAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store.SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL },
   { "SdAddresses", CFG_TYPE_ADDRESSES, ITEM(res_store.SDaddrs), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL },
   { "SdSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store.SDsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_store.working_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_WORKINGDIR, NULL, NULL },
   { "PidDirectory", CFG_TYPE_DIR, ITEM(res_store.pid_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_PIDDIR, NULL, NULL },
   { "SubSysDirectory", CFG_TYPE_DIR, ITEM(res_store.subsys_directory), CFG_ITEM_DEPRECATED, 0, NULL, NULL, NULL },
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
   { "BackendDirectory", CFG_TYPE_ALIST_DIR, ITEM(res_store.backend_directories), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_BACKENDDIR, NULL, NULL },
#endif
   { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_store.plugin_directory), 0, 0, NULL, NULL, NULL },
   { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_store.plugin_names), 0, 0, NULL, NULL, NULL },
   { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_store.scripts_directory), 0, 0, NULL, NULL, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL },
   { "MaximumConnections", CFG_TYPE_PINT32, ITEM(res_store.MaxConnections), 0, CFG_ITEM_DEFAULT, "42", "15.2.3-", NULL },
   { "Messages", CFG_TYPE_RES, ITEM(res_store.messages), R_MSGS, 0, NULL, NULL, NULL },
   { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
   { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_store.FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_store.max_network_buffer_size), 0, 0, NULL, NULL, NULL },
   { "ClientConnectWait", CFG_TYPE_TIME, ITEM(res_store.client_wait), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
   { "VerId", CFG_TYPE_STR, ITEM(res_store.verid), 0, 0, NULL, NULL, NULL },
   { "Compatible", CFG_TYPE_BOOL, ITEM(res_store.compatible), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store.max_bandwidth_per_job), 0, 0, NULL, NULL, NULL },
   { "AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_store.allow_bw_bursting), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "NdmpEnable", CFG_TYPE_BOOL, ITEM(res_store.ndmp_enable), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_store.ndmp_snooping), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_store.ndmploglevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL },
   { "NdmpAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_store.NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL },
   { "NdmpAddresses", CFG_TYPE_ADDRESSES, ITEM(res_store.NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL },
   { "NdmpPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_store.NDMPaddrs), 0, CFG_ITEM_DEFAULT, "10000", NULL, NULL },
   { "AutoXFlateOnReplication", CFG_TYPE_BOOL, ITEM(res_store.autoxflateonreplication), 0, CFG_ITEM_DEFAULT, "false", "13.4.0-", NULL },
   { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_store.jcr_watchdog_time), 0, 0, NULL, NULL, NULL },
   { "CollectDeviceStatistics", CFG_TYPE_BOOL, ITEM(res_store.collect_dev_stats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "CollectJobStatistics", CFG_TYPE_BOOL, ITEM(res_store.collect_job_stats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_store.stats_collect_interval), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
   { "DeviceReserveByMediaType", CFG_TYPE_BOOL, ITEM(res_store.device_reserve_by_mediatype), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "FileDeviceConcurrentRead", CFG_TYPE_BOOL, ITEM(res_store.filedevice_concurrent_read), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_store.secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
     "Specify command that will be called when bareos unlinks files." },
   { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_store.log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL },
   TLS_CONFIG(res_store)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Directors that can speak to the Storage daemon
 */
static RES_ITEM dir_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Monitor", CFG_TYPE_BOOL, ITEM(res_dir.monitor), 0, 0, NULL, NULL, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir.max_bandwidth_per_job), 0, 0, NULL, NULL, NULL },
   { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.keyencrkey), 1, 0, NULL, NULL, NULL },
   TLS_CONFIG(res_dir)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * NDMP DMA's that can speak to the Storage daemon
 */
static RES_ITEM ndmp_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_ndmp.hdr.name), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_ndmp.hdr.desc), 0, 0, 0, NULL, NULL },
   { "Username", CFG_TYPE_STR, ITEM(res_ndmp.username), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_ndmp.password), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL },
   { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_ndmp.AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL },
   { "LogLevel", CFG_TYPE_PINT32, ITEM(res_ndmp.LogLevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, 0, NULL, NULL }
};

/*
 * Device definition
 */
static RES_ITEM dev_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_dev.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "Unique identifier of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_dev.hdr.desc), 0, 0, NULL, NULL,
     "The Description directive provides easier human recognition, but is not used by Bareos directly." },
   { "MediaType", CFG_TYPE_STRNAME, ITEM(res_dev.media_type), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "DeviceType", CFG_TYPE_DEVTYPE, ITEM(res_dev.dev_type), 0, 0, NULL, NULL, NULL },
   { "ArchiveDevice", CFG_TYPE_STRNAME, ITEM(res_dev.device_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "DeviceOptions", CFG_TYPE_STR, ITEM(res_dev.device_options), 0, 0, NULL, "15.2.0-", NULL },
   { "DiagnosticDevice", CFG_TYPE_STRNAME, ITEM(res_dev.diag_device_name), 0, 0, NULL, NULL, NULL },
   { "HardwareEndOfFile", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_EOF, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "HardwareEndOfMedium", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_EOM, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "BackwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_BSR, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "BackwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_BSF, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "BsfAtEom", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_BSFATEOM, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "TwoEof", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_TWOEOF, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "ForwardSpaceRecord", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_FSR, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "ForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_FSF, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "FastForwardSpaceFile", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_FASTFSF, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "RemovableMedia", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_REM, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "RandomAccess", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_RACCESS, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "AutomaticMount", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_AUTOMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "LabelMedia", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_LABEL, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "AlwaysOpen", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_ALWAYSOPEN, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "Autochanger", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_AUTOCHANGER, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "CloseOnPoll", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_CLOSEONPOLL, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "BlockPositioning", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_POSITIONBLOCKS, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "UseMtiocget", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_MTIOCGET, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "CheckLabels", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_CHECKLABELS, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "RequiresMount", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_REQMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "OfflineOnUnmount", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_OFFLINEUNMOUNT, CFG_ITEM_DEFAULT, "off", NULL, NULL },
   { "BlockChecksum", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_BLOCKCHECKSUM, CFG_ITEM_DEFAULT, "on", NULL, NULL },
   { "AutoSelect", CFG_TYPE_BOOL, ITEM(res_dev.autoselect), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_dev.changer_name), 0, 0, NULL, NULL, NULL },
   { "ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_dev.changer_command), 0, 0, NULL, NULL, NULL },
   { "AlertCommand", CFG_TYPE_STRNAME, ITEM(res_dev.alert_command), 0, 0, NULL, NULL, NULL },
   { "MaximumChangerWait", CFG_TYPE_TIME, ITEM(res_dev.max_changer_wait), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL },
   { "MaximumOpenWait", CFG_TYPE_TIME, ITEM(res_dev.max_open_wait), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL },
   { "MaximumOpenVolumes", CFG_TYPE_PINT32, ITEM(res_dev.max_open_vols), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_dev.max_network_buffer_size), 0, 0, NULL, NULL, NULL },
   { "VolumePollInterval", CFG_TYPE_TIME, ITEM(res_dev.vol_poll_interval), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL },
   { "MaximumRewindWait", CFG_TYPE_TIME, ITEM(res_dev.max_rewind_wait), 0, CFG_ITEM_DEFAULT, "300" /* 5 minutes */, NULL, NULL },
   { "LabelBlockSize", CFG_TYPE_PINT32, ITEM(res_dev.label_block_size), 0, CFG_ITEM_DEFAULT, "64512"/* DEFAULT_BLOCK_SIZE */, NULL, NULL },
   { "MinimumBlockSize", CFG_TYPE_PINT32, ITEM(res_dev.min_block_size), 0, 0, NULL, NULL, NULL },
   { "MaximumBlockSize", CFG_TYPE_MAXBLOCKSIZE, ITEM(res_dev.max_block_size), 0, 0, NULL, NULL, NULL },
   { "MaximumVolumeSize", CFG_TYPE_SIZE64, ITEM(res_dev.max_volume_size), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL },
   { "MaximumFileSize", CFG_TYPE_SIZE64, ITEM(res_dev.max_file_size), 0, CFG_ITEM_DEFAULT, "1000000000", NULL, NULL },
   { "VolumeCapacity", CFG_TYPE_SIZE64, ITEM(res_dev.volume_capacity), 0, 0, NULL, NULL, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dev.max_concurrent_jobs), 0, 0, NULL, NULL, NULL },
   { "SpoolDirectory", CFG_TYPE_DIR, ITEM(res_dev.spool_directory), 0, 0, NULL, NULL, NULL },
   { "MaximumSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev.max_spool_size), 0, 0, NULL, NULL, NULL },
   { "MaximumJobSpoolSize", CFG_TYPE_SIZE64, ITEM(res_dev.max_job_spool_size), 0, 0, NULL, NULL, NULL },
   { "DriveIndex", CFG_TYPE_PINT16, ITEM(res_dev.drive_index), 0, 0, NULL, NULL, NULL },
   { "MaximumPartSize", CFG_TYPE_SIZE64, ITEM(res_dev.max_part_size), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL },
   { "MountPoint", CFG_TYPE_STRNAME, ITEM(res_dev.mount_point), 0, 0, NULL, NULL, NULL },
   { "MountCommand", CFG_TYPE_STRNAME, ITEM(res_dev.mount_command), 0, 0, NULL, NULL, NULL },
   { "UnmountCommand", CFG_TYPE_STRNAME, ITEM(res_dev.unmount_command), 0, 0, NULL, NULL, NULL },
   { "WritePartCommand", CFG_TYPE_STRNAME, ITEM(res_dev.write_part_command), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL },
   { "FreeSpaceCommand", CFG_TYPE_STRNAME, ITEM(res_dev.free_space_command), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL },
   { "LabelType", CFG_TYPE_LABEL, ITEM(res_dev.label_type), 0, 0, NULL, NULL, NULL },
   { "NoRewindOnClose", CFG_TYPE_BOOL, ITEM(res_dev.norewindonclose), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "DriveTapeAlertEnabled", CFG_TYPE_BOOL, ITEM(res_dev.drive_tapealert_enabled), 0, 0, NULL, NULL, NULL },
   { "DriveCryptoEnabled", CFG_TYPE_BOOL, ITEM(res_dev.drive_crypto_enabled), 0, 0, NULL, NULL, NULL },
   { "QueryCryptoStatus", CFG_TYPE_BOOL, ITEM(res_dev.query_crypto_status), 0, 0, NULL, NULL, NULL },
   { "AutoDeflate", CFG_TYPE_IODIRECTION, ITEM(res_dev.autodeflate), 0, 0, NULL, "13.4.0-", NULL },
   { "AutoDeflateAlgorithm", CFG_TYPE_CMPRSALGO, ITEM(res_dev.autodeflate_algorithm), 0, 0, NULL, "13.4.0-", NULL },
   { "AutoDeflateLevel", CFG_TYPE_PINT16, ITEM(res_dev.autodeflate_level), 0, CFG_ITEM_DEFAULT, "6", "13.4.0-", NULL },
   { "AutoInflate", CFG_TYPE_IODIRECTION, ITEM(res_dev.autoinflate), 0, 0, NULL, "13.4.0-", NULL },
   { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_dev.collectstats), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Autochanger definition
 */
static RES_ITEM changer_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_changer.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_changer.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Device", CFG_TYPE_ALIST_RES, ITEM(res_changer.device), R_DEVICE, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_changer.changer_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "ChangerCommand", CFG_TYPE_STRNAME, ITEM(res_changer.changer_command), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

// { "mountanonymousvolumes", CFG_TYPE_BIT, ITEM(res_dev.cap_bits), CAP_ANONVOLS, CFG_ITEM_DEFAULT, "off" },

/*
 * Message resource
 */
#include "lib/msg_res.h"

/*
 * This is the master resource definition
 */
static RES_TABLE resources[] = {
   { "Director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { "Ndmp", ndmp_items, R_NDMP, sizeof(NDMPRES) },
   { "Storage", store_items, R_STORAGE, sizeof(STORES) },
   { "Device", dev_items, R_DEVICE, sizeof(DEVRES) },
   { "Messages", msgs_items, R_MSGS, sizeof(MSGSRES) },
   { "Autochanger", changer_items, R_AUTOCHANGER, sizeof(AUTOCHANGERRES)  },
   { NULL, NULL, 0 }
};

/*
 * Authentication methods
 */
static struct s_kw authmethods[] = {
   { "None", AT_NONE },
   { "Clear", AT_CLEAR },
   { "MD5", AT_MD5 },
   { NULL, 0 }
};

/*
 * Device types
 *
 * device type, device code = token
 */
static s_kw dev_types[] = {
   { "file", B_FILE_DEV },
   { "tape", B_TAPE_DEV },
   { "fifo", B_FIFO_DEV },
   { "vtl", B_VTL_DEV },
   { "gfapi", B_GFAPI_DEV },
   { "object", B_OBJECT_STORE_DEV },
   { "rados", B_RADOS_DEV },
   { "cephfs", B_CEPHFS_DEV },
   { "elasto", B_ELASTO_DEV },
   { NULL, 0 }
};

/*
 * IO directions.
 */
static s_kw io_directions[] = {
   { "in", IO_DIRECTION_IN },
   { "out", IO_DIRECTION_OUT },
   { "both", IO_DIRECTION_INOUT },
   { NULL, 0 }
};

/*
 * Compression algorithms
 */
static s_kw compression_algorithms[] = {
   { "gzip", COMPRESS_GZIP },
   { "lzo", COMPRESS_LZO1X },
   { "lzfast", COMPRESS_FZFZ },
   { "lz4", COMPRESS_FZ4L },
   { "lz4hc", COMPRESS_FZ4H },
   { NULL, 0 }
};

/*
 * Store authentication type (Mostly for NDMP like clear or MD5).
 */
static void store_authtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; authmethods[i].name; i++) {
      if (bstrcasecmp(lc->str, authmethods[i].name)) {
         *(uint32_t *)(item->value) = authmethods[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Authentication Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store password either clear if for NDMP or MD5 hashed for native.
 */
static void store_autopassword(LEX *lc, RES_ITEM *item, int index, int pass)
{
   switch (res_all.hdr.rcode) {
   case R_DIRECTOR:
      /*
       * As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1.
       */
      switch (item->code) {
      case 1:
         store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
         break;
      default:
         store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
         break;
      }
      break;
   case R_NDMP:
      store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
   default:
      store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
   }
}
/*
 * Store Device Type (File, FIFO, Tape)
 */
static void store_devtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the label pass 2 so that type is defined
    */
   for (i = 0; dev_types[i].name; i++) {
      if (bstrcasecmp(lc->str, dev_types[i].name)) {
         *(uint32_t *)(item->value) = dev_types[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Device Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Maximum Block Size, and check it is not greater than MAX_BLOCK_LENGTH
 */
static void store_maxblocksize(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_resource(CFG_TYPE_SIZE32, lc, item, index, pass);
   if (*(uint32_t *)(item->value) > MAX_BLOCK_LENGTH) {
      scan_err2(lc, _("Maximum Block Size configured value %u is greater than allowed maximum: %u"),
         *(uint32_t *)(item->value), MAX_BLOCK_LENGTH);
   }
}

/*
 * Store the IO direction on a certain device.
 */
static void store_io_direction(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   for (i = 0; io_directions[i].name; i++) {
      if (bstrcasecmp(lc->str, io_directions[i].name)) {
         *(uint16_t *)(item->value) = io_directions[i].token & 0xffff;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a IO direction keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store the compression algorithm to use on a certain device.
 */
static void store_compressionalgorithm(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   for (i = 0; compression_algorithms[i].name; i++) {
      if (bstrcasecmp(lc->str, compression_algorithms[i].name)) {
         *(uint32_t *)(item->value) = compression_algorithms[i].token & 0xffffffff;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Compression algorithm keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *reshdr,
                   void sendit(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data)
{
   POOL_MEM buf;
   URES *res = (URES *)reshdr;
   BRSRES *resclass;
   int recurse = 1;

   if (res == NULL) {
      sendit(sock, _("Warning: no \"%s\" resource (%d) defined.\n"), res_to_str(type), type);
      return;
   }

   if (type < 0) { /* no recursion */
      type = - type;
      recurse = 0;
   }

   switch (type) {
   case R_MSGS: {
      MSGSRES *resclass = (MSGSRES *)reshdr;
      resclass->print_config(buf);
      break;
   }
   default:
      resclass = (BRSRES *)reshdr;
      resclass->print_config(buf);
      break;
   }
   sendit(sock, "%s", buf.c_str());

   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, (RES *)res->res_dir.hdr.next, sendit, sock, hide_sensitive_data);
   }
}

/*
 * Free memory of resource.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(RES *sres, int type)
{
   RES *nres;
   URES *res = (URES *)sres;

   if (res == NULL)
      return;

   /*
    * Common stuff -- free the resource name
    */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }

   switch (type) {
   case R_DIRECTOR:
      if (res->res_dir.password.value) {
         free(res->res_dir.password.value);
      }
      if (res->res_dir.address) {
         free(res->res_dir.address);
      }
      if (res->res_dir.keyencrkey.value) {
         free(res->res_dir.keyencrkey.value);
      }
      free_tls_t(res->res_dir.tls);
      break;
   case R_NDMP:
      if (res->res_ndmp.username) {
         free(res->res_ndmp.username);
      }
      if (res->res_ndmp.password.value) {
         free(res->res_ndmp.password.value);
      }
      break;
   case R_AUTOCHANGER:
      if (res->res_changer.changer_name) {
         free(res->res_changer.changer_name);
      }
      if (res->res_changer.changer_command) {
         free(res->res_changer.changer_command);
      }
      if (res->res_changer.device) {
         delete res->res_changer.device;
      }
      rwl_destroy(&res->res_changer.changer_lock);
      break;
   case R_STORAGE:
      if (res->res_store.SDaddrs) {
         free_addresses(res->res_store.SDaddrs);
      }
      if (res->res_store.SDsrc_addr) {
         free_addresses(res->res_store.SDsrc_addr);
      }
      if (res->res_store.NDMPaddrs) {
         free_addresses(res->res_store.NDMPaddrs);
      }
      if (res->res_store.working_directory) {
         free(res->res_store.working_directory);
      }
      if (res->res_store.pid_directory) {
         free(res->res_store.pid_directory);
      }
      if (res->res_store.subsys_directory) {
         free(res->res_store.subsys_directory);
      }
      if (res->res_store.plugin_directory) {
         free(res->res_store.plugin_directory);
      }
      if (res->res_store.plugin_names) {
         delete res->res_store.plugin_names;
      }
      if (res->res_store.scripts_directory) {
         free(res->res_store.scripts_directory);
      }
      if (res->res_store.backend_directories) {
         delete res->res_store.backend_directories;
      }
      if (res->res_store.verid) {
         free(res->res_store.verid);
      }
      if (res->res_store.secure_erase_cmdline) {
         free(res->res_store.secure_erase_cmdline);
      }
      if (res->res_store.log_timestamp_format) {
         free(res->res_store.log_timestamp_format);
      }
      free_tls_t(res->res_store.tls);
      break;
   case R_DEVICE:
      if (res->res_dev.media_type) {
         free(res->res_dev.media_type);
      }
      if (res->res_dev.device_name) {
         free(res->res_dev.device_name);
      }
      if (res->res_dev.device_options) {
         free(res->res_dev.device_options);
      }
      if (res->res_dev.diag_device_name) {
         free(res->res_dev.diag_device_name);
      }
      if (res->res_dev.changer_name) {
         free(res->res_dev.changer_name);
      }
      if (res->res_dev.changer_command) {
         free(res->res_dev.changer_command);
      }
      if (res->res_dev.alert_command) {
         free(res->res_dev.alert_command);
      }
      if (res->res_dev.spool_directory) {
         free(res->res_dev.spool_directory);
      }
      if (res->res_dev.mount_point) {
         free(res->res_dev.mount_point);
      }
      if (res->res_dev.mount_command) {
         free(res->res_dev.mount_command);
      }
      if (res->res_dev.unmount_command) {
         free(res->res_dev.unmount_command);
      }
      if (res->res_dev.write_part_command) {
         free(res->res_dev.write_part_command);
      }
      if (res->res_dev.free_space_command) {
         free(res->res_dev.free_space_command);
      }
      break;
   case R_MSGS:
      if (res->res_msgs.mail_cmd) {
         free(res->res_msgs.mail_cmd);
      }
      if (res->res_msgs.operator_cmd) {
         free(res->res_msgs.operator_cmd);
      }
      if (res->res_msgs.timestamp_format) {
         free(res->res_msgs.timestamp_format);
      }
      free_msgs_res((MSGSRES *)res);  /* free message resource */
      res = NULL;
      break;
   default:
      Dmsg1(0, _("Unknown resource type %d\n"), type);
      break;
   }
   /*
    * Common stuff again -- free the resource, recurse to next one
    */
   if (res) {
      free(res);
   }
   if (nres) {
      free_resource(nres, type);
   }
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * or alist pointers.
 */
bool save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - R_FIRST;
   int i;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i = 0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
         if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
            Emsg2(M_ERROR_TERM, 0,
                  _("\"%s\" item is required in \"%s\" resource, but not found.\n"),
                  items[i].name, resources[rindex]);
          }
      }
      /*
       * If this triggers, take a look at lib/parse_conf.h
       */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR_TERM, 0, _("Too many items in \"%s\" resource\n"), resources[rindex]);
      }
   }

   /*
    * During pass 2, we looked up pointers to all the resources
    * referrenced in the current resource, , now we
    * must copy their address from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      DEVRES *dev;
      int errstat;

      switch (type) {
      case R_DEVICE:
      case R_MSGS:
      case R_NDMP:
         /*
          * Resources not containing a resource
          */
         break;
      case R_DIRECTOR:
         /*
          * Resources containing a resource or an alist
          */
         if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.name())) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.name());
         } else {
            res->res_dir.tls.allowed_cns = res_all.res_dir.tls.allowed_cns;
         }
         break;
      case R_STORAGE:
         if ((res = (URES *)GetResWithName(R_STORAGE, res_all.res_store.name())) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Storage resource %s\n"), res_all.res_store.name());
         } else {
            res->res_store.plugin_names = res_all.res_store.plugin_names;
            res->res_store.messages = res_all.res_store.messages;
            res->res_store.backend_directories = res_all.res_store.backend_directories;
            res->res_store.tls.allowed_cns = res_all.res_store.tls.allowed_cns;
         }
         break;
      case R_AUTOCHANGER:
         if ((res = (URES *)GetResWithName(type, res_all.res_changer.name())) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find AutoChanger resource %s\n"), res_all.res_changer.name());
         } else {
            /*
             * We must explicitly copy the device alist pointer
             */
            res->res_changer.device = res_all.res_changer.device;

            /*
             * Now update each device in this resource to point back to the changer resource.
             */
            foreach_alist(dev, res->res_changer.device) {
               dev->changer_res = (AUTOCHANGERRES *)&res->res_changer;
            }

            if ((errstat = rwl_init(&res->res_changer.changer_lock, PRIO_SD_ACH_ACCESS)) != 0) {
               berrno be;
               Jmsg1(NULL, M_ERROR_TERM, 0, _("Unable to init lock: ERR=%s\n"), be.bstrerror(errstat));
            }
         }
         break;
      default:
         printf(_("Unknown resource type %d\n"), type);
         error = 1;
         break;
      }

      if (res_all.res_dir.hdr.name) {
         free(res_all.res_dir.hdr.name);
         res_all.res_dir.hdr.name = NULL;
      }

      if (res_all.res_dir.hdr.desc) {
         free(res_all.res_dir.hdr.desc);
         res_all.res_dir.hdr.desc = NULL;
      }

      return (error == 0);
   }

   /*
    * Common
    */
   if (!error) {
      res = (URES *)malloc(resources[rindex].size);
      memcpy(res, &res_all, resources[rindex].size);
      if (!res_head[rindex]) {
         res_head[rindex] = (RES *)res; /* store first entry */
      } else {
         RES *next, *last;
         /*
          * Add new res to end of chain
          */
         for (last = next = res_head[rindex]; next; next = next->next) {
            last = next;
            if (bstrcmp(next->name, res->res_dir.name())) {
               Emsg2(M_ERROR_TERM, 0,
                  _("Attempt to define second \"%s\" resource named \"%s\" is not permitted.\n"),
                  resources[rindex].name, res->res_dir.name());
            }
         }
         last->next = (RES *)res;
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type), res->res_dir.name());
      }
   }
   return (error == 0);
}

/*
 * callback function for init_resource
 * See ../lib/parse_conf.c, function init_resource, for more generic handling.
 */
static void init_resource_cb(RES_ITEM *item, int pass)
{
   switch (pass) {
   case 1:
      switch (item->type) {
      case CFG_TYPE_AUTHTYPE:
         for (int i = 0; authmethods[i].name; i++) {
            if (bstrcasecmp(item->default_value, authmethods[i].name)) {
               *(uint32_t *)(item->value) = authmethods[i].token;
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

/*
 * callback function for parse_config
 * See ../lib/parse_conf.c, function parse_config, for more generic handling.
 */
static void parse_config_cb(LEX *lc, RES_ITEM *item, int index, int pass)
{
   switch (item->type) {
   case CFG_TYPE_AUTOPASSWORD:
      store_autopassword(lc, item, index, pass);
      break;
   case CFG_TYPE_AUTHTYPE:
      store_authtype(lc, item, index, pass);
      break;
   case CFG_TYPE_DEVTYPE:
      store_devtype(lc, item, index, pass);
      break;
   case CFG_TYPE_MAXBLOCKSIZE:
      store_maxblocksize(lc, item, index, pass);
      break;
   case CFG_TYPE_IODIRECTION:
      store_io_direction(lc, item, index, pass);
      break;
   case CFG_TYPE_CMPRSALGO:
      store_compressionalgorithm(lc, item, index, pass);
      break;
   default:
      break;
   }
}

void init_sd_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile,
                NULL,
                NULL,
                init_resource_cb,
                parse_config_cb,
                NULL,
                exit_code,
                (void *)&res_all,
                res_all_size,
                R_FIRST,
                R_LAST,
                resources,
                res_head);
   config->set_default_config_filename(CONFIG_FILE);
   config->set_config_include_dir("bareos-sd.d");
}

bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code)
{
   bool retval;

   init_sd_config(config, configfile, exit_code);
   retval = config->parse_config();

   if (retval) {
      me = (STORES *)GetNextRes(R_STORAGE, NULL);
      if (!me) {
         Emsg1(exit_code, 0, _("No Storage resource defined in %s. Cannot continue.\n"), configfile);
         return retval;
      }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
      sd_set_backend_dirs(me->backend_directories);
#endif
   }

   return retval;
}

/*
 * Print configuration file schema in json format
 */
#ifdef HAVE_JANSSON
bool print_config_schema_json(POOL_MEM &buffer)
{
   RES_TABLE *resources = my_config->m_resources;

   initialize_json();

   json_t *json = json_object();
   json_object_set_new(json, "format-version", json_integer(2));
   json_object_set_new(json, "component", json_string("bareos-sd"));
   json_object_set_new(json, "version", json_string(VERSION));

   /*
    * Resources
    */
   json_t *resource = json_object();
   json_object_set(json, "resource", resource);
   json_t *bareos_sd = json_object();
   json_object_set(resource, "bareos-sd", bareos_sd);

   for (int r = 0; resources[r].name; r++) {
      RES_TABLE resource = my_config->m_resources[r];
      json_object_set(bareos_sd, resource.name, json_items(resource.items));
   }

   pm_strcat(buffer, json_dumps(json, JSON_INDENT(2)));
   json_decref(json);

   return true;
}
#else
bool print_config_schema_json(POOL_MEM &buffer)
{
   pm_strcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");
   return false;
}
#endif
