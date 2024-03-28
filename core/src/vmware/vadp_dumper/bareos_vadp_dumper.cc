/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2024 Bareos GmbH & Co. KG
   Copyright (C) 2015-2015 Planets Communications B.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.vadp.

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
 * VADP Dumper - vStorage APIs for Data Protection Dumper program.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <signal.h>
#include <vector>
#include <cerrno>
#include <algorithm>

#include "copy_thread.h"

#include <jansson.h>

/*
 * json_array_foreach macro was added in jansson version 2.5
 * we can compile also against an lower version if we define it ourselves.
 */
#if JANSSON_VERSION_HEX < 0x020500
#  define json_array_foreach(array, index, value)              \
    for (index = 0; index < json_array_size(array)             \
                    && (value = json_array_get(array, index)); \
         index++)
#endif

extern "C" {
#include <vixDiskLib.h>
}

/* This is the minimum version we require, i.e. vSphere 6.5 or later */
#define VIXDISKLIB_VERSION_MAJOR 6
#define VIXDISKLIB_VERSION_MINOR 5

#define VSPHERE_DEFAULT_ADMIN_PORT 0

// VixDiskLib does all processing in sectors of 512 bytes.
#define DEFAULT_SECTOR_SIZE VIXDISKLIB_SECTOR_SIZE

/*
 * In each call to the VixDiskLib read/write this number of sectors.
 * e.g. 512 means 256 Kb per call (e.g. 512 x 512 bytes);
 * Actual value may be changed at runtime with the -s option.
 */
#define DEFAULT_SECTORS_PER_CALL 1024

#define MIN(a, b) ((a) < b) ? (a) : (b)
#define MAX(a, b) ((a) > b) ? (a) : (b)

#define CON_PARAMS_KEY "ConnParams"
#define CON_PARAMS_VM_MOREF_KEY "VmMoRef"
#define CON_PARAMS_HOST_KEY "VsphereHostName"
#define CON_PARAMS_THUMBPRINT_KEY "VsphereThumbPrint"
#define CON_PARAMS_USERNAME_KEY "VsphereUsername"
#define CON_PARAMS_PASSWORD_KEY "VspherePassword"
#define CON_PARAMS_SNAPSHOT_MOREF_KEY "VsphereSnapshotMoRef"

#define DISK_PARAMS_KEY "DiskParams"
#define DISK_PARAMS_DISK_PATH_KEY "diskPath"

#define CBT_DISKCHANGEINFO_KEY "DiskChangeInfo"
#define CBT_DISK_SIZE "length"
#define CBT_CHANGEDAREA_KEY "changedArea"
#define CBT_CHANGEDAREA_START_KEY "start"
#define CBT_CHANGEDAREA_LENGTH_KEY "length"
#define CBT_START_OFFSET "startOffset"

#define BAREOSMAGIC 0x12122012u
#define PROTOCOL_VERSION 1

#define BAREOS_VADPDUMPER_IDENTITY "BareosVADPDumper"

struct disk_type {
  const char* type;
  VixDiskLibDiskType vadp_type;
};

static disk_type disk_types[]
    = {{"monolithic_sparse", VIXDISKLIB_DISK_MONOLITHIC_SPARSE},
       {"monolithic_flat", VIXDISKLIB_DISK_MONOLITHIC_FLAT},
       {"split_sparse", VIXDISKLIB_DISK_SPLIT_SPARSE},
       {"split_flat", VIXDISKLIB_DISK_SPLIT_FLAT},
       {"vmfs_flat", VIXDISKLIB_DISK_VMFS_FLAT},
       {"optimized", VIXDISKLIB_DISK_STREAM_OPTIMIZED},
       {"vmfs_thin", VIXDISKLIB_DISK_VMFS_THIN},
       {"vmfs_sparse", VIXDISKLIB_DISK_VMFS_SPARSE},
       {NULL, VIXDISKLIB_DISK_UNKNOWN}};

/*
 * Generic identification structure, 128 bytes with padding.
 * This includes a protocol version.
 */
struct runtime_disk_info_encoding {
  uint32_t start_magic;
  uint32_t protocol_version;
  uint64_t absolute_disk_length;
  uint64_t absolute_start_offset;
  uint32_t bios_cylinders;
  uint32_t bios_heads;
  uint32_t bios_sectors;
  uint32_t phys_cylinders;
  uint32_t phys_heads;
  uint32_t phys_sectors;
  uint64_t phys_capacity;
  uint32_t adapter_type;
  uint32_t padding[16];
  uint32_t end_magic;
};
inline constexpr int rdie_size = sizeof(runtime_disk_info_encoding);

/*
 * Disk Meta data structure,
 * Encodes what follows e.g. meta_key and meta_data.
 * e.g. [META_META_DATA] [META_DATA_KEY] [META_DATA] ...
 */
struct runtime_meta_data_encoding {
  uint32_t start_magic;
  uint32_t meta_key_length;
  uint32_t meta_data_length;
  uint32_t end_magic;
};
inline constexpr int rmde_size = sizeof(runtime_meta_data_encoding);

/*
 * Changed Block Tracking structure.
 * Encodes the CBT data followed by the actual data.
 * e.g. [CBT] [DATA] ...
 */
struct runtime_cbt_encoding {
  uint32_t start_magic;
  uint64_t start_offset;
  uint64_t offset_length;
  uint32_t end_magic;
};
inline constexpr int rce_size = sizeof(runtime_cbt_encoding);

static bool cleanup_on_start = false;
static bool cleanup_on_disconnect = false;
static bool save_metadata = false;
static bool verbose = false;
static bool check_size = true;
static bool create_disk = false;
static bool local_vmdk = false;
static bool do_query_allocated = true;
static uint64 ChunkSize = VIXDISKLIB_MIN_CHUNK_SIZE;
static bool multi_threaded = false;
static bool restore_meta_data = false;
static uint64_t sectors_per_call = DEFAULT_SECTORS_PER_CALL;
static uint64_t absolute_start_offset = 0;
static char* vmdk_disk_name = NULL;
static char* raw_disk_name = NULL;
static int raw_disk_fd = -1;
static char* force_transport = NULL;
static char* disktype = NULL;
static VixDiskLibConnectParams* cnxParams = nullptr;
static VixDiskLibConnection connection = NULL;
static VixDiskLibHandle read_diskHandle = NULL;
static VixDiskLibHandle write_diskHandle = NULL;
static VixDiskLibInfo* info = NULL;
static json_t* json_config = NULL;
static int exit_code = 1;

// Encode the VDDK VixDiskLibInfo into an internal representation.
static inline void fill_runtime_disk_info_encoding(
    runtime_disk_info_encoding* rdie)
{
  memset(rdie, 0, rdie_size);

  rdie->protocol_version = PROTOCOL_VERSION;
  rdie->start_magic = BAREOSMAGIC;
  rdie->end_magic = BAREOSMAGIC;

  if (info->biosGeo.cylinders > 0) {
    rdie->bios_cylinders = info->biosGeo.cylinders;
  } else {
    rdie->bios_cylinders = info->physGeo.cylinders;
  }
  if (info->biosGeo.heads > 0) {
    rdie->bios_heads = info->biosGeo.heads;
  } else {
    rdie->bios_heads = info->physGeo.heads;
  }
  if (info->biosGeo.sectors > 0) {
    rdie->bios_sectors = info->biosGeo.sectors;
  } else {
    rdie->bios_sectors = info->physGeo.sectors;
  }

  rdie->phys_cylinders = info->physGeo.cylinders;
  rdie->phys_heads = info->physGeo.heads;
  rdie->phys_sectors = info->physGeo.sectors;

  rdie->phys_capacity = info->capacity;
  rdie->adapter_type = info->adapterType;
}

/*
 * Dump the important content of the internal disk representation for verbose
 * mode.
 */
static inline void dump_runtime_disk_info_encoding(
    runtime_disk_info_encoding* rdie)
{
  fprintf(stderr, "Protocol version = %u\n", rdie->protocol_version);
  fprintf(stderr, "Absolute disk length = %lu\n", rdie->absolute_disk_length);
  fprintf(stderr, "Absolute start offset = %lu\n", rdie->absolute_start_offset);
  fprintf(stderr, "BIOS geometry (%u cyl, %u heads, %u sectors)\n",
          rdie->bios_cylinders, rdie->bios_heads, rdie->bios_sectors);
  fprintf(stderr, "PHYS geometry (%u cyl, %u heads, %u sectors)\n",
          rdie->phys_cylinders, rdie->phys_heads, rdie->phys_sectors);
  fprintf(stderr, "Physical capacity %lu\n", rdie->phys_capacity);
  fprintf(stderr, "Adapter Type %u\n", rdie->adapter_type);
}

/*
 * Validate the disk sizes from the internal disk representation to the current
 * VDMK settings.
 */
static inline char validate_runtime_disk_info_encoding(
    runtime_disk_info_encoding* rdie)
{
  if (info->biosGeo.cylinders > 0
      && info->biosGeo.cylinders < rdie->bios_cylinders) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u BIOS "
            "cylinders original had %u\n",
            info->biosGeo.cylinders, rdie->bios_cylinders);
    goto bail_out;
  }

  if (info->biosGeo.heads > 0 && info->biosGeo.heads < rdie->bios_heads) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u BIOS heads "
            "original had %u\n",
            info->biosGeo.heads, rdie->bios_heads);
    goto bail_out;
  }

  if (info->biosGeo.cylinders > 0
      && info->biosGeo.cylinders < rdie->bios_cylinders) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u BIOS "
            "sectors original had %u\n",
            info->biosGeo.sectors, rdie->bios_sectors);
    goto bail_out;
  }

  if (info->physGeo.cylinders < rdie->phys_cylinders) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u PHYS "
            "cylinders original had %u\n",
            info->physGeo.cylinders, rdie->phys_cylinders);
    goto bail_out;
  }

  if (info->physGeo.heads < rdie->phys_heads) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u PHYS heads "
            "original had %u\n",
            info->biosGeo.heads, rdie->phys_heads);
    goto bail_out;
  }

  if (info->physGeo.cylinders < rdie->phys_cylinders) {
    fprintf(stderr,
            "[validate_runtime_disk_info_encoding] New disk has %u PHYS "
            "sectors original had %u\n",
            info->biosGeo.sectors, rdie->phys_sectors);
    goto bail_out;
  }

  return 1;

bail_out:
  return 0;
}

// Writer function that handles partial writes.
static inline size_t robust_writer(int fd, void* buffer, int size)
{
  if (size == 0) { return 0; }
  size_t total_bytes = 0;
  ssize_t cnt = 0;

  do {
    cnt = write(fd, (char*)buffer + total_bytes, size);
    if (cnt > 0) {
      size -= cnt;
      total_bytes += cnt;
    } else if (cnt < 0) {
      fprintf(stderr, "[robust_writer] Encountered write error: %d ERR=%s\n",
              errno, strerror(errno));
      return 0;
    }
  } while (size > 0 && cnt > 0);

  return total_bytes;
}

// Reader function that handles partial reads.
static inline size_t robust_reader(int fd, void* buffer, int size)
{
  if (size == 0) { return 0; }

  size_t total_bytes = 0;
  ssize_t cnt = 0;

  do {
    cnt = read(fd, (char*)buffer + total_bytes, size);
    if (cnt > 0) {
      size -= cnt;
      total_bytes += cnt;
    } else if (cnt < 0) {
      fprintf(stderr, "[robust_reader] Encountered read error: %d ERR=%s\n",
              errno, strerror(errno));
      return 0;
    }
  } while (size > 0 && cnt > 0);

  return total_bytes;
}

// VDDK helper functions.
static void LogFunction(const char* fmt, va_list args)
{
  if (verbose) {
    fprintf(stderr, "Log: ");
    vfprintf(stderr, fmt, args);
  }
}

static void WarningFunction(const char* fmt, va_list args)
{
  fprintf(stderr, "Warning: ");
  vfprintf(stderr, fmt, args);
}

static void PanicFunction(const char* fmt, va_list args)
{
  fprintf(stderr, "Log: ");
  vfprintf(stderr, fmt, args);
  exit_code = 10;
  exit(exit_code);
}

static inline void cleanup_cnxParams()
{
  if (!cnxParams) { return; }

  if (cnxParams->vmxSpec) {
    free(cnxParams->vmxSpec);
    cnxParams->vmxSpec = nullptr;
  }

  if (cnxParams->serverName) {
    free(cnxParams->serverName);
    cnxParams->serverName = nullptr;
  }

  if (cnxParams->creds.uid.userName) {
    free(cnxParams->creds.uid.userName);
    cnxParams->creds.uid.userName = nullptr;
  }

  if (cnxParams->creds.uid.password) {
    free(cnxParams->creds.uid.password);
    cnxParams->creds.uid.password = nullptr;
  }

  if (cnxParams->thumbPrint) {
    free(cnxParams->thumbPrint);
    cnxParams->thumbPrint = nullptr;
  }

  VixDiskLib_FreeConnectParams(cnxParams);
  cnxParams = nullptr;
}

static inline void cleanup_vixdisklib()
{
  uint32_t numCleanedUp, numRemaining;

  VixDiskLib_Cleanup(cnxParams, &numCleanedUp, &numRemaining);
}

// Generic cleanup function.
static void cleanup(void)
{
  VixError err;

  if (info) { VixDiskLib_FreeInfo(info); }

  if (read_diskHandle) {
    VixDiskLib_Close(read_diskHandle);
    read_diskHandle = NULL;
  }

  if (write_diskHandle) {
    VixDiskLib_Close(write_diskHandle);
    write_diskHandle = NULL;
  }

  if (connection) {
    VixDiskLib_Disconnect(connection);
    if (cleanup_on_disconnect) { cleanup_vixdisklib(); }
  }

  if (!local_vmdk) {
    err = VixDiskLib_EndAccess(cnxParams, BAREOS_VADPDUMPER_IDENTITY);
    if (VIX_FAILED(err)) {
      char* error_txt;

      error_txt = VixDiskLib_GetErrorText(err, NULL);
      fprintf(stderr, "Failed to End Access: %s [%lu]\n", error_txt, err);
      VixDiskLib_FreeErrorText(error_txt);
    }
  }

  if (raw_disk_fd != -1) {
    if (verbose) { fprintf(stderr, "Log: RAWFILE: Closing RAW file\n"); }
    close(raw_disk_fd);
  }

  cleanup_cnxParams();

  VixDiskLib_Exit();

  if (json_config) { json_decref(json_config); }

  if (vmdk_disk_name) { free(vmdk_disk_name); }

  if (force_transport) { free(force_transport); }

  if (disktype) { free(disktype); }

  _exit(exit_code);
}

// Convert the configured disktype to the right VADP type.
static inline VixDiskLibDiskType lookup_disktype()
{
  int cnt;

  for (cnt = 0; disk_types[cnt].type; cnt++) {
    if (!strcasecmp(disk_types[cnt].type, disktype)) { break; }
  }

  if (!disk_types[cnt].type) {
    fprintf(stderr, "Unknown disktype %s\n", disktype);
    exit(1);
  }

  return disk_types[cnt].vadp_type;
}

// Connect using VDDK to a VSPHERE server.
static inline void do_vixdisklib_connect(const char* key,
                                         json_t* connect_params,
                                         bool readonly,
                                         bool need_snapshot_moref)
{
  int succeeded = 0;
  VixError err;
  const char* snapshot_moref = NULL;

  cnxParams = VixDiskLib_AllocateConnectParams();

  if (!cnxParams) {
    fprintf(stderr, "Failed to allocate vixdisklib connection params.\n");
    goto bail_out;
  }

  err = VixDiskLib_InitEx(VIXDISKLIB_VERSION_MAJOR, VIXDISKLIB_VERSION_MINOR,
                          LogFunction, WarningFunction, PanicFunction,
                          "/usr/lib/vmware-vix-disklib", NULL);

  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "Failed to initialize vixdisklib %s [%lu]\n", error_txt,
            err);
    VixDiskLib_FreeErrorText(error_txt);
    goto bail_out;
  }

  // Start extracting the wanted information from the JSON passed in.
  if (!local_vmdk) {
    json_t* object;

    object = json_object_get(connect_params, CON_PARAMS_VM_MOREF_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              CON_PARAMS_VM_MOREF_KEY, key);
      goto bail_out;
    }
    cnxParams->specType = VIXDISKLIB_SPEC_VMX;
    cnxParams->vmxSpec = strdup(json_string_value(object));
    if (!cnxParams->vmxSpec) {
      fprintf(stderr, "Failed to allocate memory for holding %s\n",
              CON_PARAMS_VM_MOREF_KEY);
      goto bail_out;
    }

    object = json_object_get(connect_params, CON_PARAMS_HOST_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              CON_PARAMS_HOST_KEY, key);
      goto bail_out;
    }
    cnxParams->serverName = strdup(json_string_value(object));
    if (!cnxParams->serverName) {
      fprintf(stderr, "Failed to allocate memory for holding %s\n",
              CON_PARAMS_HOST_KEY);
      goto bail_out;
    }

    object = json_object_get(connect_params, CON_PARAMS_THUMBPRINT_KEY);
    if (object) {
      cnxParams->thumbPrint = strdup(json_string_value(object));
      if (!cnxParams->thumbPrint) {
        fprintf(stderr, "Failed to allocate memory for holding %s\n",
                CON_PARAMS_USERNAME_KEY);
        goto bail_out;
      }
    }

    object = json_object_get(connect_params, CON_PARAMS_USERNAME_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              CON_PARAMS_USERNAME_KEY, key);
      goto bail_out;
    }
    cnxParams->credType = VIXDISKLIB_CRED_UID;
    cnxParams->creds.uid.userName = strdup(json_string_value(object));
    if (!cnxParams->creds.uid.userName) {
      fprintf(stderr, "Failed to allocate memory for holding %s\n",
              CON_PARAMS_USERNAME_KEY);
      goto bail_out;
    }


    object = json_object_get(connect_params, CON_PARAMS_PASSWORD_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              CON_PARAMS_PASSWORD_KEY, key);
      goto bail_out;
    }
    cnxParams->creds.uid.password = strdup(json_string_value(object));
    if (!cnxParams->creds.uid.password) {
      fprintf(stderr, "Failed to allocate memory for holding %s\n",
              CON_PARAMS_PASSWORD_KEY);
      goto bail_out;
    }
    cnxParams->port = VSPHERE_DEFAULT_ADMIN_PORT;

    if (need_snapshot_moref) {
      object = json_object_get(connect_params, CON_PARAMS_SNAPSHOT_MOREF_KEY);
      if (!object) {
        fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
                CON_PARAMS_SNAPSHOT_MOREF_KEY, key);
        goto bail_out;
      }
      snapshot_moref = json_string_value(object);
    }

    if (!local_vmdk) {
      err = VixDiskLib_PrepareForAccess(cnxParams, BAREOS_VADPDUMPER_IDENTITY);
      if (VIX_FAILED(err)) {
        char* error_txt;

        error_txt = VixDiskLib_GetErrorText(err, NULL);
        fprintf(stderr, "Failed to Prepare For Access: %s [%lu]\n", error_txt,
                err);
        VixDiskLib_FreeErrorText(error_txt);
      }
    }
  }

  err = VixDiskLib_ConnectEx(cnxParams, (readonly) ? TRUE : FALSE,
                             snapshot_moref, force_transport, &connection);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "Failed to connect to %s : %s [%lu]\n",
            cnxParams->serverName, error_txt, err);
    VixDiskLib_FreeErrorText(error_txt);
    goto bail_out;
  }

  // Register our exit handler.
  atexit(cleanup);

  succeeded = 1;

bail_out:
  if (!succeeded) { exit(1); }
}

// Open a VMDK using VDDK.
static inline void do_vixdisklib_open(const char* key,
                                      const char* disk_name,
                                      json_t* disk_params,
                                      bool readonly,
                                      bool getdiskinfo,
                                      VixDiskLibHandle* diskHandle)
{
  VixError err;
  const char* disk_path;
  uint32_t flags;

  if (!disk_name) {
    json_t* object;

    // Start extracting the wanted information from the JSON passed in.
    object = json_object_get(disk_params, DISK_PARAMS_DISK_PATH_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              DISK_PARAMS_DISK_PATH_KEY, key);
      exit(1);
    }

    disk_path = json_string_value(object);
  } else {
    disk_path = vmdk_disk_name;
  }

  flags = 0;
  if (readonly) { flags |= VIXDISKLIB_FLAG_OPEN_READ_ONLY; }

  err = VixDiskLib_Open(connection, disk_path, flags, diskHandle);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "Failed to open %s : %s [%lu]\n", disk_path, error_txt,
            err);
    VixDiskLib_FreeErrorText(error_txt);
    exit(1);
  }

  if (getdiskinfo) {
    // See how big the logical disk is.
    err = VixDiskLib_GetInfo(*diskHandle, &info);
    if (VIX_FAILED(err)) {
      char* error_txt;

      error_txt = VixDiskLib_GetErrorText(err, NULL);
      fprintf(stderr, "Failed to get Logical Disk Info for %s, %s [%lu]\n",
              disk_path, error_txt, err);
      VixDiskLib_FreeErrorText(error_txt);
      exit(1);
    }
#ifdef VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE
    if (verbose) {
      fprintf(stderr, "DiskInfo logicalSectorSize: %u\n",
              info->logicalSectorSize);
      fprintf(stderr, "DiskInfo physicalSectorSize: %u\n",
              info->physicalSectorSize);
      fprintf(stderr, "DiskInfo capacity: %lu\n", info->capacity);
    }
#endif
  }

  if (verbose) {
    fprintf(stderr, "Selected transport method: %s\n",
            VixDiskLib_GetTransportMode(*diskHandle));
  }
}

// Create a VMDK using VDDK.
static inline void do_vixdisklib_create(const char* key,
                                        const char* disk_name,
                                        json_t* disk_params,
                                        uint64_t absolute_disk_length)
{
  int succeeded = 0;
  VixError err;
  const char* disk_path;
  VixDiskLibCreateParams createParams;

  if (!local_vmdk) {
    fprintf(stderr, "Cannot create a remote disk via VADP\n");
    goto bail_out;
  }

  if (!disk_name) {
    json_t* object;

    // Start extracting the wanted information from the JSON passed in.
    object = json_object_get(disk_params, DISK_PARAMS_DISK_PATH_KEY);
    if (!object) {
      fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
              DISK_PARAMS_DISK_PATH_KEY, key);
      goto bail_out;
    }

    disk_path = json_string_value(object);
  } else {
    disk_path = vmdk_disk_name;
  }

  createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC;
  createParams.capacity = (absolute_disk_length / VIXDISKLIB_SECTOR_SIZE);
  if (disktype) {
    createParams.diskType = lookup_disktype();
  } else {
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_SPARSE;
  }
#ifdef VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE
  createParams.physicalSectorSize = VIXDISKLIB_SECTOR_SIZE;
  createParams.logicalSectorSize = VIXDISKLIB_SECTOR_SIZE;
#endif
  createParams.hwVersion = 7; /* for ESX(i)4 */
  err = VixDiskLib_Create(connection, disk_path, &createParams, NULL, NULL);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "Failed to create Logical Disk for %s, %s [%lu]\n",
            disk_path, error_txt, err);
    VixDiskLib_FreeErrorText(error_txt);
    goto bail_out;
  }

  succeeded = 1;

bail_out:
  if (!succeeded) { exit(1); }
}

// Read data from a VMDK using the VDDP functions.
static size_t read_from_vmdk(size_t sector_offset, size_t nbyte, void* buf)
{
  VixError err;

  err = VixDiskLib_Read(read_diskHandle, sector_offset,
                        nbyte / DEFAULT_SECTOR_SIZE, (uint8*)buf);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "VMDK Read error: %s [%lu]\n", error_txt, err);
    return -1;
  }

  return nbyte;
}

// Write data to a VMDK using the VDDP functions.
static size_t write_to_vmdk(size_t sector_offset, size_t nbyte, void* buf)
{
  VixError err;

  err = VixDiskLib_Write(write_diskHandle, sector_offset,
                         nbyte / DEFAULT_SECTOR_SIZE, (uint8*)buf);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr, "VMDK Write error: %s [%lu]\n", error_txt, err);
    return -1;
  }

  return nbyte;
}

// Read data from a stream using the robust reader function.
static size_t read_from_stream(size_t, size_t nbyte, void* buf)
{
  return robust_reader(STDIN_FILENO, buf, nbyte);
}

// Write data from a stream using the robust writer function.
static size_t write_to_stream(size_t sector_offset, size_t nbyte, void* buf)
{
  // Should we clone to rawdevice ?
  if (raw_disk_fd != -1) { robust_writer(raw_disk_fd, buf, nbyte); }

  // Should we clone to new VMDK file ?
  if (write_diskHandle) {
    VixError err;

    err = VixDiskLib_Write(write_diskHandle, sector_offset,
                           nbyte / DEFAULT_SECTOR_SIZE, (uint8*)buf);
    if (VIX_FAILED(err)) {
      char* error_txt;

      error_txt = VixDiskLib_GetErrorText(err, NULL);
      fprintf(stderr, "VMDK Write error: %s [%lu]\n", error_txt, err);
    }
  }

  return robust_writer(STDOUT_FILENO, buf, nbyte);
}

// Encode the disk info of the disk saved into the backup output stream.
static inline bool save_disk_info(const char* key,
                                  json_t* cbt,
                                  uint64_t* absolute_disk_length)
{
  bool retval = false;
  runtime_disk_info_encoding rdie;
  json_t* object;

  fill_runtime_disk_info_encoding(&rdie);

  object = json_object_get(cbt, CBT_DISK_SIZE);
  if (!object) {
    fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
            CBT_DISK_SIZE, key);
    goto bail_out;
  }

  rdie.absolute_disk_length = json_integer_value(object);

  object = json_object_get(cbt, CBT_START_OFFSET);
  if (!object) {
    fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
            CBT_START_OFFSET, key);
    goto bail_out;
  }

  rdie.absolute_start_offset = json_integer_value(object);

  // Save the absolute offset we should use.
  absolute_start_offset = rdie.absolute_start_offset;

  if (robust_writer(STDOUT_FILENO, (char*)&rdie, rdie_size) != rdie_size) {
    fprintf(stderr,
            "Failed to write runtime_disk_info_encoding structure to output "
            "datastream\n");
    goto bail_out;
  }

  retval = true;
  if (absolute_disk_length) {
    *absolute_disk_length = rdie.absolute_disk_length;
  }

bail_out:
  return retval;
}

// Decode the disk info of the disk restored from the backup input stream.
static inline bool process_disk_info(bool validate_only, json_t* value)
{
  runtime_disk_info_encoding rdie;

  memset(&rdie, 0, rdie_size);
  if (robust_reader(STDIN_FILENO, (char*)&rdie, rdie_size) != rdie_size) {
    fprintf(stderr, "Failed to read a valid runtime_disk_info_encoding\n");
    goto bail_out;
  }

  if (rdie.start_magic != BAREOSMAGIC) {
    fprintf(stderr,
            "[runtime_disk_info_encoding] Failed to find valid MAGIC start "
            "marker read %u should have been %u\n",
            rdie.start_magic, BAREOSMAGIC);
    goto bail_out;
  }

  if (rdie.end_magic != BAREOSMAGIC) {
    fprintf(stderr,
            "[runtime_disk_info_encoding] Failed to find valid MAGIC end "
            "marker read %u should have been %u\n",
            rdie.end_magic, BAREOSMAGIC);
    goto bail_out;
  }

  if (verbose) { dump_runtime_disk_info_encoding(&rdie); }

  if (create_disk && !validate_only) {
    do_vixdisklib_create(DISK_PARAMS_KEY, vmdk_disk_name, value,
                         rdie.phys_capacity * VIXDISKLIB_SECTOR_SIZE);
    do_vixdisklib_open(DISK_PARAMS_KEY, vmdk_disk_name, value, false, true,
                       &write_diskHandle);

    if (!write_diskHandle) {
      fprintf(
          stderr,
          "Cannot process restore data as no VixDiskLib disk handle opened\n");
      goto bail_out;
    }
  }

  // Validate that things make sense to restore on the opened VMDK.
  if (!validate_only && check_size) {
    if (!validate_runtime_disk_info_encoding(&rdie)) {
      fprintf(stderr,
              "[runtime_disk_info_encoding] Invalid disk geometry for "
              "restoring to this volume\n");
      goto bail_out;
    }
  }

  // Save the absolute offset we should use.
  absolute_start_offset = rdie.absolute_start_offset;

  return true;

bail_out:
  return false;
}

// Read a specific meta data key and encode it into the output stream.
static inline bool read_meta_data_key(char* key)
{
  bool retval = false;
  VixError err;
  size_t requiredLen;
  char* buffer = NULL;
  runtime_meta_data_encoding rmde;

  if (verbose) { fprintf(stderr, "Processing metadata key %s\n", key); }

  err = VixDiskLib_ReadMetadata(read_diskHandle, key, NULL, 0, &requiredLen);
  if (err != VIX_OK && err != VIX_E_BUFFER_TOOSMALL) { return false; }

  buffer = (char*)malloc(requiredLen);
  if (!buffer) {
    fprintf(
        stderr,
        "Failed to allocate memory for holding metadata keys, exiting ...\n");
    return false;
  }

  err = VixDiskLib_ReadMetadata(read_diskHandle, key, buffer, requiredLen,
                                NULL);
  if (VIX_FAILED(err)) {
    char* error_txt;

    error_txt = VixDiskLib_GetErrorText(err, NULL);
    fprintf(stderr,
            "Failed to read metadata for key %s : %s [%lu] exiting ...\n", key,
            error_txt, err);
    goto bail_out;
  }

  // Should we clone metadata to new VMDK file ?
  if (write_diskHandle) {
    err = VixDiskLib_WriteMetadata(write_diskHandle, key, buffer);
    if (VIX_FAILED(err)) {
      char* error_txt;

      error_txt = VixDiskLib_GetErrorText(err, NULL);
      fprintf(stderr,
              "Failed to write metadata for key %s : %s [%lu] exiting ...\n",
              key, error_txt, err);
      goto bail_out;
    }
  }

  rmde.start_magic = BAREOSMAGIC;
  rmde.end_magic = BAREOSMAGIC;
  rmde.meta_key_length = strlen(key) + 1;
  rmde.meta_data_length = requiredLen;

  if (robust_writer(STDOUT_FILENO, &rmde, rmde_size) != rmde_size) {
    fprintf(stderr,
            "Failed to write runtime_meta_data_encoding structure to output "
            "datastream\n");
    goto bail_out;
  }

  if (robust_writer(STDOUT_FILENO, key, rmde.meta_key_length)
      != rmde.meta_key_length) {
    fprintf(stderr, "Failed to write meta data key to output datastream\n");
    goto bail_out;
  }

  if (robust_writer(STDOUT_FILENO, buffer, rmde.meta_data_length)
      != rmde.meta_data_length) {
    fprintf(stderr, "Failed to write meta data to output datastream\n");
    goto bail_out;
  }

  retval = true;

bail_out:
  if (buffer) { free(buffer); }

  return retval;
}

// Read all meta data from a disk and encode it into the output stream.
static inline bool save_meta_data()
{
  bool retval = false;
  VixError err;
  size_t requiredLen;
  char* bp;
  char* buffer = NULL;
  runtime_meta_data_encoding rmde;

  /* See if we are actually saving all meta data or should only write the META
   * data end marker. */
  if (save_metadata) {
    err = VixDiskLib_GetMetadataKeys(read_diskHandle, NULL, 0, &requiredLen);
    if (err != VIX_OK && err != VIX_E_BUFFER_TOOSMALL) { return false; }

    buffer = (char*)malloc(requiredLen);
    if (!buffer) {
      fprintf(
          stderr,
          "Failed to allocate memory for holding metadata keys, exiting ...\n");
      return false;
    }

    err = VixDiskLib_GetMetadataKeys(read_diskHandle, buffer, requiredLen,
                                     NULL);
    if (VIX_FAILED(err)) {
      char* error_txt;

      error_txt = VixDiskLib_GetErrorText(err, NULL);
      fprintf(stderr, "Failed to read metadata keys : %s [%lu] exiting ...\n",
              error_txt, err);
      goto bail_out;
    }

    bp = buffer;
    while (*bp) {
      if (!read_meta_data_key(bp)) { goto bail_out; }

      bp += strlen(bp) + 1;
    }
  }

  /* Write an META data end marker.
   * e.g. metadata header with key and data length == 0 */
  rmde.start_magic = BAREOSMAGIC;
  rmde.end_magic = BAREOSMAGIC;
  rmde.meta_key_length = 0;
  rmde.meta_data_length = 0;

  if (robust_writer(STDOUT_FILENO, &rmde, rmde_size) != rmde_size) {
    fprintf(stderr,
            "Failed to write runtime_meta_data_encoding structure to output "
            "datastream\n");
    goto bail_out;
  }

  retval = true;

bail_out:
  if (buffer) { free(buffer); }

  return retval;
}

/*
 * Read a backup stream from STDIN and process its metadata.
 * Stop processing when we encounter the special end of metadata tag
 * which is when the meta_key_length and meta_data_length are zero.
 */
static inline bool process_meta_data(bool validate_only)
{
  runtime_meta_data_encoding rmde;
  char* key = NULL;
  char* buffer = NULL;

  while (1) {
    if (robust_reader(STDIN_FILENO, &rmde, rmde_size) != rmde_size) {
      fprintf(stderr,
              "Failed to read runtime_meta_data_encoding structure from input "
              "datastream\n");
      goto bail_out;
    }

    if (rmde.start_magic != BAREOSMAGIC) {
      fprintf(stderr,
              "[runtime_meta_data_encoding] Failed to find valid MAGIC start "
              "marker read %u should have been %u\n",
              rmde.start_magic, BAREOSMAGIC);
      goto bail_out;
    }

    if (rmde.end_magic != BAREOSMAGIC) {
      fprintf(stderr,
              "[runtime_meta_data_encoding] Failed to find valid MAGIC end "
              "marker read %u should have been %u\n",
              rmde.end_magic, BAREOSMAGIC);
      goto bail_out;
    }

    // See if we processed the last meta data item.
    if (rmde.meta_key_length == 0 && rmde.meta_data_length == 0) { break; }

    key = (char*)malloc(rmde.meta_key_length);
    if (!key) {
      fprintf(stderr, "Failed to allocate %d bytes for meta data key\n",
              rmde.meta_key_length);
      goto bail_out;
    }

    if (robust_reader(STDIN_FILENO, key, rmde.meta_key_length)
        != rmde.meta_key_length) {
      fprintf(stderr, "Failed to read meta data key from input datastream\n");
      goto bail_out;
    }

    buffer = (char*)malloc(rmde.meta_data_length);
    if (!key) {
      fprintf(stderr, "Failed to allocate %d bytes for meta data\n",
              rmde.meta_data_length);
      goto bail_out;
    }

    if (robust_reader(STDIN_FILENO, buffer, rmde.meta_data_length)
        != rmde.meta_data_length) {
      fprintf(stderr, "Failed to read meta data from input datastream\n");
      goto bail_out;
    }

    if (verbose) {
      fprintf(stderr, "Meta data key %s, value %s\n", key, buffer);
    }

    if (!validate_only && restore_meta_data) {
      VixError err;

      err = VixDiskLib_WriteMetadata(write_diskHandle, key, buffer);
      if (VIX_FAILED(err)) {
        char* error_txt;

        error_txt = VixDiskLib_GetErrorText(err, NULL);
        fprintf(stderr,
                "Failed to write metadata for key %s : %s [%lu] exiting ...\n",
                key, error_txt, err);
        goto bail_out;
      }
    }

    free(key);
    free(buffer);
  }

  return true;

bail_out:
  return false;
}

/* Process a single cbt record. */
static bool process_single_cbt(std::vector<uint8>& buffer,
                               uint64 start_offset,
                               uint64 offset_length)
{
  runtime_cbt_encoding rce;
  rce.start_magic = BAREOSMAGIC;
  rce.end_magic = BAREOSMAGIC;
  rce.start_offset = start_offset;
  rce.offset_length = offset_length;

  if (verbose) {
    fprintf(stderr, "start = %lu\n", start_offset);
    fprintf(stderr, "length = %lu\n", offset_length);
    fprintf(stderr, "nr length = %lu\n", offset_length / DEFAULT_SECTOR_SIZE);
    fflush(stderr);
  }


  // Write the CBT info into the output stream.
  if (robust_writer(STDOUT_FILENO, (char*)&rce, rce_size) != rce_size) {
    fprintf(stderr,
            "Failed to write runtime_cbt_encoding structure to output "
            "datastream\n");
    return false;
  }

  if (raw_disk_fd != -1) {
    lseek(raw_disk_fd, start_offset, SEEK_SET);
    if (verbose) {
      fprintf(stderr, "Log: RAWFILE: Adusting seek position in file\n");
    }
  }

  bool retval = true;
  /* Calculate the start offset and read as many sectors as defined by the
   * length element of the JSON structure. */
  uint64 current_offset = absolute_start_offset + start_offset;
  uint64 max_offset = current_offset + offset_length;
  uint64 sector_offset = current_offset / DEFAULT_SECTOR_SIZE;
  while (current_offset < max_offset) {
    /* The number of sectors to read is the minimum of either the total number
     * of sectors still available in this CBT range or the upper setting
     * specified in the sectors_per_call variable. */
    uint64 sectors_to_read
        = MIN(sectors_per_call, (offset_length / DEFAULT_SECTOR_SIZE));

    if (multi_threaded) {
      if (!send_to_copy_thread(sector_offset,
                               sectors_to_read * DEFAULT_SECTOR_SIZE)) {
        retval = false;
        break;
      }
    } else {
      if (read_from_vmdk(sector_offset, sectors_to_read * DEFAULT_SECTOR_SIZE,
                         buffer.data())
          != sectors_to_read * DEFAULT_SECTOR_SIZE) {
        fprintf(stderr, "Read error on VMDK\n");
        retval = false;
        break;
      }

      if (write_to_stream(sector_offset, sectors_to_read * DEFAULT_SECTOR_SIZE,
                          buffer.data())
          != sectors_to_read * DEFAULT_SECTOR_SIZE) {
        fprintf(stderr, "Failed to write data to output datastream\n");
        retval = false;
        break;
      }
    }

    // Calculate the new offsets for a next run.
    current_offset += (sectors_to_read * DEFAULT_SECTOR_SIZE);
    sector_offset += sectors_to_read;
    offset_length -= sectors_to_read * DEFAULT_SECTOR_SIZE;
  }

  if (multi_threaded) {
    /* we need to wait until the thread has finished writing
     * all data that we have given him to write -- otherwise both this thread
     * and the copy thread would write to stdout at the same time! */
    flush_copy_thread();
  }

  if (verbose) { fflush(stderr); }

  return retval;
}

// std::vector<> can often not be used because of a version mismatch
// between the vixdisklib stdlib version and the compiler stdlib version.
struct vec {
  size_t capacity{0};
  size_t count{0};
  VixDiskLibBlock* data{nullptr};

  using value_type = VixDiskLibBlock;
  using pointer = VixDiskLibBlock*;
  using reference = VixDiskLibBlock&;

  vec(size_t t_capacity = 200)
      : capacity{t_capacity}
      , data{reinterpret_cast<pointer>(malloc(sizeof(value_type) * t_capacity))}
  {
  }

  vec(const vec&) = delete;
  vec& operator=(const vec&) = delete;

  vec& operator=(vec&& other)
  {
    std::swap(data, other.data);
    std::swap(capacity, other.capacity);
    std::swap(count, other.count);

    return *this;
  }

  vec(vec&& other) { *this = std::move(other); }

  void push_back(VixDiskLibBlock b)
  {
    if (count == capacity) {
      if (capacity <= 2) {
        capacity = 16;
      } else {
        capacity = capacity + (capacity >> 1);
      }
      data = reinterpret_cast<pointer>(
          realloc(data, sizeof(value_type) * capacity));
    }
    data[count++] = b;
  }

  size_t size() const { return count; }

  reference operator[](size_t i) { return data[i]; }

  ~vec()
  {
    if (data) { free(data); }
  }
};

/*
 * Process the Change Block Tracking information and write the wanted sectors
 * to the output stream. We self encode the data using a prefix header that
 * describes the data that is encoded after it including a MAGIC key and the
 * actual CBT information e.g. start of the read sectors and the number of
 * sectors that follow.
 * A wanted sector is a sector that is both allocated and has changed.
 */
static inline bool process_cbt(const char* key, vec allocated, json_t* cbt)
{
  if (verbose) {
    fprintf(stderr, "Allocated Blocks:\n");
    for (size_t i = 0; i < allocated.size(); ++i) {
      auto& block = allocated[i];
      auto boffset = block.offset * DEFAULT_SECTOR_SIZE;
      auto blength = block.length * DEFAULT_SECTOR_SIZE;

      fprintf(stderr, "  %10lu: { start: %lu, length: %lu }\n", i, boffset,
              blength);
    }
    fprintf(stderr, "\n\n");
  }

  bool retval = false;
  size_t index;
  json_t *object, *array_element, *start, *length;
  uint64_t start_offset, offset_length;
  uint64 current_block = 0;
  uint64 changed_len = 0, saved_len = 0;

  std::vector<uint8> buffer;
  if (!multi_threaded) {
    // we read at most sectors_per_call sectors at once
    // buffer is unused in multithreaded mode
    buffer.resize(DEFAULT_SECTOR_SIZE * sectors_per_call);
  }

  if (!read_diskHandle) {
    fprintf(stderr,
            "Cannot process CBT data as no VixDiskLib disk handle opened\n");
    goto bail_out;
  }

  object = json_object_get(cbt, CBT_CHANGEDAREA_KEY);
  if (!object) {
    fprintf(stderr, "Failed to find %s in JSON definition of object %s\n",
            CBT_CHANGEDAREA_KEY, key);
    goto bail_out;
  }

  /* Iterate over each element of the JSON array and get the "start" and
   * "length" member.
   * The json array is a sorted list of disjoint sector intervals,
   * which were changed. allocated is a sorted list of disjoint sector intervals
   * which are allocated.  We want to save their intersection, i.e. only sectors
   * which are both allocated and have changed. To visualise this:
   *
   * sectors    0 1 2 3 4 5 6 7 8 9
   * changed     [. . .] [. .]   [.] (as list: (1-3), (5-6))
   * allocated [. .] [. . . . .]     (as list: (0-1), (3-7))
   * saved:      [.] [.] [. .]       (as list: (1), (3), (5-6))
   *
   * Instead of backing up each sector (~512byte) separately, we instead
   * want to compute the resulting sector interval list directly. Since we are
   * given two "sorted" arrays, we can proceed similar to the merge step
   * of merge sort:
   * We look at the first elements of both lists, then
   * - if they have no intersection, one of them has to be completely smaller
   *   than the other (i.e. the last sector of one of them comes before the
   *   first sector of the other). Pop the smaller one from its list and
   *   continue.
   * - if they have some intersection, then compute the intersection
   *   and back it up.  Now select one of the intervals with the smallest
   *   end sector and pop it of its list.  This works because we know that
   *   it cannot have an nonempty intersection with any other interval
   *   of the other list (since both list contain only disjoint intervals and
   * are sorted). Then continue.
   *
   * We are finished once one list is empty, since we do not care about changed,
   * unallocated blocks and allocated blocks that were not changed.
   * In this specific implementation, popping of the changed list happens
   * automatically in each iteration of the outermost loop (since we just
   * iterate over them), whereas popping the allocated list happens byr
   * advancing the current_block index.
   */
  json_array_foreach(object, index, array_element)
  {
    // Get the two members we are interested in.
    start = json_object_get(array_element, CBT_CHANGEDAREA_START_KEY);
    length = json_object_get(array_element, CBT_CHANGEDAREA_LENGTH_KEY);

    if (!start || !length) { continue; }

    start_offset = json_integer_value(start);
    offset_length = json_integer_value(length);

    changed_len += offset_length;

    if (allocated.size() == current_block) {
      // All further sectors are unallocated, so we can stop here.
      break;
    }

    for (;;) {
      auto& block = allocated[current_block];

      auto boffset = block.offset * DEFAULT_SECTOR_SIZE;
      auto blength = block.length * DEFAULT_SECTOR_SIZE;

      if (start_offset + offset_length < boffset) {
        // skip unallocated block
        break;
      }

      // in a perfect world we would also save information about
      // newly unallocated blocks as well.
      // But since we cannot currently take advantage of that information
      // -- we do restores first -> last instead of last -> first and we
      //    do not do consolidations for plugins --
      // we just ignore them.  If needed in the future, we can mark "empty"
      // by e.g. changing the BAREOS_MAGIC to a different one.
      if (boffset < start_offset + offset_length
          && boffset + blength > start_offset) {
        uint64 offset = std::max(boffset, start_offset);
        uint64 min_length = std::min(blength, offset_length);

        saved_len += min_length;

        if (!process_single_cbt(buffer, offset, min_length)) { goto bail_out; }
      }

      if (boffset + blength <= start_offset + offset_length) {
        current_block += 1;
      }

      if (start_offset + offset_length <= boffset + blength) { break; }
    }
  }

  if (verbose) {
    fprintf(stderr, "Changed len: %lu, Saved len: %lu\n", changed_len,
            saved_len);
  }


  retval = true;

bail_out:
  if (read_diskHandle) {
    VixDiskLib_Close(read_diskHandle);
    read_diskHandle = NULL;
  }

  return retval;
}

/*
 * Read a backup stream from STDIN and process it. When validate_only is set
 * to true we only try to process the data but do not actually write it back
 * to the VMDK.
 */
static inline bool process_restore_stream(bool validate_only, json_t* value)
{
  bool retval = false;
  size_t cnt;
  uint64_t current_offset, max_offset;
  uint64_t sector_offset, sectors_to_read;
  runtime_cbt_encoding rce;

  std::vector<uint8> buffer;
  if (!multi_threaded) {
    // we read at most sectors_per_call sectors at once
    // buffer is unused in multithreaded mode
    buffer.resize(DEFAULT_SECTOR_SIZE * sectors_per_call);
  }

  if (!create_disk && !validate_only) {
    do_vixdisklib_open(DISK_PARAMS_KEY, vmdk_disk_name, value, false, true,
                       &write_diskHandle);

    if (!write_diskHandle) {
      fprintf(
          stderr,
          "Cannot process restore data as no VixDiskLib disk handle opened\n");
      goto bail_out;
    }
  }

  // Setup multi threading if requested.
  if (!validate_only && multi_threaded) {
    if (!setup_copy_thread(read_from_stream, write_to_vmdk)) {
      fprintf(stderr, "Failed to initialize multithreading\n");
      goto bail_out;
    }
  }

  // Process the disk info data.
  if (!process_disk_info(validate_only, value)) { goto bail_out; }

  // Process the disk meta data,
  if (!process_meta_data(validate_only)) { goto bail_out; }

  memset(&rce, 0, rce_size);
  while (robust_reader(STDIN_FILENO, (char*)&rce, rce_size) == rce_size) {
    if (rce.start_magic != BAREOSMAGIC) {
      fprintf(stderr,
              "[runtime_cbt_encoding] Failed to find valid MAGIC start marker "
              "read %u should have been %u\n",
              rce.start_magic, BAREOSMAGIC);
      goto bail_out;
    }

    if (rce.end_magic != BAREOSMAGIC) {
      fprintf(stderr,
              "[runtime_cbt_encoding] Failed to find valid MAGIC end marker "
              "read %u should have been %u\n",
              rce.end_magic, BAREOSMAGIC);
      goto bail_out;
    }

    if (verbose) {
      fprintf(stderr, "start = %lu\n", rce.start_offset);
      fprintf(stderr, "length = %lu\n", rce.offset_length);
      fprintf(stderr, "nr length = %lu\n",
              rce.offset_length / DEFAULT_SECTOR_SIZE);
      fflush(stderr);
    }

    current_offset = absolute_start_offset + rce.start_offset;
    max_offset = current_offset + rce.offset_length;
    sector_offset = current_offset / DEFAULT_SECTOR_SIZE;
    while (current_offset < max_offset) {
      /* The number of sectors to read is the minimum of either the total number
       * of sectors still available in this CBT range or the upper setting
       * specified in the sectors_per_call variable. */
      sectors_to_read
          = MIN(sectors_per_call, (rce.offset_length / DEFAULT_SECTOR_SIZE));

      if (!validate_only && multi_threaded) {
        if (!send_to_copy_thread(sector_offset,
                                 sectors_to_read * DEFAULT_SECTOR_SIZE)) {
          goto bail_out;
        }
      } else {
        cnt = robust_reader(STDIN_FILENO, (char*)buffer.data(),
                            sectors_to_read * DEFAULT_SECTOR_SIZE);
        if (cnt != sectors_to_read * DEFAULT_SECTOR_SIZE) { goto bail_out; }

        if (!validate_only) {
          if (write_to_vmdk(sector_offset, cnt, buffer.data()) != cnt) {
            goto bail_out;
          }
        }
      }

      // Calculate the new offsets for a next run.
      current_offset += (sectors_to_read * DEFAULT_SECTOR_SIZE);
      sector_offset += sectors_to_read;
      rce.offset_length -= sectors_to_read * DEFAULT_SECTOR_SIZE;
    }

    if (multi_threaded) { flush_copy_thread(); }

    memset(&rce, 0, rce_size);
  }

  if (multi_threaded) { cleanup_copy_thread(); }

  retval = true;

bail_out:
  if (write_diskHandle) {
    VixDiskLib_Close(write_diskHandle);
    write_diskHandle = NULL;
  }

  return retval;
}

/*
 * All work to this program is passed in using a JSON work file which has the
 * needed information to perform the wanted operation. This function loads the
 * JSON data into memory using the Jansson JSON library.
 */
static inline void process_json_work_file(const char* json_work_file)
{
  json_error_t error;

  // Open the JSON work file.
  json_config = json_load_file(json_work_file, JSON_DECODE_ANY, &error);
  if (!json_config) {
    fprintf(stderr,
            "Failed to parse JSON config %s [%s at line %d column %d]\n",
            json_work_file, error.text, error.line, error.column);
    exit(1);
  }

  if (verbose) {
    // Dump the internal parsed data in pretty print format.
    json_dumpf(json_config, stderr,
               JSON_PRESERVE_ORDER | JSON_COMPACT | JSON_INDENT(3));
    fprintf(stderr, "\n");
    fflush(stderr);
  }
}

// Worker function that performs the dump operation of the program.
static inline bool dump_vmdk_stream(const char* json_work_file)
{
  json_t* value;
  uint64_t absolute_disk_length = 0;

  process_json_work_file(json_work_file);

  value = json_object_get(json_config, CON_PARAMS_KEY);
  if (!value) {
    fprintf(stderr, "Failed to find %s in JSON definition\n", CON_PARAMS_KEY);
    exit(1);
  }

  do_vixdisklib_connect(CON_PARAMS_KEY, value, true, true);

  if (cleanup_on_start) { cleanup_vixdisklib(); }

  value = json_object_get(json_config, DISK_PARAMS_KEY);
  if (!value) {
    fprintf(stderr, "Failed to find %s in JSON definition\n", DISK_PARAMS_KEY);
    exit(1);
  }

  do_vixdisklib_open(DISK_PARAMS_KEY, NULL, value, true, true,
                     &read_diskHandle);

  value = json_object_get(json_config, CBT_DISKCHANGEINFO_KEY);
  if (!value) {
    fprintf(stderr, "Failed to find %s in JSON definition\n",
            CBT_DISKCHANGEINFO_KEY);
    exit(1);
  }

  // Setup multi threading if requested.
  if (multi_threaded) {
    if (!setup_copy_thread(read_from_vmdk, write_to_stream)) {
      fprintf(stderr, "Failed to initialize multithreading\n");
      exit(1);
    }
  }

  if (!save_disk_info(CBT_DISKCHANGEINFO_KEY, value, &absolute_disk_length)) {
    exit(1);
  }

  /* See if we are requested to clone the content to a new VMDK.
   * save_disk_info() initializes absolute_disk_length. */
  if (vmdk_disk_name) {
    if (create_disk) {
      do_vixdisklib_create(NULL, vmdk_disk_name, value, absolute_disk_length);
    }

    do_vixdisklib_open(NULL, vmdk_disk_name, value, false, false,
                       &write_diskHandle);
  }

  if (!save_meta_data()) { exit(1); }

  // See if we are requested to clone the content to a rawdevice.
  if (raw_disk_name) {
    if (verbose) { fprintf(stderr, "Log: RAWFILE: Trying to open RAW file\n"); }

    if ((raw_disk_fd = open(raw_disk_name, O_WRONLY | O_TRUNC)) == -1) {
      fprintf(stderr, "Error: Failed to open the RAW DISK FILE\n");
      exit(1);
    }
  }

  vec blocks{};

  if (do_query_allocated) {
    uint64 Offset = 0;
    uint64 Capacity = info->capacity;

    if (ChunkSize > Capacity) { ChunkSize = Capacity; }

    if (ChunkSize < VIXDISKLIB_MIN_CHUNK_SIZE) {
      ChunkSize = VIXDISKLIB_MIN_CHUNK_SIZE;
    }

    uint64 NumChunks = Capacity / ChunkSize;
    uint64 BlocksAllocated = 0;
    uint64 NumBlocks = 0;

    if (verbose) {
      fprintf(stderr, "ChunkSize: %lu, NumChunks: %lu\n", ChunkSize, NumChunks);
    }

    while (NumChunks > 0) {
      uint64 NumChunksToQuery
          = std::min((uint64)VIXDISKLIB_MAX_CHUNK_NUMBER, NumChunks);
      VixDiskLibBlockList* blocklist;

      auto err = VixDiskLib_QueryAllocatedBlocks(read_diskHandle, Offset,
                                                 NumChunksToQuery * ChunkSize,
                                                 ChunkSize, &blocklist);

      if (VIX_FAILED(err)) {
        char* error_txt;

        error_txt = VixDiskLib_GetErrorText(err, NULL);
        fprintf(stderr, "Failed to query allocated blocks: %s [%lu]\n",
                error_txt, err);
        VixDiskLib_FreeErrorText(error_txt);
        goto bail_out;
      }

      for (uint32 i = 0; i < blocklist->numBlocks; ++i) {
        blocks.push_back(blocklist->blocks[i]);
        NumBlocks += 1;
        BlocksAllocated += blocklist->blocks[i].length;
      }

      VixDiskLib_FreeBlockList(blocklist);

      Offset += NumChunksToQuery * ChunkSize;
      NumChunks -= NumChunksToQuery;
    }

    uint64 Unaligned = Capacity % ChunkSize;

    if (verbose) {
      fprintf(stderr, "Allocated Sectors: %lu\n", Unaligned + BlocksAllocated);
    }

    if (Unaligned > 0) {
      if (verbose) { fprintf(stderr, "Unaligned: %lu bytes\n", Unaligned); }
      blocks.push_back(VixDiskLibBlock{.offset = Offset, .length = Unaligned});
    }
  } else {
    uint64 Capacity = info->capacity;
    blocks.push_back(VixDiskLibBlock{.offset = 0, .length = Capacity});
  }

  return process_cbt(CBT_DISKCHANGEINFO_KEY, std::move(blocks), value);
bail_out:
  return false;
}

// Worker function that performs the restore operation of the program.
static inline bool restore_vmdk_stream(const char* json_work_file)
{
  json_t* value;

  process_json_work_file(json_work_file);

  value = json_object_get(json_config, CON_PARAMS_KEY);
  if (!value) {
    fprintf(stderr, "Failed to find %s in JSON definition\n", CON_PARAMS_KEY);
    exit(1);
  }

  if (cleanup_on_start) { cleanup_vixdisklib(); }

  do_vixdisklib_connect(CON_PARAMS_KEY, value, false, false);

  value = json_object_get(json_config, DISK_PARAMS_KEY);
  if (!value) {
    fprintf(stderr, "Failed to find %s in JSON definition\n", DISK_PARAMS_KEY);
    exit(1);
  }

  return process_restore_stream(false, value);
}

// Worker function that performs the show operation of the program.
static inline int show_backup_stream()
{
  return process_restore_stream(true, NULL);
}

static void signal_handler(int sig)
{
  exit_code = sig;
  exit(sig);
}

void usage(const char* program_name)
{
  fprintf(stderr,
          "Usage: %s [-d <vmdk_diskname>] [-f force_transport] [-s "
          "sectors_per_call] [-t disktype] [-k chunksize] [-CcDlMmRSvQ] dump "
          "<workfile> | "
          "restore <workfile> | show\n",
          program_name);
  fprintf(stderr, "Where:\n");
  fprintf(stderr, "   -C - Create local VMDK\n");
  fprintf(stderr, "   -c - Don't check size of VMDK\n");
  fprintf(stderr, "   -D - Cleanup on Disconnect\n");
  fprintf(stderr, "   -d - Specify local VMDK name\n");
  fprintf(stderr, "   -f - Specify forced transport method\n");
  fprintf(stderr, "   -h - This help text\n");
  fprintf(stderr, "   -l - Write to a local VMDK\n");
  fprintf(stderr, "   -M - Save metadata of VMDK on dump action\n");
  fprintf(stderr, "   -m - Use multithreading\n");
  fprintf(stderr, "   -r - RAW Image disk name\n");
  fprintf(stderr, "   -R - Restore metadata of VMDK on restore action\n");
  fprintf(stderr, "   -S - Cleanup on Start\n");
  fprintf(stderr, "   -s - Sectors to read per call to VDDK\n");
  fprintf(stderr, "   -t - Disktype to create for local VMDK\n");
  fprintf(stderr, "   -Q - Do not query allocated blocks\n");
  fprintf(stderr, "   -k - Query allocated blocks with this chunk size\n");
  fprintf(stderr, "   -v - Verbose output\n");
  fprintf(stderr, "   -? - This help text\n");
  exit(1);
}

int main(int argc, char** argv)
{
  bool retval = false;
  const char* program_name;
  int ch;

  program_name = argv[0];
  while ((ch = getopt(argc, argv, "CcDd:r:f:hlMmRSs:Qk:t:v?")) != -1) {
    switch (ch) {
      case 'C':
        create_disk = true;
        /* If we create the disk we should not check for the size as that won't
         * match. */
        check_size = false;
        break;
      case 'c':
        check_size = false;
        break;
      case 'D':
        cleanup_on_disconnect = true;
        break;
      case 'd':
        vmdk_disk_name = strdup(optarg);
        if (!vmdk_disk_name) {
          fprintf(stderr,
                  "Failed to allocate memory to hold diskname, exiting ...\n");
          exit(1);
        }
        break;
      case 'r':
        raw_disk_name = strdup(optarg);
        if (!raw_disk_name) {
          fprintf(
              stderr,
              "Failed to allocate memory to hold rawdiskname, exiting ...\n");
          exit(1);
        }
        break;
      case 'f':
        force_transport = strdup(optarg);
        if (!force_transport) {
          fprintf(stderr,
                  "Failed to allocate memory to hold forced transport, exiting "
                  "...\n");
          exit(1);
        }
        break;
      case 'l':
        local_vmdk = true;
        break;
      case 'k':
        ChunkSize = atoi(optarg);
        break;
      case 'Q':
        do_query_allocated = false;
        break;
      case 'M':
        save_metadata = true;
        break;
      case 'm':
        multi_threaded = true;
        break;
      case 'R':
        restore_meta_data = true;
        break;
      case 'S':
        cleanup_on_start = true;
        break;
      case 's': {
        auto new_sectors_per_call = atoi(optarg);

        if (new_sectors_per_call <= 0) {
          fprintf(stderr,
                  "We cannot back up data while not being able to request data "
                  "from vmware; sectors_per_call has to be a number > 0 "
                  "(got '%s')!\n",
                  optarg);
          exit(1);
        }

        sectors_per_call = new_sectors_per_call;

      } break;
      case 't':
        disktype = strdup(optarg);
        if (!disktype) {
          fprintf(stderr,
                  "Failed to allocate memory to hold disktype, exiting ...\n");
          exit(1);
        }
        break;
      case 'v':
        verbose = true;
        break;
      case 'h':
      case '?':
      default:
        usage(program_name);
        break;
    }
  }

  // Install signal handlers for the most important signals.
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  argc -= optind;
  argv += optind;

  if (argc <= 0) { usage(program_name); }

  if (strcasecmp(argv[0], "dump") == 0) {
    if (argc <= 1) { usage(program_name); }

    retval = dump_vmdk_stream(argv[1]);
  } else if (strcasecmp(argv[0], "restore") == 0) {
    if (argc <= 1) { usage(program_name); }

    retval = restore_vmdk_stream(argv[1]);
  } else if (strcasecmp(argv[0], "show") == 0) {
    retval = show_backup_stream();
  } else {
    fprintf(stderr, "Unknown action %s\n", argv[1]);
  }

  if (retval) { exit_code = 0; }

  exit(exit_code);
}
