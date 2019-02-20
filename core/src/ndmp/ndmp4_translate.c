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


#ifndef NDMOS_OPTION_NO_NDMP4


/*
 * Pervasive Types
 ****************************************************************
 */

/*
 * ndmp_error
 ****************************************************************
 */

struct enum_conversion ndmp_49_error[] = {
    {NDMP4_UNDEFINED_ERR, NDMP9_UNDEFINED_ERR}, /* default */
    {NDMP4_NO_ERR, NDMP9_NO_ERR},
    {NDMP4_NOT_SUPPORTED_ERR, NDMP9_NOT_SUPPORTED_ERR},
    {NDMP4_DEVICE_BUSY_ERR, NDMP9_DEVICE_BUSY_ERR},
    {NDMP4_DEVICE_OPENED_ERR, NDMP9_DEVICE_OPENED_ERR},
    {NDMP4_NOT_AUTHORIZED_ERR, NDMP9_NOT_AUTHORIZED_ERR},
    {NDMP4_PERMISSION_ERR, NDMP9_PERMISSION_ERR},
    {NDMP4_DEV_NOT_OPEN_ERR, NDMP9_DEV_NOT_OPEN_ERR},
    {NDMP4_IO_ERR, NDMP9_IO_ERR},
    {NDMP4_TIMEOUT_ERR, NDMP9_TIMEOUT_ERR},
    {NDMP4_ILLEGAL_ARGS_ERR, NDMP9_ILLEGAL_ARGS_ERR},
    {NDMP4_NO_TAPE_LOADED_ERR, NDMP9_NO_TAPE_LOADED_ERR},
    {NDMP4_WRITE_PROTECT_ERR, NDMP9_WRITE_PROTECT_ERR},
    {NDMP4_EOF_ERR, NDMP9_EOF_ERR},
    {NDMP4_EOM_ERR, NDMP9_EOM_ERR},
    {NDMP4_FILE_NOT_FOUND_ERR, NDMP9_FILE_NOT_FOUND_ERR},
    {NDMP4_BAD_FILE_ERR, NDMP9_BAD_FILE_ERR},
    {NDMP4_NO_DEVICE_ERR, NDMP9_NO_DEVICE_ERR},
    {NDMP4_NO_BUS_ERR, NDMP9_NO_BUS_ERR},
    {NDMP4_XDR_DECODE_ERR, NDMP9_XDR_DECODE_ERR},
    {NDMP4_ILLEGAL_STATE_ERR, NDMP9_ILLEGAL_STATE_ERR},
    {NDMP4_UNDEFINED_ERR, NDMP9_UNDEFINED_ERR},
    {NDMP4_XDR_ENCODE_ERR, NDMP9_XDR_ENCODE_ERR},
    {NDMP4_NO_MEM_ERR, NDMP9_NO_MEM_ERR},
    {NDMP4_CONNECT_ERR, NDMP9_CONNECT_ERR},
    {NDMP4_SEQUENCE_NUM_ERR, NDMP9_SEQUENCE_NUM_ERR},
    {NDMP4_READ_IN_PROGRESS_ERR, NDMP9_READ_IN_PROGRESS_ERR},
    {NDMP4_PRECONDITION_ERR, NDMP9_PRECONDITION_ERR},
    {NDMP4_CLASS_NOT_SUPPORTED, NDMP9_CLASS_NOT_SUPPORTED},
    {NDMP4_VERSION_NOT_SUPPORTED, NDMP9_VERSION_NOT_SUPPORTED},
    {NDMP4_EXT_DUPL_CLASSES, NDMP9_EXT_DUPL_CLASSES},
    {NDMP4_EXT_DN_ILLEGAL, NDMP9_EXT_DN_ILLEGAL},

    END_ENUM_CONVERSION_TABLE};


extern int ndmp_4to9_error(ndmp4_error* error4, ndmp9_error* error9)
{
  *error9 = convert_enum_to_9(ndmp_49_error, *error4);
  return 0;
}

extern int ndmp_9to4_error(ndmp9_error* error9, ndmp4_error* error4)
{
  *error4 = convert_enum_from_9(ndmp_49_error, *error9);
  return 0;
}


/*
 * ndmp_pval
 ****************************************************************
 */

int ndmp_4to9_pval(ndmp4_pval* pval4, ndmp9_pval* pval9)
{
  CNVT_STRDUP_TO_9(pval4, pval9, name);
  CNVT_STRDUP_TO_9(pval4, pval9, value);

  return 0;
}

int ndmp_9to4_pval(ndmp9_pval* pval9, ndmp4_pval* pval4)
{
  CNVT_STRDUP_FROM_9(pval4, pval9, name);
  CNVT_STRDUP_FROM_9(pval4, pval9, value);

  return 0;
}

int ndmp_4to9_pval_vec(ndmp4_pval* pval4, ndmp9_pval* pval9, unsigned n_pval)
{
  unsigned int i;

  for (i = 0; i < n_pval; i++) ndmp_4to9_pval(&pval4[i], &pval9[i]);

  return 0;
}

int ndmp_9to4_pval_vec(ndmp9_pval* pval9, ndmp4_pval* pval4, unsigned n_pval)
{
  unsigned int i;

  for (i = 0; i < n_pval; i++) ndmp_9to4_pval(&pval9[i], &pval4[i]);

  return 0;
}

int ndmp_4to9_pval_vec_dup(ndmp4_pval* pval4,
                           ndmp9_pval** pval9_p,
                           unsigned n_pval)
{
  *pval9_p = NDMOS_MACRO_NEWN(ndmp9_pval, n_pval);
  if (!*pval9_p) return -1;

  return ndmp_4to9_pval_vec(pval4, *pval9_p, n_pval);
}

int ndmp_9to4_pval_vec_dup(ndmp9_pval* pval9,
                           ndmp4_pval** pval4_p,
                           unsigned n_pval)
{
  *pval4_p = NDMOS_MACRO_NEWN(ndmp4_pval, n_pval);
  if (!*pval4_p) return -1;

  return ndmp_9to4_pval_vec(pval9, *pval4_p, n_pval);
}

int ndmp_4to9_pval_free(ndmp9_pval* pval9)
{
  CNVT_FREE(pval9, name);
  CNVT_FREE(pval9, value);

  return 0;
}

int ndmp_4to9_pval_vec_free(ndmp9_pval* pval9, unsigned n_pval)
{
  unsigned int i;

  for (i = 0; i < n_pval; i++) ndmp_4to9_pval_free(&pval9[i]);
  NDMOS_MACRO_FREE(pval9);

  return 0;
}

/*
 * ndmp_addr
 ****************************************************************
 */

struct enum_conversion ndmp_49_addr_type[] = {
    {NDMP_INVALID_GENERAL, NDMP_INVALID_GENERAL}, /* default */
    {NDMP4_ADDR_LOCAL, NDMP9_ADDR_LOCAL},
    {NDMP4_ADDR_TCP, NDMP9_ADDR_TCP},
    END_ENUM_CONVERSION_TABLE};


extern int ndmp_4to9_addr(ndmp4_addr* addr4, ndmp9_addr* addr9)
{
  switch (addr4->addr_type) {
    case NDMP4_ADDR_LOCAL:
      addr9->addr_type = NDMP9_ADDR_LOCAL;
      break;

    case NDMP4_ADDR_TCP:
      addr9->addr_type = NDMP9_ADDR_TCP;
      if (addr4->ndmp4_addr_u.tcp_addr.tcp_addr_len < 1) return -1;
      addr9->ndmp9_addr_u.tcp_addr.ip_addr =
          addr4->ndmp4_addr_u.tcp_addr.tcp_addr_val[0].ip_addr;
      addr9->ndmp9_addr_u.tcp_addr.port =
          addr4->ndmp4_addr_u.tcp_addr.tcp_addr_val[0].port;
      break;

    default:
      NDMOS_MACRO_ZEROFILL(addr9);
      addr9->addr_type = -1;
      return -1;
  }

  return 0;
}

extern int ndmp_9to4_addr(ndmp9_addr* addr9, ndmp4_addr* addr4)
{
  ndmp4_tcp_addr* tcp;

  switch (addr9->addr_type) {
    case NDMP9_ADDR_LOCAL:
      addr4->addr_type = NDMP4_ADDR_LOCAL;
      break;

    case NDMP9_ADDR_TCP:
      addr4->addr_type = NDMP4_ADDR_TCP;

      tcp = NDMOS_MACRO_NEWN(ndmp4_tcp_addr, 1);
      NDMOS_MACRO_ZEROFILL(tcp);

      tcp[0].ip_addr = addr9->ndmp9_addr_u.tcp_addr.ip_addr;
      tcp[0].port = addr9->ndmp9_addr_u.tcp_addr.port;

      addr4->ndmp4_addr_u.tcp_addr.tcp_addr_val = tcp;
      addr4->ndmp4_addr_u.tcp_addr.tcp_addr_len = 1;
      break;

    default:
      NDMOS_MACRO_ZEROFILL(addr4);
      addr4->addr_type = -1;
      return -1;
  }

  return 0;
}

int ndmp_9to4_addr_free(ndmp4_addr* addr4)
{
  if (addr4->addr_type == NDMP4_ADDR_TCP) {
    NDMOS_MACRO_FREE(addr4->ndmp4_addr_u.tcp_addr.tcp_addr_val);
  }
  return 0;
}

/*
 * CONNECT INTERFACES
 ****************************************************************
 */

struct enum_conversion ndmp_49_auth_type[] = {
    {NDMP_INVALID_GENERAL, NDMP_INVALID_GENERAL}, /* default */
    {NDMP4_AUTH_NONE, NDMP9_AUTH_NONE},
    {NDMP4_AUTH_TEXT, NDMP9_AUTH_TEXT},
    {NDMP4_AUTH_MD5, NDMP9_AUTH_MD5},
    END_ENUM_CONVERSION_TABLE};

int ndmp_4to9_auth_data(ndmp4_auth_data* auth_data4,
                        ndmp9_auth_data* auth_data9)
{
  int n_error = 0;
  int rc;
  ndmp4_auth_text* text4;
  ndmp9_auth_text* text9;
  ndmp4_auth_md5* md54;
  ndmp9_auth_md5* md59;

  switch (auth_data4->auth_type) {
    case NDMP4_AUTH_NONE:
      auth_data9->auth_type = NDMP9_AUTH_NONE;
      break;

    case NDMP4_AUTH_TEXT:
      auth_data9->auth_type = NDMP9_AUTH_TEXT;
      text4 = &auth_data4->ndmp4_auth_data_u.auth_text;
      text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
      rc = CNVT_STRDUP_TO_9(text4, text9, auth_id);
      if (rc) { return rc; /* no mem */ }
      rc = CNVT_STRDUP_TO_9(text4, text9, auth_password);
      if (rc) {
        NDMOS_API_FREE(text9->auth_id);
        text9->auth_id = 0;
        return rc; /* no mem */
      }
      break;

    case NDMP4_AUTH_MD5:
      auth_data9->auth_type = NDMP9_AUTH_MD5;
      md54 = &auth_data4->ndmp4_auth_data_u.auth_md5;
      md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
      rc = CNVT_STRDUP_TO_9(md54, md59, auth_id);
      if (rc) { return rc; /* no mem */ }
      NDMOS_API_BCOPY(md54->auth_digest, md59->auth_digest, 16);
      break;

    default:
      auth_data9->auth_type = auth_data4->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_data9->ndmp9_auth_data_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_9to4_auth_data(ndmp9_auth_data* auth_data9,
                        ndmp4_auth_data* auth_data4)
{
  int n_error = 0;
  int rc;
  ndmp9_auth_text* text9;
  ndmp4_auth_text* text4;
  ndmp9_auth_md5* md59;
  ndmp4_auth_md5* md54;

  switch (auth_data9->auth_type) {
    case NDMP9_AUTH_NONE:
      auth_data4->auth_type = NDMP4_AUTH_NONE;
      break;

    case NDMP9_AUTH_TEXT:
      auth_data4->auth_type = NDMP4_AUTH_TEXT;
      text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
      text4 = &auth_data4->ndmp4_auth_data_u.auth_text;
      rc = CNVT_STRDUP_FROM_9(text4, text9, auth_id);
      if (rc) { return rc; /* no mem */ }
      rc = CNVT_STRDUP_FROM_9(text4, text9, auth_password);
      if (rc) {
        NDMOS_API_FREE(text9->auth_id);
        text4->auth_id = 0;
        return rc; /* no mem */
      }
      break;

    case NDMP9_AUTH_MD5:
      auth_data4->auth_type = NDMP4_AUTH_MD5;
      md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
      md54 = &auth_data4->ndmp4_auth_data_u.auth_md5;
      rc = CNVT_STRDUP_FROM_9(md54, md59, auth_id);
      if (rc) { return rc; /* no mem */ }
      NDMOS_API_BCOPY(md59->auth_digest, md54->auth_digest, 16);
      break;

    default:
      auth_data4->auth_type = auth_data9->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_data4->ndmp4_auth_data_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_4to9_auth_attr(ndmp4_auth_attr* auth_attr4,
                        ndmp9_auth_attr* auth_attr9)
{
  int n_error = 0;

  switch (auth_attr4->auth_type) {
    case NDMP4_AUTH_NONE:
      auth_attr9->auth_type = NDMP9_AUTH_NONE;
      break;

    case NDMP4_AUTH_TEXT:
      auth_attr9->auth_type = NDMP9_AUTH_TEXT;
      break;

    case NDMP4_AUTH_MD5:
      auth_attr9->auth_type = NDMP9_AUTH_MD5;
      NDMOS_API_BCOPY(auth_attr4->ndmp4_auth_attr_u.challenge,
                      auth_attr9->ndmp9_auth_attr_u.challenge, 64);
      break;

    default:
      auth_attr9->auth_type = auth_attr4->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_attr9->ndmp9_auth_attr_u);
      n_error++;
      break;
  }

  return n_error;
}

int ndmp_9to4_auth_attr(ndmp9_auth_attr* auth_attr9,
                        ndmp4_auth_attr* auth_attr4)
{
  int n_error = 0;

  switch (auth_attr9->auth_type) {
    case NDMP9_AUTH_NONE:
      auth_attr4->auth_type = NDMP4_AUTH_NONE;
      break;

    case NDMP9_AUTH_TEXT:
      auth_attr4->auth_type = NDMP4_AUTH_TEXT;
      break;

    case NDMP9_AUTH_MD5:
      auth_attr4->auth_type = NDMP4_AUTH_MD5;
      NDMOS_API_BCOPY(auth_attr9->ndmp9_auth_attr_u.challenge,
                      auth_attr4->ndmp4_auth_attr_u.challenge, 64);
      break;

    default:
      auth_attr4->auth_type = auth_attr9->auth_type;
      NDMOS_MACRO_ZEROFILL(&auth_attr4->ndmp4_auth_attr_u);
      n_error++;
      break;
  }

  return n_error;
}


/*
 * ndmp_connect_open
 * just error reply
 */

int ndmp_4to9_connect_open_request(ndmp4_connect_open_request* request4,
                                   ndmp9_connect_open_request* request9)
{
  CNVT_TO_9(request4, request9, protocol_version);
  return 0;
}

int ndmp_9to4_connect_open_request(ndmp9_connect_open_request* request9,
                                   ndmp4_connect_open_request* request4)
{
  CNVT_FROM_9(request4, request9, protocol_version);
  return 0;
}


/*
 * ndmp_connect_client_auth
 * just error reply
 */


int ndmp_4to9_connect_client_auth_request(
    ndmp4_connect_client_auth_request* request4,
    ndmp9_connect_client_auth_request* request9)
{
  int rc;

  rc = ndmp_4to9_auth_data(&request4->auth_data, &request9->auth_data);

  return rc;
}

int ndmp_9to4_connect_client_auth_request(
    ndmp9_connect_client_auth_request* request9,
    ndmp4_connect_client_auth_request* request4)
{
  int rc;

  rc = ndmp_9to4_auth_data(&request9->auth_data, &request4->auth_data);

  return rc;
}


/*
 * ndmp_connect_close
 * no arg request, **NO REPLY**
 */

/*
 * ndmp_connect_server_auth
 */

/* TBD */
int ndmp_4to9_connect_server_auth_request(
    ndmp4_connect_server_auth_request* request4,
    ndmp9_connect_server_auth_request* request9)
{
  return -1;
}

int ndmp_9to4_connect_server_auth_request(
    ndmp9_connect_server_auth_request* request9,
    ndmp4_connect_server_auth_request* request4)
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

int ndmp_4to9_config_get_host_info_reply(
    ndmp4_config_get_host_info_reply* reply4,
    ndmp9_config_get_host_info_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_STRDUP_TO_9x(reply4, reply9, hostname, config_info.hostname);
  CNVT_STRDUP_TO_9x(reply4, reply9, os_type, config_info.os_type);
  CNVT_STRDUP_TO_9x(reply4, reply9, os_vers, config_info.os_vers);
  CNVT_STRDUP_TO_9x(reply4, reply9, hostid, config_info.hostid);

  return n_error;
}

int ndmp_9to4_config_get_host_info_reply(
    ndmp9_config_get_host_info_reply* reply9,
    ndmp4_config_get_host_info_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_STRDUP_FROM_9x(reply4, reply9, hostname, config_info.hostname);
  CNVT_STRDUP_FROM_9x(reply4, reply9, os_type, config_info.os_type);
  CNVT_STRDUP_FROM_9x(reply4, reply9, os_vers, config_info.os_vers);
  CNVT_STRDUP_FROM_9x(reply4, reply9, hostid, config_info.hostid);

  return 0;
}


/*
 * ndmp_config_get_connection_type
 * no args request
 */

int ndmp_4to9_config_get_connection_type_reply(
    ndmp4_config_get_connection_type_reply* reply4,
    ndmp9_config_get_connection_type_reply* reply9)
{
  int n_error = 0;
  unsigned int i;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  for (i = 0; i < reply4->addr_types.addr_types_len; i++) {
    switch (reply4->addr_types.addr_types_val[i]) {
      case NDMP4_ADDR_LOCAL:
        reply9->config_info.conntypes |= NDMP9_CONFIG_CONNTYPE_LOCAL;
        break;

      case NDMP4_ADDR_TCP:
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

int ndmp_9to4_config_get_connection_type_reply(
    ndmp9_config_get_connection_type_reply* reply9,
    ndmp4_config_get_connection_type_reply* reply4)
{
  int i = 0;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  reply4->addr_types.addr_types_val = NDMOS_MACRO_NEWN(ndmp4_addr_type, 3);
  if (!reply4->addr_types.addr_types_val) { return -1; /* no mem */ }

  i = 0;
  if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_LOCAL) {
    reply4->addr_types.addr_types_val[i++] = NDMP4_ADDR_LOCAL;
  }
  if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_TCP) {
    reply4->addr_types.addr_types_val[i++] = NDMP4_ADDR_TCP;
  }
  reply4->addr_types.addr_types_len = i;

  return 0;
}


/*
 * ndmp_config_get_auth_attr
 */

int ndmp_4to9_config_get_auth_attr_request(
    struct ndmp4_config_get_auth_attr_request* request4,
    struct ndmp9_config_get_auth_attr_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, auth_type, ndmp_49_auth_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request4, request9, auth_type);
    n_error++;
  }

  return n_error;
}

int ndmp_9to4_config_get_auth_attr_request(
    struct ndmp9_config_get_auth_attr_request* request9,
    struct ndmp4_config_get_auth_attr_request* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, auth_type, ndmp_49_auth_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, auth_type);
    n_error++;
  }

  return n_error;
}

int ndmp_4to9_config_get_auth_attr_reply(
    struct ndmp4_config_get_auth_attr_reply* reply4,
    struct ndmp9_config_get_auth_attr_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  n_error += ndmp_4to9_auth_attr(&reply4->server_attr, &reply9->server_attr);

  return n_error;
}

int ndmp_9to4_config_get_auth_attr_reply(
    struct ndmp9_config_get_auth_attr_reply* reply9,
    struct ndmp4_config_get_auth_attr_reply* reply4)
{
  int n_error = 0;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  n_error += ndmp_9to4_auth_attr(&reply9->server_attr, &reply4->server_attr);

  return n_error;
}


/*
 * ndmp_config_get_server_info
 * no args request
 */

int ndmp_4to9_config_get_server_info_reply(
    ndmp4_config_get_server_info_reply* reply4,
    ndmp9_config_get_server_info_reply* reply9)
{
  unsigned int i, n_error = 0;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_STRDUP_TO_9x(reply4, reply9, vendor_name, config_info.vendor_name);
  CNVT_STRDUP_TO_9x(reply4, reply9, product_name, config_info.product_name);
  CNVT_STRDUP_TO_9x(reply4, reply9, revision_number,
                    config_info.revision_number);

  reply9->config_info.authtypes = 0;
  for (i = 0; i < reply4->auth_type.auth_type_len; i++) {
    switch (reply4->auth_type.auth_type_val[i]) {
      case NDMP4_AUTH_NONE:
        reply9->config_info.authtypes |= NDMP9_CONFIG_AUTHTYPE_NONE;
        break;

      case NDMP4_AUTH_TEXT:
        reply9->config_info.authtypes |= NDMP9_CONFIG_AUTHTYPE_TEXT;
        break;

      case NDMP4_AUTH_MD5:
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

int ndmp_9to4_config_get_server_info_reply(
    ndmp9_config_get_server_info_reply* reply9,
    ndmp4_config_get_server_info_reply* reply4)
{
  int i = 0;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_STRDUP_FROM_9x(reply4, reply9, vendor_name, config_info.vendor_name);
  CNVT_STRDUP_FROM_9x(reply4, reply9, product_name, config_info.product_name);
  CNVT_STRDUP_FROM_9x(reply4, reply9, revision_number,
                      config_info.revision_number);

  reply4->auth_type.auth_type_val = NDMOS_MACRO_NEWN(ndmp4_auth_type, 3);
  if (!reply4->auth_type.auth_type_val) { return -1; }

  i = 0;
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_NONE) {
    reply4->auth_type.auth_type_val[i++] = NDMP4_AUTH_NONE;
  }
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_TEXT) {
    reply4->auth_type.auth_type_val[i++] = NDMP4_AUTH_TEXT;
  }
  if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_MD5) {
    reply4->auth_type.auth_type_val[i++] = NDMP4_AUTH_MD5;
  }
  reply4->auth_type.auth_type_len = i;

  return 0;
}


/*
 * ndmp_config_get_butype_info
 * no args request
 */

int ndmp_4to9_config_get_butype_info_reply(
    ndmp4_config_get_butype_info_reply* reply4,
    ndmp9_config_get_butype_info_reply* reply9)
{
  int n;
  int i;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  n = reply4->butype_info.butype_info_len;
  if (n == 0) {
    reply9->config_info.butype_info.butype_info_len = 0;
    reply9->config_info.butype_info.butype_info_val = 0;
    return 0;
  }

  reply9->config_info.butype_info.butype_info_val =
      NDMOS_MACRO_NEWN(ndmp9_butype_info, n);

  for (i = 0; i < n; i++) {
    ndmp9_butype_info* bu9;
    ndmp4_butype_info* bu4;

    bu4 = &reply4->butype_info.butype_info_val[i];
    bu9 = &reply9->config_info.butype_info.butype_info_val[i];

    NDMOS_MACRO_ZEROFILL(bu9);

    CNVT_STRDUP_TO_9(bu4, bu9, butype_name);
    ndmp_4to9_pval_vec_dup(bu4->default_env.default_env_val,
                           &bu9->default_env.default_env_val,
                           bu4->default_env.default_env_len);

    bu9->default_env.default_env_len = bu4->default_env.default_env_len;

    bu9->v4attr.valid = NDMP9_VALIDITY_VALID;
    bu9->v4attr.value = bu4->attrs;
  }

  reply9->config_info.butype_info.butype_info_len = n;

  return 0;
}

int ndmp_9to4_config_get_butype_info_reply(
    ndmp9_config_get_butype_info_reply* reply9,
    ndmp4_config_get_butype_info_reply* reply4)
{
  int n;
  int i;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  n = reply9->config_info.butype_info.butype_info_len;
  if (n == 0) {
    reply4->butype_info.butype_info_len = 0;
    reply4->butype_info.butype_info_val = 0;
    return 0;
  }

  reply4->butype_info.butype_info_val = NDMOS_MACRO_NEWN(ndmp4_butype_info, n);

  for (i = 0; i < n; i++) {
    ndmp4_butype_info* bu4;
    ndmp9_butype_info* bu9;

    bu9 = &reply9->config_info.butype_info.butype_info_val[i];
    bu4 = &reply4->butype_info.butype_info_val[i];

    NDMOS_MACRO_ZEROFILL(bu4);

    CNVT_STRDUP_FROM_9(bu4, bu9, butype_name);
    ndmp_9to4_pval_vec_dup(bu9->default_env.default_env_val,
                           &bu4->default_env.default_env_val,
                           bu9->default_env.default_env_len);

    bu4->default_env.default_env_len = bu9->default_env.default_env_len;

    bu4->attrs = bu9->v4attr.value;
  }

  reply4->butype_info.butype_info_len = n;

  return 0;
}


/*
 * ndmp_config_get_fs_info
 * no args request
 */

int ndmp_4to9_config_get_fs_info_reply(ndmp4_config_get_fs_info_reply* reply4,
                                       ndmp9_config_get_fs_info_reply* reply9)
{
  int n;
  int i;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  n = reply4->fs_info.fs_info_len;
  if (n == 0) {
    reply9->config_info.fs_info.fs_info_len = 0;
    reply9->config_info.fs_info.fs_info_val = 0;
    return 0;
  }

  reply9->config_info.fs_info.fs_info_val = NDMOS_MACRO_NEWN(ndmp9_fs_info, n);

  for (i = 0; i < n; i++) {
    ndmp4_fs_info* fs4;
    ndmp9_fs_info* fs9;

    fs4 = &reply4->fs_info.fs_info_val[i];
    fs9 = &reply9->config_info.fs_info.fs_info_val[i];

    NDMOS_MACRO_ZEROFILL(fs9);

    CNVT_STRDUP_TO_9(fs4, fs9, fs_type);
    CNVT_STRDUP_TO_9(fs4, fs9, fs_logical_device);
    CNVT_STRDUP_TO_9(fs4, fs9, fs_physical_device);
    CNVT_STRDUP_TO_9(fs4, fs9, fs_status);

    ndmp_4to9_pval_vec_dup(fs4->fs_env.fs_env_val, &fs9->fs_env.fs_env_val,
                           fs4->fs_env.fs_env_len);

    fs9->fs_env.fs_env_len = fs4->fs_env.fs_env_len;
  }

  reply9->config_info.fs_info.fs_info_len = n;

  return 0;
}

int ndmp_9to4_config_get_fs_info_reply(ndmp9_config_get_fs_info_reply* reply9,
                                       ndmp4_config_get_fs_info_reply* reply4)
{
  int n;
  int i;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  n = reply9->config_info.fs_info.fs_info_len;
  if (n == 0) {
    reply4->fs_info.fs_info_len = 0;
    reply4->fs_info.fs_info_val = 0;
    return 0;
  }

  reply4->fs_info.fs_info_val = NDMOS_MACRO_NEWN(ndmp4_fs_info, n);

  for (i = 0; i < n; i++) {
    ndmp9_fs_info* fs9;
    ndmp4_fs_info* fs4;

    fs9 = &reply9->config_info.fs_info.fs_info_val[i];
    fs4 = &reply4->fs_info.fs_info_val[i];

    NDMOS_MACRO_ZEROFILL(fs4);

    CNVT_STRDUP_FROM_9(fs4, fs9, fs_type);
    CNVT_STRDUP_FROM_9(fs4, fs9, fs_logical_device);
    CNVT_STRDUP_FROM_9(fs4, fs9, fs_physical_device);
    CNVT_STRDUP_FROM_9(fs4, fs9, fs_status);

    ndmp_9to4_pval_vec_dup(fs9->fs_env.fs_env_val, &fs4->fs_env.fs_env_val,
                           fs9->fs_env.fs_env_len);

    fs4->fs_env.fs_env_len = fs9->fs_env.fs_env_len;
  }

  reply4->fs_info.fs_info_len = n;

  return 0;
}


/*
 * ndmp_config_get_tape_info
 * no args request
 */

/*
 * ndmp_config_get_scsi_info
 * no args request
 */

int ndmp_4to9_device_info_vec_dup(ndmp4_device_info* devinf4,
                                  ndmp9_device_info** devinf9_p,
                                  int n_devinf)
{
  ndmp9_device_info* devinf9;
  int i;
  unsigned int j;

  devinf9 = *devinf9_p = NDMOS_MACRO_NEWN(ndmp9_device_info, n_devinf);
  if (!devinf9) { return -1; /* no mem */ }

  for (i = 0; i < n_devinf; i++) {
    ndmp4_device_info* di4 = &devinf4[i];
    ndmp9_device_info* di9 = &devinf9[i];

    NDMOS_MACRO_ZEROFILL(di9);

    CNVT_STRDUP_TO_9(di4, di9, model);

    di9->caplist.caplist_val =
        NDMOS_MACRO_NEWN(ndmp9_device_capability, di4->caplist.caplist_len);

    if (!di9->caplist.caplist_val) { return -1; }

    for (j = 0; j < di4->caplist.caplist_len; j++) {
      ndmp4_device_capability* cap4;
      ndmp9_device_capability* cap9;

      cap4 = &di4->caplist.caplist_val[j];
      cap9 = &di9->caplist.caplist_val[j];

      NDMOS_MACRO_ZEROFILL(cap9);

      cap9->v4attr.valid = NDMP9_VALIDITY_VALID;
      cap9->v4attr.value = cap4->attr;

      CNVT_STRDUP_TO_9(cap4, cap9, device);

      ndmp_4to9_pval_vec_dup(cap4->capability.capability_val,
                             &cap9->capability.capability_val,
                             cap4->capability.capability_len);

      cap9->capability.capability_len = cap4->capability.capability_len;
    }
    di9->caplist.caplist_len = j;
  }

  return 0;
}

int ndmp_9to4_device_info_vec_dup(ndmp9_device_info* devinf9,
                                  ndmp4_device_info** devinf4_p,
                                  int n_devinf)
{
  ndmp4_device_info* devinf4;
  int i;
  unsigned int j;

  devinf4 = *devinf4_p = NDMOS_MACRO_NEWN(ndmp4_device_info, n_devinf);
  if (!devinf4) { return -1; /* no mem */ }

  for (i = 0; i < n_devinf; i++) {
    ndmp9_device_info* di9 = &devinf9[i];
    ndmp4_device_info* di4 = &devinf4[i];

    NDMOS_MACRO_ZEROFILL(di4);

    CNVT_STRDUP_FROM_9(di4, di9, model);

    di4->caplist.caplist_val =
        NDMOS_MACRO_NEWN(ndmp4_device_capability, di9->caplist.caplist_len);

    if (!di4->caplist.caplist_val) { return -1; }

    for (j = 0; j < di9->caplist.caplist_len; j++) {
      ndmp9_device_capability* cap9;
      ndmp4_device_capability* cap4;

      cap9 = &di9->caplist.caplist_val[j];
      cap4 = &di4->caplist.caplist_val[j];

      NDMOS_MACRO_ZEROFILL(cap4);

      CNVT_STRDUP_FROM_9(cap4, cap9, device);

      ndmp_9to4_pval_vec_dup(cap9->capability.capability_val,
                             &cap4->capability.capability_val,
                             cap9->capability.capability_len);

      cap4->capability.capability_len = cap9->capability.capability_len;
    }
    di4->caplist.caplist_len = j;
  }

  return 0;
}

int ndmp_4to9_config_get_tape_info_reply(
    ndmp4_config_get_tape_info_reply* reply4,
    ndmp9_config_get_tape_info_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  ndmp_4to9_device_info_vec_dup(reply4->tape_info.tape_info_val,
                                &reply9->config_info.tape_info.tape_info_val,
                                reply4->tape_info.tape_info_len);

  reply9->config_info.tape_info.tape_info_len = reply4->tape_info.tape_info_len;

  return 0;
}

int ndmp_9to4_config_get_tape_info_reply(
    ndmp9_config_get_tape_info_reply* reply9,
    ndmp4_config_get_tape_info_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  ndmp_9to4_device_info_vec_dup(reply9->config_info.tape_info.tape_info_val,
                                &reply4->tape_info.tape_info_val,
                                reply9->config_info.tape_info.tape_info_len);

  reply4->tape_info.tape_info_len = reply9->config_info.tape_info.tape_info_len;

  return 0;
}

int ndmp_4to9_config_get_scsi_info_reply(
    ndmp4_config_get_scsi_info_reply* reply4,
    ndmp9_config_get_scsi_info_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  ndmp_4to9_device_info_vec_dup(reply4->scsi_info.scsi_info_val,
                                &reply9->config_info.scsi_info.scsi_info_val,
                                reply4->scsi_info.scsi_info_len);

  reply9->config_info.scsi_info.scsi_info_len = reply4->scsi_info.scsi_info_len;

  return 0;
}

int ndmp_9to4_config_get_scsi_info_reply(
    ndmp9_config_get_scsi_info_reply* reply9,
    ndmp4_config_get_scsi_info_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  ndmp_9to4_device_info_vec_dup(reply9->config_info.scsi_info.scsi_info_val,
                                &reply4->scsi_info.scsi_info_val,
                                reply9->config_info.scsi_info.scsi_info_len);

  reply4->scsi_info.scsi_info_len = reply9->config_info.scsi_info.scsi_info_len;

  return 0;
}


/*
 * SCSI INTERFACES
 ****************************************************************
 */

/*
 * ndmp_scsi_open
 * just error reply
 */
int ndmp_4to9_scsi_open_request(ndmp4_scsi_open_request* request4,
                                ndmp9_scsi_open_request* request9)
{
  request9->device = NDMOS_API_STRDUP(request4->device);
  if (!request9->device) { return -1; /* no memory */ }
  return 0;
}

int ndmp_9to4_scsi_open_request(ndmp9_scsi_open_request* request9,
                                ndmp4_scsi_open_request* request4)
{
  request4->device = NDMOS_API_STRDUP(request9->device);
  if (!request4->device) { return -1; /* no memory */ }
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

int ndmp_4to9_scsi_get_state_reply(ndmp4_scsi_get_state_reply* reply4,
                                   ndmp9_scsi_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_TO_9(reply4, reply9, target_controller);
  CNVT_TO_9(reply4, reply9, target_id);
  CNVT_TO_9(reply4, reply9, target_lun);

  return 0;
}

int ndmp_9to4_scsi_get_state_reply(ndmp9_scsi_get_state_reply* reply9,
                                   ndmp4_scsi_get_state_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_FROM_9(reply4, reply9, target_controller);
  CNVT_FROM_9(reply4, reply9, target_id);
  CNVT_FROM_9(reply4, reply9, target_lun);

  return 0;
}

/*
 * ndmp_scsi_set_target -- deprecated
 * just error reply
 */


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

int ndmp_4to9_execute_cdb_request(ndmp4_execute_cdb_request* request4,
                                  ndmp9_execute_cdb_request* request9)
{
  int n_error = 0;
  uint32_t len;
  char* p;

  switch (request4->flags) {
    case 0:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_NONE;
      break;

    case NDMP4_SCSI_DATA_IN:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
      break;

    case NDMP4_SCSI_DATA_OUT:
      request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
      break;

    default:
      /* deemed insolvable */
      n_error++;
      return -1;
  }

  CNVT_TO_9(request4, request9, timeout);
  CNVT_TO_9(request4, request9, datain_len);

  len = request4->dataout.dataout_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(request4->dataout.dataout_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request9->dataout.dataout_len = len;
  request9->dataout.dataout_val = p;

  len = request4->cdb.cdb_len;
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
    NDMOS_API_BCOPY(request4->cdb.cdb_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request9->cdb.cdb_len = len;
  request9->cdb.cdb_val = p;

  return 0;
}

int ndmp_9to4_execute_cdb_request(ndmp9_execute_cdb_request* request9,
                                  ndmp4_execute_cdb_request* request4)
{
  int n_error = 0;
  uint32_t len;
  char* p;

  switch (request9->data_dir) {
    case NDMP9_SCSI_DATA_DIR_NONE:
      request4->flags = 0;
      break;

    case NDMP9_SCSI_DATA_DIR_IN:
      request4->flags = NDMP4_SCSI_DATA_IN;
      break;

    case NDMP9_SCSI_DATA_DIR_OUT:
      request4->flags = NDMP4_SCSI_DATA_OUT;
      break;

    default:
      /* deemed insolvable */
      n_error++;
      return -1;
  }

  CNVT_FROM_9(request4, request9, timeout);
  CNVT_FROM_9(request4, request9, datain_len);

  len = request9->dataout.dataout_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(request9->dataout.dataout_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request4->dataout.dataout_len = len;
  request4->dataout.dataout_val = p;

  len = request9->cdb.cdb_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (request4->dataout.dataout_val) {
        NDMOS_API_FREE(request4->dataout.dataout_val);
        request4->dataout.dataout_len = 0;
        request4->dataout.dataout_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(request9->cdb.cdb_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  request4->cdb.cdb_len = len;
  request4->cdb.cdb_val = p;

  return 0;
}

int ndmp_4to9_execute_cdb_reply(ndmp4_execute_cdb_reply* reply4,
                                ndmp9_execute_cdb_reply* reply9)
{
  uint32_t len;
  char* p;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_TO_9(reply4, reply9, status);
  CNVT_TO_9(reply4, reply9, dataout_len);

  len = reply4->datain.datain_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply4->datain.datain_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply9->datain.datain_len = len;
  reply9->datain.datain_val = p;

  len = reply4->ext_sense.ext_sense_len;
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
    NDMOS_API_BCOPY(reply4->ext_sense.ext_sense_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply9->ext_sense.ext_sense_len = len;
  reply9->ext_sense.ext_sense_val = p;

  return 0;
}

int ndmp_9to4_execute_cdb_reply(ndmp9_execute_cdb_reply* reply9,
                                ndmp4_execute_cdb_reply* reply4)
{
  uint32_t len;
  char* p;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_FROM_9(reply4, reply9, status);
  CNVT_FROM_9(reply4, reply9, dataout_len);

  len = reply9->datain.datain_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply9->datain.datain_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply4->datain.datain_len = len;
  reply4->datain.datain_val = p;

  len = reply9->ext_sense.ext_sense_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) {
      if (reply4->datain.datain_val) {
        NDMOS_API_FREE(reply9->datain.datain_val);
        reply4->datain.datain_len = 0;
        reply4->datain.datain_val = 0;
      }
      return -1;
    }
    NDMOS_API_BCOPY(reply9->ext_sense.ext_sense_val, p, len);
  } else {
    len = 0;
    p = 0;
  }
  reply4->ext_sense.ext_sense_len = len;
  reply4->ext_sense.ext_sense_val = p;

  return 0;
}


/*
 * TAPE INTERFACES
 ****************************************************************
 */


/*
 * ndmp_tape_open_request
 */

struct enum_conversion ndmp_49_tape_open_mode[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_TAPE_READ_MODE, NDMP9_TAPE_READ_MODE},
    {NDMP4_TAPE_RDWR_MODE, NDMP9_TAPE_RDWR_MODE},
    END_ENUM_CONVERSION_TABLE};


int ndmp_4to9_tape_open_request(ndmp4_tape_open_request* request4,
                                ndmp9_tape_open_request* request9)
{
  int n_error = 0;
  int rc;

  /*
   * Mode codes are compatible between versions.
   * We let untranslated values through to
   * facilitate testing illegal values.
   */
  rc = convert_enum_to_9(ndmp_49_tape_open_mode, request4->mode);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    request9->mode = request4->mode;
  } else {
    request9->mode = rc;
  }

  request9->device = NDMOS_API_STRDUP(request4->device);
  if (!request9->device) { return -1; /* no memory */ }

  return n_error;
}

int ndmp_4to9_tape_open_free_request(ndmp9_tape_open_request* request9)
{
  NDMOS_API_FREE(request9->device);
  request9->device = NULL;
  return 0;
}

int ndmp_9to4_tape_open_request(ndmp9_tape_open_request* request9,
                                ndmp4_tape_open_request* request4)
{
  int n_error = 0;
  int rc;

  rc = convert_enum_from_9(ndmp_49_tape_open_mode, request9->mode);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    request4->mode = request9->mode;
  } else {
    request4->mode = rc;
  }

  request4->device = NDMOS_API_STRDUP(request9->device);
  if (!request4->device) { return -1; /* no memory */ }

  return n_error;
}


/*
 * ndmp_tape_get_state_reply
 ****************************************************************
 */

extern int ndmp_4to9_tape_get_state_reply(ndmp4_tape_get_state_reply* reply4,
                                          ndmp9_tape_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_TO_9(reply4, reply9, flags);
  CNVT_VUL_TO_9(reply4, reply9, file_num);
  CNVT_VUL_TO_9(reply4, reply9, soft_errors);
  CNVT_VUL_TO_9(reply4, reply9, block_size);
  CNVT_VUL_TO_9(reply4, reply9, blockno);
  CNVT_VUQ_TO_9(reply4, reply9, total_space);
  CNVT_VUQ_TO_9(reply4, reply9, space_remain);

  if (reply4->unsupported & NDMP4_TAPE_STATE_FILE_NUM_UNS)
    CNVT_IUL_TO_9(reply9, file_num);

  if (reply4->unsupported & NDMP4_TAPE_STATE_SOFT_ERRORS_UNS)
    CNVT_IUL_TO_9(reply9, soft_errors);

  if (reply4->unsupported & NDMP4_TAPE_STATE_BLOCK_SIZE_UNS)
    CNVT_IUL_TO_9(reply9, block_size);

  if (reply4->unsupported & NDMP4_TAPE_STATE_BLOCKNO_UNS)
    CNVT_IUL_TO_9(reply9, blockno);

  if (reply4->unsupported & NDMP4_TAPE_STATE_TOTAL_SPACE_UNS)
    CNVT_IUQ_TO_9(reply9, total_space);

  if (reply4->unsupported & NDMP4_TAPE_STATE_SPACE_REMAIN_UNS)
    CNVT_IUQ_TO_9(reply9, space_remain);

  return 0;
}

extern int ndmp_9to4_tape_get_state_reply(ndmp9_tape_get_state_reply* reply9,
                                          ndmp4_tape_get_state_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_FROM_9(reply4, reply9, flags);
  CNVT_VUL_FROM_9(reply4, reply9, file_num);
  CNVT_VUL_FROM_9(reply4, reply9, soft_errors);
  CNVT_VUL_FROM_9(reply4, reply9, block_size);
  CNVT_VUL_FROM_9(reply4, reply9, blockno);
  CNVT_VUQ_FROM_9(reply4, reply9, total_space);
  CNVT_VUQ_FROM_9(reply4, reply9, space_remain);

  reply4->unsupported = 0;

  if (!reply9->file_num.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_FILE_NUM_UNS;

  if (!reply9->soft_errors.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_SOFT_ERRORS_UNS;

  if (!reply9->block_size.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_BLOCK_SIZE_UNS;

  if (!reply9->blockno.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_BLOCKNO_UNS;

  if (!reply9->total_space.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_TOTAL_SPACE_UNS;

  if (!reply9->space_remain.valid)
    reply4->unsupported |= NDMP4_TAPE_STATE_SPACE_REMAIN_UNS;

  return 0;
}


/*
 * ndmp_tape_mtio_request
 */

struct enum_conversion ndmp_49_tape_mtio_op[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_MTIO_FSF, NDMP9_MTIO_FSF},
    {NDMP4_MTIO_BSF, NDMP9_MTIO_BSF},
    {NDMP4_MTIO_FSR, NDMP9_MTIO_FSR},
    {NDMP4_MTIO_BSR, NDMP9_MTIO_BSR},
    {NDMP4_MTIO_REW, NDMP9_MTIO_REW},
    {NDMP4_MTIO_EOF, NDMP9_MTIO_EOF},
    {NDMP4_MTIO_OFF, NDMP9_MTIO_OFF},
    END_ENUM_CONVERSION_TABLE};


int ndmp_4to9_tape_mtio_request(ndmp4_tape_mtio_request* request4,
                                ndmp9_tape_mtio_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, tape_op, ndmp_49_tape_mtio_op);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    CNVT_TO_9(request4, request9, tape_op);
  }

  CNVT_TO_9(request4, request9, count);

  return n_error;
}

int ndmp_9to4_tape_mtio_request(ndmp9_tape_mtio_request* request9,
                                ndmp4_tape_mtio_request* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, tape_op, ndmp_49_tape_mtio_op);
  if (rc == NDMP_INVALID_GENERAL) {
    n_error++;
    CNVT_FROM_9(request4, request9, tape_op);
  }

  CNVT_FROM_9(request4, request9, count);

  return n_error;
}

int ndmp_4to9_tape_mtio_reply(ndmp4_tape_mtio_reply* reply4,
                              ndmp9_tape_mtio_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_TO_9(reply4, reply9, resid_count);
  return 0;
}

int ndmp_9to4_tape_mtio_reply(ndmp9_tape_mtio_reply* reply9,
                              ndmp4_tape_mtio_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_FROM_9(reply4, reply9, resid_count);
  return 0;
}


/*
 * ndmp_tape_write
 */

int ndmp_4to9_tape_write_request(ndmp4_tape_write_request* request4,
                                 ndmp9_tape_write_request* request9)
{
  uint32_t len;
  char* p;

  len = request4->data_out.data_out_len;

  p = NDMOS_API_MALLOC(len);
  if (!p) { return -1; }

  NDMOS_API_BCOPY(request4->data_out.data_out_val, p, len);

  request9->data_out.data_out_val = p;
  request9->data_out.data_out_len = len;

  return 0;
}

int ndmp_9to4_tape_write_request(ndmp9_tape_write_request* request9,
                                 ndmp4_tape_write_request* request4)
{
  uint32_t len;
  char* p;

  len = request9->data_out.data_out_len;

  p = NDMOS_API_MALLOC(len);
  if (!p) { return -1; }

  NDMOS_API_BCOPY(request9->data_out.data_out_val, p, len);

  request4->data_out.data_out_val = p;
  request4->data_out.data_out_len = len;

  return 0;
}

int ndmp_4to9_tape_write_reply(ndmp4_tape_write_reply* reply4,
                               ndmp9_tape_write_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_TO_9(reply4, reply9, count);
  return 0;
}

int ndmp_9to4_tape_write_reply(ndmp9_tape_write_reply* reply9,
                               ndmp4_tape_write_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_FROM_9(reply4, reply9, count);
  return 0;
}


/*
 * ndmp_tape_read
 */

int ndmp_4to9_tape_read_request(ndmp4_tape_read_request* request4,
                                ndmp9_tape_read_request* request9)
{
  CNVT_TO_9(request4, request9, count);
  return 0;
}

int ndmp_9to4_tape_read_request(ndmp9_tape_read_request* request9,
                                ndmp4_tape_read_request* request4)
{
  CNVT_FROM_9(request4, request9, count);
  return 0;
}

int ndmp_4to9_tape_read_reply(ndmp4_tape_read_reply* reply4,
                              ndmp9_tape_read_reply* reply9)
{
  uint32_t len;
  char* p;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  len = reply4->data_in.data_in_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply4->data_in.data_in_val, p, len);
  } else {
    p = 0;
    len = 0;
  }

  reply9->data_in.data_in_len = len;
  reply9->data_in.data_in_val = p;

  return 0;
}

int ndmp_9to4_tape_read_reply(ndmp9_tape_read_reply* reply9,
                              ndmp4_tape_read_reply* reply4)
{
  uint32_t len;
  char* p;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  len = reply9->data_in.data_in_len;
  if (len > 0) {
    p = NDMOS_API_MALLOC(len);
    if (!p) { return -1; }
    NDMOS_API_BCOPY(reply9->data_in.data_in_val, p, len);
  } else {
    p = 0;
    len = 0;
  }

  reply4->data_in.data_in_len = len;
  reply4->data_in.data_in_val = p;

  return 0;
}


/*
 * MOVER INTERFACES
 ****************************************************************
 */

/*
 * ndmp_mover_get_state
 * no args request
 */


struct enum_conversion ndmp_49_mover_mode[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_MOVER_MODE_READ, NDMP9_MOVER_MODE_READ},
    {NDMP4_MOVER_MODE_WRITE, NDMP9_MOVER_MODE_WRITE},

    END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_49_mover_state[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_MOVER_STATE_IDLE, NDMP9_MOVER_STATE_IDLE},
    {NDMP4_MOVER_STATE_LISTEN, NDMP9_MOVER_STATE_LISTEN},
    {NDMP4_MOVER_STATE_ACTIVE, NDMP9_MOVER_STATE_ACTIVE},
    {NDMP4_MOVER_STATE_PAUSED, NDMP9_MOVER_STATE_PAUSED},
    {NDMP4_MOVER_STATE_HALTED, NDMP9_MOVER_STATE_HALTED},

    /* alias */
    {NDMP4_MOVER_STATE_ACTIVE, NDMP9_MOVER_STATE_STANDBY},

    END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_49_mover_pause_reason[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_MOVER_PAUSE_NA, NDMP9_MOVER_PAUSE_NA},
    {NDMP4_MOVER_PAUSE_EOM, NDMP9_MOVER_PAUSE_EOM},
    {NDMP4_MOVER_PAUSE_EOF, NDMP9_MOVER_PAUSE_EOF},
    {NDMP4_MOVER_PAUSE_SEEK, NDMP9_MOVER_PAUSE_SEEK},
    /* no NDMP4_MOVER_PAUSE_MEDIA_ERROR, so use EOF */
    {NDMP4_MOVER_PAUSE_EOF, NDMP9_MOVER_PAUSE_MEDIA_ERROR},
    {NDMP4_MOVER_PAUSE_EOW, NDMP9_MOVER_PAUSE_EOW},
    END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_49_mover_halt_reason[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_MOVER_HALT_NA, NDMP9_MOVER_HALT_NA},
    {NDMP4_MOVER_HALT_CONNECT_CLOSED, NDMP9_MOVER_HALT_CONNECT_CLOSED},
    {NDMP4_MOVER_HALT_ABORTED, NDMP9_MOVER_HALT_ABORTED},
    {NDMP4_MOVER_HALT_INTERNAL_ERROR, NDMP9_MOVER_HALT_INTERNAL_ERROR},
    {NDMP4_MOVER_HALT_CONNECT_ERROR, NDMP9_MOVER_HALT_CONNECT_ERROR},
    {NDMP4_MOVER_HALT_MEDIA_ERROR, NDMP9_MOVER_HALT_MEDIA_ERROR},
    END_ENUM_CONVERSION_TABLE};


extern int ndmp_4to9_mover_get_state_reply(ndmp4_mover_get_state_reply* reply4,
                                           ndmp9_mover_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_E_TO_9(reply4, reply9, state, ndmp_49_mover_state);
  CNVT_E_TO_9(reply4, reply9, pause_reason, ndmp_49_mover_pause_reason);
  CNVT_E_TO_9(reply4, reply9, halt_reason, ndmp_49_mover_halt_reason);

  CNVT_TO_9(reply4, reply9, record_size);
  CNVT_TO_9(reply4, reply9, record_num);
  CNVT_TO_9(reply4, reply9, bytes_moved);
  CNVT_TO_9(reply4, reply9, seek_position);
  CNVT_TO_9(reply4, reply9, bytes_left_to_read);
  CNVT_TO_9(reply4, reply9, window_offset);
  CNVT_TO_9(reply4, reply9, window_length);

  ndmp_4to9_addr(&reply4->data_connection_addr, &reply9->data_connection_addr);

  return 0;
}

extern int ndmp_9to4_mover_get_state_reply(ndmp9_mover_get_state_reply* reply9,
                                           ndmp4_mover_get_state_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_E_FROM_9(reply4, reply9, state, ndmp_49_mover_state);
  CNVT_E_FROM_9(reply4, reply9, pause_reason, ndmp_49_mover_pause_reason);
  CNVT_E_FROM_9(reply4, reply9, halt_reason, ndmp_49_mover_halt_reason);

  CNVT_FROM_9(reply4, reply9, record_size);
  CNVT_FROM_9(reply4, reply9, record_num);
  CNVT_FROM_9(reply4, reply9, bytes_moved);
  CNVT_FROM_9(reply4, reply9, seek_position);
  CNVT_FROM_9(reply4, reply9, bytes_left_to_read);
  CNVT_FROM_9(reply4, reply9, window_offset);
  CNVT_FROM_9(reply4, reply9, window_length);

  ndmp_9to4_addr(&reply9->data_connection_addr, &reply4->data_connection_addr);

  return 0;
}

int ndmp_9to4_mover_get_state_free_reply(ndmp4_mover_get_state_reply* reply4)
{
  //    ndmp_9to4_addr_free(&reply4->data_connection_addr);
  return 0;
}

/*
 * ndmp_mover_listen
 */

int ndmp_4to9_mover_listen_request(ndmp4_mover_listen_request* request4,
                                   ndmp9_mover_listen_request* request9)
{
  int rc;

  rc = CNVT_E_TO_9(request4, request9, mode, ndmp_49_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request4, request9, mode); }
  rc = CNVT_E_TO_9(request4, request9, addr_type, ndmp_49_addr_type);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request4, request9, addr_type); }

  return 0;
}

int ndmp_9to4_mover_listen_request(ndmp9_mover_listen_request* request9,
                                   ndmp4_mover_listen_request* request4)
{
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, mode, ndmp_49_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_FROM_9(request4, request9, mode); }
  rc = CNVT_E_FROM_9(request4, request9, addr_type, ndmp_49_addr_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, addr_type);
  }

  return 0;
}

int ndmp_4to9_mover_listen_reply(ndmp4_mover_listen_reply* reply4,
                                 ndmp9_mover_listen_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  n_error +=
      ndmp_4to9_addr(&reply4->connect_addr, &reply9->data_connection_addr);

  return n_error;
}

int ndmp_9to4_mover_listen_reply(ndmp9_mover_listen_reply* reply9,
                                 ndmp4_mover_listen_reply* reply4)
{
  int n_error = 0;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  n_error +=
      ndmp_9to4_addr(&reply9->data_connection_addr, &reply4->connect_addr);

  return n_error;
}

/*
 * ndmp_mover_connect
 * just error reply
 */

int ndmp_4to9_mover_connect_request(ndmp4_mover_connect_request* request4,
                                    ndmp9_mover_connect_request* request9)
{
  int rc;

  rc = CNVT_E_TO_9(request4, request9, mode, ndmp_49_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request4, request9, mode); }
  return ndmp_4to9_addr(&request4->addr, &request9->addr);
}

int ndmp_9to4_mover_connect_request(ndmp9_mover_connect_request* request9,
                                    ndmp4_mover_connect_request* request4)
{
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, mode, ndmp_49_mover_mode);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_FROM_9(request4, request9, mode); }
  return ndmp_9to4_addr(&request9->addr, &request4->addr);
}


/*
 * ndmp_mover_set_record_size
 * just error reply
 */

int ndmp_4to9_mover_set_record_size_request(
    ndmp4_mover_set_record_size_request* request4,
    ndmp9_mover_set_record_size_request* request9)
{
  CNVT_TO_9x(request4, request9, len, record_size);
  return 0;
}

int ndmp_9to4_mover_set_record_size_request(
    ndmp9_mover_set_record_size_request* request9,
    ndmp4_mover_set_record_size_request* request4)
{
  CNVT_FROM_9x(request4, request9, len, record_size);
  return 0;
}


/*
 * ndmp_mover_set_window
 * just error reply
 */

int ndmp_4to9_mover_set_window_request(ndmp4_mover_set_window_request* request4,
                                       ndmp9_mover_set_window_request* request9)
{
  CNVT_TO_9(request4, request9, offset);
  CNVT_TO_9(request4, request9, length);
  return 0;
}

int ndmp_9to4_mover_set_window_request(ndmp9_mover_set_window_request* request9,
                                       ndmp4_mover_set_window_request* request4)
{
  CNVT_FROM_9(request4, request9, offset);
  CNVT_FROM_9(request4, request9, length);
  return 0;
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
 * ndmp_mover_read
 * just error reply
 */

int ndmp_4to9_mover_read_request(ndmp4_mover_read_request* request4,
                                 ndmp9_mover_read_request* request9)
{
  CNVT_TO_9(request4, request9, offset);
  CNVT_TO_9(request4, request9, length);
  return 0;
}

int ndmp_9to4_mover_read_request(ndmp9_mover_read_request* request9,
                                 ndmp4_mover_read_request* request4)
{
  CNVT_FROM_9(request4, request9, offset);
  CNVT_FROM_9(request4, request9, length);
  return 0;
}

/*
 * ndmp_mover_close
 * no args request, just error reply
 */


/*
 * DATA INTERFACES
 */


/*
 * ndmp_name
 ****************************************************************
 */

int ndmp_4to9_name(ndmp4_name* name4, ndmp9_name* name9)
{
  name9->original_path = NDMOS_API_STRDUP(name4->original_path);
  name9->destination_path = NDMOS_API_STRDUP(name4->destination_path);
  name9->name = NDMOS_API_STRDUP(name4->name);
  name9->other_name = NDMOS_API_STRDUP(name4->other_name);

  name9->node = name4->node;
  if (name4->fh_info != NDMP_INVALID_U_QUAD) {
    name9->fh_info.valid = NDMP9_VALIDITY_VALID;
    name9->fh_info.value = name4->fh_info;
  } else {
    name9->fh_info.valid = NDMP9_VALIDITY_INVALID;
    name9->fh_info.value = NDMP_INVALID_U_QUAD;
  }

  return 0;
}

int ndmp_9to4_name(ndmp9_name* name9, ndmp4_name* name4)
{
  name4->original_path = NDMOS_API_STRDUP(name9->original_path);
  name4->destination_path = NDMOS_API_STRDUP(name9->destination_path);
  name4->name = NDMOS_API_STRDUP(name9->name);
  name4->other_name = NDMOS_API_STRDUP(name9->other_name);

  name4->node = name9->node;
  if (name9->fh_info.valid == NDMP9_VALIDITY_VALID) {
    name4->fh_info = name9->fh_info.value;
  } else {
    name4->fh_info = NDMP_INVALID_U_QUAD;
  }

  return 0;
}

int ndmp_4to9_name_vec(ndmp4_name* name4, ndmp9_name* name9, unsigned n_name)
{
  unsigned int i;

  for (i = 0; i < n_name; i++) ndmp_4to9_name(&name4[i], &name9[i]);

  return 0;
}

int ndmp_9to4_name_vec(ndmp9_name* name9, ndmp4_name* name4, unsigned n_name)
{
  unsigned int i;

  for (i = 0; i < n_name; i++) ndmp_9to4_name(&name9[i], &name4[i]);

  return 0;
}

int ndmp_4to9_name_vec_dup(ndmp4_name* name4,
                           ndmp9_name** name9_p,
                           unsigned n_name)
{
  *name9_p = NDMOS_MACRO_NEWN(ndmp9_name, n_name);
  if (!*name9_p) return -1;

  return ndmp_4to9_name_vec(name4, *name9_p, n_name);
}

int ndmp_9to4_name_vec_dup(ndmp9_name* name9,
                           ndmp4_name** name4_p,
                           unsigned n_name)
{
  *name4_p = NDMOS_MACRO_NEWN(ndmp4_name, n_name);
  if (!*name4_p) return -1;

  return ndmp_9to4_name_vec(name9, *name4_p, n_name);
}


/*
 * ndmp_data_get_state_reply
 ****************************************************************
 */

struct enum_conversion ndmp_49_data_operation[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_DATA_OP_NOACTION, NDMP9_DATA_OP_NOACTION},
    {NDMP4_DATA_OP_BACKUP, NDMP9_DATA_OP_BACKUP},
    {NDMP4_DATA_OP_RECOVER, NDMP9_DATA_OP_RECOVER},
    {NDMP4_DATA_OP_RECOVER_FILEHIST, NDMP9_DATA_OP_RECOVER_FILEHIST},
    END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_49_data_state[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_DATA_STATE_IDLE, NDMP9_DATA_STATE_IDLE},
    {NDMP4_DATA_STATE_ACTIVE, NDMP9_DATA_STATE_ACTIVE},
    {NDMP4_DATA_STATE_HALTED, NDMP9_DATA_STATE_HALTED},
    {NDMP4_DATA_STATE_CONNECTED, NDMP9_DATA_STATE_CONNECTED},
    {NDMP4_DATA_STATE_LISTEN, NDMP9_DATA_STATE_LISTEN},

    END_ENUM_CONVERSION_TABLE};

struct enum_conversion ndmp_49_data_halt_reason[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_DATA_HALT_NA, NDMP9_DATA_HALT_NA},
    {NDMP4_DATA_HALT_SUCCESSFUL, NDMP9_DATA_HALT_SUCCESSFUL},
    {NDMP4_DATA_HALT_ABORTED, NDMP9_DATA_HALT_ABORTED},
    {NDMP4_DATA_HALT_INTERNAL_ERROR, NDMP9_DATA_HALT_INTERNAL_ERROR},
    {NDMP4_DATA_HALT_CONNECT_ERROR, NDMP9_DATA_HALT_CONNECT_ERROR},
    END_ENUM_CONVERSION_TABLE};

extern int ndmp_4to9_data_get_state_reply(ndmp4_data_get_state_reply* reply4,
                                          ndmp9_data_get_state_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);
  CNVT_E_TO_9(reply4, reply9, operation, ndmp_49_data_operation);
  CNVT_E_TO_9(reply4, reply9, state, ndmp_49_data_state);
  CNVT_E_TO_9(reply4, reply9, halt_reason, ndmp_49_data_halt_reason);

  CNVT_TO_9(reply4, reply9, bytes_processed);

  CNVT_VUQ_TO_9(reply4, reply9, est_bytes_remain);
  CNVT_VUL_TO_9(reply4, reply9, est_time_remain);

  ndmp_4to9_addr(&reply4->data_connection_addr, &reply9->data_connection_addr);

  CNVT_TO_9(reply4, reply9, read_offset);
  CNVT_TO_9(reply4, reply9, read_length);

  return 0;
}

extern int ndmp_9to4_data_get_state_reply(ndmp9_data_get_state_reply* reply9,
                                          ndmp4_data_get_state_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);
  CNVT_E_FROM_9(reply4, reply9, operation, ndmp_49_data_operation);
  CNVT_E_FROM_9(reply4, reply9, state, ndmp_49_data_state);
  CNVT_E_FROM_9(reply4, reply9, halt_reason, ndmp_49_data_halt_reason);

  CNVT_FROM_9(reply4, reply9, bytes_processed);

  CNVT_VUQ_FROM_9(reply4, reply9, est_bytes_remain);
  CNVT_VUL_FROM_9(reply4, reply9, est_time_remain);

  ndmp_9to4_addr(&reply9->data_connection_addr, &reply4->data_connection_addr);

  CNVT_FROM_9(reply4, reply9, read_offset);
  CNVT_FROM_9(reply4, reply9, read_length);

  return 0;
}


/*
 * ndmp_data_start_backup
 * just error reply
 */

int ndmp_4to9_data_start_backup_request(
    ndmp4_data_start_backup_request* request4,
    ndmp9_data_start_backup_request* request9)
{
  int n_error = 0;

  CNVT_STRDUP_TO_9x(request4, request9, butype_name, bu_type);

  ndmp_4to9_pval_vec_dup(request4->env.env_val, &request9->env.env_val,
                         request4->env.env_len);

  request9->env.env_len = request4->env.env_len;

  request9->addr.addr_type = NDMP9_ADDR_AS_CONNECTED;

  return n_error;
}

int ndmp_9to4_data_start_backup_request(
    ndmp9_data_start_backup_request* request9,
    ndmp4_data_start_backup_request* request4)
{
  int n_error = 0;

  CNVT_STRDUP_FROM_9x(request4, request9, butype_name, bu_type);

  ndmp_9to4_pval_vec_dup(request9->env.env_val, &request4->env.env_val,
                         request9->env.env_len);

  request4->env.env_len = request9->env.env_len;

  return n_error;
}


/*
 * ndmp_data_start_recover
 * ndmp_data_start_recover_filehist
 * just error reply
 */

int ndmp_4to9_data_start_recover_request(
    ndmp4_data_start_recover_request* request4,
    ndmp9_data_start_recover_request* request9)
{
  int n_error = 0;

  CNVT_STRDUP_TO_9x(request4, request9, butype_name, bu_type);

  ndmp_4to9_pval_vec_dup(request4->env.env_val, &request9->env.env_val,
                         request4->env.env_len);

  request9->env.env_len = request4->env.env_len;

  ndmp_4to9_name_vec_dup(request4->nlist.nlist_val, &request9->nlist.nlist_val,
                         request4->nlist.nlist_len);

  request9->nlist.nlist_len = request4->nlist.nlist_len;

  request9->addr.addr_type = NDMP9_ADDR_AS_CONNECTED;

  return n_error;
}

int ndmp_9to4_data_start_recover_request(
    ndmp9_data_start_recover_request* request9,
    ndmp4_data_start_recover_request* request4)
{
  int n_error = 0;

  CNVT_STRDUP_FROM_9x(request4, request9, butype_name, bu_type);

  ndmp_9to4_pval_vec_dup(request9->env.env_val, &request4->env.env_val,
                         request9->env.env_len);

  request4->env.env_len = request9->env.env_len;

  ndmp_9to4_name_vec_dup(request9->nlist.nlist_val, &request4->nlist.nlist_val,
                         request9->nlist.nlist_len);

  request4->nlist.nlist_len = request9->nlist.nlist_len;


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

int ndmp_4to9_data_get_env_reply(ndmp4_data_get_env_reply* reply4,
                                 ndmp9_data_get_env_reply* reply9)
{
  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  ndmp_4to9_pval_vec_dup(reply4->env.env_val, &reply9->env.env_val,
                         reply4->env.env_len);

  reply9->env.env_len = reply4->env.env_len;

  return 0;
}

int ndmp_4to9_data_get_env_free_reply(ndmp9_data_get_env_reply* reply9)
{
  ndmp_4to9_pval_vec_free(reply9->env.env_val, reply9->env.env_len);

  return 0;
}

int ndmp_9to4_data_get_env_reply(ndmp9_data_get_env_reply* reply9,
                                 ndmp4_data_get_env_reply* reply4)
{
  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  ndmp_9to4_pval_vec_dup(reply9->env.env_val, &reply4->env.env_val,
                         reply9->env.env_len);
  reply4->env.env_len = reply9->env.env_len;

  return 0;
}


/*
 * ndmp_data_stop
 * no args request, just error reply
 */

/*
 * ndmp_data_listen
 */
int ndmp_4to9_data_listen_request(ndmp4_data_listen_request* request4,
                                  ndmp9_data_listen_request* request9)
{
  int rc;

  rc = CNVT_E_TO_9(request4, request9, addr_type, ndmp_49_addr_type);
  if (rc == NDMP_INVALID_GENERAL) { CNVT_TO_9(request4, request9, addr_type); }

  return 0;
}

int ndmp_9to4_data_listen_request(ndmp9_data_listen_request* request9,
                                  ndmp4_data_listen_request* request4)
{
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, addr_type, ndmp_49_addr_type);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, addr_type);
  }

  return 0;
}

int ndmp_4to9_data_listen_reply(ndmp4_data_listen_reply* reply4,
                                ndmp9_data_listen_reply* reply9)
{
  int n_error = 0;

  CNVT_E_TO_9(reply4, reply9, error, ndmp_49_error);

  n_error +=
      ndmp_4to9_addr(&reply4->connect_addr, &reply9->data_connection_addr);

  return n_error;
}

int ndmp_9to4_data_listen_reply(ndmp9_data_listen_reply* reply9,
                                ndmp4_data_listen_reply* reply4)
{
  int n_error = 0;

  CNVT_E_FROM_9(reply4, reply9, error, ndmp_49_error);

  n_error +=
      ndmp_9to4_addr(&reply9->data_connection_addr, &reply4->connect_addr);

  return n_error;
}


/*
 * ndmp_data_connect
 * just error reply
 */

int ndmp_4to9_data_connect_request(ndmp4_data_connect_request* request4,
                                   ndmp9_data_connect_request* request9)
{
  return ndmp_4to9_addr(&request4->addr, &request9->addr);
}

int ndmp_9to4_data_connect_request(ndmp9_data_connect_request* request9,
                                   ndmp4_data_connect_request* request4)
{
  return ndmp_9to4_addr(&request9->addr, &request4->addr);
}


/*
 * NOTIFY INTERFACES
 ****************************************************************
 */

/*
 * ndmp_notify_data_halted
 * just error reply
 */

int ndmp_4to9_notify_data_halted_request(
    ndmp4_notify_data_halted_post* request4,
    ndmp9_notify_data_halted_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, reason, ndmp_49_data_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request4, request9, reason);
    n_error++;
  }

  return n_error;
}

int ndmp_9to4_notify_data_halted_request(
    ndmp9_notify_data_halted_request* request9,
    ndmp4_notify_data_halted_post* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, reason, ndmp_49_data_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, reason);
    n_error++;
  }

  return n_error;
}


/*
 * ndmp_notify_connected
 * just error reply
 */

/* NDMP4_NOTIFY_CONNECTED */
struct enum_conversion ndmp_49_connect_reason[] = {
    {
        NDMP_INVALID_GENERAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_CONNECTED, NDMP9_CONNECTED},
    {NDMP4_SHUTDOWN, NDMP9_SHUTDOWN},
    {NDMP4_REFUSED, NDMP9_REFUSED},
    END_ENUM_CONVERSION_TABLE};

int ndmp_4to9_notify_connection_status_request(
    ndmp4_notify_connection_status_post* request4,
    ndmp9_notify_connected_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, reason, ndmp_49_connect_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request4, request9, reason);
    n_error++;
  }

  CNVT_TO_9(request4, request9, protocol_version);

  CNVT_STRDUP_TO_9(request4, request9, text_reason);

  return n_error;
}

int ndmp_9to4_notify_connection_status_request(
    ndmp9_notify_connected_request* request9,
    ndmp4_notify_connection_status_post* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, reason, ndmp_49_connect_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, reason);
    n_error++;
  }

  CNVT_FROM_9(request4, request9, protocol_version);

  CNVT_STRDUP_FROM_9(request4, request9, text_reason);

  return n_error;
}


/*
 * ndmp_notify_mover_halted
 * just error reply
 */

int ndmp_4to9_notify_mover_halted_request(
    ndmp4_notify_mover_halted_post* request4,
    ndmp9_notify_mover_halted_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, reason, ndmp_49_mover_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request4, request9, reason);
    n_error++;
  }

  return n_error;
}

int ndmp_9to4_notify_mover_halted_request(
    ndmp9_notify_mover_halted_request* request9,
    ndmp4_notify_mover_halted_post* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, reason, ndmp_49_mover_halt_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, reason);
    n_error++;
  }

  return n_error;
}


/*
 * ndmp_notify_mover_paused
 * just error reply
 */

int ndmp_4to9_notify_mover_paused_request(
    ndmp4_notify_mover_paused_post* request4,
    ndmp9_notify_mover_paused_request* request9)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_TO_9(request4, request9, reason, ndmp_49_mover_pause_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_TO_9(request4, request9, reason);
    n_error++;
  }

  CNVT_TO_9(request4, request9, seek_position);

  return n_error;
}

int ndmp_9to4_notify_mover_paused_request(
    ndmp9_notify_mover_paused_request* request9,
    ndmp4_notify_mover_paused_post* request4)
{
  int n_error = 0;
  int rc;

  rc = CNVT_E_FROM_9(request4, request9, reason, ndmp_49_mover_pause_reason);
  if (rc == NDMP_INVALID_GENERAL) {
    CNVT_FROM_9(request4, request9, reason);
    n_error++;
  }

  CNVT_FROM_9(request4, request9, seek_position);

  return n_error;
}


/*
 * ndmp_notify_data_read
 * just error reply
 */

int ndmp_4to9_notify_data_read_request(ndmp4_notify_data_read_post* request4,
                                       ndmp9_notify_data_read_request* request9)
{
  CNVT_TO_9(request4, request9, offset);
  CNVT_TO_9(request4, request9, length);
  return 0;
}

int ndmp_9to4_notify_data_read_request(ndmp9_notify_data_read_request* request9,
                                       ndmp4_notify_data_read_post* request4)
{
  CNVT_FROM_9(request4, request9, offset);
  CNVT_FROM_9(request4, request9, length);
  return 0;
}


/*
 * LOGGING INTERFACES
 ****************************************************************
 */

struct enum_conversion ndmp_49_recovery_status[] = {
    {NDMP4_RECOVERY_FAILED_UNDEFINED_ERROR,
     NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR}, /* default */
    {NDMP4_RECOVERY_SUCCESSFUL, NDMP9_RECOVERY_SUCCESSFUL},
    {NDMP4_RECOVERY_FAILED_PERMISSION, NDMP9_RECOVERY_FAILED_PERMISSION},
    {NDMP4_RECOVERY_FAILED_NOT_FOUND, NDMP9_RECOVERY_FAILED_NOT_FOUND},
    {NDMP4_RECOVERY_FAILED_NO_DIRECTORY, NDMP9_RECOVERY_FAILED_NO_DIRECTORY},
    {NDMP4_RECOVERY_FAILED_OUT_OF_MEMORY, NDMP9_RECOVERY_FAILED_OUT_OF_MEMORY},
    {NDMP4_RECOVERY_FAILED_IO_ERROR, NDMP9_RECOVERY_FAILED_IO_ERROR},
    {NDMP4_RECOVERY_FAILED_UNDEFINED_ERROR,
     NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR},
    END_ENUM_CONVERSION_TABLE};


int ndmp_4to9_log_file_request(ndmp4_log_file_post* request4,
                               ndmp9_log_file_request* request9)
{
  CNVT_E_TO_9(request4, request9, recovery_status, ndmp_49_recovery_status);
  CNVT_STRDUP_TO_9(request4, request9, name);
  return 0;
}

int ndmp_9to4_log_file_request(ndmp9_log_file_request* request9,
                               ndmp4_log_file_post* request4)
{
  CNVT_E_FROM_9(request4, request9, recovery_status, ndmp_49_recovery_status);
  CNVT_STRDUP_FROM_9(request4, request9, name);
  return 0;
}


/*
 * ndmp_log_type
 */

struct enum_conversion ndmp_49_log_type[] = {
    {
        NDMP4_LOG_NORMAL,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_LOG_NORMAL, NDMP9_LOG_NORMAL},
    {NDMP4_LOG_DEBUG, NDMP9_LOG_DEBUG},
    {NDMP4_LOG_ERROR, NDMP9_LOG_ERROR},
    {NDMP4_LOG_WARNING, NDMP9_LOG_WARNING},
    END_ENUM_CONVERSION_TABLE};


int ndmp_4to9_log_message_request(ndmp4_log_message_post* request4,
                                  ndmp9_log_message_request* request9)
{
  CNVT_E_TO_9(request4, request9, log_type, ndmp_49_log_type);
  CNVT_TO_9(request4, request9, message_id);
  CNVT_STRDUP_TO_9(request4, request9, entry);

  switch (request4->associated_message_valid) {
    case NDMP4_HAS_ASSOCIATED_MESSAGE:
      request9->associated_message_sequence.valid = NDMP9_VALIDITY_VALID;
      break;

    default:
    case NDMP4_NO_ASSOCIATED_MESSAGE:
      request9->associated_message_sequence.valid = NDMP9_VALIDITY_INVALID;
      break;
  }

  request9->associated_message_sequence.value =
      request4->associated_message_sequence;

  return 0;
}

int ndmp_4to9_log_message_free_request(ndmp9_log_message_request* request9)
{
  CNVT_FREE(request9, entry);
  return 0;
}

int ndmp_9to4_log_message_request(ndmp9_log_message_request* request9,
                                  ndmp4_log_message_post* request4)
{
  CNVT_E_FROM_9(request4, request9, log_type, ndmp_49_log_type);
  CNVT_FROM_9(request4, request9, message_id);
  CNVT_STRDUP_TO_9(request4, request9, entry);

  switch (request9->associated_message_sequence.valid) {
    case NDMP9_VALIDITY_VALID:
      request4->associated_message_valid = NDMP4_HAS_ASSOCIATED_MESSAGE;
      break;

    default:
      request4->associated_message_valid = NDMP4_NO_ASSOCIATED_MESSAGE;
      break;
  }

  request4->associated_message_sequence =
      request9->associated_message_sequence.value;

  return 0;
}


/*
 * FILE HISTORY INTERFACES
 ****************************************************************
 */


/*
 * ndmp[_unix]_file_stat
 */

struct enum_conversion ndmp_49_file_type[] = {
    {
        NDMP4_FILE_OTHER,
        NDMP_INVALID_GENERAL,
    }, /* default */
    {NDMP4_FILE_DIR, NDMP9_FILE_DIR},
    {NDMP4_FILE_FIFO, NDMP9_FILE_FIFO},
    {NDMP4_FILE_CSPEC, NDMP9_FILE_CSPEC},
    {NDMP4_FILE_BSPEC, NDMP9_FILE_BSPEC},
    {NDMP4_FILE_REG, NDMP9_FILE_REG},
    {NDMP4_FILE_SLINK, NDMP9_FILE_SLINK},
    {NDMP4_FILE_SOCK, NDMP9_FILE_SOCK},
    {NDMP4_FILE_REGISTRY, NDMP9_FILE_REGISTRY},
    {NDMP4_FILE_OTHER, NDMP9_FILE_OTHER},
    END_ENUM_CONVERSION_TABLE};

extern int ndmp_4to9_file_stat(ndmp4_file_stat* fstat4,
                               ndmp9_file_stat* fstat9,
                               ndmp9_u_quad node,
                               ndmp9_u_quad fh_info)
{
  CNVT_E_TO_9(fstat4, fstat9, ftype, ndmp_49_file_type);

  CNVT_VUL_TO_9(fstat4, fstat9, mtime);
  CNVT_VUL_TO_9(fstat4, fstat9, atime);
  CNVT_VUL_TO_9(fstat4, fstat9, ctime);
  CNVT_VUL_TO_9x(fstat4, fstat9, owner, uid);
  CNVT_VUL_TO_9x(fstat4, fstat9, group, gid);

  CNVT_VUL_TO_9x(fstat4, fstat9, fattr, mode);

  CNVT_VUQ_TO_9(fstat4, fstat9, size);

  CNVT_VUL_TO_9(fstat4, fstat9, links);

  convert_valid_u_quad_to_9(&node, &fstat9->node);
  convert_valid_u_quad_to_9(&fh_info, &fstat9->fh_info);

  if (fstat4->unsupported & NDMP4_FILE_STAT_ATIME_UNS)
    CNVT_IUL_TO_9(fstat9, atime);

  if (fstat4->unsupported & NDMP4_FILE_STAT_CTIME_UNS)
    CNVT_IUL_TO_9(fstat9, ctime);

  if (fstat4->unsupported & NDMP4_FILE_STAT_GROUP_UNS)
    CNVT_IUL_TO_9(fstat9, gid);

  return 0;
}

extern int ndmp_9to4_file_stat(ndmp9_file_stat* fstat9, ndmp4_file_stat* fstat4)
{
  CNVT_E_FROM_9(fstat4, fstat9, ftype, ndmp_49_file_type);

  fstat4->fs_type = NDMP4_FS_UNIX;

  CNVT_VUL_FROM_9(fstat4, fstat9, mtime);
  CNVT_VUL_FROM_9(fstat4, fstat9, atime);
  CNVT_VUL_FROM_9(fstat4, fstat9, ctime);
  CNVT_VUL_FROM_9x(fstat4, fstat9, owner, uid);
  CNVT_VUL_FROM_9x(fstat4, fstat9, group, gid);

  CNVT_VUL_FROM_9x(fstat4, fstat9, fattr, mode);

  CNVT_VUQ_FROM_9(fstat4, fstat9, size);

  CNVT_VUL_FROM_9(fstat4, fstat9, links);

  fstat4->unsupported = 0;

  if (!fstat9->atime.valid) fstat4->unsupported |= NDMP4_FILE_STAT_ATIME_UNS;

  if (!fstat9->ctime.valid) fstat4->unsupported |= NDMP4_FILE_STAT_CTIME_UNS;

  if (!fstat9->gid.valid) fstat4->unsupported |= NDMP4_FILE_STAT_GROUP_UNS;

  /* fh_info ignored */
  /* node ignored */

  return 0;
}


/*
 * ndmp_fh_add_file_request
 */

int ndmp_4to9_fh_add_file_request(ndmp4_fh_add_file_post* request4,
                                  ndmp9_fh_add_file_request* request9)
{
  int n_ent = request4->files.files_len;
  int i;
  unsigned int j;
  ndmp9_file* table;

  table = NDMOS_MACRO_NEWN(ndmp9_file, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp4_file* ent4 = &request4->files.files_val[i];
    ndmp4_file_name* file_name;
    ndmp4_file_stat* file_stat = 0;
    ndmp4_file_stat _file_stat;
    char* filename;
    ndmp9_file* ent9 = &table[i];

    filename = "no-unix-name";
    for (j = 0; j < ent4->names.names_len; j++) {
      file_name = &ent4->names.names_val[j];
      if (file_name->fs_type == NDMP4_FS_UNIX) {
        filename = file_name->ndmp4_file_name_u.unix_name;
        break;
      }
    }
    for (j = 0; j < ent4->stats.stats_len; j++) {
      file_stat = &ent4->stats.stats_val[j];
      if (file_stat->fs_type == NDMP4_FS_UNIX) { break; }
    }
    if (j >= ent4->stats.stats_len) {
      file_stat = &_file_stat;
      NDMOS_MACRO_ZEROFILL(file_stat);
    }

    ent9->unix_path = NDMOS_API_STRDUP(filename);
    ndmp_4to9_file_stat(file_stat, &ent9->fstat, ent4->node, ent4->fh_info);
  }

  request9->files.files_len = n_ent;
  request9->files.files_val = table;

  return 0;
}

int ndmp_4to9_fh_add_file_free_request(ndmp9_fh_add_file_request* request9)
{
  int i;

  for (i = 0; i < request9->files.files_len; i++) {
    NDMOS_API_FREE(request9->files.files_val[i].unix_path);
  }

  NDMOS_MACRO_FREE(request9->files.files_val);

  return 0;
}

int ndmp_9to4_fh_add_file_request(ndmp9_fh_add_file_request* request9,
                                  ndmp4_fh_add_file_post* request4)
{
  int n_ent = request9->files.files_len;
  int i;
  ndmp4_file* table;

  table = NDMOS_MACRO_NEWN(ndmp4_file, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp9_file* ent9 = &request9->files.files_val[i];
    ndmp4_file* ent4 = &table[i];

    ent4->names.names_val = NDMOS_MACRO_NEW(ndmp4_file_name);
    ent4->names.names_len = 1;
    ent4->stats.stats_val = NDMOS_MACRO_NEW(ndmp4_file_stat);
    ent4->stats.stats_len = 1;

    ent4->names.names_val[0].fs_type = NDMP4_FS_UNIX;
    ent4->names.names_val[0].ndmp4_file_name_u.unix_name =
        NDMOS_API_STRDUP(ent9->unix_path);

    ndmp_9to4_file_stat(&ent9->fstat, &ent4->stats.stats_val[0]);
    ent4->node = ent9->fstat.node.value;
    ent4->fh_info = ent9->fstat.fh_info.value;
  }

  request4->files.files_len = n_ent;
  request4->files.files_val = table;

  return 0;
}


/*
 * ndmp_fh_add_unix_dir
 */

int ndmp_4to9_fh_add_dir_request(ndmp4_fh_add_dir_post* request4,
                                 ndmp9_fh_add_dir_request* request9)
{
  int n_ent = request4->dirs.dirs_len;
  int i;
  unsigned int j;
  ndmp9_dir* table;

  table = NDMOS_MACRO_NEWN(ndmp9_dir, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp4_dir* ent4 = &request4->dirs.dirs_val[i];
    ndmp4_file_name* file_name;
    char* filename;
    ndmp9_dir* ent9 = &table[i];

    filename = "no-unix-name";
    for (j = 0; j < ent4->names.names_len; j++) {
      file_name = &ent4->names.names_val[j];
      if (file_name->fs_type == NDMP4_FS_UNIX) {
        filename = file_name->ndmp4_file_name_u.unix_name;
        break;
      }
    }

    ent9->unix_name = NDMOS_API_STRDUP(filename);
    ent9->node = ent4->node;
    ent9->parent = ent4->parent;
  }

  request9->dirs.dirs_len = n_ent;
  request9->dirs.dirs_val = table;

  return 0;
}

int ndmp_4to9_fh_add_dir_free_request(ndmp9_fh_add_dir_request* request9)
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

int ndmp_9to4_fh_add_dir_request(ndmp9_fh_add_dir_request* request9,
                                 ndmp4_fh_add_dir_post* request4)
{
  int n_ent = request9->dirs.dirs_len;
  int i;
  ndmp4_dir* table;

  table = NDMOS_MACRO_NEWN(ndmp4_dir, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp9_dir* ent9 = &request9->dirs.dirs_val[i];
    ndmp4_dir* ent4 = &table[i];

    ent4->names.names_val = NDMOS_MACRO_NEW(ndmp4_file_name);
    ent4->names.names_len = 1;

    ent4->names.names_val[0].fs_type = NDMP4_FS_UNIX;
    ent4->names.names_val[0].ndmp4_file_name_u.unix_name =
        NDMOS_API_STRDUP(ent9->unix_name);

    ent4->node = ent9->node;
    ent4->parent = ent9->parent;
  }

  request4->dirs.dirs_len = n_ent;
  request4->dirs.dirs_val = table;

  return 0;
}

int ndmp_9to4_fh_add_dir_free_request(ndmp4_fh_add_dir_post* request4)
{
  int i;

  if (request4) {
    if (request4->dirs.dirs_val) {
      int n_ent = request4->dirs.dirs_len;

      for (i = 0; i < n_ent; i++) {
        ndmp4_dir* ent4 = &request4->dirs.dirs_val[i];
        if (ent4->names.names_val) {
          if (ent4->names.names_val[0].ndmp4_file_name_u.unix_name)
            NDMOS_API_FREE(
                ent4->names.names_val[0].ndmp4_file_name_u.unix_name);
          ent4->names.names_val[0].ndmp4_file_name_u.unix_name = 0;

          NDMOS_API_FREE(ent4->names.names_val);
        }
        ent4->names.names_val = 0;
      }

      NDMOS_API_FREE(request4->dirs.dirs_val);
    }
    request4->dirs.dirs_val = 0;
  }

  return 0;
}

/*
 * ndmp_fh_add_node_request
 */

int ndmp_4to9_fh_add_node_request(ndmp4_fh_add_node_post* request4,
                                  ndmp9_fh_add_node_request* request9)
{
  int n_ent = request4->nodes.nodes_len;
  int i;
  unsigned int j;
  ndmp9_node* table;

  table = NDMOS_MACRO_NEWN(ndmp9_node, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp4_node* ent4 = &request4->nodes.nodes_val[i];
    ndmp4_file_stat* file_stat = 0;
    ndmp4_file_stat _file_stat;
    ndmp9_node* ent9 = &table[i];

    for (j = 0; j < ent4->stats.stats_len; j++) {
      file_stat = &ent4->stats.stats_val[j];
      if (file_stat->fs_type == NDMP4_FS_UNIX) { break; }
    }
    if (j >= ent4->stats.stats_len) {
      file_stat = &_file_stat;
      NDMOS_MACRO_ZEROFILL(file_stat);
    }

    ndmp_4to9_file_stat(file_stat, &ent9->fstat, ent4->node, ent4->fh_info);
  }

  request9->nodes.nodes_len = n_ent;
  request9->nodes.nodes_val = table;

  return 0;
}

int ndmp_4to9_fh_add_node_free_request(ndmp9_fh_add_node_request* request9)
{
  if (request9) {
    if (request9->nodes.nodes_val) {
      NDMOS_API_FREE(request9->nodes.nodes_val);
    }
    request9->nodes.nodes_val = 0;
  }
  return 0;
}

int ndmp_9to4_fh_add_node_request(ndmp9_fh_add_node_request* request9,
                                  ndmp4_fh_add_node_post* request4)
{
  int n_ent = request9->nodes.nodes_len;
  int i;
  ndmp4_node* table;

  table = NDMOS_MACRO_NEWN(ndmp4_node, n_ent);
  if (!table) return -1;

  NDMOS_API_BZERO(table, sizeof *table * n_ent);

  for (i = 0; i < n_ent; i++) {
    ndmp9_node* ent9 = &request9->nodes.nodes_val[i];
    ndmp4_node* ent4 = &table[i];

    ent4->stats.stats_val = NDMOS_MACRO_NEW(ndmp4_file_stat);
    ent4->stats.stats_len = 1;

    ndmp_9to4_file_stat(&ent9->fstat, &ent4->stats.stats_val[0]);
    ent4->node = ent9->fstat.node.value;
    ent4->fh_info = ent9->fstat.fh_info.value;
  }

  request4->nodes.nodes_len = n_ent;
  request4->nodes.nodes_val = table;

  return 0;
}

int ndmp_9to4_fh_add_node_free_request(ndmp4_fh_add_node_post* request4)
{
  if (request4) {
    if (request4->nodes.nodes_val) {
      NDMOS_API_FREE(request4->nodes.nodes_val);
    }
    request4->nodes.nodes_val = 0;
  }
  return 0;
}


/*
 * request/reply translation
 */

#define NO_ARG_REQUEST ndmp_xtox_no_arguments, ndmp_xtox_no_arguments

#define JUST_ERROR_REPLY ndmp_4to9_error, ndmp_9to4_error

#define NO_ARG_REQUEST_JUST_ERROR_REPLY NO_ARG_REQUEST, JUST_ERROR_REPLY

#define NO_MEMUSED_REQUEST ndmp_xtox_no_memused, ndmp_xtox_no_memused

#define NO_MEMUSED_REPLY ndmp_xtox_no_memused, ndmp_xtox_no_memused

#define NO_MEMUSED                                                  \
  ndmp_xtox_no_memused, ndmp_xtox_no_memused, ndmp_xtox_no_memused, \
      ndmp_xtox_no_memused


struct reqrep_xlate ndmp4_reqrep_xlate_table[] = {
    {
        NDMP4_CONNECT_OPEN, NDMP9_CONNECT_OPEN, ndmp_4to9_connect_open_request,
        ndmp_9to4_connect_open_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONNECT_CLIENT_AUTH, NDMP9_CONNECT_CLIENT_AUTH,
        ndmp_4to9_connect_client_auth_request,
        ndmp_9to4_connect_client_auth_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONNECT_CLOSE, NDMP9_CONNECT_CLOSE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY, /* actually no reply */
        NO_MEMUSED /* no memory free routines written yet */
    },
#ifdef notyet
    {
        NDMP4_CONNECT_SERVER_AUTH, NDMP9_CONNECT_SERVER_AUTH,
        ndmp_4to9_connect_server_auth_request,
        ndmp_9to4_connect_server_auth_request,
        ndmp_4to9_connect_server_auth_reply,
        ndmp_9to4_connect_server_auth_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
#endif /* notyet */
    {
        NDMP4_CONFIG_GET_HOST_INFO, NDMP9_CONFIG_GET_HOST_INFO, NO_ARG_REQUEST,
        ndmp_4to9_config_get_host_info_reply,
        ndmp_9to4_config_get_host_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_CONNECTION_TYPE, NDMP9_CONFIG_GET_CONNECTION_TYPE,
        NO_ARG_REQUEST, ndmp_4to9_config_get_connection_type_reply,
        ndmp_9to4_config_get_connection_type_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_AUTH_ATTR, NDMP9_CONFIG_GET_AUTH_ATTR,
        ndmp_4to9_config_get_auth_attr_request,
        ndmp_9to4_config_get_auth_attr_request,
        ndmp_4to9_config_get_auth_attr_reply,
        ndmp_9to4_config_get_auth_attr_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_SERVER_INFO, NDMP9_CONFIG_GET_SERVER_INFO,
        NO_ARG_REQUEST, ndmp_4to9_config_get_server_info_reply,
        ndmp_9to4_config_get_server_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_BUTYPE_INFO, NDMP9_CONFIG_GET_BUTYPE_INFO,
        NO_ARG_REQUEST, ndmp_4to9_config_get_butype_info_reply,
        ndmp_9to4_config_get_butype_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_FS_INFO, NDMP9_CONFIG_GET_FS_INFO, NO_ARG_REQUEST,
        ndmp_4to9_config_get_fs_info_reply, ndmp_9to4_config_get_fs_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_TAPE_INFO, NDMP9_CONFIG_GET_TAPE_INFO, NO_ARG_REQUEST,
        ndmp_4to9_config_get_tape_info_reply,
        ndmp_9to4_config_get_tape_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_CONFIG_GET_SCSI_INFO, NDMP9_CONFIG_GET_SCSI_INFO, NO_ARG_REQUEST,
        ndmp_4to9_config_get_scsi_info_reply,
        ndmp_9to4_config_get_scsi_info_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP4_SCSI_OPEN, NDMP9_SCSI_OPEN, ndmp_4to9_scsi_open_request,
        ndmp_9to4_scsi_open_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_SCSI_CLOSE, NDMP9_SCSI_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_SCSI_GET_STATE, NDMP9_SCSI_GET_STATE, NO_ARG_REQUEST,
        ndmp_4to9_scsi_get_state_reply, ndmp_9to4_scsi_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_SCSI_RESET_DEVICE, NDMP9_SCSI_RESET_DEVICE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_SCSI_EXECUTE_CDB, NDMP9_SCSI_EXECUTE_CDB,
        ndmp_4to9_execute_cdb_request, ndmp_9to4_execute_cdb_request,
        ndmp_4to9_execute_cdb_reply, ndmp_9to4_execute_cdb_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },


    {NDMP4_TAPE_OPEN, NDMP9_TAPE_OPEN, ndmp_4to9_tape_open_request,
     ndmp_9to4_tape_open_request, JUST_ERROR_REPLY,
     ndmp_4to9_tape_open_free_request, ndmp_xtox_no_memused,
     ndmp_xtox_no_memused, ndmp_xtox_no_memused},
    {
        NDMP4_TAPE_CLOSE, NDMP9_TAPE_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_TAPE_GET_STATE, NDMP9_TAPE_GET_STATE, NO_ARG_REQUEST,
        ndmp_4to9_tape_get_state_reply, ndmp_9to4_tape_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_TAPE_MTIO, NDMP9_TAPE_MTIO, ndmp_4to9_tape_mtio_request,
        ndmp_9to4_tape_mtio_request, ndmp_4to9_tape_mtio_reply,
        ndmp_9to4_tape_mtio_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_TAPE_WRITE, NDMP9_TAPE_WRITE, ndmp_4to9_tape_write_request,
        ndmp_9to4_tape_write_request, ndmp_4to9_tape_write_reply,
        ndmp_9to4_tape_write_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_TAPE_READ, NDMP9_TAPE_READ, ndmp_4to9_tape_read_request,
        ndmp_9to4_tape_read_request, ndmp_4to9_tape_read_reply,
        ndmp_9to4_tape_read_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_TAPE_EXECUTE_CDB, NDMP9_TAPE_EXECUTE_CDB,
        ndmp_4to9_execute_cdb_request, ndmp_9to4_execute_cdb_request,
        ndmp_4to9_execute_cdb_reply, ndmp_9to4_execute_cdb_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP4_DATA_GET_STATE, NDMP9_DATA_GET_STATE, NO_ARG_REQUEST,
        ndmp_4to9_data_get_state_reply, ndmp_9to4_data_get_state_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_START_BACKUP, NDMP9_DATA_START_BACKUP,
        ndmp_4to9_data_start_backup_request,
        ndmp_9to4_data_start_backup_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_START_RECOVER, NDMP9_DATA_START_RECOVER,
        ndmp_4to9_data_start_recover_request,
        ndmp_9to4_data_start_recover_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_ABORT, NDMP9_DATA_ABORT, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {NDMP4_DATA_GET_ENV, NDMP9_DATA_GET_ENV, NO_ARG_REQUEST,
     ndmp_4to9_data_get_env_reply, ndmp_9to4_data_get_env_reply,
     ndmp_xtox_no_memused, ndmp_xtox_no_memused,
     ndmp_4to9_data_get_env_free_reply, ndmp_xtox_no_memused},
    {
        NDMP4_DATA_STOP, NDMP9_DATA_STOP, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_LISTEN, NDMP9_DATA_LISTEN, ndmp_4to9_data_listen_request,
        ndmp_9to4_data_listen_request, ndmp_4to9_data_listen_reply,
        ndmp_9to4_data_listen_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_CONNECT, NDMP9_DATA_CONNECT, ndmp_4to9_data_connect_request,
        ndmp_9to4_data_connect_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_DATA_START_RECOVER_FILEHIST, NDMP9_DATA_START_RECOVER_FILEHIST,
        ndmp_4to9_data_start_recover_request,
        ndmp_9to4_data_start_recover_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {
        NDMP4_NOTIFY_DATA_HALTED, NDMP9_NOTIFY_DATA_HALTED,
        ndmp_4to9_notify_data_halted_request,
        ndmp_9to4_notify_data_halted_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP4_NOTIFY_CONNECTION_STATUS, NDMP9_NOTIFY_CONNECTED,
        ndmp_4to9_notify_connection_status_request,
        ndmp_9to4_notify_connection_status_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP4_NOTIFY_MOVER_HALTED, NDMP9_NOTIFY_MOVER_HALTED,
        ndmp_4to9_notify_mover_halted_request,
        ndmp_9to4_notify_mover_halted_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP4_NOTIFY_MOVER_PAUSED, NDMP9_NOTIFY_MOVER_PAUSED,
        ndmp_4to9_notify_mover_paused_request,
        ndmp_9to4_notify_mover_paused_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },
    {
        NDMP4_NOTIFY_DATA_READ, NDMP9_NOTIFY_DATA_READ,
        ndmp_4to9_notify_data_read_request, ndmp_9to4_notify_data_read_request,
        JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED        /* no memory free routines written yet */
    },

    {
        NDMP4_LOG_FILE, NDMP9_LOG_FILE, ndmp_4to9_log_file_request,
        ndmp_9to4_log_file_request, JUST_ERROR_REPLY, /* no reply actually */
        NO_MEMUSED /* no memory free routines written yet */
    },
    {NDMP4_LOG_MESSAGE, NDMP9_LOG_MESSAGE, ndmp_4to9_log_message_request,
     ndmp_9to4_log_message_request, JUST_ERROR_REPLY, /* no reply actually */
     ndmp_4to9_log_message_free_request, ndmp_xtox_no_memused,
     ndmp_xtox_no_memused, ndmp_xtox_no_memused},

    {NDMP4_FH_ADD_FILE, NDMP9_FH_ADD_FILE, ndmp_4to9_fh_add_file_request,
     ndmp_9to4_fh_add_file_request, JUST_ERROR_REPLY, /* no reply actually */
     ndmp_4to9_fh_add_file_free_request, ndmp_xtox_no_memused,
     ndmp_xtox_no_memused, ndmp_xtox_no_memused},
    {NDMP4_FH_ADD_DIR, NDMP9_FH_ADD_DIR, ndmp_4to9_fh_add_dir_request,
     ndmp_9to4_fh_add_dir_request, JUST_ERROR_REPLY, /* no reply actually */
     ndmp_4to9_fh_add_dir_free_request, ndmp_9to4_fh_add_dir_free_request,
     NO_MEMUSED_REPLY},
    {NDMP4_FH_ADD_NODE, NDMP9_FH_ADD_NODE, ndmp_4to9_fh_add_node_request,
     ndmp_9to4_fh_add_node_request, JUST_ERROR_REPLY, /* no reply actually */
     ndmp_4to9_fh_add_node_free_request, ndmp_9to4_fh_add_node_free_request,
     NO_MEMUSED_REPLY},

    {NDMP4_MOVER_GET_STATE, NDMP9_MOVER_GET_STATE, NO_ARG_REQUEST,
     ndmp_4to9_mover_get_state_reply, ndmp_9to4_mover_get_state_reply,
     ndmp_xtox_no_memused, ndmp_xtox_no_memused, ndmp_xtox_no_memused,
     ndmp_9to4_mover_get_state_free_reply},
    {
        NDMP4_MOVER_LISTEN, NDMP9_MOVER_LISTEN, ndmp_4to9_mover_listen_request,
        ndmp_9to4_mover_listen_request, ndmp_4to9_mover_listen_reply,
        ndmp_9to4_mover_listen_reply,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_CONNECT, NDMP9_MOVER_CONNECT,
        ndmp_4to9_mover_connect_request, ndmp_9to4_mover_connect_request,
        JUST_ERROR_REPLY, NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_SET_RECORD_SIZE, NDMP9_MOVER_SET_RECORD_SIZE,
        ndmp_4to9_mover_set_record_size_request,
        ndmp_9to4_mover_set_record_size_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_SET_WINDOW, NDMP9_MOVER_SET_WINDOW,
        ndmp_4to9_mover_set_window_request, ndmp_9to4_mover_set_window_request,
        JUST_ERROR_REPLY, NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_CONTINUE, NDMP9_MOVER_CONTINUE,
        NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_ABORT, NDMP9_MOVER_ABORT, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_STOP, NDMP9_MOVER_STOP, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_READ, NDMP9_MOVER_READ, ndmp_4to9_mover_read_request,
        ndmp_9to4_mover_read_request, JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },
    {
        NDMP4_MOVER_CLOSE, NDMP9_MOVER_CLOSE, NO_ARG_REQUEST_JUST_ERROR_REPLY,
        NO_MEMUSED /* no memory free routines written yet */
    },

    {0},
};


#endif /* !NDMOS_OPTION_NO_NDMP4 */
