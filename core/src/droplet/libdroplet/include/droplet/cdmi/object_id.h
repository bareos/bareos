/*
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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CDMI_OBJECT_ID_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CDMI_OBJECT_ID_H_

typedef struct {
  uint32_t enterprise_number;
  unsigned char reserved;
  unsigned char length;
  uint16_t crc;
  char opaque[32];
} dpl_cdmi_object_id_t;

#define DPL_CDMI_OBJECT_ID_LEN (sizeof(dpl_cdmi_object_id_t) * 2 + 1)

dpl_status_t dpl_cdmi_object_id_init(dpl_cdmi_object_id_t* object_id,
                                     uint32_t enterprise_number,
                                     const void* opaque_data,
                                     char opaque_len);
dpl_status_t dpl_cdmi_object_id_to_string(const dpl_cdmi_object_id_t* object_id,
                                          char* output);
dpl_status_t dpl_cdmi_opaque_to_string(const dpl_cdmi_object_id_t* object_id,
                                       char* output);
dpl_status_t dpl_cdmi_string_to_object_id(const char* input,
                                          dpl_cdmi_object_id_t* output);
dpl_status_t dpl_cdmi_string_to_opaque(const char* input,
                                       char* output,
                                       int* opaque_lenp);
void dpl_cdmi_object_id_undef(dpl_cdmi_object_id_t* object_id);
int dpl_cdmi_object_id_is_def(const dpl_cdmi_object_id_t* object_id);
dpl_status_t dpl_cdmi_object_id_opaque_len(
    const dpl_cdmi_object_id_t* object_id,
    size_t* lenp);

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CDMI_OBJECT_ID_H_
