/*
 * Copyright (c) 1998,1999,2000
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


#include "ndmagents.h"


#ifndef NDMOS_OPTION_NO_ROBOT_AGENT


/*
 * Initialization and Cleanup
 ****************************************************************
 */

/* Initialize -- Set data structure to know value, ignore current value */
int ndmra_initialize(struct ndm_session* sess)
{
  sess->robot_acb = NDMOS_API_MALLOC(sizeof(struct ndm_robot_agent));
  if (!sess->robot_acb) return -1;
  NDMOS_MACRO_ZEROFILL(sess->robot_acb);
  sess->robot_acb->scsi_state.error = NDMP9_DEV_NOT_OPEN_ERR;

  return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int ndmra_commission([[maybe_unused]] struct ndm_session* sess) { return 0; }

/* Decommission -- Discard agent */
int ndmra_decommission([[maybe_unused]] struct ndm_session* sess) { return 0; }

/* Destroy -- Destroy agent */
int ndmra_destroy(struct ndm_session* sess)
{
  if (!sess->robot_acb) { return 0; }

#  ifdef NDMOS_OPTION_ROBOT_SIMULATOR
  if (sess->robot_acb->sim_dir) { NDMOS_API_FREE(sess->robot_acb->sim_dir); }
#  endif

  NDMOS_API_FREE(sess->robot_acb);
  sess->robot_acb = NULL;

  return 0;
}

//

#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */
