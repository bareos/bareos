/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include "dropletp.h"
#include "droplet/cdmi/object_id.h"
#include "droplet/cdmi/crcmodel.h"

#define HEXLUT "0123456789ABCDEF"

dpl_status_t dpl_cdmi_object_id_init(dpl_cdmi_object_id_t* object_id,
                                     uint32_t enterprise_number,
                                     const void* opaque_data,
                                     char opaque_len)
{
#if 0
  if ( enterprise_number & 0xff000000 )
    return DPL_EINVAL;
#endif
  if (opaque_len > 32) return DPL_EINVAL;

  object_id->enterprise_number = htonl(enterprise_number & 0xffffff);
  object_id->reserved = 0x00;
  memcpy(object_id->opaque, opaque_data, opaque_len);
  object_id->length
      = sizeof(*object_id) - sizeof(object_id->opaque) + opaque_len;

  object_id->crc = 0x0000;

  cm_t cm = {.cm_width = 16,
             .cm_poly = 0x8005,
             .cm_init = 0x0000,
             .cm_refin = TRUE,
             .cm_refot = TRUE,
             .cm_xorot = 0x0000};
  cm_ini(&cm);
  cm_blk(&cm, (void*)object_id, object_id->length);
  object_id->crc = htons(cm_crc(&cm));

  return DPL_SUCCESS;
}

dpl_status_t dpl_cdmi_object_id_to_string(const dpl_cdmi_object_id_t* object_id,
                                          char* output)
{
  int i;

  if (NULL == output) return DPL_EINVAL;

  for (i = 0; i < object_id->length; ++i) {
    if (i * 2 + 1 > DPL_CDMI_OBJECT_ID_LEN) return DPL_FAILURE;

    output[i * 2] = ((((char*)object_id)[i] & 0xf0) >> 4)[HEXLUT];
    output[i * 2 + 1] = (((char*)object_id)[i] & 0x0f)[HEXLUT];
  }
  output[object_id->length * 2] = '\0';

  return DPL_SUCCESS;
}

dpl_status_t dpl_cdmi_opaque_to_string(const dpl_cdmi_object_id_t* object_id,
                                       char* output)
{
  int i, j;

  if (NULL == output) return DPL_EINVAL;

  for (i = 8, j = 0; i < object_id->length; ++i, ++j) {
    if (j * 2 + 1 > DPL_CDMI_OBJECT_ID_LEN) return DPL_FAILURE;

    output[j * 2] = ((((char*)object_id)[i] & 0xf0) >> 4)[HEXLUT];
    output[j * 2 + 1] = (((char*)object_id)[i] & 0x0f)[HEXLUT];
  }
  output[j * 2] = '\0';

  return DPL_SUCCESS;
}

dpl_status_t dpl_cdmi_string_to_object_id(const char* input,
                                          dpl_cdmi_object_id_t* output)
{
  int i;
  int len_min = sizeof(*output) - sizeof(output->opaque) + 1;
  int odd = FALSE;
  for (i = 0; input[i]; ++i) {
    char nibble;
    if (input[i] >= '0' && input[i] <= '9')
      nibble = input[i] - '0';
    else if (input[i] >= 'a' && input[i] <= 'f')
      nibble = input[i] - 'a' + 10;
    else if (input[i] >= 'A' && input[i] <= 'F')
      nibble = input[i] - 'A' + 10;

    else
      return DPL_EINVAL;

    if (!odd)
      ((char*)output)[i / 2] = nibble << 4;
    else
      ((char*)output)[i / 2] = ((char*)output)[i / 2] | nibble;

    odd = !odd;
  }
  if (i < len_min * 2) return DPL_EINVAL;

  uint16_t crc = output->crc;
  output->crc = 0x0000;

  cm_t cm = {.cm_width = 16,
             .cm_poly = 0x8005,
             .cm_init = 0x0000,
             .cm_refin = TRUE,
             .cm_refot = TRUE,
             .cm_xorot = 0x0000};
  cm_ini(&cm);
  cm_blk(&cm, (void*)output, output->length);
  output->crc = htons(cm_crc(&cm));

  if (output->crc != crc) return DPL_EINVAL;

  return DPL_SUCCESS;
}

dpl_status_t dpl_cdmi_string_to_opaque(const char* input,
                                       char* output,
                                       int* output_lenp)
{
  int i;
  int odd = FALSE;
  for (i = 0; input[i]; ++i) {
    char nibble;
    if (input[i] >= '0' && input[i] <= '9')
      nibble = input[i] - '0';
    else if (input[i] >= 'a' && input[i] <= 'f')
      nibble = input[i] - 'a' + 10;
    else if (input[i] >= 'A' && input[i] <= 'F')
      nibble = input[i] - 'A' + 10;

    else
      return DPL_EINVAL;

    if (!odd)
      ((char*)output)[i / 2] = nibble << 4;
    else
      ((char*)output)[i / 2] = ((char*)output)[i / 2] | nibble;

    odd = !odd;
  }

  if (NULL != output_lenp) *output_lenp = i / 2;

  return DPL_SUCCESS;
}

void dpl_cdmi_object_id_undef(dpl_cdmi_object_id_t* object_id)
{
  // memset(object_id, 0, sizeof(*object_id));
  object_id->reserved = 1;
}

int dpl_cdmi_object_id_is_def(const dpl_cdmi_object_id_t* object_id)
{
  return object_id->reserved == 0;
}

dpl_status_t dpl_cdmi_object_id_opaque_len(
    const dpl_cdmi_object_id_t* object_id,
    size_t* lenp)
{
  if (!dpl_cdmi_object_id_is_def(object_id)) return DPL_EINVAL;
  if (NULL != lenp)
    *lenp = object_id->length + sizeof(object_id->opaque) - sizeof(*object_id);
  return DPL_SUCCESS;
}
