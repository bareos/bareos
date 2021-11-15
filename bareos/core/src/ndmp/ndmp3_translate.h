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

#ifdef __cplusplus
extern "C" {
#endif

extern int ndmp_3to9_error(ndmp3_error* error3, ndmp9_error* error9);
extern int ndmp_9to3_error(ndmp9_error* error9, ndmp3_error* error3);

extern int ndmp_3to9_data_get_state_reply(ndmp3_data_get_state_reply* reply3,
                                          ndmp9_data_get_state_reply* reply9);
extern int ndmp_9to3_data_get_state_reply(ndmp9_data_get_state_reply* reply9,
                                          ndmp3_data_get_state_reply* reply3);

extern int ndmp_3to9_tape_get_state_reply(ndmp3_tape_get_state_reply* reply3,
                                          ndmp9_tape_get_state_reply* reply9);
extern int ndmp_9to3_tape_get_state_reply(ndmp9_tape_get_state_reply* reply9,
                                          ndmp3_tape_get_state_reply* reply3);

extern int ndmp_3to9_mover_get_state_reply(ndmp3_mover_get_state_reply* reply3,
                                           ndmp9_mover_get_state_reply* reply9);
extern int ndmp_9to3_mover_get_state_reply(ndmp9_mover_get_state_reply* reply9,
                                           ndmp3_mover_get_state_reply* reply3);

extern int ndmp_3to9_addr(ndmp3_addr* addr3, ndmp9_addr* addr9);
extern int ndmp_9to3_addr(ndmp9_addr* addr9, ndmp3_addr* addr3);

extern int ndmp_3to9_file_stat(ndmp3_file_stat* fstat3,
                               ndmp9_file_stat* fstat9,
                               ndmp9_u_quad node,
                               ndmp9_u_quad fh_info);
extern int ndmp_9to3_file_stat(ndmp9_file_stat* fstat9,
                               ndmp3_file_stat* fstat3);

extern int ndmp_3to9_pval(ndmp3_pval* pval3, ndmp9_pval* pval9);
extern int ndmp_9to3_pval(ndmp9_pval* pval9, ndmp3_pval* pval3);

extern int ndmp_3to9_pval_vec(ndmp3_pval* pval3,
                              ndmp9_pval* pval9,
                              unsigned n_pval);
extern int ndmp_9to3_pval_vec(ndmp9_pval* pval9,
                              ndmp3_pval* pval3,
                              unsigned n_pval);

extern int ndmp_3to9_name(ndmp3_name* name3, ndmp9_name* name9);
extern int ndmp_9to3_name(ndmp9_name* name9, ndmp3_name* name3);

extern int ndmp_3to9_name_vec(ndmp3_name* name3,
                              ndmp9_name* name9,
                              unsigned n_name);
extern int ndmp_9to3_name_vec(ndmp9_name* name9,
                              ndmp3_name* name3,
                              unsigned n_name);

extern struct reqrep_xlate ndmp3_reqrep_xlate_table[];

#ifdef __cplusplus
}
#endif
#endif /* !NDMOS_OPTION_NO_NDMP3 */
