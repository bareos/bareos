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
 ********************************************************************
 *
 * NDMP Elements of a backup/restore session
 *
 *                   +-----+     ###########
 *                   | Job |----># CONTROL #      +----------------+
 *                   +-----+     #  Agent  #<---->|FILE/MEDIA INDEX|
 *                               #         #      +----------------+
 *                               ###########
 *                                 |  |  |
 *                +----------------+  |  +---------------------+
 *                |           control | connections            |
 *                V                   V                        V
 *           ############        ############  +-------+   #########
 *           #  DATA    #        #  TAPE    #  |       |   # ROBOT #
 *           #  Agent   #        #  Agent   #  | ROBOT |<-># Agent #
 *  +-----+  # +------+ # image  # +------+ #  |+-----+|   #       #
 *  |FILES|====|butype|============|mover |=====|DRIVE||   #       #
 *  +-----+  # +------+ # stream # +------+ #  |+-----+|   #       #
 *           ############        ############  +-------+   #########
 *
 *
 ********************************************************************
 *
 * NDMAGENTS components overview
 *
 *
 *    "job" -> ndma_client_session()    ndma_server_session()
 *                   |                           |
 *     /-------------/                           Q
 *     |                                         v
 *     |       +------------------------------------+      +-----------+
 *     |    /->|           SESSION QUANTUM          |----->| disp conn |
 *     |    |  +------------------------------------+ \    +-----------+
 *     |    Q   Q       Q         Q       Q        Q  |         v    |
 *     |    |   |       |         |       |        |  | +----------+ |
 *     |  /-----|----+--|----+----|----+--|----+---|----| dispatch | |
 *     |  | |   |    |  |    |    |    |  |    |   |  | | request  | |
 *     |  v |   v    v  v    v    v    v  v    v   v  | +----------+ |
 *     | +-------+  +----+  +------+  +----+  +-----+ |      ^       |
 *     +>|CONTROL|  |DATA|  |IMAGE |  |TAPE|  |ROBOT| |      |       |
 *       |       |  |    |->|STREAM|<-|    |  |     | |      |       |
 *       |       |  |   *====*    *====*   |  |     | |      |  ndmconn_recv()
 *       | ndmca |  ndmda|  |ndmis |  ndmta|  |ndmra| |      |       |
 *       +-------+  +----+  +------+  +----+  +-----+ |      |resi   |
 *              |     | |    |    |       |           |   +------+   |
 *              \-----|-+----|----+-------+-------------->| call |   |
 *                    |      |                        |   +------+   |
 *           formatter|      |image_stream            |      |remo   |
 *                    v      v                        |      v       v
 *                   +---------+<---ndmchan_poll()----/     +---------+
 *                   | ndmchan |<---------------------------| ndmconn |
 *                   +---------+                            +---------+
 *                 non-blocking I/O                         XDR wrapper
 *
 *   -----> caller/callee
 *   --Q--> quantum (CPU scheduling)
 *   ====== image stream shared data structures
 *
 ********************************************************************
 */


#include "ndmp/ndmlib.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * VERSION AND RELEASE CONSTANTS -- KEEPER ONLY
 ****************************************************************
 *
 * Revision constants for the source code. These may only be
 * changed by the keeper of these sources. Contact ndmp-tech@ndmp.org
 * for a pointer to the latest sources and the current keeper.
 *
 * The Version increases every time there is a significant
 * design change. Significant means new functionality,
 * reorganization of key data structures, etc. The Release
 * increases every time the keeper releases an update to
 * the current version, such as bug fixes or integration
 * of new contributions.
 *
 * There are provisions for a free-form string for revisions
 * of the O/S specific portions (NDMOS_CONST_NDMOS_REVISION)
 * and for local revisions (NDMOS_CONST_NDMJOBLIB_REVISION)
 * which reflect in-house change levels.
 */
#define NDMJOBLIB_VERSION	1
#define NDMJOBLIB_RELEASE	2




struct ndm_session;		/* forward decl */




/*
 * NDM_ENV_TABLE and NDM_NLIST_TABLE
 ****************************************************************
 * Used by DATA and CONTROL agents
 */
#ifndef NDM_MAX_ENV
#define NDM_MAX_ENV		1024
#endif

struct ndm_env_entry {
	ndmp9_pval		pval;
	struct ndm_env_entry *	next;
};

struct ndm_env_table {
	int32_t			n_env;
	ndmp9_pval *		enumerate;
	int32_t			enumerate_length;
	struct ndm_env_entry *	head;
	struct ndm_env_entry *	tail;
};

/* ndma_listmgt.c */
extern ndmp9_pval *		ndma_enumerate_env_list (struct ndm_env_table *envtab);
extern struct ndm_env_entry *	ndma_store_env_list (struct ndm_env_table *envtab, ndmp9_pval *pv);
extern struct ndm_env_entry *	ndma_update_env_list (struct ndm_env_table *envtab, ndmp9_pval *pv);
extern void			ndma_destroy_env_list (struct ndm_env_table *envtab);

#ifndef NDM_MAX_NLIST
#define NDM_MAX_NLIST		10240
#endif

struct ndm_nlist_entry {
	ndmp9_name			name;
	ndmp9_error			result_err;
	unsigned			result_count;
	struct ndm_nlist_entry *	next;
};


struct ndm_nlist_table {
	int32_t				n_nlist;
	ndmp9_name *			enumerate;
	int32_t				enumerate_length;
	struct ndm_nlist_entry *	head;
	struct ndm_nlist_entry *	tail;
};


/* ndma_listmgt.c */
extern ndmp9_name *		ndma_enumerate_nlist (struct ndm_nlist_table *nlist);
extern struct ndm_nlist_entry *	ndma_store_nlist (struct ndm_nlist_table *nlist, ndmp9_name *nl);
extern void			ndma_destroy_nlist (struct ndm_nlist_table *nlist);

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
/*
 * CONTROL AGENT
 ****************************************************************
 */

#include "ndmp/smc.h"		/* SCSI Media Changer */


#ifndef NDM_MAX_MEDIA
#define NDM_MAX_MEDIA		40
#endif

struct ndm_media_table {
	int32_t			n_media;
	struct ndmmedia		*head;
	struct ndmmedia		*tail;
};

/* ndma_listmgt.c */
extern struct ndmmedia *	ndma_store_media (struct ndm_media_table *mtab, uint16_t element_address);
extern struct ndmmedia *	ndma_clone_media_entry (struct ndm_media_table *mtab, struct ndmmedia *to_clone);
extern void			ndmca_destroy_media_table (struct ndm_media_table *mtab);

#define NDM_JOB_OP_BACKUP	(0x100 | 'c')
#define NDM_JOB_OP_EXTRACT	(0x100 | 'x')
#define NDM_JOB_OP_TOC		(0x100 | 't')
#define NDM_JOB_OP_QUERY_AGENTS	(0x100 | 'q')
#define NDM_JOB_OP_INIT_LABELS	(0x100 | 'I')
#define NDM_JOB_OP_LIST_LABELS	(0x100 | 'L')
#define NDM_JOB_OP_REMEDY_ROBOT	(0x100 | 'Z')

/* test operations */
#define NDM_JOB_OP_TEST_TAPE	(0x200 | 'T')
#define NDM_JOB_OP_TEST_MOVER	(0x200 | 'M')
#define NDM_JOB_OP_TEST_DATA	(0x200 | 'D')

/* tape handling operations */
#define NDM_JOB_OP_REWIND_TAPE	(0x300 | 'r')
#define NDM_JOB_OP_EJECT_TAPE	(0x300 | 'j')
#define NDM_JOB_OP_MOVE_TAPE	(0x300 | 'm')
#define NDM_JOB_OP_LOAD_TAPE	(0x300 | 'l')
#define NDM_JOB_OP_UNLOAD_TAPE	(0x300 | 'u')
#define NDM_JOB_OP_IMPORT_TAPE	(0x300 | 'i')
#define NDM_JOB_OP_EXPORT_TAPE	(0x300 | 'e')
#define NDM_JOB_OP_INIT_ELEM_STATUS (0x300 | 'I')

/* daemon operations */
#define NDM_JOB_OP_DAEMON 'd'

struct ndm_job_param {
	int32_t			operation;	/* NDM_JOB_OP_... */
	int32_t			time_limit;	/* command timeout, 0 is off */

	struct ndmagent		data_agent;	/* DATA AGENT host/pw */
	char *			bu_type;	/* e.g. "tar" */
	int32_t			bu_level;	/* e.g. 0..9 for dump */
	struct ndm_env_table	env_tab;	/* for BACKUP+RECOVER ops */
	struct ndm_nlist_table	nlist_tab;	/* for RECOVER ops */
	struct ndm_env_table	result_env_tab;	/* after BACKUP */
	struct ndmlog		index_log;	/* to log NDMP_FH_ADD_... */

	struct ndmagent		tape_agent;	/* TAPE AGENT host/pw */
	char *			tape_device;	/* eg "/dev/rmt0" */
	unsigned		tape_timeout;	/* secs total to retry open */
	unsigned		record_size;	/* in bytes, 10k typical */
        uint64_t		last_w_offset;	/* last window offset sent */
	struct ndmscsi_target *	tape_target;	/* unused for now */
	char *			tape_tcp;	/* tcp direct */
	NDM_FLAG_DECL(use_eject)		/* eject upon close (unload) */

	struct ndmagent		robot_agent;	/* ROBOT AGENT host/pw */
	struct ndmscsi_target *	robot_target;	/* SCSI coord of robot */
	unsigned		robot_timeout;	/* secs total to retry move */
	NDM_FLAG_DECL(have_robot)		/* yes, we have robot, today */
	NDM_FLAG_DECL(auto_remedy)		/* if drive loaded, unload */
	NDM_FLAG_DECL(remedy_all)		/* OP_REMEDY, all drives */
	NDM_FLAG_DECL(drive_addr_given)
	NDM_FLAG_DECL(from_addr_given)
	NDM_FLAG_DECL(to_addr_given)
	unsigned		drive_addr;	/* 0->first, !0->elem addr */
	unsigned		from_addr;	/* for MOVE and EXPORT */
	unsigned		to_addr;	/* for MOVE and IMPORT */
						/* use move for many I/E */

	struct ndm_media_table	media_tab;	/* media to use, params */
	struct ndm_media_table	result_media_tab; /* results after job */

	uint32_t		n_file_entry;
	uint32_t		n_dir_entry;
	uint32_t		n_node_entry;
	uint64_t		root_node;

	uint64_t		bytes_written;
	uint64_t		bytes_read;
};

/* ndma_job.c */
extern int	ndma_job_audit (struct ndm_job_param *job,
				char *errbuf, int errskip);
extern int	ndma_job_media_audit (struct ndm_job_param *job,
				char *errbuf, int errskip);
extern void	ndma_job_auto_adjust (struct ndm_job_param *job);


struct ndm_control_agent {
	/* The JOB, see immediately above */
	struct ndm_job_param	job;
	NDM_FLAG_DECL(swap_connect)
	NDM_FLAG_DECL(has_tcp_addr)
	NDM_FLAG_DECL(has_local_addr)

	/* DATA agent */
	ndmp9_data_operation	data_op;
	ndmp9_data_get_state_reply data_state;
	NDM_FLAG_DECL(pending_notify_data_read)
	NDM_FLAG_DECL(pending_notify_data_halted)
	ndmp9_notify_data_read_request last_notify_data_read;
	ndmp9_addr		data_addr;
	int32_t			recover_log_file_count;
	int32_t			recover_log_file_ok;
	int32_t			recover_log_file_error;

	/* Image stream */
	ndmp9_addr		mover_addr;
	ndmp9_mover_mode	mover_mode;

	/* TAPE Agent */
	ndmp9_mover_get_state_reply mover_state;
	NDM_FLAG_DECL(pending_notify_mover_paused)
	NDM_FLAG_DECL(pending_notify_mover_halted)
	ndmp9_notify_mover_paused_request last_notify_mover_paused;

	ndmp9_tape_open_mode	tape_mode;
	ndmp9_tape_get_state_reply tape_state;

	/* Media management, media_table inside of job */
	int32_t			cur_media_ix;		/* references struct ndmmedia index field */
	NDM_FLAG_DECL(media_is_loaded)
	NDM_FLAG_DECL(is_label_op)

	/* ROBOT Agent */
	struct smc_ctrl_block *	smc_cb;
	unsigned		drive_addr;

#ifndef NDMOS_OPTION_NO_TEST_AGENTS
	/* when testing */
	char *			active_test;		/* name of test or 0 if no test */
	char *			active_test_failed;	/* active test failed */
	char *			active_test_warned;	/* active test warned */

	char *			test_phase;		/* name of sub-series test phase */
	int32_t			test_step;		/* test sequence number */

	int32_t			n_step_pass;		/* per phase test stats */
	int32_t			n_step_fail;
	int32_t			n_step_warn;
	int32_t			n_step_tests;

	int32_t			total_n_step_pass;	/* total test stats */
	int32_t			total_n_step_fail;
	int32_t			total_n_step_warn;
	int32_t			total_n_step_tests;
#endif

#ifdef NDMOS_MACRO_CONTROL_AGENT_ADDITIONS
	NDMOS_MACRO_CONTROL_AGENT_ADDITIONS
#endif /* NDMOS_MACRO_CONTROL_AGENT_ADDITIONS */
};


/* ndma_control.c */
extern int	ndmca_initialize (struct ndm_session *sess);
extern int	ndmca_commission (struct ndm_session *sess);
extern int	ndmca_decommission (struct ndm_session *sess);
extern int	ndmca_destroy (struct ndm_session *sess);
extern int	ndmca_control_agent (struct ndm_session *sess);

/* ndma_cops_backreco.c */
extern int	ndmca_op_create_backup (struct ndm_session *sess);
extern int	ndmca_op_recover_files (struct ndm_session *sess);
extern int	ndmca_op_recover_fh (struct ndm_session *sess);
extern int	ndmca_monitor_backup (struct ndm_session *sess);
extern int	ndmca_monitor_get_post_backup_env (struct ndm_session *sess);
extern int	ndmca_monitor_recover (struct ndm_session *sess);
extern int	ndmca_backreco_startup (struct ndm_session *sess);
extern int	ndmca_monitor_startup (struct ndm_session *sess);
extern int	ndmca_monitor_shutdown (struct ndm_session *sess);
extern int	ndmca_monitor_get_states (struct ndm_session *sess);
extern int	ndmca_monitor_load_next (struct ndm_session *sess);
extern int	ndmca_monitor_seek_tape (struct ndm_session *sess);
extern int	ndmca_monitor_unload_last_tape (struct ndm_session *sess);
extern int	ndmca_mon_wait_for_something (struct ndm_session *sess,
			int32_t max_delay_secs);

/* ndma_cops_labels.c */
extern int	ndmca_op_init_labels (struct ndm_session *sess);
extern int	ndmca_op_list_labels (struct ndm_session *sess);


/* ndma_cops_query.c */
extern int	ndmca_op_query (struct ndm_session *sess);
extern int	ndmca_opq_data (struct ndm_session *sess);
extern int	ndmca_opq_tape (struct ndm_session *sess);
extern int	ndmca_opq_robot (struct ndm_session *sess);
extern int	ndmca_opq_host_info (struct ndm_session *sess,
			struct ndmconn *conn);
extern int	ndmca_opq_get_mover_type (struct ndm_session *sess,
			struct ndmconn *conn);
extern int	ndmca_opq_get_butype_attr (struct ndm_session *sess,
			struct ndmconn *conn);
extern int	ndmca_opq_get_fs_info (struct ndm_session *sess,
			struct ndmconn *conn);
extern int	ndmca_opq_get_tape_info (struct ndm_session *sess,
			struct ndmconn *conn);
extern int	ndmca_opq_get_scsi_info (struct ndm_session *sess,
			struct ndmconn *conn);
extern void	ndmalogqr (struct ndm_session *sess, char *fmt, ...);


/* ndma_cops_robot.c */
extern int	ndmca_op_robot_remedy (struct ndm_session *sess);
extern int	ndmca_op_robot_startup (struct ndm_session *sess,
			int32_t verify_media_flag);
extern int	ndmca_op_init_elem_status (struct ndm_session *sess);
extern int	ndmca_op_rewind_tape (struct ndm_session *sess);
extern int	ndmca_op_eject_tape (struct ndm_session *sess);
extern int	ndmca_op_mtio (struct ndm_session *sess,
			ndmp9_tape_mtio_op mtio_op);

extern int	ndmca_op_move_tape (struct ndm_session *sess);
extern int	ndmca_op_import_tape (struct ndm_session *sess);
extern int	ndmca_op_export_tape (struct ndm_session *sess);
extern int	ndmca_op_load_tape (struct ndm_session *sess);
extern int	ndmca_op_unload_tape (struct ndm_session *sess);


/* ndma_ctrl_calls.c */
extern int	ndmca_connect_close (struct ndm_session *sess);
extern int	ndmca_data_get_state (struct ndm_session *sess);
extern int	ndmca_data_connect (struct ndm_session *sess);
extern int	ndmca_data_listen (struct ndm_session *sess);
extern int	ndmca_data_start_backup (struct ndm_session *sess);
extern int	ndmca_data_start_recover (struct ndm_session *sess);
extern int	ndmca_data_start_recover_filehist (struct ndm_session *sess);
extern int	ndmca_data_abort (struct ndm_session *sess);
extern int	ndmca_data_get_env (struct ndm_session *sess);
extern int	ndmca_data_stop (struct ndm_session *sess);
extern int	ndmca_tape_open (struct ndm_session *sess);
extern int	ndmca_tape_close (struct ndm_session *sess);
extern int	ndmca_tape_get_state (struct ndm_session *sess);
extern int	ndmca_tape_get_state_no_tattle (struct ndm_session *sess);
extern int	ndmca_tape_mtio (struct ndm_session *sess,
			ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid);
extern int	ndmca_tape_write (struct ndm_session *sess,
			char *buf, unsigned count);
extern int	ndmca_tape_read (struct ndm_session *sess,
			char *buf, unsigned count);
extern int	ndmca_mover_get_state (struct ndm_session *sess);
extern int	ndmca_mover_listen (struct ndm_session *sess);
extern int	ndmca_mover_connect (struct ndm_session *sess);
extern int	ndmca_mover_continue (struct ndm_session *sess);
extern int	ndmca_mover_abort (struct ndm_session *sess);
extern int	ndmca_mover_stop (struct ndm_session *sess);
extern int	ndmca_mover_set_window (struct ndm_session *sess,
			uint64_t offset, uint64_t length);
extern int	ndmca_mover_read (struct ndm_session *sess,
			uint64_t offset, uint64_t length);
extern int	ndmca_mover_close (struct ndm_session *sess);
extern int	ndmca_mover_set_record_size (struct ndm_session *sess);


/* ndma_ctrl_media.c */

struct ndmca_media_callbacks {
	int (*load_first)(struct ndm_session *sess);
	int (*load_next)(struct ndm_session *sess);
	int (*unload_current)(struct ndm_session *sess);
};

extern void	ndmca_media_register_callbacks (struct ndm_session *sess,
				struct ndmca_media_callbacks *callbacks);
extern void	ndmca_media_unregister_callbacks (struct ndm_session *sess);
extern int	ndmca_media_load_first (struct ndm_session *sess);
extern int	ndmca_media_load_next (struct ndm_session *sess);
extern int	ndmca_media_unload_last (struct ndm_session *sess);
extern int	ndmca_media_change (struct ndm_session *sess);
extern int	ndmca_media_load_seek (struct ndm_session *sess,
			uint64_t pos);
extern int	ndmca_media_load_current (struct ndm_session *sess);
extern int	ndmca_media_unload_current (struct ndm_session *sess);
extern int	ndmca_media_unload_best_effort (struct ndm_session *sess);
extern int	ndmca_media_open_tape (struct ndm_session *sess);
extern int	ndmca_media_close_tape (struct ndm_session *sess);
extern int	ndmca_media_mtio_tape (struct ndm_session *sess,
			ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid);
extern int	ndmca_media_write_filemarks (struct ndm_session *sess);
extern int	ndmca_media_read_label (struct ndm_session *sess,
			char labbuf[]);
extern int	ndmca_media_write_label (struct ndm_session *sess,
			int32_t type, char labbuf[]);
extern int	ndmca_media_check_label (struct ndm_session *sess,
			int32_t type, char labbuf[]);
extern int	ndmca_media_verify (struct ndm_session *sess);
extern int	ndmca_media_tattle (struct ndm_session *sess);
extern uint64_t
		ndmca_media_capture_tape_offset (struct ndm_session *sess);
extern int	ndmca_media_capture_mover_window (struct ndm_session *sess);
extern int	ndmca_media_calculate_windows (struct ndm_session *sess);
extern int	ndmca_media_calculate_offsets (struct ndm_session *sess);
extern int	ndmca_media_set_window_current (struct ndm_session *sess);


/* ndma_ctrl_robot.c */
extern int	ndmca_robot_issue_scsi_req (struct smc_ctrl_block *smc);
extern int	ndmca_robot_prep_target (struct ndm_session *sess);
extern int	ndmca_robot_obtain_info (struct ndm_session *sess);
extern int	ndmca_robot_init_elem_status (struct ndm_session *sess);
extern int	ndmca_robot_startup (struct ndm_session *sess);
extern int	ndmca_robot_move (struct ndm_session *sess,
			int32_t src_addr, int32_t dst_addr);
extern int	ndmca_robot_load (struct ndm_session *sess, int32_t slot_addr);
extern int	ndmca_robot_unload (struct ndm_session *sess, int32_t slot_addr);
extern struct smc_element_descriptor *
			ndmca_robot_find_element (struct ndm_session *sess,
							int32_t element_address);
extern int	ndmca_robot_check_ready (struct ndm_session *sess);
extern int	ndmca_robot_remedy_ready (struct ndm_session *sess);
extern int	ndmca_robot_query (struct ndm_session *sess);
extern int	ndmca_robot_verify_media (struct ndm_session *sess);
extern int	ndmca_robot_synthesize_media (struct ndm_session *sess);


/* ndma_ctrl_conn.c */
extern int	ndmca_connect_xxx_agent (struct ndm_session *sess,
			struct ndmconn **connp, char *prefix,
			struct ndmagent *agent);
extern int	ndmca_connect_data_agent (struct ndm_session *sess);
extern int	ndmca_connect_tape_agent (struct ndm_session *sess);
extern int	ndmca_connect_robot_agent (struct ndm_session *sess);
extern int	ndmca_connect_control_agent (struct ndm_session *sess);



/* ndma_ctst_tape.c */
extern int	ndmca_op_test_tape (struct ndm_session *sess);

/* ndma_ctst_mover.c */
extern int	ndmca_op_test_mover (struct ndm_session *sess);

/* ndma_ctst_data.c */
extern int	ndmca_op_test_data (struct ndm_session *sess);

/* ndma_ctst_subr.c */
extern int	ndmca_test_query_conn_types (struct ndm_session *sess,
					     struct ndmconn *ref_conn);
extern int	ndmca_test_load_tape (struct ndm_session *sess);
extern int	ndmca_test_unload_tape (struct ndm_session *sess);
extern int	ndmca_test_call (struct ndmconn *conn,
			struct ndmp_xa_buf *xa, ndmp9_error expect_err);
extern int	ndmca_test_check_expect_errs (struct ndmconn *conn,
			int32_t rc, ndmp9_error expect_errs[]);
extern int	ndmca_test_check_expect (struct ndmconn *conn,
			int32_t rc, ndmp9_error expect_err);
extern int	ndmca_test_check_expect_no_err (struct ndmconn *conn,
			int32_t rc);
extern int	ndmca_test_check_expect_illegal_state (struct ndmconn *conn,
			int32_t rc);
extern int	ndmca_test_check_expect_illegal_args (struct ndmconn *conn,
			int32_t rc);
extern void	ndmca_test_phase (struct ndm_session *sess,
			char *test_phase, char *desc);
extern void	ndmca_test_log_step (struct ndm_session *sess,
			int32_t level, char *msg);
extern void	ndmca_test_log_note (struct ndm_session *sess,
			int32_t level, char *msg);
extern void	ndmca_test_done_phase (struct ndm_session *sess);
extern void	ndmca_test_done_series (struct ndm_session *sess,
			char *series_name);
extern void	ndmca_test_open (struct ndm_session *sess,
				 char *test_name,
				 char *sub_test_name);
extern void	ndmca_test_warn (struct ndm_session *sess, char *warn_msg);
extern void	ndmca_test_fail (struct ndm_session *sess, char *fail_msg);
extern void	ndmca_test_close (struct ndm_session *sess);


extern void	ndmca_test_fill_data (char *buf, int32_t bufsize,
			int32_t recno, int32_t fileno);

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */




#ifndef NDMOS_OPTION_NO_DATA_AGENT
/*
 * DATA AGENT
 ****************************************************************
 *
 * NDMP Elements of Data Agent (backup)
 *
 *  +-----------+stderr
 *  | bu_type   |--------formatter_err---------NDMP_LOG-------> to CONTROL
 *  | formatter |
 *  | process   |========formatter_data=+=====================> to image stream
 *  +-----------+stdout                 |
 *                                  +-------+
 *                                  | snoop |--NDMP_FH_ADD_x--> to CONTROL
 *                                  +-------+
 *
 ****************************************************************
 *
 * NDMP Elements of Data Agent (recover)
 *
 *  +-----------+stderr
 *  | bu_type   |--------formatter_err---------NDMP_LOG-------> to CONTROL
 *  | formatter |                 +--------+
 *  | process   |<=======fmt_data=| direct |==================< from image str
 *  +-----------+stdin            |        |-NOTIFY_DATA_READ-> to CONTROL
 *                                | snoop  |
 *                                +--------+
 *
 ****************************************************************
 *
 * NDMP Elements of Data Agent (recover filehist)
 *
 *                                +--------+
 *                                | direct |==================< from image str
 *                                |        |-NOTIFY_DATA_READ-> to CONTROL
 *                                | snoop  |--NDMP_FH_ADD_x---> to CONTROL
 *                                +--------+
 *
 ****************************************************************
 */



#ifndef NDMDA_N_FMT_IMAGE_BUF
#define NDMDA_N_FMT_IMAGE_BUF	(8*1024)
#endif
#ifndef NDMDA_N_FHH_BUF
#define NDMDA_N_FHH_BUF		(8*1024)
#endif
#ifndef NDMDA_N_FMT_ERROR_BUF
#define NDMDA_N_FMT_ERROR_BUF	(8*1024)
#endif
#ifndef NDMDA_N_FMT_WRAP_BUF
#define NDMDA_N_FMT_WRAP_BUF	(4*1024)
#endif
#ifndef NDMDA_MAX_CMD
#define NDMDA_MAX_CMD		(4*1024)
#endif



#ifndef NDMOS_OPTION_NO_GTAR_BUTYPE
extern int	ndmda_butype_gtar_config_get_attrs (struct ndm_session *sess,
					uint32_t *attrs_p, int32_t protocol_version);

extern int	ndmda_butype_gtar_config_get_default_env (
					struct ndm_session *sess,
					ndmp9_pval **env_p, int32_t *n_env_p,
					int32_t protocol_version);
extern int	ndmda_butype_gtar_attach (struct ndm_session *sess);
#endif
#ifndef NDMOS_OPTION_NO_DUMP_BUTYPE
extern int	ndmda_butype_dump_config_get_attrs (struct ndm_session *sess,
					uint32_t *attrs_p, int32_t protocol_version);

extern int	ndmda_butype_dump_config_get_default_env (
					struct ndm_session *sess,
					ndmp9_pval **env_p, int32_t *n_env_p,
					int32_t protocol_version);
extern int	ndmda_butype_dump_attach (struct ndm_session *sess);
#endif




struct ndm_data_recovery_interval {
	ndmp9_u_quad	offset;
	ndmp9_u_quad	length;
};

enum ndm_data_recovery_access_method {
	NDMDA_RECO_ACCESS_SEQUENTIAL = 1,
	NDMDA_RECO_ACCESS_DIRECT,
	NDMDA_RECO_ACCESS_SEMI_DIRECT,
	NDMDA_RECO_ACCESS_SEMI_DIRECT_PENDING
};

enum ndm_data_recovery_state {
	NDMDA_RECO_STATE_START = 1,
	NDMDA_RECO_STATE_PASS_THRU,
	NDMDA_RECO_STATE_CHOOSE_NLENT,
	NDMDA_RECO_STATE_ACQUIRE,
	NDMDA_RECO_STATE_DISPOSE,
	NDMDA_RECO_STATE_FINISH_NLENT,
	NDMDA_RECO_STATE_ALL_DONE
};

enum ndm_data_recovery_acquire_mode {
	NDMDA_RECO_ACQUIRE_EVERYTHING = 1,
	NDMDA_RECO_ACQUIRE_SEARCHING,
	NDMDA_RECO_ACQUIRE_MATCHING
};

enum ndm_data_recovery_disposition {
	NDMDA_RECO_DISPOSITION_PASS = 1,
	NDMDA_RECO_DISPOSITION_DISCARD
};


struct ndm_data_agent {
	int32_t			protocol_version;

	char			bu_type[32];
	struct ndm_env_table	env_tab;
	struct ndm_nlist_table	nlist_tab;

	NDM_FLAG_DECL(enable_hist)
	uint64_t		pass_resid;

	ndmp9_data_get_state_reply data_state;
	int32_t			data_notify_pending;

	struct ndmchan		formatter_image;	/* stdin/out */
	struct ndmchan		formatter_error;	/* stderr */
	struct ndmchan		formatter_wrap;		/* fd=3 */
	int32_t			formatter_pid;

	char *			fmt_image_buf;
	char *			fmt_error_buf;
	char *			fmt_wrap_buf;

	struct ndmfhheap	fhh;
	uint32_t *		fhh_buf;

#ifdef NDMOS_MACRO_DATA_AGENT_ADDITIONS
	NDMOS_MACRO_DATA_AGENT_ADDITIONS
#endif /* NDMOS_MACRO_DATA_AGENT_ADDITIONS */
};



/* ndma_data.c */
extern int		ndmda_initialize (struct ndm_session *sess);
extern int		ndmda_commission (struct ndm_session *sess);
extern int		ndmda_decommission (struct ndm_session *sess);
extern int		ndmda_destroy (struct ndm_session *sess);
extern int		ndmda_belay (struct ndm_session *sess);

extern ndmp9_error	ndmda_data_start_backup (struct ndm_session *sess);
extern ndmp9_error	ndmda_data_start_recover (struct ndm_session *sess);
extern ndmp9_error	ndmda_data_start_recover_fh (struct ndm_session *sess);

extern void		ndmda_sync_state (struct ndm_session *sess);
extern ndmp9_error	ndmda_data_listen (struct ndm_session *sess);
extern ndmp9_error	ndmda_data_connect (struct ndm_session *sess);
extern void		ndmda_data_abort (struct ndm_session *sess);
extern void		ndmda_sync_environment (struct ndm_session *sess);
extern void		ndmda_data_halt (struct ndm_session *sess,
				ndmp9_data_halt_reason reason);
extern void		ndmda_data_stop (struct ndm_session *sess);

extern int		ndmda_quantum (struct ndm_session *sess);
extern int		ndmda_quantum_image (struct ndm_session *sess);
extern int		ndmda_quantum_stderr (struct ndm_session *sess);
extern int		ndmda_quantum_wrap (struct ndm_session *sess);

extern int		ndmda_wrap_in (struct ndm_session *sess,
				char *wrap_line);

extern void		ndmda_send_logmsg (struct ndm_session *sess,
				char *fmt, ...);

extern void		ndmda_send_notice (struct ndm_session *sess);
extern void		ndmda_send_data_read (struct ndm_session *sess,
				uint64_t offset, uint64_t length);

extern int		ndmda_copy_environment (struct ndm_session *sess,
				ndmp9_pval *env, unsigned n_env);
extern struct ndmp9_pval *ndmda_find_env (struct ndm_session *sess,
				char *name);

extern int		ndmda_interpret_boolean_value (char *value_str,
				int32_t default_value);

extern void		ndmda_purge_environment (struct ndm_session *sess);

extern int		ndmda_copy_nlist (struct ndm_session *sess,
				ndmp9_name *nlist, unsigned n_nlist);
extern void		ndmda_purge_nlist (struct ndm_session *sess);
extern int		ndmda_count_invalid_fh_info (struct ndm_session *sess);
extern int		ndmda_count_invalid_fh_info_pending
						(struct ndm_session *sess);

/* in ndma_data_recover.c */
extern int	ndmda_quantum_recover_common (struct ndm_session *sess);
extern int	ndmda_reco_state_start (struct ndm_session *sess);
extern int	ndmda_reco_pass_thru (struct ndm_session *sess);
extern int	ndmda_reco_state_pass_thru (struct ndm_session *sess);
extern int	ndmda_reco_state_choose_nlent (struct ndm_session *sess);
extern int	ndmda_reco_state_acquire (struct ndm_session *sess);
extern int	ndmda_reco_state_dispose (struct ndm_session *sess);
extern int	ndmda_reco_state_finish_nlent (struct ndm_session *sess);
extern int	ndmda_reco_state_all_done (struct ndm_session *sess);
extern int	ndmda_reco_assess_channels (struct ndm_session *sess);
extern int	ndmda_reco_assess_intervals (struct ndm_session *sess);
extern int	ndmda_reco_align_to_wanted (struct ndm_session *sess);
extern int	ndmda_reco_obtain_wanted (struct ndm_session *sess);
extern int	ndmda_reco_send_data_read (struct ndm_session *sess,
						uint64_t offset, uint64_t length);
extern int	ndmda_reco_internal_error (struct ndm_session *sess,
						char *why);

/* ndma_data_fh.c */
extern int		ndmda_fh_initialize (struct ndm_session *sess);
extern int		ndmda_fh_commission (struct ndm_session *sess);
extern int		ndmda_fh_decommission (struct ndm_session *sess);
extern int		ndmda_fh_destroy (struct ndm_session *sess);
extern int		ndmda_fh_belay (struct ndm_session *sess);
extern void		ndmda_fh_add_file (struct ndm_session *sess,
				ndmp9_file_stat *filestat, char *name);
extern void		ndmda_fh_add_dir (struct ndm_session *sess,
				uint64_t dir_fileno,
				char *name,
				uint64_t fileno);
extern void		ndmda_fh_add_node (struct ndm_session *sess,
				ndmp9_file_stat *filestat);

extern int		ndmda_fh_prepare (struct ndm_session *sess,
				  int vers, int msg, int entry_size,
				  unsigned n_item,
				  unsigned total_size_of_items);
extern void		ndmda_fh_flush (struct ndm_session *sess);

/* ndma_data_pfe.c (pipe-fork-exec) */
extern int		ndmda_pipe_fork_exec (struct ndm_session *sess,
				char *cmd, int is_backup);
extern int		ndmda_add_to_cmd_with_escapes (char *cmd,
				char *word, char *special);
extern int		ndmda_add_to_cmd (char *cmd, char *word);
extern int		ndmda_add_to_cmd_allow_file_wildcards (char *cmd,
				char *word);

#endif /* !NDMOS_OPTION_NO_DATA_AGENT */




#ifndef NDMOS_OPTION_NO_TAPE_AGENT
/*
 * TAPE AGENT
 ****************************************************************
 */

struct ndm_tape_agent {
	int			protocol_version;

	/* TAPE */
	ndmp9_tape_get_state_reply tape_state;

	/* MOVER */
	ndmp9_mover_get_state_reply mover_state;
	uint32_t		mover_window_first_blockno;
	uint64_t		mover_window_end;
	uint64_t		mover_want_pos;
	int			mover_notify_pending;

	int			pending_change_after_drain;
	ndmp9_mover_state	pending_mover_state;
	ndmp9_mover_halt_reason	pending_mover_halt_reason;
	ndmp9_mover_pause_reason pending_mover_pause_reason;

	char			*tape_buffer;
	uint32_t		tb_blockno;

#ifdef NDMOS_MACRO_TAPE_AGENT_ADDITIONS
	NDMOS_MACRO_TAPE_AGENT_ADDITIONS
#endif /* NDMOS_MACRO_DATA_AGENT_ADDITIONS */
};

#define NDMTA_TAPE_IS_WRITABLE(TA) \
	(   (TA)->tape_state.open_mode == NDMP9_TAPE_RDWR_MODE \
	 || (TA)->tape_state.open_mode == NDMP9_TAPE_RAW_MODE)

extern int		ndmta_initialize (struct ndm_session *sess);
extern int		ndmta_commission (struct ndm_session *sess);
extern int		ndmta_decommission (struct ndm_session *sess);
extern int		ndmta_destroy (struct ndm_session *sess);
extern int		ndmta_init_mover_state (struct ndm_session *sess);

extern void		ndmta_mover_sync_state (struct ndm_session *sess);
ndmp9_error		ndmta_mover_listen (struct ndm_session *sess,
				ndmp9_mover_mode mover_mode);
ndmp9_error		ndmta_mover_connect (struct ndm_session *sess,
				ndmp9_mover_mode mover_mode);
extern void		ndmta_mover_halt (struct ndm_session *sess,
				ndmp9_mover_halt_reason reason);
extern void		ndmta_mover_pause (struct ndm_session *sess,
				ndmp9_mover_pause_reason reason);
extern void		ndmta_mover_active (struct ndm_session *sess);
extern void		ndmta_mover_start_active (struct ndm_session *sess);
extern void		ndmta_mover_stop (struct ndm_session *sess);
extern void		ndmta_mover_abort (struct ndm_session *sess);
extern void		ndmta_mover_continue (struct ndm_session *sess);
extern void		ndmta_mover_close (struct ndm_session *sess);
extern void		ndmta_mover_read (struct ndm_session *sess,
				uint64_t offset, uint64_t length);

extern int		ndmta_quantum (struct ndm_session *sess);
extern int		ndmta_read_quantum (struct ndm_session *sess);
extern int		ndmta_write_quantum (struct ndm_session *sess);
extern void		ndmta_mover_send_notice (struct ndm_session *sess);

#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */




#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
/*
 * ROBOT AGENT
 ****************************************************************
 */

struct ndm_robot_agent {
	int			protocol_version;

	ndmp9_scsi_get_state_reply scsi_state;

#ifdef NDMOS_MACRO_ROBOT_AGENT_ADDITIONS
	NDMOS_MACRO_ROBOT_AGENT_ADDITIONS
#endif /* NDMOS_MACRO_ROBOT_AGENT_ADDITIONS */
};

extern int		ndmra_initialize (struct ndm_session *sess);
extern int		ndmra_commission (struct ndm_session *sess);
extern int		ndmra_decommission (struct ndm_session *sess);
extern int		ndmra_destroy (struct ndm_session *sess);

/* all semantic operations are done directly to the ndmos_scsi layer */
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */




/*
 * IMAGE STREAM
 ****************************************************************
 */

enum ndmis_connect_status {
	NDMIS_CONN_IDLE = 0,
	NDMIS_CONN_LISTEN,
	NDMIS_CONN_ACCEPTED,
	NDMIS_CONN_CONNECTED,
	NDMIS_CONN_DISCONNECTED,
	NDMIS_CONN_CLOSED,
	NDMIS_CONN_BOTCHED,
	NDMIS_CONN_REMOTE,
	NDMIS_CONN_EXCLUDE
};
typedef enum ndmis_connect_status	ndmis_connect_status;

struct ndmis_end_point {
	char *			name;
	ndmis_connect_status	connect_status;
	int			transfer_mode;
	ndmp9_addr_type		addr_type;
};

struct ndmis_remote {
	ndmis_connect_status	connect_status;
	int			transfer_mode;
	ndmp9_addr		local_addr;
	ndmp9_addr		peer_addr;
	ndmp9_addr		listen_addr;
	struct ndmchan		listen_chan;
	struct ndmchan		sanity_chan;
};

struct ndm_image_stream {
	struct ndmis_end_point	data_ep;
	struct ndmis_end_point	tape_ep;

	struct ndmis_remote	remote;

	/* transfer stuff */
	int			transfer_mode;
	struct ndmchan		chan;
	char *			buf;
	int			buflen;
};

extern int		ndmis_initialize (struct ndm_session *sess);
extern int		ndmis_commission (struct ndm_session *sess);
extern int		ndmis_decommission (struct ndm_session *sess);
extern int		ndmis_destroy (struct ndm_session *sess);
extern int		ndmis_belay (struct ndm_session *sess);

extern int		ndmis_quantum (struct ndm_session *sess);

extern ndmp9_error	ndmis_data_listen (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, ndmp9_addr *ret_addr,
			  char *reason);
extern ndmp9_error	ndmis_tape_listen (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, ndmp9_addr *ret_addr,
			  char *reason);
extern ndmp9_error	ndmis_data_connect (struct ndm_session *sess,
			  ndmp9_addr *addr, char *reason);
extern ndmp9_error	ndmis_tape_connect (struct ndm_session *sess,
			  ndmp9_addr *addr, char *reason);
extern int
ndmis_data_start (struct ndm_session *sess, int chan_mode);
extern int
ndmis_tape_start (struct ndm_session *sess, int chan_mode);
extern int
ndmis_data_close (struct ndm_session *sess);
extern int
ndmis_tape_close (struct ndm_session *sess);


extern ndmp9_error	ndmis_audit_data_listen (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, char *reason);
extern ndmp9_error	ndmis_audit_tape_listen (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, char *reason);
extern ndmp9_error	ndmis_audit_data_connect (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, char *reason);
extern ndmp9_error	ndmis_audit_tape_connect (struct ndm_session *sess,
			  ndmp9_addr_type addr_type, char *reason);

extern ndmp9_error	ndmis_audit_ep_listen (
			  struct ndm_session *sess,
			  ndmp9_addr_type addr_type,
			  char *reason,
			  struct ndmis_end_point *mine_ep,
			  struct ndmis_end_point *peer_ep);

extern ndmp9_error	ndmis_audit_ep_connect (
			  struct ndm_session *sess,
			  ndmp9_addr_type addr_type,
			  char *reason,
			  struct ndmis_end_point *mine_ep,
			  struct ndmis_end_point *peer_ep);

extern ndmp9_error	ndmis_ep_listen (
			  struct ndm_session *sess,
			  ndmp9_addr_type addr_type,
			  ndmp9_addr *ret_addr,
			  char *reason,
			  struct ndmis_end_point *mine_ep,
			  struct ndmis_end_point *peer_ep);

extern ndmp9_error	ndmis_ep_connect (
			  struct ndm_session *sess,
			  ndmp9_addr *addr,
			  char *reason,
			  struct ndmis_end_point *mine_ep,
			  struct ndmis_end_point *peer_ep);

extern int		ndmis_ep_close (
			  struct ndm_session *sess,
			  struct ndmis_end_point *mine_ep,
			  struct ndmis_end_point *peer_ep);

extern int		ndmis_tcp_listen (struct ndm_session *sess,
			  struct ndmp9_addr *listen_addr);
extern int		ndmis_tcp_accept (struct ndm_session *sess);
extern int		ndmis_tcp_connect (struct ndm_session *sess,
			  struct ndmp9_addr *connect_addr);
extern int		ndmis_tcp_close (struct ndm_session *sess);

extern int		ndmis_tcp_get_local_and_peer_addrs (
			  struct ndm_session *sess);
extern int		ndmis_tcp_green_light (struct ndm_session *sess,
			  int sock, ndmis_connect_status new_status);



/*
 * PLUMBING
 ****************************************************************
 */

struct ndm_plumbing {
	struct ndmconn *		control;
	struct ndmconn *		data;
	struct ndmconn *		tape;
	struct ndmconn *		robot;

	struct ndm_image_stream *	image_stream;
};




/*
 * SESSION
 ****************************************************************
 */

struct ndm_session_param {
	struct ndmlog		log;
	char *			log_tag;
	int			log_level;
	char *			config_file_name;
};

struct ndm_session {
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
	struct ndm_control_agent *control_acb;
	struct ndmca_media_callbacks *nmc;
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
#ifndef NDMOS_OPTION_NO_DATA_AGENT
	struct ndm_data_agent	*data_acb;
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
	struct ndm_tape_agent	*tape_acb;
	struct ndm_tape_simulator_callbacks *ntsc;
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
	struct ndm_robot_agent	*robot_acb;
	struct ndm_robot_simulator_callbacks *nrsc;
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */

	struct ndm_auth_callbacks *nac;

	struct ndm_plumbing	plumb;

	struct ndm_session_param *param;

	/* scratch pad stuff */
	ndmp9_config_info	*config_info;
	char			md5_challenge[64]; /* CONNECT_AUTH MD5 */

	NDM_FLAG_DECL(conn_open)
	NDM_FLAG_DECL(conn_authorized)
	NDM_FLAG_DECL(conn_snooping)
	NDM_FLAG_DECL(md5_challenge_valid)
	NDM_FLAG_DECL(control_agent_enabled)
	NDM_FLAG_DECL(data_agent_enabled)
	NDM_FLAG_DECL(tape_agent_enabled)
	NDM_FLAG_DECL(robot_agent_enabled)
	NDM_FLAG_DECL(dump_media_info)
	NDM_FLAG_DECL(error_raised)

	int			connect_status;

	/* Void pointer to session specific data the calling process wants to associate */
	void *			session_handle;

#ifdef NDMOS_MACRO_SESSION_ADDITIONS
	NDMOS_MACRO_SESSION_ADDITIONS
#endif /* NDMOS_MACRO_SESSION_ADDITIONS */
};


/* ndma_session.c */
extern int	ndma_client_session (struct ndm_session *sess,
			struct ndm_job_param *job, int swap_connect);
extern int	ndma_server_session (struct ndm_session *sess,
			int control_sock);
extern int	ndma_daemon_session (struct ndm_session *sess, int port);
extern int	ndma_session_quantum (struct ndm_session *sess,
			int max_delay_secs);
extern int	ndma_session_initialize (struct ndm_session *sess);
extern int	ndma_session_commission (struct ndm_session *sess);
extern int	ndma_session_decommission (struct ndm_session *sess);
extern int	ndma_session_destroy (struct ndm_session *sess);




/* ndma_comm_subr.c */
extern void	ndmalogf (struct ndm_session *sess, char *tag,
					int level, char *fmt, ...);
extern void	ndmalogfv (struct ndm_session *sess, char *tag,
					int level, char *fmt, va_list ap);




/*
 * Dispatch Version/Reqeust Tables (DVT, DRT)
 ****************************************************************
 */

struct ndm_dispatch_request_table {
	uint16_t	message;
	uint16_t	flags;
	int		(*dispatch_request) (	/* "dr" for short */
				struct ndm_session *sess,
				struct ndmp_xa_buf *xa,
				struct ndmconn *ref_conn);
};

struct ndm_dispatch_version_table {
	int					protocol_version;
	struct ndm_dispatch_request_table *	dispatch_request_table;
};

#define NDM_DRT_FLAG_OK_NOT_CONNECTED		0x0001
#define NDM_DRT_FLAG_OK_NOT_AUTHORIZED		0x0002


/* ndma_comm_dispatch.c */
extern int	ndma_dispatch_request (struct ndm_session *sess,
			struct ndmp_xa_buf *xa, struct ndmconn *ref_conn);
extern int	ndma_dispatch_raise_error (struct ndm_session *sess,
			struct ndmp_xa_buf *xa, struct ndmconn *ref_conn,
			ndmp9_error error, char *errstr);
extern int	ndma_dispatch_conn (struct ndm_session *sess,
			struct ndmconn *conn);
extern void	ndma_dispatch_ctrl_unexpected (struct ndmconn *conn,
			struct ndmp_msg_buf *nmb);
extern int	ndmta_local_mover_read (struct ndm_session *sess,
			uint64_t offset, uint64_t length);
extern int	ndma_call_no_tattle (struct ndmconn *conn,
			struct ndmp_xa_buf *xa);
extern int	ndma_call (struct ndmconn *conn, struct ndmp_xa_buf *xa);
extern int	ndma_send_to_control (struct ndm_session *sess,
			struct ndmp_xa_buf *xa,
			struct ndmconn *from_conn);
extern int	ndma_tattle (struct ndmconn *conn,
			struct ndmp_xa_buf *xa, int rc);
extern struct ndm_dispatch_request_table *
		ndma_drt_lookup (struct ndm_dispatch_version_table *dvt,
			unsigned protocol_version, unsigned message);

#define NDMADR_RAISE(ERROR,ERRSTR) \
	   return ndma_dispatch_raise_error (sess, xa, ref_conn, ERROR, ERRSTR)

#define NDMADR_RAISE_ILLEGAL_ARGS(ERRSTR) \
			NDMADR_RAISE(NDMP9_ILLEGAL_ARGS_ERR, ERRSTR)
#define NDMADR_RAISE_ILLEGAL_STATE(ERRSTR) \
			NDMADR_RAISE(NDMP9_ILLEGAL_STATE_ERR, ERRSTR)




#define NDMADR_UNIMPLEMENTED_MESSAGE	(-1)	/* aka "TODO" */
#define NDMADR_UNSPECIFIED_MESSAGE	(-123)	/* no such per specs */
#define NDMADR_UNIMPLEMENTED_VERSION	(-1234)	/* implementation error */




/*
 * Operating System Specific
 ****************************************************************
 * Must be implemented in ndmos_xxx.c
 */

/* from ndma_dispatch_request() in ndma_dispatch.c */
extern int		ndmos_dispatch_request (struct ndm_session *sess,
				struct ndmp_xa_buf *xa,
				struct ndmconn *ref_conn);

/* from ndmadr_connect_client_auth() in ndma_dispatch.c */
struct ndm_auth_callbacks {
	int (*validate_password)(struct ndm_session *sess, char *name, char *pass);
	int (*validate_md5)(struct ndm_session *sess, char *name, char digest[16]);
};

extern void		ndmos_auth_register_callbacks (struct ndm_session *sess,
				struct ndm_auth_callbacks *callbacks);
extern void		ndmos_auth_unregister_callbacks (struct ndm_session *sess);
extern int		ndmos_ok_name_password (struct ndm_session *sess,
				char *name, char *pass);
extern int		ndmos_get_md5_challenge (struct ndm_session *sess);
extern int		ndmos_ok_name_md5_digest (struct ndm_session *sess,
				char *name, char digest[16]);

/* from ndmadr_config_get_{host,server}_info() in ndma_dispatch.c */
extern void		ndmos_sync_config_info (struct ndm_session *sess);

/* from ndma_image_stream.c and ndma_ctrl_conn.c and others */
extern void		ndmos_condition_pipe_fd (struct ndm_session *sess,
				int fd);
extern void		ndmos_condition_listen_socket (
				struct ndm_session *sess, int sock);
extern void		ndmos_condition_control_socket (
				struct ndm_session *sess, int sock);
extern void		ndmos_condition_image_stream_socket (
				struct ndm_session *sess, int sock);

#ifndef NDMOS_OPTION_NO_TAPE_AGENT

struct ndm_tape_simulator_callbacks {
	ndmp9_error (*tape_open)(struct ndm_session *sess,
		char *drive_name, int will_write);
	ndmp9_error (*tape_close)(struct ndm_session *sess);
	ndmp9_error (*tape_mtio)(struct ndm_session *sess,
		ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid);
	ndmp9_error (*tape_write)(struct ndm_session *sess,
		char *buf, uint32_t count, uint32_t *done_count);
	ndmp9_error (*tape_wfm)(struct ndm_session *sess);
	ndmp9_error (*tape_read)(struct ndm_session *sess,
		char *buf, uint32_t count, uint32_t *done_count);
};

extern void		ndmos_tape_register_callbacks (struct ndm_session *sess,
				struct ndm_tape_simulator_callbacks *callbacks);
extern void		ndmos_tape_unregister_callbacks (struct ndm_session *sess);
extern int		ndmos_tape_initialize (struct ndm_session *sess);
extern ndmp9_error	ndmos_tape_open (struct ndm_session *sess,
				char *drive_name, int will_write);
extern ndmp9_error	ndmos_tape_close (struct ndm_session *sess);
extern void		ndmos_tape_sync_state (struct ndm_session *sess);
extern ndmp9_error	ndmos_tape_mtio (struct ndm_session *sess,
				ndmp9_tape_mtio_op op,
				uint32_t count, uint32_t *resid);
extern ndmp9_error	ndmos_tape_write (struct ndm_session *sess,
				char *buf,
				uint32_t count, uint32_t *done_count);
extern ndmp9_error	ndmos_tape_wfm (struct ndm_session *sess);
extern ndmp9_error	ndmos_tape_read (struct ndm_session *sess,
				char *buf,
				uint32_t count, uint32_t *done_count);
extern ndmp9_error	ndmos_tape_execute_cdb (struct ndm_session *sess,
				ndmp9_execute_cdb_request *request,
				ndmp9_execute_cdb_reply *reply);
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

#ifndef NDMOS_OPTION_NO_ROBOT_AGENT

struct ndm_robot_simulator_callbacks {
	ndmp9_error (*scsi_open)(struct ndm_session *sess, char *name);
	ndmp9_error (*scsi_close)(struct ndm_session *sess);
	ndmp9_error (*scsi_reset)(struct ndm_session *sess);
	ndmp9_error (*scsi_execute_cdb)(struct ndm_session *sess,
		ndmp9_execute_cdb_request *request,
		ndmp9_execute_cdb_reply *reply);
};

extern void		ndmos_scsi_register_callbacks (struct ndm_session *sess,
				struct ndm_robot_simulator_callbacks *callbacks);
extern void		ndmos_scsi_unregister_callbacks (struct ndm_session *sess);
extern int		ndmos_scsi_initialize (struct ndm_session *sess);
extern void		ndmos_scsi_sync_state (struct ndm_session *sess);
extern void		ndmos_scsi_sync_config_info (struct ndm_session *sess);
extern ndmp9_error	ndmos_scsi_open (struct ndm_session *sess, char *name);
extern ndmp9_error	ndmos_scsi_close (struct ndm_session *sess);
extern ndmp9_error	ndmos_scsi_set_target (struct ndm_session *sess);
extern ndmp9_error	ndmos_scsi_reset_device (struct ndm_session *sess);
extern ndmp9_error	ndmos_scsi_reset_bus (struct ndm_session *sess);
extern ndmp9_error	ndmos_scsi_execute_cdb (struct ndm_session *sess,
				ndmp9_execute_cdb_request *request,
				ndmp9_execute_cdb_reply *reply);
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */


/* ndma_noti_calls.c */
#ifndef NDMOS_OPTION_NO_DATA_AGENT
extern int	ndma_notify_data_halted (struct ndm_session *sess);
extern int	ndma_notify_data_read (struct ndm_session *sess,
			uint64_t offset, uint64_t length);
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
extern int	ndma_notify_mover_halted (struct ndm_session *sess);
extern int	ndma_notify_mover_paused (struct ndm_session *sess);
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#ifndef NDMOS_EFFECT_NO_SERVER_AGENTS
extern void	ndma_send_logmsg (struct ndm_session *sess,
			ndmp9_log_type ltype, struct ndmconn *from_conn,
			char *fmt, ...);

#ifdef  __cplusplus
}
#endif

#endif /* !NDMOS_EFFECT_NO_SERVER_AGENTS */
