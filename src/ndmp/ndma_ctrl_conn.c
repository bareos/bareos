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


#include "ndmagents.h"


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT


int
ndmca_connect_xxx_agent (
  struct ndm_session *sess,
  struct ndmconn **connp,
  char *prefix,
  struct ndmagent *agent)
{
	struct ndmconn *	conn = *connp;
	char *			err;
	int			rc;

	if (conn)
		return 0;		/* already connected */

	if (agent->conn_type == NDMCONN_TYPE_NONE) {
		ndmalogf (sess, 0, 0, "agent %s not give", prefix+1);
		return -1;
	}

	conn = ndmconn_initialize (0, prefix);
	if (!conn) {
		ndmalogf (sess, prefix, 0, "can't init connection");
		return -1;
	}

	if (sess->control_acb->job.time_limit > 0)
	    conn->time_limit = sess->control_acb->job.time_limit;

	if (sess->conn_snooping) {
		ndmconn_set_snoop (conn, &sess->param->log, sess->param->log_level);
	}

	conn->call = ndma_call;
	conn->context = sess;
	conn->unexpected = ndma_dispatch_ctrl_unexpected;

	rc = ndmconn_connect_agent (conn, agent);
	if (rc) {
		err = "Can't connect";
		goto error_out;
	}

	rc = ndmconn_auth_agent (conn, agent);
	if (rc) {
		err = "Can't auth (bad pw?)";
		goto error_out;
	}

	*connp = conn;
	return 0;

  error_out:
	ndmalogf (sess, prefix, 0, "err %s", ndmconn_get_err_msg (conn));
	//ndmconn_destruct (conn);
	*connp = conn;
	return -1;

}

int
ndmca_connect_data_agent (struct ndm_session *sess)
{
	int		rc;

	rc = ndmca_connect_xxx_agent (sess,
				&sess->plumb.data,
				"#D",
				&sess->control_acb->job.data_agent);
	if (rc == 0) {
		if (sess->plumb.data->conn_type == NDMCONN_TYPE_RESIDENT) {
			sess->data_acb->protocol_version =
					sess->plumb.data->protocol_version;
		}
	}

	return rc;
}

int
ndmca_connect_tape_agent (struct ndm_session *sess)
{
	int		rc;

	if (sess->control_acb->job.tape_agent.conn_type == NDMCONN_TYPE_NONE) {
		rc = ndmca_connect_data_agent (sess);
		if (rc) {
			ndmconn_destruct (sess->plumb.data);
			sess->plumb.data = NULL;
			return rc;
		}
		sess->plumb.tape = sess->plumb.data;
		rc = 0;
	} else {
		rc = ndmca_connect_xxx_agent (sess,
				&sess->plumb.tape,
				"#T",
				&sess->control_acb->job.tape_agent);
		ndmalogf (sess, 0, 7, "ndmca_connect_tape_agent: %d %p", rc, sess->plumb.tape);
	}

	if (rc == 0) {
		if (sess->plumb.tape->conn_type == NDMCONN_TYPE_RESIDENT) {
			sess->tape_acb->protocol_version =
					sess->plumb.tape->protocol_version;
		}
	}

	return rc;
}

int
ndmca_connect_robot_agent (struct ndm_session *sess)
{
	int		rc;

	if (sess->control_acb->job.robot_agent.conn_type == NDMCONN_TYPE_NONE) {
		rc = ndmca_connect_tape_agent (sess);
		if (rc) return rc;
		sess->plumb.robot = sess->plumb.tape;
		rc = 0;
	} else {
		rc = ndmca_connect_xxx_agent (sess,
				&sess->plumb.robot,
				"#R",
				&sess->control_acb->job.robot_agent);
	}

	if (rc == 0) {
		if (sess->plumb.robot->conn_type == NDMCONN_TYPE_RESIDENT) {
			sess->robot_acb->protocol_version =
					sess->plumb.robot->protocol_version;
		}
	}

	return rc;
}

int
ndmca_connect_control_agent (struct ndm_session *sess)
{
	struct ndmagent	control_agent;
	int		rc;

	ndmagent_from_str (&control_agent, ".");	/* resident */

	rc = ndmca_connect_xxx_agent (sess,
				&sess->plumb.control,
				"#C.",
				&control_agent);

	return rc;
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
