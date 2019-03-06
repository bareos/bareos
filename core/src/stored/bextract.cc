/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Kern E. Sibbald, MM
 */
/**
 * @file
 * Dumb program to extract files from a Bareos backup.
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/butil.h"
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
#include "lib/edit.h"
#include "lib/bsignal.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

static void DoExtract(char* devname);
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);

static Device* dev = NULL;
static DeviceControlRecord* dcr;
static BareosWinFilePacket bfd;
static JobControlRecord* jcr;
static FindFilesPacket* ff;
static BootStrapRecord* bsr = NULL;
static bool extract = false;
static int non_support_data = 0;
static long total = 0;
static Attributes* attr;
static char* where;
static uint32_t num_files = 0;
static int prog_name_msg = 0;
static int win32_data_msg = 0;
static char* VolumeName = NULL;
static char* DirectorName = NULL;
static DirectorResource* director = NULL;

static struct acl_data_t acl_data;
static struct xattr_data_t xattr_data;
static alist* delayed_streams = NULL;

static char* wbuf;            /* write buffer address */
static uint32_t wsize;        /* write size */
static uint64_t fileAddr = 0; /* file write address */

static void usage()
{
  fprintf(stderr,
          _(PROG_COPYRIGHT
            "\nVersion: %s (%s)\n\n"
            "Usage: bextract <options> <bareos-archive-device-name> "
            "<directory-to-store-files>\n"
            "       -b <file>       specify a bootstrap file\n"
            "       -c <path>       specify a Storage configuration file or "
            "directory\n"
            "       -D <director>   specify a director name specified in the "
            "Storage\n"
            "                       configuration file for the Key Encryption "
            "Key selection\n"
            "       -d <nn>         set debug level to <nn>\n"
            "       -dt             print timestamp in debug output\n"
            "       -e <file>       exclude list\n"
            "       -i <file>       include list\n"
            "       -p              proceed inspite of I/O errors\n"
            "       -v              verbose\n"
            "       -V <volumes>    specify Volume names (separated by |)\n"
            "       -?              print this message\n\n"),
          2000, VERSION, BDATE);
  exit(1);
}

int main(int argc, char* argv[])
{
  int ch;
  FILE* fd;
  char line[1000];
  bool got_inc = false;

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  working_directory = "/tmp";
  MyNameIs(argc, argv, "bextract");
  InitMsg(NULL, NULL); /* setup message handler */

  OSDependentInit();

  ff = init_find_files();
  binit(&bfd);

  while ((ch = getopt(argc, argv, "b:c:D:d:e:i:pvV:?")) != -1) {
    switch (ch) {
      case 'b': /* bootstrap file */
        bsr = libbareos::parse_bsr(NULL, optarg);
        //       DumpBsr(bsr, true);
        break;

      case 'c': /* specify config file */
        if (configfile != NULL) { free(configfile); }
        configfile = bstrdup(optarg);
        break;

      case 'D': /* specify director name */
        if (DirectorName != NULL) { free(DirectorName); }
        DirectorName = bstrdup(optarg);
        break;

      case 'd': /* debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'e': /* exclude list */
        if ((fd = fopen(optarg, "rb")) == NULL) {
          BErrNo be;
          Pmsg2(0, _("Could not open exclude file: %s, ERR=%s\n"), optarg,
                be.bstrerror());
          exit(1);
        }
        while (fgets(line, sizeof(line), fd) != NULL) {
          StripTrailingJunk(line);
          Dmsg1(900, "add_exclude %s\n", line);
          AddFnameToExcludeList(ff, line);
        }
        fclose(fd);
        break;

      case 'i': /* include list */
        if ((fd = fopen(optarg, "rb")) == NULL) {
          BErrNo be;
          Pmsg2(0, _("Could not open include file: %s, ERR=%s\n"), optarg,
                be.bstrerror());
          exit(1);
        }
        while (fgets(line, sizeof(line), fd) != NULL) {
          StripTrailingJunk(line);
          Dmsg1(900, "add_include %s\n", line);
          AddFnameToIncludeList(ff, 0, line);
        }
        fclose(fd);
        got_inc = true;
        break;

      case 'p':
        forge_on = true;
        break;

      case 'v':
        verbose++;
        break;

      case 'V': /* Volume name */
        VolumeName = optarg;
        break;

      case '?':
      default:
        usage();

    } /* end switch */
  }   /* end while */
  argc -= optind;
  argv += optind;

  if (argc != 2) {
    Pmsg0(0,
          _("Wrong number of arguments. Make sure the last two parameters are "
            "<bareos-archive-device-name> <directory-to-store-files>\n"));
    usage();
  }

  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);

  if (DirectorName) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->name(), DirectorName)) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          _("No Director resource named %s defined in %s. Cannot continue.\n"),
          DirectorName, configfile);
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  if (!got_inc) {                      /* If no include file, */
    AddFnameToIncludeList(ff, 0, "/"); /*   include everything */
  }

  where = argv[1];
  DoExtract(argv[0]);

  if (bsr) { libbareos::FreeBsr(bsr); }
  if (prog_name_msg) {
    Pmsg1(000,
          _("%d Program Name and/or Program Data Stream records ignored.\n"),
          prog_name_msg);
  }
  if (win32_data_msg) {
    Pmsg1(000, _("%d Win32 data or Win32 gzip data stream records. Ignored.\n"),
          win32_data_msg);
  }
  TermIncludeExcludeFiles(ff);
  TermFindFiles(ff);
  return 0;
}

/*
 * Cleanup of delayed restore stack with streams for later processing.
 */
static inline void DropDelayedDataStreams()
{
  DelayedDataStream* dds = nullptr;

  if (!delayed_streams || delayed_streams->empty()) { return; }

  foreach_alist (dds, delayed_streams) {
    free(dds->content);
  }

  delayed_streams->destroy();
}

/*
 * Push a data stream onto the delayed restore stack for later processing.
 */
static inline void PushDelayedDataStream(int stream,
                                         char* content,
                                         uint32_t content_length)
{
  DelayedDataStream* dds;

  if (!delayed_streams) { delayed_streams = New(alist(10, owned_by_alist)); }

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

  /*
   * See if there is anything todo.
   */
  if (!delayed_streams || delayed_streams->empty()) { return; }

  /*
   * Only process known delayed data streams here.
   * If you start using more delayed data streams
   * be sure to add them in this loop and add the
   * proper calls here.
   *
   * Currently we support delayed data stream
   * processing for the following type of streams:
   * - *_ACL_*
   * - *_XATTR_*
   */
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
             _("Unknown stream=%d ignored. This shouldn't happen!\n"),
             dds->stream);
        break;
    }
  }

  /*
   * We processed the stack so we can destroy it.
   */
  delayed_streams->destroy();

  /*
   * (Re)Initialize the stack for a new use.
   */
  delayed_streams->init(10, owned_by_alist);

  return;
}

static void ClosePreviousStream(void)
{
  PopDelayedDataStreams();
  SetAttributes(jcr, attr, &bfd);
}

static void DoExtract(char* devname)
{
  struct stat statp;
  uint32_t decompress_buf_size;

  EnableBackupPrivileges(NULL, 1);

  dcr = New(DeviceControlRecord);
  jcr = SetupJcr("bextract", devname, bsr, director, dcr, VolumeName,
                 true); /* read device */
  if (!jcr) { exit(1); }
  dev = jcr->read_dcr->dev;
  if (!dev) { exit(1); }
  dcr = jcr->read_dcr;

  /*
   * Make sure where directory exists and that it is a directory
   */
  if (stat(where, &statp) < 0) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, _("Cannot stat %s. It must exist. ERR=%s\n"), where,
          be.bstrerror());
  }
  if (!S_ISDIR(statp.st_mode)) {
    Emsg1(M_ERROR_TERM, 0, _("%s must be a directory.\n"), where);
  }

  free(jcr->where);
  jcr->where = bstrdup(where);
  attr = new_attr(jcr);

  jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
  SetupDecompressionBuffers(jcr, &decompress_buf_size);
  if (decompress_buf_size > 0) {
    memset(&jcr->compress, 0, sizeof(CompressionContext));
    jcr->compress.inflate_buffer = GetMemory(decompress_buf_size);
    jcr->compress.inflate_buffer_size = decompress_buf_size;
  }

  acl_data.last_fname = GetPoolMemory(PM_FNAME);
  xattr_data.last_fname = GetPoolMemory(PM_FNAME);

  ReadRecords(dcr, RecordCb, MountNextReadVolume);

  /*
   * If output file is still open, it was the last one in the
   * archive since we just hit an end of file, so close the file.
   */
  if (IsBopen(&bfd)) { ClosePreviousStream(); }
  FreeAttr(attr);

  FreePoolMemory(acl_data.last_fname);
  FreePoolMemory(xattr_data.last_fname);

  if (delayed_streams) {
    DropDelayedDataStreams();
    delete delayed_streams;
  }

  CleanupCompression(jcr);

  CleanDevice(jcr->dcr);
  dev->term();
  FreeDeviceControlRecord(dcr);
  FreeJcr(jcr);

  printf(_("%u files restored.\n"), num_files);

  return;
}

static bool StoreData(BareosWinFilePacket* bfd,
                      char* data,
                      const int32_t length)
{
  if (is_win32_stream(attr->data_stream) && !have_win32_api()) {
    SetPortableBackup(bfd);
    if (!processWin32BackupAPIBlock(bfd, data, length)) {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0, _("Write error on %s: %s\n"), attr->ofname,
            be.bstrerror());
      return false;
    }
  } else if (bwrite(bfd, data, length) != (ssize_t)length) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, _("Write error on %s: %s\n"), attr->ofname,
          be.bstrerror());
    return false;
  }

  return true;
}

/*
 * Called here for each record from ReadRecords()
 */
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
                _("Logic error output file should be open but is not.\n"));
        }
        ClosePreviousStream();
        extract = false;
      }

      if (!UnpackAttributesRecord(jcr, rec->Stream, rec->data, rec->data_len,
                                  attr)) {
        Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }

      if (FileIsIncluded(ff, attr->fname) && !FileIsExcluded(ff, attr->fname)) {
        attr->data_stream = DecodeStat(attr->attr, &attr->statp,
                                       sizeof(attr->statp), &attr->LinkFI);
        if (!IsRestoreStreamSupported(attr->data_stream)) {
          if (!non_support_data++) {
            Jmsg(jcr, M_ERROR, 0,
                 _("%s stream not supported on this Client.\n"),
                 stream_to_ascii(attr->data_stream));
          }
          extract = false;
          return true;
        }

        BuildAttrOutputFnames(jcr, attr);

        if (attr->type ==
            FT_DELETED) { /* TODO: choose the right fname/ofname */
          Jmsg(jcr, M_INFO, 0, _("%s was deleted.\n"), attr->fname);
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
              Emsg2(M_ERROR_TERM, 0, _("Seek error on %s: %s\n"), attr->ofname,
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
        if (rec->maskedStream == STREAM_SPARSE_GZIP_DATA ||
            rec->maskedStream == STREAM_SPARSE_COMPRESSED_DATA) {
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

              Emsg3(M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
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
      break;

    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in the storage daemon
      break;

    case STREAM_PROGRAM_NAMES:
    case STREAM_PROGRAM_DATA:
      if (!prog_name_msg) {
        Pmsg0(000, _("Got Program Name or Data Stream. Ignored.\n"));
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
        PmStrcpy(acl_data.last_fname, attr->fname);
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
        PmStrcpy(xattr_data.last_fname, attr->fname);
        PushDelayedDataStream(rec->maskedStream, rec->data, rec->data_len);
      }
      break;

    case STREAM_NDMP_SEPARATOR:
      break;

    default:
      /*
       * If extracting, weird stream (not 1 or 2), close output file anyway
       */
      if (extract) {
        if (!IsBopen(&bfd)) {
          Emsg0(M_ERROR, 0,
                _("Logic error output file should be open but is not.\n"));
        }
        ClosePreviousStream();
        extract = false;
      }
      Jmsg(jcr, M_ERROR, 0,
           _("Unknown stream=%d ignored. This shouldn't happen!\n"),
           rec->Stream);
      break;

  } /* end switch */
  return true;
}
