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
#include "scsiconst.h"


int smc_scsi_xa(struct smc_ctrl_block* smc)
{
  int try = 0;
  int rc;
  int sense_key;
  unsigned char* sense_data = smc->scsi_req.sense_data;

  for (try = 0; try < 2; try++) {
    rc = (*smc->issue_scsi_req)(smc);
    if (rc || smc->scsi_req.completion_status != SMCSR_CS_GOOD) {
      strcpy(smc->errmsg, "SCSI request failed");
      if (rc == 0) rc = -1;
      continue; /* retry */
    }

    switch (SCSI_STATUS_BYTE_CODE(smc->scsi_req.status_byte)) {
      case SCSI_STATUS_GOOD:
        return 0;

      case SCSI_STATUS_CHECK_CONDITION:
        /* sense data processed below */
        break;

      default:
        strcpy(smc->errmsg, "SCSI unexpected status");
        return -1;
    }

    sense_key = sense_data[2] & SCSI_SENSE_SENSE_KEY_MASK;

    if (sense_key == SCSI_SENSE_KEY_UNIT_ATTENTION) {
      int valid;
      int asc, ascq, asq, cmd;
      long info;

      valid = sense_data[0] & SCSI_SENSE_VALID_BIT;
      info = SMC_GET4(&sense_data[3]);
      asc = sense_data[12];
      ascq = sense_data[13];
      asq = _ASQ(asc, ascq);
      cmd = smc->scsi_req.cmd[0];

      sprintf(smc->errmsg, "SCSI attn s0=%x asq=%x,%x cmd=%x info=%lx",
              sense_data[0], asc, ascq, cmd, info);

      rc = 1;
    } else {
      strcpy(smc->errmsg, "SCSI check condition");
      rc = 1;
      break; /* don't retry, investigate */
    }
  }

  if (!rc) rc = -1;
  return rc;
}


#define SINQ_MEDIA_CHANGER 0x08

int smc_inquire(struct smc_ctrl_block* smc)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  unsigned char data[128];
  int rc;
  int i;

  bzero(sr, sizeof *sr);
  bzero(data, sizeof data);

  sr->n_cmd = 6;
  sr->cmd[0] = SCSI_CMD_INQUIRY;
  sr->cmd[4] = sizeof data; /* allocation length */

  sr->data = data;
  sr->n_data_avail = sizeof data;
  sr->data_dir = SMCSR_DD_IN;

  rc = smc_scsi_xa(smc);
  if (rc != 0) return rc;

  if (data[0] != SINQ_MEDIA_CHANGER) {
    strcpy(smc->errmsg, "Not a media changer");
    return -1;
  }

  for (i = 28 - 1; i >= 0; i--) {
    int c = data[8 + i];

    if (c != ' ') break;
  }

  for (; i >= 0; i--) {
    int c = data[8 + i];

    if (!(' ' <= c && c < 0x7F)) c = '*';
    smc->ident[i] = c;
  }

  return 0;
}

int smc_test_unit_ready(struct smc_ctrl_block* smc)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  int rc;

  bzero(sr, sizeof *sr);

  sr->n_cmd = 6;
  sr->cmd[0] = SCSI_CMD_TEST_UNIT_READY;

  rc = smc_scsi_xa(smc);

  return rc;
}

int smc_get_elem_aa(struct smc_ctrl_block* smc)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  unsigned char data[256];
  int rc;

  bzero(sr, sizeof *sr);
  bzero(data, sizeof data);
  bzero(&smc->elem_aa, sizeof smc->elem_aa);
  smc->valid_elem_aa = 0;

  sr->n_cmd = 6;
  sr->cmd[0] = SCSI_CMD_MODE_SENSE_6;
  sr->cmd[1] = 0x08; /* DBD */
  sr->cmd[2] = 0x1D; /* current elem addrs */
  sr->cmd[3] = 0;    /* reserved */
  sr->cmd[4] = 255;  /* allocation length */
  sr->cmd[5] = 0;    /* reserved */

  sr->data = data;
  sr->n_data_avail = 255;
  sr->data_dir = SMCSR_DD_IN;

  rc = smc_scsi_xa(smc);
  if (rc != 0) return rc;

  if (data[0] < 18) {
    strcpy(smc->errmsg, "short sense data");
    return -1;
  }


  rc = smc_parse_element_address_assignment((void*)&data[4], &smc->elem_aa);
  if (rc) {
    strcpy(smc->errmsg, "elem_addr_assignment format error");
    return -1;
  }

  smc->valid_elem_aa = 1;

  return 0;
}

/*
 * 17.2.2 INITIALIZE ELEMENT STATUS command
 *
 * The INITIALIZE ELEMENT STATUS command (see table 329) will cause the
 * medium changer to check all elements for medium and any other status
 * relevant to that element. The intent of this command is to enable the
 * initiator to get a quick response from a following READ ELEMENT STATUS
 * command. It may be useful to issue this command after a power failure,
 * or if medium has been changed by an operator, or if configurations have
 * been changed.
 *
 *                 Table 329 - INITIALIZE ELEMENT STATUS command
 * +====-=======-========-========-========-========-========-========-======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0  |
 * |Byte|       |        |        |        |        |        |        |      |
 * |====+====================================================================|
 * | 0  |                          Operation code (07h)                      |
 * |----+--------------------------------------------------------------------|
 * | 1  |Logical unit number      |                Reserved                  |
 * |----+--------------------------------------------------------------------|
 * | 2  |                          Reserved                                  |
 * |----+--------------------------------------------------------------------|
 * | 3  |                          Reserved                                  |
 * |----+--------------------------------------------------------------------|
 * | 4  |                          Reserved                                  |
 * |----+--------------------------------------------------------------------|
 * | 5  |                          Control                                   |
 * +=========================================================================+
 */


int smc_init_elem_status(struct smc_ctrl_block* smc)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  int rc;

  bzero(sr, sizeof *sr);

  sr->n_cmd = 6;
  sr->cmd[0] = SCSI_CMD_INITIALIZE_ELEMENT_STATUS;

  sr->data_dir = SMCSR_DD_NONE;

  rc = smc_scsi_xa(smc);
  if (rc != 0) return rc;

  return 0;
}


/*
 * 17.2.5 READ ELEMENT STATUS command
 *
 * The READ ELEMENT STATUS command (see table 332) requests that the
 * target report the status of its internal elements to the initiator.
 *
 *                    Table 332 - READ ELEMENT STATUS command
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=====================================================================|
 * | 0  |                          Operation code (B8h)                       |
 * |----+---------------------------------------------------------------------|
 * | 1  |Logical unit number      | VolTag |        Element type code         |
 * |----+---------------------------------------------------------------------|
 * | 2  |(MSB)                                                                |
 * |----+--                        Starting element address                 --|
 * | 3  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 4  |(MSB)                                                                |
 * |----+--                        Number of elements                       --|
 * | 5  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 6  |                          Reserved                                   |
 * |----+---------------------------------------------------------------------|
 * | 7  |(MSB)                                                                |
 * |----+--                                                                 --|
 * | 8  |                          Allocation length                          |
 * |----+--                                                                 --|
 * | 9  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * |10  |                          Reserved                                   |
 * |----+---------------------------------------------------------------------|
 * |11  |                          Control                                    |
 * +==========================================================================+
 *
 *
 * A volume tag (VolTag) bit of one indicates that the target shall report
 * volume tag information if this feature is supported. A value of zero
 * indicates that volume tag information shall not be reported. If the
 * volume tag feature is not supported this field shall be treated as
 * reserved.
 *
 * The element type code field specifies the particular element type(s)
 * selected for reporting by this command.  A value of zero specifies that
 * status for all element types shall be reported.  The element type codes
 * are defined in table 333.
 *
 *                         Table 333 - Element type code
 *      +=============-===================================================+
 *      |    Code     |  Description                                      |
 *      |-------------+---------------------------------------------------|
 *      |      0h     |  All element types reported, (valid in CDB only)  |
 *      |      1h     |  Medium transport element                         |
 *      |      2h     |  Storage element                                  |
 *      |      3h     |  Import export element                            |
 *      |      4h     |  Data transfer element                            |
 *      |   5h - Fh   |  Reserved                                         |
 *      +=================================================================+
 *
 *
 * The starting element address specifies the minimum element address to
 * report. Only elements with an element type code permitted by the
 * element type code specification, and an element address greater than or
 * equal to the starting element address shall be reported. Element
 * descriptor blocks are not generated for undefined element addresses.
 *
 * The number of elements specifies the maximum number of element
 * descriptors to be created by the target for this command. The value
 * specified by this field is not the range of element addresses to be
 * considered for reporting but rather the number of defined elements to
 * report. If the allocation length is not sufficient to transfer all the
 * element descriptors, the target shall transfer all those descriptors
 * that can be completely transferred and this shall not be considered an
 * error.
 */

int smc_read_elem_status(struct smc_ctrl_block* smc)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  unsigned char data[SMC_PAGE_LEN];
  int rc;

retry:
  bzero(sr, sizeof *sr);
  bzero(data, sizeof data);
  smc_cleanup_element_status_data(smc);
  smc->n_elem_desc = 0;
  smc->valid_elem_desc = 0;

  sr->n_cmd = 12;
  sr->cmd[0] = SCSI_CMD_READ_ELEMENT_STATUS;
  if (!smc->dont_ask_for_voltags) {
    sr->cmd[1] = 0x10; /* VolTag, all types */
  } else {
    sr->cmd[1] = 0x00; /* !VolTag, all types */
  }
  sr->cmd[2] = 0;                             /* starting elem MSB */
  sr->cmd[3] = 0;                             /* starting elem LSB */
  sr->cmd[4] = (SMC_MAX_ELEMENT >> 8) & 0xff; /* number of elem MSB */
  sr->cmd[5] = (SMC_MAX_ELEMENT)&0xff;        /* number of elem LSB */
  sr->cmd[6] = 0;                             /* reserved */
  SMC_PUT3(&sr->cmd[7], sizeof data);
  sr->cmd[10] = 0; /* reserved */

  sr->data = data;
  sr->n_data_avail = sizeof data;
  sr->data_dir = SMCSR_DD_IN;

  rc = smc_scsi_xa(smc);
  if (rc != 0) {
    if (smc->dont_ask_for_voltags) return rc;
    smc->dont_ask_for_voltags = 1;
    goto retry;
  }

  rc = smc_parse_element_status_data((void*)data, sr->n_data_done, smc,
                                     SMC_MAX_ELEMENT);
  if (rc < 0) {
    strcpy(smc->errmsg, "elem_status format error");
    return -1;
  }

  smc->n_elem_desc = rc;

  smc->valid_elem_aa = 1;

  return 0;
}


int smc_move(struct smc_ctrl_block* smc,
             unsigned from_addr,
             unsigned to_addr,
             [[maybe_unused]] int invert,
             unsigned chs_addr)
{
  struct smc_scsi_req* sr = &smc->scsi_req;
  int rc;

  bzero(sr, sizeof *sr);

  sr->n_cmd = 12;
  sr->cmd[0] = SCSI_CMD_MOVE_MEDIUM;
  SMC_PUT2(&sr->cmd[2], chs_addr);
  SMC_PUT2(&sr->cmd[4], from_addr);
  SMC_PUT2(&sr->cmd[6], to_addr);
  /* TODO: invert */

  sr->data_dir = SMCSR_DD_NONE;

  rc = smc_scsi_xa(smc);
  if (rc != 0) return rc;

  return 0;
}

int smc_position([[maybe_unused]] struct smc_ctrl_block* smc,
                 [[maybe_unused]] unsigned to_addr,
                 [[maybe_unused]] int invert)
{
  return -1;
}


int smc_handy_move_to_drive([[maybe_unused]] struct smc_ctrl_block* smc,
                            [[maybe_unused]] unsigned from_se_ix)
{
  return -1;
}

int smc_handy_move_from_drive([[maybe_unused]] struct smc_ctrl_block* smc,
                              [[maybe_unused]] unsigned to_se_ix)
{
  return -1;
}
