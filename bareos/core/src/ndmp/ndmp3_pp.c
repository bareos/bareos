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


#ifndef NDMOS_OPTION_NO_NDMP3


int ndmp3_pp_header(void* data, char* buf)
{
  ndmp3_header* mh = (ndmp3_header*)data;

  if (mh->message_type == NDMP3_MESSAGE_REQUEST) {
    sprintf(buf, "C %s %lu", ndmp3_message_to_str(mh->message), mh->sequence);
  } else if (mh->message_type == NDMP3_MESSAGE_REPLY) {
    sprintf(buf, "R %s %lu (%lu)", ndmp3_message_to_str(mh->message),
            mh->reply_sequence, mh->sequence);
    if (mh->error != NDMP3_NO_ERR) {
      sprintf(NDMOS_API_STREND(buf), " %s", ndmp3_error_to_str(mh->error));
      return 0; /* no body */
    }
  } else {
    strcpy(buf, "??? INVALID MESSAGE TYPE");
    return -1; /* no body */
  }
  return 1; /* body */
}

int ndmp3_pp_addr(char* buf, ndmp3_addr* ma)
{
  sprintf(buf, "%s", ndmp3_addr_type_to_str(ma->addr_type));
  if (ma->addr_type == NDMP3_ADDR_TCP) {
    sprintf(NDMOS_API_STREND(buf), "(%lx,%d)",
            ma->ndmp3_addr_u.tcp_addr.ip_addr, ma->ndmp3_addr_u.tcp_addr.port);
  }
  return 0;
}


int ndmp3_pp_request(ndmp3_message msg, void* data, int lineno, char* buf)
{
  int i;
  unsigned int j;

  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP3_CONNECT_OPEN:
      NDMP_PP_WITH(ndmp3_connect_open_request)
      sprintf(buf, "version=%d", p->protocol_version);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_CONNECT_CLIENT_AUTH:
      NDMP_PP_WITH(ndmp3_connect_client_auth_request)
      sprintf(buf, "auth_type=%s",
              ndmp3_auth_type_to_str(p->auth_data.auth_type));
      sprintf(buf, "auth_type=%s",
              ndmp3_auth_type_to_str(p->auth_data.auth_type));
      switch (p->auth_data.auth_type) {
        case NDMP3_AUTH_NONE:
          break;

        case NDMP3_AUTH_TEXT:
          sprintf(NDMOS_API_STREND(buf), " auth_id=%s",
                  p->auth_data.ndmp3_auth_data_u.auth_text.auth_id);
          break;

        case NDMP3_AUTH_MD5:
          sprintf(NDMOS_API_STREND(buf), " auth_id=%s",
                  p->auth_data.ndmp3_auth_data_u.auth_md5.auth_id);
          break;

        default:
          sprintf(NDMOS_API_STREND(buf), " ????");
          break;
      }
      NDMP_PP_ENDWITH
      break;

    case NDMP3_CONNECT_CLOSE:
    case NDMP3_CONFIG_GET_HOST_INFO:
    case NDMP3_CONFIG_GET_CONNECTION_TYPE:
    case NDMP3_CONFIG_GET_SERVER_INFO:
    case NDMP3_CONFIG_GET_BUTYPE_INFO:
    case NDMP3_CONFIG_GET_FS_INFO:
    case NDMP3_CONFIG_GET_TAPE_INFO:
    case NDMP3_CONFIG_GET_SCSI_INFO:
    case NDMP3_SCSI_CLOSE:
    case NDMP3_SCSI_GET_STATE:
    case NDMP3_SCSI_RESET_DEVICE:
    case NDMP3_SCSI_RESET_BUS:
    case NDMP3_TAPE_GET_STATE:
    case NDMP3_TAPE_CLOSE:
    case NDMP3_MOVER_GET_STATE:
    case NDMP3_MOVER_CONTINUE:
    case NDMP3_MOVER_ABORT:
    case NDMP3_MOVER_STOP:
    case NDMP3_MOVER_CLOSE:
    case NDMP3_DATA_GET_STATE:
    case NDMP3_DATA_ABORT:
    case NDMP3_DATA_STOP:
    case NDMP3_DATA_GET_ENV:
      *buf = 0; /* no body */
      return 0;

    case NDMP3_CONNECT_SERVER_AUTH:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP3_CONFIG_GET_AUTH_ATTR:
      NDMP_PP_WITH(ndmp3_config_get_auth_attr_request)
      sprintf(buf, "auth_type=%s", ndmp3_auth_type_to_str(p->auth_type));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_SCSI_OPEN:
      NDMP_PP_WITH(ndmp3_scsi_open_request)
      sprintf(buf, "device='%s'", p->device);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_SCSI_SET_TARGET:
      NDMP_PP_WITH(ndmp3_scsi_set_target_request)
      sprintf(buf, "device='%s' cont=%d sid=%d lun=%d", p->device,
              p->target_controller, p->target_id, p->target_lun);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_SCSI_EXECUTE_CDB:
    case NDMP3_TAPE_EXECUTE_CDB:
      NDMP_PP_WITH(ndmp3_execute_cdb_request)
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

    case NDMP3_TAPE_OPEN:
      NDMP_PP_WITH(ndmp3_tape_open_request)
      sprintf(buf, "device='%s' mode=%s", p->device,
              ndmp3_tape_open_mode_to_str(p->mode));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_MTIO:
      NDMP_PP_WITH(ndmp3_tape_mtio_request)
      sprintf(buf, "op=%s count=%ld", ndmp3_tape_mtio_op_to_str(p->tape_op),
              p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_WRITE:
      NDMP_PP_WITH(ndmp3_tape_write_request)
      sprintf(buf, "data_out_len=%d", p->data_out.data_out_len);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_READ:
      NDMP_PP_WITH(ndmp3_tape_read_request)
      sprintf(buf, "count=%ld", p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_DATA_START_BACKUP:
      NDMP_PP_WITH(ndmp3_data_start_backup_request)
      if (lineno == 0) {
        sprintf(buf, "bu_type='%s' n_env=%d", p->bu_type, p->env.env_len);
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

    case NDMP3_DATA_START_RECOVER:
    case NDMP3_DATA_START_RECOVER_FILEHIST:
      NDMP_PP_WITH(ndmp3_data_start_recover_request)
      if (lineno == 0) {
        sprintf(buf, "bu_type='%s' n_env=%d n_nlist=%d", p->bu_type,
                p->env.env_len, p->nlist.nlist_len);
      } else {
        i = lineno - 1;
        if (0 <= i && (unsigned)i < p->env.env_len) {
          sprintf(buf, "env[%d] name='%s' value='%s'", i,
                  p->env.env_val[i].name, p->env.env_val[i].value);
        } else {
          i -= p->env.env_len;
          if (0 <= i && (unsigned)i < p->nlist.nlist_len * 4) {
            ndmp3_name* nm = &p->nlist.nlist_val[i / 4];

            switch (i % 4) {
              case 0:
                sprintf(buf, "nl[%d] original_path='%s'", i / 4,
                        nm->original_path);
                break;
              case 1:
                sprintf(buf, "..... destination_dir='%s'", nm->destination_dir);
                break;
              case 2:
                sprintf(buf, "..... new_name='%s' other='%s'", nm->new_name,
                        nm->other_name);
                break;
              case 3:
                sprintf(buf, "..... node=%lld fh_info=%lld", nm->node,
                        nm->fh_info);
                break;
            }
          } else {
            strcpy(buf, "--INVALID--");
          }
        }
      }
      return 1 + p->env.env_len + p->nlist.nlist_len * 4;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_DATA_LISTEN:
      NDMP_PP_WITH(ndmp3_data_listen_request)
      sprintf(buf, "addr_type=%s", ndmp3_addr_type_to_str(p->addr_type));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_DATA_CONNECT:
      NDMP_PP_WITH(ndmp3_data_connect_request)
      sprintf(buf, "addr=");
      ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->addr);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_NOTIFY_DATA_HALTED:
      NDMP_PP_WITH(ndmp3_notify_data_halted_request)
      sprintf(buf, "reason=%s text_reason='%s'",
              ndmp3_data_halt_reason_to_str(p->reason), p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_NOTIFY_CONNECTED:
      NDMP_PP_WITH(ndmp3_notify_connected_request)
      sprintf(buf, "reason=%s protocol_version=%d text_reason='%s'",
              ndmp3_connect_reason_to_str(p->reason), p->protocol_version,
              p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_NOTIFY_MOVER_HALTED:
      NDMP_PP_WITH(ndmp3_notify_mover_halted_request)
      sprintf(buf, "reason=%s text_reason='%s'",
              ndmp3_mover_halt_reason_to_str(p->reason), p->text_reason);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_NOTIFY_MOVER_PAUSED:
      NDMP_PP_WITH(ndmp3_notify_mover_paused_request)
      sprintf(buf, "reason=%s seek_position=%lld",
              ndmp3_mover_pause_reason_to_str(p->reason), p->seek_position);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_NOTIFY_DATA_READ:
      NDMP_PP_WITH(ndmp3_notify_data_read_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_LOG_FILE:
      NDMP_PP_WITH(ndmp3_log_file_request)
      sprintf(buf, "file=%s error=%s", p->name, ndmp3_error_to_str(p->error));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_LOG_MESSAGE:
      NDMP_PP_WITH(ndmp3_log_message_request)
      sprintf(buf, "log_type=%s id=%lu message='%s'",
              ndmp3_log_type_to_str(p->log_type), p->message_id, p->entry);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_FH_ADD_FILE:
      NDMP_PP_WITH(ndmp3_fh_add_file_request)
      int n_line = 0, n_names = 0, n_stats = 0;
      unsigned int n_normal = 0;

      n_line++;
      for (j = 0; j < p->files.files_len; j++) {
        int nn, ns;

        nn = p->files.files_val[j].names.names_len;
        ns = p->files.files_val[j].stats.stats_len;

        n_line += 1 + nn + ns;
        if (nn == 1 && ns == 1) n_normal++;
        n_names += nn;
        n_stats += ns;
      }

      if (n_normal == p->files.files_len) {
        /* could do something more efficient here */
      }

      if (lineno == 0) {
        sprintf(buf, "n_files=%d  total n_names=%d n_stats=%d",
                p->files.files_len, n_names, n_stats);
        return n_line;
      }
      lineno--;
      for (j = 0; j < p->files.files_len; j++) {
        ndmp3_file* file = &p->files.files_val[j];
        unsigned int k;

        if (lineno == 0) {
          sprintf(buf, "[%d] n_names=%d n_stats=%d node=%lld fhinfo=%lld", j,
                  file->names.names_len, file->stats.stats_len, file->node,
                  file->fh_info);
          return n_line;
        }

        lineno--;

        for (k = 0; k < file->names.names_len; k++, lineno--) {
          ndmp3_file_name* filename;

          if (lineno != 0) continue;

          filename = &file->names.names_val[k];

          sprintf(buf, "  name[%d] fs_type=%s", k,
                  ndmp3_fs_type_to_str(filename->fs_type));

          switch (filename->fs_type) {
            default:
              sprintf(NDMOS_API_STREND(buf), " other=%s",
                      filename->ndmp3_file_name_u.other_name);
              break;

            case NDMP3_FS_UNIX:
              sprintf(NDMOS_API_STREND(buf), " unix=%s",
                      filename->ndmp3_file_name_u.unix_name);
              break;

            case NDMP3_FS_NT:
              sprintf(NDMOS_API_STREND(buf), " nt=%s dos=%s",
                      filename->ndmp3_file_name_u.nt_name.nt_path,
                      filename->ndmp3_file_name_u.nt_name.dos_path);
              break;
          }
          return n_line;
        }

        for (k = 0; k < file->stats.stats_len; k++, lineno--) {
          ndmp3_file_stat* filestat;

          if (lineno != 0) continue;

          filestat = &file->stats.stats_val[k];

          sprintf(buf, "  stat[%ud] fs_type=%s ftype=%s size=%lld", k,
                  ndmp3_fs_type_to_str(filestat->fs_type),
                  ndmp3_file_type_to_str(filestat->ftype), filestat->size);

          return n_line;
        }
      }
      sprintf(buf, "  YIKES n_line=%d lineno=%d", n_line, lineno);
      return -1;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_FH_ADD_DIR:
      NDMP_PP_WITH(ndmp3_fh_add_dir_request)
      int n_line = 0, n_names = 0;
      unsigned int n_normal = 0;

      n_line++;
      for (j = 0; j < p->dirs.dirs_len; j++) {
        int nn;

        nn = p->dirs.dirs_val[j].names.names_len;

        n_line += 1 + nn;
        if (nn == 1) n_normal++;
        n_names += nn;
      }

      if (n_normal == p->dirs.dirs_len) {
        /* could do something more efficient here */
      }

      if (lineno == 0) {
        sprintf(buf, "n_dirs=%d  total n_names=%d", p->dirs.dirs_len, n_names);
        return n_line;
      }
      lineno--;
      for (j = 0; j < p->dirs.dirs_len; j++) {
        ndmp3_dir* dir = &p->dirs.dirs_val[j];
        unsigned int k;

        if (lineno == 0) {
          sprintf(buf, "[%ud] n_names=%d node=%lld parent=%lld", j,
                  dir->names.names_len, dir->node, dir->parent);
          return n_line;
        }

        lineno--;

        for (k = 0; k < dir->names.names_len; k++, lineno--) {
          ndmp3_file_name* filename;

          if (lineno != 0) continue;

          filename = &dir->names.names_val[k];

          sprintf(buf, "  name[%ud] fs_type=%s", k,
                  ndmp3_fs_type_to_str(filename->fs_type));

          switch (filename->fs_type) {
            default:
              sprintf(NDMOS_API_STREND(buf), " other=%s",
                      filename->ndmp3_file_name_u.other_name);
              break;

            case NDMP3_FS_UNIX:
              sprintf(NDMOS_API_STREND(buf), " unix=%s",
                      filename->ndmp3_file_name_u.unix_name);
              break;

            case NDMP3_FS_NT:
              sprintf(NDMOS_API_STREND(buf), " nt=%s dos=%s",
                      filename->ndmp3_file_name_u.nt_name.nt_path,
                      filename->ndmp3_file_name_u.nt_name.dos_path);
              break;
          }
          return n_line;
        }
      }
      sprintf(buf, "  YIKES n_line=%d lineno=%d", n_line, lineno);
      return -1;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_FH_ADD_NODE:
      NDMP_PP_WITH(ndmp3_fh_add_node_request)
      int n_line = 0, n_stats = 0;
      unsigned int n_normal = 0;

      n_line++;
      for (j = 0; j < p->nodes.nodes_len; j++) {
        int ns;

        ns = p->nodes.nodes_val[j].stats.stats_len;

        n_line += 1 + ns;
        if (ns == 1) n_normal++;
        n_stats += ns;
      }

      if (n_normal == p->nodes.nodes_len) {
        /* could do something more efficient here */
      }

      if (lineno == 0) {
        sprintf(buf, "n_nodes=%d  total n_stats=%d", p->nodes.nodes_len,
                n_stats);
        return n_line;
      }
      lineno--;
      for (j = 0; j < p->nodes.nodes_len; j++) {
        ndmp3_node* node = &p->nodes.nodes_val[j];
        unsigned int k;

        if (lineno == 0) {
          sprintf(buf, "[%ud] n_stats=%d node=%lld fhinfo=%lld", j,
                  node->stats.stats_len, node->node, node->fh_info);
          return n_line;
        }

        lineno--;

        for (k = 0; k < node->stats.stats_len; k++, lineno--) {
          ndmp3_file_stat* filestat;

          if (lineno != 0) continue;

          filestat = &node->stats.stats_val[k];

          sprintf(buf, "  stat[%ud] fs_type=%s ftype=%s size=%lld", k,
                  ndmp3_fs_type_to_str(filestat->fs_type),
                  ndmp3_file_type_to_str(filestat->ftype), filestat->size);

          return n_line;
        }
      }
      sprintf(buf, "  YIKES n_line=%d lineno=%d", n_line, lineno);
      return -1;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_LISTEN:
      NDMP_PP_WITH(ndmp3_mover_listen_request)
      sprintf(buf, "mode=%s addr_type=%s", ndmp3_mover_mode_to_str(p->mode),
              ndmp3_addr_type_to_str(p->addr_type));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_SET_WINDOW:
      NDMP_PP_WITH(ndmp3_mover_set_window_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_READ:
      NDMP_PP_WITH(ndmp3_mover_read_request)
      sprintf(buf, "offset=%lld length=%lld", p->offset, p->length);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_SET_RECORD_SIZE:
      NDMP_PP_WITH(ndmp3_mover_set_record_size_request)
      sprintf(buf, "len=%lu", p->len);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_CONNECT:
      NDMP_PP_WITH(ndmp3_mover_connect_request)
      sprintf(buf, "mode=%s addr=", ndmp3_mover_mode_to_str(p->mode));
      ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->addr);
      NDMP_PP_ENDWITH
      break;
  }
  return 1; /* one line in buf */
}


int ndmp3_pp_reply(ndmp3_message msg, void* data, int lineno, char* buf)
{
  int i;
  unsigned int j;

  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP3_CONNECT_OPEN:
    case NDMP3_CONNECT_CLIENT_AUTH:
    case NDMP3_SCSI_OPEN:
    case NDMP3_SCSI_CLOSE:
    case NDMP3_SCSI_SET_TARGET:
    case NDMP3_SCSI_RESET_DEVICE:
    case NDMP3_SCSI_RESET_BUS:
    case NDMP3_TAPE_OPEN:
    case NDMP3_TAPE_CLOSE:
    case NDMP3_MOVER_CONTINUE:
    case NDMP3_MOVER_ABORT:
    case NDMP3_MOVER_STOP:
    case NDMP3_MOVER_READ:
    case NDMP3_MOVER_SET_WINDOW:
    case NDMP3_MOVER_CLOSE:
    case NDMP3_MOVER_SET_RECORD_SIZE:
    case NDMP3_MOVER_CONNECT:
    case NDMP3_DATA_START_BACKUP:
    case NDMP3_DATA_START_RECOVER:
    case NDMP3_DATA_START_RECOVER_FILEHIST:
    case NDMP3_DATA_ABORT:
    case NDMP3_DATA_STOP:
    case NDMP3_DATA_CONNECT:
      NDMP_PP_WITH(ndmp3_error)
      sprintf(buf, "error=%s", ndmp3_error_to_str(*p));
      NDMP_PP_ENDWITH
      break;

    case NDMP3_CONFIG_GET_CONNECTION_TYPE:
      NDMP_PP_WITH(ndmp3_config_get_connection_type_reply)
      sprintf(buf, "error=%s addr_types[%d]={", ndmp3_error_to_str(p->error),
              p->addr_types.addr_types_len);
      for (j = 0; j < p->addr_types.addr_types_len; j++) {
        sprintf(NDMOS_API_STREND(buf), " %s",
                ndmp3_addr_type_to_str(p->addr_types.addr_types_val[j]));
      }
      strcat(buf, " }");
      NDMP_PP_ENDWITH
      break;

    case NDMP3_CONFIG_GET_SERVER_INFO:
    case NDMP3_CONFIG_GET_BUTYPE_INFO:
    case NDMP3_CONFIG_GET_FS_INFO:
    case NDMP3_CONFIG_GET_TAPE_INFO:
    case NDMP3_CONFIG_GET_SCSI_INFO:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP3_CONNECT_CLOSE:
      *buf = 0;
      return 0;

    case NDMP3_CONNECT_SERVER_AUTH:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP3_CONFIG_GET_HOST_INFO:
      NDMP_PP_WITH(ndmp3_config_get_host_info_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s hostname=%s", ndmp3_error_to_str(p->error),
                  p->hostname);
          break;
        case 1:
          sprintf(buf, "os_type=%s os_vers=%s hostid=%s", p->os_type,
                  p->os_vers, p->hostid);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 2;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_CONFIG_GET_AUTH_ATTR:
      strcpy(buf, "<<unimplemented pp>>");
      break;

    case NDMP3_SCSI_GET_STATE:
      NDMP_PP_WITH(ndmp3_scsi_get_state_reply)
      sprintf(buf, "error=%s cont=%d sid=%d lun=%d",
              ndmp3_error_to_str(p->error), p->target_controller, p->target_id,
              p->target_lun);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_SCSI_EXECUTE_CDB:
    case NDMP3_TAPE_EXECUTE_CDB:
      NDMP_PP_WITH(ndmp3_execute_cdb_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s status=%02x dataout_len=%ld datain_len=%d",
                  ndmp3_error_to_str(p->error), p->status, p->dataout_len,
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

    case NDMP3_TAPE_GET_STATE:
      NDMP_PP_WITH(ndmp3_tape_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "invalid=%lx error=%s flags=0x%lx file_num=%ld",
                  p->invalid, ndmp3_error_to_str(p->error), p->flags,
                  p->file_num);
          break;
        case 1:
          sprintf(buf, "soft_errors=%lu block_size=%lu blockno=%lu",
                  p->soft_errors, p->block_size, p->blockno);
          break;
        case 2:
          sprintf(buf, "total_space=%lld space_remain=%lld partition=%lu",
                  p->total_space, p->space_remain, p->partition);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 3;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_MTIO:
      NDMP_PP_WITH(ndmp3_tape_mtio_reply)
      sprintf(buf, "error=%s resid_count=%ld", ndmp3_error_to_str(p->error),
              p->resid_count);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_WRITE:
      NDMP_PP_WITH(ndmp3_tape_write_reply)
      sprintf(buf, "error=%s count=%ld", ndmp3_error_to_str(p->error),
              p->count);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_TAPE_READ:
      NDMP_PP_WITH(ndmp3_tape_read_reply)
      sprintf(buf, "error=%s data_in_len=%d", ndmp3_error_to_str(p->error),
              p->data_in.data_in_len);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_DATA_GET_STATE:
      NDMP_PP_WITH(ndmp3_data_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "invalid=%lx error=%s op=%s", p->invalid,
                  ndmp3_error_to_str(p->error),
                  ndmp3_data_operation_to_str(p->operation));
          break;
        case 1:
          sprintf(buf, "state=%s", ndmp3_data_state_to_str(p->state));
          break;
        case 2:
          sprintf(buf, "halt_reason=%s",
                  ndmp3_data_halt_reason_to_str(p->halt_reason));
          break;
        case 3:
          sprintf(buf, "bytes_processed=%lld est_bytes_remain=%lld",
                  p->bytes_processed, p->est_bytes_remain);
          break;
        case 4:
          sprintf(buf,
                  "est_time_remain=%ld data_conn_addr=", p->est_time_remain);
          ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->data_connection_addr);
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

    case NDMP3_DATA_GET_ENV:
      NDMP_PP_WITH(ndmp3_data_get_env_reply)
      if (lineno == 0) {
        sprintf(buf, "error=%s n_env=%d", ndmp3_error_to_str(p->error),
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

    case NDMP3_NOTIFY_DATA_HALTED:
    case NDMP3_NOTIFY_CONNECTED:
    case NDMP3_NOTIFY_MOVER_HALTED:
    case NDMP3_NOTIFY_MOVER_PAUSED:
    case NDMP3_NOTIFY_DATA_READ:
    case NDMP3_LOG_FILE:
    case NDMP3_LOG_MESSAGE:
    case NDMP3_FH_ADD_FILE:
    case NDMP3_FH_ADD_DIR:
    case NDMP3_FH_ADD_NODE:
      strcpy(buf, "<<ILLEGAL REPLY>>");
      break;

    case NDMP3_MOVER_GET_STATE:
      NDMP_PP_WITH(ndmp3_mover_get_state_reply)
      switch (lineno) {
        case 0:
          sprintf(buf, "error=%s state=%s", ndmp3_error_to_str(p->error),
                  ndmp3_mover_state_to_str(p->state));
          break;
        case 1:
          sprintf(buf, "pause_reason=%s",
                  ndmp3_mover_pause_reason_to_str(p->pause_reason));
          break;
        case 2:
          sprintf(buf, "halt_reason=%s",
                  ndmp3_mover_halt_reason_to_str(p->halt_reason));
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
        case 5:
          sprintf(buf, "data_conn_addr=");
          ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->data_connection_addr);
          break;
        default:
          strcpy(buf, "--INVALID--");
          break;
      }
      return 6;
      NDMP_PP_ENDWITH
      break;

    case NDMP3_DATA_LISTEN:
      NDMP_PP_WITH(ndmp3_data_listen_reply)
      sprintf(buf, "error=%s mover_conn_addr=", ndmp3_error_to_str(p->error));
      ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->data_connection_addr);
      NDMP_PP_ENDWITH
      break;

    case NDMP3_MOVER_LISTEN:
      NDMP_PP_WITH(ndmp3_mover_listen_reply)
      sprintf(buf, "error=%s data_conn_addr=", ndmp3_error_to_str(p->error));
      ndmp3_pp_addr(NDMOS_API_STREND(buf), &p->data_connection_addr);
      NDMP_PP_ENDWITH
      break;
  }

  return 1; /* one line in buf */
}

#endif /* !NDMOS_OPTION_NO_NDMP3 */
