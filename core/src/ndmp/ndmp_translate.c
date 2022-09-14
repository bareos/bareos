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


#include "ndmos.h" /* rpc/rpc.h */
#include "ndmprotocol.h"
#include "ndmp_msg_buf.h"
#include "ndmp_translate.h"


/*
 * enum_conversion tables
 ****************************************************************
 * Used to make enum conversion convenient and dense.
 * The first row is the default case in both directions,
 * and is skipped while attempting precise conversion.
 * The search stops with the first match.
 */

int /* ndmp9_.... */
convert_enum_to_9(struct enum_conversion* ectab, int enum_x)
{
  struct enum_conversion* ec = &ectab[1];

  for (; !IS_END_ENUM_CONVERSION_TABLE(ec); ec++) {
    if (ec->enum_x == enum_x) return ec->enum_9;
  }

  return ectab[0].enum_9;
}

int /* ndmpx_.... */
convert_enum_from_9(struct enum_conversion* ectab, int enum_9)
{
  struct enum_conversion* ec = &ectab[1];

  for (; !IS_END_ENUM_CONVERSION_TABLE(ec); ec++) {
    if (ec->enum_9 == enum_9) return ec->enum_x;
  }

  return ectab[0].enum_x;
}

int convert_valid_u_long_to_9(uint32_t* valx, ndmp9_valid_u_long* val9)
{
  val9->value = *valx;

  if (*valx == NDMP_INVALID_U_LONG)
    val9->valid = NDMP9_VALIDITY_INVALID;
  else
    val9->valid = NDMP9_VALIDITY_VALID;

  return 0;
}

int convert_valid_u_long_from_9(uint32_t* valx, ndmp9_valid_u_long* val9)
{
  if (!val9->valid)
    *valx = NDMP_INVALID_U_LONG;
  else
    *valx = val9->value;

  return 0;
}

int convert_invalid_u_long_9(struct ndmp9_valid_u_long* val9)
{
  val9->value = NDMP_INVALID_U_LONG;
  val9->valid = NDMP9_VALIDITY_INVALID;

  return 0;
}

int convert_valid_u_quad_to_9(ndmp9_u_quad* valx, ndmp9_valid_u_quad* val9)
{
  val9->value = *valx;

  if (*valx == NDMP_INVALID_U_QUAD)
    val9->valid = NDMP9_VALIDITY_INVALID;
  else
    val9->valid = NDMP9_VALIDITY_VALID;

  return 0;
}

int convert_valid_u_quad_from_9(ndmp9_u_quad* valx, ndmp9_valid_u_quad* val9)
{
  if (!val9->valid)
    *valx = NDMP_INVALID_U_QUAD;
  else
    *valx = val9->value;

  return 0;
}

int convert_invalid_u_quad_9(struct ndmp9_valid_u_quad* val9)
{
  val9->value = NDMP_INVALID_U_QUAD;
  val9->valid = NDMP9_VALIDITY_INVALID;

  return 0;
}

int convert_strdup(char* src, char** dstp)
{
  if (src == 0) {
    *dstp = 0;
    return 0;
  }
  *dstp = NDMOS_API_STRDUP(src);
  if (!*dstp) return -1;

  return 0;
}


/*
 * request/reply translation tables
 ****************************************************************
 */

struct reqrep_xlate_version_table reqrep_xlate_version_table[] = {
#ifndef NDMOS_OPTION_NO_NDMP2
    {NDMP2VER, ndmp2_reqrep_xlate_table},
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
    {NDMP3VER, ndmp3_reqrep_xlate_table},
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
    {NDMP4VER, ndmp4_reqrep_xlate_table},
#endif /* !NDMOS_OPTION_NO_NDMP4 */
    {0}};

struct reqrep_xlate* reqrep_xlate_lookup_version(
    struct reqrep_xlate_version_table* rrvt,
    unsigned protocol_version)
{
  for (; rrvt->protocol_version > 0; rrvt++) {
    if (rrvt->protocol_version == (int)protocol_version) {
      return rrvt->reqrep_xlate_table;
    }
  }

  return 0;
}


struct reqrep_xlate* ndmp_reqrep_by_v9(struct reqrep_xlate* table,
                                       ndmp9_message v9_message)
{
  struct reqrep_xlate* rrx = table;

  for (; rrx->v9_message; rrx++)
    if (rrx->v9_message == v9_message) return rrx;

  return 0;
}

struct reqrep_xlate* ndmp_reqrep_by_vx(struct reqrep_xlate* table,
                                       int vx_message)
{
  struct reqrep_xlate* rrx = table;

  for (; rrx->v9_message; rrx++)
    if (rrx->vx_message == vx_message) return rrx;

  return 0;
}

int ndmp_xtox_no_arguments([[maybe_unused]] void* vxbody,
                           [[maybe_unused]] void* vybody)
{
  return 0;
}


int ndmp_xtox_no_memused([[maybe_unused]] void* vxbody) { return 0; }
