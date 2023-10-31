/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
// Kern E. Sibbald, MM
/**
 * @file
 * Dumb program to extract files from a Bareos backup.
 */

#include <unistd.h>
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "findlib/find.h"
#include "findlib/attribs.h"
#include "findlib/create_file.h"
#include "findlib/match.h"
#include "findlib/get_priv.h"
#include "lib/address_conf.h"
#include "lib/attribs.h"
#include "lib/berrno.h"
#include "lib/cli.h"
#include "lib/edit.h"
#include "lib/bsignal.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"
#include "lib/compression.h"
#include "lib/serial.h"

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

static void DoExtract(char* devname,
                      std::string VolumeName,
                      BootStrapRecord* bsr,
                      DirectorResource* director);
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);

static Device* dev = nullptr;
static DeviceControlRecord* dcr;
static BareosFilePacket bfd;
static JobControlRecord* jcr;
static FindFilesPacket* ff;
static bool extract = false;
static int non_support_data = 0;
static long total = 0;
static Attributes* attr;
static char* where;
static uint32_t num_files = 0;
static int prog_name_msg = 0;
static int win32_data_msg = 0;

static AclData acl_data;
static XattrData xattr_data;
static alist<DelayedDataStream*>* delayed_streams = nullptr;

static char* wbuf;            /* write buffer address */
static uint32_t wsize;        /* write size */
static uint64_t fileAddr = 0; /* file write address */


int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  working_directory = "/tmp";
  MyNameIs(argc, argv, "bextract");
  InitMsg(nullptr, nullptr); /* setup message handler */

  OSDependentInit();

  ff = init_find_files();
  binit(&bfd);

  CLI::App bextract_app;
  InitCLIApp(bextract_app, "The Bareos Extraction tool.", 2000);

  BootStrapRecord* bsr = nullptr;
  bextract_app
      .add_option(
          "-b,--parse-bootstrap",
          [&bsr](std::vector<std::string> vals) {
            bsr = libbareos::parse_bsr(nullptr, vals.front().data());
            return true;
          },
          "Specify a bootstrap file.")
      ->check(CLI::ExistingFile)
      ->type_name("<file>");

  bextract_app
      .add_option(
          "-c,--config",
          [](std::vector<std::string> val) {
            if (configfile != nullptr) { free(configfile); }
            configfile = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string DirectorName{};
  bextract_app
      .add_option("-D,--director", DirectorName,
                  "Specify a director name specified in the storage.\n"
                  "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  AddDebugOptions(bextract_app);

  FILE* fd;
  char line[1000];
  bextract_app
      .add_option(
          "-e,--exclude",
          [&line, &fd](std::vector<std::string> val) {
            if ((fd = fopen(val.front().c_str(), "rb")) == nullptr) {
              BErrNo be;
              Pmsg2(0, T_("Could not open exclude file: %s, ERR=%s\n"),
                    val.front().c_str(), be.bstrerror());
              exit(BEXIT_FAILURE);
            }
            while (fgets(line, sizeof(line), fd) != nullptr) {
              StripTrailingJunk(line);
              Dmsg1(900, "add_exclude %s\n", line);
              AddFnameToExcludeList(ff, line);
            }
            fclose(fd);
            return true;
          },
          "Exclude list.")
      ->type_name("<file>");

  bool got_inc = false;

  bextract_app
      .add_option(
          "-i,--include-list",
          [&line, &fd, &got_inc](std::vector<std::string> val) {
            if ((fd = fopen(val.front().c_str(), "rb")) == nullptr) {
              BErrNo be;
              Pmsg2(0, T_("Could not open include file: %s, ERR=%s\n"),
                    val.front().c_str(), be.bstrerror());
              exit(BEXIT_FAILURE);
            }
            while (fgets(line, sizeof(line), fd) != nullptr) {
              StripTrailingJunk(line);
              Dmsg1(900, "add_include %s\n", line);
              AddFnameToIncludeList(ff, 0, line);
            }
            fclose(fd);
            got_inc = true;
            return true;
          },
          "Include list.")
      ->type_name("<file>");

  bextract_app.add_flag("-p,--ignore-errors", forge_on,
                        "Proceed inspite of IO errors.");

  AddVerboseOption(bextract_app);

  std::string VolumeNames;
  bextract_app
      .add_option("-V,--volumes", VolumeNames, "Volume names (separated by |).")
      ->type_name("<vol1|vol2|...>");

  std::string archive_device_name;
  bextract_app
      .add_option("bareos-archive-device-name", archive_device_name,
                  "Specify the input device name (either as name of a Bareos "
                  "Storage Daemon Device resource or identical to the Archive "
                  "Device in a Bareos Storage Daemon Device resource).")
      ->required()
      ->type_name(" ");

  std::string directory_to_store_files;
  bextract_app
      .add_option("target-directory", directory_to_store_files,
                  "Specify directory where to store files.")
      ->required()
      ->type_name(" ");


  ParseBareosApp(bextract_app, argc, argv);

  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

  static DirectorResource* director = nullptr;
  if (!DirectorName.empty()) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName.c_str())) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          T_("No Director resource named %s defined in %s. Cannot continue.\n"),
          DirectorName.c_str(), configfile);
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);


  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  if (!got_inc) {                      /* If no include file, */
    AddFnameToIncludeList(ff, 0, "/"); /*   include everything */
  }

  where = directory_to_store_files.data();
  DoExtract(archive_device_name.data(), VolumeNames, bsr, director);

  if (bsr) { libbareos::FreeBsr(bsr); }
  if (prog_name_msg) {
    Pmsg1(000,
          T_("%d Program Name and/or Program Data Stream records ignored.\n"),
          prog_name_msg);
  }
  if (win32_data_msg) {
    Pmsg1(000, T_("%d Win32 data or Win32 gzip data stream records. Ignored.\n"),
          win32_data_msg);
  }
  TermIncludeExcludeFiles(ff);
  TermFindFiles(ff);
  return BEXIT_SUCCESS;
}

// Cleanup of delayed restore stack with streams for later processing.
static inline void DropDelayedDataStreams()
{
  DelayedDataStream* dds = nullptr;

  if (!delayed_streams || delayed_streams->empty()) { return; }

  foreach_alist (dds, delayed_streams) { free(dds->content); }

  delayed_streams->destroy();
}

// Push a data stream onto the delayed restore stack for later processing.
static inline void PushDelayedDataStream(int stream,
                                         char* content,
                                         uint32_t content_length)
{
  DelayedDataStream* dds;

  if (!delayed_streams) {
    delayed_streams = new alist<DelayedDataStream*>(10, owned_by_alist);
  }

  dds = (DelayedDataStream*)malloc(sizeof(DelayedDataStream));
  dds->stream = stream;
  dds->content = (char*)malloc(content_length);
  memcpy(dds->content, content, content_length);
  dds->content_length = content_length;

  delayed_streams->append(dds);
}

/*
 * Restore any data streams that are restored after the file
 * is fully restored and has its attributes restored. Things
 * like acls and xattr are restored after we set the file
 * attributes otherwise we might clear some security flags
 * by setting the attributes.
 */
static inline void PopDelayedDataStreams()
{
  DelayedDataStream* dds = nullptr;

  // See if there is anything todo.
  if (!delayed_streams || delayed_streams->empty()) { return; }

  /* Only process known delayed data streams here.
   * If you start using more delayed data streams
   * be sure to add them in this loop and add the
   * proper calls here.
   *
   * Currently we support delayed data stream
   * processing for the following type of streams:
   * - *_ACL_*
   * - *_XATTR_* */
  foreach_alist (dds, delayed_streams) {
    switch (dds->stream) {
      case STREAM_UNIX_ACCESS_ACL:
      case STREAM_UNIX_DEFAULT_ACL:
      case STREAM_ACL_AIX_TEXT:
      case STREAM_ACL_DARWIN_ACCESS_ACL:
      case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      case STREAM_ACL_FREEBSD_ACCESS_ACL:
      case STREAM_ACL_HPUX_ACL_ENTRY:
      case STREAM_ACL_IRIX_DEFAULT_ACL:
      case STREAM_ACL_IRIX_ACCESS_ACL:
      case STREAM_ACL_LINUX_DEFAULT_ACL:
      case STREAM_ACL_LINUX_ACCESS_ACL:
      case STREAM_ACL_TRU64_DEFAULT_ACL:
      case STREAM_ACL_TRU64_DEFAULT_DIR_ACL:
      case STREAM_ACL_TRU64_ACCESS_ACL:
      case STREAM_ACL_SOLARIS_ACLENT:
      case STREAM_ACL_SOLARIS_ACE:
      case STREAM_ACL_AFS_TEXT:
      case STREAM_ACL_AIX_AIXC:
      case STREAM_ACL_AIX_NFS4:
      case STREAM_ACL_FREEBSD_NFS4_ACL:
      case STREAM_ACL_HURD_DEFAULT_ACL:
      case STREAM_ACL_HURD_ACCESS_ACL:
        parse_acl_streams(jcr, &acl_data, dds->stream, dds->content,
                          dds->content_length);
        free(dds->content);
        break;
      case STREAM_XATTR_HURD:
      case STREAM_XATTR_IRIX:
      case STREAM_XATTR_TRU64:
      case STREAM_XATTR_AIX:
      case STREAM_XATTR_OPENBSD:
      case STREAM_XATTR_SOLARIS_SYS:
      case STREAM_XATTR_DARWIN:
      case STREAM_XATTR_FREEBSD:
      case STREAM_XATTR_LINUX:
      case STREAM_XATTR_NETBSD:
        ParseXattrStreams(jcr, &xattr_data, dds->stream, dds->content,
                          dds->content_length);
        free(dds->content);
        break;
      default:
        Jmsg(jcr, M_WARNING, 0,
             T_("Unknown stream=%d ignored. This shouldn't happen!\n"),
             dds->stream);
        break;
    }
  }

  // We processed the stack so we can destroy it.
  delayed_streams->destroy();

  // (Re)Initialize the stack for a new use.
  delayed_streams->init(10, owned_by_alist);

  return;
}

static void ClosePreviousStream(void)
{
  PopDelayedDataStreams();
  SetAttributes(jcr, attr, &bfd);
}

static void DoExtract(char* devname,
                      std::string VolumeName,
                      BootStrapRecord* bsr,
                      DirectorResource* director)
{
  struct stat statp;
  /* uint32_t decompress_buf_size; */

  EnableBackupPrivileges(nullptr, 1);

  dcr = new DeviceControlRecord;
  jcr = SetupJcr("bextract", devname, bsr, director, dcr, VolumeName,
                 true); /* read device */
  if (!jcr) { exit(BEXIT_FAILURE); }
  dev = jcr->sd_impl->read_dcr->dev;
  if (!dev) { exit(BEXIT_FAILURE); }
  dcr = jcr->sd_impl->read_dcr;

  // Let SD plugins setup the record translation
  if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation, dcr) != bRC_OK) {
    Jmsg(jcr, M_FATAL, 0, T_("bSdEventSetupRecordTranslation call failed!\n"));
  }

  // Make sure where directory exists and that it is a directory
  if (stat(where, &statp) < 0) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Cannot stat %s. It must exist. ERR=%s\n"), where,
          be.bstrerror());
  }
  if (!S_ISDIR(statp.st_mode)) {
    Emsg1(M_ERROR_TERM, 0, T_("%s must be a directory.\n"), where);
  }

  free(jcr->where);
  jcr->where = strdup(where);
  attr = new_attr(jcr);

  jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;

  uint32_t decompress_buf_size;

  SetupDecompressionBuffers(jcr, &decompress_buf_size);
  if (decompress_buf_size > 0) {
    // See if we need to create a new compression buffer or make sure the
    // existing is big enough.
    if (!jcr->compress.inflate_buffer) {
      jcr->compress.inflate_buffer = GetMemory(decompress_buf_size);
      jcr->compress.inflate_buffer_size = decompress_buf_size;
    } else {
      if (decompress_buf_size > jcr->compress.inflate_buffer_size) {
        jcr->compress.inflate_buffer = ReallocPoolMemory(
            jcr->compress.inflate_buffer, decompress_buf_size);
        jcr->compress.inflate_buffer_size = decompress_buf_size;
      }
    }
  }

  acl_data.last_fname = GetPoolMemory(PM_FNAME);
  xattr_data.last_fname = GetPoolMemory(PM_FNAME);

  ReadRecords(dcr, RecordCb, MountNextReadVolume);

  /* If output file is still open, it was the last one in the
   * archive since we just hit an end of file, so close the file. */
  if (IsBopen(&bfd)) { ClosePreviousStream(); }
  FreeAttr(attr);

  FreePoolMemory(acl_data.last_fname);
  FreePoolMemory(xattr_data.last_fname);

  if (delayed_streams) {
    DropDelayedDataStreams();
    delete delayed_streams;
  }


  CleanDevice(jcr->sd_impl->dcr);

  delete dev;

  FreeDeviceControlRecord(dcr);

  CleanupCompression(jcr);
  FreePlugins(jcr);
  FreeJcr(jcr);
  UnloadSdPlugins();


  printf(T_("%u files restored.\n"), num_files);

  return;
}

static bool StoreData(BareosFilePacket* bfd, char* data, const int32_t length)
{
  if (is_win32_stream(attr->data_stream) && !have_win32_api()) {
    SetPortableBackup(bfd);
    if (!processWin32BackupAPIBlock(bfd, data, length)) {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0, T_("Write error on %s: %s\n"), attr->ofname,
            be.bstrerror());
      return false;
    }
  } else if (bwrite(bfd, data, length) != (ssize_t)length) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Write error on %s: %s\n"), attr->ofname,
          be.bstrerror());
    return false;
  }

  return true;
}

// Called here for each record from ReadRecords()
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  int status;
  JobControlRecord* jcr = dcr->jcr;

  if (rec->FileIndex < 0) { return true; /* we don't want labels */ }

  /* File Attributes stream */

  switch (rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:

      /* If extracting, it was from previous stream, so
       * close the output file.
       */
      if (extract) {
        if (!IsBopen(&bfd)) {
          Emsg0(M_ERROR, 0,
                T_("Logic error output file should be open but is not.\n"));
        }
        ClosePreviousStream();
        extract = false;
      }

      if (!UnpackAttributesRecord(jcr, rec->Stream, rec->data, rec->data_len,
                                  attr)) {
        Emsg0(M_ERROR_TERM, 0, T_("Cannot continue.\n"));
      }

      if (FileIsIncluded(ff, attr->fname) && !FileIsExcluded(ff, attr->fname)) {
        attr->data_stream = DecodeStat(attr->attr, &attr->statp,
                                       sizeof(attr->statp), &attr->LinkFI);
        if (!IsRestoreStreamSupported(attr->data_stream)) {
          if (!non_support_data++) {
            Jmsg(jcr, M_ERROR, 0,
                 T_("%s stream not supported on this Client.\n"),
                 stream_to_ascii(attr->data_stream));
          }
          extract = false;
          return true;
        }

        BuildAttrOutputFnames(jcr, attr);

        if (attr->type
            == FT_DELETED) { /* TODO: choose the right fname/ofname */
          Jmsg(jcr, M_INFO, 0, T_("%s was deleted.\n"), attr->fname);
          extract = false;
          return true;
        }

        extract = false;
        status = CreateFile(jcr, attr, &bfd, REPLACE_ALWAYS);
        switch (status) {
          case CF_ERROR:
          case CF_SKIP:
            break;
          case CF_EXTRACT:
            extract = true;
            PrintLsOutput(jcr, attr);
            num_files++;
            fileAddr = 0;
            break;
          case CF_CREATED:
            ClosePreviousStream();
            PrintLsOutput(jcr, attr);
            num_files++;
            fileAddr = 0;
            break;
        }
      }
      break;

    case STREAM_RESTORE_OBJECT:
      /* nothing to do */
      break;

    /* Data stream and extracting */
    case STREAM_FILE_DATA:
    case STREAM_SPARSE_DATA:
    case STREAM_WIN32_DATA:

      if (extract) {
        if (rec->maskedStream == STREAM_SPARSE_DATA) {
          ser_declare;
          uint64_t faddr;
          wbuf = rec->data + OFFSET_FADDR_SIZE;
          wsize = rec->data_len - OFFSET_FADDR_SIZE;
          SerBegin(rec->data, OFFSET_FADDR_SIZE);
          unser_uint64(faddr);
          if (fileAddr != faddr) {
            fileAddr = faddr;
            if (blseek(&bfd, (boffset_t)fileAddr, SEEK_SET) < 0) {
              BErrNo be;
              Emsg2(M_ERROR_TERM, 0, T_("Seek error on %s: %s\n"), attr->ofname,
                    be.bstrerror());
            }
          }
        } else {
          wbuf = rec->data;
          wsize = rec->data_len;
        }
        total += wsize;
        Dmsg2(8, "Write %u bytes, total=%u\n", wsize, total);
        StoreData(&bfd, wbuf, wsize);
        fileAddr += wsize;
      }
      break;

    /* GZIP data stream and Compressed data stream */
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_COMPRESSED_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
      if (extract) {
        if (rec->maskedStream == STREAM_SPARSE_GZIP_DATA
            || rec->maskedStream == STREAM_SPARSE_COMPRESSED_DATA) {
          ser_declare;
          uint64_t faddr;
          char ec1[50];

          wbuf = rec->data + OFFSET_FADDR_SIZE;
          wsize = rec->data_len - OFFSET_FADDR_SIZE;

          SerBegin(rec->data, OFFSET_FADDR_SIZE);
          unser_uint64(faddr);
          SerEnd(rec->data, OFFSET_FADDR_SIZE);

          if (fileAddr != faddr) {
            fileAddr = faddr;
            if (blseek(&bfd, (boffset_t)fileAddr, SEEK_SET) < 0) {
              BErrNo be;

              Emsg3(M_ERROR, 0, T_("Seek to %s error on %s: ERR=%s\n"),
                    edit_uint64(fileAddr, ec1), attr->ofname, be.bstrerror());
              extract = false;
              return true;
            }
          }
        } else {
          wbuf = rec->data;
          wsize = rec->data_len;
        }

        if (DecompressData(jcr, attr->ofname, rec->maskedStream, &wbuf, &wsize,
                           false)) {
          Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n",
                wsize, total);
          StoreData(&bfd, wbuf, wsize);
          total += wsize;
          fileAddr += wsize;
          Dmsg2(100, "Compress len=%d uncompressed=%d\n", rec->data_len, wsize);
        } else {
          extract = false;
          return false;
        }
      }
      break;

    case STREAM_MD5_DIGEST:
    case STREAM_SHA1_DIGEST:
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
    case STREAM_XXH128_DIGEST:
      break;

    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in the storage daemon
      break;

    case STREAM_PROGRAM_NAMES:
    case STREAM_PROGRAM_DATA:
      if (!prog_name_msg) {
        Pmsg0(000, T_("Got Program Name or Data Stream. Ignored.\n"));
        prog_name_msg++;
      }
      break;

    case STREAM_UNIX_ACCESS_ACL:  /* Deprecated Standard ACL attributes on UNIX
                                   */
    case STREAM_UNIX_DEFAULT_ACL: /* Deprecated Default ACL attributes on UNIX
                                   */
    case STREAM_ACL_AIX_TEXT:
    case STREAM_ACL_DARWIN_ACCESS_ACL:
    case STREAM_ACL_FREEBSD_DEFAULT_ACL:
    case STREAM_ACL_FREEBSD_ACCESS_ACL:
    case STREAM_ACL_HPUX_ACL_ENTRY:
    case STREAM_ACL_IRIX_DEFAULT_ACL:
    case STREAM_ACL_IRIX_ACCESS_ACL:
    case STREAM_ACL_LINUX_DEFAULT_ACL:
    case STREAM_ACL_LINUX_ACCESS_ACL:
    case STREAM_ACL_TRU64_DEFAULT_ACL:
    case STREAM_ACL_TRU64_DEFAULT_DIR_ACL:
    case STREAM_ACL_TRU64_ACCESS_ACL:
    case STREAM_ACL_SOLARIS_ACLENT:
    case STREAM_ACL_SOLARIS_ACE:
    case STREAM_ACL_AFS_TEXT:
    case STREAM_ACL_AIX_AIXC:
    case STREAM_ACL_AIX_NFS4:
    case STREAM_ACL_FREEBSD_NFS4_ACL:
    case STREAM_ACL_HURD_DEFAULT_ACL:
    case STREAM_ACL_HURD_ACCESS_ACL:
      if (extract) {
        PmStrcpy(acl_data.last_fname, attr->ofname);
        PushDelayedDataStream(rec->maskedStream, rec->data, rec->data_len);
      }
      break;

    case STREAM_XATTR_HURD:
    case STREAM_XATTR_IRIX:
    case STREAM_XATTR_TRU64:
    case STREAM_XATTR_AIX:
    case STREAM_XATTR_OPENBSD:
    case STREAM_XATTR_SOLARIS_SYS:
    case STREAM_XATTR_SOLARIS:
    case STREAM_XATTR_DARWIN:
    case STREAM_XATTR_FREEBSD:
    case STREAM_XATTR_LINUX:
    case STREAM_XATTR_NETBSD:
      if (extract) {
        PmStrcpy(xattr_data.last_fname, attr->ofname);
        PushDelayedDataStream(rec->maskedStream, rec->data, rec->data_len);
      }
      break;

    case STREAM_NDMP_SEPARATOR:
      break;

    default:
      // If extracting, weird stream (not 1 or 2), close output file anyway
      if (extract) {
        if (!IsBopen(&bfd)) {
          Emsg0(M_ERROR, 0,
                T_("Logic error output file should be open but is not.\n"));
        }
        ClosePreviousStream();
        extract = false;
      }
      Jmsg(jcr, M_ERROR, 0,
           T_("Unknown stream=%d ignored. This shouldn't happen!\n"),
           rec->Stream);
      break;

  } /* end switch */
  return true;
}
