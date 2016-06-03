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


#include "smc_priv.h"

static char *strend(char *s);



char *
smc_elem_type_code_to_str(int code)
{
	switch (code) {
	case SMC_ELEM_TYPE_ALL:		return "ALL";
	case SMC_ELEM_TYPE_MTE:		return "ARM";
	case SMC_ELEM_TYPE_SE:		return "SLOT";
	case SMC_ELEM_TYPE_IEE:		return "IEE";
	case SMC_ELEM_TYPE_DTE:		return "TAPE";
	default:			return "???";
	}
}

int
smc_pp_element_address_assignments (struct smc_element_address_assignment *eaa,
  int lineno, char *buf)
{
	sprintf (buf, "slots %d@%d  drive %d@%d  arm %d@%d  i/e %d@%d",
		eaa->se_count,  eaa->se_addr,
		eaa->dte_count, eaa->dte_addr,
		eaa->mte_count, eaa->mte_addr,
		eaa->iee_count, eaa->iee_addr);

	return 1;
}

int
smc_pp_element_descriptor (struct smc_element_descriptor *edp,
  int lineno, char *ret_buf)
{
	int		nline = 0;
	char		buf[100];

	*ret_buf = 0;
	*buf = 0;

	sprintf (buf, "@%-3d %-4s",
		edp->element_address,
		smc_elem_type_code_to_str(edp->element_type_code));

	if (edp->Full)
		strcat (buf, " Full ");
	else
		strcat (buf, " Empty");

	if (edp->element_type_code == SMC_ELEM_TYPE_MTE) {
		if (edp->Access) {
			/* unusual for MTE */
			/* actually not defined */
			strcat (buf, " ?access=granted?");
		}
	} else {
		if (!edp->Access) {
			/* unusual for all non-MTE elements */
			strcat (buf, " ?access=denied?");
		}
	}

	if (edp->PVolTag && edp->Full) {
		sprintf (strend(buf), " PVolTag(%s,#%d)",
			edp->primary_vol_tag->volume_id,
			edp->primary_vol_tag->volume_seq);
	}

	if (edp->Except) {
		sprintf (strend(buf), " Except(asc=%02x,ascq=%02x)",
			edp->asc, edp->ascq);
	}

	if (*buf && nline++ == lineno) strcpy (ret_buf, buf);
	*buf = 0;

#define INDENT_SPACES "          "	/* 10 spaces */

	if (edp->AVolTag) {
		sprintf (buf, INDENT_SPACES "AVolTag(%s,#%d)",
			edp->alternate_vol_tag->volume_id,
			edp->alternate_vol_tag->volume_seq);
	}

	if (*buf && nline++ == lineno) strcpy (ret_buf, buf);
	*buf = 0;

	if (edp->SValid) {
		sprintf (buf, INDENT_SPACES "SValid(src=%d,%sinvert)",
			edp->src_se_addr,
			edp->Invert ? "" : "!");
	}

	if (*buf && nline++ == lineno) strcpy (ret_buf, buf);
	*buf = 0;

	if (edp->element_type_code == SMC_ELEM_TYPE_DTE) {
		strcpy (buf, INDENT_SPACES);
		if (edp->ID_valid) {
			sprintf (strend(buf), "ID sid=%d", edp->scsi_sid);
		} else {
			strcat (buf, "no-sid-data");
		}
		if (edp->LU_valid) {
			sprintf (strend(buf), " lun=%d", edp->scsi_lun);
		} else {
			strcat (buf, " no-lun-data");
		}

		if (edp->ID_valid && edp->Not_bus) {
			strcat (buf, " not-same-bus");
		}
	}

	if (*buf && nline++ == lineno) strcpy (ret_buf, buf);
	*buf = 0;

	if (edp->element_type_code == SMC_ELEM_TYPE_IEE) {
		strcpy (buf, INDENT_SPACES);

		if (edp->InEnab)
			strcat (buf, " can-import");
		else
			strcat (buf, " can-not-import");

		if (edp->ExEnab)
			strcat (buf, " can-export");
		else
			strcat (buf, " can-not-export");

		if (edp->ImpExp)
			strcat (buf, " by-oper");
		else
			strcat (buf, " by-mte");
	}

	if (*buf && nline++ == lineno) strcpy (ret_buf, buf);
	*buf = 0;

	return nline;
}

static char *
strend (char *s)
{
	while (*s) s++;
	return s;
}
