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


#include "smc_priv.h"


#define WITH(P, V, T) \
  {                   \
    T* V = (T*)P;
#define ENDWITH }

int smc_parse_volume_tag(struct smc_raw_volume_tag* raw,
                         struct smc_volume_tag* vtag)
{
  int i;

  bzero(vtag, sizeof *vtag);

  for (i = 31; i >= 0 && raw->volume_id[i] == ' '; i--) continue;

  for (; i >= 0; i--) vtag->volume_id[i] = raw->volume_id[i];

  vtag->volume_seq = SMC_GET2(raw->volume_seq);

  return 0;
}


void smc_cleanup_element_status_data(struct smc_ctrl_block* smc)
{
  struct smc_element_descriptor *edp, *edp_next;

  edp = smc->elem_desc;
  while (edp) {
    edp_next = edp->next;
    if (edp->primary_vol_tag) { NDMOS_API_FREE(edp->primary_vol_tag); }
    if (edp->alternate_vol_tag) { NDMOS_API_FREE(edp->alternate_vol_tag); }
    NDMOS_API_FREE(edp);
    edp = edp_next;
  }

  smc->elem_desc = (struct smc_element_descriptor*)NULL;
  smc->elem_desc_tail = (struct smc_element_descriptor*)NULL;
}


int smc_parse_element_status_data(char* raw,
                                  unsigned raw_len,
                                  struct smc_ctrl_block* smc,
                                  unsigned max_elem_desc)
{
  unsigned char* p = (unsigned char*)raw;
  unsigned char* raw_end = p + raw_len;
  unsigned int n_elem = 0;
  struct smc_element_descriptor* edp;

  smc_cleanup_element_status_data(smc);

  WITH(p, esdh, struct smc_raw_element_status_data_header)
  unsigned byte_count = SMC_GET3(esdh->byte_count);

  if (raw_len > byte_count + 8) {
    /* probably an error, but press on */
    raw_len = byte_count + 8;
  }
  raw_end = p + raw_len;
  ENDWITH

  p += 8;

  while (p + 8 < raw_end) { /* +8 for sizeof *esph */
    WITH(p, esph, struct smc_raw_element_status_page_header)
    unsigned desc_size = SMC_GET2(esph->elem_desc_len);
    unsigned byte_count = SMC_GET3(esph->byte_count);
    unsigned elem_type = esph->element_type;
    unsigned char* pgend = p + byte_count + 8;
    int PVolTag = 0;
    int AVolTag = 0;

    if (pgend > raw_end) {
      /* malformed, really, but punt */
      pgend = raw_end;
    }

    if (esph->flag1 & SMC_RAW_ESP_F1_PVolTag) PVolTag = 1;
    if (esph->flag1 & SMC_RAW_ESP_F1_AVolTag) AVolTag = 1;

    p += 8;

    for (; p + desc_size <= pgend; p += desc_size) {
      if (n_elem >= max_elem_desc) {
        /* bust out */
        goto done;
      }

      edp = NDMOS_API_MALLOC(sizeof(struct smc_element_descriptor));
      NDMOS_MACRO_ZEROFILL(edp);

      WITH(p, red, struct smc_raw_element_descriptor)
      unsigned char* p2;

      edp->element_type_code = elem_type;
      edp->element_address = SMC_GET2(red->element_address);
      edp->PVolTag = PVolTag;
      edp->AVolTag = AVolTag;
#define FLAG(RAWMEM, BIT, MEM) \
  if (red->RAWMEM & BIT) edp->MEM = 1;
      FLAG(flags2, SMC_RAW_ED_F2_Full, Full);
      FLAG(flags2, SMC_RAW_ED_F2_ImpExp, ImpExp);
      FLAG(flags2, SMC_RAW_ED_F2_Except, Except);
      FLAG(flags2, SMC_RAW_ED_F2_Access, Access);
      FLAG(flags2, SMC_RAW_ED_F2_ExEnab, ExEnab);
      FLAG(flags2, SMC_RAW_ED_F2_InEnab, InEnab);

      edp->asc = red->asc;
      edp->ascq = red->ascq;

      edp->scsi_lun = red->flags6 & 7;
      FLAG(flags6, SMC_RAW_ED_F6_LU_valid, LU_valid);
      FLAG(flags6, SMC_RAW_ED_F6_ID_valid, ID_valid);
      FLAG(flags6, SMC_RAW_ED_F6_Not_bus, Not_bus);

      edp->scsi_sid = red->scsi_sid;

      FLAG(flags9, SMC_RAW_ED_F9_Invert, Invert);
      FLAG(flags9, SMC_RAW_ED_F9_SValid, SValid);
#undef FLAG

      edp->src_se_addr = SMC_GET2(red->src_se_addr);

      p2 = (unsigned char*)&red->data;
      if (edp->PVolTag) {
        edp->primary_vol_tag = NDMOS_API_MALLOC(sizeof(struct smc_volume_tag));
        smc_parse_volume_tag((void*)p2, edp->primary_vol_tag);
        p2 += SMC_VOL_TAG_LEN;
      }
      if (edp->AVolTag) {
        edp->alternate_vol_tag =
            NDMOS_API_MALLOC(sizeof(struct smc_volume_tag));
        smc_parse_volume_tag((void*)p2, edp->alternate_vol_tag);
        p2 += SMC_VOL_TAG_LEN;
      }
      p2 += 4; /* resv84 */
               /* p2 ready for vendor_specific */
      ENDWITH

      if (!smc->elem_desc_tail) {
        smc->elem_desc = edp;
        smc->elem_desc_tail = edp;
      } else {
        smc->elem_desc_tail->next = edp;
        smc->elem_desc_tail = edp;
      }
    }
    p = pgend;
    ENDWITH
  }

done:
  return n_elem;
}


int smc_parse_element_address_assignment(
    struct smc_raw_element_address_assignment_page* raw,
    struct smc_element_address_assignment* eaa)
{
  eaa->mte_addr = SMC_GET2(raw->mte_addr);
  eaa->mte_count = SMC_GET2(raw->mte_count);
  eaa->se_addr = SMC_GET2(raw->se_addr);
  eaa->se_count = SMC_GET2(raw->se_count);
  eaa->iee_addr = SMC_GET2(raw->iee_addr);
  eaa->iee_count = SMC_GET2(raw->iee_count);
  eaa->dte_addr = SMC_GET2(raw->dte_addr);
  eaa->dte_count = SMC_GET2(raw->dte_count);

  return 0;
}
