/*
 * Copyright (c) 1998,1999,2000
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


#include "ndmlib.h"


/*
 * NAME[,[CNUM,]SID[,LUN]
 *
 * The foregoing pattern is ambiguous. Here's the disambiguating rules:
 * 1) If just a name is given, controller, sid, and lun are all
 *    set to invalid (-1)
 * 2) If one number comes after the device name (,sid) it is the
 *    SID; controller is set to -1, lun is set to 0
 * 3) If two numbers come after the device name (,sid,lun), they are
 *    the SID and LUN; controller is set to -1
 * 4) Three numbers after the device name are all three number fields.
 */


int
ndmscsi_target_from_str (struct ndmscsi_target *targ, char *str)
{
	char *		p;
	int		n1, n2, n3;

	NDMOS_MACRO_ZEROFILL (targ);

	p = strchr (str, ',');
	if (p) {
		*p++ = 0;
	}

	if (strlen (str) >= NDMOS_CONST_PATH_MAX) {
		if (p) p[-1] = ',';
		return -2;
	}

	strcpy (targ->dev_name, str);

	if (!p) {
		targ->controller = -1;
		targ->sid = -1;
		targ->lun = -1;

		return 0;
	}

	p[-1] = ',';

	if (*p < '0' || '9' < *p) {
		return -3;
	}
	n1 = strtol (p, &p, 0);
	if (*p != 0 && *p != ',') {
		return -4;
	}
	if (*p == 0) {
		targ->controller = -1;
		targ->sid = n1;
		targ->lun = 0;

		return 0;
	}

	p++;

	if (*p < '0' || '9' < *p) {
		return -5;
	}
	n2 = strtol (p, &p, 0);

	if (*p == 0) {
		/* SID,LUN */
		targ->controller = -1;
		targ->sid = n1;
		targ->lun = n2;
	} else {
		if (*p != ',') {
			return -6;
		}
		p++;

		if (*p < '0' || '9' < *p) {
			return -7;
		}
		n3 = strtol (p, &p, 0);

		if (*p != 0) {
			return -8;
		}

		targ->controller = n1;
		targ->sid = n2;
		targ->lun = n3;
	}

	return 0;
}


int
ndmscsi_open (struct ndmconn *conn, char *dev_name)
{
	int		rc;

	NDMC_WITH(ndmp9_scsi_open, NDMP9VER)
		request->device = dev_name;
		rc = NDMC_CALL(conn);
	NDMC_ENDWITH

	return rc;
}

int
ndmscsi_close (struct ndmconn *conn)
{
	int		rc;

	NDMC_WITH_VOID_REQUEST(ndmp9_scsi_close, NDMP9VER)
		rc = NDMC_CALL(conn);
	NDMC_ENDWITH

	return rc;
}

int
ndmscsi_get_state (struct ndmconn *conn, struct ndmscsi_target *targ)
{
	int		rc;

	NDMOS_MACRO_ZEROFILL (targ);

	NDMC_WITH_VOID_REQUEST(ndmp9_scsi_get_state, NDMP9VER)
		rc = NDMC_CALL(conn);
		targ->controller = reply->target_controller;
		targ->sid = reply->target_id;
		targ->lun = reply->target_lun;
	NDMC_ENDWITH

	return rc;
}

int
ndmscsi_set_target (struct ndmconn *conn, struct ndmscsi_target *targ)
{
	int		rc;

	NDMC_WITH(ndmp9_scsi_set_target, NDMP9VER)
		request->device = targ->dev_name;
		request->target_controller = targ->controller;
		request->target_id = targ->sid;
		request->target_lun = targ->lun;
		rc = NDMC_CALL(conn);
	NDMC_ENDWITH

	return rc;
}

int
ndmscsi_use (struct ndmconn *conn, struct ndmscsi_target *targ)
{
	int		rc;

#if 0
	rc = ndmscsi_close (conn);
	/* error ignored */
#endif

	rc = ndmscsi_open (conn, targ->dev_name);
	if (rc) return rc;

	if (targ->controller != -1 || targ->sid != -1 || targ->lun != -1) {
#ifndef NDMOS_OPTION_NO_NDMP4
		if (conn->protocol_version == NDMP4VER) {
			return -1;	/* can't set target */
		}
#endif /* !NDMOS_OPTION_NO_NDMP4 */

		rc = ndmscsi_set_target (conn, targ);
		if (rc) {
			ndmscsi_close (conn);	/* best effort */
			return rc;
		}
	}

	return 0;
}

int
ndmscsi_execute (struct ndmconn *conn,
  struct ndmscsi_request *req,
  struct ndmscsi_target *targ)
{
	int		rc;

	if (targ) {
		rc = ndmscsi_use (conn, targ);
		if (rc) return rc;
	}

	NDMC_WITH(ndmp9_scsi_execute_cdb, NDMP9VER)
		request->cdb.cdb_len = req->n_cmd;
		request->cdb.cdb_val = (char*)req->cmd;

		switch (req->data_dir) {
		case NDMSCSI_DD_NONE:
			request->data_dir = NDMP9_SCSI_DATA_DIR_NONE;
			break;

		case NDMSCSI_DD_IN:
			request->data_dir = NDMP9_SCSI_DATA_DIR_IN;
			request->datain_len = req->n_data_avail;
			break;

		case NDMSCSI_DD_OUT:
			request->data_dir = NDMP9_SCSI_DATA_DIR_OUT;
			request->dataout.dataout_len = req->n_data_avail;
			request->dataout.dataout_val = (char*)req->data;
			break;
		}
		request->timeout = 300000;	/* five minutes */

		rc = NDMC_CALL (conn);
		if (rc) {
			req->completion_status = NDMSCSI_CS_FAIL;
			return rc;
		}

		req->status_byte = reply->status;
		req->n_data_done = 0;
		req->n_sense_data = 0;

		rc = reply->ext_sense.ext_sense_len;
		if (rc > 0) {
			if (rc > NDMSCSI_MAX_SENSE_DATA)
				rc = NDMSCSI_MAX_SENSE_DATA;

			req->n_sense_data = rc;
			NDMOS_API_BCOPY (reply->ext_sense.ext_sense_val,
					req->sense_data, rc);
		}

		switch (req->data_dir) {
		case NDMSCSI_DD_IN:
			req->n_data_done = reply->datain.datain_len;
			if (req->n_data_done > 0) {
				NDMOS_API_BCOPY (reply->datain.datain_val,
					req->data, req->n_data_done);
			}
			break;

		case NDMSCSI_DD_OUT:
			req->n_data_done = reply->dataout_len;
			break;
		}
		req->completion_status = NDMSCSI_CS_GOOD;

		NDMC_FREE_REPLY();
	NDMC_ENDWITH

	return 0;
}
