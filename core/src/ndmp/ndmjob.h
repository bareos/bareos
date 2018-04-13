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



#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL char *			progname;

GLOBAL struct ndm_session	the_session;
GLOBAL struct ndm_session_param	the_param;

GLOBAL int			the_mode;
GLOBAL int			d_debug;
GLOBAL char *			L_log_file;
GLOBAL int			n_noop;
GLOBAL int			v_verbose;
GLOBAL int			o_no_time_stamps;
GLOBAL char *			o_config_file;
GLOBAL char *			o_tape_tcp;
GLOBAL char *			o_load_files_file;

GLOBAL FILE *			log_fp;

extern void			error_byebye (char *fmt, ...);

extern struct ndmp_enum_str_table	mode_long_name_table[];

extern int			process_args (int argc, char *argv[]);
extern int			handle_long_option (char *str);
extern void			set_job_mode (int mode);
extern void			usage (void);
extern void			help (void);
extern void			ndmjob_version_info (void);
extern void			dump_settings (void);

extern int			copy_args_expanding_macros (int argc, char *argv[],
					char *av[], int max_ac);
extern int			lookup_and_snarf (char *av[], char *name);
extern int			snarf_macro (char *av[], char *val);

extern void			ndmjob_log_deliver(struct ndmlog *log, char *tag,
					int lev, char *msg);

extern void			ndmjob_log (int level, char *fmt, ...);


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT

#define MAX_EXCLUDE_PATTERN	100
#define MAX_FILE_ARG		NDM_MAX_NLIST

GLOBAL char *			B_bu_type;
GLOBAL int			b_bsize;
GLOBAL char *			C_chdir;
GLOBAL struct ndmagent		D_data_agent;
GLOBAL struct ndm_env_table	E_environment;
GLOBAL char *			e_exclude_pattern[MAX_EXCLUDE_PATTERN];
GLOBAL int			n_e_exclude_pattern;
GLOBAL char *			f_tape_device;
GLOBAL char *			I_index_file;	/* output */
GLOBAL char *			J_index_file;	/* input */
GLOBAL struct ndm_media_table	media_;
GLOBAL struct ndmagent		R_robot_agent;
GLOBAL struct ndmscsi_target *	r_robot_target;
GLOBAL struct ndmagent		T_tape_agent;
GLOBAL char *			U_user;

GLOBAL int			o_time_limit;
GLOBAL int			o_swap_connect;
GLOBAL int			o_use_eject;
GLOBAL int			o_tape_addr;
GLOBAL int			o_from_addr;
GLOBAL int			o_to_addr;
GLOBAL struct ndmscsi_target *	o_tape_scsi;
GLOBAL int			o_tape_timeout;
GLOBAL int			o_robot_timeout;
GLOBAL char *			o_rules;
GLOBAL off_t			o_tape_limit;
GLOBAL int			p_ndmp_port;

GLOBAL char *			file_arg[MAX_FILE_ARG];
GLOBAL char *			file_arg_new[MAX_FILE_ARG];
GLOBAL int			n_file_arg;

/* The ji_ variables are set according to the -J input index */
GLOBAL struct ndm_media_table	ji_media;
GLOBAL struct ndm_env_table	ji_environment;

GLOBAL ndmp9_name		nlist[MAX_FILE_ARG];	/* parallels file_arg[] */

GLOBAL FILE *			index_fp;

GLOBAL struct ndm_job_param	the_job;

#define AGENT_GIVEN(AGENT)	(AGENT.conn_type != NDMCONN_TYPE_NONE)
#define ROBOT_GIVEN()		(r_robot_target)

extern void			ndmjob_ixlog_deliver(struct ndmlog *log, char *tag,
					int lev, char *msg);

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

extern void			ndmjob_register_callbacks (struct ndm_session *sess,
					struct ndmlog *ixlog);
extern void			ndmjob_unregister_callbacks (struct ndm_session *sess,
					struct ndmlog *ixlog);

extern int			ndmjobfhdb_add_file (struct ndmlog *ixlog, int tagc,
					char *raw_name, ndmp9_file_stat *fstat);
extern int			ndmjobfhdb_add_dir (struct ndmlog *ixlog, int tagc,
					char *raw_name, ndmp9_u_quad dir_node, ndmp9_u_quad node);
extern int			ndmjobfhdb_add_node (struct ndmlog *ixlog, int tagc,
					ndmp9_u_quad node, ndmp9_file_stat *fstat);
extern int			ndmjobfhdb_add_dirnode_root (struct ndmlog *ixlog, int tagc,
					ndmp9_u_quad root_node);

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
extern int			start_index_file (void);
extern int			sort_index_file (void);
extern int			build_job (void);
extern int			args_to_job (void);
extern int			args_to_job_backup_env (void);
extern int			args_to_job_recover_env (void);
extern int			args_to_job_recover_nlist (void);
extern int			jndex_doit (void);
extern int			jndex_tattle (void);
extern int			jndex_merge_media (void);
extern int			jndex_audit_not_found (void);
extern int			jndex_merge_environment (void);
extern int			jndex_fetch_post_backup_data_env (FILE *fp);
extern int			jndex_fetch_post_backup_media (FILE *fp);

extern int			apply_rules (struct ndm_job_param *job, char *rules);
extern int			help_rules (void);
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
