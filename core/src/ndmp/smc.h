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

#ifndef BAREOS_SRC_NDMP_SMCC_H_
#define BAREOS_SRC_NDMP_SMCC_H_


#ifdef  __cplusplus
extern "C" {
#endif

#define SMC_MAX_SENSE_DATA	127

/* carefully layed out so that 16-byte/line hex dumps look nice */
struct smc_scsi_req {
	unsigned char	completion_status;
	unsigned char	status_byte;
	unsigned char	data_dir;
	unsigned char	n_cmd;

	unsigned char	cmd[12];

	unsigned char *	data;
	unsigned	n_data_avail;
	unsigned	n_data_done;
	uint32_t	_pad;

	unsigned char	n_sense_data;
	unsigned char	sense_data[SMC_MAX_SENSE_DATA];
};

#define SMCSR_CS_GOOD	0
#define SMCSR_CS_FAIL	1
/* more? */

#define SMCSR_DD_NONE	0
#define SMCSR_DD_IN	1	/* adapter->app */
#define SMCSR_DD_OUT	2	/* app->adapter */



struct smc_volume_tag {
	unsigned char	volume_id[32];
	uint16_t	volume_seq;
};



struct smc_element_address_assignment {
	unsigned	mte_addr;	/* media transport element */
	unsigned	mte_count;

	unsigned	se_addr;	/* storage element */
	unsigned	se_count;

	unsigned	iee_addr;	/* import/export element */
	unsigned	iee_count;

	unsigned	dte_addr;	/* data transfer element */
	unsigned	dte_count;
};


#define SMC_ELEM_TYPE_ALL	0
#define SMC_ELEM_TYPE_MTE	1
#define SMC_ELEM_TYPE_SE	2
#define SMC_ELEM_TYPE_IEE	3
#define SMC_ELEM_TYPE_DTE	4

struct smc_element_descriptor {
	unsigned char	element_type_code;
	uint16_t	element_address;

	/* Flags, use SCSI spec names for convenience */
	unsigned	PVolTag : 1;	/* MSID primary vol tag info present */
	unsigned	AVolTag : 1;	/* MSID alternate vol tag present */
	unsigned	InEnab	: 1;	/* --I- supports import */
	unsigned	ExEnab	: 1;	/* --I- supports export */
	unsigned	Access	: 1;	/* -SID access by a MTE allowed */
	unsigned	Except	: 1;	/* MSID element in abnormal state */
	unsigned	ImpExp	: 1;	/* --I- placed by operator */
	unsigned	Full	: 1;	/* MSID contains a unit of media */
	unsigned	Not_bus	: 1;	/* ---D if ID_valid, not same bus */
	unsigned	ID_valid: 1;	/* ---D scsi_sid valid */
	unsigned	LU_valid: 1;	/* ---D scsi_lun valid */
	unsigned	SValid	: 1;	/* MSID src_se_addr and Invert valid */
	unsigned	Invert	: 1;	/* MSID inverted by MOVE/EXCHANGE */

	unsigned char	asc;		/* Additional sense code */
	unsigned char	ascq;		/* Additional sense code qualifier */
	uint16_t	src_se_addr;	/* if Svalid, last *STORAGE* element */

	unsigned char	scsi_sid;	/* if ID_valid, SID of drive */
	unsigned char	scsi_lun;	/* if LU_valid, LUN of drive */

	struct smc_volume_tag *primary_vol_tag;	/* if PVolTag */
	struct smc_volume_tag *alternate_vol_tag;/* if AVolTag */
	struct smc_element_descriptor *next;
};



#ifndef SMC_PAGE_LEN
#define SMC_PAGE_LEN		32768
#endif

#ifndef SMC_MAX_ELEMENT
#define SMC_MAX_ELEMENT		320
#endif

struct smc_ctrl_block {
	unsigned char		ident[32];

	unsigned char		valid_elem_aa;
	unsigned char		valid_elem_desc;

	struct smc_element_address_assignment
				elem_aa;

	struct smc_element_descriptor *
				elem_desc;
	struct smc_element_descriptor *
				elem_desc_tail;
	unsigned		n_elem_desc;

	struct smc_scsi_req	scsi_req;

	int			(*issue_scsi_req)(struct smc_ctrl_block *smc);
	void *			app_data;

	int			dont_ask_for_voltags;

	char			errmsg[64];
};


extern void	smc_cleanup_element_status_data (struct smc_ctrl_block *smc);
extern int	smc_inquire (struct smc_ctrl_block *smc);
extern int	smc_test_unit_ready (struct smc_ctrl_block *smc);
extern int	smc_get_elem_aa (struct smc_ctrl_block *smc);
extern int	smc_init_elem_status (struct smc_ctrl_block *smc);
extern int	smc_read_elem_status (struct smc_ctrl_block *smc);

extern int	smc_move (struct smc_ctrl_block *smc,
			unsigned from_addr, unsigned to_addr, int invert,
			unsigned chs_addr);
extern int	smc_position (struct smc_ctrl_block *smc,
			unsigned to_addr, int invert);

extern int	smc_handy_move_to_drive (struct smc_ctrl_block *smc,
			unsigned from_se_ix);
extern int	smc_handy_move_from_drive (struct smc_ctrl_block *smc,
			unsigned to_se_ix);

extern char *	smc_elem_type_code_to_str(int code);

extern int	smc_pp_element_address_assignments (
			struct smc_element_address_assignment *eaa,
			int lineno, char *buf);

extern int	smc_pp_element_descriptor (struct smc_element_descriptor *edp,
			int lineno, char *ret_buf);

#ifdef  __cplusplus
}
#endif

#endif // BAREOS_SRC_NDMP_SMCC_H_
