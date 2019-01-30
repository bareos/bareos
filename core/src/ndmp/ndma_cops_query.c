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

void ndmca_query_register_callbacks (struct ndm_session *sess,
      struct ndmca_query_callbacks *callbacks)
{
   /*
    * Only allow one register and make sure callbacks are set
    */
   if (!sess->query_cbs && callbacks) {
      sess->query_cbs = NDMOS_API_MALLOC (sizeof(struct ndmca_query_callbacks));
      if (sess->query_cbs) {
         memcpy (sess->query_cbs, callbacks, sizeof(struct ndmca_query_callbacks));
      }
   }
}

void ndmca_query_unregister_callbacks (struct ndm_session *sess)
{
   if (sess->query_cbs) {
      NDMOS_API_FREE (sess->query_cbs);
      sess->query_cbs = NULL;
   }
}





int
ndmca_op_query (struct ndm_session *sess)
{
	ndmca_opq_data(sess);
	ndmca_opq_tape(sess);
	ndmca_opq_robot(sess);
	return 0;
}

int
ndmca_opq_data (struct ndm_session *sess)
{
	struct ndm_job_param *	job = &sess->control_acb->job;
	int			rc;

	if (job->data_agent.conn_type == NDMCONN_TYPE_NONE)
		return 0;

	rc = ndmca_connect_data_agent (sess);
	if (rc) {
		ndmconn_destruct (sess->plumb.data);
		sess->plumb.data = NULL;
		return rc;	/* already tattled */
	}

	ndmalogqr (sess, "");
	ndmalogqr (sess, "Data Agent %s NDMPv%d",
		job->data_agent.host,
		sess->plumb.data->protocol_version);
	ndmca_opq_host_info (sess, sess->plumb.data);
	ndmca_opq_get_mover_type (sess, sess->plumb.data);
	ndmca_opq_get_butype_attr (sess, sess->plumb.data);
#ifndef NDMOS_OPTION_NO_NDMP3
	if (sess->plumb.data->protocol_version == NDMP3VER) {
		ndmca_opq_get_fs_info (sess, sess->plumb.data);
	}
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	if (sess->plumb.data->protocol_version == NDMP4VER) {
		ndmca_opq_get_fs_info (sess, sess->plumb.data);
	}
#endif /* !NDMOS_OPTION_NO_NDMP4 */

	return 0;
}

int
ndmca_opq_tape (struct ndm_session *sess)
{
	struct ndm_job_param *	job = &sess->control_acb->job;
	int			rc;

	if (job->tape_agent.conn_type == NDMCONN_TYPE_NONE)
		return 0;

	rc = ndmca_connect_tape_agent (sess);
	if (rc) {
		ndmconn_destruct (sess->plumb.tape);
		sess->plumb.data = NULL;
		return rc;	/* already tattled */
	}

	ndmalogqr (sess, "");
	ndmalogqr (sess, "Tape Agent %s NDMPv%d",
		job->tape_agent.host,
		sess->plumb.tape->protocol_version);
	if (sess->plumb.tape != sess->plumb.data) {	/* don't be boring */
		ndmca_opq_host_info (sess, sess->plumb.tape);
		ndmca_opq_get_mover_type (sess, sess->plumb.tape);
	}

#ifndef NDMOS_OPTION_NO_NDMP3
	if (sess->plumb.tape->protocol_version == NDMP3VER) {
		ndmca_opq_get_tape_info (sess, sess->plumb.tape);
	}
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	if (sess->plumb.tape->protocol_version == NDMP4VER) {
		ndmca_opq_get_tape_info (sess, sess->plumb.tape);
	}
#endif /* !NDMOS_OPTION_NO_NDMP4 */

	return 0;
}

int
ndmca_opq_robot (struct ndm_session *sess)
{
	struct ndm_job_param *	job = &sess->control_acb->job;
	int			rc;

	if (job->robot_agent.conn_type == NDMCONN_TYPE_NONE
	 && !job->have_robot)
		return 0;

	rc = ndmca_connect_robot_agent (sess);
	if (rc)
		return rc;	/* already tattled */

	ndmalogqr (sess, "");
	ndmalogqr (sess, "Robot Agent %s NDMPv%d",
		job->robot_agent.host,
		sess->plumb.robot->protocol_version);

	if (sess->plumb.robot != sess->plumb.data
	 && sess->plumb.robot != sess->plumb.tape) {
		/* don't be boring */
		ndmca_opq_host_info (sess, sess->plumb.robot);
	}

#ifndef NDMOS_OPTION_NO_NDMP3
	if (sess->plumb.robot->protocol_version == NDMP3VER) {
		ndmca_opq_get_scsi_info (sess, sess->plumb.robot);
	}
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	if (sess->plumb.robot->protocol_version == NDMP4VER) {
		ndmca_opq_get_scsi_info (sess, sess->plumb.robot);
	}
#endif /* !NDMOS_OPTION_NO_NDMP4 */

	if (job->have_robot) {
		if (ndmca_robot_prep_target(sess)) {
			ndmalogqr (sess, "  robot init failed");
			return -1;
		}

		ndmca_robot_query (sess);
	}

	return 0;
}

int
ndmca_opq_host_info (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;
	int		cnt;
	int		out;
	unsigned int	i;
	char		buf[100];

	switch (conn->protocol_version) {
	default:
		ndmalogqr (sess, "  Host info NDMPv???? %d",
				conn->protocol_version);
		ndmalogqr (sess, "    INTERNAL ERROR, CHECK BUILD");
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH_VOID_REQUEST(ndmp2_config_get_host_info, NDMP2VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_host_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Host info");
		ndmalogqr (sess, "    hostname   %s", reply->hostname);
		ndmalogqr (sess, "    os_type    %s", reply->os_type);
		ndmalogqr (sess, "    os_vers    %s", reply->os_vers);
		ndmalogqr (sess, "    hostid     %s", reply->hostid);

		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->auth_type.auth_type_len; i++) {
			ndmp2_auth_type atyp;

			atyp = reply->auth_type.auth_type_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp2_auth_type_to_str (atyp));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp2_auth_type_to_str (atyp));
			out += rc;
			cnt -= rc;
		}
		buf[sizeof(buf) - 1] = '\0';

		ndmalogqr (sess, "    auths      (%d) %s",
					reply->auth_type.auth_type_len, buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_host_info, NDMP3VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_host_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Host info");
		ndmalogqr (sess, "    hostname   %s", reply->hostname);
		ndmalogqr (sess, "    os_type    %s", reply->os_type);
		ndmalogqr (sess, "    os_vers    %s", reply->os_vers);
		ndmalogqr (sess, "    hostid     %s", reply->hostid);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH

	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_server_info, NDMP3VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_server_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Server info");

		ndmalogqr (sess, "    vendor     %s", reply->vendor_name);
		ndmalogqr (sess, "    product    %s", reply->product_name);
		ndmalogqr (sess, "    revision   %s", reply->revision_number);
		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->auth_type.auth_type_len; i++) {
			ndmp3_auth_type atyp;

			atyp = reply->auth_type.auth_type_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp3_auth_type_to_str (atyp));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp3_auth_type_to_str (atyp));
			out += rc;
			cnt -= rc;
		}
		buf[sizeof(buf) - 1] = '\0';

		ndmalogqr (sess, "    auths      (%d) %s",
					reply->auth_type.auth_type_len, buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_host_info, NDMP4VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_host_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Host info");
		ndmalogqr (sess, "    hostname   %s", reply->hostname);
		ndmalogqr (sess, "    os_type    %s", reply->os_type);
		ndmalogqr (sess, "    os_vers    %s", reply->os_vers);
		ndmalogqr (sess, "    hostid     %s", reply->hostid);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH

	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_server_info, NDMP4VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_server_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Server info");

		ndmalogqr (sess, "    vendor     %s", reply->vendor_name);
		ndmalogqr (sess, "    product    %s", reply->product_name);
		ndmalogqr (sess, "    revision   %s", reply->revision_number);
		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->auth_type.auth_type_len; i++) {
			ndmp4_auth_type atyp;

			atyp = reply->auth_type.auth_type_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp4_auth_type_to_str (atyp));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp4_auth_type_to_str (atyp));
			out += rc;
			cnt -= rc;
		}
		buf[sizeof(buf) - 1] = '\0';

		ndmalogqr (sess, "    auths      (%d) %s",
					reply->auth_type.auth_type_len, buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}

int
ndmca_opq_get_mover_type (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;
	int		cnt;
	int		out;
	unsigned int	i;
	char		buf[100];

	switch (conn->protocol_version) {
	default:
		/* already tattled in ndmca_opq_host_info() */
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH_VOID_REQUEST(ndmp2_config_get_mover_type, NDMP2VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_mover_info failed");
			return rc;
		}

		ndmalogqr (sess, "  Mover types");

		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->methods.methods_len; i++) {
			ndmp2_mover_addr_type	val;

			val = reply->methods.methods_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp2_mover_addr_type_to_str (val));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp2_mover_addr_type_to_str (val));
			out += rc;
			cnt -= rc;
		}
		ndmalogqr (sess, "    methods    (%d) %s",
					reply->methods.methods_len,
					buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_connection_type, NDMP3VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_connection_type failed");
			return rc;
		}

		ndmalogqr (sess, "  Connection types");
		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->addr_types.addr_types_len; i++) {
			ndmp3_addr_type		val;

			val = reply->addr_types.addr_types_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp3_addr_type_to_str (val));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp3_addr_type_to_str (val));
			out += rc;
			cnt -= rc;
		}
		ndmalogqr (sess, "    addr_types (%d) %s",
					reply->addr_types.addr_types_len, buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_connection_type, NDMP4VER)
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "get_connection_type failed");
			return rc;
		}

		ndmalogqr (sess, "  Connection types");
		*buf = 0;
		out = 0;
		cnt = sizeof(buf) - 1;
		for (i = 0; i < reply->addr_types.addr_types_len; i++) {
			ndmp4_addr_type		val;

			val = reply->addr_types.addr_types_val[i];
			if (out)
				rc = snprintf(buf + out, cnt, " %s", ndmp4_addr_type_to_str (val));
                        else
				rc = snprintf(buf + out, cnt, "%s", ndmp4_addr_type_to_str (val));
			out += rc;
			cnt -= rc;
		}
		ndmalogqr (sess, "    addr_types (%d) %s",
					reply->addr_types.addr_types_len, buf);
		ndmalogqr (sess, "");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}

int
ndmca_opq_get_butype_attr (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;

	switch (conn->protocol_version) {
	default:
		/* already tattled in ndmca_opq_host_info() */
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_config_get_butype_attr, NDMP2VER)
		request->name = sess->control_acb->job.bu_type;
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "  get_butype_attr '%s' failed",
					sess->control_acb->job.bu_type);
			return rc;
		}

		ndmalogqr (sess, "  Backup type attributes of %s format",
			sess->control_acb->job.bu_type);
		ndmalogqr (sess, "    backup-filelist   %s",
			(reply->attrs&NDMP2_NO_BACKUP_FILELIST) ? "no":"yes");
		ndmalogqr (sess, "    backup-fhinfo     %s",
			(reply->attrs&NDMP2_NO_BACKUP_FHINFO) ? "no":"yes");
		ndmalogqr (sess, "    recover-filelist  %s",
			(reply->attrs&NDMP2_NO_RECOVER_FILELIST) ? "no":"yes");
		ndmalogqr (sess, "    recover-fhinfo    %s",
			(reply->attrs&NDMP2_NO_RECOVER_FHINFO) ? "no":"yes");
		ndmalogqr (sess, "    recover-inc-only  %s",
			(reply->attrs&NDMP2_NO_RECOVER_INC_ONLY) ? "no":"yes");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_butype_info, NDMP3VER)
		unsigned int	i, j;

		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "  get_butype_info failed");
			return rc;
		}

		for (i = 0; i < reply->butype_info.butype_info_len; i++) {
		    ndmp3_butype_info *	buti;

		    buti = &reply->butype_info.butype_info_val[i];
		    ndmalogqr (sess, "  Backup type info of %s format",
				buti->butype_name);
		    ndmalogqr (sess, "    attrs      0x%lx",
				buti->attrs);

		    ndmalogqr (sess, "      backup-file-history   %s",
			       (buti->attrs&NDMP3_BUTYPE_BACKUP_FILE_HISTORY) ? "yes":"no");
		    ndmalogqr (sess, "      backup-filelist   %s",
			       (buti->attrs&NDMP3_BUTYPE_BACKUP_FILELIST) ? "yes":"no");
		    ndmalogqr (sess, "      recover-filelist   %s",
			       (buti->attrs&NDMP3_BUTYPE_RECOVER_FILELIST) ? "yes":"no");
		    ndmalogqr (sess, "      backup-direct   %s",
			       (buti->attrs&NDMP3_BUTYPE_BACKUP_DIRECT) ? "yes":"no");
		    ndmalogqr (sess, "      recover-direct   %s",
			       (buti->attrs&NDMP3_BUTYPE_RECOVER_DIRECT) ? "yes":"no");
		    ndmalogqr (sess, "      backup-incremental   %s",
			       (buti->attrs&NDMP3_BUTYPE_BACKUP_INCREMENTAL) ? "yes":"no");
		    ndmalogqr (sess, "      recover-incremental   %s",
			       (buti->attrs&NDMP3_BUTYPE_RECOVER_INCREMENTAL) ? "yes":"no");
		    ndmalogqr (sess, "      backup-utf8   %s",
			       (buti->attrs&NDMP3_BUTYPE_BACKUP_UTF8) ? "yes":"no");
		    ndmalogqr (sess, "      recover-utf8   %s",
			       (buti->attrs&NDMP3_BUTYPE_RECOVER_UTF8) ? "yes":"no");
		    ndmalogqr (sess, "      recover-file-history   %s",
			       (buti->attrs&NDMP3_BUTYPE_RECOVER_FILE_HISTORY) ? "yes":"no");




		    for (j = 0; j < buti->default_env.default_env_len; j++) {
			ndmalogqr (sess, "    set        %s=%s",
				buti->default_env.default_env_val[j].name,
				buti->default_env.default_env_val[j].value);
		    }
		    if (j == 0)
			ndmalogqr (sess, "    empty default env");
		    ndmalogqr (sess, "");
		}
		if (i == 0)
			ndmalogqr (sess, "  Empty backup type info");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_butype_info, NDMP4VER)
		unsigned int	i, j;

		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "  get_butype_info failed");
			return rc;
		}

		for (i = 0; i < reply->butype_info.butype_info_len; i++) {
		    ndmp4_butype_info *	buti;

		    buti = &reply->butype_info.butype_info_val[i];
		    ndmalogqr (sess, "  Backup type info of %s format",
				buti->butype_name);
		    ndmalogqr (sess, "    attrs      0x%lx",
				buti->attrs);
		    for (j = 0; j < buti->default_env.default_env_len; j++) {
			ndmalogqr (sess, "    set        %s=%s",
				buti->default_env.default_env_val[j].name,
				buti->default_env.default_env_val[j].value);
		    }
		    if (j == 0)
			ndmalogqr (sess, "    empty default env");
		    ndmalogqr (sess, "");
		}
		if (i == 0)
			ndmalogqr (sess, "  Empty backup type info");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}

#ifndef NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4
int
ndmca_opq_get_fs_info (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;

	switch (conn->protocol_version) {
	default:
		/* already tattled in ndmca_opq_host_info() */
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
		break;	/* why are we here? */
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_fs_info, NDMP3VER)
		unsigned int	i, j;

		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "  get_fs_info failed");
			return rc;
		}

		for (i = 0; i < reply->fs_info.fs_info_len; i++) {
			ndmp3_fs_info *		fsi;

			fsi = &reply->fs_info.fs_info_val[i];

			ndmalogqr (sess, "  File system %s",
					fsi->fs_logical_device);

			ndmalogqr (sess, "    physdev    %s",
					fsi->fs_physical_device);
			ndmalogqr (sess, "    invalid    0x%lx", fsi->invalid);
			if (fsi->invalid & NDMP3_FS_INFO_TOTAL_SIZE_INVALID)
			    ndmalogqr (sess, "        TOTAL_SIZE_INVALID");
			if (fsi->invalid & NDMP3_FS_INFO_USED_SIZE_INVALID)
			    ndmalogqr (sess, "        USED_SIZE_INVALID");
			if (fsi->invalid & NDMP3_FS_INFO_AVAIL_SIZE_INVALID)
			    ndmalogqr (sess, "        AVAIL_SIZE_INVALID");

			if (fsi->invalid & NDMP3_FS_INFO_TOTAL_INODES_INVALID)
			    ndmalogqr (sess, "        TOTAL_INODES_INVALID");
			if (fsi->invalid & NDMP3_FS_INFO_USED_INODES_INVALID)
			    ndmalogqr (sess, "        USED_INODES_INVALID");
			ndmalogqr (sess, "    type       %s", fsi->fs_type);
			ndmalogqr (sess, "    status     %s", fsi->fs_status);
			ndmalogqr (sess,
			    "    space      %lld total, %lld used, %lld avail",
					fsi->total_size,
					fsi->used_size,
					fsi->avail_size);

			ndmalogqr (sess, "    inodes     %lld total, %lld used",
					fsi->total_inodes,
					fsi->used_inodes);

			for (j = 0; j < fsi->fs_env.fs_env_len; j++) {
				ndmalogqr (sess, "    set        %s=%s",
					fsi->fs_env.fs_env_val[j].name,
					fsi->fs_env.fs_env_val[j].value);
			}
			if (j == 0)
				ndmalogqr (sess, "    empty default env");
			ndmalogqr (sess, "");
		}
		if (i == 0)
			ndmalogqr (sess, "  Empty fs info");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_fs_info, NDMP4VER)
		unsigned int	i, j;

		rc = NDMC_CALL(conn);
		if (rc) {
			ndmalogqr (sess, "  get_fs_info failed");
			return rc;
		}

		for (i = 0; i < reply->fs_info.fs_info_len; i++) {
			ndmp4_fs_info *		fsi;

			fsi = &reply->fs_info.fs_info_val[i];

			ndmalogqr (sess, "  File system %s",
					fsi->fs_logical_device);

			ndmalogqr (sess, "    physdev    %s",
					fsi->fs_physical_device);
			ndmalogqr (sess, "    unsupported 0x%lx",
						fsi->unsupported);
			if (fsi->unsupported & NDMP4_FS_INFO_TOTAL_SIZE_UNS)
			    ndmalogqr (sess, "        TOTAL_SIZE_UNS");
			if (fsi->unsupported & NDMP4_FS_INFO_USED_SIZE_UNS)
			    ndmalogqr (sess, "        USED_SIZE_UNS");
			if (fsi->unsupported & NDMP4_FS_INFO_AVAIL_SIZE_UNS)
			    ndmalogqr (sess, "        AVAIL_SIZE_UNS");

			if (fsi->unsupported & NDMP4_FS_INFO_TOTAL_INODES_UNS)
			    ndmalogqr (sess, "        TOTAL_INODES_UNS");
			if (fsi->unsupported & NDMP4_FS_INFO_USED_INODES_UNS)
			    ndmalogqr (sess, "        USED_INODES_UNS");


			ndmalogqr (sess, "    type       %s", fsi->fs_type);
			ndmalogqr (sess, "    status     %s", fsi->fs_status);
			ndmalogqr (sess,
			    "    space      %lld total, %lld used, %lld avail",
					fsi->total_size,
					fsi->used_size,
					fsi->avail_size);

			ndmalogqr (sess, "    inodes     %lld total, %lld used",
					fsi->total_inodes,
					fsi->used_inodes);

			for (j = 0; j < fsi->fs_env.fs_env_len; j++) {
				ndmalogqr (sess, "    set        %s=%s",
					fsi->fs_env.fs_env_val[j].name,
					fsi->fs_env.fs_env_val[j].value);
			}
			if (j == 0)
				ndmalogqr (sess, "    empty default env");
			ndmalogqr (sess, "");
		}
		if (i == 0)
			ndmalogqr (sess, "  Empty fs info");

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}

int
ndmca_opq_show_device_info (struct ndm_session *sess,
  ndmp9_device_info *info,
  unsigned n_info, char *what)
{
	unsigned int	i, j, k;

	for (i = 0; i < n_info; i++) {
		ndmalogqr (sess, "  %s %s", what, info[i].model);
		for (j = 0; j < info[i].caplist.caplist_len; j++) {
			ndmp9_device_capability *dc;
			uint32_t attr;

			dc = &info[i].caplist.caplist_val[j];

			ndmalogqr (sess, "    device     %s", dc->device);
			if (!strcmp(what, "tape")) {
#ifndef NDMOS_OPTION_NO_NDMP3
			    if (sess->plumb.tape->protocol_version == 3) {
				attr = dc->v3attr.value;
				ndmalogqr (sess, "      attr       0x%lx",
					   attr);
				if (attr & NDMP3_TAPE_ATTR_REWIND)
				    ndmalogqr (sess, "        REWIND");
				if (attr & NDMP3_TAPE_ATTR_UNLOAD)
				    ndmalogqr (sess, "        UNLOAD");
			    }
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
			    if (sess->plumb.tape->protocol_version == 4) {
				attr = dc->v4attr.value;
				ndmalogqr (sess, "      attr       0x%lx",
					   attr);
				if (attr & NDMP4_TAPE_ATTR_REWIND)
				    ndmalogqr (sess, "        REWIND");
				if (attr & NDMP4_TAPE_ATTR_UNLOAD)
				    ndmalogqr (sess, "        UNLOAD");
			    }
#endif /* !NDMOS_OPTION_NO_NDMP4 */
			}
			for (k = 0; k < dc->capability.capability_len; k++) {
				ndmalogqr (sess, "      set        %s=%s",
				    dc->capability.capability_val[k].name,
				    dc->capability.capability_val[k].value);
			}
			if (k == 0)
				ndmalogqr (sess, "      empty capabilities");
		}
		if (j == 0)
			ndmalogqr (sess, "    empty caplist");
		ndmalogqr (sess, "");
	}
	if (i == 0)
		ndmalogqr (sess, "  Empty %s info", what);

	return 0;
}

int
ndmca_opq_get_tape_info (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;

    NDMC_WITH_VOID_REQUEST(ndmp9_config_get_tape_info, NDMP9VER)
	rc = NDMC_CALL(conn);
	if (rc) {
		ndmalogqr (sess, "  get_tape_info failed");
		return rc;
	}
	ndmca_opq_show_device_info (sess,
		reply->config_info.tape_info.tape_info_val,
		reply->config_info.tape_info.tape_info_len,
		"tape");

/* execute callback if exists */
   if (sess->query_cbs && sess->query_cbs->get_tape_info) {
      rc = sess->query_cbs->get_tape_info(sess,
            reply->config_info.tape_info.tape_info_val,
            reply->config_info.tape_info.tape_info_len);
   }

	NDMC_FREE_REPLY();

	return 0;
    NDMC_ENDWITH
}

int
ndmca_opq_get_scsi_info (struct ndm_session *sess, struct ndmconn *conn)
{
	int		rc;

    NDMC_WITH_VOID_REQUEST(ndmp9_config_get_scsi_info, NDMP9VER)
	rc = NDMC_CALL(conn);
	if (rc) {
		ndmalogqr (sess, "  get_scsi_info failed");
		return rc;
	}
	ndmca_opq_show_device_info (sess,
		reply->config_info.scsi_info.scsi_info_val,
		reply->config_info.scsi_info.scsi_info_len,
		"scsi");

	NDMC_FREE_REPLY();

	return 0;
    NDMC_ENDWITH
}

#endif /* !NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 */

void
ndmalogqr (struct ndm_session *sess, char *fmt, ...)
{
	va_list		ap;

	va_start (ap, fmt);
	ndmalogfv (sess, "QR", 0, fmt, ap);
	va_end (ap);
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
