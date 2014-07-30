/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * ndmp_tape.c implements the NDMP TAPE service which interfaces to
 * the internal Bareos infrstructure. This is implemented as a seperate
 * daemon protocol on a different port (10000 NDMP by default) which
 * interfaces to the standard Bareos storage daemon at the record level.
 *
 * E.g. normal data from a FD comes via the 9103 port and then get turned
 * into records for NDMP packets travel via the NDMP protocol library
 * which is named libbareosndmp and the data gets turned into native Bareos
 * tape records.
 *
 * Marco van Wieringen, May 2012
 */

#include "bareos.h"

#if HAVE_NDMP

#include "stored.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_LIBWRAP
#include "tcpd.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#elif HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "ndmp/ndmagents.h"

/*
 * Structure used to pass arguments to the ndmp_thread_server thread
 * via a void * argument. Things like the addresslist, maximum number
 * of clients and the client workqueue to use are passed using this
 * structure.
 */
struct ndmp_thread_server_args {
   dlist *addr_list;
   int max_clients;
   workq_t *client_wq;
};

/*
 * Internal structure to keep track of private data for a NDMP session.
 * Referenced via (struct ndm_session)->session_handle.
 */
struct ndmp_session_handle {
   int fd;                         /* Socket file descriptor */
   char *host;                     /* Host name/IP */
   int port;                       /* Local port */
   struct sockaddr client_addr;    /* Client's IP address */
   struct sockaddr_in peer_addr;   /* Peer's IP address */
   JCR *jcr;                       /* Internal JCR bound to this NDMP session */
};

/*
 * Internal structure to keep track of private data.
 */
struct ndmp_internal_state {
   uint32_t LogLevel;
   JCR *jcr;
};
typedef struct ndmp_internal_state NIS;

struct ndmp_backup_format_option {
   char *format;
   bool uses_level;
};

static ndmp_backup_format_option ndmp_backup_format_options[] = {
   { (char *)"dump", true },
   { (char *)"tar", false },
   { (char *)"smtape", false },
   { (char *)"zfs", true },
   { NULL, false }
};

static ndmp_backup_format_option *lookup_backup_format_options(const char *backup_format)
{
   int i = 0;

   while (ndmp_backup_format_options[i].format) {
      if (bstrcasecmp(backup_format, ndmp_backup_format_options[i].format)) {
         break;
      }
      i++;
   }

   if (ndmp_backup_format_options[i].format) {
      return &ndmp_backup_format_options[i];
   }

   return (ndmp_backup_format_option *)NULL;
}

/* Static globals */
static bool quit = false;
static bool ndmp_initialized = false;
static pthread_t ndmp_tid;
static struct ndmp_thread_server_args ndmp_thread_server_args;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */

static inline int native_to_ndmp_loglevel(int debuglevel, NIS *nis)
{
   unsigned int level;

   memset(nis, 0, sizeof(NIS));

   /*
    * Lookup the initial default log_level from the default STORES.
    */
   nis->LogLevel = me->ndmploglevel;

   /*
    * NDMP loglevels run from 0 - 9 so we take a look at the
    * current debug level and divide it by 100 to get a proper
    * value. If the debuglevel is below the wanted initial level
    * we set the loglevel to the wanted initial level. As the
    * debug logging takes care of logging messages that are
    * unwanted we can set the loglevel higher and still don't
    * get debug messages.
    */
   level = debuglevel / 100;
   if (level < nis->LogLevel) {
      level = nis->LogLevel;
   }

   /*
    * Make sure the level is in the wanted range.
    */
   if (level < 0) {
      level = 0;
   } else if (level > 9) {
      level = 9;
   }

   return level;
}

/*
 * Interface function which glues the logging infra of the NDMP lib with the daemon.
 */
extern "C" void ndmp_loghandler(struct ndmlog *log, char *tag, int level, char *msg)
{
   unsigned int internal_level = level * 100;
   NIS *nis;

   /*
    * We don't want any trailing newline in log messages.
    */
   strip_trailing_newline(msg);

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   /*
    * If the log level of this message is under our logging treshold we
    * log it as part of the Job.
    */
   if ((internal_level / 100) <= nis->LogLevel) {
      if (nis->jcr) {
         /*
          * Look at the tag field to see what is logged.
          */
         if (bstrncmp(tag + 1, "LM", 2)) {
            /*
             * *LM* messages. E.g. log message NDMP protocol msgs.
             * First character of the tag is the agent sending the
             * message e.g. 'D' == Data Agent
             *              'T' == Tape Agent
             *              'R' == Robot Agent
             *              'C' == Control Agent (DMA)
             *
             * Last character is the type of message e.g.
             * 'n' - normal message
             * 'd' - debug message
             * 'e' - error message
             * 'w' - warning message
             * '?' - unknown message level
             */
            switch (*(tag + 3)) {
            case 'n':
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            case 'e':
               Jmsg(nis->jcr, M_ERROR, 0, "%s\n", msg);
               break;
            case 'w':
               Jmsg(nis->jcr, M_WARNING, 0, "%s\n", msg);
               break;
            case '?':
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            default:
               break;
            }
         } else {
            Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
         }
      }
   }

   /*
    * Print any debug message we convert the NDMP level back to an internal
    * level and let the normal debug logging handle if it needs to be printed
    * or not.
    */
   Dmsg3(internal_level, "NDMP: [%s] [%d] %s\n", tag, (internal_level / 100), msg);
}

/*
 * Clear text authentication callback.
 */
extern "C" int bndmp_auth_clear(struct ndm_session *sess, char *name, char *pass)
{
   NDMPRES *auth_config;

   foreach_res(auth_config, R_NDMP) {
      /*
       * Only consider entries for AT_CLEAR authentication type.
       */
      if (auth_config->AuthType != AT_CLEAR) {
         continue;
      }

      ASSERT(auth_config->password.encoding == p_encoding_clear);

      if (bstrcmp(name, auth_config->username) &&
          bstrcmp(pass, auth_config->password.value)) {
         /*
          * See if we need to adjust the logging level.
          */
         if (sess->param->log.ctx) {
            NIS *nis;

            nis = (NIS *)sess->param->log.ctx;
            if (nis->LogLevel != auth_config->LogLevel) {
               if (auth_config->LogLevel >= 0 && auth_config->LogLevel <= 9) {
                  nis->LogLevel = auth_config->LogLevel;
               }
            }
         }

         return 1;
      }
   }
   return 0;
}

/*
 * MD5 authentication callback.
 */
extern "C" int bndmp_auth_md5(struct ndm_session *sess, char *name, char digest[16])
{
   NDMPRES *auth_config;

   foreach_res(auth_config, R_NDMP) {
      /*
       * Only consider entries for AT_MD5 authentication type.
       */
      if (auth_config->AuthType != AT_MD5) {
         continue;
      }

      if (!bstrcmp(name, auth_config->username)) {
         continue;
      }

      ASSERT(auth_config->password.encoding == p_encoding_clear);

      if (!ndmmd5_ok_digest(sess->md5_challenge, auth_config->password.value, digest)) {
         return 0;
      }

      /*
       * See if we need to adjust the logging level.
       */
      if (sess->param->log.ctx) {
         NIS *nis;

         nis = (NIS *)sess->param->log.ctx;
         if (nis->LogLevel != auth_config->LogLevel) {
            if (auth_config->LogLevel >= 0 && auth_config->LogLevel <= 9) {
               nis->LogLevel = auth_config->LogLevel;
            }
         }
      }

      return 1;
   }

   return 0;
}

/*
 * Save a record using the native routines.
 */
static inline bool bndmp_write_data_to_block(JCR *jcr,
                                             int stream,
                                             char *data,
                                             u_long data_length)
{
   bool retval = false;
   DCR *dcr = jcr->dcr;
   POOLMEM *rec_data;

   /*
    * Keep track of the original data buffer and restore it on exit from this function.
    */
   rec_data = dcr->rec->data;

   dcr->rec->VolSessionId = jcr->VolSessionId;
   dcr->rec->VolSessionTime = jcr->VolSessionTime;
   dcr->rec->FileIndex = dcr->FileIndex;
   dcr->rec->Stream = stream;
   dcr->rec->maskedStream = stream & STREAMMASK_TYPE; /* strip high bits */
   dcr->rec->data = data;
   dcr->rec->data_len = data_length;

   if (!dcr->write_record()) {
      goto bail_out;
   }

   if (stream == STREAM_UNIX_ATTRIBUTES) {
      dcr->dir_update_file_attributes(dcr->rec);
   }

   retval = true;

bail_out:
   dcr->rec->data = rec_data;
   return retval;
}

/*
 * Read a record using the native routines.
 *
 * data_length == 0 = EOF
 */
static inline bool bndmp_read_data_from_block(JCR *jcr,
                                              char *data,
                                              u_long wanted_data_length,
                                              u_long *data_length)
{
   DCR *dcr = jcr->read_dcr;
   READ_CTX *rctx = jcr->rctx;
   bool done = false;
   bool ok = true;

   if (!rctx) {
      return false;
   }

   while (!done) {
      /*
       * See if there are any records left to process.
       */
      if (!is_block_empty(rctx->rec)) {
         if (!read_next_record_from_block(dcr, rctx, &done)) {
            /*
             * When the done flag is set to true we are done reading all
             * records or end of block read next block.
             */
            continue;
         }
      } else {
         /*
          * Read the next block into our buffers.
          */
         if (!read_next_block_from_device(dcr, &rctx->sessrec,
                                          NULL, mount_next_read_volume, &ok)) {
            return false;
         }

         /*
          * Get a new record for each Job as defined by VolSessionId and VolSessionTime
          */
         if (!rctx->rec ||
              rctx->rec->VolSessionId != dcr->block->VolSessionId ||
              rctx->rec->VolSessionTime != dcr->block->VolSessionTime) {
            read_context_set_record(dcr, rctx);
         }

         rctx->records_processed = 0;
         rctx->rec->state_bits = 0;
         rctx->lastFileIndex = READ_NO_FILEINDEX;

         if (!read_next_record_from_block(dcr, rctx, &done)) {
            /*
             * When the done flag is set to true we are done reading all
             * records or end of block read next block.
             */
            continue;
         }
      }

      /*
       * Here we should have read a record from the block which contains some data.
       * Its either:
       *
       * - STREAM_UNIX_ATTRIBUTES
       *      Which is the start of the dump when we encounter that we just read the next record.
       * - STREAM_FILE_DATA
       *      Normal NDMP data.
       * - STREAM_NDMP_SEPERATOR
       *      End of NDMP data stream.
       *
       * anything other means a corrupted stream of records and means we give an EOF.
       */
      switch (rctx->rec->maskedStream) {
      case STREAM_UNIX_ATTRIBUTES:
         continue;
      case STREAM_FILE_DATA:
         if (wanted_data_length < rctx->rec->data_len) {
            Jmsg0(jcr, M_FATAL, 0,
                  _("Data read from volume bigger then NDMP databuffer, please increase the NDMP blocksize.\n"));
            return false;
         }
         memcpy(data, rctx->rec->data, rctx->rec->data_len);
         *data_length = rctx->rec->data_len;
         return true;
      case STREAM_NDMP_SEPERATOR:
         *data_length = 0;
         return true;
      default:
         *data_length = 0;
         return true;
      }
   }

   if (done) {
      *data_length = 0;
   }

   return true;
}

/*
 * Generate virtual file attributes for the whole NDMP stream.
 */
static inline bool bndmp_create_virtual_file(JCR *jcr, char *filename)
{
   DCR *dcr = jcr->dcr;
   struct stat statp;
   time_t now = time(NULL);
   POOL_MEM attribs(PM_NAME),
            data(PM_NAME);
   int32_t size;

   memset(&statp, 0, sizeof(statp));
   statp.st_mode = 0700 | S_IFREG;
   statp.st_ctime = now;
   statp.st_mtime = now;
   statp.st_atime = now;
   statp.st_size = -1;
   statp.st_blksize = 4096;
   statp.st_blocks = 1;

   /*
    * Encode a stat structure into an ASCII string.
    */
   encode_stat(attribs.c_str(), &statp, sizeof(statp), dcr->FileIndex, STREAM_UNIX_ATTRIBUTES);

   /*
    * Generate a file attributes stream.
    *   File_index
    *   File type
    *   Filename (full path)
    *   Encoded attributes
    *   Link name (if type==FT_LNK or FT_LNKSAVED)
    *   Encoded extended-attributes (for Win32)
    *   Delta Sequence Number
    */
   size = Mmsg(data,
               "%ld %d %s%c%s%c%s%c%s%c%d%c",
               dcr->FileIndex,       /* File_index */
               FT_REG,               /* File type */
               filename,             /* Filename (full path) */
               0,
               attribs.c_str(),      /* Encoded attributes */
               0,
               "",                   /* Link name (if type==FT_LNK or FT_LNKSAVED) */
               0,
               "",                   /* Encoded extended-attributes (for Win32) */
               0,
               0,                    /* Delta Sequence Number */
               0);

   return bndmp_write_data_to_block(jcr, STREAM_UNIX_ATTRIBUTES, data.c_str(), size);
}

static int bndmp_simu_flush_weof(struct ndm_session *sess)
{
   struct ndm_tape_agent * ta = sess->tape_acb;

   if (ta->weof_on_close) {
      /* best effort */
      ndmos_tape_wfm (sess);
   }
   return 0;
}

/*
 * Search the JCRs for one with the given security key.
 */
static inline JCR *get_jcr_by_security_key(char *security_key)
{
   JCR *jcr;

   foreach_jcr(jcr) {
      if (bstrcmp(jcr->sd_auth_key, security_key)) {
         jcr->inc_use_count();
         break;
      }
   }
   endeach_jcr(jcr);
   return jcr;
}

extern "C" ndmp9_error bndmp_tape_open(struct ndm_session *sess,
                                       char *drive_name,
                                       int will_write)
{
   JCR *jcr;
   DCR *dcr;
   char *filesystem;
   struct ndmp_session_handle *handle;
   struct ndm_tape_agent *ta;
   ndmp_backup_format_option *nbf_options;

   /*
    * The drive_name should be in the form <AuthKey>@<file_system>
    */
   if ((filesystem = strchr(drive_name, '@')) == NULL) {
      return NDMP9_NO_DEVICE_ERR;
   }

   /*
    * Lookup the jobid the drive_name should contain a valid authentication key.
    */
   *filesystem++ = '\0';
   if (!(jcr = get_jcr_by_security_key(drive_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("NDMP tape open failed: Security Key not found: %s\n"), drive_name);
      return NDMP9_NO_DEVICE_ERR;
   }

   /*
    * When we found a JCR with the wanted security key it also implictly
    * means the authentication succeeded as the connecting NDMP session
    * only knows the exact security key as it was inserted by the director.
    */
   jcr->authenticated = true;

   /*
    * There is a native storage daemon session waiting for the FD to connect.
    * In NDMP terms this is the same as a FD connecting so wake any waiting
    * threads.
    */
   pthread_cond_signal(&jcr->job_start_wait);

   /*
    * Save the JCR to ndm_session binding so everything furher
    * knows which JCR belongs to which NDMP session. We have
    * a special ndmp_session_handle which we can use to track
    * session specific information.
    */
   handle = (struct ndmp_session_handle *)sess->session_handle;

   /*
    * If we already have a JCR binding for this connection we release it here
    * as we are about to establish a new binding (e.g. second NDMP save for
    * the same job) and we should end up with the same binding.
    */
   if (handle->jcr) {
      free_jcr(handle->jcr);
   }
   handle->jcr = jcr;

   /*
    * Keep track of the JCR for logging purposes.
    */
   if (sess->param->log.ctx) {
      NIS *nis;

      nis = (NIS *)sess->param->log.ctx;
      nis->jcr = jcr;
   }

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = lookup_backup_format_options(jcr->backup_format);

   /*
    * Depending on the open mode select the right DCR.
    */
   if (will_write) {
      dcr = jcr->dcr;
   } else {
      dcr = jcr->read_dcr;
   }

   if (!dcr) {
      Jmsg0(jcr, M_FATAL, 0, _("DCR is NULL!!!\n"));
      return NDMP9_NO_DEVICE_ERR;
   }

   if (!dcr->dev) {
      Jmsg0(jcr, M_FATAL, 0, _("DEVICE is NULL!!!\n"));
      return NDMP9_NO_DEVICE_ERR;
   }

   /*
    * See if need to setup for write or read.
    */
   if (will_write) {
      POOL_MEM virtual_filename(PM_FNAME);

      /*
       * Setup internal system for writing data.
       */
      Dmsg1(100, "Start append data. res=%d\n", dcr->dev->num_reserved());

      /*
       * One NDMP backup Job can be one or more save sessions so we keep
       * track if we already acquired the storage.
       */
      if (!jcr->acquired_storage) {
         /*
          * Actually acquire the device which we reserved.
          */
         if (!acquire_device_for_append(dcr)) {
            goto bail_out;
         }

         /*
          * Let any SD plugin know now its time to setup the record translation infra.
          */
         if (generate_plugin_event(jcr, bsdEventSetupRecordTranslation, dcr) != bRC_OK) {
            goto bail_out;
         }

         /*
          * Keep track that we acquired the storage.
          */
         jcr->acquired_storage = true;

         Dmsg1(50, "Begin append device=%s\n", dcr->dev->print_name());

         /*
          * Change the Job to running state.
          */
         jcr->sendJobStatus(JS_Running);

         /*
          * As we only generate very limited attributes info e.g. one
          * set per NDMP backup stream we only setup data spooling and not
          * attribute spooling.
          */

         if (!begin_data_spool(dcr) ) {
            goto bail_out;
         }

         /*
          * Write Begin Session Record
          */
         if (!write_session_label(dcr, SOS_LABEL)) {
            Jmsg1(jcr, M_FATAL, 0,
                  _("Write session label failed. ERR=%s\n"),
                  dcr->dev->bstrerror());
            goto bail_out;
         }

         dcr->VolFirstIndex = dcr->VolLastIndex = 0;
         jcr->run_time = time(NULL);              /* start counting time for rates */

         /*
          * The session is saved as one file stream.
          */
         dcr->FileIndex = 1;
         jcr->JobFiles = 1;
      } else {
         /*
          * The next session is saved as one file stream.
          */
         dcr->FileIndex++;
         jcr->JobFiles++;
      }

      /*
       * Create a virtual file name @NDMP/<filesystem>%<dumplevel> or
       * @NDMP/<filesystem> and save the attributes to the director.
       */
      if (nbf_options && nbf_options->uses_level) {
         Mmsg(virtual_filename, "/@NDMP%s%%%d", filesystem, jcr->DumpLevel);
      } else {
         Mmsg(virtual_filename, "/@NDMP%s", filesystem);
      }
      if (!bndmp_create_virtual_file(jcr, virtual_filename.c_str())) {
         Jmsg0(jcr, M_FATAL, 0,
               _("Creating virtual file attributes failed.\n"));
         goto bail_out;
      }
   } else {
      bool ok = true;
      READ_CTX *rctx;

      /*
       * Setup internal system for reading data (if not done before).
       */
      if (!jcr->acquired_storage) {
         Dmsg0(20, "Start read data.\n");

         if (jcr->NumReadVolumes == 0) {
            Jmsg(jcr, M_FATAL, 0, _("No Volume names found for restore.\n"));
            goto bail_out;
         }

         Dmsg2(200, "Found %d volumes names to restore. First=%s\n",
               jcr->NumReadVolumes, jcr->VolList->VolumeName);

         /*
          * Ready device for reading
          */
         if (!acquire_device_for_read(dcr)) {
            goto bail_out;
         }

         /*
          * Let any SD plugin know now its time to setup the record translation infra.
          */
         if (generate_plugin_event(jcr, bsdEventSetupRecordTranslation, dcr) != bRC_OK) {
            goto bail_out;
         }

         /*
          * Keep track that we acquired the storage.
          */
         jcr->acquired_storage = true;

         /*
          * Change the Job to running state.
          */
         jcr->sendJobStatus(JS_Running);

         Dmsg1(50, "Begin reading device=%s\n", dcr->dev->print_name());

         position_device_to_first_file(jcr, dcr);
         jcr->mount_next_volume = false;

         /*
          * Allocate a new read context for this Job.
          */
         rctx = new_read_context();
         jcr->rctx = rctx;

         /*
          * Read the first block and setup record processing.
          */
         if (!read_next_block_from_device(dcr, &rctx->sessrec,
                                          NULL, mount_next_read_volume, &ok)) {
            Jmsg1(jcr, M_FATAL, 0,
                  _("Read session label failed. ERR=%s\n"),
                  dcr->dev->bstrerror());
            goto bail_out;
         }

         read_context_set_record(dcr, rctx);
         rctx->records_processed = 0;
         rctx->rec->state_bits = 0;
         rctx->lastFileIndex = READ_NO_FILEINDEX;
      }
   }

   /*
    * Setup NDMP states.
    */
   ta = sess->tape_acb;
   ta->tape_fd = 0; /* fake filedescriptor */
   bzero (&ta->tape_state, sizeof ta->tape_state);
   ta->tape_state.error = NDMP9_NO_ERR;
   ta->tape_state.state = NDMP9_TAPE_STATE_OPEN;
   ta->tape_state.open_mode = will_write ?
                              NDMP9_TAPE_RDWR_MODE :
                              NDMP9_TAPE_READ_MODE;
   ta->tape_state.file_num.valid = NDMP9_VALIDITY_VALID;
   ta->tape_state.soft_errors.valid = NDMP9_VALIDITY_VALID;
   ta->tape_state.block_size.valid = NDMP9_VALIDITY_VALID;
   ta->tape_state.blockno.valid = NDMP9_VALIDITY_VALID;
   ta->tape_state.total_space.valid = NDMP9_VALIDITY_INVALID;
   ta->tape_state.space_remain.valid = NDMP9_VALIDITY_INVALID;

   return NDMP9_NO_ERR;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);
   return NDMP9_NO_DEVICE_ERR;
}

extern "C" ndmp9_error bndmp_tape_close(struct ndm_session *sess)
{
   JCR *jcr;
   ndmp9_error err;
   struct ndmp_session_handle *handle;
   struct ndm_tape_agent *ta = sess->tape_acb;
   char ndmp_seperator[] = { "NdMpSePeRaToR" };

   if (ta->tape_fd < 0) {
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   bndmp_simu_flush_weof(sess);

   /*
    * Setup the glue between the NDMP layer and the Storage Daemon.
    */
   handle = (struct ndmp_session_handle *)sess->session_handle;

   jcr = handle->jcr;
   if (!jcr) {
      Jmsg0(NULL, M_FATAL, 0, _("JCR is NULL!!!\n"));
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   err = NDMP9_NO_ERR;
   if (NDMTA_TAPE_IS_WRITABLE(ta)) {
      /*
       * Write a seperator record so on restore we can recognize the different
       * NDMP datastreams from each other.
       */
      if (!bndmp_write_data_to_block(jcr, STREAM_NDMP_SEPERATOR, ndmp_seperator, 13)) {
         err = NDMP9_IO_ERR;
      }
   }

   pthread_cond_signal(&jcr->job_end_wait); /* wake any waiting thread */

   ta->tape_fd = -1;
   ndmos_tape_initialize(sess);

   return err;
}

extern "C" ndmp9_error bndmp_tape_mtio(struct ndm_session *sess,
                                       ndmp9_tape_mtio_op op,
                                       u_long count,
                                       u_long *resid)
{
   struct ndm_tape_agent *ta = sess->tape_acb;

   *resid = 0;

   if (ta->tape_fd < 0) {
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   /*
    * audit for valid op and for tape mode
    */
   switch (op) {
   case NDMP9_MTIO_FSF:
      return NDMP9_NO_ERR;

   case NDMP9_MTIO_BSF:
      return NDMP9_NO_ERR;

   case NDMP9_MTIO_FSR:
      return NDMP9_NO_ERR;

   case NDMP9_MTIO_BSR:
      return NDMP9_NO_ERR;

   case NDMP9_MTIO_REW:
      bndmp_simu_flush_weof(sess);
      *resid = 0;
      ta->tape_state.file_num.value = 0;
      ta->tape_state.blockno.value = 0;
      break;

   case NDMP9_MTIO_OFF:
      return NDMP9_NO_ERR;

   case NDMP9_MTIO_EOF:      /* should be "WFM" write-file-mark */
      return NDMP9_NO_ERR;

   default:
      return NDMP9_ILLEGAL_ARGS_ERR;
   }

   return NDMP9_NO_ERR;
}

extern "C" ndmp9_error bndmp_tape_write(struct ndm_session *sess,
                                        char *buf,
                                        u_long count,
                                        u_long *done_count)
{
   JCR *jcr;
   ndmp9_error err;
   struct ndmp_session_handle *handle;
   struct ndm_tape_agent *ta = sess->tape_acb;

   if (ta->tape_fd < 0) {
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
      return NDMP9_PERMISSION_ERR;
   }

   /*
    * Setup the glue between the NDMP layer and the Storage Daemon.
    */
   handle = (struct ndmp_session_handle *)sess->session_handle;

   jcr = handle->jcr;
   if (!jcr) {
      Jmsg0(NULL, M_FATAL, 0, _("JCR is NULL!!!\n"));
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   /*
    * Turn the NDMP data into a internal record and save it.
    */
   if (bndmp_write_data_to_block(jcr, STREAM_FILE_DATA, buf, count)) {
      ta->tape_state.blockno.value++;
      *done_count = count;
      err = NDMP9_NO_ERR;
   } else {
      jcr->setJobStatus(JS_ErrorTerminated);
      err = NDMP9_IO_ERR;
   }

   ta->weof_on_close = 1;

   return err;
}

extern "C" ndmp9_error bndmp_tape_wfm(struct ndm_session *sess)
{
   ndmp9_error err;
   struct ndm_tape_agent *ta = sess->tape_acb;

   ta->weof_on_close = 0;

   if (ta->tape_fd < 0) {
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
      return NDMP9_PERMISSION_ERR;
   }

   err = NDMP9_NO_ERR;

   return err;
}

extern "C" ndmp9_error bndmp_tape_read(struct ndm_session *sess,
                                       char *buf,
                                       u_long count,
                                       u_long *done_count)
{
   JCR *jcr;
   ndmp9_error err;
   struct ndmp_session_handle *handle;
   struct ndm_tape_agent *ta = sess->tape_acb;

   if (ta->tape_fd < 0) {
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   /*
    * Setup the glue between the NDMP layer and the Storage Daemon.
    */
   handle = (struct ndmp_session_handle *)sess->session_handle;

   jcr = handle->jcr;
   if (!jcr) {
      Jmsg0(NULL, M_FATAL, 0, _("JCR is NULL!!!\n"));
      return NDMP9_DEV_NOT_OPEN_ERR;
   }

   if (bndmp_read_data_from_block(jcr, buf, count, done_count)) {
      ta->tape_state.blockno.value++;
      if (*done_count == 0) {
         err = NDMP9_EOF_ERR;
      } else {
         err = NDMP9_NO_ERR;
      }
   } else {
      jcr->setJobStatus(JS_ErrorTerminated);
      err = NDMP9_IO_ERR;
   }

   return err;
}

static inline void register_callback_hooks(void)
{
   struct ndm_auth_callbacks auth_callbacks;
   struct ndm_tape_simulator_callbacks tape_callbacks;

   /*
    * Register the authentication callbacks.
    */
   auth_callbacks.validate_password = bndmp_auth_clear;
   auth_callbacks.validate_md5 = bndmp_auth_md5;
   ndmos_auth_register_callbacks(&auth_callbacks);

   /*
    * Register the tape simulator callbacks.
    */
   tape_callbacks.tape_open = bndmp_tape_open;
   tape_callbacks.tape_close = bndmp_tape_close;
   tape_callbacks.tape_mtio = bndmp_tape_mtio;
   tape_callbacks.tape_write = bndmp_tape_write;
   tape_callbacks.tape_wfm = bndmp_tape_wfm;
   tape_callbacks.tape_read = bndmp_tape_read;
   ndmos_tape_register_callbacks(&tape_callbacks);
}

static inline void unregister_callback_hooks(void)
{
   ndmos_tape_unregister_callbacks();
   ndmos_auth_unregister_callbacks();
}

void end_of_ndmp_backup(JCR *jcr)
{
   DCR *dcr = jcr->dcr;
   char ec[50];

   /*
    * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
    * and the subsequent Jmsg() editing will break
    */
   int32_t job_elapsed = time(NULL) - jcr->run_time;

   if (job_elapsed <= 0) {
      job_elapsed = 1;
   }

   Jmsg(dcr->jcr, M_INFO, 0, _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
        job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
        edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec));


   /*
    * Check if we can still write. This may not be the case
    *  if we are at the end of the tape or we got a fatal I/O error.
    */
   if (dcr->dev->can_write()) {
      Dmsg1(200, "Write EOS label JobStatus=%c\n", jcr->JobStatus);

      if (!write_session_label(dcr, EOS_LABEL)) {
         /*
          * Print only if JobStatus JS_Terminated and not cancelled to avoid spurious messages
          */
         if (jcr->is_JobStatus(JS_Terminated) && !jcr->is_job_canceled()) {
            Jmsg1(jcr, M_FATAL, 0,
                  _("Error writing end session label. ERR=%s\n"),
                  dcr->dev->bstrerror());
         }
         jcr->setJobStatus(JS_ErrorTerminated);
      }

      Dmsg0(90, "back from write_end_session_label()\n");

      /*
       * Flush out final partial block of this session
       */
      if (!dcr->write_block_to_device()) {
         /*
          * Print only if JobStatus JS_Terminated and not cancelled to avoid spurious messages
          */
         if (jcr->is_JobStatus(JS_Terminated) && !jcr->is_job_canceled()) {
            Jmsg2(jcr, M_FATAL, 0,
                  _("Fatal append error on device %s: ERR=%s\n"),
                  dcr->dev->print_name(), dcr->dev->bstrerror());
         }
         jcr->setJobStatus(JS_ErrorTerminated);
      }
   }

   if (jcr->is_JobStatus(JS_Terminated)) {
      /*
       * Note: if commit is OK, the device will remain blocked
       */
      commit_data_spool(dcr);
   } else {
      discard_data_spool(dcr);
   }

   /*
    * Release the device -- and send final Vol info to DIR and unlock it.
    */
   if (jcr->acquired_storage) {
      release_device(dcr);
      jcr->acquired_storage = false;
   } else {
      dcr->unreserve_device();
   }

   jcr->sendJobStatus();              /* update director */
}

void end_of_ndmp_restore(JCR *jcr)
{
   if (jcr->rctx) {
      free_read_context(jcr->rctx);
      jcr->rctx = NULL;
   }

   if (jcr->acquired_storage) {
      release_device(jcr->read_dcr);
      jcr->acquired_storage = false;
   } else {
      jcr->read_dcr->unreserve_device();
   }
}

extern "C" void *handle_ndmp_client_request(void *arg)
{
   int status;
   struct ndmconn *conn;
   struct ndm_session *sess;
   struct ndmp_session_handle *handle;
   NIS *nis;

   handle = (struct ndmp_session_handle *)arg;
   if (!handle) {
      Emsg0(M_ABORT, 0,
            _("Illegal call to handle_ndmp_client_request with NULL session handle\n"));
      return NULL;
   }

   /*
    * Initialize a new NDMP session
    */
   sess = (struct ndm_session *)malloc(sizeof(struct ndm_session));
   memset(sess, 0, sizeof(struct ndm_session));

   sess->param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(sess->param, 0, sizeof(struct ndm_session_param));
   sess->param->log.deliver = ndmp_loghandler;
   nis = (NIS *)malloc(sizeof(NIS));
   sess->param->log.ctx = nis;
   sess->param->log_level = native_to_ndmp_loglevel(debug_level, nis);
   sess->param->log_tag = bstrdup("SD-NDMP");

   /*
    * We duplicate some of the code from the ndma server session handling available
    * in the NDMJOB NDMP library e.g. we do not enter via ndma_daemon_session()
    * and then continue to ndma_server_session() which is the normal entry point
    * into the library as the ndma_daemon_session() function does things like creating
    * a listen socket, fork and accept the connection and the ndma_server_session()
    * function tries to get peername and socket names before eventually establishing
    * the NDMP connection. We already do all of that ourself via proven code
    * implemented in ndmp_thread_server which is calling us.
    */

   /*
    * Make the ndm_session usable for a new connection e.g. initialize and commission.
    */
   sess->tape_agent_enabled = 1;
   sess->data_agent_enabled = 1;

   status = ndma_session_initialize(sess);
   if (status) {
      Emsg0(M_ABORT, 0,
            _("Cannot initialize new NDMA session\n"));
      goto bail_out;
   }

   status = ndma_session_commission(sess);
   if (status) {
      Emsg0(M_ABORT, 0,
            _("Cannot commission new NDMA session\n"));
      goto bail_out;
   }

   conn = ndmconn_initialize(0, (char *)"#C");
   if (!conn) {
      Emsg0(M_ABORT, 0,
            _("Cannot initialize new NDMA connection\n"));
      goto bail_out;
   }

   /*
    * Tell the lower levels which socket to use and setup snooping.
    */
   ndmos_condition_control_socket(sess, handle->fd);
   if (me->ndmp_snooping) {
      ndmconn_set_snoop(conn, &sess->param->log, sess->param->log_level);
   }
   ndmconn_accept(conn, handle->fd);

   /*
    * Initialize some members now that we have a initialized NDMP connection.
    */
   conn->call = ndma_call;
   conn->context = sess;
   sess->plumb.control = conn;
   sess->session_handle = handle;

   /*
    * This does the actual work e.g. run through the NDMP state machine.
    */
   while (!conn->chan.eof) {
      ndma_session_quantum(sess, 1000);
   }

   /*
    * Tear down the NDMP connection.
    */
   ndmconn_destruct(conn);
   ndma_session_decommission(sess);

bail_out:
   free(sess->param->log.ctx);
   free(sess->param->log_tag);
   free(sess->param);
   free(sess);

   close(handle->fd);
   if (handle->jcr) {
      free_jcr(handle->jcr);
   }
   free(handle);

   return NULL;
}

/*
 * Create a seperate thread that accepts NDMP connections.
 * We don't use the Bareos native bnet_thread_server_tcp which
 * uses the bsock class which is a bit to much overhead
 * for simple sockets which we need and has all kinds of
 * extra features likes TLS and keep-alive support etc.
 * which wouldn't work for NDMP anyhow.
 *
 * So this is a bnet_thread_server_tcp put on a diet which
 * also contains the absolute minimum code needed to
 * have a robust connection handler.
 */
extern "C" void *ndmp_thread_server(void *arg)
{
   struct ndmp_thread_server_args *ntsa;
   int new_sockfd, status;
   socklen_t clilen;
   struct sockaddr cli_addr;       /* client's address */
   int tlog, tmax;
   int turnon = 1;
#ifdef HAVE_LIBWRAP
   struct request_info request;
#endif
   IPADDR *ipaddr, *next;
   struct s_sockfd {
      dlink link;                  /* this MUST be the first item */
      int fd;
      int port;
   } *fd_ptr = NULL;
   char buf[128];
   alist sockfds(10, not_owned_by_alist);
#ifdef HAVE_POLL
   nfds_t nfds;
   struct pollfd *pfds;
#endif

   ntsa = (struct ndmp_thread_server_args *)arg;
   if (!ntsa) {
      return NULL;
   }

   /*
    * Remove any duplicate addresses.
    */
   for (ipaddr = (IPADDR *)ntsa->addr_list->first(); ipaddr;
        ipaddr = (IPADDR *)ntsa->addr_list->next(ipaddr)) {
      for (next = (IPADDR *)ntsa->addr_list->next(ipaddr); next;
           next = (IPADDR *)ntsa->addr_list->next(next)) {
         if (ipaddr->get_sockaddr_len() == next->get_sockaddr_len() &&
             memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                    ipaddr->get_sockaddr_len()) == 0) {
            ntsa->addr_list->remove(next);
         }
      }
   }

   char allbuf[256 * 10];
   Dmsg1(100, "Addresses %s\n", build_addresses_str(ntsa->addr_list, allbuf, sizeof(allbuf)));

#ifdef HAVE_POLL
   nfds = 0;
#endif
   foreach_dlist(ipaddr, ntsa->addr_list) {
      /*
       * Allocate on stack from -- no need to free
       */
      fd_ptr = (s_sockfd *)alloca(sizeof(s_sockfd));
      fd_ptr->port = ipaddr->get_port_net_order();

      /*
       * Open a TCP socket
       */
      for (tlog= 60; (fd_ptr->fd = socket(ipaddr->get_family(), SOCK_STREAM, 0)) < 0; tlog -= 10) {
         if (tlog <= 0) {
            berrno be;
            char curbuf[256];
            Emsg3(M_ABORT, 0,
                  _("Cannot open stream socket. ERR=%s. Current %s All %s\n"),
                  be.bstrerror(),
                  ipaddr->build_address_str(curbuf, sizeof(curbuf)),
                  build_addresses_str(ntsa->addr_list, allbuf, sizeof(allbuf)));
         }
         bmicrosleep(10, 0);
      }

      /*
       * Reuse old sockets
       */
      if (setsockopt(fd_ptr->fd, SOL_SOCKET, SO_REUSEADDR,
                    (sockopt_val_t)&turnon, sizeof(turnon)) < 0) {
         berrno be;
         Emsg1(M_WARNING, 0,
               _("Cannot set SO_REUSEADDR on socket: %s\n"),
               be.bstrerror());
      }

      tmax = 30 * (60 / 5);    /* wait 30 minutes max */
      for (tlog = 0; bind(fd_ptr->fd, ipaddr->get_sockaddr(), ipaddr->get_sockaddr_len()) < 0; tlog -= 5) {
         berrno be;
         if (tlog <= 0) {
            tlog = 2 * 60;         /* Complain every 2 minutes */
            Emsg2(M_WARNING, 0,
                  _("Cannot bind port %d: ERR=%s: Retrying ...\n"),
                  ntohs(fd_ptr->port), be.bstrerror());
         }
         bmicrosleep(5, 0);
         if (--tmax <= 0) {
            Emsg2(M_ABORT, 0,
                  _("Cannot bind port %d: ERR=%s.\n"), ntohs(fd_ptr->port),
                  be.bstrerror());
         }
      }
      listen(fd_ptr->fd, 50);      /* tell system we are ready */
      sockfds.append(fd_ptr);
#ifdef HAVE_POLL
      nfds++;
#endif
   }

   /*
    * Start work queue thread
    */
   if ((status = workq_init(ntsa->client_wq, ntsa->max_clients, handle_ndmp_client_request)) != 0) {
      berrno be;
      be.set_errno(status);
      Emsg1(M_ABORT, 0,
            _("Could not init ndmp client queue: ERR=%s\n"),
            be.bstrerror());
   }

#ifdef HAVE_POLL
   /*
    * Allocate on stack from -- no need to free
    */
   pfds = (struct pollfd *)alloca(sizeof(struct pollfd) * nfds);
   memset(pfds, 0, sizeof(struct pollfd) * nfds);

   nfds = 0;
   foreach_alist(fd_ptr, &sockfds) {
      pfds[nfds].fd = fd_ptr->fd;
      pfds[nfds].events |= POLL_IN;
      nfds++;
   }
#endif

   register_callback_hooks();

   /*
    * Wait for a connection from the client process.
    */
   while (!quit) {
#ifndef HAVE_POLL
      unsigned int maxfd = 0;
      fd_set sockset;
      FD_ZERO(&sockset);

      foreach_alist(fd_ptr, &sockfds) {
         FD_SET((unsigned)fd_ptr->fd, &sockset);
         maxfd = maxfd > (unsigned)fd_ptr->fd ? maxfd : fd_ptr->fd;
      }

      errno = 0;
      if ((status = select(maxfd + 1, &sockset, NULL, NULL, NULL)) < 0) {
         berrno be;                   /* capture errno */
         if (errno == EINTR) {
            continue;
         }
         Emsg1(M_FATAL, 0, _("Error in select: %s\n"), be.bstrerror());
         break;
      }

      foreach_alist(fd_ptr, &sockfds) {
         if (FD_ISSET(fd_ptr->fd, &sockset)) {
#else
      int cnt;

      errno = 0;
      if ((status = poll(pfds, nfds, -1)) < 0) {
         berrno be;                   /* capture errno */
         if (errno == EINTR) {
            continue;
         }
         Emsg1(M_FATAL, 0, _("Error in poll: %s\n"), be.bstrerror());
         break;
      }

      cnt = 0;
      foreach_alist(fd_ptr, &sockfds) {
         if (pfds[cnt++].revents & POLLIN) {
#endif
            /*
             * Got a connection, now accept it.
             */
            do {
               clilen = sizeof(cli_addr);
               new_sockfd = accept(fd_ptr->fd, &cli_addr, &clilen);
            } while (new_sockfd < 0 && errno == EINTR);
            if (new_sockfd < 0) {
               continue;
            }
#ifdef HAVE_LIBWRAP
            P(mutex);              /* hosts_access is not thread safe */
            request_init(&request, RQ_DAEMON, my_name, RQ_FILE, new_sockfd, 0);
            fromhost(&request);
            if (!hosts_access(&request)) {
               V(mutex);
               Jmsg2(NULL, M_SECURITY, 0,
                     _("Connection from %s:%d refused by hosts.access\n"),
                     sockaddr_to_ascii(&cli_addr, buf, sizeof(buf)),
                     sockaddr_get_port(&cli_addr));
               close(new_sockfd);
               continue;
            }
            V(mutex);
#endif

            /*
             * Receive notification when connection dies.
             */
            if (setsockopt(new_sockfd, SOL_SOCKET, SO_KEEPALIVE,
                          (sockopt_val_t)&turnon, sizeof(turnon)) < 0) {
               berrno be;
               Emsg1(M_WARNING, 0,
                     _("Cannot set SO_KEEPALIVE on socket: %s\n"),
                     be.bstrerror());
            }

            /*
             * See who client is. i.e. who connected to us.
             */
            P(mutex);
            sockaddr_to_ascii(&cli_addr, buf, sizeof(buf));
            V(mutex);

            struct ndmp_session_handle *new_handle;
            new_handle = (struct ndmp_session_handle *)malloc(sizeof(struct ndmp_session_handle));
            memset(new_handle, 0, sizeof(struct ndmp_session_handle));
            new_handle->fd = new_sockfd;
            new_handle->host = bstrdup(buf);
            memset(&new_handle->peer_addr, 0, sizeof(new_handle->peer_addr));
            memcpy(&new_handle->client_addr, &cli_addr, sizeof(new_handle->client_addr));

            /*
             * Queue client to be served
             */
            if ((status = workq_add(ntsa->client_wq, (void *)new_handle, NULL, 0)) != 0) {
               berrno be;
               be.set_errno(status);
               Jmsg1(NULL, M_ABORT, 0, _("Could not add job to ndmp client queue: ERR=%s\n"),
                     be.bstrerror());
            }
         }
      }
   }

   /*
    * Cleanup open files.
    */
   fd_ptr = (s_sockfd *)sockfds.first();
   while (fd_ptr) {
      close(fd_ptr->fd);
      fd_ptr = (s_sockfd *)sockfds.next();
   }

   /*
    * Stop work queue thread
    */
   if ((status = workq_destroy(ntsa->client_wq)) != 0) {
      berrno be;
      be.set_errno(status);
      Emsg1(M_FATAL, 0,
            _("Could not destroy ndmp client queue: ERR=%s\n"),
            be.bstrerror());
   }

   unregister_callback_hooks();

   return NULL;
}

int start_ndmp_thread_server(dlist *addr_list, int max_clients, workq_t *client_wq)
{
   int status;

   ndmp_thread_server_args.addr_list = addr_list;
   ndmp_thread_server_args.max_clients = max_clients;
   ndmp_thread_server_args.client_wq = client_wq;

   if ((status = pthread_create(&ndmp_tid, NULL, ndmp_thread_server,
                               (void *)&ndmp_thread_server_args)) != 0) {
      return status;
   }

   ndmp_initialized = true;

   return 0;
}

void stop_ndmp_thread_server()
{
   if (!ndmp_initialized) {
      return;
   }

   quit = true;
   if (!pthread_equal(ndmp_tid, pthread_self())) {
      pthread_join(ndmp_tid, NULL);
   }
}
#else
void end_of_ndmp_backup(JCR *jcr)
{
}

void end_of_ndmp_restore(JCR *jcr)
{
}
#endif /* HAVE_NDMP */
