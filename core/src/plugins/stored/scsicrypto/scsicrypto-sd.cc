/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
// Marco van Wieringen, March 2012
/**
 * @file
 * SCSI Encryption Storage daemon Plugin
 *
 * LTO4 and LTO5 drives and other modern tape drives
 * support hardware encryption.
 *
 * There are several ways of using encryption with these drives
 * The following types of key management are available for
 * doing encryption:
 *
 * - Transmission of the keys to the tapes is typically accomplished
 *   by using a backup application that supports Application Managed
 *   Encryption (AME)
 * - Transmission of the keys to the tapes is typically accomplished
 *   by using a tape library that supports Library Managed Encryption (LME)
 * - Transmission of the keys to the tapes is typically accomplished
 *   by using a Key Management Appliance (KMA).
 *
 * This plugin implements an Application Managed Encryption scheme where
 * on labeling a crypto key is generated for a volume and when the volume
 * is mounted the crypto key is loaded and when unloaded the key is cleared
 * from the memory of the Tape Drive using the SCSI SPOUT command set.
 *
 * If you have implemented Library Managed Encryption (LME) or
 * a Key Management Appliance (KMA) there is no need to have support
 * from Bareos on loading and clearing the encryption keys as either
 * the Library knows the per volume encryption keys itself or it
 * will ask the KMA for the encryption key when it needs it. For
 * big installations you might consider using a KMA but the Application
 * Managed Encryption implemented in Bareos should also scale rather
 * well and has low overhead as the keys are only loaded and cleared
 * when needed.
 *
 * This plugin uses the lowlevel SCSI key loading implemented in the
 * libbareos shared library.
 */
#include "include/bareos.h"
#include "stored/device_control_record.h"
#include "stored/device_status_information.h"
#include "stored/jcr_private.h"
#include "stored/stored.h"
#include "lib/berrno.h"
#include "lib/crypto_wrap.h"
#include "lib/scsi_crypto.h"

using namespace storagedaemon;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Marco van Wieringen"
#define PLUGIN_DATE "March 2012"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "SCSI Encryption Storage Daemon Plugin"
#define PLUGIN_USAGE "(No usage yet)"

// Forward referenced functions
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bSdEvent* event, void* value);
static bRC do_set_scsi_encryption_key(void* value);
static bRC do_clear_scsi_encryption_key(void* value);
static bRC handle_read_error(void* value);
static bRC send_device_encryption_status(void* value);
static bRC send_volume_encryption_status(void* value);

// Pointers to Bareos functions
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo
    = {sizeof(pluginInfo), SD_PLUGIN_INTERFACE_VERSION,
       SD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), SD_PLUGIN_INTERFACE_VERSION,

       // Entry points into plugin
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent};

static int const debuglevel = 200;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load the plugin
 */
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  Dmsg2(debuglevel, "scsicrypto-sd: Loaded: size=%d version=%d\n",
        bareos_core_functions->size, bareos_core_functions->version);
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

// External entry point to unload the plugin
bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(PluginContext* ctx)
{
  int JobId = 0;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg1(debuglevel, "scsicrypto-sd: newPlugin JobId=%d\n", JobId);

  /*
   * Only register plugin events we are interested in.
   *
   * bSdEventLabelRead - Read of volume label clear key as volume
   *                     labels are unencrypted (as we are in mixed
   *                     decryption mode we could leave the current
   *                     encryption key but most likely its the key
   *                     from an previous volume and most of the times
   *                     it will be cleared already by the
   *                     bSdEventVolumeUnload event already.)
   * bSdEventLabelVerified - Label of volume is verified and found
   *                         to be OK, any next data read from the
   *                         volume will be backup data and most
   *                         likely encrypted so load the volume
   *                         specific encryption key.
   * bSdEventLabelWrite - Write of volume label clear key as volume
   *                      labels are unencrypted.
   * bSdEventVolumeUnload - Unload of volume clear key
   * bSdEventReadError - Read error on volume see if its due to
   *                     the fact encryption is enabled and we
   *                     have either the wrong key loaded or no key
   *                     at all.
   * bSdEventDriveStatus - plugin callback for encryption status
   *                       of the drive.
   * bSdEventVolumeStatus - plugin callback for encryption status
   *                        of the volume loaded in the drive.
   */
  bareos_core_functions->registerBareosEvents(
      ctx, 7, bSdEventLabelRead, bSdEventLabelVerified, bSdEventLabelWrite,
      bSdEventVolumeUnload, bSdEventReadError, bSdEventDriveStatus,
      bSdEventVolumeStatus);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  int JobId = 0;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg1(debuglevel, "scsicrypto-sd: freePlugin JobId=%d\n", JobId);

  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue([[maybe_unused]] PluginContext* ctx,
                          pVariable var,
                          [[maybe_unused]] void* value)
{
  Dmsg1(debuglevel, "scsicrypto-sd: getPluginValue var=%d\n", var);

  return bRC_OK;
}

// Set a plugin value (none defined)
static bRC setPluginValue([[maybe_unused]] PluginContext* ctx,
                          [[maybe_unused]] pVariable var,
                          [[maybe_unused]] void* value)
{
  Dmsg1(debuglevel, "scsicrypto-sd: setPluginValue var=%d\n", var);

  return bRC_OK;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent([[maybe_unused]] PluginContext* ctx,
                             bSdEvent* event,
                             void* value)
{
  switch (event->eventType) {
    case bSdEventLabelRead:
    case bSdEventLabelWrite:
    case bSdEventVolumeUnload:
      return do_clear_scsi_encryption_key(value);
    case bSdEventLabelVerified:
      return do_set_scsi_encryption_key(value);
    case bSdEventReadError:
      return handle_read_error(value);
    case bSdEventDriveStatus:
      return send_device_encryption_status(value);
    case bSdEventVolumeStatus:
      return send_volume_encryption_status(value);
    default:
      Dmsg1(debuglevel, "scsicrypto-sd: Unknown event %d\n", event->eventType);
      return bRC_Error;
  }

  return bRC_OK;
}

static pthread_mutex_t crypto_operation_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline bool GetVolumeEncryptionKey(DeviceControlRecord* dcr,
                                          char* VolEncrKey)
{
  // See if we have valid VolCatInfo.
  if (dcr->haveVolCatInfo()) {
    bstrncpy(VolEncrKey, dcr->VolCatInfo.VolEncrKey, MAX_NAME_LENGTH);
    return true;
  } else if (dcr->jcr && dcr->jcr->dir_bsock) {
    /*
     * No valid VolCatInfo but we can get the info as we have
     * a connection to the director.
     */
    if (bareos_core_functions->UpdateVolumeInfo(dcr)) {
      bstrncpy(VolEncrKey, dcr->VolCatInfo.VolEncrKey, MAX_NAME_LENGTH);
      return true;
    }
  } else {
    /*
     * No valid VolCatInfo and we have no connection to the director.
     * Try to get the encryption key from the cache. The cached_key
     * is string dupped in the LookupCryptoKey function so we need to
     * free it here.
     */
    char* cached_key;

    if ((cached_key
         = bareos_core_functions->LookupCryptoKey(dcr->VolumeName))) {
      bstrncpy(VolEncrKey, cached_key, MAX_NAME_LENGTH);
      free(cached_key);
      return true;
    }
  }
  return false;
}

static bRC do_set_scsi_encryption_key(void* value)
{
  DeviceControlRecord* dcr;
  Device* dev;
  DeviceResource* device_resource;
  DirectorResource* director;
  char StoredVolEncrKey[MAX_NAME_LENGTH];
  char VolEncrKey[MAX_NAME_LENGTH];

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: dcr is not set!\n");
    return bRC_Error;
  }
  dev = dcr->dev;
  if (!dev) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: dev is not set!\n");
    return bRC_Error;
  }
  device_resource = dev->device_resource;
  if (!device_resource) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: device_resource is not set!\n");
    return bRC_Error;
  }

  // See if device_resource supports hardware encryption.
  if (!device_resource->drive_crypto_enabled) { return bRC_OK; }

  *StoredVolEncrKey = '\0';
  if (!GetVolumeEncryptionKey(dcr, StoredVolEncrKey)) {
    Dmsg0(debuglevel, "scsicrypto-sd: Could not GetVolumeEncryptionKey!\n");

    // Check if encryption key is needed for reading this volume.
    lock_mutex(crypto_operation_mutex);
    if (!NeedScsiCryptoKey(dev->fd, dev->archive_device_string, true)) {
      unlock_mutex(crypto_operation_mutex);
      Dmsg0(debuglevel, "scsicrypto-sd: No encryption key needed!\n");
      return bRC_OK;
    }
    unlock_mutex(crypto_operation_mutex);

    return bRC_Error;
  }

  // See if a volume encryption key is available.
  if (!*StoredVolEncrKey) {
    Dmsg0(debuglevel,
          "scsicrypto-sd: No encryption key to load on device_resource\n");
    return bRC_OK;
  }

  /*
   * The key passed from the director to the storage daemon is always base64
   * encoded.
   */
  Base64ToBin(VolEncrKey, sizeof(VolEncrKey), StoredVolEncrKey,
              strlen(StoredVolEncrKey));

  /*
   * See if we have an key encryption key in the config then the passed key
   * has been wrapped using RFC3394 key wrapping. We first copy the current
   * wrapped key into a temporary variable for unwrapping.
   */
  if (dcr->jcr && dcr->jcr->impl->director) {
    director = dcr->jcr->impl->director;
    if (director->keyencrkey.value) {
      char WrappedVolEncrKey[MAX_NAME_LENGTH];

      memcpy(WrappedVolEncrKey, VolEncrKey, MAX_NAME_LENGTH);
      memset(VolEncrKey, 0, MAX_NAME_LENGTH);

      if (AesUnwrap((unsigned char*)director->keyencrkey.value,
                    DEFAULT_PASSPHRASE_LENGTH / 8,
                    (unsigned char*)WrappedVolEncrKey,
                    (unsigned char*)VolEncrKey)
          != 0) {
        Emsg1(M_ERROR, 0,
              "scsicrypto-sd: Failed to unwrap encryption key using %s, "
              "probably wrong KeyEncryptionKey in config\n",
              director->keyencrkey.value);
        return bRC_Error;
      }
    }
  }

  Dmsg1(debuglevel, "scsicrypto-sd: Loading new crypto key %s\n", VolEncrKey);

  lock_mutex(crypto_operation_mutex);
  if (SetScsiEncryptionKey(dev->fd, dev->archive_device_string, VolEncrKey)) {
    dev->SetCryptoEnabled();
    unlock_mutex(crypto_operation_mutex);
    return bRC_OK;
  } else {
    unlock_mutex(crypto_operation_mutex);
    return bRC_Error;
  }
}

static bRC do_clear_scsi_encryption_key(void* value)
{
  DeviceControlRecord* dcr;
  Device* dev;
  DeviceResource* device_resource;
  bool need_to_clear;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: dcr is not set!\n");
    return bRC_Error;
  }
  dev = dcr->dev;
  if (!dev) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: dev is not set!\n");
    return bRC_Error;
  }
  device_resource = dev->device_resource;
  if (!device_resource) {
    Dmsg0(debuglevel, "scsicrypto-sd: Error: device_resource is not set!\n");
    return bRC_Error;
  }

  // See if device_resource supports hardware encryption.
  if (!device_resource->drive_crypto_enabled) { return bRC_OK; }

  lock_mutex(crypto_operation_mutex);
  /*
   * See if we need to query the drive or use the tracked encryption status of
   * the stored.
   */
  if (device_resource->query_crypto_status) {
    need_to_clear
        = IsScsiEncryptionEnabled(dev->fd, dev->archive_device_string);
  } else {
    need_to_clear = dev->IsCryptoEnabled();
  }
  if (need_to_clear) {
    Dmsg0(debuglevel, "scsicrypto-sd: Clearing crypto key\n");
    if (ClearScsiEncryptionKey(dev->fd, dev->archive_device_string)) {
      dev->ClearCryptoEnabled();
      unlock_mutex(crypto_operation_mutex);
      return bRC_OK;
    } else {
      unlock_mutex(crypto_operation_mutex);
      return bRC_Error;
    }
  } else {
    Dmsg0(debuglevel,
          "scsicrypto-sd: Not clearing crypto key because encryption is "
          "currently not enabled on drive\n");
    unlock_mutex(crypto_operation_mutex);
    return bRC_OK;
  }
}

static bRC handle_read_error(void* value)
{
  DeviceControlRecord* dcr;
  Device* dev;
  DeviceResource* device_resource;
  bool decryption_needed;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) { return bRC_Error; }
  dev = dcr->dev;
  if (!dev) { return bRC_Error; }
  device_resource = dev->device_resource;
  if (!device_resource) { return bRC_Error; }

  // See if drive crypto is enabled.
  if (device_resource->drive_crypto_enabled) {
    /*
     * See if the read error is an EIO which can be returned when we try to read
     * an encrypted block from a volume without decryption enabled or without a
     * proper encryption key loaded.
     */
    switch (dev->dev_errno) {
      case EIO:
        /*
         * See if we need to query the drive or use the tracked encryption
         * status of the stored. When we can query the drive we look at the next
         * block encryption state to see if we need decryption of the data on
         * the volume.
         */
        if (device_resource->query_crypto_status) {
          lock_mutex(crypto_operation_mutex);
          if (NeedScsiCryptoKey(dev->fd, dev->archive_device_string, false)) {
            decryption_needed = true;
          } else {
            decryption_needed = false;
          }
          unlock_mutex(crypto_operation_mutex);
        } else {
          decryption_needed = dev->IsCryptoEnabled();
        }

        /*
         * Alter the error message so it known this error is most likely due to
         * a failed decryption of the encrypted data on the volume.
         */
        if (decryption_needed) {
          BErrNo be;

          be.SetErrno(dev->dev_errno);
          Mmsg5(
              dev->errmsg,
              _("Read error on fd=%d at file:blk %u:%u on device %s. ERR=%s.\n"
                "Probably due to reading encrypted data from volume\n"),
              dev->fd, dev->file, dev->block_num, dev->print_name(),
              be.bstrerror());
        }
        break;
      default:
        break;
    }
  }
  return bRC_OK;
}

static bRC send_device_encryption_status(void* value)
{
  DeviceStatusInformation* dst;

  // Unpack the arguments passed in.
  dst = (DeviceStatusInformation*)value;
  if (!dst) { return bRC_Error; }

  // See if drive crypto is enabled.
  if (dst->device_resource->drive_crypto_enabled) {
    lock_mutex(crypto_operation_mutex);
    dst->status_length = GetScsiDriveEncryptionStatus(
        dst->device_resource->dev->fd,
        dst->device_resource->dev->archive_device_string, dst->status, 4);
    unlock_mutex(crypto_operation_mutex);
  }
  return bRC_OK;
}

static bRC send_volume_encryption_status(void* value)
{
  DeviceStatusInformation* dst;

  // Unpack the arguments passed in.
  dst = (DeviceStatusInformation*)value;
  if (!dst) { return bRC_Error; }

  // See if drive crypto is enabled.
  if (dst->device_resource->drive_crypto_enabled) {
    lock_mutex(crypto_operation_mutex);
    dst->status_length = GetScsiVolumeEncryptionStatus(
        dst->device_resource->dev->fd,
        dst->device_resource->dev->archive_device_string, dst->status, 4);
    unlock_mutex(crypto_operation_mutex);
  }
  return bRC_OK;
}
