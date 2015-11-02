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

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * conversion macros
 ****************************************************************
 * These make things a bit more readable and less error prone.
 * They also foster consistency.
 */


/* straight assignments */
#define CNVT_TO_9(PX, P9, MEMBER)	((P9)->MEMBER = (PX)->MEMBER)
#define CNVT_FROM_9(PX, P9, MEMBER)	((PX)->MEMBER = (P9)->MEMBER)
#define CNVT_TO_9x(PX, P9, MEMBERX, MEMBER9)	((P9)->MEMBER9 = (PX)->MEMBERX)
#define CNVT_FROM_9x(PX, P9, MEMBERX, MEMBER9)	((PX)->MEMBERX = (P9)->MEMBER9)

/* enum conversion */
#define CNVT_E_TO_9(PX, P9, MEMBER, ENUM_CONVERSION_TABLE) \
    ((P9)->MEMBER = convert_enum_to_9 (ENUM_CONVERSION_TABLE, (PX)->MEMBER))

#define CNVT_E_FROM_9(PX, P9, MEMBER, ENUM_CONVERSION_TABLE) \
    ((PX)->MEMBER = convert_enum_from_9 (ENUM_CONVERSION_TABLE, (P9)->MEMBER))

#define CNVT_VUL_TO_9(PX, P9, MEMBER) \
    convert_valid_u_long_to_9 (&(PX)->MEMBER, &(P9)->MEMBER)

#define CNVT_VUL_FROM_9(PX, P9, MEMBER) \
    convert_valid_u_long_from_9 (&(PX)->MEMBER, &(P9)->MEMBER)

#define CNVT_VUL_TO_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_valid_u_long_to_9 (&(PX)->MEMBERX, &(P9)->MEMBER9)

#define CNVT_VUL_FROM_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_valid_u_long_from_9 (&(PX)->MEMBERX, &(P9)->MEMBER9)

#define CNVT_IUL_TO_9(P9, MEMBER) \
    convert_invalid_u_long_9 (&(P9)->MEMBER)

#define CNVT_VUQ_TO_9(PX, P9, MEMBER) \
    convert_valid_u_quad_to_9 (&(PX)->MEMBER, &(P9)->MEMBER)

#define CNVT_VUQ_FROM_9(PX, P9, MEMBER) \
    convert_valid_u_quad_from_9 (&(PX)->MEMBER, &(P9)->MEMBER)

#define CNVT_VUQ_TO_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_valid_u_quad_to_9 (&(PX)->MEMBERX, &(P9)->MEMBER9)

#define CNVT_VUQ_FROM_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_valid_u_quad_from_9 (&(PX)->MEMBERX, &(P9)->MEMBER9)

#define CNVT_IUQ_TO_9(P9, MEMBER) \
    convert_invalid_u_quad_9 (&(P9)->MEMBER)

#define CNVT_STRDUP_TO_9(PX, P9, MEMBER) \
    convert_strdup ((PX)->MEMBER, &(P9)->MEMBER)

#define CNVT_STRDUP_FROM_9(PX, P9, MEMBER) \
    convert_strdup ((P9)->MEMBER, &(PX)->MEMBER)

#define CNVT_STRDUP_TO_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_strdup ((PX)->MEMBERX, &(P9)->MEMBER9)

#define CNVT_STRDUP_FROM_9x(PX, P9, MEMBERX, MEMBER9) \
    convert_strdup ((P9)->MEMBER9, &(PX)->MEMBERX)

#define CNVT_FREE(PX, MEMBERX) \
    { NDMOS_API_FREE((PX)->MEMBERX) ; (PX)->MEMBERX = NULL; };


/*
 * enum_conversion tables
 ****************************************************************
 * Used to make enum conversion convenient and dense.
 * The first row is the default case in both directions,
 * and is skipped while attempting precise conversion.
 * The search stops with the first match.
 */

struct enum_conversion {
	int	enum_x;
	int	enum_9;
};

#define END_ENUM_CONVERSION_TABLE		{ -1, -1 }
#define IS_END_ENUM_CONVERSION_TABLE(EC) \
		((EC)->enum_x == -1 && (EC)->enum_9 == -1)



extern int	/* ndmp9_.... */
convert_enum_to_9 (struct enum_conversion *ectab, int enum_x);
extern int	/* ndmpx_.... */
convert_enum_from_9 (struct enum_conversion *ectab, int enum_9);
extern int
convert_valid_u_long_to_9 (uint32_t *valx, ndmp9_valid_u_long *val9);
extern int
convert_valid_u_long_from_9 (uint32_t *valx, ndmp9_valid_u_long *val9);
extern int
convert_invalid_u_long_9 (struct ndmp9_valid_u_long *val9);
extern int
convert_valid_u_quad_to_9 (ndmp9_u_quad *valx, ndmp9_valid_u_quad *val9);
extern int
convert_valid_u_quad_from_9 (ndmp9_u_quad *valx, ndmp9_valid_u_quad *val9);
extern int
convert_invalid_u_quad_9 (struct ndmp9_valid_u_quad *val9);
extern int
convert_strdup (char *src, char **dstp);



/*
 * request/reply translation tables
 ****************************************************************
 */

struct reqrep_xlate {
	int		vx_message;
	ndmp9_message	v9_message;

	int		(*request_xto9) (/* void *vxbody, void *v9body */);
	int		(*request_9tox) (/* void *v9body, void *vxbody */);
	int		(*reply_xto9) (/* void *vxbody, void *v9body */);
	int		(*reply_9tox) (/* void *v9body, void *vxbody */);

	int		(*free_request_xto9) (/* void *v9body */);
	int		(*free_request_9tox) (/* void *vxbody */);
	int		(*free_reply_xto9) (/* void *v9body */);
	int		(*free_reply_9tox) (/* void *vxbody */);

};

struct reqrep_xlate_version_table {
	int				protocol_version;
	struct reqrep_xlate *		reqrep_xlate_table;
};

extern struct reqrep_xlate_version_table	reqrep_xlate_version_table[];

extern struct reqrep_xlate *reqrep_xlate_lookup_version (
				struct reqrep_xlate_version_table *rrvt,
				unsigned protocol_version);

extern struct reqrep_xlate *ndmp_reqrep_by_v9 (struct reqrep_xlate *table,
				ndmp9_message v9_message);
extern struct reqrep_xlate *ndmp_reqrep_by_vx (struct reqrep_xlate *table,
				int vx_message);

extern int	ndmp_xtox_no_arguments (void *vxbody, void *vybody);

extern int	ndmp_xtox_no_memused (void *vxbody);




/*
 * PROTOCOL VERSIONS
 ****************************************************************
 *
 */

#ifndef NDMOS_OPTION_NO_NDMP2
#include "ndmp2_translate.h"
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
#include "ndmp3_translate.h"
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
#include "ndmp4_translate.h"
#endif /* !NDMOS_OPTION_NO_NDMP4 */

#ifdef  __cplusplus
}
#endif
