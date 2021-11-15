/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */


#include "ndmos.h" /* rpc/rpc.h */
#include "ndmprotocol.h"


#ifndef NDMOS_OPTION_NO_NDMP2


int ndmp2_pp_header(void* data, char* buf)
{
  ndmp2_header* mh = (ndmp2_header*)data;

  if (mh->message_type == NDMP2_MESSAGE_REQUEST) {
    sprintf(buf, "C %s %lu", ndmp2_message_to_str(mh->message), mh->sequence);
  } else if (mh->message_type == NDMP2_MESSAGE_REPLY) {
    sprintf(buf, "R %s %lu (%lu)", ndmp2_message_to_str(mh->message),
            mh->reply_sequence, mh->sequence);
    if (mh->error != NDMP2_NO_ERR) {
      sprintf(NDMOS_API_STREND(buf), " %s", ndmp2_error_to_str(mh->error));
      return 0; /* no body */
    }
  } else {
    strcpy(buf, "??? INVALID MESSAGE TYPE");
    return -1; /* no body */
  }
  return 1; /* body */
}

int ndmp2_pp_mover_addr(char* buf, ndmp2_mover_addr* ma)
{
  sprintf(buf, "%s", ndmp2_mover_addr_type_to_str(ma->addr_type));
  if (ma->addr_type == NDMP2_ADDR_TCP) {
    sprintf(NDMOS_API_STREND(buf), "(%lx,%d)",
            ma->ndmp2_mover_addr_u.addr.ip_addr,
            ma->ndmp2_mover_addr_u.addr.port);
  }
  return 0;
}


int ndmp2_pp_request(ndmp2_message msg, void* data, int lineno, char* buf)
{
  int i;
  unsigned int j;

  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP2_CONNECT_OPEN:
      NDMP_PP_WITH(ndmp2_connect_open_request)
      sprintf(buf, "version=%d", p->protocol_version);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONNECT_CLIENT_AUTH:
      NDMP_PP_WITH(ndmp2_connect_client_auth_request)
      sprintf(buf, "auth_type=%s",
              ndmp2_auth_type_to_str(p->auth_data.auth_type));
      switch (p->auth_data.auth_type) {
        case NDMP2_AUTH_NONE:
          break;

        case NDMP2_AUTH_TEXT:
          sprintf(NDMOS_API_STREND(buf), " auth_id=%s",
                  p->auth_data.ndmp2_auth_data_u.auth_text.auth_id);
          break;

        case NDMP2_AUTH_MD5:
          sprintf(NDMOS_API_STREND(buf), " auth_id=%s",
                  p->auth_data.ndmp2_auth_data_u.auth_md5.auth_id);
          break;

        default:
          sprintf(NDMOS_API_STREND(buf), " ????");
          break;
      }
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONNECT_CLOSE:
    case NDMP2_CONFIG_GET_HOST_INFO:
    case NDMP2_CONFIG_GET_MOVER_TYPE:
    case NDMP2_SCSI_CLOSE:
    case NDMP2_SCSI_GET_STATE:
    case NDMP2_SCSI_RESET_DEVICE:
    case NDMP2_SCSI_RESET_BUS:
    case NDMP2_TAPE_GET_STATE:
    case NDMP2_TAPE_CLOSE:
    case NDMP2_MOVER_GET_STATE:
    case NDMP2_MOVER_CONTINUE:
    case NDMP2_MOVER_ABORT:
    case NDMP2_MOVER_STOP:
    case NDMP2_MOVER_CLOSE:
    case NDMP2_DATA_GET_STATE:
    case NDMP2_DATA_ABORT:
    case NDMP2_DATA_STOP:
    case NDMP2_DATA_GET_ENV:
      *buf = 0; /* no body */
      return 0;

    case NDMP2_CONNECT_SERVER_AUTH:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP2_CONFIG_GET_BUTYPE_ATTR:
      NDMP_PP_WITH(ndmp2_config_get_butype_attr_request)
      sprintf(buf, "bu_type='%s'", p->name);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONFIG_GET_AUTH_ATTR:
      NDMP_PP_WITH(ndmp2_config_get_auth_attr_request)
      sprintf(buf, "auth_type=%s", ndmp2_auth_type_to_str(p->auth_type));
      NDMP_PP_ENDWITH
      break;

    case NDMP2_SCSI_OPEN:
      NDMP_PP_WITH(ndmp2_scsi_open_request)
      sprintf(buf, "device='%s'", p->device.name);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_SCSI_SET_TARGET:
      NDMP_PP_WITH(ndmp2_scsi_set_target_request)
      sprintf(buf, "device='%s' cont=%d sid=%d lun=%d", p->device.name,
              p->target_controller, p->target_id, p->target_lun);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_SCSI_EXECUTE_CDB:
    case NDMP2_TAPE_EXECUTE_CDB:
      NDMP_PP_WITH(ndmp2_execute_cdb_request)
      switch (lineno) {
        case 0:
          sprintf(buf, "flags=0x%lx timeout=%ld datain_len=%ld", p->flags,
                  p->timeout, p->datain_len);
          break;
        case 1:
          sprintf(buf, "cmd[%d]={", p->cdb.cdb_len);
          for (j = 0; j < p->cdb.cdb_len; j++) {
            sprintf(NDMOS_API_STREND(buf), " %02x", p->cdb.cdb_val[j] & 0xFF);
          }
          strcat(buf, " }");
          break;
      }
      return 2;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_OPEN:
      NDMP_PP_WITH(ndmp2_tape_open_request)
      sprintf(buf, "device='%s' mode=%s", p->device.name,
              ndmp2_tape_open_mode_to_str(p->mode));
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_MTIO:
      NDMP_PP_WITH(ndmp2_tape_mtio_request)
      sprintf(buf, "op=%s count=%ld", ndmp2_tape_mtio_op_to_str(p->tape_op),
              p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_WRITE:
      NDMP_PP_WITH(ndmp2_tape_write_request)
      sprintf(buf, "data_out_len=%d", p->data_out.data_out_len);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_READ:
      NDMP_PP_WITH(ndmp2_tape_read_request)
      sprintf(buf, "count=%ld", p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_DATA_START_BACKUP:
      NDMP_PP_WITH(ndmp2_data_start_backup_request)
      if (lineno == 0) {
        sprintf(buf, "bu_type='%s' n_env=%d mover=", p->bu_type,
                p->env.env_len);
        ndmp2_pp_mover_addr(NDMOS_API_STREND(buf), &p->mover);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->env.env_len) {
          sprintf(buf, "env[%d] name='%s' value='%s'", i,
                  p->env.env_val[i].name, p->env.env_val[i].value);
        } else {
          strcpy(buf, "--INVALID--");
        }
      }
      return 1 + p->env.env_len;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_DATA_START_RECOVER:
    case NDMP2_DATA_START_RECOVER_FILEHIST:
      NDMP_PP_WITH(ndmp2_data_start_recover_request)
      if (lineno == 0) {
        sprintf(buf, "bu_type='%s' n_env=%d n_nlist=%d mover=", p->bu_type,
                p->env.env_len, p->nlist.nlist_len);
        ndmp2_pp_mover_addr(NDMOS_API_STREND(buf), &p->mover);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->env.env_len) {
          sprintf(buf, "env[%d] name='%s' value='%s'", i,
                  p->env.env_val[i].name, p->env.env_val[i].value);
        } else {
          i -= p->env.env_len;
          if (0 <= i && (unsigned)i < p->nlist.nlist_len) {
            sprintf(buf, "nl[%d] name='%s' fhi=%lld dest='%s'", i,
                    p->nlist.nlist_val[i].name, p->nlist.nlist_val[i].fh_info,
                    p->nlist.nlist_val[i].dest);
          } else {
            strcpy(buf, "--INVALID--");
          }
        }
      }
      return 1 + p->env.env_len + p->nlist.nlist_len;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_DATA_HALTED:
      NDMP_PP_WITH(ndmp2_notify_data_halted_request)
      sprintf(buf, "reason=%s text_reason='%s'",
              ndmp2_data_halt_reason_to_str(p->reason), p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_CONNECTED:
      NDMP_PP_WITH(ndmp2_notify_connected_request)
      sprintf(buf, "reason=%s protocol_version=%d text_reason='%s'",
              ndmp2_connect_reason_to_str(p->reason), p->protocol_version,
              p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_MOVER_HALTED:
      NDMP_PP_WITH(ndmp2_notify_mover_halted_request)
      sprintf(buf, "reason=%s text_reason='%s'",
              ndmp2_mover_halt_reason_to_str(p->reason), p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_MOVER_PAUSED:
      NDMP_PP_WITH(ndmp2_notify_mover_paused_request)
      sprintf(buf, "reason=%s seek_position=%lld",
              ndmp2_mover_pause_reason_to_str(p->reason), p->seek_position);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_DATA_READ:
      NDMP_PP_WITH(ndmp2_notify_data_read_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_LOG_LOG:
      NDMP_PP_WITH(ndmp2_log_log_request)
      sprintf(buf, "entry='%s'", p->entry);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_LOG_DEBUG:
      NDMP_PP_WITH(ndmp2_log_debug_request)
      sprintf(buf, "level=%s message='%s'", ndmp2_debug_level_to_str(p->level),
              p->message);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_LOG_FILE:
      NDMP_PP_WITH(ndmp2_log_file_request)
      sprintf(buf, "file=%s error=%s", p->name, ndmp2_error_to_str(p->error));
      NDMP_PP_ENDWITH
      break;

    case NDMP2_FH_ADD_UNIX_PATH:
      NDMP_PP_WITH(ndmp2_fh_add_unix_path_request)
      if (lineno == 0) {
        sprintf(buf, "n_paths=%d", p->paths.paths_len);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->paths.paths_len) {
          struct ndmp2_fh_unix_path* pa;

          pa = &p->paths.paths_val[i];
          sprintf(buf, "[%d] %-15s %7llu %s [%lld]", i,
                  ndmp2_unix_file_type_to_str(pa->fstat.ftype), pa->fstat.size,
                  pa->name, pa->fstat.fh_info);
        } else {
          strcpy(buf, "--INVALID--");
        }
      }
      return 1 + p->paths.paths_len;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_FH_ADD_UNIX_DIR:
      NDMP_PP_WITH(ndmp2_fh_add_unix_dir_request)
      if (lineno == 0) {
        sprintf(buf, "n_dirs=%d", p->dirs.dirs_len);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->dirs.dirs_len) {
          struct ndmp2_fh_unix_dir* de;

          de = &p->dirs.dirs_val[i];
          sprintf(buf, "[%d] %lu %lu %s", i, de->node, de->parent, de->name);
        } else {
          strcpy(buf, "--INVALID--");
        }
      }
      return 1 + p->dirs.dirs_len;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_FH_ADD_UNIX_NODE:
      NDMP_PP_WITH(ndmp2_fh_add_unix_node_request)
      if (lineno == 0) {
        sprintf(buf, "n_nodes=%d", p->nodes.nodes_len);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->nodes.nodes_len) {
          struct ndmp2_fh_unix_node* nd;

          nd = &p->nodes.nodes_val[i];
          sprintf(buf, "[%d] %-15s %7llu %lu [%lld]", i,
                  ndmp2_unix_file_type_to_str(nd->fstat.ftype), nd->fstat.size,
                  nd->node, nd->fstat.fh_info);
        } else {
          strcpy(buf, "--INVALID--");
        }
      }
      return 1 + p->nodes.nodes_len;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_MOVER_LISTEN:
      NDMP_PP_WITH(ndmp2_mover_listen_request)
      sprintf(buf, "mode=%s addr_type=%s", ndmp2_mover_mode_to_str(p->mode),
              ndmp2_mover_addr_type_to_str(p->addr_type));
      NDMP_PP_ENDWITH
      break;

    case NDMP2_MOVER_SET_WINDOW:
      NDMP_PP_WITH(ndmp2_mover_set_window_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_MOVER_READ:
      NDMP_PP_WITH(ndmp2_mover_read_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_MOVER_SET_RECORD_SIZE:
      NDMP_PP_WITH(ndmp2_mover_set_record_size_request)
      sprintf(buf, "len=%lu", p->len);
      NDMP_PP_ENDWITH
      break;
  }
  return 1; /* one line in buf */
}


int ndmp2_pp_reply(ndmp2_message msg, void* data, int lineno, char* buf)
{
  int i;
  unsigned int j;

  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP2_CONNECT_OPEN:
    case NDMP2_CONNECT_CLIENT_AUTH:
    case NDMP2_SCSI_OPEN:
    case NDMP2_SCSI_CLOSE:
    case NDMP2_SCSI_SET_TARGET:
    case NDMP2_SCSI_RESET_DEVICE:
    case NDMP2_SCSI_RESET_BUS:
    case NDMP2_TAPE_OPEN:
    case NDMP2_TAPE_CLOSE:
    case NDMP2_MOVER_CONTINUE:
    case NDMP2_MOVER_ABORT:
    case NDMP2_MOVER_STOP:
    case NDMP2_MOVER_READ:
    case NDMP2_MOVER_SET_WINDOW:
    case NDMP2_MOVER_CLOSE:
    case NDMP2_MOVER_SET_RECORD_SIZE:
    case NDMP2_DATA_START_BACKUP:
    case NDMP2_DATA_START_RECOVER:
    case NDMP2_DATA_START_RECOVER_FILEHIST:
    case NDMP2_DATA_ABORT:
    case NDMP2_DATA_STOP:
      NDMP_PP_WITH(ndmp2_error)
      sprintf(buf, "error=%s", ndmp2_error_to_str(*p));
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONNECT_CLOSE:
      *buf = 0;
      return 0;

    case NDMP2_CONNECT_SERVER_AUTH:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP2_CONFIG_GET_HOST_INFO:
      NDMP_PP_WITH(ndmp2_config_get_host_info_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s hostname=%s", ndmp2_error_to_str(p->error),
                  p->hostname);
          break;
        case 1:
          sprintf(buf, "os_type=%s os_vers=%s hostid=%s", p->os_type,
                  p->os_vers, p->hostid);
          break;
        case 2:
          sprintf(buf, "auth_type[%d]={", p->auth_type.auth_type_len);
          for (j = 0; j < p->auth_type.auth_type_len; j++) {
            sprintf(NDMOS_API_STREND(buf), " %s",
                    ndmp2_auth_type_to_str(p->auth_type.auth_type_val[j]));
          }
          strcat(buf, " }");
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 3;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONFIG_GET_BUTYPE_ATTR:
      NDMP_PP_WITH(ndmp2_config_get_butype_attr_reply)
      sprintf(buf, "error=%s attrs=0x%lx", ndmp2_error_to_str(p->error),
              p->attrs);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONFIG_GET_MOVER_TYPE:
      NDMP_PP_WITH(ndmp2_config_get_mover_type_reply)
      sprintf(buf, "error=%s methods[%d]={", ndmp2_error_to_str(p->error),
              p->methods.methods_len);
      for (j = 0; j < p->methods.methods_len; j++) {
        sprintf(NDMOS_API_STREND(buf), " %s",
                ndmp2_mover_addr_type_to_str(p->methods.methods_val[j]));
      }
      strcat(buf, " }");
      NDMP_PP_ENDWITH
      break;

    case NDMP2_CONFIG_GET_AUTH_ATTR:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP2_SCSI_GET_STATE:
      NDMP_PP_WITH(ndmp2_scsi_get_state_reply)
      sprintf(buf, "error=%s cont=%d sid=%d lun=%d",
              ndmp2_error_to_str(p->error), p->target_controller, p->target_id,
              p->target_lun);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_SCSI_EXECUTE_CDB:
    case NDMP2_TAPE_EXECUTE_CDB:
      NDMP_PP_WITH(ndmp2_execute_cdb_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s status=%02x dataout_len=%ld datain_len=%d",
                  ndmp2_error_to_str(p->error), p->status, p->dataout_len,
                  p->datain.datain_len);
          break;
        case 1:
          sprintf(buf, "sense[%d]={", p->ext_sense.ext_sense_len);
          for (j = 0; j < p->ext_sense.ext_sense_len; j++) {
            sprintf(NDMOS_API_STREND(buf), " %02x",
                    p->ext_sense.ext_sense_val[j] & 0xFF);
          }
          strcat(buf, " }");
          break;
      }
      return 2;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_GET_STATE:
      NDMP_PP_WITH(ndmp2_tape_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s flags=0x%lx file_num=%ld",
                  ndmp2_error_to_str(p->error), p->flags, p->file_num);
          break;
        case 1:
          sprintf(buf, "soft_errors=%lu block_size=%lu blockno=%lu",
                  p->soft_errors, p->block_size, p->blockno);
          break;
        case 2:
          sprintf(buf, "total_space=%lld space_remain=%lld", p->total_space,
                  p->space_remain);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 3;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_MTIO:
      NDMP_PP_WITH(ndmp2_tape_mtio_reply)
      sprintf(buf, "error=%s resid_count=%ld", ndmp2_error_to_str(p->error),
              p->resid_count);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_WRITE:
      NDMP_PP_WITH(ndmp2_tape_write_reply)
      sprintf(buf, "error=%s count=%ld", ndmp2_error_to_str(p->error),
              p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_TAPE_READ:
      NDMP_PP_WITH(ndmp2_tape_read_reply)
      sprintf(buf, "error=%s data_in_len=%d", ndmp2_error_to_str(p->error),
              p->data_in.data_in_len);
      NDMP_PP_ENDWITH
      break;

    case NDMP2_DATA_GET_STATE:
      NDMP_PP_WITH(ndmp2_data_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s op=%s", ndmp2_error_to_str(p->error),
                  ndmp2_data_operation_to_str(p->operation));
          break;
        case 1:
          sprintf(buf, "state=%s", ndmp2_data_state_to_str(p->state));
          break;
        case 2:
          sprintf(buf, "halt_reason=%s",
                  ndmp2_data_halt_reason_to_str(p->halt_reason));
          break;
        case 3:
          sprintf(buf, "bytes_processed=%lld est_bytes_remain=%lld",
                  p->bytes_processed, p->est_bytes_remain);
          break;
        case 4:
          sprintf(buf, "est_time_remain=%ld mover=", p->est_time_remain);
          ndmp2_pp_mover_addr(NDMOS_API_STREND(buf), &p->mover);
          break;
        case 5:
          sprintf(buf, "read_offset=%lld read_length=%lld", p->read_offset,
                  p->read_length);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 6;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_DATA_GET_ENV:
      NDMP_PP_WITH(ndmp2_data_get_env_reply)
      if (lineno == 0) {
        sprintf(buf, "error=%s n_env=%d", ndmp2_error_to_str(p->error),
                p->env.env_len);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->env.env_len) {
          sprintf(buf, "[%d] name='%s' value='%s'", i, p->env.env_val[i].name,
                  p->env.env_val[i].value);
        } else {
          strcpy(buf, "--INVALID--");
        }
      }
      return p->env.env_len + 1;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_NOTIFY_DATA_HALTED:
    case NDMP2_NOTIFY_CONNECTED:
    case NDMP2_NOTIFY_MOVER_HALTED:
    case NDMP2_NOTIFY_MOVER_PAUSED:
    case NDMP2_NOTIFY_DATA_READ:
    case NDMP2_LOG_LOG:
    case NDMP2_LOG_DEBUG:
    case NDMP2_LOG_FILE:
    case NDMP2_FH_ADD_UNIX_PATH:
    case NDMP2_FH_ADD_UNIX_DIR:
    case NDMP2_FH_ADD_UNIX_NODE:
      strcpy(buf, "<<ILLEGAL REPLY>>");
      break;

    case NDMP2_MOVER_GET_STATE:
      NDMP_PP_WITH(ndmp2_mover_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s state=%s", ndmp2_error_to_str(p->error),
                  ndmp2_mover_state_to_str(p->state));
          break;
        case 1:
          sprintf(buf, "pause_reason=%s",
                  ndmp2_mover_pause_reason_to_str(p->pause_reason));
          break;
        case 2:
          sprintf(buf, "halt_reason=%s",
                  ndmp2_mover_halt_reason_to_str(p->halt_reason));
          break;
        case 3:
          sprintf(buf, "record_size=%lu record_num=%lu data_written=%lld",
                  p->record_size, p->record_num, p->data_written);
          break;
        case 4:
          sprintf(buf, "seek=%lld to_read=%lld win_off=%lld win_len=%lld",
                  p->seek_position, p->bytes_left_to_read, p->window_offset,
                  p->window_length);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 5;
      NDMP_PP_ENDWITH
      break;

    case NDMP2_MOVER_LISTEN:
      NDMP_PP_WITH(ndmp2_mover_listen_reply)
      sprintf(buf, "error=%s mover=", ndmp2_error_to_str(p->error));
      ndmp2_pp_mover_addr(NDMOS_API_STREND(buf), &p->mover);
      NDMP_PP_ENDWITH
      break;
  }

  return 1; /* one line in buf */
}

#endif /* NDMOS_OPTION_NO_NDMP2 */
