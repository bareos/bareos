/*
 * Copyright (c) 2000
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
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


#include "ndmos.h"		/* rpc/rpc.h */
#include "ndmprotocol.h"
#include "ndmp_msg_buf.h"
#include "ndmp_translate.h"


#ifndef NDMOS_OPTION_NO_NDMP3


/*
 * Pervasive Types
 ****************************************************************
 */

/*
 * ndmp_error
 ****************************************************************
 */

struct enum_conversion ndmp_39_error[] = {
      { NDMP3_UNDEFINED_ERR,		NDMP9_UNDEFINED_ERR }, /* default */
      { NDMP3_NO_ERR,			NDMP9_NO_ERR },
      { NDMP3_NOT_SUPPORTED_ERR,	NDMP9_NOT_SUPPORTED_ERR },
      { NDMP3_DEVICE_BUSY_ERR,		NDMP9_DEVICE_BUSY_ERR },
      { NDMP3_DEVICE_OPENED_ERR,	NDMP9_DEVICE_OPENED_ERR },
      { NDMP3_NOT_AUTHORIZED_ERR,	NDMP9_NOT_AUTHORIZED_ERR },
      { NDMP3_PERMISSION_ERR,		NDMP9_PERMISSION_ERR },
      { NDMP3_DEV_NOT_OPEN_ERR,		NDMP9_DEV_NOT_OPEN_ERR },
      { NDMP3_IO_ERR,			NDMP9_IO_ERR },
      { NDMP3_TIMEOUT_ERR,		NDMP9_TIMEOUT_ERR },
      { NDMP3_ILLEGAL_ARGS_ERR,		NDMP9_ILLEGAL_ARGS_ERR },
      { NDMP3_NO_TAPE_LOADED_ERR,	NDMP9_NO_TAPE_LOADED_ERR },
      { NDMP3_WRITE_PROTECT_ERR,	NDMP9_WRITE_PROTECT_ERR },
      { NDMP3_EOF_ERR,			NDMP9_EOF_ERR },
      { NDMP3_EOM_ERR,			NDMP9_EOM_ERR },
      { NDMP3_FILE_NOT_FOUND_ERR,	NDMP9_FILE_NOT_FOUND_ERR },
      { NDMP3_BAD_FILE_ERR,		NDMP9_BAD_FILE_ERR },
      { NDMP3_NO_DEVICE_ERR,		NDMP9_NO_DEVICE_ERR },
      { NDMP3_NO_BUS_ERR,		NDMP9_NO_BUS_ERR },
      { NDMP3_XDR_DECODE_ERR,		NDMP9_XDR_DECODE_ERR },
      { NDMP3_ILLEGAL_STATE_ERR,	NDMP9_ILLEGAL_STATE_ERR },
      { NDMP3_UNDEFINED_ERR,		NDMP9_UNDEFINED_ERR },
      { NDMP3_XDR_ENCODE_ERR,		NDMP9_XDR_ENCODE_ERR },
      { NDMP3_NO_MEM_ERR,		NDMP9_NO_MEM_ERR },
      { NDMP3_CONNECT_ERR,		NDMP9_CONNECT_ERR },
	END_ENUM_CONVERSION_TABLE
};


extern int
ndmp_3to9_error (
  ndmp3_error *error3,
  ndmp9_error *error9)
{
	*error9 = convert_enum_to_9 (ndmp_39_error, *error3);
	return 0;
}

extern int
ndmp_9to3_error (
  ndmp9_error *error9,
  ndmp3_error *error3)
{
	*error3 = convert_enum_from_9 (ndmp_39_error, *error9);
	return 0;
}




/*
 * ndmp_pval
 ****************************************************************
 */

int
ndmp_3to9_pval (
  ndmp3_pval *pval3,
  ndmp9_pval *pval9)
{
	CNVT_STRDUP_TO_9(pval3, pval9, name);
	CNVT_STRDUP_TO_9(pval3, pval9, value);

	return 0;
}

int
ndmp_9to3_pval (
  ndmp9_pval *pval9,
  ndmp3_pval *pval3)
{
	CNVT_STRDUP_FROM_9(pval3, pval9, name);
	CNVT_STRDUP_FROM_9(pval3, pval9, value);

	return 0;
}

int
ndmp_3to9_pval_vec (
  ndmp3_pval *pval3,
  ndmp9_pval *pval9,
  unsigned n_pval)
{
	unsigned int	i;

	for (i = 0; i < n_pval; i++)
		ndmp_3to9_pval (&pval3[i], &pval9[i]);

	return 0;
}

int
ndmp_9to3_pval_vec (
  ndmp9_pval *pval9,
  ndmp3_pval *pval3,
  unsigned n_pval)
{
	unsigned int	i;

	for (i = 0; i < n_pval; i++)
		ndmp_9to3_pval (&pval9[i], &pval3[i]);

	return 0;
}

int
ndmp_3to9_pval_vec_dup (
  ndmp3_pval *pval3,
  ndmp9_pval **pval9_p,
  unsigned n_pval)
{
	*pval9_p = NDMOS_MACRO_NEWN (ndmp9_pval, n_pval);
	if (!*pval9_p)
		return -1;

	return ndmp_3to9_pval_vec (pval3, *pval9_p, n_pval);
}

int
ndmp_9to3_pval_vec_dup (
  ndmp9_pval *pval9,
  ndmp3_pval **pval3_p,
  unsigned n_pval)
{
	*pval3_p = NDMOS_MACRO_NEWN (ndmp3_pval, n_pval);
	if (!*pval3_p)
		return -1;

	return ndmp_9to3_pval_vec (pval9, *pval3_p, n_pval);
}


/*
 * ndmp_addr
 ****************************************************************
 */

struct enum_conversion ndmp_39_addr_type[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL }, /* default */
      { NDMP3_ADDR_LOCAL,		NDMP9_ADDR_LOCAL },
      { NDMP3_ADDR_TCP,			NDMP9_ADDR_TCP },
	END_ENUM_CONVERSION_TABLE
};


extern int
ndmp_3to9_addr (
  ndmp3_addr *addr3,
  ndmp9_addr *addr9)
{
	switch (addr3->addr_type) {
	case NDMP3_ADDR_LOCAL:
		addr9->addr_type = NDMP9_ADDR_LOCAL;
		break;

	case NDMP3_ADDR_TCP:
		addr9->addr_type = NDMP9_ADDR_TCP;
		addr9->ndmp9_addr_u.tcp_addr.ip_addr =
			addr3->ndmp3_addr_u.tcp_addr.ip_addr;
		addr9->ndmp9_addr_u.tcp_addr.port =
			addr3->ndmp3_addr_u.tcp_addr.port;
		break;

	default:
		NDMOS_MACRO_ZEROFILL (addr9);
		addr9->addr_type = -1;
		return -1;
	}

	return 0;
}

extern int
ndmp_9to3_addr (
  ndmp9_addr *addr9,
  ndmp3_addr *addr3)
{
	switch (addr9->addr_type) {
	case NDMP9_ADDR_LOCAL:
		addr3->addr_type = NDMP3_ADDR_LOCAL;
		break;

	case NDMP9_ADDR_TCP:
		addr3->addr_type = NDMP3_ADDR_TCP;
		addr3->ndmp3_addr_u.tcp_addr.ip_addr =
			addr9->ndmp9_addr_u.tcp_addr.ip_addr;
		addr3->ndmp3_addr_u.tcp_addr.port =
			addr9->ndmp9_addr_u.tcp_addr.port;
		break;

	default:
		NDMOS_MACRO_ZEROFILL (addr3);
		addr3->addr_type = -1;
		return -1;
	}

	return 0;
}




/*
 * CONNECT INTERFACES
 ****************************************************************
 */

struct enum_conversion ndmp_39_auth_type[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL }, /* default */
      { NDMP3_AUTH_NONE,		NDMP9_AUTH_NONE },
      { NDMP3_AUTH_TEXT,		NDMP9_AUTH_TEXT },
      { NDMP3_AUTH_MD5,			NDMP9_AUTH_MD5 },
	END_ENUM_CONVERSION_TABLE
};

int
ndmp_3to9_auth_data (
  ndmp3_auth_data *auth_data3,
  ndmp9_auth_data *auth_data9)
{
	int			n_error = 0;
	int			rc;
	ndmp3_auth_text		*text3;
	ndmp9_auth_text		*text9;
	ndmp3_auth_md5		*md53;
	ndmp9_auth_md5		*md59;

	switch (auth_data3->auth_type) {
	case NDMP3_AUTH_NONE:
		auth_data9->auth_type = NDMP9_AUTH_NONE;
		break;

	case NDMP3_AUTH_TEXT:
		auth_data9->auth_type = NDMP9_AUTH_TEXT;
		text3 = &auth_data3->ndmp3_auth_data_u.auth_text;
		text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
		rc = CNVT_STRDUP_TO_9(text3, text9, auth_id);
		if (rc) {
			return rc;	/* no mem */
		}
		rc = CNVT_STRDUP_TO_9(text3, text9, auth_password);
		if (rc) {
			NDMOS_API_FREE (text9->auth_id);
			text9->auth_id = 0;
			return rc;	/* no mem */
		}
		break;

	case NDMP3_AUTH_MD5:
		auth_data9->auth_type = NDMP9_AUTH_MD5;
		md53 = &auth_data3->ndmp3_auth_data_u.auth_md5;
		md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
		rc = CNVT_STRDUP_TO_9(md53, md59, auth_id);
		if (rc) {
			return rc;	/* no mem */
		}
		NDMOS_API_BCOPY (md53->auth_digest, md59->auth_digest, 16);
		break;

	default:
		auth_data9->auth_type = auth_data3->auth_type;
		NDMOS_MACRO_ZEROFILL (&auth_data9->ndmp9_auth_data_u);
		n_error++;
		break;
	}

	return n_error;
}

int
ndmp_9to3_auth_data (
  ndmp9_auth_data *auth_data9,
  ndmp3_auth_data *auth_data3)
{
	int			n_error = 0;
	int			rc;
	ndmp9_auth_text		*text9;
	ndmp3_auth_text		*text3;
	ndmp9_auth_md5		*md59;
	ndmp3_auth_md5		*md53;

	switch (auth_data9->auth_type) {
	case NDMP9_AUTH_NONE:
		auth_data3->auth_type = NDMP3_AUTH_NONE;
		break;

	case NDMP9_AUTH_TEXT:
		auth_data3->auth_type = NDMP3_AUTH_TEXT;
		text9 = &auth_data9->ndmp9_auth_data_u.auth_text;
		text3 = &auth_data3->ndmp3_auth_data_u.auth_text;
		rc = CNVT_STRDUP_FROM_9(text3, text9, auth_id);
		if (rc) {
			return rc;	/* no mem */
		}
		rc = CNVT_STRDUP_FROM_9(text3, text9, auth_password);
		if (rc) {
			NDMOS_API_FREE (text9->auth_id);
			text3->auth_id = 0;
			return rc;	/* no mem */
		}
		break;

	case NDMP9_AUTH_MD5:
		auth_data3->auth_type = NDMP3_AUTH_MD5;
		md59 = &auth_data9->ndmp9_auth_data_u.auth_md5;
		md53 = &auth_data3->ndmp3_auth_data_u.auth_md5;
		rc = CNVT_STRDUP_FROM_9(md53, md59, auth_id);
		if (rc) {
			return rc;	/* no mem */
		}
		NDMOS_API_BCOPY (md59->auth_digest, md53->auth_digest, 16);
		break;

	default:
		auth_data3->auth_type = auth_data9->auth_type;
		NDMOS_MACRO_ZEROFILL (&auth_data3->ndmp3_auth_data_u);
		n_error++;
		break;
	}

	return n_error;
}

int
ndmp_3to9_auth_attr (
  ndmp3_auth_attr *auth_attr3,
  ndmp9_auth_attr *auth_attr9)
{
	int			n_error = 0;

	switch (auth_attr3->auth_type) {
	case NDMP3_AUTH_NONE:
		auth_attr9->auth_type = NDMP9_AUTH_NONE;
		break;

	case NDMP3_AUTH_TEXT:
		auth_attr9->auth_type = NDMP9_AUTH_TEXT;
		break;

	case NDMP3_AUTH_MD5:
		auth_attr9->auth_type = NDMP9_AUTH_MD5;
		NDMOS_API_BCOPY (auth_attr3->ndmp3_auth_attr_u.challenge,
			auth_attr9->ndmp9_auth_attr_u.challenge, 64);
		break;

	default:
		auth_attr9->auth_type = auth_attr3->auth_type;
		NDMOS_MACRO_ZEROFILL (&auth_attr9->ndmp9_auth_attr_u);
		n_error++;
		break;
	}

	return n_error;
}

int
ndmp_9to3_auth_attr (
  ndmp9_auth_attr *auth_attr9,
  ndmp3_auth_attr *auth_attr3)
{
	int			n_error = 0;

	switch (auth_attr9->auth_type) {
	case NDMP9_AUTH_NONE:
		auth_attr3->auth_type = NDMP3_AUTH_NONE;
		break;

	case NDMP9_AUTH_TEXT:
		auth_attr3->auth_type = NDMP3_AUTH_TEXT;
		break;

	case NDMP9_AUTH_MD5:
		auth_attr3->auth_type = NDMP3_AUTH_MD5;
		NDMOS_API_BCOPY (auth_attr9->ndmp9_auth_attr_u.challenge,
			auth_attr3->ndmp3_auth_attr_u.challenge, 64);
		break;

	default:
		auth_attr3->auth_type = auth_attr9->auth_type;
		NDMOS_MACRO_ZEROFILL (&auth_attr3->ndmp3_auth_attr_u);
		n_error++;
		break;
	}

	return n_error;
}


/*
 * ndmp_connect_open
 * just error reply
 */

int
ndmp_3to9_connect_open_request (
  ndmp3_connect_open_request *request3,
  ndmp9_connect_open_request *request9)
{
	CNVT_TO_9 (request3, request9, protocol_version);
	return 0;
}

int
ndmp_9to3_connect_open_request (
  ndmp9_connect_open_request *request9,
  ndmp3_connect_open_request *request3)
{
	CNVT_FROM_9 (request3, request9, protocol_version);
	return 0;
}


/*
 * ndmp_connect_client_auth
 * just error reply
 */

int
ndmp_3to9_connect_client_auth_request (
  ndmp3_connect_client_auth_request *request3,
  ndmp9_connect_client_auth_request *request9)
{
	int		rc;

	rc = ndmp_3to9_auth_data (&request3->auth_data, &request9->auth_data);

	return rc;
}

int
ndmp_9to3_connect_client_auth_request (
  ndmp9_connect_client_auth_request *request9,
  ndmp3_connect_client_auth_request *request3)
{
	int		rc;

	rc = ndmp_9to3_auth_data (&request9->auth_data, &request3->auth_data);

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
int
ndmp_3to9_connect_server_auth_request (
  ndmp3_connect_server_auth_request *request3,
  ndmp9_connect_server_auth_request *request9)
{
	return -1;
}

int
ndmp_9to3_connect_server_auth_request (
  ndmp9_connect_server_auth_request *request9,
  ndmp3_connect_server_auth_request *request3)
{
	return -1;
}




/*
 * CONFIG INTERFACES
 ****************************************************************
 */

int
ndmp_3to9_device_info_vec_dup (
  ndmp3_device_info *devinf3,
  ndmp9_device_info **devinf9_p,
  int n_devinf)
{
	ndmp9_device_info *	devinf9;
	int			i;
	unsigned int		j;

	devinf9 = *devinf9_p = NDMOS_MACRO_NEWN(ndmp9_device_info, n_devinf);
	if (!devinf9) {
		return -1;	/* no mem */
	}

	for (i = 0; i < n_devinf; i++) {
		ndmp3_device_info *	di3 = &devinf3[i];
		ndmp9_device_info *	di9 = &devinf9[i];

		NDMOS_MACRO_ZEROFILL (di9);

		CNVT_STRDUP_TO_9 (di3, di9, model);

		di9->caplist.caplist_val =
			NDMOS_MACRO_NEWN (ndmp9_device_capability,
				di3->caplist.caplist_len);

		if (!di9->caplist.caplist_val) {
			return -1;
		}

		for (j = 0; j < di3->caplist.caplist_len; j++) {
			ndmp3_device_capability *	cap3;
			ndmp9_device_capability *	cap9;

			cap3 = &di3->caplist.caplist_val[j];
			cap9 = &di9->caplist.caplist_val[j];

			NDMOS_MACRO_ZEROFILL (cap9);

			cap9->v3attr.valid = NDMP9_VALIDITY_VALID;
			cap9->v3attr.value = cap3->attr;

			CNVT_STRDUP_TO_9 (cap3, cap9, device);

			ndmp_3to9_pval_vec_dup (
				cap3->capability.capability_val,
				&cap9->capability.capability_val,
				cap3->capability.capability_len);

			cap9->capability.capability_len =
				cap3->capability.capability_len;
		}
		di9->caplist.caplist_len = j;
	}

	return 0;
}

int
ndmp_9to3_device_info_vec_dup (
  ndmp9_device_info *devinf9,
  ndmp3_device_info **devinf3_p,
  int n_devinf)
{
	ndmp3_device_info *	devinf3;
	int			i;
	unsigned int		j;

	devinf3 = *devinf3_p = NDMOS_MACRO_NEWN(ndmp3_device_info, n_devinf);
	if (!devinf3) {
		return -1;	/* no mem */
	}

	for (i = 0; i < n_devinf; i++) {
		ndmp9_device_info *	di9 = &devinf9[i];
		ndmp3_device_info *	di3 = &devinf3[i];

		NDMOS_MACRO_ZEROFILL (di3);

		CNVT_STRDUP_FROM_9 (di3, di9, model);

		di3->caplist.caplist_val =
			NDMOS_MACRO_NEWN (ndmp3_device_capability,
				di9->caplist.caplist_len);

		if (!di3->caplist.caplist_val) {
			return -1;
		}

		for (j = 0; j < di9->caplist.caplist_len; j++) {
			ndmp9_device_capability *	cap9;
			ndmp3_device_capability *	cap3;

			cap9 = &di9->caplist.caplist_val[j];
			cap3 = &di3->caplist.caplist_val[j];

			NDMOS_MACRO_ZEROFILL (cap3);

			CNVT_STRDUP_FROM_9 (cap3, cap9, device);

			ndmp_9to3_pval_vec_dup (
				cap9->capability.capability_val,
				&cap3->capability.capability_val,
				cap9->capability.capability_len);

			cap3->capability.capability_len =
				cap9->capability.capability_len;
		}
		di3->caplist.caplist_len = j;
	}

	return 0;
}


/*
 * ndmp_config_get_host_info
 * no args request
 */

int
ndmp_3to9_config_get_host_info_reply (
  ndmp3_config_get_host_info_reply *reply3,
  ndmp9_config_get_host_info_reply *reply9)
{
	int		n_error = 0;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_STRDUP_TO_9x (reply3, reply9, hostname, config_info.hostname);
	CNVT_STRDUP_TO_9x (reply3, reply9, os_type, config_info.os_type);
	CNVT_STRDUP_TO_9x (reply3, reply9, os_vers, config_info.os_vers);
	CNVT_STRDUP_TO_9x (reply3, reply9, hostid, config_info.hostid);

	return n_error;
}

int
ndmp_9to3_config_get_host_info_reply (
  ndmp9_config_get_host_info_reply *reply9,
  ndmp3_config_get_host_info_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_STRDUP_FROM_9x (reply3, reply9, hostname, config_info.hostname);
	CNVT_STRDUP_FROM_9x (reply3, reply9, os_type, config_info.os_type);
	CNVT_STRDUP_FROM_9x (reply3, reply9, os_vers, config_info.os_vers);
	CNVT_STRDUP_FROM_9x (reply3, reply9, hostid, config_info.hostid);

	return 0;
}



/*
 * ndmp_config_get_connection_type
 * no args request
 */

int
ndmp_3to9_config_get_connection_type_reply (
  ndmp3_config_get_connection_type_reply *reply3,
  ndmp9_config_get_connection_type_reply *reply9)
{
	int			n_error = 0;
	unsigned int		i;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	for (i = 0; i < reply3->addr_types.addr_types_len; i++) {
		switch (reply3->addr_types.addr_types_val[i]) {
		case NDMP3_ADDR_LOCAL:
			reply9->config_info.conntypes |=
				NDMP9_CONFIG_CONNTYPE_LOCAL;
			break;

		case NDMP3_ADDR_TCP:
			reply9->config_info.conntypes |=
				NDMP9_CONFIG_CONNTYPE_TCP;
			break;

		default:
			n_error++;
			/* ignore */
			break;
		}
	}

	return n_error;
}

int
ndmp_9to3_config_get_connection_type_reply (
  ndmp9_config_get_connection_type_reply *reply9,
  ndmp3_config_get_connection_type_reply *reply3)
{
	int			i = 0;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	reply3->addr_types.addr_types_val =
			NDMOS_MACRO_NEWN(ndmp3_addr_type, 3);
	if (!reply3->addr_types.addr_types_val) {
		return -1;	/* no mem */
	}

	i = 0;
	if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_LOCAL) {
		reply3->addr_types.addr_types_val[i++] = NDMP3_ADDR_LOCAL;
	}
	if (reply9->config_info.conntypes & NDMP9_CONFIG_CONNTYPE_TCP) {
		reply3->addr_types.addr_types_val[i++] = NDMP3_ADDR_TCP;
	}
	reply3->addr_types.addr_types_len = i;

	return 0;
}


/*
 * ndmp_config_get_auth_attr
 */

int
ndmp_3to9_config_get_auth_attr_request (
  struct ndmp3_config_get_auth_attr_request *request3,
  struct ndmp9_config_get_auth_attr_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, auth_type, ndmp_39_auth_type);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_TO_9 (request3, request9, auth_type);
		n_error++;
	}

	return n_error;
}

int
ndmp_9to3_config_get_auth_attr_request (
  struct ndmp9_config_get_auth_attr_request *request9,
  struct ndmp3_config_get_auth_attr_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, auth_type, ndmp_39_auth_type);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_FROM_9 (request3, request9, auth_type);
		n_error++;
	}

	return n_error;
}

int
ndmp_3to9_config_get_auth_attr_reply (
  struct ndmp3_config_get_auth_attr_reply *reply3,
  struct ndmp9_config_get_auth_attr_reply *reply9)
{
	int		n_error = 0;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_3to9_auth_attr (&reply3->server_attr,
					&reply9->server_attr);

	return n_error;
}

int
ndmp_9to3_config_get_auth_attr_reply (
  struct ndmp9_config_get_auth_attr_reply *reply9,
  struct ndmp3_config_get_auth_attr_reply *reply3)
{
	int		n_error = 0;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_9to3_auth_attr (&reply9->server_attr,
					&reply3->server_attr);

	return n_error;
}


/*
 * ndmp_config_get_butype_info
 * no args request
 */

int
ndmp_3to9_config_get_butype_info_reply (
  ndmp3_config_get_butype_info_reply *reply3,
  ndmp9_config_get_butype_info_reply *reply9)
{
	int		n;
	int		i;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	n = reply3->butype_info.butype_info_len;
	if (n == 0) {
		reply9->config_info.butype_info.butype_info_len = 0;
		reply9->config_info.butype_info.butype_info_val = 0;
		return 0;
	}

	reply9->config_info.butype_info.butype_info_val =
			NDMOS_MACRO_NEWN (ndmp9_butype_info, n);

	for (i = 0; i < n; i++) {
		ndmp9_butype_info *	bu9;
		ndmp3_butype_info *	bu3;

		bu3 = &reply3->butype_info.butype_info_val[i];
		bu9 = &reply9->config_info.butype_info.butype_info_val[i];

		NDMOS_MACRO_ZEROFILL (bu9);

		CNVT_STRDUP_TO_9 (bu3, bu9, butype_name);
		ndmp_3to9_pval_vec_dup (bu3->default_env.default_env_val,
					&bu9->default_env.default_env_val,
					bu3->default_env.default_env_len);

		bu9->default_env.default_env_len =
			bu3->default_env.default_env_len;

		bu9->v3attr.valid = NDMP9_VALIDITY_VALID;
		bu9->v3attr.value = bu3->attrs;
	}

	reply9->config_info.butype_info.butype_info_len = n;

	return 0;
}

int
ndmp_9to3_config_get_butype_info_reply (
  ndmp9_config_get_butype_info_reply *reply9,
  ndmp3_config_get_butype_info_reply *reply3)
{
	int		n;
	int		i;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	n = reply9->config_info.butype_info.butype_info_len;
	if (n == 0) {
		reply3->butype_info.butype_info_len = 0;
		reply3->butype_info.butype_info_val = 0;
		return 0;
	}

	reply3->butype_info.butype_info_val =
			NDMOS_MACRO_NEWN (ndmp3_butype_info, n);

	for (i = 0; i < n; i++) {
		ndmp3_butype_info *	bu3;
		ndmp9_butype_info *	bu9;

		bu9 = &reply9->config_info.butype_info.butype_info_val[i];
		bu3 = &reply3->butype_info.butype_info_val[i];

		NDMOS_MACRO_ZEROFILL (bu3);

		CNVT_STRDUP_FROM_9 (bu3, bu9, butype_name);
		ndmp_9to3_pval_vec_dup (bu9->default_env.default_env_val,
					&bu3->default_env.default_env_val,
					bu9->default_env.default_env_len);

		bu3->default_env.default_env_len =
			bu9->default_env.default_env_len;

		bu3->attrs = bu9->v3attr.value;
	}

	reply3->butype_info.butype_info_len = n;

	return 0;
}


/*
 * ndmp_config_get_fs_info
 * no args request
 */

int
ndmp_3to9_config_get_fs_info_reply (
  ndmp3_config_get_fs_info_reply *reply3,
  ndmp9_config_get_fs_info_reply *reply9)
{
	int		n;
	int		i;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	n = reply3->fs_info.fs_info_len;
	if (n == 0) {
		reply9->config_info.fs_info.fs_info_len = 0;
		reply9->config_info.fs_info.fs_info_val = 0;
		return 0;
	}

	reply9->config_info.fs_info.fs_info_val =
			NDMOS_MACRO_NEWN (ndmp9_fs_info, n);

	for (i = 0; i < n; i++) {
		ndmp3_fs_info *		fs3;
		ndmp9_fs_info *		fs9;

		fs3 = &reply3->fs_info.fs_info_val[i];
		fs9 = &reply9->config_info.fs_info.fs_info_val[i];

		NDMOS_MACRO_ZEROFILL (fs9);

		CNVT_STRDUP_TO_9 (fs3, fs9, fs_type);
		CNVT_STRDUP_TO_9 (fs3, fs9, fs_logical_device);
		CNVT_STRDUP_TO_9 (fs3, fs9, fs_physical_device);
		CNVT_STRDUP_TO_9 (fs3, fs9, fs_status);

		ndmp_3to9_pval_vec_dup (fs3->fs_env.fs_env_val,
					&fs9->fs_env.fs_env_val,
					fs3->fs_env.fs_env_len);

		fs9->fs_env.fs_env_len = fs3->fs_env.fs_env_len;
	}

	reply9->config_info.fs_info.fs_info_len = n;

	return 0;
}

int
ndmp_9to3_config_get_fs_info_reply (
  ndmp9_config_get_fs_info_reply *reply9,
  ndmp3_config_get_fs_info_reply *reply3)
{
	int		n;
	int		i;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	n = reply9->config_info.fs_info.fs_info_len;
	if (n == 0) {
		reply3->fs_info.fs_info_len = 0;
		reply3->fs_info.fs_info_val = 0;
		return 0;
	}

	reply3->fs_info.fs_info_val = NDMOS_MACRO_NEWN (ndmp3_fs_info, n);

	for (i = 0; i < n; i++) {
		ndmp9_fs_info *		fs9;
		ndmp3_fs_info *		fs3;

		fs9 = &reply9->config_info.fs_info.fs_info_val[i];
		fs3 = &reply3->fs_info.fs_info_val[i];

		NDMOS_MACRO_ZEROFILL (fs3);

		CNVT_STRDUP_FROM_9 (fs3, fs9, fs_type);
		CNVT_STRDUP_FROM_9 (fs3, fs9, fs_logical_device);
		CNVT_STRDUP_FROM_9 (fs3, fs9, fs_physical_device);
		CNVT_STRDUP_FROM_9 (fs3, fs9, fs_status);

		ndmp_9to3_pval_vec_dup (fs9->fs_env.fs_env_val,
					&fs3->fs_env.fs_env_val,
					fs9->fs_env.fs_env_len);

		fs3->fs_env.fs_env_len = fs9->fs_env.fs_env_len;
	}

	reply3->fs_info.fs_info_len = n;

	return 0;
}


/*
 * ndmp_config_get_tape_info
 * no args request
 */

int
ndmp_3to9_config_get_tape_info_reply (
  ndmp3_config_get_tape_info_reply *reply3,
  ndmp9_config_get_tape_info_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_3to9_device_info_vec_dup (
		reply3->tape_info.tape_info_val,
		&reply9->config_info.tape_info.tape_info_val,
		reply3->tape_info.tape_info_len);

	reply9->config_info.tape_info.tape_info_len =
			reply3->tape_info.tape_info_len;

	return 0;
}

int
ndmp_9to3_config_get_tape_info_reply (
  ndmp9_config_get_tape_info_reply *reply9,
  ndmp3_config_get_tape_info_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_9to3_device_info_vec_dup (
		reply9->config_info.tape_info.tape_info_val,
		&reply3->tape_info.tape_info_val,
		reply9->config_info.tape_info.tape_info_len);

	reply3->tape_info.tape_info_len =
		reply9->config_info.tape_info.tape_info_len;

	return 0;
}


/*
 * ndmp_config_get_scsi_info
 * no args request
 */

int
ndmp_3to9_config_get_scsi_info_reply (
  ndmp3_config_get_scsi_info_reply *reply3,
  ndmp9_config_get_scsi_info_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_3to9_device_info_vec_dup (
		reply3->scsi_info.scsi_info_val,
		&reply9->config_info.scsi_info.scsi_info_val,
		reply3->scsi_info.scsi_info_len);

	reply9->config_info.scsi_info.scsi_info_len =
		reply3->scsi_info.scsi_info_len;

	return 0;
}

int
ndmp_9to3_config_get_scsi_info_reply (
  ndmp9_config_get_scsi_info_reply *reply9,
  ndmp3_config_get_scsi_info_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_9to3_device_info_vec_dup (
		reply9->config_info.scsi_info.scsi_info_val,
		&reply3->scsi_info.scsi_info_val,
		reply9->config_info.scsi_info.scsi_info_len);

	reply3->scsi_info.scsi_info_len =
		reply9->config_info.scsi_info.scsi_info_len;

	return 0;
}


/*
 * ndmp_config_get_server_info
 * no args request
 */

int
ndmp_3to9_config_get_server_info_reply (
  ndmp3_config_get_server_info_reply *reply3,
  ndmp9_config_get_server_info_reply *reply9)
{
	unsigned int	i, n_error = 0;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_STRDUP_TO_9x (reply3, reply9,
			vendor_name, config_info.vendor_name);
	CNVT_STRDUP_TO_9x (reply3, reply9,
			product_name, config_info.product_name);
	CNVT_STRDUP_TO_9x (reply3, reply9,
			revision_number, config_info.revision_number);

	reply9->config_info.authtypes = 0;
	for (i = 0; i < reply3->auth_type.auth_type_len; i++) {
		switch (reply3->auth_type.auth_type_val[i]) {
		case NDMP3_AUTH_NONE:
			reply9->config_info.authtypes |=
				NDMP9_CONFIG_AUTHTYPE_NONE;
			break;

		case NDMP3_AUTH_TEXT:
			reply9->config_info.authtypes |=
				NDMP9_CONFIG_AUTHTYPE_TEXT;
			break;

		case NDMP3_AUTH_MD5:
			reply9->config_info.authtypes |=
				NDMP9_CONFIG_AUTHTYPE_MD5;
			break;

		default:
			n_error++;
			/* ignore */
			break;
		}
	}

	return n_error;
}

int
ndmp_9to3_config_get_server_info_reply (
  ndmp9_config_get_server_info_reply *reply9,
  ndmp3_config_get_server_info_reply *reply3)
{
	int			i = 0;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_STRDUP_FROM_9x (reply3, reply9,
			vendor_name, config_info.vendor_name);
	CNVT_STRDUP_FROM_9x (reply3, reply9,
			product_name, config_info.product_name);
	CNVT_STRDUP_FROM_9x (reply3, reply9,
			revision_number, config_info.revision_number);

	reply3->auth_type.auth_type_val = NDMOS_MACRO_NEWN(ndmp3_auth_type, 3);
	if (!reply3->auth_type.auth_type_val) {
		return -1;
	}

	i = 0;
	if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_NONE) {
		reply3->auth_type.auth_type_val[i++] = NDMP3_AUTH_NONE;
	}
	if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_TEXT) {
		reply3->auth_type.auth_type_val[i++] = NDMP3_AUTH_TEXT;
	}
	if (reply9->config_info.authtypes & NDMP9_CONFIG_AUTHTYPE_MD5) {
		reply3->auth_type.auth_type_val[i++] = NDMP3_AUTH_MD5;
	}
	reply3->auth_type.auth_type_len	= i;

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
int
ndmp_3to9_scsi_open_request (
  ndmp3_scsi_open_request *request3,
  ndmp9_scsi_open_request *request9)
{
	request9->device = NDMOS_API_STRDUP (request3->device);
	if (!request9->device) {
		return -1;	/* no memory */
	}
	return 0;
}

int
ndmp_9to3_scsi_open_request (
  ndmp9_scsi_open_request *request9,
  ndmp3_scsi_open_request *request3)
{
	request3->device = NDMOS_API_STRDUP (request9->device);
	if (!request3->device) {
		return -1;	/* no memory */
	}
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

int
ndmp_3to9_scsi_get_state_reply (
  ndmp3_scsi_get_state_reply *reply3,
  ndmp9_scsi_get_state_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_TO_9 (reply3, reply9, target_controller);
	CNVT_TO_9 (reply3, reply9, target_id);
	CNVT_TO_9 (reply3, reply9, target_lun);

	return 0;
}

int
ndmp_9to3_scsi_get_state_reply (
  ndmp9_scsi_get_state_reply *reply9,
  ndmp3_scsi_get_state_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_FROM_9 (reply3, reply9, target_controller);
	CNVT_FROM_9 (reply3, reply9, target_id);
	CNVT_FROM_9 (reply3, reply9, target_lun);

	return 0;
}

/*
 * ndmp_scsi_set_target -- deprecated
 * just error reply
 */

int
ndmp_3to9_scsi_set_target_request (
  ndmp3_scsi_set_target_request *request3,
  ndmp9_scsi_set_target_request *request9)
{
	request9->device = NDMOS_API_STRDUP (request3->device);
	if (!request9->device) {
		return -1;	/* no memory */
	}

	CNVT_TO_9 (request3, request9, target_controller);
	CNVT_TO_9 (request3, request9, target_id);
	CNVT_TO_9 (request3, request9, target_lun);

	return 0;
}

int
ndmp_9to3_scsi_set_target_request (
  ndmp9_scsi_set_target_request *request9,
  ndmp3_scsi_set_target_request *request3)
{
	request3->device = NDMOS_API_STRDUP (request9->device);
	if (!request3->device) {
		return -1;	/* no memory */
	}

	CNVT_FROM_9 (request3, request9, target_controller);
	CNVT_FROM_9 (request3, request9, target_id);
	CNVT_FROM_9 (request3, request9, target_lun);

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

int
ndmp_3to9_execute_cdb_request (
  ndmp3_execute_cdb_request *request3,
  ndmp9_execute_cdb_request *request9)
{
	int		n_error = 0;
	uint32_t	len;
	char *		p;

	switch (request3->flags) {
	case 0:
		request9->data_dir = NDMP9_SCSI_DATA_DIR_NONE;
		break;

	case NDMP3_SCSI_DATA_IN:
		request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
		break;

	case NDMP3_SCSI_DATA_OUT:
		request9->data_dir = NDMP9_SCSI_DATA_DIR_IN;
		break;

	default:
		/* deemed insolvable */
		n_error++;
		return -1;
	}

	CNVT_TO_9 (request3, request9, timeout);
	CNVT_TO_9 (request3, request9, datain_len);

	len = request3->dataout.dataout_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (request3->dataout.dataout_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	request9->dataout.dataout_len = len;
	request9->dataout.dataout_val = p;

	len = request3->cdb.cdb_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			if (request9->dataout.dataout_val) {
				NDMOS_API_FREE (request9->dataout.dataout_val);
				request9->dataout.dataout_len = 0;
				request9->dataout.dataout_val = 0;
			}
			return -1;
		}
		NDMOS_API_BCOPY (request3->cdb.cdb_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	request9->cdb.cdb_len = len;
	request9->cdb.cdb_val = p;

	return 0;
}

int
ndmp_9to3_execute_cdb_request (
  ndmp9_execute_cdb_request *request9,
  ndmp3_execute_cdb_request *request3)
{
	int		n_error = 0;
	uint32_t	len;
	char *		p;

	switch (request9->data_dir) {
	case NDMP9_SCSI_DATA_DIR_NONE:
		request3->flags = 0;
		break;

	case NDMP9_SCSI_DATA_DIR_IN:
		request3->flags = NDMP3_SCSI_DATA_IN;
		break;

	case NDMP9_SCSI_DATA_DIR_OUT:
		request3->flags = NDMP3_SCSI_DATA_OUT;
		break;

	default:
		/* deemed insolvable */
		n_error++;
		return -1;
	}

	CNVT_FROM_9 (request3, request9, timeout);
	CNVT_FROM_9 (request3, request9, datain_len);

	len = request9->dataout.dataout_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (request9->dataout.dataout_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	request3->dataout.dataout_len = len;
	request3->dataout.dataout_val = p;

	len = request9->cdb.cdb_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			if (request3->dataout.dataout_val) {
				NDMOS_API_FREE (request3->dataout.dataout_val);
				request3->dataout.dataout_len = 0;
				request3->dataout.dataout_val = 0;
			}
			return -1;
		}
		NDMOS_API_BCOPY (request9->cdb.cdb_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	request3->cdb.cdb_len = len;
	request3->cdb.cdb_val = p;

	return 0;
}

int
ndmp_3to9_execute_cdb_reply (
  ndmp3_execute_cdb_reply *reply3,
  ndmp9_execute_cdb_reply *reply9)
{
	uint32_t	len;
	char *		p;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_TO_9 (reply3, reply9, status);
	CNVT_TO_9 (reply3, reply9, dataout_len);

	len = reply3->datain.datain_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (reply3->datain.datain_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	reply9->datain.datain_len = len;
	reply9->datain.datain_val = p;

	len = reply3->ext_sense.ext_sense_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			if (reply9->datain.datain_val) {
				NDMOS_API_FREE (reply9->datain.datain_val);
				reply9->datain.datain_len = 0;
				reply9->datain.datain_val = 0;
			}
			return -1;
		}
		NDMOS_API_BCOPY (reply3->ext_sense.ext_sense_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	reply9->ext_sense.ext_sense_len = len;
	reply9->ext_sense.ext_sense_val = p;

	return 0;
}

int
ndmp_9to3_execute_cdb_reply (
  ndmp9_execute_cdb_reply *reply9,
  ndmp3_execute_cdb_reply *reply3)
{
	uint32_t	len;
	char *		p;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_FROM_9 (reply3, reply9, status);
	CNVT_FROM_9 (reply3, reply9, dataout_len);

	len = reply9->datain.datain_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (reply9->datain.datain_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	reply3->datain.datain_len = len;
	reply3->datain.datain_val = p;

	len = reply9->ext_sense.ext_sense_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			if (reply3->datain.datain_val) {
				NDMOS_API_FREE (reply9->datain.datain_val);
				reply3->datain.datain_len = 0;
				reply3->datain.datain_val = 0;
			}
			return -1;
		}
		NDMOS_API_BCOPY (reply9->ext_sense.ext_sense_val, p, len);
	} else {
		len = 0;
		p = 0;
	}
	reply3->ext_sense.ext_sense_len = len;
	reply3->ext_sense.ext_sense_val = p;

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

struct enum_conversion	ndmp_39_tape_open_mode[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_TAPE_READ_MODE,		NDMP9_TAPE_READ_MODE },
      { NDMP3_TAPE_RDWR_MODE,		NDMP9_TAPE_RDWR_MODE },
	END_ENUM_CONVERSION_TABLE
};



int
ndmp_3to9_tape_open_request (
  ndmp3_tape_open_request *request3,
  ndmp9_tape_open_request *request9)
{
	int		n_error = 0;
	int		rc;

	/*
	 * Mode codes are compatible between versions.
	 * We let untranslated values through to
	 * facilitate testing illegal values.
	 */
	rc = convert_enum_to_9 (ndmp_39_tape_open_mode, request3->mode);
	if (rc == NDMP_INVALID_GENERAL) {
		n_error++;
		request9->mode = request3->mode;
	} else {
		request9->mode = rc;
	}

	request9->device = NDMOS_API_STRDUP (request3->device);
	if (!request9->device) {
		return -1;	/* no memory */
	}

	return n_error;
}

int
ndmp_9to3_tape_open_request (
  ndmp9_tape_open_request *request9,
  ndmp3_tape_open_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = convert_enum_from_9 (ndmp_39_tape_open_mode, request9->mode);
	if (rc == NDMP_INVALID_GENERAL) {
		n_error++;
		request3->mode = request9->mode;
	} else {
		request3->mode = rc;
	}

	request3->device = NDMOS_API_STRDUP (request9->device);
	if (!request3->device) {
		return -1;	/* no memory */
	}

	return n_error;
}





/*
 * ndmp_tape_get_state_reply
 * no arg request
 */

extern int
ndmp_3to9_tape_get_state_reply (
  ndmp3_tape_get_state_reply *reply3,
  ndmp9_tape_get_state_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_TO_9 (reply3, reply9, flags);
	CNVT_VUL_TO_9 (reply3, reply9, file_num);
	CNVT_VUL_TO_9 (reply3, reply9, soft_errors);
	CNVT_VUL_TO_9 (reply3, reply9, block_size);
	CNVT_VUL_TO_9 (reply3, reply9, blockno);
	CNVT_VUQ_TO_9 (reply3, reply9, total_space);
	CNVT_VUQ_TO_9 (reply3, reply9, space_remain);

	if (reply3->invalid & NDMP3_TAPE_STATE_FILE_NUM_INVALID)
		CNVT_IUL_TO_9 (reply9, file_num);

	if (reply3->invalid & NDMP3_TAPE_STATE_SOFT_ERRORS_INVALID)
		CNVT_IUL_TO_9 (reply9, soft_errors);

	if (reply3->invalid & NDMP3_TAPE_STATE_BLOCK_SIZE_INVALID)
		CNVT_IUL_TO_9 (reply9, block_size);

	if (reply3->invalid & NDMP3_TAPE_STATE_BLOCKNO_INVALID)
		CNVT_IUL_TO_9 (reply9, blockno);

	if (reply3->invalid & NDMP3_TAPE_STATE_TOTAL_SPACE_INVALID)
		CNVT_IUQ_TO_9 (reply9, total_space);

	if (reply3->invalid & NDMP3_TAPE_STATE_SPACE_REMAIN_INVALID)
		CNVT_IUQ_TO_9 (reply9, space_remain);

	return 0;
}

extern int
ndmp_9to3_tape_get_state_reply (
  ndmp9_tape_get_state_reply *reply9,
  ndmp3_tape_get_state_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_FROM_9 (reply3, reply9, flags);
	CNVT_VUL_FROM_9 (reply3, reply9, file_num);
	CNVT_VUL_FROM_9 (reply3, reply9, soft_errors);
	CNVT_VUL_FROM_9 (reply3, reply9, block_size);
	CNVT_VUL_FROM_9 (reply3, reply9, blockno);
	CNVT_VUQ_FROM_9 (reply3, reply9, total_space);
	CNVT_VUQ_FROM_9 (reply3, reply9, space_remain);

	reply3->invalid = 0;

	if (!reply9->file_num.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_FILE_NUM_INVALID;

	if (!reply9->soft_errors.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_SOFT_ERRORS_INVALID;

	if (!reply9->block_size.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_BLOCK_SIZE_INVALID;

	if (!reply9->blockno.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_BLOCKNO_INVALID;

	if (!reply9->total_space.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_TOTAL_SPACE_INVALID;

	if (!reply9->space_remain.valid)
		reply3->invalid |= NDMP3_TAPE_STATE_SPACE_REMAIN_INVALID;

	reply3->invalid |= NDMP3_TAPE_STATE_PARTITION_INVALID;

	return 0;
}


/*
 * ndmp_tape_mtio_request
 */

struct enum_conversion	ndmp_39_tape_mtio_op[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_MTIO_FSF,			NDMP9_MTIO_FSF },
      { NDMP3_MTIO_BSF,			NDMP9_MTIO_BSF },
      { NDMP3_MTIO_FSR,			NDMP9_MTIO_FSR },
      { NDMP3_MTIO_BSR,			NDMP9_MTIO_BSR },
      { NDMP3_MTIO_REW,			NDMP9_MTIO_REW },
      { NDMP3_MTIO_EOF,			NDMP9_MTIO_EOF },
      { NDMP3_MTIO_OFF,			NDMP9_MTIO_OFF },
	END_ENUM_CONVERSION_TABLE
};


int
ndmp_3to9_tape_mtio_request (
  ndmp3_tape_mtio_request *request3,
  ndmp9_tape_mtio_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, tape_op, ndmp_39_tape_mtio_op);
	if (rc == NDMP_INVALID_GENERAL) {
		n_error++;
		CNVT_TO_9(request3, request9, tape_op);
	}

	CNVT_TO_9(request3, request9, count);

	return n_error;
}

int
ndmp_9to3_tape_mtio_request (
  ndmp9_tape_mtio_request *request9,
  ndmp3_tape_mtio_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, tape_op, ndmp_39_tape_mtio_op);
	if (rc == NDMP_INVALID_GENERAL) {
		n_error++;
		CNVT_FROM_9(request3, request9, tape_op);
	}

	CNVT_FROM_9(request3, request9, count);

	return n_error;
}

int
ndmp_3to9_tape_mtio_reply (
  ndmp3_tape_mtio_reply *reply3,
  ndmp9_tape_mtio_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_TO_9 (reply3, reply9, resid_count);
	return 0;
}

int
ndmp_9to3_tape_mtio_reply (
  ndmp9_tape_mtio_reply *reply9,
  ndmp3_tape_mtio_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_FROM_9 (reply3, reply9, resid_count);
	return 0;
}


/*
 * ndmp_tape_write
 */

int
ndmp_3to9_tape_write_request (
  ndmp3_tape_write_request *request3,
  ndmp9_tape_write_request *request9)
{
	uint32_t	len;
	char *		p;

	len = request3->data_out.data_out_len;

	p = NDMOS_API_MALLOC (len);
	if (!p) {
		return -1;
	}

	NDMOS_API_BCOPY (request3->data_out.data_out_val, p, len);

	request9->data_out.data_out_val = p;
	request9->data_out.data_out_len = len;

	return 0;
}

int
ndmp_9to3_tape_write_request (
  ndmp9_tape_write_request *request9,
  ndmp3_tape_write_request *request3)
{
	uint32_t	len;
	char *		p;

	len = request9->data_out.data_out_len;

	p = NDMOS_API_MALLOC (len);
	if (!p) {
		return -1;
	}

	NDMOS_API_BCOPY (request9->data_out.data_out_val, p, len);

	request3->data_out.data_out_val = p;
	request3->data_out.data_out_len = len;

	return 0;
}

int
ndmp_3to9_tape_write_reply (
  ndmp3_tape_write_reply *reply3,
  ndmp9_tape_write_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_TO_9 (reply3, reply9, count);
	return 0;
}

int
ndmp_9to3_tape_write_reply (
  ndmp9_tape_write_reply *reply9,
  ndmp3_tape_write_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_FROM_9 (reply3, reply9, count);
	return 0;
}


/*
 * ndmp_tape_read
 */

int
ndmp_3to9_tape_read_request (
  ndmp3_tape_read_request *request3,
  ndmp9_tape_read_request *request9)
{
	CNVT_TO_9 (request3, request9, count);
	return 0;
}

int
ndmp_9to3_tape_read_request (
  ndmp9_tape_read_request *request9,
  ndmp3_tape_read_request *request3)
{
	CNVT_FROM_9 (request3, request9, count);
	return 0;
}

int
ndmp_3to9_tape_read_reply (
  ndmp3_tape_read_reply *reply3,
  ndmp9_tape_read_reply *reply9)
{
	uint32_t	len;
	char *		p;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	len = reply3->data_in.data_in_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (reply3->data_in.data_in_val, p, len);
	} else {
		p = 0;
		len = 0;
	}

	reply9->data_in.data_in_len = len;
	reply9->data_in.data_in_val = p;

	return 0;
}

int
ndmp_9to3_tape_read_reply (
  ndmp9_tape_read_reply *reply9,
  ndmp3_tape_read_reply *reply3)
{
	uint32_t	len;
	char *		p;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	len = reply9->data_in.data_in_len;
	if (len > 0) {
		p = NDMOS_API_MALLOC (len);
		if (!p) {
			return -1;
		}
		NDMOS_API_BCOPY (reply9->data_in.data_in_val, p, len);
	} else {
		p = 0;
		len = 0;
	}

	reply3->data_in.data_in_len = len;
	reply3->data_in.data_in_val = p;

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


struct enum_conversion	ndmp_39_mover_mode[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_MOVER_MODE_READ,		NDMP9_MOVER_MODE_READ },
      { NDMP3_MOVER_MODE_WRITE,		NDMP9_MOVER_MODE_WRITE },

	END_ENUM_CONVERSION_TABLE
};

struct enum_conversion	ndmp_39_mover_state[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_MOVER_STATE_IDLE,		NDMP9_MOVER_STATE_IDLE },
      { NDMP3_MOVER_STATE_LISTEN,	NDMP9_MOVER_STATE_LISTEN },
      { NDMP3_MOVER_STATE_ACTIVE,	NDMP9_MOVER_STATE_ACTIVE },
      { NDMP3_MOVER_STATE_PAUSED,	NDMP9_MOVER_STATE_PAUSED },
      { NDMP3_MOVER_STATE_HALTED,	NDMP9_MOVER_STATE_HALTED },

	/* alias */
      { NDMP3_MOVER_STATE_ACTIVE,	NDMP9_MOVER_STATE_STANDBY },

	END_ENUM_CONVERSION_TABLE
};

struct enum_conversion	ndmp_39_mover_pause_reason[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_MOVER_PAUSE_NA,		NDMP9_MOVER_PAUSE_NA },
      { NDMP3_MOVER_PAUSE_EOM,		NDMP9_MOVER_PAUSE_EOM },
      { NDMP3_MOVER_PAUSE_EOF,		NDMP9_MOVER_PAUSE_EOF },
      { NDMP3_MOVER_PAUSE_SEEK,		NDMP9_MOVER_PAUSE_SEEK },
      { NDMP3_MOVER_PAUSE_MEDIA_ERROR,	NDMP9_MOVER_PAUSE_MEDIA_ERROR },
      { NDMP3_MOVER_PAUSE_EOW,		NDMP9_MOVER_PAUSE_EOW },
	END_ENUM_CONVERSION_TABLE
};

struct enum_conversion	ndmp_39_mover_halt_reason[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_MOVER_HALT_NA,		NDMP9_MOVER_HALT_NA },
      { NDMP3_MOVER_HALT_CONNECT_CLOSED, NDMP9_MOVER_HALT_CONNECT_CLOSED },
      { NDMP3_MOVER_HALT_ABORTED,	NDMP9_MOVER_HALT_ABORTED },
      { NDMP3_MOVER_HALT_INTERNAL_ERROR, NDMP9_MOVER_HALT_INTERNAL_ERROR },
      { NDMP3_MOVER_HALT_CONNECT_ERROR,	NDMP9_MOVER_HALT_CONNECT_ERROR },
	END_ENUM_CONVERSION_TABLE
};


extern int
ndmp_3to9_mover_get_state_reply (
  ndmp3_mover_get_state_reply *reply3,
  ndmp9_mover_get_state_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_E_TO_9 (reply3, reply9, state, ndmp_39_mover_state);
	CNVT_E_TO_9 (reply3, reply9, pause_reason, ndmp_39_mover_pause_reason);
	CNVT_E_TO_9 (reply3, reply9, halt_reason, ndmp_39_mover_halt_reason);

	CNVT_TO_9 (reply3, reply9, record_size);
	CNVT_TO_9 (reply3, reply9, record_num);
	CNVT_TO_9x (reply3, reply9, data_written, bytes_moved);
	CNVT_TO_9 (reply3, reply9, seek_position);
	CNVT_TO_9 (reply3, reply9, bytes_left_to_read);
	CNVT_TO_9 (reply3, reply9, window_offset);
	CNVT_TO_9 (reply3, reply9, window_length);

	ndmp_3to9_addr (&reply3->data_connection_addr,
					&reply9->data_connection_addr);

	return 0;
}

extern int
ndmp_9to3_mover_get_state_reply (
  ndmp9_mover_get_state_reply *reply9,
  ndmp3_mover_get_state_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_E_FROM_9 (reply3, reply9, state, ndmp_39_mover_state);
	CNVT_E_FROM_9 (reply3, reply9, pause_reason,
						ndmp_39_mover_pause_reason);
	CNVT_E_FROM_9 (reply3, reply9, halt_reason,
						ndmp_39_mover_halt_reason);

	CNVT_FROM_9 (reply3, reply9, record_size);
	CNVT_FROM_9 (reply3, reply9, record_num);
	CNVT_FROM_9x (reply3, reply9, data_written, bytes_moved);
	CNVT_FROM_9 (reply3, reply9, seek_position);
	CNVT_FROM_9 (reply3, reply9, bytes_left_to_read);
	CNVT_FROM_9 (reply3, reply9, window_offset);
	CNVT_FROM_9 (reply3, reply9, window_length);

	ndmp_9to3_addr (&reply9->data_connection_addr,
					&reply3->data_connection_addr);

	return 0;
}


/*
* ndmp_mover_listen
 */

int
ndmp_3to9_mover_listen_request (
  ndmp3_mover_listen_request *request3,
  ndmp9_mover_listen_request *request9)
{
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, mode, ndmp_39_mover_mode);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_TO_9 (request3, request9, mode);
	}
	rc = CNVT_E_TO_9 (request3, request9, addr_type, ndmp_39_addr_type);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_TO_9 (request3, request9, addr_type);
	}

	return 0;
}

int
ndmp_9to3_mover_listen_request (
  ndmp9_mover_listen_request *request9,
  ndmp3_mover_listen_request *request3)
{
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, mode, ndmp_39_mover_mode);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_FROM_9 (request3, request9, mode);
	}
	rc = CNVT_E_FROM_9 (request3, request9, addr_type, ndmp_39_addr_type);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_FROM_9 (request3, request9, addr_type);
	}

	return 0;
}

int
ndmp_3to9_mover_listen_reply (
  ndmp3_mover_listen_reply *reply3,
  ndmp9_mover_listen_reply *reply9)
{
	int		n_error = 0;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_3to9_addr (&reply3->data_connection_addr,
			&reply9->data_connection_addr);

	return n_error;
}

int
ndmp_9to3_mover_listen_reply (
  ndmp9_mover_listen_reply *reply9,
  ndmp3_mover_listen_reply *reply3)
{
	int		n_error = 0;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_9to3_addr (&reply9->data_connection_addr,
			&reply3->data_connection_addr);

	return n_error;
}

/*
 * ndmp_mover_connect
 * just error reply
 */

int
ndmp_3to9_mover_connect_request (
  ndmp3_mover_connect_request *request3,
  ndmp9_mover_connect_request *request9)
{
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, mode, ndmp_39_mover_mode);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_TO_9 (request3, request9, mode);
	}
	return ndmp_3to9_addr (&request3->addr, &request9->addr);
}

int
ndmp_9to3_mover_connect_request (
  ndmp9_mover_connect_request *request9,
  ndmp3_mover_connect_request *request3)
{
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, mode, ndmp_39_mover_mode);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_FROM_9 (request3, request9, mode);
	}
	return ndmp_9to3_addr (&request9->addr, &request3->addr);
}



/*
 * ndmp_mover_continue
 * no arg request, just error reply
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

int
ndmp_3to9_mover_set_window_request (
  ndmp3_mover_set_window_request *request3,
  ndmp9_mover_set_window_request *request9)
{
	CNVT_TO_9 (request3, request9, offset);
	CNVT_TO_9 (request3, request9, length);
	return 0;
}

int
ndmp_9to3_mover_set_window_request (
  ndmp9_mover_set_window_request *request9,
  ndmp3_mover_set_window_request *request3)
{
	CNVT_FROM_9 (request3, request9, offset);
	CNVT_FROM_9 (request3, request9, length);
	return 0;
}


/*
 * ndmp_mover_read
 * just error reply
 */

int
ndmp_3to9_mover_read_request (
  ndmp3_mover_read_request *request3,
  ndmp9_mover_read_request *request9)
{
	CNVT_TO_9 (request3, request9, offset);
	CNVT_TO_9 (request3, request9, length);
	return 0;
}

int
ndmp_9to3_mover_read_request (
  ndmp9_mover_read_request *request9,
  ndmp3_mover_read_request *request3)
{
	CNVT_FROM_9 (request3, request9, offset);
	CNVT_FROM_9 (request3, request9, length);
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

int
ndmp_3to9_mover_set_record_size_request (
  ndmp3_mover_set_record_size_request *request3,
  ndmp9_mover_set_record_size_request *request9)
{
	CNVT_TO_9x (request3, request9, len, record_size);
	return 0;
}

int
ndmp_9to3_mover_set_record_size_request (
  ndmp9_mover_set_record_size_request *request9,
  ndmp3_mover_set_record_size_request *request3)
{
	CNVT_FROM_9x (request3, request9, len, record_size);
	return 0;
}


/*
 * DATA INTERFACES
 ****************************************************************
 */


int
ndmp_3to9_name (
  ndmp3_name *name3,
  ndmp9_name *name9)
{
	char		buf[1024];
	int		cnt;

	cnt = sizeof(buf) - 1;
	name9->original_path = NDMOS_API_STRDUP(name3->original_path);
	if (name3->new_name && *name3->new_name && *name3->destination_dir) {
	    snprintf (buf, cnt, "%s/%s", name3->destination_dir, name3->new_name);
	} else if (name3->new_name && *name3->new_name) {
	    snprintf (buf, cnt, "/%s", name3->new_name);
	} else {
	    strncpy (buf, name3->destination_dir, cnt);
	}
	buf[cnt] = '\0';
	name9->destination_path = NDMOS_API_STRDUP(buf);

	/* per the following email's on the NDMP tech mailing on
	 * Apr. 10 & 11 2000 and Feb. 21, 2001  with matching
         * references in the V3.1.3 specification:
         *
         * V2 and V4 are close in behavior but V3 does the following:
         *
         * If new_name is not NULL then the destination path is
	 *     destination_path = destination_dir PLUS new_name
         *         otherwise
         *     destination_path = destination_dir PLUS original_path
         * and original_path is missing the trailing component.
         */
	if (name3->new_name && *name3->new_name) {
	    if (*name3->original_path) {
		snprintf (buf, cnt, "%s/%s", name3->original_path, name3->new_name);
	    } else {
		strncpy (buf, name3->new_name, cnt);
	    }
	    buf[cnt] = '\0';
	    name9->original_path = NDMOS_API_STRDUP(buf);
	} else {
	    name9->original_path = NDMOS_API_STRDUP(name3->original_path);
	}

	if (name3->new_name && *name3->new_name) {
	    if (*name3->destination_dir) {
		snprintf (buf, cnt, "%s/%s", name3->destination_dir, name3->new_name);
	    } else {
		strncpy (buf, name3->new_name, cnt);
	    }
	    buf[cnt] = '\0';
	    name9->original_path = NDMOS_API_STRDUP(buf);
	} else {
	    if (*name3->destination_dir) {
		snprintf (buf, cnt, "%s/%s", name3->destination_dir, name3->original_path);
	    } else {
		strncpy (buf, name3->original_path, cnt);
	    }
	    buf[cnt] = '\0';
	}
	name9->destination_path = NDMOS_API_STRDUP(buf);

	name9->other_name = NDMOS_API_STRDUP (name3->other_name);
	name9->node = name3->node;

	if (name3->fh_info != NDMP_INVALID_U_QUAD) {
		name9->fh_info.valid = NDMP9_VALIDITY_VALID;
		name9->fh_info.value = name3->fh_info;
	} else {
		name9->fh_info.valid = NDMP9_VALIDITY_INVALID;
		name9->fh_info.value = NDMP_INVALID_U_QUAD;
	}

	return 0;
}

int
ndmp_9to3_name (
  ndmp9_name *name9,
  ndmp3_name *name3)
{
	char		buf[1024];
	int		olen, dlen, offset;

	/* see comment in ndmp_3to9_name() above */

	if (!strcmp(name9->original_path,".")) {
	    // special case
	    name3->original_path = NDMOS_API_STRDUP(name9->original_path);
	    name3->destination_dir = NDMOS_API_STRDUP(name9->destination_path);
	    name3->new_name = NDMOS_API_STRDUP("");
	} else {
	    olen = strlen(name9->original_path);
	    dlen = strlen(name9->destination_path);
	    offset = dlen - olen;
	    if ((olen < dlen) && (!strcmp(name9->original_path,
					  &name9->destination_path[offset]))) {
		/* original path part of destination path */
		name3->original_path = NDMOS_API_STRDUP(name9->original_path);
		*buf = 0;
		strncat(buf, name9->destination_path, offset);
		name3->destination_dir = NDMOS_API_STRDUP(buf);
		name3->new_name = NDMOS_API_STRDUP("");
	    } else {
		name3->original_path = NDMOS_API_STRDUP(name9->original_path);
		name3->destination_dir = NDMOS_API_STRDUP("");
		name3->new_name = NDMOS_API_STRDUP(name9->destination_path);
	    }
	}

	name3->other_name = NDMOS_API_STRDUP (name9->other_name);
	name3->node = name9->node;

	if (name9->fh_info.valid == NDMP9_VALIDITY_VALID) {
		name3->fh_info = name9->fh_info.value;
	} else {
		name3->fh_info = NDMP_INVALID_U_QUAD;
	}

	return 0;
}

int
ndmp_3to9_name_vec (
  ndmp3_name *name3,
  ndmp9_name *name9,
  unsigned n_name)
{
	unsigned int	i;

	for (i = 0; i < n_name; i++)
		ndmp_3to9_name (&name3[i], &name9[i]);

	return 0;
}

int
ndmp_9to3_name_vec (
  ndmp9_name *name9,
  ndmp3_name *name3,
  unsigned n_name)
{
	unsigned int	i;

	for (i = 0; i < n_name; i++)
		ndmp_9to3_name (&name9[i], &name3[i]);

	return 0;
}

int
ndmp_3to9_name_vec_dup (
  ndmp3_name *name3,
  ndmp9_name **name9_p,
  unsigned n_name)
{
	*name9_p = NDMOS_MACRO_NEWN (ndmp9_name, n_name);
	if (!*name9_p)
		return -1;

	return ndmp_3to9_name_vec (name3, *name9_p, n_name);
}

int
ndmp_9to3_name_vec_dup (
  ndmp9_name *name9,
  ndmp3_name **name3_p,
  unsigned n_name)
{
	*name3_p = NDMOS_MACRO_NEWN (ndmp3_name, n_name);
	if (!*name3_p)
		return -1;

	return ndmp_9to3_name_vec (name9, *name3_p, n_name);
}

/*
 * ndmp_data_get_state
 * no args request
 */

struct enum_conversion	ndmp_39_data_operation[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_DATA_OP_NOACTION,		NDMP9_DATA_OP_NOACTION },
      { NDMP3_DATA_OP_BACKUP,		NDMP9_DATA_OP_BACKUP },
      { NDMP3_DATA_OP_RESTORE,		NDMP9_DATA_OP_RECOVER },
      { NDMP3_DATA_OP_RESTORE_FILEHIST,	NDMP9_DATA_OP_RECOVER_FILEHIST },
	END_ENUM_CONVERSION_TABLE
};

struct enum_conversion	ndmp_39_data_state[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_DATA_STATE_IDLE,		NDMP9_DATA_STATE_IDLE },
      { NDMP3_DATA_STATE_ACTIVE,	NDMP9_DATA_STATE_ACTIVE },
      { NDMP3_DATA_STATE_HALTED,	NDMP9_DATA_STATE_HALTED },
      { NDMP3_DATA_STATE_CONNECTED,	NDMP9_DATA_STATE_CONNECTED },
      { NDMP3_DATA_STATE_LISTEN,	NDMP9_DATA_STATE_LISTEN },

	END_ENUM_CONVERSION_TABLE
};

struct enum_conversion	ndmp_39_data_halt_reason[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_DATA_HALT_NA,		NDMP9_DATA_HALT_NA },
      { NDMP3_DATA_HALT_SUCCESSFUL,	NDMP9_DATA_HALT_SUCCESSFUL },
      { NDMP3_DATA_HALT_ABORTED,	NDMP9_DATA_HALT_ABORTED },
      { NDMP3_DATA_HALT_INTERNAL_ERROR,	NDMP9_DATA_HALT_INTERNAL_ERROR },
      { NDMP3_DATA_HALT_CONNECT_ERROR,	NDMP9_DATA_HALT_CONNECT_ERROR },
	END_ENUM_CONVERSION_TABLE
};

extern int
ndmp_3to9_data_get_state_reply (
  ndmp3_data_get_state_reply *reply3,
  ndmp9_data_get_state_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_E_TO_9 (reply3, reply9, operation, ndmp_39_data_operation);
	CNVT_E_TO_9 (reply3, reply9, state, ndmp_39_data_state);
	CNVT_E_TO_9 (reply3, reply9, halt_reason, ndmp_39_data_halt_reason);

	CNVT_TO_9 (reply3, reply9, bytes_processed);

	CNVT_VUQ_TO_9 (reply3, reply9, est_bytes_remain);
	CNVT_VUL_TO_9 (reply3, reply9, est_time_remain);

	ndmp_3to9_addr (&reply3->data_connection_addr,
					&reply9->data_connection_addr);

	CNVT_TO_9 (reply3, reply9, read_offset);
	CNVT_TO_9 (reply3, reply9, read_length);

	return 0;
}

extern int
ndmp_9to3_data_get_state_reply (
  ndmp9_data_get_state_reply *reply9,
  ndmp3_data_get_state_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);
	CNVT_E_FROM_9 (reply3, reply9, operation, ndmp_39_data_operation);
	CNVT_E_FROM_9 (reply3, reply9, state, ndmp_39_data_state);
	CNVT_E_FROM_9 (reply3, reply9, halt_reason, ndmp_39_data_halt_reason);

	CNVT_FROM_9 (reply3, reply9, bytes_processed);

	CNVT_VUQ_FROM_9 (reply3, reply9, est_bytes_remain);
	CNVT_VUL_FROM_9 (reply3, reply9, est_time_remain);

	ndmp_9to3_addr (&reply9->data_connection_addr,
					&reply3->data_connection_addr);

	CNVT_FROM_9 (reply3, reply9, read_offset);
	CNVT_FROM_9 (reply3, reply9, read_length);

	return 0;
}


/*
 * ndmp_data_start_backup
 * just error reply
 */

int
ndmp_3to9_data_start_backup_request (
  ndmp3_data_start_backup_request *request3,
  ndmp9_data_start_backup_request *request9)
{
	int		n_error = 0;

	CNVT_STRDUP_TO_9 (request3, request9, bu_type);

	ndmp_3to9_pval_vec_dup (request3->env.env_val,
				&request9->env.env_val,
				request3->env.env_len);

	request9->env.env_len = request3->env.env_len;

	request9->addr.addr_type = NDMP9_ADDR_AS_CONNECTED;

	return n_error;
}

int
ndmp_9to3_data_start_backup_request (
  ndmp9_data_start_backup_request *request9,
  ndmp3_data_start_backup_request *request3)
{
	int		n_error = 0;

	CNVT_STRDUP_FROM_9 (request3, request9, bu_type);

	ndmp_9to3_pval_vec_dup (request9->env.env_val,
				&request3->env.env_val,
				request9->env.env_len);

	request3->env.env_len = request9->env.env_len;

	return n_error;
}


/*
 * ndmp_data_start_recover
 * ndmp_data_start_recover_filehist
 * just error reply
 */

int
ndmp_3to9_data_start_recover_request (
  ndmp3_data_start_recover_request *request3,
  ndmp9_data_start_recover_request *request9)
{
	int		n_error = 0;

	CNVT_STRDUP_TO_9 (request3, request9, bu_type);

	ndmp_3to9_pval_vec_dup (request3->env.env_val,
				&request9->env.env_val,
				request3->env.env_len);

	request9->env.env_len = request3->env.env_len;

	ndmp_3to9_name_vec_dup (request3->nlist.nlist_val,
				&request9->nlist.nlist_val,
				request3->nlist.nlist_len);

	request9->nlist.nlist_len = request3->nlist.nlist_len;

	request9->addr.addr_type = NDMP9_ADDR_AS_CONNECTED;

	return n_error;
}

int
ndmp_9to3_data_start_recover_request (
  ndmp9_data_start_recover_request *request9,
  ndmp3_data_start_recover_request *request3)
{
	int		n_error = 0;

	CNVT_STRDUP_FROM_9 (request3, request9, bu_type);

	ndmp_9to3_pval_vec_dup (request9->env.env_val,
				&request3->env.env_val,
				request9->env.env_len);

	request3->env.env_len = request9->env.env_len;

	ndmp_9to3_name_vec_dup (request9->nlist.nlist_val,
				&request3->nlist.nlist_val,
				request9->nlist.nlist_len);

	request3->nlist.nlist_len = request9->nlist.nlist_len;


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

int
ndmp_3to9_data_get_env_reply (
  ndmp3_data_get_env_reply *reply3,
  ndmp9_data_get_env_reply *reply9)
{
	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_3to9_pval_vec_dup (reply3->env.env_val,
				&reply9->env.env_val,
				reply3->env.env_len);

	reply9->env.env_len = reply3->env.env_len;

	return 0;
}

int
ndmp_9to3_data_get_env_reply (
  ndmp9_data_get_env_reply *reply9,
  ndmp3_data_get_env_reply *reply3)
{
	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	ndmp_9to3_pval_vec_dup (reply9->env.env_val,
				&reply3->env.env_val,
				reply9->env.env_len);

	reply3->env.env_len = reply9->env.env_len;

	return 0;
}


/*
 * ndmp_data_stop
 * no args request, just error reply
 */

/*
 * ndmp_data_listen
 */

int
ndmp_3to9_data_listen_request (
  ndmp3_data_listen_request *request3,
  ndmp9_data_listen_request *request9)
{
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, addr_type, ndmp_39_addr_type);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_TO_9 (request3, request9, addr_type);
	}

	return 0;
}

int
ndmp_9to3_data_listen_request (
  ndmp9_data_listen_request *request9,
  ndmp3_data_listen_request *request3)
{
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, addr_type, ndmp_39_addr_type);
	if (rc == NDMP_INVALID_GENERAL) {
		 CNVT_FROM_9 (request3, request9, addr_type);
	}

	return 0;
}

int
ndmp_3to9_data_listen_reply (
  ndmp3_data_listen_reply *reply3,
  ndmp9_data_listen_reply *reply9)
{
	int		n_error = 0;

	CNVT_E_TO_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_3to9_addr (&reply3->data_connection_addr,
			&reply9->data_connection_addr);

	return n_error;
}

int
ndmp_9to3_data_listen_reply (
  ndmp9_data_listen_reply *reply9,
  ndmp3_data_listen_reply *reply3)
{
	int		n_error = 0;

	CNVT_E_FROM_9 (reply3, reply9, error, ndmp_39_error);

	n_error += ndmp_9to3_addr (&reply9->data_connection_addr,
			&reply3->data_connection_addr);

	return n_error;
}


/*
 * ndmp_data_connect
 * just error reply
 */

int
ndmp_3to9_data_connect_request (
  ndmp3_data_connect_request *request3,
  ndmp9_data_connect_request *request9)
{
	return ndmp_3to9_addr (&request3->addr, &request9->addr);
}

int
ndmp_9to3_data_connect_request (
  ndmp9_data_connect_request *request9,
  ndmp3_data_connect_request *request3)
{
	return ndmp_9to3_addr (&request9->addr, &request3->addr);
}




/*
 * NOTIFY INTERFACES
 ****************************************************************
 */

/*
 * ndmp_notify_data_halted
 * just error reply
 */

int
ndmp_3to9_notify_data_halted_request (
  ndmp3_notify_data_halted_request *request3,
  ndmp9_notify_data_halted_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, reason,
				ndmp_39_data_halt_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_TO_9 (request3, request9, reason);
		n_error++;
	}

	return n_error;
}

int
ndmp_9to3_notify_data_halted_request (
  ndmp9_notify_data_halted_request *request9,
  ndmp3_notify_data_halted_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, reason,
				ndmp_39_data_halt_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_FROM_9 (request3, request9, reason);
		n_error++;
	}

	request3->text_reason = NDMOS_API_STRDUP("whatever");

	return n_error;
}


/*
 * ndmp_notify_connected
 * just error reply
 */

/* NDMP3_NOTIFY_CONNECTED */
struct enum_conversion	ndmp_39_connect_reason[] = {
      { NDMP_INVALID_GENERAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_CONNECTED,		NDMP9_CONNECTED },
      { NDMP3_SHUTDOWN,			NDMP9_SHUTDOWN },
      { NDMP3_REFUSED,			NDMP9_REFUSED },
	END_ENUM_CONVERSION_TABLE
};

int
ndmp_3to9_notify_connected_request (
  ndmp3_notify_connected_request *request3,
  ndmp9_notify_connected_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, reason, ndmp_39_connect_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_TO_9 (request3, request9, reason);
		n_error++;
	}

	CNVT_TO_9 (request3, request9, protocol_version);

	CNVT_STRDUP_TO_9 (request3, request9, text_reason);

	return n_error;
}

int
ndmp_9to3_notify_connected_request (
  ndmp9_notify_connected_request *request9,
  ndmp3_notify_connected_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9(request3, request9, reason, ndmp_39_connect_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_FROM_9 (request3, request9, reason);
		n_error++;
	}

	CNVT_FROM_9 (request3, request9, protocol_version);

	CNVT_STRDUP_FROM_9 (request3, request9, text_reason);

	return n_error;
}


/*
 * ndmp_notify_mover_halted
 * just error reply
 */

int
ndmp_3to9_notify_mover_halted_request (
  ndmp3_notify_mover_halted_request *request3,
  ndmp9_notify_mover_halted_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, reason,
				ndmp_39_mover_halt_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_TO_9 (request3, request9, reason);
		n_error++;
	}

	return n_error;
}

int
ndmp_9to3_notify_mover_halted_request (
  ndmp9_notify_mover_halted_request *request9,
  ndmp3_notify_mover_halted_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, reason,
				ndmp_39_mover_halt_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_FROM_9 (request3, request9, reason);
		n_error++;
	}

	request3->text_reason = NDMOS_API_STRDUP ("Whatever");

	return n_error;
}


/*
 * ndmp_notify_mover_paused
 * just error reply
 */

int
ndmp_3to9_notify_mover_paused_request (
  ndmp3_notify_mover_paused_request *request3,
  ndmp9_notify_mover_paused_request *request9)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_TO_9 (request3, request9, reason,
				ndmp_39_mover_pause_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_TO_9 (request3, request9, reason);
		n_error++;
	}

	CNVT_TO_9 (request3, request9, seek_position);

	return n_error;
}

int
ndmp_9to3_notify_mover_paused_request (
  ndmp9_notify_mover_paused_request *request9,
  ndmp3_notify_mover_paused_request *request3)
{
	int		n_error = 0;
	int		rc;

	rc = CNVT_E_FROM_9 (request3, request9, reason,
				ndmp_39_mover_pause_reason);
	if (rc == NDMP_INVALID_GENERAL) {
		CNVT_FROM_9 (request3, request9, reason);
		n_error++;
	}

	CNVT_FROM_9 (request3, request9, seek_position);

	return n_error;
}


/*
 * ndmp_notify_data_read
 * just error reply
 */

int
ndmp_3to9_notify_data_read_request (
  ndmp3_notify_data_read_request *request3,
  ndmp9_notify_data_read_request *request9)
{
	CNVT_TO_9 (request3, request9, offset);
	CNVT_TO_9 (request3, request9, length);
	return 0;
}

int
ndmp_9to3_notify_data_read_request (
  ndmp9_notify_data_read_request *request9,
  ndmp3_notify_data_read_request *request3)
{
	CNVT_FROM_9 (request3, request9, offset);
	CNVT_FROM_9 (request3, request9, length);
	return 0;
}




/*
 * LOGGING INTERFACES
 ****************************************************************
 */

struct enum_conversion	ndmp_39_recovery_status[] = {
      { NDMP3_UNDEFINED_ERR,
	NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR }, /* default */
      { NDMP3_NO_ERR,
	NDMP9_RECOVERY_SUCCESSFUL },
      { NDMP3_PERMISSION_ERR,
	NDMP9_RECOVERY_FAILED_PERMISSION },
      { NDMP3_FILE_NOT_FOUND_ERR,
	NDMP9_RECOVERY_FAILED_NOT_FOUND },
      { NDMP3_BAD_FILE_ERR,
	NDMP9_RECOVERY_FAILED_NO_DIRECTORY },
      { NDMP3_NO_MEM_ERR,
	NDMP9_RECOVERY_FAILED_OUT_OF_MEMORY },
      { NDMP3_IO_ERR,
	NDMP9_RECOVERY_FAILED_IO_ERROR },
      { NDMP3_UNDEFINED_ERR,
	NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR },
	END_ENUM_CONVERSION_TABLE
};


int
ndmp_3to9_log_file_request (
  ndmp3_log_file_request *request3,
  ndmp9_log_file_request *request9)
{
	request9->recovery_status =
		convert_enum_to_9 (ndmp_39_recovery_status, request3->error);
	CNVT_STRDUP_TO_9 (request3, request9, name);
	return 0;
}

int
ndmp_9to3_log_file_request (
  ndmp9_log_file_request *request9,
  ndmp3_log_file_request *request3)
{
	request3->error = convert_enum_from_9 (ndmp_39_recovery_status,
					request9->recovery_status);
	CNVT_STRDUP_FROM_9 (request3, request9, name);
	return 0;
}


/*
 * ndmp_log_type
 */

struct enum_conversion	ndmp_39_log_type[] = {
      { NDMP3_LOG_NORMAL,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_LOG_NORMAL,		NDMP9_LOG_NORMAL },
      { NDMP3_LOG_DEBUG,		NDMP9_LOG_DEBUG },
      { NDMP3_LOG_ERROR,		NDMP9_LOG_ERROR },
      { NDMP3_LOG_WARNING,		NDMP9_LOG_WARNING },
	END_ENUM_CONVERSION_TABLE
};



int
ndmp_3to9_log_message_request (
  ndmp3_log_message_request *request3,
  ndmp9_log_message_request *request9)
{
	CNVT_E_TO_9 (request3, request9, log_type, ndmp_39_log_type);
	CNVT_TO_9 (request3, request9, message_id);
	CNVT_STRDUP_TO_9 (request3, request9, entry);

	request9->associated_message_sequence.valid = NDMP9_VALIDITY_INVALID;
	request9->associated_message_sequence.value = NDMP9_INVALID_U_LONG;

	return 0;
}

int
ndmp_9to3_log_message_request (
  ndmp9_log_message_request *request9,
  ndmp3_log_message_request *request3)
{
	CNVT_E_FROM_9 (request3, request9, log_type, ndmp_39_log_type);
	CNVT_FROM_9 (request3, request9, message_id);
	CNVT_STRDUP_TO_9 (request3, request9, entry);

	return 0;
}




/*
 * FILE HISTORY INTERFACES
 ****************************************************************
 */


/*
 * ndmp[_unix]_file_stat
 */

struct enum_conversion	ndmp_39_file_type[] = {
      { NDMP3_FILE_OTHER,		NDMP_INVALID_GENERAL, }, /* default */
      { NDMP3_FILE_DIR,			NDMP9_FILE_DIR },
      { NDMP3_FILE_FIFO,		NDMP9_FILE_FIFO },
      { NDMP3_FILE_CSPEC,		NDMP9_FILE_CSPEC },
      { NDMP3_FILE_BSPEC,		NDMP9_FILE_BSPEC },
      { NDMP3_FILE_REG,			NDMP9_FILE_REG },
      { NDMP3_FILE_SLINK,		NDMP9_FILE_SLINK },
      { NDMP3_FILE_SOCK,		NDMP9_FILE_SOCK },
      { NDMP3_FILE_REGISTRY,		NDMP9_FILE_REGISTRY },
      { NDMP3_FILE_OTHER,		NDMP9_FILE_OTHER },
	END_ENUM_CONVERSION_TABLE
};

extern int
ndmp_3to9_file_stat (
  ndmp3_file_stat *fstat3,
  ndmp9_file_stat *fstat9,
  ndmp9_u_quad node,
  ndmp9_u_quad fh_info)
{
	CNVT_E_TO_9 (fstat3, fstat9, ftype, ndmp_39_file_type);

	CNVT_VUL_TO_9 (fstat3, fstat9, mtime);
	CNVT_VUL_TO_9 (fstat3, fstat9, atime);
	CNVT_VUL_TO_9 (fstat3, fstat9, ctime);
	CNVT_VUL_TO_9x (fstat3, fstat9, owner, uid);
	CNVT_VUL_TO_9x (fstat3, fstat9, group, gid);

	CNVT_VUL_TO_9x (fstat3, fstat9, fattr, mode);

	CNVT_VUQ_TO_9 (fstat3, fstat9, size);

	CNVT_VUL_TO_9 (fstat3, fstat9, links);

	convert_valid_u_quad_to_9 (&node, &fstat9->node);
	convert_valid_u_quad_to_9 (&fh_info, &fstat9->fh_info);

	if (fstat3->invalid & NDMP3_FILE_STAT_ATIME_INVALID)
		CNVT_IUL_TO_9 (fstat9, atime);

	if (fstat3->invalid & NDMP3_FILE_STAT_CTIME_INVALID)
		CNVT_IUL_TO_9 (fstat9, ctime);

	if (fstat3->invalid & NDMP3_FILE_STAT_GROUP_INVALID)
		CNVT_IUL_TO_9 (fstat9, gid);

	return 0;
}

extern int
ndmp_9to3_file_stat (
  ndmp9_file_stat *fstat9,
  ndmp3_file_stat *fstat3)
{
	CNVT_E_FROM_9 (fstat3, fstat9, ftype, ndmp_39_file_type);

	fstat3->fs_type = NDMP3_FS_UNIX;

	CNVT_VUL_FROM_9 (fstat3, fstat9, mtime);
	CNVT_VUL_FROM_9 (fstat3, fstat9, atime);
	CNVT_VUL_FROM_9 (fstat3, fstat9, ctime);
	CNVT_VUL_FROM_9x (fstat3, fstat9, owner, uid);
	CNVT_VUL_FROM_9x (fstat3, fstat9, group, gid);

	CNVT_VUL_FROM_9x (fstat3, fstat9, fattr, mode);

	CNVT_VUQ_FROM_9 (fstat3, fstat9, size);

	CNVT_VUL_FROM_9 (fstat3, fstat9, links);

	fstat3->invalid = 0;

	if (!fstat9->atime.valid)
		fstat3->invalid |= NDMP3_FILE_STAT_ATIME_INVALID;

	if (!fstat9->ctime.valid)
		fstat3->invalid |= NDMP3_FILE_STAT_CTIME_INVALID;

	if (!fstat9->gid.valid)
		fstat3->invalid |= NDMP3_FILE_STAT_GROUP_INVALID;

	/* fh_info ignored */
	/* node ignored */

	return 0;
}


/*
 * ndmp_fh_add_file_request
 */

int
ndmp_3to9_fh_add_file_request (
  ndmp3_fh_add_file_request *request3,
  ndmp9_fh_add_file_request *request9)
{
	int			n_ent = request3->files.files_len;
	unsigned int		j;
	int			i;
	ndmp9_file *		table;

	table = NDMOS_MACRO_NEWN(ndmp9_file, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp3_file *		ent3 = &request3->files.files_val[i];
		ndmp3_file_name *	file_name;
		ndmp3_file_stat *	file_stat = 0;
		ndmp3_file_stat		_file_stat;
		char *			filename;
		ndmp9_file *		ent9 = &table[i];

		filename = "no-unix-name";
		for (j = 0; j < ent3->names.names_len; j++) {
			file_name = &ent3->names.names_val[j];
			if (file_name->fs_type == NDMP3_FS_UNIX) {
				filename =
				    file_name->ndmp3_file_name_u.unix_name;
				break;
			}
		}
		for (j = 0; j < ent3->stats.stats_len; j++) {
			file_stat = &ent3->stats.stats_val[j];
			if (file_stat->fs_type == NDMP3_FS_UNIX) {
				break;
			}
		}
		if (j >= ent3->stats.stats_len) {
			file_stat = &_file_stat;
			NDMOS_MACRO_ZEROFILL (file_stat);
		}

		ent9->unix_path = NDMOS_API_STRDUP(filename);
		ndmp_3to9_file_stat (file_stat, &ent9->fstat,
					ent3->node, ent3->fh_info);
	}

	request9->files.files_len = n_ent;
	request9->files.files_val = table;

	return 0;
}

int
ndmp_9to3_fh_add_file_request (
  ndmp9_fh_add_file_request *request9,
  ndmp3_fh_add_file_request *request3)
{
	int			n_ent = request9->files.files_len;
	int			i;
	ndmp3_file *		table;

	table = NDMOS_MACRO_NEWN(ndmp3_file, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp9_file *		ent9 = &request9->files.files_val[i];
		ndmp3_file *		ent3 = &table[i];

		ent3->names.names_val = NDMOS_MACRO_NEW(ndmp3_file_name);
		ent3->names.names_len = 1;
		ent3->stats.stats_val = NDMOS_MACRO_NEW(ndmp3_file_stat);
		ent3->stats.stats_len = 1;

		ent3->names.names_val[0].fs_type = NDMP3_FS_UNIX;
		ent3->names.names_val[0].ndmp3_file_name_u.unix_name =
				NDMOS_API_STRDUP(ent9->unix_path);

		ndmp_9to3_file_stat (&ent9->fstat,
				&ent3->stats.stats_val[0]);
		ent3->node = ent9->fstat.node.value;
		ent3->fh_info = ent9->fstat.fh_info.value;
	}

	request3->files.files_len = n_ent;
	request3->files.files_val = table;

	return 0;
}


/*
 * ndmp_fh_add_unix_dir
 */

int
ndmp_3to9_fh_add_dir_request (
  ndmp3_fh_add_dir_request *request3,
  ndmp9_fh_add_dir_request *request9)
{
	int			n_ent = request3->dirs.dirs_len;
	int			i;
	unsigned int		j;
	ndmp9_dir *		table;

	table = NDMOS_MACRO_NEWN(ndmp9_dir, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp3_dir *		ent3 = &request3->dirs.dirs_val[i];
		ndmp3_file_name *	file_name;
		char *			filename;
		ndmp9_dir *		ent9 = &table[i];

		filename = "no-unix-name";
		for (j = 0; j < ent3->names.names_len; j++) {
			file_name = &ent3->names.names_val[j];
			if (file_name->fs_type == NDMP3_FS_UNIX) {
				filename =
				    file_name->ndmp3_file_name_u.unix_name;
				break;
			}
		}

		ent9->unix_name = NDMOS_API_STRDUP(filename);
		ent9->node = ent3->node;
		ent9->parent = ent3->parent;
	}

	request9->dirs.dirs_len = n_ent;
	request9->dirs.dirs_val = table;

	return 0;
}

int
ndmp_3to9_fh_add_dir_free_request (ndmp9_fh_add_dir_request *request9)
{
    int	i;

    if (request9) {
	if(request9->dirs.dirs_val) {
	    int n_ent = request9->dirs.dirs_len;

	    for (i = 0; i < n_ent; i++) {
		ndmp9_dir *ent9 = &request9->dirs.dirs_val[i];
		if (ent9->unix_name)
		    NDMOS_API_FREE(ent9->unix_name);
		ent9->unix_name = 0;
	    }

	    NDMOS_API_FREE(request9->dirs.dirs_val);
	}
	request9->dirs.dirs_val = 0;
    }
    return 0;
}

int
ndmp_9to3_fh_add_dir_request (
  ndmp9_fh_add_dir_request *request9,
  ndmp3_fh_add_dir_request *request3)
{
	int			n_ent = request9->dirs.dirs_len;
	int			i;
	ndmp3_dir *		table;

	table = NDMOS_MACRO_NEWN(ndmp3_dir, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp9_dir *		ent9 = &request9->dirs.dirs_val[i];
		ndmp3_dir *		ent3 = &table[i];

		ent3->names.names_val = NDMOS_MACRO_NEW(ndmp3_file_name);
		ent3->names.names_len = 1;

		ent3->names.names_val[0].fs_type = NDMP3_FS_UNIX;
		ent3->names.names_val[0].ndmp3_file_name_u.unix_name =
				NDMOS_API_STRDUP(ent9->unix_name);

		ent3->node = ent9->node;
		ent3->parent = ent9->parent;
	}

	request3->dirs.dirs_len = n_ent;
	request3->dirs.dirs_val = table;

	return 0;
}

int
ndmp_9to3_fh_add_dir_free_request (ndmp3_fh_add_dir_request *request3)
{
    int	i;

    if (request3) {
	if(request3->dirs.dirs_val) {
	    int n_ent = request3->dirs.dirs_len;

	    for (i = 0; i < n_ent; i++) {
		ndmp3_dir *ent3 = &request3->dirs.dirs_val[i];
		if (ent3->names.names_val) {
		    if (ent3->names.names_val[0].ndmp3_file_name_u.unix_name)
			NDMOS_API_FREE(ent3->names.names_val[0].ndmp3_file_name_u.unix_name);
		    ent3->names.names_val[0].ndmp3_file_name_u.unix_name = 0;

		    NDMOS_API_FREE(ent3->names.names_val);
		}
		ent3->names.names_val = 0;
	    }

	    NDMOS_API_FREE(request3->dirs.dirs_val);
	}
	request3->dirs.dirs_val = 0;
    }

    return 0;
}


/*
 * ndmp_fh_add_node_request
 */

int
ndmp_3to9_fh_add_node_request (
  ndmp3_fh_add_node_request *request3,
  ndmp9_fh_add_node_request *request9)
{
	int			n_ent = request3->nodes.nodes_len;
	int			i;
	unsigned int		j;
	ndmp9_node *		table;

	table = NDMOS_MACRO_NEWN(ndmp9_node, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp3_node *		ent3 = &request3->nodes.nodes_val[i];
		ndmp3_file_stat *	file_stat = 0;
		ndmp3_file_stat		_file_stat;
		ndmp9_node *		ent9 = &table[i];

		for (j = 0; j < ent3->stats.stats_len; j++) {
			file_stat = &ent3->stats.stats_val[j];
			if (file_stat->fs_type == NDMP3_FS_UNIX) {
				break;
			}
		}
		if (j >= ent3->stats.stats_len) {
			file_stat = &_file_stat;
			NDMOS_MACRO_ZEROFILL (file_stat);
		}

		ndmp_3to9_file_stat (file_stat, &ent9->fstat,
					ent3->node, ent3->fh_info);
	}

	request9->nodes.nodes_len = n_ent;
	request9->nodes.nodes_val = table;

	return 0;
}

int
ndmp_3to9_fh_add_node_free_request (ndmp9_fh_add_node_request *request9)
{
    if (request9) {
	if(request9->nodes.nodes_val) {
	    NDMOS_API_FREE(request9->nodes.nodes_val);
	}
	request9->nodes.nodes_val = 0;
    }
    return 0;
}


int
ndmp_9to3_fh_add_node_request (
  ndmp9_fh_add_node_request *request9,
  ndmp3_fh_add_node_request *request3)
{
	int			n_ent = request9->nodes.nodes_len;
	int			i;
	ndmp3_node *		table;

	table = NDMOS_MACRO_NEWN(ndmp3_node, n_ent);
	if (!table)
		return -1;

	NDMOS_API_BZERO (table, sizeof *table * n_ent);

	for (i = 0; i < n_ent; i++) {
		ndmp9_node *		ent9 = &request9->nodes.nodes_val[i];
		ndmp3_node *		ent3 = &table[i];

		ent3->stats.stats_val = NDMOS_MACRO_NEW(ndmp3_file_stat);
		ent3->stats.stats_len = 1;

		ndmp_9to3_file_stat (&ent9->fstat,
				&ent3->stats.stats_val[0]);
		ent3->node = ent9->fstat.node.value;
		ent3->fh_info = ent9->fstat.fh_info.value;
	}

	request3->nodes.nodes_len = n_ent;
	request3->nodes.nodes_val = table;

	return 0;
}

int
ndmp_9to3_fh_add_node_free_request (ndmp3_fh_add_node_request *request3)
{
    if (request3) {
	if(request3->nodes.nodes_val) {
	    NDMOS_API_FREE(request3->nodes.nodes_val);
	}
	request3->nodes.nodes_val = 0;
    }
    return 0;
}



/*
 * request/reply translation
 */

#define NO_ARG_REQUEST \
		ndmp_xtox_no_arguments, ndmp_xtox_no_arguments

#define JUST_ERROR_REPLY \
		ndmp_3to9_error, ndmp_9to3_error

#define NO_ARG_REQUEST_JUST_ERROR_REPLY \
		NO_ARG_REQUEST, JUST_ERROR_REPLY

#define NO_MEMUSED_REQUEST \
		ndmp_xtox_no_memused, ndmp_xtox_no_memused

#define NO_MEMUSED_REPLY \
		ndmp_xtox_no_memused, ndmp_xtox_no_memused

#define NO_MEMUSED \
		ndmp_xtox_no_memused, ndmp_xtox_no_memused, ndmp_xtox_no_memused, ndmp_xtox_no_memused





struct reqrep_xlate	ndmp3_reqrep_xlate_table[] = {
    {
	NDMP3_CONNECT_OPEN,		NDMP9_CONNECT_OPEN,
	ndmp_3to9_connect_open_request,
	ndmp_9to3_connect_open_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONNECT_CLIENT_AUTH,	NDMP9_CONNECT_CLIENT_AUTH,
	ndmp_3to9_connect_client_auth_request,
	ndmp_9to3_connect_client_auth_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONNECT_CLOSE,		NDMP9_CONNECT_CLOSE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,	/* actually no reply */
	NO_MEMUSED /* no memory free routines written yet */
    },
#ifdef notyet
    {
	NDMP3_CONNECT_SERVER_AUTH,	NDMP9_CONNECT_SERVER_AUTH,
	ndmp_3to9_connect_server_auth_request,
	ndmp_9to3_connect_server_auth_request,
	ndmp_3to9_connect_server_auth_reply,
	ndmp_9to3_connect_server_auth_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
#endif /* notyet */

    {
	NDMP3_CONFIG_GET_HOST_INFO,	NDMP9_CONFIG_GET_HOST_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_host_info_reply,
	ndmp_9to3_config_get_host_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_CONNECTION_TYPE, NDMP9_CONFIG_GET_CONNECTION_TYPE,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_connection_type_reply,
	ndmp_9to3_config_get_connection_type_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_AUTH_ATTR,	NDMP9_CONFIG_GET_AUTH_ATTR,
	ndmp_3to9_config_get_auth_attr_request,
	ndmp_9to3_config_get_auth_attr_request,
	ndmp_3to9_config_get_auth_attr_reply,
	ndmp_9to3_config_get_auth_attr_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_BUTYPE_INFO,	NDMP9_CONFIG_GET_BUTYPE_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_butype_info_reply,
	ndmp_9to3_config_get_butype_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_FS_INFO,	NDMP9_CONFIG_GET_FS_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_fs_info_reply,
	ndmp_9to3_config_get_fs_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_TAPE_INFO,	NDMP9_CONFIG_GET_TAPE_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_tape_info_reply,
	ndmp_9to3_config_get_tape_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_SCSI_INFO,	NDMP9_CONFIG_GET_SCSI_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_scsi_info_reply,
	ndmp_9to3_config_get_scsi_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_CONFIG_GET_SERVER_INFO,	NDMP9_CONFIG_GET_SERVER_INFO,
	NO_ARG_REQUEST,
	ndmp_3to9_config_get_server_info_reply,
	ndmp_9to3_config_get_server_info_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },

    {
	NDMP3_SCSI_OPEN,	NDMP9_SCSI_OPEN,
	ndmp_3to9_scsi_open_request,
	ndmp_9to3_scsi_open_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_CLOSE,	NDMP9_SCSI_CLOSE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_GET_STATE,	NDMP9_SCSI_GET_STATE,
	NO_ARG_REQUEST,
	ndmp_3to9_scsi_get_state_reply,
	ndmp_9to3_scsi_get_state_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_SET_TARGET,	NDMP9_SCSI_SET_TARGET,
	ndmp_3to9_scsi_set_target_request,
	ndmp_9to3_scsi_set_target_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_RESET_DEVICE, NDMP9_SCSI_RESET_DEVICE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_RESET_BUS, NDMP9_SCSI_RESET_BUS,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_SCSI_EXECUTE_CDB,	NDMP9_SCSI_EXECUTE_CDB,
	ndmp_3to9_execute_cdb_request,
	ndmp_9to3_execute_cdb_request,
	ndmp_3to9_execute_cdb_reply,
	ndmp_9to3_execute_cdb_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },


    {
	NDMP3_TAPE_OPEN,	NDMP9_TAPE_OPEN,
	ndmp_3to9_tape_open_request,
	ndmp_9to3_tape_open_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_CLOSE,	NDMP9_TAPE_CLOSE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_GET_STATE,	NDMP9_TAPE_GET_STATE,
	NO_ARG_REQUEST,
	ndmp_3to9_tape_get_state_reply,
	ndmp_9to3_tape_get_state_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_MTIO,	NDMP9_TAPE_MTIO,
	ndmp_3to9_tape_mtio_request,
	ndmp_9to3_tape_mtio_request,
	ndmp_3to9_tape_mtio_reply,
	ndmp_9to3_tape_mtio_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_WRITE,	NDMP9_TAPE_WRITE,
	ndmp_3to9_tape_write_request,
	ndmp_9to3_tape_write_request,
	ndmp_3to9_tape_write_reply,
	ndmp_9to3_tape_write_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_READ,	NDMP9_TAPE_READ,
	ndmp_3to9_tape_read_request,
	ndmp_9to3_tape_read_request,
	ndmp_3to9_tape_read_reply,
	ndmp_9to3_tape_read_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_TAPE_EXECUTE_CDB,	NDMP9_TAPE_EXECUTE_CDB,
	ndmp_3to9_execute_cdb_request,
	ndmp_9to3_execute_cdb_request,
	ndmp_3to9_execute_cdb_reply,
	ndmp_9to3_execute_cdb_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },

    {
	NDMP3_DATA_GET_STATE,	NDMP9_DATA_GET_STATE,
	NO_ARG_REQUEST,
	ndmp_3to9_data_get_state_reply,
	ndmp_9to3_data_get_state_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_START_BACKUP, NDMP9_DATA_START_BACKUP,
	ndmp_3to9_data_start_backup_request,
	ndmp_9to3_data_start_backup_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_START_RECOVER, NDMP9_DATA_START_RECOVER,
	ndmp_3to9_data_start_recover_request,
	ndmp_9to3_data_start_recover_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_ABORT,	NDMP9_DATA_ABORT,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_GET_ENV,	NDMP9_DATA_GET_ENV,
	NO_ARG_REQUEST,
	ndmp_3to9_data_get_env_reply,
	ndmp_9to3_data_get_env_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_STOP,	NDMP9_DATA_STOP,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_LISTEN,	NDMP9_DATA_LISTEN,
	ndmp_3to9_data_listen_request,
	ndmp_9to3_data_listen_request,
	ndmp_3to9_data_listen_reply,
	ndmp_9to3_data_listen_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_CONNECT,	NDMP9_DATA_CONNECT,
	ndmp_3to9_data_connect_request,
	ndmp_9to3_data_connect_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_DATA_START_RECOVER_FILEHIST, NDMP9_DATA_START_RECOVER_FILEHIST,
	ndmp_3to9_data_start_recover_request,
	ndmp_9to3_data_start_recover_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },

    {
	NDMP3_NOTIFY_DATA_HALTED,	NDMP9_NOTIFY_DATA_HALTED,
	ndmp_3to9_notify_data_halted_request,
	ndmp_9to3_notify_data_halted_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_NOTIFY_CONNECTED,		NDMP9_NOTIFY_CONNECTED,
	ndmp_3to9_notify_connected_request,
	ndmp_9to3_notify_connected_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_NOTIFY_MOVER_HALTED,	NDMP9_NOTIFY_MOVER_HALTED,
	ndmp_3to9_notify_mover_halted_request,
	ndmp_9to3_notify_mover_halted_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_NOTIFY_MOVER_PAUSED,	NDMP9_NOTIFY_MOVER_PAUSED,
	ndmp_3to9_notify_mover_paused_request,
	ndmp_9to3_notify_mover_paused_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_NOTIFY_DATA_READ,		NDMP9_NOTIFY_DATA_READ,
	ndmp_3to9_notify_data_read_request,
	ndmp_9to3_notify_data_read_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },

    {
	NDMP3_LOG_FILE,			NDMP9_LOG_FILE,
	ndmp_3to9_log_file_request,
	ndmp_9to3_log_file_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_LOG_MESSAGE,		NDMP9_LOG_MESSAGE,
	ndmp_3to9_log_message_request,
	ndmp_9to3_log_message_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },

    {
	NDMP3_FH_ADD_FILE,		NDMP9_FH_ADD_FILE,
	ndmp_3to9_fh_add_file_request,
	ndmp_9to3_fh_add_file_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_FH_ADD_DIR,		NDMP9_FH_ADD_DIR,
	ndmp_3to9_fh_add_dir_request,
	ndmp_9to3_fh_add_dir_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	ndmp_3to9_fh_add_dir_free_request,
	ndmp_9to3_fh_add_dir_free_request,
	NO_MEMUSED_REPLY
    },
    {
	NDMP3_FH_ADD_NODE,		NDMP9_FH_ADD_NODE,
	ndmp_3to9_fh_add_node_request,
	ndmp_9to3_fh_add_node_request,
	JUST_ERROR_REPLY,		/* no reply actually */
	ndmp_3to9_fh_add_node_free_request,
	ndmp_9to3_fh_add_node_free_request,
	NO_MEMUSED_REPLY
    },

    {
	NDMP3_MOVER_GET_STATE,	NDMP9_MOVER_GET_STATE,
	NO_ARG_REQUEST,
	ndmp_3to9_mover_get_state_reply,
	ndmp_9to3_mover_get_state_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_LISTEN,	NDMP9_MOVER_LISTEN,
	ndmp_3to9_mover_listen_request,
	ndmp_9to3_mover_listen_request,
	ndmp_3to9_mover_listen_reply,
	ndmp_9to3_mover_listen_reply,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_CONNECT,	NDMP9_MOVER_CONNECT,
	ndmp_3to9_mover_connect_request,
	ndmp_9to3_mover_connect_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_CONTINUE,	NDMP9_MOVER_CONTINUE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_ABORT,	NDMP9_MOVER_ABORT,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_STOP,	NDMP9_MOVER_STOP,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_SET_WINDOW,	NDMP9_MOVER_SET_WINDOW,
	ndmp_3to9_mover_set_window_request,
	ndmp_9to3_mover_set_window_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_READ,	NDMP9_MOVER_READ,
	ndmp_3to9_mover_read_request,
	ndmp_9to3_mover_read_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_CLOSE,	NDMP9_MOVER_CLOSE,
	NO_ARG_REQUEST_JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
    {
	NDMP3_MOVER_SET_RECORD_SIZE, NDMP9_MOVER_SET_RECORD_SIZE,
	ndmp_3to9_mover_set_record_size_request,
	ndmp_9to3_mover_set_record_size_request,
	JUST_ERROR_REPLY,
	NO_MEMUSED /* no memory free routines written yet */
    },
	/* ndmp_mover_connnect TBD */

    { 0 },
};


#endif /* !NDMOS_OPTION_NO_NDMP3 */
