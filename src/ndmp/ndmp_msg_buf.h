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

#ifdef  __cplusplus
extern "C" {
#endif

#define NDMNMB_FLAG_NO_FREE	1
#define NDMNMB_FLAG_NO_SEND	2

/*
 * Most replies are regular in that 'error' is the
 * first field. This affords certain efficiencies
 * and conveniences in the implementation.
 * NDMPv3 introduced replies that broke this regularity.
 * This is used to work around such replies
 * in areas that otherwise take advantage
 * of the convenient regularity.
 */

struct ndmp3_unfortunate_error {
	uint32_t		invalid_probably;
	ndmp9_error		error;
};

/*
 * NDMNMB_IS_UNFORTUNATE_REPLY_TYPE(vers,msg)
 */
#ifdef NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE(vers,msg) 0
#else /* NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 */
#ifndef NDMOS_OPTION_NO_NDMP3
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V3(vers,msg) \
	((vers) == NDMP3VER			\
	 && ((msg) == NDMP3_TAPE_GET_STATE	\
	  || (msg) == NDMP3_DATA_GET_STATE))
#else /* !NDMOS_OPTION_NO_NDMP3 */
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V3(vers,msg) 0
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V4(vers,msg) \
	((vers) == NDMP4VER			\
	 && ((msg) == NDMP4_TAPE_GET_STATE	\
	  || (msg) == NDMP4_DATA_GET_STATE))
#else /* !NDMOS_OPTION_NO_NDMP4 */
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V4(vers,msg) 0
#endif /* !NDMOS_OPTION_NO_NDMP4 */
#define NDMNMB_IS_UNFORTUNATE_REPLY_TYPE(vers,msg) \
	(NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V3(vers,msg) \
	 || NDMNMB_IS_UNFORTUNATE_REPLY_TYPE_V4(vers,msg))
#endif /* NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 */

/* 92 bytes, checked 970930 */
struct ndmp_msg_buf {
	ndmp0_header		header;
	unsigned char		protocol_version;
	unsigned char		flags;
	unsigned char		_pad[2];
	union {
	  ndmp0_connect_open_request ndmp0_connect_open_request_body;
	  ndmp0_connect_open_reply ndmp0_connect_open_reply_body;
	  ndmp0_notify_connected_request ndmp0_notify_connected_request_body;

#ifndef NDMOS_OPTION_NO_NDMP2
	  ndmp2_error ndmp2_error_reply;
	  ndmp2_connect_open_request ndmp2_connect_open_request_body;
	  ndmp2_connect_open_reply ndmp2_connect_open_reply_body;
	  ndmp2_connect_client_auth_request ndmp2_connect_client_auth_request_body;
	  ndmp2_connect_client_auth_reply ndmp2_connect_client_auth_reply_body;
	  ndmp2_connect_server_auth_request ndmp2_connect_server_auth_request_body;
	  ndmp2_connect_server_auth_reply ndmp2_connect_server_auth_reply_body;
	  ndmp2_config_get_host_info_reply ndmp2_config_get_host_info_reply_body;
	  ndmp2_config_get_butype_attr_request ndmp2_config_get_butype_attr_request_body;
	  ndmp2_config_get_butype_attr_reply ndmp2_config_get_butype_attr_reply_body;
	  ndmp2_config_get_mover_type_reply ndmp2_config_get_mover_type_reply_body;
	  ndmp2_config_get_auth_attr_request ndmp2_config_get_auth_attr_request_body;
	  ndmp2_config_get_auth_attr_reply ndmp2_config_get_auth_attr_reply_body;
	  ndmp2_scsi_open_request ndmp2_scsi_open_request_body;
	  ndmp2_scsi_open_reply ndmp2_scsi_open_reply_body;
	  ndmp2_scsi_close_reply ndmp2_scsi_close_reply_body;
	  ndmp2_scsi_get_state_reply ndmp2_scsi_get_state_reply_body;
	  ndmp2_scsi_set_target_request ndmp2_scsi_set_target_request_body;
	  ndmp2_scsi_set_target_reply ndmp2_scsi_set_target_reply_body;
	  ndmp2_scsi_reset_device_reply ndmp2_scsi_reset_device_reply_body;
	  ndmp2_scsi_reset_bus_reply ndmp2_scsi_reset_bus_reply_body;
	  ndmp2_scsi_execute_cdb_request ndmp2_scsi_execute_cdb_request_body;
	  ndmp2_scsi_execute_cdb_reply ndmp2_scsi_execute_cdb_reply_body;
	  ndmp2_tape_open_request ndmp2_tape_open_request_body;
	  ndmp2_tape_open_reply ndmp2_tape_open_reply_body;
	  ndmp2_tape_close_reply ndmp2_tape_close_reply_body;
	  ndmp2_tape_get_state_reply ndmp2_tape_get_state_reply_body;
	  ndmp2_tape_mtio_request ndmp2_tape_mtio_request_body;
	  ndmp2_tape_mtio_reply ndmp2_tape_mtio_reply_body;
	  ndmp2_tape_write_request ndmp2_tape_write_request_body;
	  ndmp2_tape_write_reply ndmp2_tape_write_reply_body;
	  ndmp2_tape_read_request ndmp2_tape_read_request_body;
	  ndmp2_tape_read_reply ndmp2_tape_read_reply_body;
	  ndmp2_tape_execute_cdb_request ndmp2_tape_execute_cdb_request_body;
	  ndmp2_tape_execute_cdb_reply ndmp2_tape_execute_cdb_reply_body;
	  ndmp2_data_get_state_reply ndmp2_data_get_state_reply_body;
	  ndmp2_data_start_backup_request ndmp2_data_start_backup_request_body;
	  ndmp2_data_start_backup_reply ndmp2_data_start_backup_reply_body;
	  ndmp2_data_start_recover_request ndmp2_data_start_recover_request_body;
	  ndmp2_data_start_recover_reply ndmp2_data_start_recover_reply_body;
	  ndmp2_data_abort_reply ndmp2_data_abort_reply_body;
	  ndmp2_data_get_env_reply ndmp2_data_get_env_reply_body;
	  ndmp2_data_stop_reply ndmp2_data_stop_reply_body;
	  ndmp2_data_start_recover_filehist_request ndmp2_data_start_recover_filehist_request_body;
	  ndmp2_data_start_recover_filehist_reply ndmp2_data_start_recover_filehist_reply_body;
	  ndmp2_notify_data_halted_request ndmp2_notify_data_halted_request_body;
	  ndmp2_notify_connected_request ndmp2_notify_connected_request_body;
	  ndmp2_notify_mover_halted_request ndmp2_notify_mover_halted_request_body;
	  ndmp2_notify_mover_paused_request ndmp2_notify_mover_paused_request_body;
	  ndmp2_notify_data_read_request ndmp2_notify_data_read_request_body;
	  ndmp2_log_log_request ndmp2_log_log_request_body;
	  ndmp2_log_debug_request ndmp2_log_debug_request_body;
	  ndmp2_log_file_request ndmp2_log_file_request_body;
	  ndmp2_fh_add_unix_path_request ndmp2_fh_add_unix_path_request_body;
	  ndmp2_fh_add_unix_dir_request ndmp2_fh_add_unix_dir_request_body;
	  ndmp2_fh_add_unix_node_request ndmp2_fh_add_unix_node_request_body;
	  ndmp2_mover_get_state_reply ndmp2_mover_get_state_reply_body;
	  ndmp2_mover_listen_request ndmp2_mover_listen_request_body;
	  ndmp2_mover_listen_reply ndmp2_mover_listen_reply_body;
	  ndmp2_mover_continue_reply ndmp2_mover_continue_reply_body;
	  ndmp2_mover_abort_reply ndmp2_mover_abort_reply_body;
	  ndmp2_mover_stop_reply ndmp2_mover_stop_reply_body;
	  ndmp2_mover_set_window_request ndmp2_mover_set_window_request_body;
	  ndmp2_mover_set_window_reply ndmp2_mover_set_window_reply_body;
	  ndmp2_mover_read_request ndmp2_mover_read_request_body;
	  ndmp2_mover_read_reply ndmp2_mover_read_reply_body;
	  ndmp2_mover_close_reply ndmp2_mover_close_reply_body;
	  ndmp2_mover_set_record_size_request ndmp2_mover_set_record_size_request_body;
	  ndmp2_mover_set_record_size_reply ndmp2_mover_set_record_size_reply_body;

#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	  ndmp3_error ndmp3_error_reply;
	  ndmp3_connect_open_request ndmp3_connect_open_request_body;
	  ndmp3_connect_open_reply ndmp3_connect_open_reply_body;
	  ndmp3_connect_client_auth_request ndmp3_connect_client_auth_request_body;
	  ndmp3_connect_client_auth_reply ndmp3_connect_client_auth_reply_body;
	  ndmp3_connect_server_auth_request ndmp3_connect_server_auth_request_body;
	  ndmp3_connect_server_auth_reply ndmp3_connect_server_auth_reply_body;
	  ndmp3_config_get_host_info_reply ndmp3_config_get_host_info_reply_body;
	  ndmp3_config_get_connection_type_reply ndmp3_config_get_connection_type_reply_body;
	  ndmp3_config_get_auth_attr_request ndmp3_config_get_auth_attr_request_body;
	  ndmp3_config_get_auth_attr_reply ndmp3_config_get_auth_attr_reply_body;
	  ndmp3_config_get_butype_info_reply ndmp3_config_get_butype_info_reply_body;
	  ndmp3_config_get_fs_info_reply ndmp3_config_get_fs_info_reply_body;
	  ndmp3_config_get_tape_info_reply ndmp3_config_get_tape_info_reply_body;
	  ndmp3_config_get_scsi_info_reply ndmp3_config_get_scsi_info_reply_body;
	  ndmp3_config_get_server_info_reply ndmp3_config_get_server_info_reply_body;
	  ndmp3_scsi_open_request ndmp3_scsi_open_request_body;
	  ndmp3_scsi_open_reply ndmp3_scsi_open_reply_body;
	  ndmp3_scsi_close_reply ndmp3_scsi_close_reply_body;
	  ndmp3_scsi_get_state_reply ndmp3_scsi_get_state_reply_body;
	  ndmp3_scsi_set_target_request ndmp3_scsi_set_target_request_body;
	  ndmp3_scsi_set_target_reply ndmp3_scsi_set_target_reply_body;
	  ndmp3_scsi_reset_device_reply ndmp3_scsi_reset_device_reply_body;
	  ndmp3_scsi_reset_bus_reply ndmp3_scsi_reset_bus_reply_body;
	  ndmp3_scsi_execute_cdb_request ndmp3_scsi_execute_cdb_request_body;
	  ndmp3_scsi_execute_cdb_reply ndmp3_scsi_execute_cdb_reply_body;
	  ndmp3_tape_open_request ndmp3_tape_open_request_body;
	  ndmp3_tape_open_reply ndmp3_tape_open_reply_body;
	  ndmp3_tape_close_reply ndmp3_tape_close_reply_body;
	  ndmp3_tape_get_state_reply ndmp3_tape_get_state_reply_body;
	  ndmp3_tape_mtio_request ndmp3_tape_mtio_request_body;
	  ndmp3_tape_mtio_reply ndmp3_tape_mtio_reply_body;
	  ndmp3_tape_write_request ndmp3_tape_write_request_body;
	  ndmp3_tape_write_reply ndmp3_tape_write_reply_body;
	  ndmp3_tape_read_request ndmp3_tape_read_request_body;
	  ndmp3_tape_read_reply ndmp3_tape_read_reply_body;
	  ndmp3_tape_execute_cdb_request ndmp3_tape_execute_cdb_request_body;
	  ndmp3_tape_execute_cdb_reply ndmp3_tape_execute_cdb_reply_body;
	  ndmp3_data_get_state_reply ndmp3_data_get_state_reply_body;
	  ndmp3_data_start_backup_request ndmp3_data_start_backup_request_body;
	  ndmp3_data_start_backup_reply ndmp3_data_start_backup_reply_body;
	  ndmp3_data_start_recover_request ndmp3_data_start_recover_request_body;
	  ndmp3_data_start_recover_reply ndmp3_data_start_recover_reply_body;
	  ndmp3_data_abort_reply ndmp3_data_abort_reply_body;
	  ndmp3_data_get_env_reply ndmp3_data_get_env_reply_body;
	  ndmp3_data_stop_reply ndmp3_data_stop_reply_body;
	  ndmp3_data_start_recover_filehist_request ndmp3_data_start_recover_filehist_request_body;
	  ndmp3_data_start_recover_filehist_reply ndmp3_data_start_recover_filehist_reply_body;
	  ndmp3_data_listen_request ndmp3_data_listen_request_body;
	  ndmp3_data_listen_reply ndmp3_data_listen_reply_body;
	  ndmp3_data_connect_request ndmp3_data_connect_request_body;
	  ndmp3_data_connect_reply ndmp3_data_connect_reply_body;
	  ndmp3_notify_data_halted_request ndmp3_notify_data_halted_request_body;
	  ndmp3_notify_connected_request ndmp3_notify_connected_request_body;
	  ndmp3_notify_mover_halted_request ndmp3_notify_mover_halted_request_body;
	  ndmp3_notify_mover_paused_request ndmp3_notify_mover_paused_request_body;
	  ndmp3_notify_data_read_request ndmp3_notify_data_read_request_body;
	  ndmp3_log_file_request ndmp3_log_file_request_body;
	  ndmp3_log_message_request ndmp3_log_message_request_body;
	  ndmp3_fh_add_file_request ndmp3_fh_add_file_request_body;
	  ndmp3_fh_add_dir_request ndmp3_fh_add_dir_request_body;
	  ndmp3_fh_add_node_request ndmp3_fh_add_node_request_body;
	  ndmp3_mover_get_state_reply ndmp3_mover_get_state_reply_body;
	  ndmp3_mover_listen_request ndmp3_mover_listen_request_body;
	  ndmp3_mover_listen_reply ndmp3_mover_listen_reply_body;
	  ndmp3_mover_continue_reply ndmp3_mover_continue_reply_body;
	  ndmp3_mover_abort_reply ndmp3_mover_abort_reply_body;
	  ndmp3_mover_stop_reply ndmp3_mover_stop_reply_body;
	  ndmp3_mover_set_window_request ndmp3_mover_set_window_request_body;
	  ndmp3_mover_set_window_reply ndmp3_mover_set_window_reply_body;
	  ndmp3_mover_read_request ndmp3_mover_read_request_body;
	  ndmp3_mover_read_reply ndmp3_mover_read_reply_body;
	  ndmp3_mover_close_reply ndmp3_mover_close_reply_body;
	  ndmp3_mover_set_record_size_request ndmp3_mover_set_record_size_request_body;
	  ndmp3_mover_set_record_size_reply ndmp3_mover_set_record_size_reply_body;
	  ndmp3_mover_connect_request ndmp3_mover_connect_request_body;
	  ndmp3_mover_connect_reply ndmp3_mover_connect_reply_body;

#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	  ndmp4_error ndmp4_error_reply;
	  ndmp4_connect_open_request ndmp4_connect_open_request_body;
	  ndmp4_connect_open_reply ndmp4_connect_open_reply_body;
	  ndmp4_connect_client_auth_request ndmp4_connect_client_auth_request_body;
	  ndmp4_connect_client_auth_reply ndmp4_connect_client_auth_reply_body;
	  ndmp4_connect_server_auth_request ndmp4_connect_server_auth_request_body;
	  ndmp4_connect_server_auth_reply ndmp4_connect_server_auth_reply_body;
	  ndmp4_config_get_host_info_reply ndmp4_config_get_host_info_reply_body;
	  ndmp4_config_get_connection_type_reply ndmp4_config_get_connection_type_reply_body;
	  ndmp4_config_get_auth_attr_request ndmp4_config_get_auth_attr_request_body;
	  ndmp4_config_get_auth_attr_reply ndmp4_config_get_auth_attr_reply_body;
	  ndmp4_config_get_butype_info_reply ndmp4_config_get_butype_info_reply_body;
	  ndmp4_config_get_fs_info_reply ndmp4_config_get_fs_info_reply_body;
	  ndmp4_config_get_tape_info_reply ndmp4_config_get_tape_info_reply_body;
	  ndmp4_config_get_scsi_info_reply ndmp4_config_get_scsi_info_reply_body;
	  ndmp4_config_get_server_info_reply ndmp4_config_get_server_info_reply_body;
	  ndmp4_scsi_open_request ndmp4_scsi_open_request_body;
	  ndmp4_scsi_open_reply ndmp4_scsi_open_reply_body;
	  ndmp4_scsi_close_reply ndmp4_scsi_close_reply_body;
	  ndmp4_scsi_get_state_reply ndmp4_scsi_get_state_reply_body;
	  ndmp4_scsi_reset_device_reply ndmp4_scsi_reset_device_reply_body;
	  ndmp4_scsi_execute_cdb_request ndmp4_scsi_execute_cdb_request_body;
	  ndmp4_scsi_execute_cdb_reply ndmp4_scsi_execute_cdb_reply_body;
	  ndmp4_tape_open_request ndmp4_tape_open_request_body;
	  ndmp4_tape_open_reply ndmp4_tape_open_reply_body;
	  ndmp4_tape_close_reply ndmp4_tape_close_reply_body;
	  ndmp4_tape_get_state_reply ndmp4_tape_get_state_reply_body;
	  ndmp4_tape_mtio_request ndmp4_tape_mtio_request_body;
	  ndmp4_tape_mtio_reply ndmp4_tape_mtio_reply_body;
	  ndmp4_tape_write_request ndmp4_tape_write_request_body;
	  ndmp4_tape_write_reply ndmp4_tape_write_reply_body;
	  ndmp4_tape_read_request ndmp4_tape_read_request_body;
	  ndmp4_tape_read_reply ndmp4_tape_read_reply_body;
	  ndmp4_tape_execute_cdb_request ndmp4_tape_execute_cdb_request_body;
	  ndmp4_tape_execute_cdb_reply ndmp4_tape_execute_cdb_reply_body;
	  ndmp4_data_get_state_reply ndmp4_data_get_state_reply_body;
	  ndmp4_data_start_backup_request ndmp4_data_start_backup_request_body;
	  ndmp4_data_start_backup_reply ndmp4_data_start_backup_reply_body;
	  ndmp4_data_start_recover_request ndmp4_data_start_recover_request_body;
	  ndmp4_data_start_recover_reply ndmp4_data_start_recover_reply_body;
	  ndmp4_data_abort_reply ndmp4_data_abort_reply_body;
	  ndmp4_data_get_env_reply ndmp4_data_get_env_reply_body;
	  ndmp4_data_stop_reply ndmp4_data_stop_reply_body;
	  ndmp4_data_start_recover_filehist_request ndmp4_data_start_recover_filehist_request_body;
	  ndmp4_data_start_recover_filehist_reply ndmp4_data_start_recover_filehist_reply_body;
	  ndmp4_data_listen_request ndmp4_data_listen_request_body;
	  ndmp4_data_listen_reply ndmp4_data_listen_reply_body;
	  ndmp4_data_connect_request ndmp4_data_connect_request_body;
	  ndmp4_data_connect_reply ndmp4_data_connect_reply_body;
	  ndmp4_notify_data_halted_post ndmp4_notify_data_halted_post_body;
	  ndmp4_notify_connection_status_post ndmp4_notify_connection_status_post_body;
	  ndmp4_notify_mover_halted_post ndmp4_notify_mover_halted_post_body;
	  ndmp4_notify_mover_paused_post ndmp4_notify_mover_paused_post_body;
	  ndmp4_notify_data_read_post ndmp4_notify_data_read_post_body;
	  ndmp4_log_file_post ndmp4_log_file_post_body;
	  ndmp4_log_message_post ndmp4_log_message_post_body;
	  ndmp4_fh_add_file_post ndmp4_fh_add_file_post_body;
	  ndmp4_fh_add_dir_post ndmp4_fh_add_dir_post_body;
	  ndmp4_fh_add_node_post ndmp4_fh_add_node_post_body;
	  ndmp4_mover_get_state_reply ndmp4_mover_get_state_reply_body;
	  ndmp4_mover_listen_request ndmp4_mover_listen_request_body;
	  ndmp4_mover_listen_reply ndmp4_mover_listen_reply_body;
	  ndmp4_mover_continue_reply ndmp4_mover_continue_reply_body;
	  ndmp4_mover_abort_reply ndmp4_mover_abort_reply_body;
	  ndmp4_mover_stop_reply ndmp4_mover_stop_reply_body;
	  ndmp4_mover_set_window_request ndmp4_mover_set_window_request_body;
	  ndmp4_mover_set_window_reply ndmp4_mover_set_window_reply_body;
	  ndmp4_mover_read_request ndmp4_mover_read_request_body;
	  ndmp4_mover_read_reply ndmp4_mover_read_reply_body;
	  ndmp4_mover_close_reply ndmp4_mover_close_reply_body;
	  ndmp4_mover_set_record_size_request ndmp4_mover_set_record_size_request_body;
	  ndmp4_mover_set_record_size_reply ndmp4_mover_set_record_size_reply_body;
	  ndmp4_mover_connect_request ndmp4_mover_connect_request_body;
	  ndmp4_mover_connect_reply ndmp4_mover_connect_reply_body;

#endif /* !NDMOS_OPTION_NO_NDMP4 */

	  ndmp0_error error;
	  struct ndmp3_unfortunate_error unf3_error;

	  ndmp9_error ndmp9_error_reply;
	  ndmp9_connect_open_request ndmp9_connect_open_request_body;
	  ndmp9_connect_open_reply ndmp9_connect_open_reply_body;
	  ndmp9_connect_close_request ndmp9_connect_close_request_body;
	  ndmp9_connect_close_reply ndmp9_connect_close_reply_body;
	  ndmp9_connect_client_auth_request ndmp9_connect_client_auth_request_body;
	  ndmp9_connect_client_auth_reply ndmp9_connect_client_auth_reply_body;
	  ndmp9_connect_server_auth_request ndmp9_connect_server_auth_request_body;
	  ndmp9_connect_server_auth_reply ndmp9_connect_server_auth_reply_body;
	  ndmp9_config_get_host_info_reply ndmp9_config_get_host_info_reply_body;
	  ndmp9_config_get_server_info_reply ndmp9_config_get_server_info_reply_body;

	  ndmp9_config_get_butype_info_reply ndmp9_config_get_butype_info_reply_body;
	  ndmp9_config_get_fs_info_reply ndmp9_config_get_fs_info_reply_body;
	  ndmp9_config_get_tape_info_reply ndmp9_config_get_tape_info_reply_body;
	  ndmp9_config_get_scsi_info_reply ndmp9_config_get_scsi_info_reply_body;
	  ndmp9_config_get_info_reply ndmp9_config_get_info_reply_body;
	  ndmp9_config_get_auth_attr_request ndmp9_config_get_auth_attr_request_body;
	  ndmp9_config_get_auth_attr_reply ndmp9_config_get_auth_attr_reply_body;
	  ndmp9_scsi_open_request ndmp9_scsi_open_request_body;
	  ndmp9_scsi_open_reply ndmp9_scsi_open_reply_body;
	  ndmp9_scsi_close_reply ndmp9_scsi_close_reply_body;
	  ndmp9_scsi_get_state_reply ndmp9_scsi_get_state_reply_body;
	  ndmp9_scsi_set_target_request ndmp9_scsi_set_target_request_body;
	  ndmp9_scsi_set_target_reply ndmp9_scsi_set_target_reply_body;
	  ndmp9_scsi_reset_device_reply ndmp9_scsi_reset_device_reply_body;
	  ndmp9_scsi_reset_bus_reply ndmp9_scsi_reset_bus_reply_body;
	  ndmp9_scsi_execute_cdb_request ndmp9_scsi_execute_cdb_request_body;
	  ndmp9_scsi_execute_cdb_reply ndmp9_scsi_execute_cdb_reply_body;
	  ndmp9_tape_open_request ndmp9_tape_open_request_body;
	  ndmp9_tape_open_reply ndmp9_tape_open_reply_body;
	  ndmp9_tape_close_reply ndmp9_tape_close_reply_body;
	  ndmp9_tape_get_state_reply ndmp9_tape_get_state_reply_body;
	  ndmp9_tape_mtio_request ndmp9_tape_mtio_request_body;
	  ndmp9_tape_mtio_reply ndmp9_tape_mtio_reply_body;
	  ndmp9_tape_write_request ndmp9_tape_write_request_body;
	  ndmp9_tape_write_reply ndmp9_tape_write_reply_body;
	  ndmp9_tape_read_request ndmp9_tape_read_request_body;
	  ndmp9_tape_read_reply ndmp9_tape_read_reply_body;
	  ndmp9_tape_execute_cdb_request ndmp9_tape_execute_cdb_request_body;
	  ndmp9_tape_execute_cdb_reply ndmp9_tape_execute_cdb_reply_body;
	  ndmp9_data_get_state_reply ndmp9_data_get_state_reply_body;
	  ndmp9_data_start_backup_request ndmp9_data_start_backup_request_body;
	  ndmp9_data_start_backup_reply ndmp9_data_start_backup_reply_body;
	  ndmp9_data_start_recover_request ndmp9_data_start_recover_request_body;
	  ndmp9_data_start_recover_reply ndmp9_data_start_recover_reply_body;
	  ndmp9_data_abort_reply ndmp9_data_abort_reply_body;
	  ndmp9_data_get_env_reply ndmp9_data_get_env_reply_body;
	  ndmp9_data_stop_reply ndmp9_data_stop_reply_body;
	  ndmp9_data_start_recover_filehist_request ndmp9_data_start_recover_filehist_request_body;
	  ndmp9_data_start_recover_filehist_reply ndmp9_data_start_recover_filehist_reply_body;
	  ndmp9_data_listen_request ndmp9_data_listen_request_body;
	  ndmp9_data_listen_reply ndmp9_data_listen_reply_body;
	  ndmp9_data_connect_request ndmp9_data_connect_request_body;
	  ndmp9_data_connect_reply ndmp9_data_connect_reply_body;
	  ndmp9_notify_data_halted_request ndmp9_notify_data_halted_request_body;
	  ndmp9_notify_connected_request ndmp9_notify_connected_request_body;
	  ndmp9_notify_mover_halted_request ndmp9_notify_mover_halted_request_body;
	  ndmp9_notify_mover_paused_request ndmp9_notify_mover_paused_request_body;
	  ndmp9_notify_data_read_request ndmp9_notify_data_read_request_body;
	  ndmp9_log_file_request ndmp9_log_file_request_body;
	  ndmp9_log_message_request ndmp9_log_message_request_body;
	  ndmp9_fh_add_file_request ndmp9_fh_add_file_request_body;
	  ndmp9_fh_add_dir_request ndmp9_fh_add_dir_request_body;
	  ndmp9_fh_add_node_request ndmp9_fh_add_node_request_body;
	  ndmp9_mover_get_state_reply ndmp9_mover_get_state_reply_body;
	  ndmp9_mover_listen_request ndmp9_mover_listen_request_body;
	  ndmp9_mover_listen_reply ndmp9_mover_listen_reply_body;
	  ndmp9_mover_continue_reply ndmp9_mover_continue_reply_body;
	  ndmp9_mover_abort_reply ndmp9_mover_abort_reply_body;
	  ndmp9_mover_stop_reply ndmp9_mover_stop_reply_body;
	  ndmp9_mover_set_window_request ndmp9_mover_set_window_request_body;
	  ndmp9_mover_set_window_reply ndmp9_mover_set_window_reply_body;
	  ndmp9_mover_read_request ndmp9_mover_read_request_body;
	  ndmp9_mover_read_reply ndmp9_mover_read_reply_body;
	  ndmp9_mover_close_reply ndmp9_mover_close_reply_body;
	  ndmp9_mover_set_record_size_request ndmp9_mover_set_record_size_request_body;
	  ndmp9_mover_set_record_size_reply ndmp9_mover_set_record_size_reply_body;
	  ndmp9_mover_connect_request ndmp9_mover_connect_request_body;
	  ndmp9_mover_connect_reply ndmp9_mover_connect_reply_body;
	} body;
};

struct ndmp_xa_buf {
	struct ndmp_msg_buf	request;
	struct ndmp_msg_buf	reply;
};



#define MT_ndmp0_connect_open		NDMP0_CONNECT_OPEN
#define MT_ndmp0_connect_close		NDMP0_CONNECT_CLOSE
#define MT_ndmp0_notify_connected	NDMP0_NOTIFY_CONNECTED


#ifndef NDMOS_OPTION_NO_NDMP2

#define MT_ndmp2_connect_open		NDMP2_CONNECT_OPEN
#define MT_ndmp2_connect_client_auth	NDMP2_CONNECT_CLIENT_AUTH
#define MT_ndmp2_connect_close		NDMP2_CONNECT_CLOSE
#define MT_ndmp2_connect_server_auth	NDMP2_CONNECT_SERVER_AUTH
#define MT_ndmp2_config_get_host_info	NDMP2_CONFIG_GET_HOST_INFO
#define MT_ndmp2_config_get_butype_attr	NDMP2_CONFIG_GET_BUTYPE_ATTR
#define MT_ndmp2_config_get_mover_type	NDMP2_CONFIG_GET_MOVER_TYPE
#define MT_ndmp2_config_get_auth_attr	NDMP2_CONFIG_GET_AUTH_ATTR
#define MT_ndmp2_scsi_open		NDMP2_SCSI_OPEN
#define MT_ndmp2_scsi_close		NDMP2_SCSI_CLOSE
#define MT_ndmp2_scsi_get_state		NDMP2_SCSI_GET_STATE
#define MT_ndmp2_scsi_set_target	NDMP2_SCSI_SET_TARGET
#define MT_ndmp2_scsi_reset_device	NDMP2_SCSI_RESET_DEVICE
#define MT_ndmp2_scsi_reset_bus		NDMP2_SCSI_RESET_BUS
#define MT_ndmp2_scsi_execute_cdb	NDMP2_SCSI_EXECUTE_CDB
#define MT_ndmp2_tape_open		NDMP2_TAPE_OPEN
#define MT_ndmp2_tape_close		NDMP2_TAPE_CLOSE
#define MT_ndmp2_tape_get_state		NDMP2_TAPE_GET_STATE
#define MT_ndmp2_tape_mtio		NDMP2_TAPE_MTIO
#define MT_ndmp2_tape_write		NDMP2_TAPE_WRITE
#define MT_ndmp2_tape_read		NDMP2_TAPE_READ
#define MT_ndmp2_tape_execute_cdb	NDMP2_TAPE_EXECUTE_CDB
#define MT_ndmp2_data_get_state		NDMP2_DATA_GET_STATE
#define MT_ndmp2_data_start_backup	NDMP2_DATA_START_BACKUP
#define MT_ndmp2_data_start_recover	NDMP2_DATA_START_RECOVER
#define MT_ndmp2_data_abort		NDMP2_DATA_ABORT
#define MT_ndmp2_data_get_env		NDMP2_DATA_GET_ENV
#define MT_ndmp2_data_stop		NDMP2_DATA_STOP
#define MT_ndmp2_data_start_recover_filehist NDMP2_DATA_START_RECOVER_FILEHIST
#define MT_ndmp2_notify_data_halted	NDMP2_NOTIFY_DATA_HALTED
#define MT_ndmp2_notify_connected	NDMP2_NOTIFY_CONNECTED
#define MT_ndmp2_notify_mover_halted	NDMP2_NOTIFY_MOVER_HALTED
#define MT_ndmp2_notify_mover_paused	NDMP2_NOTIFY_MOVER_PAUSED
#define MT_ndmp2_notify_data_read	NDMP2_NOTIFY_DATA_READ
#define MT_ndmp2_log_log		NDMP2_LOG_LOG
#define MT_ndmp2_log_debug		NDMP2_LOG_DEBUG
#define MT_ndmp2_log_file		NDMP2_LOG_FILE
#define MT_ndmp2_fh_add_unix_path	NDMP2_FH_ADD_UNIX_PATH
#define MT_ndmp2_fh_add_unix_dir	NDMP2_FH_ADD_UNIX_DIR
#define MT_ndmp2_fh_add_unix_node	NDMP2_FH_ADD_UNIX_NODE
#define MT_ndmp2_mover_get_state	NDMP2_MOVER_GET_STATE
#define MT_ndmp2_mover_listen		NDMP2_MOVER_LISTEN
#define MT_ndmp2_mover_continue		NDMP2_MOVER_CONTINUE
#define MT_ndmp2_mover_abort		NDMP2_MOVER_ABORT
#define MT_ndmp2_mover_stop		NDMP2_MOVER_STOP
#define MT_ndmp2_mover_set_window	NDMP2_MOVER_SET_WINDOW
#define MT_ndmp2_mover_read		NDMP2_MOVER_READ
#define MT_ndmp2_mover_close		NDMP2_MOVER_CLOSE
#define MT_ndmp2_mover_set_record_size	NDMP2_MOVER_SET_RECORD_SIZE

#endif /* !NDMOS_OPTION_NO_NDMP2 */



#ifndef NDMOS_OPTION_NO_NDMP3

#define MT_ndmp3_connect_open		NDMP3_CONNECT_OPEN
#define MT_ndmp3_connect_client_auth	NDMP3_CONNECT_CLIENT_AUTH
#define MT_ndmp3_connect_close		NDMP3_CONNECT_CLOSE
#define MT_ndmp3_connect_server_auth	NDMP3_CONNECT_SERVER_AUTH
#define MT_ndmp3_config_get_host_info	NDMP3_CONFIG_GET_HOST_INFO
#define MT_ndmp3_config_get_connection_type NDMP3_CONFIG_GET_CONNECTION_TYPE
#define MT_ndmp3_config_get_auth_attr	NDMP3_CONFIG_GET_AUTH_ATTR
#define MT_ndmp3_config_get_butype_info	NDMP3_CONFIG_GET_BUTYPE_INFO
#define MT_ndmp3_config_get_fs_info	NDMP3_CONFIG_GET_FS_INFO
#define MT_ndmp3_config_get_tape_info	NDMP3_CONFIG_GET_TAPE_INFO
#define MT_ndmp3_config_get_scsi_info	NDMP3_CONFIG_GET_SCSI_INFO
#define MT_ndmp3_config_get_server_info	NDMP3_CONFIG_GET_SERVER_INFO
#define MT_ndmp3_scsi_open		NDMP3_SCSI_OPEN
#define MT_ndmp3_scsi_close		NDMP3_SCSI_CLOSE
#define MT_ndmp3_scsi_get_state		NDMP3_SCSI_GET_STATE
#define MT_ndmp3_scsi_set_target	NDMP3_SCSI_SET_TARGET
#define MT_ndmp3_scsi_reset_device	NDMP3_SCSI_RESET_DEVICE
#define MT_ndmp3_scsi_reset_bus		NDMP3_SCSI_RESET_BUS
#define MT_ndmp3_scsi_execute_cdb	NDMP3_SCSI_EXECUTE_CDB
#define MT_ndmp3_tape_open		NDMP3_TAPE_OPEN
#define MT_ndmp3_tape_close		NDMP3_TAPE_CLOSE
#define MT_ndmp3_tape_get_state		NDMP3_TAPE_GET_STATE
#define MT_ndmp3_tape_mtio		NDMP3_TAPE_MTIO
#define MT_ndmp3_tape_write		NDMP3_TAPE_WRITE
#define MT_ndmp3_tape_read		NDMP3_TAPE_READ
#define MT_ndmp3_tape_execute_cdb	NDMP3_TAPE_EXECUTE_CDB
#define MT_ndmp3_data_get_state		NDMP3_DATA_GET_STATE
#define MT_ndmp3_data_start_backup	NDMP3_DATA_START_BACKUP
#define MT_ndmp3_data_start_recover	NDMP3_DATA_START_RECOVER
#define MT_ndmp3_data_start_recover_filehist NDMP3_DATA_START_RECOVER_FILEHIST
#define MT_ndmp3_data_abort		NDMP3_DATA_ABORT
#define MT_ndmp3_data_get_env		NDMP3_DATA_GET_ENV
#define MT_ndmp3_data_stop		NDMP3_DATA_STOP
#define MT_ndmp3_data_listen		NDMP3_DATA_LISTEN
#define MT_ndmp3_data_connect		NDMP3_DATA_CONNECT
#define MT_ndmp3_notify_data_halted	NDMP3_NOTIFY_DATA_HALTED
#define MT_ndmp3_notify_connected	NDMP3_NOTIFY_CONNECTED
#define MT_ndmp3_notify_mover_halted	NDMP3_NOTIFY_MOVER_HALTED
#define MT_ndmp3_notify_mover_paused	NDMP3_NOTIFY_MOVER_PAUSED
#define MT_ndmp3_notify_data_read	NDMP3_NOTIFY_DATA_READ
#define MT_ndmp3_log_file		NDMP3_LOG_FILE
#define MT_ndmp3_log_message		NDMP3_LOG_MESSAGE
#define MT_ndmp3_fh_add_file		NDMP3_FH_ADD_FILE
#define MT_ndmp3_fh_add_dir		NDMP3_FH_ADD_DIR
#define MT_ndmp3_fh_add_node		NDMP3_FH_ADD_NODE
#define MT_ndmp3_mover_get_state	NDMP3_MOVER_GET_STATE
#define MT_ndmp3_mover_listen		NDMP3_MOVER_LISTEN
#define MT_ndmp3_mover_continue		NDMP3_MOVER_CONTINUE
#define MT_ndmp3_mover_abort		NDMP3_MOVER_ABORT
#define MT_ndmp3_mover_stop		NDMP3_MOVER_STOP
#define MT_ndmp3_mover_set_window	NDMP3_MOVER_SET_WINDOW
#define MT_ndmp3_mover_read		NDMP3_MOVER_READ
#define MT_ndmp3_mover_close		NDMP3_MOVER_CLOSE
#define MT_ndmp3_mover_set_record_size	NDMP3_MOVER_SET_RECORD_SIZE
#define MT_ndmp3_mover_connect		NDMP3_MOVER_CONNECT

#endif /* !NDMOS_OPTION_NO_NDMP3 */



#ifndef NDMOS_OPTION_NO_NDMP4

#define MT_ndmp4_connect_open		NDMP4_CONNECT_OPEN
#define MT_ndmp4_connect_client_auth	NDMP4_CONNECT_CLIENT_AUTH
#define MT_ndmp4_connect_close		NDMP4_CONNECT_CLOSE
#define MT_ndmp4_connect_server_auth	NDMP4_CONNECT_SERVER_AUTH
#define MT_ndmp4_config_get_host_info	NDMP4_CONFIG_GET_HOST_INFO
#define MT_ndmp4_config_get_connection_type NDMP4_CONFIG_GET_CONNECTION_TYPE
#define MT_ndmp4_config_get_auth_attr	NDMP4_CONFIG_GET_AUTH_ATTR
#define MT_ndmp4_config_get_butype_info	NDMP4_CONFIG_GET_BUTYPE_INFO
#define MT_ndmp4_config_get_fs_info	NDMP4_CONFIG_GET_FS_INFO
#define MT_ndmp4_config_get_tape_info	NDMP4_CONFIG_GET_TAPE_INFO
#define MT_ndmp4_config_get_scsi_info	NDMP4_CONFIG_GET_SCSI_INFO
#define MT_ndmp4_config_get_server_info	NDMP4_CONFIG_GET_SERVER_INFO
#define MT_ndmp4_scsi_open		NDMP4_SCSI_OPEN
#define MT_ndmp4_scsi_close		NDMP4_SCSI_CLOSE
#define MT_ndmp4_scsi_get_state		NDMP4_SCSI_GET_STATE
#define MT_ndmp4_scsi_reset_device	NDMP4_SCSI_RESET_DEVICE
#define MT_ndmp4_scsi_execute_cdb	NDMP4_SCSI_EXECUTE_CDB
#define MT_ndmp4_tape_open		NDMP4_TAPE_OPEN
#define MT_ndmp4_tape_close		NDMP4_TAPE_CLOSE
#define MT_ndmp4_tape_get_state		NDMP4_TAPE_GET_STATE
#define MT_ndmp4_tape_mtio		NDMP4_TAPE_MTIO
#define MT_ndmp4_tape_write		NDMP4_TAPE_WRITE
#define MT_ndmp4_tape_read		NDMP4_TAPE_READ
#define MT_ndmp4_tape_execute_cdb	NDMP4_TAPE_EXECUTE_CDB
#define MT_ndmp4_data_get_state		NDMP4_DATA_GET_STATE
#define MT_ndmp4_data_start_backup	NDMP4_DATA_START_BACKUP
#define MT_ndmp4_data_start_recover	NDMP4_DATA_START_RECOVER
#define MT_ndmp4_data_start_recover_filehist NDMP4_DATA_START_RECOVER_FILEHIST
#define MT_ndmp4_data_abort		NDMP4_DATA_ABORT
#define MT_ndmp4_data_get_env		NDMP4_DATA_GET_ENV
#define MT_ndmp4_data_stop		NDMP4_DATA_STOP
#define MT_ndmp4_data_listen		NDMP4_DATA_LISTEN
#define MT_ndmp4_data_connect		NDMP4_DATA_CONNECT
#define MT_ndmp4_notify_data_halted	NDMP4_NOTIFY_DATA_HALTED
#define MT_ndmp4_notify_connection_status NDMP4_NOTIFY_CONNECTION_STATUS
#define MT_ndmp4_notify_mover_halted	NDMP4_NOTIFY_MOVER_HALTED
#define MT_ndmp4_notify_mover_paused	NDMP4_NOTIFY_MOVER_PAUSED
#define MT_ndmp4_notify_data_read	NDMP4_NOTIFY_DATA_READ
#define MT_ndmp4_log_file		NDMP4_LOG_FILE
#define MT_ndmp4_log_message		NDMP4_LOG_MESSAGE
#define MT_ndmp4_fh_add_file		NDMP4_FH_ADD_FILE
#define MT_ndmp4_fh_add_dir		NDMP4_FH_ADD_DIR
#define MT_ndmp4_fh_add_node		NDMP4_FH_ADD_NODE
#define MT_ndmp4_mover_get_state	NDMP4_MOVER_GET_STATE
#define MT_ndmp4_mover_listen		NDMP4_MOVER_LISTEN
#define MT_ndmp4_mover_continue		NDMP4_MOVER_CONTINUE
#define MT_ndmp4_mover_abort		NDMP4_MOVER_ABORT
#define MT_ndmp4_mover_stop		NDMP4_MOVER_STOP
#define MT_ndmp4_mover_set_window	NDMP4_MOVER_SET_WINDOW
#define MT_ndmp4_mover_read		NDMP4_MOVER_READ
#define MT_ndmp4_mover_close		NDMP4_MOVER_CLOSE
#define MT_ndmp4_mover_set_record_size	NDMP4_MOVER_SET_RECORD_SIZE
#define MT_ndmp4_mover_connect		NDMP4_MOVER_CONNECT

#endif /* !NDMOS_OPTION_NO_NDMP4 */




#define MT_ndmp9_connect_open		NDMP9_CONNECT_OPEN
#define MT_ndmp9_connect_client_auth	NDMP9_CONNECT_CLIENT_AUTH
#define MT_ndmp9_connect_close		NDMP9_CONNECT_CLOSE
#define MT_ndmp9_connect_server_auth	NDMP9_CONNECT_SERVER_AUTH
#define MT_ndmp9_config_get_host_info	NDMP9_CONFIG_GET_HOST_INFO
#define MT_ndmp9_config_get_connection_type NDMP9_CONFIG_GET_CONNECTION_TYPE
#define MT_ndmp9_config_get_auth_attr	NDMP9_CONFIG_GET_AUTH_ATTR
#define MT_ndmp9_config_get_butype_info	NDMP9_CONFIG_GET_BUTYPE_INFO
#define MT_ndmp9_config_get_fs_info	NDMP9_CONFIG_GET_FS_INFO
#define MT_ndmp9_config_get_tape_info	NDMP9_CONFIG_GET_TAPE_INFO
#define MT_ndmp9_config_get_scsi_info	NDMP9_CONFIG_GET_SCSI_INFO
#define MT_ndmp9_config_get_server_info	NDMP9_CONFIG_GET_SERVER_INFO
#define MT_ndmp9_scsi_open		NDMP9_SCSI_OPEN
#define MT_ndmp9_scsi_close		NDMP9_SCSI_CLOSE
#define MT_ndmp9_scsi_get_state		NDMP9_SCSI_GET_STATE
#define MT_ndmp9_scsi_set_target	NDMP9_SCSI_SET_TARGET
#define MT_ndmp9_scsi_reset_device	NDMP9_SCSI_RESET_DEVICE
#define MT_ndmp9_scsi_reset_bus		NDMP9_SCSI_RESET_BUS
#define MT_ndmp9_scsi_execute_cdb	NDMP9_SCSI_EXECUTE_CDB
#define MT_ndmp9_tape_open		NDMP9_TAPE_OPEN
#define MT_ndmp9_tape_close		NDMP9_TAPE_CLOSE
#define MT_ndmp9_tape_get_state		NDMP9_TAPE_GET_STATE
#define MT_ndmp9_tape_mtio		NDMP9_TAPE_MTIO
#define MT_ndmp9_tape_write		NDMP9_TAPE_WRITE
#define MT_ndmp9_tape_read		NDMP9_TAPE_READ
#define MT_ndmp9_tape_execute_cdb	NDMP9_TAPE_EXECUTE_CDB
#define MT_ndmp9_data_get_state		NDMP9_DATA_GET_STATE
#define MT_ndmp9_data_start_backup	NDMP9_DATA_START_BACKUP
#define MT_ndmp9_data_start_recover	NDMP9_DATA_START_RECOVER
#define MT_ndmp9_data_start_recover_filehist NDMP9_DATA_START_RECOVER_FILEHIST
#define MT_ndmp9_data_abort		NDMP9_DATA_ABORT
#define MT_ndmp9_data_get_env		NDMP9_DATA_GET_ENV
#define MT_ndmp9_data_stop		NDMP9_DATA_STOP
#define MT_ndmp9_data_listen		NDMP9_DATA_LISTEN
#define MT_ndmp9_data_connect		NDMP9_DATA_CONNECT
#define MT_ndmp9_notify_data_halted	NDMP9_NOTIFY_DATA_HALTED
#define MT_ndmp9_notify_connected	NDMP9_NOTIFY_CONNECTED
#define MT_ndmp9_notify_mover_halted	NDMP9_NOTIFY_MOVER_HALTED
#define MT_ndmp9_notify_mover_paused	NDMP9_NOTIFY_MOVER_PAUSED
#define MT_ndmp9_notify_data_read	NDMP9_NOTIFY_DATA_READ
#define MT_ndmp9_log_file		NDMP9_LOG_FILE
#define MT_ndmp9_log_message		NDMP9_LOG_MESSAGE
#define MT_ndmp9_fh_add_file		NDMP9_FH_ADD_FILE
#define MT_ndmp9_fh_add_dir		NDMP9_FH_ADD_DIR
#define MT_ndmp9_fh_add_node		NDMP9_FH_ADD_NODE
#define MT_ndmp9_mover_get_state	NDMP9_MOVER_GET_STATE
#define MT_ndmp9_mover_listen		NDMP9_MOVER_LISTEN
#define MT_ndmp9_mover_continue		NDMP9_MOVER_CONTINUE
#define MT_ndmp9_mover_abort		NDMP9_MOVER_ABORT
#define MT_ndmp9_mover_stop		NDMP9_MOVER_STOP
#define MT_ndmp9_mover_set_window	NDMP9_MOVER_SET_WINDOW
#define MT_ndmp9_mover_read		NDMP9_MOVER_READ
#define MT_ndmp9_mover_close		NDMP9_MOVER_CLOSE
#define MT_ndmp9_mover_set_record_size	NDMP9_MOVER_SET_RECORD_SIZE
#define MT_ndmp9_mover_connect		NDMP9_MOVER_CONNECT

#ifdef  __cplusplus
}
#endif
