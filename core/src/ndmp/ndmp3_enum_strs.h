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


#ifndef NDMOS_OPTION_NO_NDMP3

extern struct ndmp_enum_str_table ndmp3_error_table[];
extern char* ndmp3_error_to_str(ndmp3_error val);
extern int ndmp3_error_from_str(ndmp3_error* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_header_message_type_table[];
extern char* ndmp3_header_message_type_to_str(ndmp3_header_message_type val);
extern int ndmp3_header_message_type_from_str(ndmp3_header_message_type* valp,
                                              char* str);
extern struct ndmp_enum_str_table ndmp3_message_table[];
extern char* ndmp3_message_to_str(ndmp3_message val);
extern int ndmp3_message_from_str(ndmp3_message* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_auth_type_table[];
extern char* ndmp3_auth_type_to_str(ndmp3_auth_type val);
extern int ndmp3_auth_type_from_str(ndmp3_auth_type* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_addr_type_table[];
extern char* ndmp3_addr_type_to_str(ndmp3_addr_type val);
extern int ndmp3_addr_type_from_str(ndmp3_addr_type* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_tape_open_mode_table[];
extern char* ndmp3_tape_open_mode_to_str(ndmp3_tape_open_mode val);
extern int ndmp3_tape_open_mode_from_str(ndmp3_tape_open_mode* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_tape_mtio_op_table[];
extern char* ndmp3_tape_mtio_op_to_str(ndmp3_tape_mtio_op val);
extern int ndmp3_tape_mtio_op_from_str(ndmp3_tape_mtio_op* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_mover_state_table[];
extern char* ndmp3_mover_state_to_str(ndmp3_mover_state val);
extern int ndmp3_mover_state_from_str(ndmp3_mover_state* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_mover_pause_reason_table[];
extern char* ndmp3_mover_pause_reason_to_str(ndmp3_mover_pause_reason val);
extern int ndmp3_mover_pause_reason_from_str(ndmp3_mover_pause_reason* valp,
                                             char* str);
extern struct ndmp_enum_str_table ndmp3_mover_halt_reason_table[];
extern char* ndmp3_mover_halt_reason_to_str(ndmp3_mover_halt_reason val);
extern int ndmp3_mover_halt_reason_from_str(ndmp3_mover_halt_reason* valp,
                                            char* str);
extern struct ndmp_enum_str_table ndmp3_mover_mode_table[];
extern char* ndmp3_mover_mode_to_str(ndmp3_mover_mode val);
extern int ndmp3_mover_mode_from_str(ndmp3_mover_mode* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_data_operation_table[];
extern char* ndmp3_data_operation_to_str(ndmp3_data_operation val);
extern int ndmp3_data_operation_from_str(ndmp3_data_operation* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_data_state_table[];
extern char* ndmp3_data_state_to_str(ndmp3_data_state val);
extern int ndmp3_data_state_from_str(ndmp3_data_state* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_data_halt_reason_table[];
extern char* ndmp3_data_halt_reason_to_str(ndmp3_data_halt_reason val);
extern int ndmp3_data_halt_reason_from_str(ndmp3_data_halt_reason* valp,
                                           char* str);
extern struct ndmp_enum_str_table ndmp3_connect_reason_table[];
extern char* ndmp3_connect_reason_to_str(ndmp3_connect_reason val);
extern int ndmp3_connect_reason_from_str(ndmp3_connect_reason* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_log_type_table[];
extern char* ndmp3_log_type_to_str(ndmp3_log_type val);
extern int ndmp3_log_type_from_str(ndmp3_log_type* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_fs_type_table[];
extern char* ndmp3_fs_type_to_str(ndmp3_fs_type val);
extern int ndmp3_fs_type_from_str(ndmp3_fs_type* valp, char* str);
extern struct ndmp_enum_str_table ndmp3_file_type_table[];
extern char* ndmp3_file_type_to_str(ndmp3_file_type val);
extern int ndmp3_file_type_from_str(ndmp3_file_type* valp, char* str);

#endif /* !NDMOS_OPTION_NO_NDMP3 */
