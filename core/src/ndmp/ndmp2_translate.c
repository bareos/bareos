/*
 * Copyright (c) 2000
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
#include "ndmp_msg_buf.h"
#include "ndmp_translate.h"


#ifndef NDMOS_OPTION_NO_NDMP2


/*
 * Pervasive Types
 ****************************************************************
 */

// ndmp_error

struct enum_conversion ndmp_29_error[]
    = {{NDMP2_UNDEFINED_ERR, NDMP9_UNDEFINED_ERR}, /* default */
       {NDMP2_NO_ERR, NDMP9_NO_ERR},
       {NDMP2_NOT_SUPPORTED_ERR, NDMP9_NOT_SUPPORTED_ERR},
       {NDMP2_DEVICE_BUSY_ERR, NDMP9_DEVICE_BUSY_ERR},
       {NDMP2_DEVICE_OPENED_ERR, NDMP9_DEVICE_OPENED_ERR},
       {NDMP2_NOT_AUTHORIZED_ERR, NDMP9_NOT_AUTHORIZED_ERR},
       {NDMP2_PERMISSION_ERR, NDMP9_PERMISSION_ERR},
       {NDMP2_DEV_NOT_OPEN_ERR, NDMP9_DEV_NOT_OPEN_ERR},
       {NDMP2_IO_ERR, NDMP9_IO_ERR},
       {NDMP2_TIMEOUT_ERR, NDMP9_TIMEOUT_ERR},
       {NDMP2_ILLEGAL_ARGS_ERR, NDMP9_ILLEGAL_ARGS_ERR},
       {NDMP2_NO_TAPE_LOADED_ERR, NDMP9_NO_TAPE_LOADED_ERR},
       {NDMP2_WRITE_PROTECT_ERR, NDMP9_WRITE_PROTECT_ERR},
       {NDMP2_EOF_ERR, NDMP9_EOF_ERR},
       {NDMP2_EOM_ERR, NDMP9_EOM_ERR},
       {NDMP2_FILE_NOT_FOUND_ERR, NDMP9_FILE_NOT_FOUND_ERR},
       {NDMP2_BAD_FILE_ERR, NDMP9_BAD_FILE_ERR},
       {NDMP2_NO_DEVICE_ERR, NDMP9_NO_DEVICE_ERR},
       {NDMP2_NO_BUS_ERR, NDMP9_NO_BUS_ERR},
       {NDMP2_XDR_DECODE_ERR, NDMP9_XDR_DECODE_ERR},
       {NDMP2_ILLEGAL_STATE_ERR, NDMP9_ILLEGAL_STATE_ERR},
       {NDMP2_UNDEFINED_ERR, NDMP9_UNDEFINED_ERR},
       {NDMP2_XDR_ENCODE_ERR, NDMP9_XDR_ENCODE_ERR},
       {NDMP2_NO_MEM_ERR, NDMP9_NO_MEM_ERR},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_error(ndmp2_error* error2, ndmp9_error* error9)
{
  *error9 = convert_enum_to_9(ndmp_29_error, *error2);
  return 0;
}

int ndmp_9to2_error(ndmp9_error* error9, ndmp2_error* error2)
{
  *error2 = convert_enum_from_9(ndmp_29_error, *error9);
  return 0;
}


/*
 * ndmp_pval
 ****************************************************************
 */

int ndmp_2to9_pval(ndmp2_pval* pval2, ndmp9_pval* pval9)
{
  CNVT_STRDUP_TO_9(pval2, pval9, name);
  CNVT_STRDUP_TO_9(pval2, pval9, value);

  return 0;
}

int ndmp_9to2_pval(ndmp9_pval* pval9, ndmp2_pval* pval2)
{
  CNVT_STRDUP_FROM_9(pval2, pval9, name);
  CNVT_STRDUP_FROM_9(pval2, pval9, value);

  return 0;
}

int ndmp_2to9_pval_vec(ndmp2_pval* pval2, ndmp9_pval* pval9, unsigned n_pval)
{
  unsigned int i;

  for (i = 0; i < n_pval; i++) ndmp_2to9_pval(&pval2[i], &pval9[i]);

  return 0;
}

int ndmp_9to2_pval_vec(ndmp9_pval* pval9, ndmp2_pval* pval2, unsigned n_pval)
{
  unsigned int i;

  for (i = 0; i < n_pval; i++) ndmp_9to2_pval(&pval9[i], &pval2[i]);

  return 0;
}

int ndmp_2to9_pval_vec_dup(ndmp2_pval* pval2,
                           ndmp9_pval** pval9_p,
                           unsigned n_pval)
{
  *pval9_p = NDMOS_MACRO_NEWN(ndmp9_pval, n_pval);
  if (!*pval9_p) return -1;

  return ndmp_2to9_pval_vec(pval2, *pval9_p, n_pval);
}

int ndmp_9to2_pval_vec_dup(ndmp9_pval* pval9,
                           ndmp2_pval** pval2_p,
                           unsigned n_pval)
{
  *pval2_p = NDMOS_MACRO_NEWN(ndmp2_pval, n_pval);
  if (!*pval2_p) return -1;

  return ndmp_9to2_pval_vec(pval9, *pval2_p, n_pval);
}


// ndmp[_mover]_addr

struct enum_conversion ndmp_29_mover_addr_type[]
    = {{NDMP_INVALID_GENERAL, NDMP_INVALID_GENERAL}, /* default */
       {NDMP2_ADDR_LOCAL, NDMP9_ADDR_LOCAL},
       {NDMP2_ADDR_TCP, NDMP9_ADDR_TCP},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_mover_addr(ndmp2_mover_addr* addr2, ndmp9_addr* addr9)
{
  switch (addr2->addr_type) {
    case NDMP2_ADDR_LOCAL:
      addr9->addr_type = NDMP9_ADDR_LOCAL;
      break;

    case NDMP2_ADDR_TCP:
      addr9->addr_type = NDMP9_ADDR_TCP;
      addr9->ndmp9_addr_u.tcp_addr.ip_addr
          = addr2->ndmp2_mover_addr_u.addr.ip_addr;
      addr9->ndmp9_addr_u.tcp_addr.port = addr2->ndmp2_mover_addr_u.addr.port;
      break;

    default:
      NDMOS_MACRO_ZEROFILL(addr9);
      addr9->addr_type = -1;
      return -1;
  }

  return 0;
}

int ndmp_9to2_mover_addr(ndmp9_addr* addr9, ndmp2_mover_addr* addr2)
{
  switch (addr9->addr_type) {
    case NDMP9_ADDR_LOCAL:
      addr2->addr_type = NDMP2_ADDR_LOCAL;
      break;

    case NDMP9_ADDR_TCP:
      addr2->addr_type = NDMP2_ADDR_TCP;
      addr2->ndmp2_mover_addr_u.addr.ip_addr
          = addr9->ndmp9_addr_u.tcp_addr.ip_addr;
      addr2->ndmp2_mover_addr_u.addr.port = addr9->ndmp9_addr_u.tcp_addr.port;
      break;

    default:
      NDMOS_MACRO_ZEROFILL(addr2);
      addr2->addr_type = -1;
      return -1;
  }

  return 0;
}


/*
 * CONNECT INTERFACES
 ****************************************************************
 */

struct enum_conversion ndmp_29_auth_type[]
    = {{NDMP_INVALID_GENERAL, NDMP_INVALID_GENERAL}, /* default */
       {NDMP2_AUTH_NONE, NDMP9_AUTH_NONE},
       {NDMP2_AUTH_TEXT, NDMP9_AUTH_TEXT},
       {NDMP2_AUTH_MD5, NDMP9_AUTH_MD5},
       END_ENUM_CONVERSION_TABLE};

int ndmp_2to9_auth_data(ndmp2_auth_data* auth_data2,
                        ndmp9_auth_data* auth_data9)
{
  int n_error = 0;
  int rc;
  ndmp2_auth_text* text2;
  ndmp9_auth_text* text9;
  ndmp2_auth_md5* md52;
  ndmp9_auth_md5* md59;

  switch (auth_data2->auth_type) {
    case NDMP2_AUTH_NONE:
      auth_data9->auth_type = NDMP9_AUTH_NONE;
      break;

    case NDMP2_AUTH_TEXT:
      auth_data9->auth_type = NDMP9_AUTH_TEXT;
      text2 = &auth_data2->ndmp2_auth_data_u.auth_text;
      text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
      rc = CNVT_STRDUP_TO_9(text2, text9, auth_id);
      if (rc) { return rc; /* no mem */ }
      rc = CNVT_STRDUP_TO_9(text2, text9, auth_password);
      if (rc) {
        NDMOS_API_FREE(text9->auth_id);
        text9->auth_id = 0;
        return rc; /* no mem */
      }
      break;

    case NDMP2_AUTH_MD5:
      auth_data9->auth_type = NDMP9_AUTH_MD5;
      md52 = &auth_data2->ndmp2_auth_data_u.auth_md5;
      md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
      rc = CNVT_STRDUP_TO_9(md52, md59, auth_id);
      if (rc) { return rc; /* no mem */ }
      NDMOS_API_BCOPY(md52->auth_digest, md59->auth_digest, 16);
      break;

    default:
      auth_data9->auth_type = auth_data2->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_data9->ndmp9_auth_data_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_9to2_auth_data(ndmp9_auth_data* auth_data9,
                        ndmp2_auth_data* auth_data2)
{
  int n_error = 0;
  int rc;
  ndmp9_auth_text* text9;
  ndmp2_auth_text* text2;
  ndmp9_auth_md5* md59;
  ndmp2_auth_md5* md52;

  switch (auth_data9->auth_type) {
    case NDMP9_AUTH_NONE:
      auth_data2->auth_type = NDMP2_AUTH_NONE;
      break;

    case NDMP9_AUTH_TEXT:
      auth_data2->auth_type = NDMP2_AUTH_TEXT;
      text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
      text2 = &auth_data2->ndmp2_auth_data_u.auth_text;
      rc = CNVT_STRDUP_FROM_9(text2, text9, auth_id);
      if (rc) { return rc; /* no mem */ }
      rc = CNVT_STRDUP_FROM_9(text2, text9, auth_password);
      if (rc) {
        NDMOS_API_FREE(text9->auth_id);
        text2->auth_id = 0;
        return rc; /* no mem */
      }
      break;

    case NDMP9_AUTH_MD5:
      auth_data2->auth_type = NDMP2_AUTH_MD5;
      md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
      md52 = &auth_data2->ndmp2_auth_data_u.auth_md5;
      rc = CNVT_STRDUP_FROM_9(md52, md59, auth_id);
      if (rc) { return rc; /* no mem */ }
      NDMOS_API_BCOPY(md59->auth_digest, md52->auth_digest, 16);
      break;

    default:
      auth_data2->auth_type = auth_data9->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_data2->ndmp2_auth_data_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_2to9_auth_attr(ndmp2_auth_attr* auth_attr2,
                        ndmp9_auth_attr* auth_attr9)
{
  int n_error = 0;

  switch (auth_attr2->auth_type) {
    case NDMP2_AUTH_NONE:
      auth_attr9->auth_type = NDMP9_AUTH_NONE;
      break;

    case NDMP2_AUTH_TEXT:
      auth_attr9->auth_type = NDMP9_AUTH_TEXT;
      break;

    case NDMP2_AUTH_MD5:
      auth_attr9->auth_type = NDMP9_AUTH_MD5;
      NDMOS_API_BCOPY(auth_attr2->ndmp2_auth_attr_u.challenge,
                      auth_attr9->ndmp9_auth_attr_u.challenge, 64);
      break;

    default:
      auth_attr9->auth_type = auth_attr2->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_attr9->ndmp9_auth_attr_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_9to2_auth_attr(ndmp9_auth_attr* auth_attr9,
                        ndmp2_auth_attr* auth_attr2)
{
  int n_error = 0;

  switch (auth_attr9->auth_type) {
    case NDMP9_AUTH_NONE:
      auth_attr2->auth_type = NDMP2_AUTH_NONE;
      break;

    case NDMP9_AUTH_TEXT:
      auth_attr2->auth_type = NDMP2_AUTH_TEXT;
      break;

    case NDMP9_AUTH_MD5:
      auth_attr2->auth_type = NDMP2_AUTH_MD5;
      NDMOS_API_BCOPY(auth_attr9->ndmp9_auth_attr_u.challenge,
                      auth_attr2->ndmp2_auth_attr_u.challenge, 64);
      break;

    default:
      auth_attr2->auth_type = auth_attr9->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_attr2->ndmp2_auth_attr_u);
      n_error++;
      break;
  }

  return n_error;
}


/*
 * ndmp_connect_open
 * just error reply
 */

int ndmp_2to9_connect_open_request(ndmp2_connect_open_request* request2,
                                   ndmp9_connect_open_request* request9)
{
  CNVT_TO_9(request2, request9, protocol_version);
  return 0;
}

int ndmp_9to2_connect_open_request(ndmp9_connect_open_request* request9,
                                   ndmp2_connect_open_request* request2)
{
  CNVT_FROM_9(request2, request9, protocol_version);
  return 0;
}


/*
 * ndmp_connect_client_auth
 * just error reply
 */


int ndmp_2to9_connect_client_auth_request(
    ndmp2_connect_client_auth_request* request2,
    ndmp9_connect_client_auth_request* request9)
{
  int rc;

  rc = ndmp_2to9_auth_data(&request2->auth_data, &request9->auth_data);

  return rc;
}

int ndmp_9to2_connect_client_auth_request(
    ndmp9_connect_client_auth_request* request9,
    ndmp2_connect_client_auth_request* request2)
{
  int rc;

  rc = ndmp_9to2_auth_data(&request9->auth_data, &request2->auth_data);

  return rc;
}


/*
 * ndmp_connect_close
 * no arg request, **NO REPLY**
 */

// ndmp_connect_server_auth

/* TBD */
int ndmp_2to9_connect_server_auth_request(
    [[maybe_unused]] ndmp2_connect_server_auth_request* request2,
    [[maybe_unused]] ndmp9_connect_server_auth_request* request9)
{
  return -1;
}

int ndmp_9to2_connect_server_auth_request(
    [[maybe_unused]] ndmp9_connect_server_auth_request* request9,
    [[maybe_unused]] ndmp2_connect_server_auth_request* request2)
{
  return -1;
}


/*
 * CONFIG INTERFACES
 ****************************************************************
 */

/*
 * ndmp_config_get_host_info
 * no args request
 */

int ndmp_2to9_config_get_host_info_reply(
    ndmp2_config_get_host_info_reply* reply2,
    ndmp9_config_get_host_info_reply* reply9)
{
  unsigned int i, n_error = 0;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_STRDUP_TO_9x(reply2, reply9, hostname, config_info.hostname);
  CNVT_STRDUP_TO_9x(reply2, reply9, os_type, config_info.os_type);
  CNVT_STRDUP_TO_9x(reply2, reply9, os_vers, config_info.os_vers);
  CNVT_STRDUP_TO_9x(reply2, reply9, hostid, config_info.hostid);

  reply9->config_info.authtypes = 0;
  for (i = 0; i < reply2->auth_type.auth_type_len; i++) {
    switch (reply2->auth_type.auth_type_val[i]) {
      case NDMP2_AUTH_NONE:
        reply9->config_info.authtypes |= NDMP9_CONFIG_AUTHTYPE_NONE;
        break;

      case NDMP2_AUTH_TEXT:
        reply9->config_info.authtypes |= NDMP9_CONFIG_AUTHTYPE_TEXT;
        break;

      case NDMP2_AUTH_MD5:
        reply9->config_info.authtypes |= NDMP9_CONFIG_AUTHTYPE_MD5;
        break;

      default:
        n_error++;
        /* ignore */
        break;
    }
  }

  return n_error;
}

int ndmp_9to2_config_get_host_info_reply(
    ndmp9_config_get_host_info_reply* reply9,
    ndmp2_config_get_host_info_reply* reply2)
{
  int i = 0;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_STRDUP_FROM_9x(reply2, reply9, hostname, config_info.hostname);
  CNVT_STRDUP_FROM_9x(reply2, reply9, os_type, config_info.os_type);
  CNVT_STRDUP_FROM_9x(reply2, reply9, os_vers, config_info.os_vers);
  CNVT_STRDUP_FROM_9x(reply2, reply9, hostid, config_info.hostid);

  reply2->auth_type.auth_type_val = NDMOS_MACRO_NEWN(ndmp2_auth_type, 3);
  if (!reply2->auth_type.auth_type_val) { return -1; }

  i = 0;
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_NONE) {
    reply2->auth_type.auth_type_val[i++] = NDMP2_AUTH_NONE;
  }
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_TEXT) {
    reply2->auth_type.auth_type_val[i++] = NDMP2_AUTH_TEXT;
  }
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_MD5) {
    reply2->auth_type.auth_type_val[i++] = NDMP2_AUTH_MD5;
  }
  reply2->auth_type.auth_type_len = i;

  return 0;
}


/*
 * ndmp_config_get_butype_attr
 * ndmp2_config_get_butype_attr handled
 * as version specific dispatch function
 * in ndma_comm_dispatch.c
 */


/*
 * ndmp_config_get_mover_type
 * no args request
 */

int ndmp_2to9_config_get_mover_type_reply(
    ndmp2_config_get_mover_type_reply* reply2,
    ndmp9_config_get_connection_type_reply* reply9)
{
  int n_error = 0;
  unsigned int i;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  for (i = 0; i < reply2->methods.methods_len; i++) {
    switch (reply2->methods.methods_val[i]) {
      case NDMP2_ADDR_LOCAL:
        reply9->config_info.conntypes |= NDMP9_CONFIG_CONNTYPE_LOCAL;
        break;

      case NDMP2_ADDR_TCP:
        reply9->config_info.conntypes |= NDMP9_CONFIG_CONNTYPE_TCP;
        break;

      default:
        n_error++;
        /* ignore */
        break;
    }
  }

  return n_error;
}

int ndmp_9to2_config_get_mover_type_reply(
    ndmp9_config_get_connection_type_reply* reply9,
    ndmp2_config_get_mover_type_reply* reply2)
{
  int i = 0;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);

  reply2->methods.methods_val = NDMOS_MACRO_NEWN(ndmp2_mover_addr_type, 3);
  if (!reply2->methods.methods_val) { return -1; /* no mem */ }

  i = 0;
  if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_LOCAL) {
    reply2->methods.methods_val[i++] = NDMP2_ADDR_LOCAL;
  }
  if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_TCP) {
    reply2->methods.methods_val[i++] = NDMP2_ADDR_TCP;
  }
  reply2->methods.methods_len = i;

  return 0;
}


// ndmp_config_get_auth_attr

int ndmp_2to9_config_get_auth_attr_request(
    struct ndmp2_config_get_auth_attr_request* request2,
    struct ndmp9_config_get_auth_attr_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, auth_type, ndmp_29_auth_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request2, request9, auth_type);
    n_error++;
  }

  return n_error;
}

int ndmp_9to2_config_get_auth_attr_request(
    struct ndmp9_config_get_auth_attr_request* request9,
    struct ndmp2_config_get_auth_attr_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, auth_type, ndmp_29_auth_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, auth_type);
    n_error++;
  }

  return n_error;
}

int ndmp_2to9_config_get_auth_attr_reply(
    struct ndmp2_config_get_auth_attr_reply* reply2,
    struct ndmp9_config_get_auth_attr_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);

  n_error += ndmp_2to9_auth_attr(&reply2->server_attr, &reply9->server_attr);

  return n_error;
}

int ndmp_9to2_config_get_auth_attr_reply(
    struct ndmp9_config_get_auth_attr_reply* reply9,
    struct ndmp2_config_get_auth_attr_reply* reply2)
{
  int n_error = 0;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);

  n_error += ndmp_9to2_auth_attr(&reply9->server_attr, &reply2->server_attr);

  return n_error;
}


/*
 * SCSI INTERFACES
 ****************************************************************
 */

/*
 * ndmp_scsi_open
 * just error reply
 */
int ndmp_2to9_scsi_open_request(ndmp2_scsi_open_request* request2,
                                ndmp9_scsi_open_request* request9)
{
  request9->device = NDMOS_API_STRDUP(request2->device.name);
  if (!request9->device) { return -1; /* no memory */ }
  return 0;
}

int ndmp_9to2_scsi_open_request(ndmp9_scsi_open_request* request9,
                                ndmp2_scsi_open_request* request2)
{
  request2->device.name = NDMOS_API_STRDUP(request9->device);
  if (!request2->device.name) { return -1; /* no memory */ }
  return 0;
}

/*
 * ndmp_scsi_close
 * no args request, just error reply
 */

/*
 * ndmp_scsi_get_state
 * no args request
 */

int ndmp_2to9_scsi_get_state_reply(ndmp2_scsi_get_state_reply* reply2,
                                   ndmp9_scsi_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_TO_9(reply2, reply9, target_controller);
  CNVT_TO_9(reply2, reply9, target_id);
  CNVT_TO_9(reply2, reply9, target_lun);

  return 0;
}

int ndmp_9to2_scsi_get_state_reply(ndmp9_scsi_get_state_reply* reply9,
                                   ndmp2_scsi_get_state_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_FROM_9(reply2, reply9, target_controller);
  CNVT_FROM_9(reply2, reply9, target_id);
  CNVT_FROM_9(reply2, reply9, target_lun);

  return 0;
}

/*
 * ndmp_scsi_set_target -- deprecated
 * just error reply
 */

int ndmp_2to9_scsi_set_target_request(ndmp2_scsi_set_target_request* request2,
                                      ndmp9_scsi_set_target_request* request9)
{
  request9->device = NDMOS_API_STRDUP(request2->device.name);
  if (!request9->device) { return -1; /* no memory */ }

  CNVT_TO_9(request2, request9, target_controller);
  CNVT_TO_9(request2, request9, target_id);
  CNVT_TO_9(request2, request9, target_lun);

  return 0;
}

int ndmp_9to2_scsi_set_target_request(ndmp9_scsi_set_target_request* request9,
                                      ndmp2_scsi_set_target_request* request2)
{
  request2->device.name = NDMOS_API_STRDUP(request9->device);
  if (!request2->device.name) { return -1; /* no memory */ }

  CNVT_FROM_9(request2, request9, target_controller);
  CNVT_FROM_9(request2, request9, target_id);
  CNVT_FROM_9(request2, request9, target_lun);

  return 0;
}

/*
 * ndmp_scsi_reset_device
 * no args request, just error reply
 */

/*
 * ndmp_scsi_reset_bus -- deprecated
 * no args request, just error reply
 */


/*
 * ndmp_tape_execute_cdb
 * ndmp_scsi_execute_cdb
 */

int ndmp_2to9_execute_cdb_request(ndmp2_execute_cdb_request* request2,
                                  ndmp9_execute_cdb_request* request9)
{
  int n_error = 0;
  uint32_t len;
  char* p;

  switch (request2->flags) {
    case 0:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_NONE;
      break;

    case NDMP2_SCSI_DATA_IN:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
      break;

    case NDMP2_SCSI_DATA_OUT:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
      break;

    default:
      /* deemed insolvable */
      n_error++;
      return -1;
  }

  CNVT_TO_9(request2, request9, timeout);
  CNVT_TO_9(request2, request9, datain_len);

  len = request2->dataout.dataout_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(request2->dataout.dataout_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request9->dataout.dataout_len = len;
  request9->dataout.dataout_val = p;

  len = request2->cdb.cdb_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (request9->dataout.dataout_val) {
        NDMOS_API_FREE(request9->dataout.dataout_val);
        request9->dataout.dataout_len = 0;
        request9->dataout.dataout_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(request2->cdb.cdb_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request9->cdb.cdb_len = len;
  request9->cdb.cdb_val = p;

  return 0;
}

int ndmp_9to2_execute_cdb_request(ndmp9_execute_cdb_request* request9,
                                  ndmp2_execute_cdb_request* request2)
{
  int n_error = 0;
  uint32_t len;
  char* p;

  switch (request9->data_dir) {
    case NDMP9_SCSI_DATA_DIR_NONE:
      request2->flags = 0;
      break;

    case NDMP9_SCSI_DATA_DIR_IN:
      request2->flags = NDMP2_SCSI_DATA_IN;
      break;

    case NDMP9_SCSI_DATA_DIR_OUT:
      request2->flags = NDMP2_SCSI_DATA_OUT;
      break;

    default:
      /* deemed insolvable */
      n_error++;
      return -1;
  }

  CNVT_FROM_9(request2, request9, timeout);
  CNVT_FROM_9(request2, request9, datain_len);

  len = request9->dataout.dataout_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(request9->dataout.dataout_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request2->dataout.dataout_len = len;
  request2->dataout.dataout_val = p;

  len = request9->cdb.cdb_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (request2->dataout.dataout_val) {
        NDMOS_API_FREE(request2->dataout.dataout_val);
        request2->dataout.dataout_len = 0;
        request2->dataout.dataout_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(request9->cdb.cdb_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request2->cdb.cdb_len = len;
  request2->cdb.cdb_val = p;

  return 0;
}

int ndmp_2to9_execute_cdb_reply(ndmp2_execute_cdb_reply* reply2,
                                ndmp9_execute_cdb_reply* reply9)
{
  uint32_t len;
  char* p;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_TO_9(reply2, reply9, status);
  CNVT_TO_9(reply2, reply9, dataout_len);

  len = reply2->datain.datain_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply2->datain.datain_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply9->datain.datain_len = len;
  reply9->datain.datain_val = p;

  len = reply2->ext_sense.ext_sense_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (reply9->datain.datain_val) {
        NDMOS_API_FREE(reply9->datain.datain_val);
        reply9->datain.datain_len = 0;
        reply9->datain.datain_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(reply2->ext_sense.ext_sense_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply9->ext_sense.ext_sense_len = len;
  reply9->ext_sense.ext_sense_val = p;

  return 0;
}

int ndmp_9to2_execute_cdb_reply(ndmp9_execute_cdb_reply* reply9,
                                ndmp2_execute_cdb_reply* reply2)
{
  uint32_t len;
  char* p;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_FROM_9(reply2, reply9, status);
  CNVT_FROM_9(reply2, reply9, dataout_len);

  len = reply9->datain.datain_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply9->datain.datain_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply2->datain.datain_len = len;
  reply2->datain.datain_val = p;

  len = reply9->ext_sense.ext_sense_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (reply2->datain.datain_val) {
        NDMOS_API_FREE(reply9->datain.datain_val);
        reply2->datain.datain_len = 0;
        reply2->datain.datain_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(reply9->ext_sense.ext_sense_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply2->ext_sense.ext_sense_len = len;
  reply2->ext_sense.ext_sense_val = p;

  return 0;
}


/*
 * TAPE INTERFACES
 ****************************************************************
 */


/*
 * ndmp_tape_open_request
 * just error reply
 */

struct enum_conversion ndmp_29_tape_open_mode[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_TAPE_READ_MODE, NDMP9_TAPE_READ_MODE},
       {NDMP2_TAPE_WRITE_MODE, NDMP9_TAPE_RDWR_MODE},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_tape_open_request(ndmp2_tape_open_request* request2,
                                ndmp9_tape_open_request* request9)
{
  int n_error = 0;
  int rc;

  /*
   * Mode codes are compatible between versions.
   * We let untranslated values through to
   * facilitate testing illegal values.
   */
  rc = convert_enum_to_9(ndmp_29_tape_open_mode, request2->mode);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    request9->mode = request2->mode;
  } else {
    request9->mode = rc;
  }

  request9->device = NDMOS_API_STRDUP(request2->device.name);
  if (!request9->device) { return -1; /* no memory */ }

  return n_error;
}

int ndmp_9to2_tape_open_request(ndmp9_tape_open_request* request9,
                                ndmp2_tape_open_request* request2)
{
  int n_error = 0;
  int rc;

  rc = convert_enum_from_9(ndmp_29_tape_open_mode, request9->mode);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    request2->mode = request9->mode;
  } else {
    request2->mode = rc;
  }

  request2->device.name = NDMOS_API_STRDUP(request9->device);
  if (!request2->device.name) { return -1; /* no memory */ }

  return n_error;
}


/*
 * ndmp_tape_close
 * no arg request, just error reply
 */


/*
 * ndmp_tape_get_state_reply
 * no arg request
 */

int ndmp_2to9_tape_get_state_reply(ndmp2_tape_get_state_reply* reply2,
                                   ndmp9_tape_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_TO_9(reply2, reply9, flags);
  CNVT_VUL_TO_9(reply2, reply9, file_num);
  CNVT_VUL_TO_9(reply2, reply9, soft_errors);
  CNVT_VUL_TO_9(reply2, reply9, block_size);
  CNVT_VUL_TO_9(reply2, reply9, blockno);
  CNVT_VUQ_TO_9(reply2, reply9, total_space);
  CNVT_VUQ_TO_9(reply2, reply9, space_remain);

  return 0;
}

int ndmp_9to2_tape_get_state_reply(ndmp9_tape_get_state_reply* reply9,
                                   ndmp2_tape_get_state_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_FROM_9(reply2, reply9, flags);
  CNVT_VUL_FROM_9(reply2, reply9, file_num);
  CNVT_VUL_FROM_9(reply2, reply9, soft_errors);
  CNVT_VUL_FROM_9(reply2, reply9, block_size);
  CNVT_VUL_FROM_9(reply2, reply9, blockno);
  CNVT_VUQ_FROM_9(reply2, reply9, total_space);
  CNVT_VUQ_FROM_9(reply2, reply9, space_remain);

  return 0;
}


// ndmp_tape_mtio_request

struct enum_conversion ndmp_29_tape_mtio_op[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_MTIO_FSF, NDMP9_MTIO_FSF},
       {NDMP2_MTIO_BSF, NDMP9_MTIO_BSF},
       {NDMP2_MTIO_FSR, NDMP9_MTIO_FSR},
       {NDMP2_MTIO_BSR, NDMP9_MTIO_BSR},
       {NDMP2_MTIO_REW, NDMP9_MTIO_REW},
       {NDMP2_MTIO_EOF, NDMP9_MTIO_EOF},
       {NDMP2_MTIO_OFF, NDMP9_MTIO_OFF},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_tape_mtio_request(ndmp2_tape_mtio_request* request2,
                                ndmp9_tape_mtio_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, tape_op, ndmp_29_tape_mtio_op);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    CNVT_TO_9(request2, request9, tape_op);
  }

  CNVT_TO_9(request2, request9, count);

  return n_error;
}

int ndmp_9to2_tape_mtio_request(ndmp9_tape_mtio_request* request9,
                                ndmp2_tape_mtio_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, tape_op, ndmp_29_tape_mtio_op);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    CNVT_FROM_9(request2, request9, tape_op);
  }

  CNVT_FROM_9(request2, request9, count);

  return n_error;
}

int ndmp_2to9_tape_mtio_reply(ndmp2_tape_mtio_reply* reply2,
                              ndmp9_tape_mtio_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_TO_9(reply2, reply9, resid_count);
  return 0;
}

int ndmp_9to2_tape_mtio_reply(ndmp9_tape_mtio_reply* reply9,
                              ndmp2_tape_mtio_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_FROM_9(reply2, reply9, resid_count);
  return 0;
}


// ndmp_tape_write

int ndmp_2to9_tape_write_request(ndmp2_tape_write_request* request2,
                                 ndmp9_tape_write_request* request9)
{
  uint32_t len;
  char* p;

  len = request2->data_out.data_out_len;

  p = NDMOS_API_MALLOC(len);
  if (!p) { return -1; }

  NDMOS_API_BCOPY(request2->data_out.data_out_val, p, len);

  request9->data_out.data_out_val = p;
  request9->data_out.data_out_len = len;

  return 0;
}

int ndmp_9to2_tape_write_request(ndmp9_tape_write_request* request9,
                                 ndmp2_tape_write_request* request2)
{
  uint32_t len;
  char* p;

  len = request9->data_out.data_out_len;

  p = NDMOS_API_MALLOC(len);
  if (!p) { return -1; }

  NDMOS_API_BCOPY(request9->data_out.data_out_val, p, len);

  request2->data_out.data_out_val = p;
  request2->data_out.data_out_len = len;

  return 0;
}

int ndmp_2to9_tape_write_reply(ndmp2_tape_write_reply* reply2,
                               ndmp9_tape_write_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_TO_9(reply2, reply9, count);
  return 0;
}

int ndmp_9to2_tape_write_reply(ndmp9_tape_write_reply* reply9,
                               ndmp2_tape_write_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_FROM_9(reply2, reply9, count);
  return 0;
}


// ndmp_tape_read

int ndmp_2to9_tape_read_request(ndmp2_tape_read_request* request2,
                                ndmp9_tape_read_request* request9)
{
  CNVT_TO_9(request2, request9, count);
  return 0;
}

int ndmp_9to2_tape_read_request(ndmp9_tape_read_request* request9,
                                ndmp2_tape_read_request* request2)
{
  CNVT_FROM_9(request2, request9, count);
  return 0;
}

int ndmp_2to9_tape_read_reply(ndmp2_tape_read_reply* reply2,
                              ndmp9_tape_read_reply* reply9)
{
  uint32_t len;
  char* p;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);

  len = reply2->data_in.data_in_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply2->data_in.data_in_val, p, len);
  } else {
    p = 0;
    len = 0;
  }

  reply9->data_in.data_in_len = len;
  reply9->data_in.data_in_val = p;

  return 0;
}

int ndmp_9to2_tape_read_reply(ndmp9_tape_read_reply* reply9,
                              ndmp2_tape_read_reply* reply2)
{
  uint32_t len;
  char* p;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);

  len = reply9->data_in.data_in_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply9->data_in.data_in_val, p, len);
  } else {
    p = 0;
    len = 0;
  }

  reply2->data_in.data_in_len = len;
  reply2->data_in.data_in_val = p;

  return 0;
}

/*
 * ndmp_tape_execute_cdb
 * see SCSI INTERFACES above
 */


/*
 * MOVER INTERFACES
 ****************************************************************
 */

/*
 * ndmp_mover_get_state
 * no args request
 */

struct enum_conversion ndmp_29_mover_mode[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_MOVER_MODE_READ, NDMP9_MOVER_MODE_READ},
       {NDMP2_MOVER_MODE_WRITE, NDMP9_MOVER_MODE_WRITE},

       END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_29_mover_state[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_MOVER_STATE_IDLE, NDMP9_MOVER_STATE_IDLE},
       {NDMP2_MOVER_STATE_LISTEN, NDMP9_MOVER_STATE_LISTEN},
       {NDMP2_MOVER_STATE_ACTIVE, NDMP9_MOVER_STATE_ACTIVE},
       {NDMP2_MOVER_STATE_PAUSED, NDMP9_MOVER_STATE_PAUSED},
       {NDMP2_MOVER_STATE_HALTED, NDMP9_MOVER_STATE_HALTED},

       /* alias */
       {NDMP2_MOVER_STATE_ACTIVE, NDMP9_MOVER_STATE_STANDBY},

       END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_29_mover_pause_reason[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_MOVER_PAUSE_NA, NDMP9_MOVER_PAUSE_NA},
       {NDMP2_MOVER_PAUSE_EOM, NDMP9_MOVER_PAUSE_EOM},
       {NDMP2_MOVER_PAUSE_EOF, NDMP9_MOVER_PAUSE_EOF},
       {NDMP2_MOVER_PAUSE_SEEK, NDMP9_MOVER_PAUSE_SEEK},
       {NDMP2_MOVER_PAUSE_MEDIA_ERROR, NDMP9_MOVER_PAUSE_MEDIA_ERROR},
       END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_29_mover_halt_reason[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_MOVER_HALT_NA, NDMP9_MOVER_HALT_NA},
       {NDMP2_MOVER_HALT_CONNECT_CLOSED, NDMP9_MOVER_HALT_CONNECT_CLOSED},
       {NDMP2_MOVER_HALT_ABORTED, NDMP9_MOVER_HALT_ABORTED},
       {NDMP2_MOVER_HALT_INTERNAL_ERROR, NDMP9_MOVER_HALT_INTERNAL_ERROR},
       {NDMP2_MOVER_HALT_CONNECT_ERROR, NDMP9_MOVER_HALT_CONNECT_ERROR},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_mover_get_state_reply(ndmp2_mover_get_state_reply* reply2,
                                    ndmp9_mover_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_E_TO_9(reply2, reply9, state, ndmp_29_mover_state);
  CNVT_E_TO_9(reply2, reply9, pause_reason, ndmp_29_mover_pause_reason);
  CNVT_E_TO_9(reply2, reply9, halt_reason, ndmp_29_mover_halt_reason);

  CNVT_TO_9(reply2, reply9, record_size);
  CNVT_TO_9(reply2, reply9, record_num);
  CNVT_TO_9x(reply2, reply9, data_written, bytes_moved);
  CNVT_TO_9(reply2, reply9, seek_position);
  CNVT_TO_9(reply2, reply9, bytes_left_to_read);
  CNVT_TO_9(reply2, reply9, window_offset);
  CNVT_TO_9(reply2, reply9, window_length);

  return 0;
}

int ndmp_9to2_mover_get_state_reply(ndmp9_mover_get_state_reply* reply9,
                                    ndmp2_mover_get_state_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_E_FROM_9(reply2, reply9, state, ndmp_29_mover_state);
  CNVT_E_FROM_9(reply2, reply9, pause_reason, ndmp_29_mover_pause_reason);
  CNVT_E_FROM_9(reply2, reply9, halt_reason, ndmp_29_mover_halt_reason);

  CNVT_FROM_9(reply2, reply9, record_size);
  CNVT_FROM_9(reply2, reply9, record_num);
  CNVT_FROM_9x(reply2, reply9, data_written, bytes_moved);
  CNVT_FROM_9(reply2, reply9, seek_position);
  CNVT_FROM_9(reply2, reply9, bytes_left_to_read);
  CNVT_FROM_9(reply2, reply9, window_offset);
  CNVT_FROM_9(reply2, reply9, window_length);

  return 0;
}


// ndmp_mover_listen

int ndmp_2to9_mover_listen_request(ndmp2_mover_listen_request* request2,
                                   ndmp9_mover_listen_request* request9)
{
  int rc;

  rc = CNVT_E_TO_9(request2, request9, mode, ndmp_29_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request2, request9, mode); }
  rc = CNVT_E_TO_9(request2, request9, addr_type, ndmp_29_mover_addr_type);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request2, request9, addr_type); }

  return 0;
}

int ndmp_9to2_mover_listen_request(ndmp9_mover_listen_request* request9,
                                   ndmp2_mover_listen_request* request2)
{
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, mode, ndmp_29_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_FROM_9(request2, request9, mode); }
  rc = CNVT_E_FROM_9(request2, request9, addr_type, ndmp_29_mover_addr_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, addr_type);
  }

  return 0;
}

int ndmp_2to9_mover_listen_reply(ndmp2_mover_listen_reply* reply2,
                                 ndmp9_mover_listen_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);

  n_error
      += ndmp_2to9_mover_addr(&reply2->mover, &reply9->data_connection_addr);

  return n_error;
}

int ndmp_9to2_mover_listen_reply(ndmp9_mover_listen_reply* reply9,
                                 ndmp2_mover_listen_reply* reply2)
{
  int n_error = 0;

  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);

  n_error
      += ndmp_9to2_mover_addr(&reply9->data_connection_addr, &reply2->mover);

  return n_error;
}

/*
 * ndmp_mover_continue
 * no args request, just error reply
 */

/*
 * ndmp_mover_abort
 * no args request, just error reply
 */

/*
 * ndmp_mover_stop
 * no args request, just error reply
 */

/*
 * ndmp_mover_set_window
 * just error reply
 */

int ndmp_2to9_mover_set_window_request(ndmp2_mover_set_window_request* request2,
                                       ndmp9_mover_set_window_request* request9)
{
  CNVT_TO_9(request2, request9, offset);
  CNVT_TO_9(request2, request9, length);
  return 0;
}

int ndmp_9to2_mover_set_window_request(ndmp9_mover_set_window_request* request9,
                                       ndmp2_mover_set_window_request* request2)
{
  CNVT_FROM_9(request2, request9, offset);
  CNVT_FROM_9(request2, request9, length);
  return 0;
}


/*
 * ndmp_mover_read
 * just error reply
 */

int ndmp_2to9_mover_read_request(ndmp2_mover_read_request* request2,
                                 ndmp9_mover_read_request* request9)
{
  CNVT_TO_9(request2, request9, offset);
  CNVT_TO_9(request2, request9, length);
  return 0;
}

int ndmp_9to2_mover_read_request(ndmp9_mover_read_request* request9,
                                 ndmp2_mover_read_request* request2)
{
  CNVT_FROM_9(request2, request9, offset);
  CNVT_FROM_9(request2, request9, length);
  return 0;
}

/*
 * ndmp_mover_close
 * no args request, just error reply
 */


/*
 * ndmp_mover_set_record_size
 * just error reply
 */

int ndmp_2to9_mover_set_record_size_request(
    ndmp2_mover_set_record_size_request* request2,
    ndmp9_mover_set_record_size_request* request9)
{
  CNVT_TO_9x(request2, request9, len, record_size);
  return 0;
}

int ndmp_9to2_mover_set_record_size_request(
    ndmp9_mover_set_record_size_request* request9,
    ndmp2_mover_set_record_size_request* request2)
{
  CNVT_FROM_9x(request2, request9, len, record_size);
  return 0;
}


/*
 * DATA INTERFACES
 ****************************************************************
 */

/*
 * ndmp_name
 ****************************************************************
 */

int ndmp_2to9_name(ndmp2_name* name2, ndmp9_name* name9)
{
  name9->original_path = NDMOS_API_STRDUP(name2->name);
  name9->destination_path = NDMOS_API_STRDUP(name2->dest);
  if (name2->fh_info != NDMP_INVALID_U_QUAD) {
    name9->fh_info.valid = NDMP9_VALIDITY_VALID;
    name9->fh_info.value = name2->fh_info;
  } else {
    name9->fh_info.valid = NDMP9_VALIDITY_INVALID;
    name9->fh_info.value = NDMP_INVALID_U_QUAD;
  }

  return 0;
}

int ndmp_9to2_name(ndmp9_name* name9, ndmp2_name* name2)
{
  name2->name = NDMOS_API_STRDUP(name9->original_path);
  name2->dest = NDMOS_API_STRDUP(name9->destination_path);
  if (name9->fh_info.valid == NDMP9_VALIDITY_VALID) {
    name2->fh_info = name9->fh_info.value;
  } else {
    name2->fh_info = NDMP_INVALID_U_QUAD;
  }

  name2->ssid = 0;

  return 0;
}

int ndmp_2to9_name_vec(ndmp2_name* name2, ndmp9_name* name9, unsigned n_name)
{
  unsigned int i;

  for (i = 0; i < n_name; i++) ndmp_2to9_name(&name2[i], &name9[i]);

  return 0;
}

int ndmp_9to2_name_vec(ndmp9_name* name9, ndmp2_name* name2, unsigned n_name)
{
  unsigned int i;

  for (i = 0; i < n_name; i++) ndmp_9to2_name(&name9[i], &name2[i]);

  return 0;
}

int ndmp_2to9_name_vec_dup(ndmp2_name* name2,
                           ndmp9_name** name9_p,
                           unsigned n_name)
{
  *name9_p = NDMOS_MACRO_NEWN(ndmp9_name, n_name);
  if (!*name9_p) return -1;

  return ndmp_2to9_name_vec(name2, *name9_p, n_name);
}

int ndmp_9to2_name_vec_dup(ndmp9_name* name9,
                           ndmp2_name** name2_p,
                           unsigned n_name)
{
  *name2_p = NDMOS_MACRO_NEWN(ndmp2_name, n_name);
  if (!*name2_p) return -1;

  return ndmp_9to2_name_vec(name9, *name2_p, n_name);
}


/*
 * ndmp_data_get_state
 * no args request
 */

struct enum_conversion ndmp_29_data_operation[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_DATA_OP_NOACTION, NDMP9_DATA_OP_NOACTION},
       {NDMP2_DATA_OP_BACKUP, NDMP9_DATA_OP_BACKUP},
       {NDMP2_DATA_OP_RESTORE, NDMP9_DATA_OP_RECOVER},
       {NDMP2_DATA_OP_RESTORE_FILEHIST, NDMP9_DATA_OP_RECOVER_FILEHIST},
       END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_29_data_state[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_DATA_STATE_IDLE, NDMP9_DATA_STATE_IDLE},
       {NDMP2_DATA_STATE_ACTIVE, NDMP9_DATA_STATE_ACTIVE},
       {NDMP2_DATA_STATE_HALTED, NDMP9_DATA_STATE_HALTED},

       /* aliases */
       {NDMP2_DATA_STATE_ACTIVE, NDMP9_DATA_STATE_CONNECTED},
       {NDMP2_DATA_STATE_ACTIVE, NDMP9_DATA_STATE_LISTEN},

       END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_29_data_halt_reason[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_DATA_HALT_NA, NDMP9_DATA_HALT_NA},
       {NDMP2_DATA_HALT_SUCCESSFUL, NDMP9_DATA_HALT_SUCCESSFUL},
       {NDMP2_DATA_HALT_ABORTED, NDMP9_DATA_HALT_ABORTED},
       {NDMP2_DATA_HALT_INTERNAL_ERROR, NDMP9_DATA_HALT_INTERNAL_ERROR},
       {NDMP2_DATA_HALT_CONNECT_ERROR, NDMP9_DATA_HALT_CONNECT_ERROR},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_data_get_state_reply(ndmp2_data_get_state_reply* reply2,
                                   ndmp9_data_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);
  CNVT_E_TO_9(reply2, reply9, operation, ndmp_29_data_operation);
  CNVT_E_TO_9(reply2, reply9, state, ndmp_29_data_state);
  CNVT_E_TO_9(reply2, reply9, halt_reason, ndmp_29_data_halt_reason);

  CNVT_TO_9(reply2, reply9, bytes_processed);

  CNVT_VUQ_TO_9(reply2, reply9, est_bytes_remain);
  CNVT_VUL_TO_9(reply2, reply9, est_time_remain);

  ndmp_2to9_mover_addr(&reply2->mover, &reply9->data_connection_addr);

  CNVT_TO_9(reply2, reply9, read_offset);
  CNVT_TO_9(reply2, reply9, read_length);

  return 0;
}

int ndmp_9to2_data_get_state_reply(ndmp9_data_get_state_reply* reply9,
                                   ndmp2_data_get_state_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);
  CNVT_E_FROM_9(reply2, reply9, operation, ndmp_29_data_operation);
  CNVT_E_FROM_9(reply2, reply9, state, ndmp_29_data_state);
  CNVT_E_FROM_9(reply2, reply9, halt_reason, ndmp_29_data_halt_reason);

  CNVT_FROM_9(reply2, reply9, bytes_processed);

  CNVT_VUQ_FROM_9(reply2, reply9, est_bytes_remain);
  CNVT_VUL_FROM_9(reply2, reply9, est_time_remain);

  ndmp_9to2_mover_addr(&reply9->data_connection_addr, &reply2->mover);

  CNVT_FROM_9(reply2, reply9, read_offset);
  CNVT_FROM_9(reply2, reply9, read_length);

  return 0;
}

/*
 * ndmp_data_start_backup
 * just error reply
 */

int ndmp_2to9_data_start_backup_request(
    ndmp2_data_start_backup_request* request2,
    ndmp9_data_start_backup_request* request9)
{
  int n_error = 0;

  CNVT_STRDUP_TO_9(request2, request9, bu_type);

  ndmp_2to9_pval_vec_dup(request2->env.env_val, &request9->env.env_val,
                         request2->env.env_len);

  request9->env.env_len = request2->env.env_len;

  n_error += ndmp_2to9_mover_addr(&request2->mover, &request9->addr);

  return n_error;
}

int ndmp_9to2_data_start_backup_request(
    ndmp9_data_start_backup_request* request9,
    ndmp2_data_start_backup_request* request2)
{
  int n_error = 0;

  CNVT_STRDUP_FROM_9(request2, request9, bu_type);

  ndmp_9to2_pval_vec_dup(request9->env.env_val, &request2->env.env_val,
                         request9->env.env_len);

  request2->env.env_len = request9->env.env_len;

  n_error += ndmp_9to2_mover_addr(&request9->addr, &request2->mover);

  return n_error;
}


/*
 * ndmp_data_start_recover
 * ndmp_data_start_recover_filehist
 * just error reply
 */

int ndmp_2to9_data_start_recover_request(
    ndmp2_data_start_recover_request* request2,
    ndmp9_data_start_recover_request* request9)
{
  int n_error = 0;

  CNVT_STRDUP_TO_9(request2, request9, bu_type);

  ndmp_2to9_pval_vec_dup(request2->env.env_val, &request9->env.env_val,
                         request2->env.env_len);

  request9->env.env_len = request2->env.env_len;

  ndmp_2to9_name_vec_dup(request2->nlist.nlist_val, &request9->nlist.nlist_val,
                         request2->nlist.nlist_len);

  request9->nlist.nlist_len = request2->nlist.nlist_len;

  n_error += ndmp_2to9_mover_addr(&request2->mover, &request9->addr);

  return n_error;
}

int ndmp_9to2_data_start_recover_request(
    ndmp9_data_start_recover_request* request9,
    ndmp2_data_start_recover_request* request2)
{
  int n_error = 0;

  CNVT_STRDUP_FROM_9(request2, request9, bu_type);

  ndmp_9to2_pval_vec_dup(request9->env.env_val, &request2->env.env_val,
                         request9->env.env_len);

  request2->env.env_len = request9->env.env_len;

  ndmp_9to2_name_vec_dup(request9->nlist.nlist_val, &request2->nlist.nlist_val,
                         request9->nlist.nlist_len);

  request2->nlist.nlist_len = request9->nlist.nlist_len;

  n_error += ndmp_9to2_mover_addr(&request9->addr, &request2->mover);

  return n_error;
}


/*
 * ndmp_data_abort
 * no args request, just error reply
 */

/*
 * ndmp_data_get_env
 * no args request
 */

int ndmp_2to9_data_get_env_reply(ndmp2_data_get_env_reply* reply2,
                                 ndmp9_data_get_env_reply* reply9)
{
  CNVT_E_TO_9(reply2, reply9, error, ndmp_29_error);

  ndmp_2to9_pval_vec_dup(reply2->env.env_val, &reply9->env.env_val,
                         reply2->env.env_len);

  reply9->env.env_len = reply2->env.env_len;

  return 0;
}

int ndmp_9to2_data_get_env_reply(ndmp9_data_get_env_reply* reply9,
                                 ndmp2_data_get_env_reply* reply2)
{
  CNVT_E_FROM_9(reply2, reply9, error, ndmp_29_error);

  ndmp_9to2_pval_vec_dup(reply9->env.env_val, &reply2->env.env_val,
                         reply9->env.env_len);

  reply2->env.env_len = reply9->env.env_len;

  return 0;
}


/*
 * ndmp_data_stop
 * no args request, just error reply
 */


/*
 * NOTIFY INTERFACES
 ****************************************************************
 */

/*
 * ndmp_notify_data_halted
 * just error reply
 */

int ndmp_2to9_notify_data_halted_request(
    ndmp2_notify_data_halted_request* request2,
    ndmp9_notify_data_halted_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, reason, ndmp_29_data_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request2, request9, reason);
    n_error++;
  }

  return n_error;
}

int ndmp_9to2_notify_data_halted_request(
    ndmp9_notify_data_halted_request* request9,
    ndmp2_notify_data_halted_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, reason, ndmp_29_data_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, reason);
    n_error++;
  }

  request2->text_reason = NDMOS_API_STRDUP("whatever");

  return n_error;
}


/*
 * ndmp_notify_connected
 * just error reply
 */

/* NDMP2_NOTIFY_CONNECTED */
struct enum_conversion ndmp_29_connect_reason[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_CONNECTED, NDMP9_CONNECTED},
       {NDMP2_SHUTDOWN, NDMP9_SHUTDOWN},
       {NDMP2_REFUSED, NDMP9_REFUSED},
       END_ENUM_CONVERSION_TABLE};

int ndmp_2to9_notify_connected_request(ndmp2_notify_connected_request* request2,
                                       ndmp9_notify_connected_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, reason, ndmp_29_connect_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request2, request9, reason);
    n_error++;
  }

  CNVT_TO_9(request2, request9, protocol_version);

  CNVT_STRDUP_TO_9(request2, request9, text_reason);

  return n_error;
}

int ndmp_9to2_notify_connected_request(ndmp9_notify_connected_request* request9,
                                       ndmp2_notify_connected_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, reason, ndmp_29_connect_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, reason);
    n_error++;
  }

  CNVT_FROM_9(request2, request9, protocol_version);

  CNVT_STRDUP_FROM_9(request2, request9, text_reason);

  return n_error;
}


/*
 * ndmp_notify_mover_halted
 * just error reply
 */

int ndmp_2to9_notify_mover_halted_request(
    ndmp2_notify_mover_halted_request* request2,
    ndmp9_notify_mover_halted_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, reason, ndmp_29_mover_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request2, request9, reason);
    n_error++;
  }

  return n_error;
}

int ndmp_9to2_notify_mover_halted_request(
    ndmp9_notify_mover_halted_request* request9,
    ndmp2_notify_mover_halted_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, reason, ndmp_29_mover_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, reason);
    n_error++;
  }

  request2->text_reason = NDMOS_API_STRDUP("Whatever");

  return n_error;
}


/*
 * ndmp_notify_mover_paused
 * just error reply
 */

int ndmp_2to9_notify_mover_paused_request(
    ndmp2_notify_mover_paused_request* request2,
    ndmp9_notify_mover_paused_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request2, request9, reason, ndmp_29_mover_pause_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request2, request9, reason);
    n_error++;
  }

  CNVT_TO_9(request2, request9, seek_position);

  return n_error;
}

int ndmp_9to2_notify_mover_paused_request(
    ndmp9_notify_mover_paused_request* request9,
    ndmp2_notify_mover_paused_request* request2)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request2, request9, reason, ndmp_29_mover_pause_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request2, request9, reason);
    n_error++;
  }

  CNVT_FROM_9(request2, request9, seek_position);

  return n_error;
}


/*
 * ndmp_notify_data_read
 * just error reply
 */

int ndmp_2to9_notify_data_read_request(ndmp2_notify_data_read_request* request2,
                                       ndmp9_notify_data_read_request* request9)
{
  CNVT_TO_9(request2, request9, offset);
  CNVT_TO_9(request2, request9, length);
  return 0;
}

int ndmp_9to2_notify_data_read_request(ndmp9_notify_data_read_request* request9,
                                       ndmp2_notify_data_read_request* request2)
{
  CNVT_FROM_9(request2, request9, offset);
  CNVT_FROM_9(request2, request9, length);
  return 0;
}


/*
 * LOG INTERFACES
 ****************************************************************
 */

/*
 * ndmp2_log_log and ndmp2_log_debug are not handled
 * by the ndmp9 translater. Like ndmp2_config_get_butype_attr,
 * these NDMP2 log interfaces do not pair well with the
 * ndmp[349] interfaces. So they are handled directly
 * rather than by translation.
 */

struct enum_conversion ndmp_29_recovery_status[] = {
    {NDMP2_UNDEFINED_ERR, NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR}, /* default */
    {NDMP2_NO_ERR, NDMP9_RECOVERY_SUCCESSFUL},
    {NDMP2_PERMISSION_ERR, NDMP9_RECOVERY_FAILED_PERMISSION},
    {NDMP2_FILE_NOT_FOUND_ERR, NDMP9_RECOVERY_FAILED_NOT_FOUND},
    {NDMP2_BAD_FILE_ERR, NDMP9_RECOVERY_FAILED_NO_DIRECTORY},
    {NDMP2_NO_MEM_ERR, NDMP9_RECOVERY_FAILED_OUT_OF_MEMORY},
    {NDMP2_IO_ERR, NDMP9_RECOVERY_FAILED_IO_ERROR},
    {NDMP2_UNDEFINED_ERR, NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR},
    END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_log_file_request(ndmp2_log_file_request* request2,
                               ndmp9_log_file_request* request9)
{
  request9->recovery_status
      = convert_enum_to_9(ndmp_29_recovery_status, request2->error);
  CNVT_STRDUP_TO_9(request2, request9, name);
  return 0;
}

int ndmp_9to2_log_file_request(ndmp9_log_file_request* request9,
                               ndmp2_log_file_request* request2)
{
  request2->error
      = convert_enum_from_9(ndmp_29_recovery_status, request9->recovery_status);
  CNVT_STRDUP_FROM_9(request2, request9, name);
  return 0;
}


/*
 * FILE HISTORY INTERFACES
 ****************************************************************
 */

/*
 * ndmp[_unix]_file_stat
 */

struct enum_conversion ndmp_29_file_type[]
    = {{
           NDMP_INVALID_GENERAL,
           NDMP_INVALID_GENERAL,
       }, /* default */
       {NDMP2_FILE_DIR, NDMP9_FILE_DIR},
       {NDMP2_FILE_FIFO, NDMP9_FILE_FIFO},
       {NDMP2_FILE_CSPEC, NDMP9_FILE_CSPEC},
       {NDMP2_FILE_BSPEC, NDMP9_FILE_BSPEC},
       {NDMP2_FILE_REG, NDMP9_FILE_REG},
       {NDMP2_FILE_SLINK, NDMP9_FILE_SLINK},
       {NDMP2_FILE_SOCK, NDMP9_FILE_SOCK},
       END_ENUM_CONVERSION_TABLE};


int ndmp_2to9_unix_file_stat(ndmp2_unix_file_stat* fstat2,
                             ndmp9_file_stat* fstat9)
{
  CNVT_E_TO_9(fstat2, fstat9, ftype, ndmp_29_file_type);

  CNVT_VUL_TO_9(fstat2, fstat9, mtime);
  CNVT_VUL_TO_9(fstat2, fstat9, atime);
  CNVT_VUL_TO_9(fstat2, fstat9, ctime);
  CNVT_VUL_TO_9(fstat2, fstat9, uid);
  CNVT_VUL_TO_9(fstat2, fstat9, gid);

  CNVT_VUL_TO_9(fstat2, fstat9, mode);

  CNVT_VUQ_TO_9(fstat2, fstat9, size);
  CNVT_VUQ_TO_9(fstat2, fstat9, fh_info);

  return 0;
}

int ndmp_9to2_unix_file_stat(ndmp9_file_stat* fstat9,
                             ndmp2_unix_file_stat* fstat2)
{
  CNVT_E_FROM_9(fstat2, fstat9, ftype, ndmp_29_file_type);

  CNVT_VUL_FROM_9(fstat2, fstat9, mtime);
  CNVT_VUL_FROM_9(fstat2, fstat9, atime);
  CNVT_VUL_FROM_9(fstat2, fstat9, ctime);
  CNVT_VUL_FROM_9(fstat2, fstat9, uid);
  CNVT_VUL_FROM_9(fstat2, fstat9, gid);

  CNVT_VUL_FROM_9(fstat2, fstat9, mode);

  CNVT_VUQ_FROM_9(fstat2, fstat9, size);
  CNVT_VUQ_FROM_9(fstat2, fstat9, fh_info);

  /* node ignored */

  return 0;
}


/*
 * ndmp_fh_add_unix_path_request
 */

int ndmp_2to9_fh_add_unix_path_request(ndmp2_fh_add_unix_path_request* request2,
                                       ndmp9_fh_add_file_request* request9)
{
  int n_ent = request2->paths.paths_len;
  int i;
  ndmp9_file* table;

  table = NDMOS_MACRO_NEWN(ndmp9_file, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp2_fh_unix_path* ent2 = &request2->paths.paths_val[i];
    ndmp9_file* ent9 = &table[i];

    CNVT_STRDUP_TO_9x(ent2, ent9, name, unix_path);
    ndmp_2to9_unix_file_stat(&ent2->fstat, &ent9->fstat);
  }

  request9->files.files_len = n_ent;
  request9->files.files_val = table;

  return 0;
}

int ndmp_9to2_fh_add_unix_path_request(ndmp9_fh_add_file_request* request9,
                                       ndmp2_fh_add_unix_path_request* request2)
{
  int n_ent = request9->files.files_len;
  int i;
  ndmp2_fh_unix_path* table;

  table = NDMOS_MACRO_NEWN(ndmp2_fh_unix_path, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp2_fh_unix_path* ent2 = &table[i];
    ndmp9_file* ent9 = &request9->files.files_val[i];

    CNVT_STRDUP_FROM_9x(ent2, ent9, name, unix_path);
    ndmp_9to2_unix_file_stat(&ent9->fstat, &ent2->fstat);
  }

  request2->paths.paths_len = n_ent;
  request2->paths.paths_val = table;

  return 0;
}


/*
 * ndmp_fh_add_unix_dir
 */

int ndmp_2to9_fh_add_unix_dir_request(ndmp2_fh_add_unix_dir_request* request2,
                                      ndmp9_fh_add_dir_request* request9)
{
  int n_ent = request2->dirs.dirs_len;
  int i;
  ndmp9_dir* table;

  table = NDMOS_MACRO_NEWN(ndmp9_dir, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp2_fh_unix_dir* ent2 = &request2->dirs.dirs_val[i];
    ndmp9_dir* ent9 = &table[i];

    CNVT_STRDUP_TO_9x(ent2, ent9, name, unix_name);
    CNVT_TO_9(ent2, ent9, node);
    CNVT_TO_9(ent2, ent9, parent);
  }

  request9->dirs.dirs_len = n_ent;
  request9->dirs.dirs_val = table;

  return 0;
}

int ndmp_2to9_fh_add_unix_dir_free_request(ndmp9_fh_add_dir_request* request9)
{
  int i;

  if (request9) {
    if (request9->dirs.dirs_val) {
      int n_ent = request9->dirs.dirs_len;

      for (i = 0; i < n_ent; i++) {
        ndmp9_dir* ent9 = &request9->dirs.dirs_val[i];
        if (ent9->unix_name) NDMOS_API_FREE(ent9->unix_name);
        ent9->unix_name = 0;
      }

      NDMOS_API_FREE(request9->dirs.dirs_val);
    }
    request9->dirs.dirs_val = 0;
  }
  return 0;
}

int ndmp_9to2_fh_add_unix_dir_request(ndmp9_fh_add_dir_request* request9,
                                      ndmp2_fh_add_unix_dir_request* request2)
{
  int n_ent = request9->dirs.dirs_len;
  int i;
  ndmp2_fh_unix_dir* table;

  table = NDMOS_MACRO_NEWN(ndmp2_fh_unix_dir, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp9_dir* ent9 = &request9->dirs.dirs_val[i];
    ndmp2_fh_unix_dir* ent2 = &table[i];

    CNVT_STRDUP_FROM_9x(ent2, ent9, name, unix_name);
    CNVT_FROM_9(ent2, ent9, node);
    CNVT_FROM_9(ent2, ent9, parent);
  }

  request2->dirs.dirs_len = n_ent;
  request2->dirs.dirs_val = table;

  return 0;
}

int ndmp_9to2_fh_add_unix_dir_free_request(
    ndmp2_fh_add_unix_dir_request* request2)
{
  int i;

  if (request2) {
    if (request2->dirs.dirs_val) {
      int n_ent = request2->dirs.dirs_len;

      for (i = 0; i < n_ent; i++) {
        ndmp2_fh_unix_dir* ent2 = &request2->dirs.dirs_val[i];

        if (ent2->name) NDMOS_API_FREE(ent2->name);
        ent2->name = 0;
      }

      NDMOS_API_FREE(request2->dirs.dirs_val);
    }
    request2->dirs.dirs_val = 0;
  }

  return 0;
}


/*
 * ndmp_fh_add_unix_node
 */

int ndmp_2to9_fh_add_unix_node_request(ndmp2_fh_add_unix_node_request* request2,
                                       ndmp9_fh_add_node_request* request9)
{
  int n_ent = request2->nodes.nodes_len;
  int i;
  ndmp9_node* table;

  table = NDMOS_MACRO_NEWN(ndmp9_node, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp2_fh_unix_node* ent2 = &request2->nodes.nodes_val[i];
    ndmp9_node* ent9 = &table[i];

    ndmp_2to9_unix_file_stat(&ent2->fstat, &ent9->fstat);
    ent9->fstat.node.valid = NDMP9_VALIDITY_VALID;
    ent9->fstat.node.value = ent2->node;
  }

  request9->nodes.nodes_len = n_ent;
  request9->nodes.nodes_val = table;

  return 0;
}

int ndmp_2to9_fh_add_unix_node_free_request(ndmp9_fh_add_node_request* request9)
{
  if (request9) {
    if (request9->nodes.nodes_val) {
      NDMOS_API_FREE(request9->nodes.nodes_val);
    }
    request9->nodes.nodes_val = 0;
  }
  return 0;
}

int ndmp_9to2_fh_add_unix_node_request(ndmp9_fh_add_node_request* request9,
                                       ndmp2_fh_add_unix_node_request* request2)
{
  int n_ent = request9->nodes.nodes_len;
  int i;
  ndmp2_fh_unix_node* table;

  table = NDMOS_MACRO_NEWN(ndmp2_fh_unix_node, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp9_node* ent9 = &request9->nodes.nodes_val[i];
    ndmp2_fh_unix_node* ent2 = &table[i];

    ndmp_9to2_unix_file_stat(&ent9->fstat, &ent2->fstat);
    ent2->node = ent9->fstat.node.value;
  }

  request2->nodes.nodes_len = n_ent;
  request2->nodes.nodes_val = table;

  return 0;
}

int ndmp_9to2_fh_add_unix_node_free_request(
    ndmp2_fh_add_unix_node_request* request2)
{
  if (request2) {
    if (request2->nodes.nodes_val) {
      NDMOS_API_FREE(request2->nodes.nodes_val);
    }
    request2->nodes.nodes_val = 0;
  }
  return 0;
}


/*
 * request/reply translation
 */

#  define NO_ARG_REQUEST ndmp_xtox_no_arguments, ndmp_xtox_no_arguments

#  define JUST_ERROR_REPLY ndmp_2to9_error, ndmp_9to2_error

#  define NO_ARG_REQUEST_JUST_ERROR_REPLY NO_ARG_REQUEST, JUST_ERROR_REPLY

#  define NO_MEMUSED_REQUEST ndmp_xtox_no_memused, ndmp_xtox_no_memused

#  define NO_MEMUSED_REPLY ndmp_xtox_no_memused, ndmp_xtox_no_memused

#  define NO_MEMUSED                                                  \
    ndmp_xtox_no_memused, ndmp_xtox_no_memused, ndmp_xtox_no_memused, \
        ndmp_xtox_no_memused


struct reqrep_xlate ndmp2_reqrep_xlate_table[] = {
    {
        NDMP2_CONNECT_OPEN, NDMP9_CONNECT_OPEN, ndmp_2to9_connect_open_request,
        ndmp_9to2_connect_open_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_CONNECT_CLIENT_AUTH, NDMP9_CONNECT_CLIENT_AUTH,
        ndmp_2to9_connect_client_auth_request,
        ndmp_9to2_connect_client_auth_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_CONNECT_CLOSE, NDMP9_CONNECT_CLOSE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY, /* actually no reply */
        NO_MEMUSED /* no memory free routines written yet */
    },
#  ifdef notyet
    {
        NDMP2_CONNECT_SERVER_AUTH, NDMP9_CONNECT_SERVER_AUTH,
        ndmp_2to9_connect_server_auth_request,
        ndmp_9to2_connect_server_auth_request,
        ndmp_2to9_connect_server_auth_reply,
        ndmp_9to2_connect_server_auth_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
#  endif /* notyet */
    {
        NDMP2_CONFIG_GET_HOST_INFO, NDMP9_CONFIG_GET_HOST_INFO, NO_ARG_REQUEST,
        ndmp_2to9_config_get_host_info_reply,
        ndmp_9to2_config_get_host_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    /*
     * ndmp2_config_get_butype_attr handled
     * as version specific dispatch function
     * in ndma_comm_dispatch.c
     */
    {
        NDMP2_CONFIG_GET_MOVER_TYPE, NDMP9_CONFIG_GET_CONNECTION_TYPE,
        NO_ARG_REQUEST, ndmp_2to9_config_get_mover_type_reply,
        ndmp_9to2_config_get_mover_type_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_CONFIG_GET_AUTH_ATTR, NDMP9_CONFIG_GET_AUTH_ATTR,
        ndmp_2to9_config_get_auth_attr_request,
        ndmp_9to2_config_get_auth_attr_request,
        ndmp_2to9_config_get_auth_attr_reply,
        ndmp_9to2_config_get_auth_attr_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP2_SCSI_OPEN, NDMP9_SCSI_OPEN, ndmp_2to9_scsi_open_request,
        ndmp_9to2_scsi_open_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_CLOSE, NDMP9_SCSI_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_GET_STATE, NDMP9_SCSI_GET_STATE, NO_ARG_REQUEST,
        ndmp_2to9_scsi_get_state_reply, ndmp_9to2_scsi_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_SET_TARGET, NDMP9_SCSI_SET_TARGET,
        ndmp_2to9_scsi_set_target_request, ndmp_9to2_scsi_set_target_request,
        JUST_ERROR_REPLY, NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_RESET_DEVICE, NDMP9_SCSI_RESET_DEVICE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_RESET_BUS, NDMP9_SCSI_RESET_BUS,
        NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_SCSI_EXECUTE_CDB, NDMP9_SCSI_EXECUTE_CDB,
        ndmp_2to9_execute_cdb_request, ndmp_9to2_execute_cdb_request,
        ndmp_2to9_execute_cdb_reply, ndmp_9to2_execute_cdb_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },


    {
        NDMP2_TAPE_OPEN, NDMP9_TAPE_OPEN, ndmp_2to9_tape_open_request,
        ndmp_9to2_tape_open_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_CLOSE, NDMP9_TAPE_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_GET_STATE, NDMP9_TAPE_GET_STATE, NO_ARG_REQUEST,
        ndmp_2to9_tape_get_state_reply, ndmp_9to2_tape_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_MTIO, NDMP9_TAPE_MTIO, ndmp_2to9_tape_mtio_request,
        ndmp_9to2_tape_mtio_request, ndmp_2to9_tape_mtio_reply,
        ndmp_9to2_tape_mtio_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_WRITE, NDMP9_TAPE_WRITE, ndmp_2to9_tape_write_request,
        ndmp_9to2_tape_write_request, ndmp_2to9_tape_write_reply,
        ndmp_9to2_tape_write_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_READ, NDMP9_TAPE_READ, ndmp_2to9_tape_read_request,
        ndmp_9to2_tape_read_request, ndmp_2to9_tape_read_reply,
        ndmp_9to2_tape_read_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_TAPE_EXECUTE_CDB, NDMP9_TAPE_EXECUTE_CDB,
        ndmp_2to9_execute_cdb_request, ndmp_9to2_execute_cdb_request,
        ndmp_2to9_execute_cdb_reply, ndmp_9to2_execute_cdb_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP2_DATA_GET_STATE, NDMP9_DATA_GET_STATE, NO_ARG_REQUEST,
        ndmp_2to9_data_get_state_reply, ndmp_9to2_data_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_START_BACKUP, NDMP9_DATA_START_BACKUP,
        ndmp_2to9_data_start_backup_request,
        ndmp_9to2_data_start_backup_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_START_RECOVER, NDMP9_DATA_START_RECOVER,
        ndmp_2to9_data_start_recover_request,
        ndmp_9to2_data_start_recover_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_ABORT, NDMP9_DATA_ABORT, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_GET_ENV, NDMP9_DATA_GET_ENV, NO_ARG_REQUEST,
        ndmp_2to9_data_get_env_reply, ndmp_9to2_data_get_env_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_STOP, NDMP9_DATA_STOP, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_DATA_START_RECOVER_FILEHIST, NDMP9_DATA_START_RECOVER_FILEHIST,
        ndmp_2to9_data_start_recover_request,
        ndmp_9to2_data_start_recover_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP2_NOTIFY_DATA_HALTED, NDMP9_NOTIFY_DATA_HALTED,
        ndmp_2to9_notify_data_halted_request,
        ndmp_9to2_notify_data_halted_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP2_NOTIFY_CONNECTED, NDMP9_NOTIFY_CONNECTED,
        ndmp_2to9_notify_connected_request, ndmp_9to2_notify_connected_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP2_NOTIFY_MOVER_HALTED, NDMP9_NOTIFY_MOVER_HALTED,
        ndmp_2to9_notify_mover_halted_request,
        ndmp_9to2_notify_mover_halted_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP2_NOTIFY_MOVER_PAUSED, NDMP9_NOTIFY_MOVER_PAUSED,
        ndmp_2to9_notify_mover_paused_request,
        ndmp_9to2_notify_mover_paused_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP2_NOTIFY_DATA_READ, NDMP9_NOTIFY_DATA_READ,
        ndmp_2to9_notify_data_read_request, ndmp_9to2_notify_data_read_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },

    {
        NDMP2_LOG_FILE, NDMP9_LOG_FILE, ndmp_2to9_log_file_request,
        ndmp_9to2_log_file_request, JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP2_FH_ADD_UNIX_PATH, NDMP9_FH_ADD_FILE,
        ndmp_2to9_fh_add_unix_path_request, ndmp_9to2_fh_add_unix_path_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {NDMP2_FH_ADD_UNIX_DIR, NDMP9_FH_ADD_DIR, ndmp_2to9_fh_add_unix_dir_request,
     ndmp_9to2_fh_add_unix_dir_request,
     JUST_ERROR_REPLY, /* no reply actually */
     ndmp_2to9_fh_add_unix_dir_free_request,
     ndmp_9to2_fh_add_unix_dir_free_request, NO_MEMUSED_REPLY},
    {NDMP2_FH_ADD_UNIX_NODE, NDMP9_FH_ADD_NODE,
     ndmp_2to9_fh_add_unix_node_request, ndmp_9to2_fh_add_unix_node_request,
     JUST_ERROR_REPLY, /* no reply actually */
     ndmp_2to9_fh_add_unix_node_free_request,
     ndmp_9to2_fh_add_unix_node_free_request, NO_MEMUSED_REPLY},

    {
        NDMP2_MOVER_GET_STATE, NDMP9_MOVER_GET_STATE, NO_ARG_REQUEST,
        ndmp_2to9_mover_get_state_reply, ndmp_9to2_mover_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_LISTEN, NDMP9_MOVER_LISTEN, ndmp_2to9_mover_listen_request,
        ndmp_9to2_mover_listen_request, ndmp_2to9_mover_listen_reply,
        ndmp_9to2_mover_listen_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_SET_RECORD_SIZE, NDMP9_MOVER_SET_RECORD_SIZE,
        ndmp_2to9_mover_set_record_size_request,
        ndmp_9to2_mover_set_record_size_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_SET_WINDOW, NDMP9_MOVER_SET_WINDOW,
        ndmp_2to9_mover_set_window_request, ndmp_9to2_mover_set_window_request,
        JUST_ERROR_REPLY, NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_CONTINUE, NDMP9_MOVER_CONTINUE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_ABORT, NDMP9_MOVER_ABORT, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_STOP, NDMP9_MOVER_STOP, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_READ, NDMP9_MOVER_READ, ndmp_2to9_mover_read_request,
        ndmp_9to2_mover_read_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP2_MOVER_CLOSE, NDMP9_MOVER_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {0},
};


#endif /* !NDMOS_OPTION_NO_NDMP2 */
