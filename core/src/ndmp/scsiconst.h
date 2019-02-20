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

/* clang-format off */

/*
 * WORKING                                                 X3T9.2
 * DRAFT                                                   Project 375D
 */

/*
                      Table D.2 - SCSI-2 Operation Codes
=============================================================================
           D - DIRECT ACCESS Device                       Device column key
           .T - SEQUENTIAL ACCESS Device                  M = Mandatory
           . L - PRINTER Device                           O = Optional
           .  P - PROCESSOR Device                        V = Vendor-specific
           .  .W - WRITE ONCE READ MULTIPLE Device        R = Reserved
           .  . R - READ ONLY (CD-ROM) Device
           .  .  S - SCANNER Device
           .  .  .O - OPTICAL MEMORY Device
           .  .  . M - MEDIA CHANGER Device
           .  .  .  C - COMMUNICATION Device
           .  .  .  .
           DTLPWRSOMC
*/

/*OP       DTLPWRSOMC */
#define SCSI_CMD_TEST_UNIT_READY 0x00                /* MMMMMMMMMM */
#define SCSI_CMD_REWIND 0x01                         /* _M________ */
#define SCSI_CMD_REZERO_UNIT 0x01                    /* O_V_OO_OO_ */
#define SCSI_CMD_REQUEST_SENSE 0x03                  /* MMMMMMMMMM */
#define SCSI_CMD_FORMAT 0x04                         /* __O_______ */
#define SCSI_CMD_FORMAT_UNIT 0x04                    /* M______O__ */
#define SCSI_CMD_READ_BLOCK_LIMITS 0x05              /* VMVVVV__V_ */
#define SCSI_CMD_INITIALIZE_ELEMENT_STATUS 0x07      /* ________O_ */
#define SCSI_CMD_REASSIGN_BLOCKS 0x07                /* OVV_O__OV_ */
#define SCSI_CMD_GET_MESSAGE_6 0x08                  /* _________M */
#define SCSI_CMD_READ_6 0x08                         /* OMV_OO_OV_ */
#define SCSI_CMD_RECEIVE 0x08                        /* ___O______ */
#define SCSI_CMD_PRINT 0x0A                          /* __M_______ */
#define SCSI_CMD_SEND_MESSAGE_6 0x0A                 /* _________M */
#define SCSI_CMD_SEND_6 0x0A                         /* ___M______ */
#define SCSI_CMD_WRITE_6 0x0A                        /* OM__O__OV_ */
#define SCSI_CMD_SEEK_6 0x0B                         /* O___OO_OV_ */
#define SCSI_CMD_SLEW_AND_PRINT 0x0B                 /* __O_______ */
#define SCSI_CMD_READ_REVERSE 0x0F                   /* VOVVVV__V_ */
#define SCSI_CMD_SYNCHRONIZE_BUFFER 0x10             /* __O_O_____ */
#define SCSI_CMD_WRITE_FILEMARKS 0x10                /* VM_VVV____ */
#define SCSI_CMD_SPACE 0x11                          /* VMVVVV____ */
#define SCSI_CMD_INQUIRY 0x12                        /* MMMMMMMMMM */
#define SCSI_CMD_VERIFY_6 0x13                       /* VOVVVV____ */
#define SCSI_CMD_RECOVER_BUFFERED_DATA 0x14          /* VOOVVV____ */
#define SCSI_CMD_MODE_SELECT_6 0x15                  /* OMO_OOOOOO */
#define SCSI_CMD_RESERVE 0x16                        /* M___MM_MO_ */
#define SCSI_CMD_RESERVE_UNIT 0x16                   /* _MM___M___ */
#define SCSI_CMD_RELEASE 0x17                        /* M___MM_MO_ */
#define SCSI_CMD_RELEASE_UNIT 0x17                   /* _MM___M___ */
#define SCSI_CMD_COPY 0x18                           /* OOOOOOOO__ */
#define SCSI_CMD_ERASE 0x19                          /* VMVVVV____ */
#define SCSI_CMD_MODE_SENSE_6 0x1A                   /* OMO_OOOOOO */
#define SCSI_CMD_LOAD_UNLOAD 0x1B                    /* _O________ */
#define SCSI_CMD_SCAN 0x1B                           /* ______O___ */
#define SCSI_CMD_STOP_PRINT 0x1B                     /* __O_______ */
#define SCSI_CMD_STOP_START_UNIT 0x1B                /* O___OO_O__ */
#define SCSI_CMD_RECEIVE_DIAGNOSTIC_RESULTS 0x1C     /* OOOOOOOOOO */
#define SCSI_CMD_SEND_DIAGNOSTIC 0x1D                /* MMMMMMMMMM */
#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1E   /* OO__OO_OO_ */
#define SCSI_CMD_SET_WINDOW 0x24                     /* V___VVM___ */
#define SCSI_CMD_GET_WINDOW 0x25                     /* ______O___ */
#define SCSI_CMD_READ_CAPACITY 0x25                  /* M___M__M__ */
#define SCSI_CMD_READ_CD_ROM_CAPACITY 0x25           /* _____M____ */
#define SCSI_CMD_GET_MESSAGE_10 0x28                 /* _________O */
#define SCSI_CMD_READ_10 0x28                        /* M___MMMM__ */
#define SCSI_CMD_READ_GENERATION 0x29                /* V___VV_O__ */
#define SCSI_CMD_SEND_MESSAGE_10 0x2A                /* _________O */
#define SCSI_CMD_SEND_10 0x2A                        /* ______O___ */
#define SCSI_CMD_WRITE_10 0x2A                       /* O___M__M__ */
#define SCSI_CMD_LOCATE 0x2B                         /* _O________ */
#define SCSI_CMD_POSITION_TO_ELEMENT 0x2B            /* ________O_ */
#define SCSI_CMD_SEEK_10 0x2B                        /* O___OO_O__ */
#define SCSI_CMD_ERASE_10 0x2C                       /* V______O__ */
#define SCSI_CMD_READ_UPDATED_BLOCK 0x2D             /* V___O__O__ */
#define SCSI_CMD_WRITE_AND_VERIFY_10 0x2E            /* O___O__O__ */
#define SCSI_CMD_VERIFY_10 0x2F                      /* O___OO_O__ */
#define SCSI_CMD_SEARCH_DATA_HIGH_10 0x30            /* O___OO_O__ */
#define SCSI_CMD_OBJECT_POSITION 0x31                /* ______O___ */
#define SCSI_CMD_SEARCH_DATA_EQUAL_10 0x31           /* O___OO_O__ */
#define SCSI_CMD_SEARCH_DATA_LOW_10 0x32             /* O___OO_O__ */
#define SCSI_CMD_SET_LIMITS_10 0x33                  /* O___OO_O__ */
#define SCSI_CMD_GET_DATA_BUFFER_STATUS 0x34         /* ______O___ */
#define SCSI_CMD_PRE_FETCH 0x34                      /* O___OO_O__ */
#define SCSI_CMD_READ_POSITION 0x34                  /* _O________ */
#define SCSI_CMD_SYNCHRONIZE_CACHE 0x35              /* O___OO_O__ */
#define SCSI_CMD_LOCK_UNLOCK_CACHE 0x36              /* O___OO_O__ */
#define SCSI_CMD_READ_DEFECT_DATA_10 0x37            /* O______O__ */
#define SCSI_CMD_MEDIUM_SCAN 0x38                    /* ____O__O__ */
#define SCSI_CMD_COMPARE 0x39                        /* OOOOOOOO__ */
#define SCSI_CMD_COPY_AND_VERIFY 0x3A                /* OOOOOOOO__ */
#define SCSI_CMD_WRITE_BUFFER 0x3B                   /* OOOOOOOOOO */
#define SCSI_CMD_READ_BUFFER 0x3C                    /* OOOOOOOOOO */
#define SCSI_CMD_UPDATE_BLOCK 0x3D                   /* ____O__O__ */
#define SCSI_CMD_READ_LONG 0x3E                      /* O___OO_O__ */
#define SCSI_CMD_WRITE_LONG 0x3F                     /* O___O__O__ */
#define SCSI_CMD_CHANGE_DEFINITION 0x40              /* OOOOOOOOOO */
#define SCSI_CMD_WRITE_SAME 0x41                     /* O_________ */
#define SCSI_CMD_READ_SUB_CHANNEL 0x42               /* _____O____ */
#define SCSI_CMD_READ_TOC 0x43                       /* _____O____ */
#define SCSI_CMD_READ_HEADER 0x44                    /* _____O____ */
#define SCSI_CMD_PLAY_AUDIO_10 0x45                  /* _____O____ */
#define SCSI_CMD_PLAY_AUDIO_MSF 0x47                 /* _____O____ */
#define SCSI_CMD_PLAY_AUDIO_TRACK_INDEX 0x48         /* _____O____ */
#define SCSI_CMD_PLAY_TRACK_RELATIVE_10 0x49         /* _____O____ */
#define SCSI_CMD_PAUSE_RESUME 0x4B                   /* _____O____ */
#define SCSI_CMD_LOG_SELECT 0x4C                     /* OOOOOOOOOO */
#define SCSI_CMD_LOG_SENSE 0x4D                      /* OOOOOOOOOO */
#define SCSI_CMD_MODE_SELECT_10 0x55                 /* OOO_OOOOOO */
#define SCSI_CMD_MODE_SENSE_10 0x5A                  /* OOO_OOOOOO */
#define SCSI_CMD_MOVE_MEDIUM 0xA5                    /* ________M_ */
#define SCSI_CMD_PLAY_AUDIO_12 0xA5                  /* _____O____ */
#define SCSI_CMD_EXCHANGE_MEDIUM 0xA6                /* ________O_ */
#define SCSI_CMD_GET_MESSAGE_12 0xA8                 /* _________O */
#define SCSI_CMD_READ_12 0xA8                        /* ____OO_O__ */
#define SCSI_CMD_PLAY_TRACK_RELATIVE_12 0xA9         /* _____O____ */
#define SCSI_CMD_SEND_MESSAGE_12 0xAA                /* _________O */
#define SCSI_CMD_WRITE_12 0xAA                       /* ____O__O__ */
#define SCSI_CMD_ERASE_12 0xAC                       /* _______O__ */
#define SCSI_CMD_WRITE_AND_VERIFY_12 0xAE            /* ____O__O__ */
#define SCSI_CMD_VERIFY_12 0xAF                      /* ____OO_O__ */
#define SCSI_CMD_SEARCH_DATA_HIGH_12 0xB0            /* ____OO_O__ */
#define SCSI_CMD_SEARCH_DATA_EQUAL_12 0xB1           /* ____OO_O__ */
#define SCSI_CMD_SEARCH_DATA_LOW_12 0xB2             /* ____OO_O__ */
#define SCSI_CMD_SET_LIMITS_12 0xB3                  /* ____OO_O__ */
#define SCSI_CMD_REQUEST_VOLUME_ELEMENT_ADDRESS 0xB5 /* ________O_ */
#define SCSI_CMD_SEND_VOLUME_TAG 0xB6                /* ________O_ */
#define SCSI_CMD_READ_DEFECT_DATA_12 0xB7            /* _______O__ */
#define SCSI_CMD_READ_ELEMENT_STATUS 0xB8            /* ________O_ */


/*
 *                             Table 26 - Status byte
 * +========-========-========-========-========-========-========-========+
 * |   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
 * |=================+============================================+========|
 * |     Reserved    |               Status byte code             |Reserved|
 * +=======================================================================+
 *
 *
 *                          Table 27 - Status byte code
 * +==================================-==============================+
 * |    Bits of status byte           |  Status                      |
 * |----------------------------------|                              |
 * |   7 | 6 | 5 | 4 | 3 | 2 | 1 | 0  |                              |
 * |-----+---+---+---+---+---+---+----+------------------------------|
 * |   R | R | 0 | 0 | 0 | 0 | 0 | R  |  GOOD                        |
 * |   R | R | 0 | 0 | 0 | 0 | 1 | R  |  CHECK CONDITION             |
 * |   R | R | 0 | 0 | 0 | 1 | 0 | R  |  CONDITION MET               |
 * |   R | R | 0 | 0 | 1 | 0 | 0 | R  |  BUSY                        |
 * |   R | R | 0 | 1 | 0 | 0 | 0 | R  |  INTERMEDIATE                |
 * |   R | R | 0 | 1 | 0 | 1 | 0 | R  |  INTERMEDIATE-CONDITION MET  |
 * |   R | R | 0 | 1 | 1 | 0 | 0 | R  |  RESERVATION CONFLICT        |
 * |   R | R | 1 | 0 | 0 | 0 | 1 | R  |  COMMAND TERMINATED          |
 * |   R | R | 1 | 0 | 1 | 0 | 0 | R  |  QUEUE FULL                  |
 * |----------------------------------|                              |
 * |           All other codes        |  Reserved                    |
 * |-----------------------------------------------------------------|
 * |  Key: R = Reserved bit                                          |
 * +=================================================================+
 *
 * A definition of the status byte codes is given below.
 *
 * 7.3.1 GOOD:  This status indicates that the target has successfully
 * completed the command.
 *
 * 7.3.2 CHECK CONDITION:  This status indicates that a contingent allegiance
 * condition has occurred (see 7.6).
 *
 * 7.3.3 CONDITION MET:  This status or INTERMEDIATE-CONDITION MET is
 * returned whenever the requested operation is satisfied (see the SEARCH
 * DATA and PREFETCH commands).
 *
 * 7.3.4 BUSY:  This status indicates that the target is busy.  This status
 * shall be returned whenever a target is unable to accept a command from an
 * otherwise acceptable initiator (i.e. no reservation conflicts).  The
 * recommended initiator recovery action is to issue the command again at a
 * later time.
 *
 * 7.3.5 INTERMEDIATE:  This status or INTERMEDIATE-CONDITION MET shall be
 * returned for every successfully completed command in a series of linked
 * commands (except the last command), unless the command is terminated with
 * CHECK CONDITION, RESERVATION CONFLICT, or COMMAND TERMINATED status.  If
 * INTERMEDIATE or INTERMEDIATE-CONDITION MET status is not returned, the
 * series of linked commands is terminated and the I/O process is ended.
 *
 * 7.3.6 INTERMEDIATE-CONDITION MET:  This status is the combination of the
 * CONDITION MET and INTERMEDIATE statuses.
 *
 * 7.3.7 RESERVATION CONFLICT:  This status shall be returned whenever an
 * initiator attempts to access a logical unit or an extent within a logical
 * unit that is reserved with a conflicting reservation type for another SCSI
 * device (see the RESERVE and RESERVE UNIT commands).  The recommended
 * initiator recovery action is to issue the command again at a later time.
 *
 * 7.3.8 COMMAND TERMINATED:  This status shall be returned whenever the
 * target terminates the current I/O process after receiving a TERMINATE I/O
 * PROCESS message (see 6.6.22).  This status also indicates that a
 * contingent allegiance condition has occurred (see 7.6).
 *
 * 7.3.9 QUEUE FULL:  This status shall be implemented if tagged queuing is
 * implemented.  This status is returned when a SIMPLE QUEUE TAG, ORDERED
 * QUEUE TAG, or HEAD OF QUEUE TAG message is received and the command queue
 * is full.  The I/O process is not placed in the command queue.
 */
/* Standard SCSI status byte values. */
#define SCSI_STATUS_MASK 0x3E
#define SCSI_STATUS_BYTE_CODE(X) ((X)&SCSI_STATUS_MASK)
#define SCSI_STATUS_GOOD 0x00
#define SCSI_STATUS_CHECK_CONDITION 0x02
#define SCSI_STATUS_CONDITION_MET 0x04
#define SCSI_STATUS_BUSY 0x08
#define SCSI_STATUS_INTERMEDIATE_GOOD 0x10
#define SCSI_STATUS_INTERMEDIATE_MET 0x14
#define SCSI_STATUS_RESERVATION_CONFLICT 0x18
#define SCSI_STATUS_COMMAND_TERMINATED 0x22
#define SCSI_STATUS_QUEUE_FULL 0x28


#define SCSI_SENSE_VALID_BIT 0x80
#define SCSI_SENSE_FILEMARK_BIT 0x80
#define SCSI_SENSE_EOM_BIT 0x40
#define SCSI_SENSE_ILI_BIT 0x20
#define SCSI_SENSE_SENSE_KEY_MASK 0x0F

/*
8.2.14.3 Sense key and sense code definitions
*/
#define SCSI_SENSE_KEY_NO_SENSE 0x0
#define SCSI_SENSE_KEY_RECOVERED_ERROR 0x1
#define SCSI_SENSE_KEY_NOT_READY 0x2
#define SCSI_SENSE_KEY_MEDIUM_ERROR 0x3
#define SCSI_SENSE_KEY_HARDWARE_ERROR 0x4
#define SCSI_SENSE_KEY_ILLEGAL_REQUEST 0x5
#define SCSI_SENSE_KEY_UNIT_ATTENTION 0x6
#define SCSI_SENSE_KEY_DATA_PROTECT 0x7
#define SCSI_SENSE_KEY_BLANK_CHECK 0x8
#define SCSI_SENSE_KEY_VENDOR_SPECIFIC 0x9
#define SCSI_SENSE_KEY_COPY_ABORTED 0xA
#define SCSI_SENSE_KEY_ABORTED_COMMAND 0xB
#define SCSI_SENSE_KEY_EQUAL 0xC
#define SCSI_SENSE_KEY_VOLUME_OVERFLOW 0xD
#define SCSI_SENSE_KEY_MISCOMPARE 0xE
#define SCSI_SENSE_KEY_RESERVED 0xF


/*
================================================================
           D - DIRECT ACCESS Device
           .T - SEQUENTIAL ACCESS Device
           . L - PRINTER Device
           .  P - PROCESSOR Device
           .  .W - WRITE ONCE READ MULTIPLE Device
           .  . R - READ ONLY (CD-ROM) Device
           .  .  S - SCANNER Device
           .  .  .O - OPTICAL MEMORY Device
           .  .  . M - MEDIA CHANGER Device
           .  .  .  C - COMMUNICATION Device
           .  .  .  .
           DTLPWRSOMC
*/

#ifndef _ASQ
#define _ASQ(ASC, ASCQ) (((ASC) << 8) | (ASCQ))
#endif

#define ASQ_NO_ADDITIONAL_SENSE_INFORMATION _ASQ(0x00, 0x00)             /* DTLPWRSOMC */
#define ASQ_FILEMARK_DETECTED _ASQ(0x00, 0x01)                           /* _T________ */
#define ASQ_END_OF_PARTITION_OR_MEDIUM_DETECTED _ASQ(0x00, 0x02)         /* _T____S___ */
#define ASQ_SETMARK_DETECTED _ASQ(0x00, 0x03)                            /* _T________ */
#define ASQ_BEGINNING_OF_PARTITION_OR_MEDIUM_DETECTED _ASQ(0x00, 0x04)   /* _T____S___ */
#define ASQ_END_OF_DATA_DETECTED _ASQ(0x00, 0x05)                        /* _T____S___ */
#define ASQ_IO_PROCESS_TERMINATED _ASQ(0x00, 0x06)                       /* DTLPWRSOMC */
#define ASQ_AUDIO_PLAY_OPERATION_IN_PROGRESS _ASQ(0x00, 0x11)            /* R_________ */
#define ASQ_AUDIO_PLAY_OPERATION_PAUSED _ASQ(0x00, 0x12)                 /* R_________ */
#define ASQ_AUDIO_PLAY_OPERATION_SUCCESSFULLY_COMPLETED _ASQ(0x00, 0x13) /* R_________ */
#define ASQ_AUDIO_PLAY_OPERATION_STOPPED_DUE_TO_ERROR _ASQ(0x00, 0x14)   /* R_________ */
#define ASQ_NO_CURRENT_AUDIO_STATUS_TO_RETURN                                                  \
  _ASQ(0x00, 0x15)                                                               /* R_________ \
                                                                                  */
#define ASQ_NO_INDEX_OR_SECTOR_SIGNAL _ASQ(0x01, 0x00)                           /* DW__O_____ */
#define ASQ_NO_SEEK_COMPLETE _ASQ(0x02, 0x00)                                    /* DWR_OM____ */
#define ASQ_PERIPHERAL_DEVICE_WRITE_FAULT _ASQ(0x03, 0x00)                       /* DTL_W_SO__ */
#define ASQ_NO_WRITE_CURRENT _ASQ(0x03, 0x01)                                    /* _T________ */
#define ASQ_EXCESSIVE_WRITE_ERRORS _ASQ(0x03, 0x02)                              /* _T________ */
#define ASQ_LOGICAL_UNIT_NOT_READY_CAUSE_NOT_REPORTABLE _ASQ(0x04, 0x00)         /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_IS_IN_PROCESS_OF_BECOMING_READY _ASQ(0x04, 0x01)        /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_NOT_READY_INITIALIZING_REQUIRED _ASQ(0x04, 0x02)        /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_NOT_READY_MANUAL_INTERVENTION_REQUIRED _ASQ(0x04, 0x03) /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_NOT_READY_FORMAT_IN_PROGRESS _ASQ(0x04, 0x04)           /* DTL____O__ */
#define ASQ_LOGICAL_UNIT_DOES_NOT_RESPOND_TO_SELECTION _ASQ(0x05, 0x00)          /* DTL_WRSOMC */
#define ASQ_NO_REFERENCE_POSITION_FOUND _ASQ(0x06, 0x00)                         /* DWR_OM__NO */
#define ASQ_MULTIPLE_PERIPHERAL_DEVICES_SELECTED _ASQ(0x07, 0x00)                /* DTL_WRSOM_ */
#define ASQ_LOGICAL_UNIT_COMMUNICATION_FAILURE                                               \
  _ASQ(0x08, 0x00)                                                             /* DTL_WRSOMC \
                                                                                */
#define ASQ_LOGICAL_UNIT_COMMUNICATION_TIME_OUT _ASQ(0x08, 0x01)               /* DTL_WRSOMC */
#define ASQ_LOGICAL_UNIT_COMMUNICATION_PARITY_ERROR _ASQ(0x08, 0x02)           /* DTL_WRSOMC */
#define ASQ_TRACK_FOLLOWING_ERROR _ASQ(0x09, 0x00)                             /* DT__WR_O__ */
#define ASQ_TRACKING_SERVO_FAILURE _ASQ(0x09, 0x01)                            /* ____WR_O__ */
#define ASQ_FOCUS_SERVO_FAILURE _ASQ(0x09, 0x02)                               /* ____WR_O__ */
#define ASQ_SPINDLE_SERVO_FAILURE _ASQ(0x09, 0x03)                             /* ____WR_O__ */
#define ASQ_ERROR_LOG_OVERFLOW _ASQ(0x0A, 0x00)                                /* DTLPWRSOMC */
#define ASQ_WRITE_ERROR _ASQ(0x0C, 0x00)                                       /* ___T_____S */
#define ASQ_WRITE_ERROR_RECOVERED_WITH_AUTO_REALLOCATION _ASQ(0x0C, 0x01)      /* D___W__O__ */
#define ASQ_WRITE_ERROR_AUTO_REALLOCATION_FAILED _ASQ(0x0C, 0x02)              /* D___W__O__ */
#define ASQ_ID_CRC_OR_ECC_ERROR _ASQ(0x10, 0x00)                               /* D___W__O__ */
#define ASQ_UNRECOVERED_READ_ERROR _ASQ(0x11, 0x00)                            /* DT__WRSO__ */
#define ASQ_READ_RETRIES_EXHAUSTED _ASQ(0x11, 0x01)                            /* DT__W_SO__ */
#define ASQ_ERROR_TOO_LONG_TO_CORRECT _ASQ(0x11, 0x02)                         /* DT__W_SO__ */
#define ASQ_MULTIPLE_READ_ERRORS _ASQ(0x11, 0x03)                              /* DT__W_SO__ */
#define ASQ_UNRECOVERED_READ_ERROR_AUTO_REALLOCATE_FAILED _ASQ(0x11, 0x04)     /* D___W__O__ */
#define ASQ_L_EC_UNCORRECTABLE_ERROR _ASQ(0x11, 0x05)                          /* ____WR_O__ */
#define ASQ_CIRC_UNRECOVERED_ERROR _ASQ(0x11, 0x06)                            /* ____WR_O__ */
#define ASQ_DATA_RESYNCHRONIZATION_ERROR _ASQ(0x11, 0x07)                      /* ____W__O__ */
#define ASQ_INCOMPLETE_BLOCK_READ _ASQ(0x11, 0x08)                             /* _T________ */
#define ASQ_NO_GAP_FOUND _ASQ(0x11, 0x09)                                      /* _T________ */
#define ASQ_MISCORRECTED_ERROR _ASQ(0x11, 0x0A)                                /* DT_____O__ */
#define ASQ_UNRECOVERED_READ_ERROR_RECOMMEND_REASSIGNMENT _ASQ(0x11, 0x0B)     /* D___W__O__ */
#define ASQ_UNRECOVERED_READ_ERROR_RECOMMEND_REWRITE_THE_DATA _ASQ(0x11, 0x0C) /* D___W__O__ */
#define ASQ_ADDRESS_MARK_NOT_FOUND_FOR_ID_FIELD _ASQ(0x12, 0x00)               /* D___W__O__ */
#define ASQ_ADDRESS_MARK_NOT_FOUND_FOR_DATA_FIELD _ASQ(0x13, 0x00)             /* D___W__O__ */
#define ASQ_RECORDED_ENTITY_NOT_FOUND _ASQ(0x14, 0x00)                         /* DTL_WRSO__ */
#define ASQ_RECORD_NOT_FOUND _ASQ(0x14, 0x01)                                  /* DT__WR_O__ */
#define ASQ_FILEMARK_OR_SETMARK_NOT_FOUND _ASQ(0x14, 0x02)                     /* _T________ */
#define ASQ_END_OF_DATA_NOT_FOUND _ASQ(0x14, 0x03)                             /* _T________ */
#define ASQ_BLOCK_SEQUENCE_ERROR _ASQ(0x14, 0x04)                              /* _T________ */
#define ASQ_RANDOM_POSITIONING_ERROR _ASQ(0x15, 0x00)                          /* DTL_WRSOM_ */
#define ASQ_MECHANICAL_POSITIONING_ERROR _ASQ(0x15, 0x01)                      /* DTL_WRSOM_ */
#define ASQ_POSITIONING_ERROR_DETECTED_BY_READ_OF_MEDIUM _ASQ(0x15, 0x02)      /* DT__WR_O__ */
#define ASQ_DATA_SYNCHRONIZATION_MARK_ERROR _ASQ(0x16, 0x00)                   /* DW_____O__ */
#define ASQ_RECOVERED_DATA_WITH_NO_ERROR_CORRECTION_APPLIED _ASQ(0x17, 0x00)   /* DT__WRSO__ */
#define ASQ_RECOVERED_DATA_WITH_RETRIES _ASQ(0x17, 0x01)                       /* DT__WRSO__ */
#define ASQ_RECOVERED_DATA_WITH_POSITIVE_HEAD_OFFSET _ASQ(0x17, 0x02)          /* DT__WR_O__ */
#define ASQ_RECOVERED_DATA_WITH_NEGATIVE_HEAD_OFFSET _ASQ(0x17, 0x03)          /* DT__WR_O__ */
#define ASQ_RECOVERED_DATA_WITH_RETRIES_ANDOR_CIRC_APPLIED _ASQ(0x17, 0x04)    /* ____WR_O__ */
#define ASQ_RECOVERED_DATA_USING_PREVIOUS_SECTOR_ID _ASQ(0x17, 0x05)           /* D___WR_O__ */
#define ASQ_RECOVERED_DATA_WITHOUT_ECC_DATA_AUTO_REALLOCATED _ASQ(0x17, 0x06)  /* D___W__O__ */
#define ASQ_RECOVERED_DATA_WITHOUT_ECC_RECOMMEND_REASSIGNMENT _ASQ(0x17, 0x07) /* D___W__O__ */
#define ASQ_RECOVERED_DATA_WITHOUT_ECC_RECOMMEND_REWRITE _ASQ(0x17, 0x08)      /* D___W__O__ */
#define ASQ_RECOVERED_DATA_WITH_ERROR_CORRECTION_APPLIED _ASQ(0x18, 0x00)      /* DT__WR_O__ */
#define ASQ_RECOVERED_DATA_WITH_ERROR_CORRECTION_AND_RETRIES_APPLIED \
  _ASQ(0x18, 0x01)                                                 /* D___WR_O__ */
#define ASQ_RECOVERED_DATA_DATA_AUTO_REALLOCATED _ASQ(0x18, 0x02)  /* D___WR_O__ */
#define ASQ_RECOVERED_DATA_WITH_CIRC _ASQ(0x18, 0x03)              /* _____R____ */
#define ASQ_RECOVERED_DATA_WITH_LEC _ASQ(0x18, 0x04)               /* _____R____ */
#define ASQ_RECOVERED_DATA_RECOMMEND_REASSIGNMENT _ASQ(0x18, 0x05) /* D___WR_O__ */
#define ASQ_RECOVERED_DATA_RECOMMEND_REWRITE _ASQ(0x18, 0x06)      /* D___WR_O__ */
#define ASQ_DEFECT_LIST_ERROR _ASQ(0x19, 0x00)                     /* D______O__ */
#define ASQ_DEFECT_LIST_NOT_AVAILABLE _ASQ(0x19, 0x01)             /* D______O__ */
#define ASQ_DEFECT_LIST_ERROR_IN_PRIMARY_LIST                              \
  _ASQ(0x19, 0x02)                                           /* D______O__ \
                                                              */
#define ASQ_DEFECT_LIST_ERROR_IN_GROWN_LIST _ASQ(0x19, 0x03) /* D______O__ */
#define ASQ_PARAMETER_LIST_LENGTH_ERROR _ASQ(0x1A, 0x00)     /* DTLPWRSOMC */
#define ASQ_SYNCHRONOUS_DATA_TRANSFER_ERROR _ASQ(0x1B, 0x00) /* DTLPWRSOMC */
#define ASQ_DEFECT_LIST_NOT_FOUND _ASQ(0x1C, 0x00)           /* D______O__ */
#define ASQ_PRIMARY_DEFECT_LIST_NOT_FOUND _ASQ(0x1C, 0x01)   /* D______O__ */
#define ASQ_GROWN_DEFECT_LIST_NOT_FOUND _ASQ(0x1C, 0x02)     /* D______O__ */
#define ASQ_MISCOMPARE_DURING_VERIFY_OPERATION                            \
  _ASQ(0x1D, 0x00)                                          /* D___W__O__ \
                                                             */
#define ASQ_RECOVERED_ID_WITH_ECC _ASQ(0x1E, 0x00)          /* D___W__O__ */
#define ASQ_INVALID_COMMAND_OPERATION_CODE _ASQ(0x20, 0x00) /* DTLPWRSOMC */
#define ASQ_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE                             \
  _ASQ(0x21, 0x00)                                           /* DT__WR_OM_ \
                                                              */
#define ASQ_INVALID_ELEMENT_ADDRESS _ASQ(0x21, 0x01)         /* ________M_ */
#define ASQ_ILLEGAL_FUNCTION _ASQ(0x22, 0x00)                /* D_________ */
#define ASQ_INVALID_FIELD_IN_CDB _ASQ(0x24, 0x00)            /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_NOT_SUPPORTED _ASQ(0x25, 0x00)      /* DTLPWRSOMC */
#define ASQ_INVALID_FIELD_IN_PARAMETER_LIST _ASQ(0x26, 0x00) /* DTLPWRSOMC */
#define ASQ_PARAMETER_NOT_SUPPORTED _ASQ(0x26, 0x01)         /* DTLPWRSOMC */
#define ASQ_PARAMETER_VALUE_INVALID _ASQ(0x26, 0x02)         /* DTLPWRSOMC */
#define ASQ_THRESHOLD_PARAMETERS_NOT_SUPPORTED                           \
  _ASQ(0x26, 0x03)                                         /* DTLPWRSOMC \
                                                            */
#define ASQ_WRITE_PROTECTED _ASQ(0x27, 0x00)               /* DT__W__O__ */
#define ASQ_NOT_READY_TO_READY_TRANSITION _ASQ(0x28, 0x00) /* DTLPWRSOMC */
#define ASQ_MEDIUM_MAY_HAVE_CHANGED _ASQ(0x28, 0x00)       /* DTLPWRSOMC */
#define ASQ_IMPORT_OR_EXPORT_ELEMENT_ACCESSED                                               \
  _ASQ(0x28, 0x01)                                                            /* ________M_ \
                                                                               */
#define ASQ_POWER_ON_OR_RESET_OR_BUS_DEVICE_RESET_OCCURRED _ASQ(0x29, 0x00)   /* DTLPWRSOMC */
#define ASQ_PARAMETERS_CHANGED _ASQ(0x2A, 0x00)                               /* DTL_WRSOMC */
#define ASQ_MODE_PARAMETERS_CHANGED _ASQ(0x2A, 0x01)                          /* DTL_WRSOMC */
#define ASQ_LOG_PARAMETERS_CHANGED _ASQ(0x2A, 0x02)                           /* DTL_WRSOMC */
#define ASQ_COPY_CANNOT_EXECUTE_SINCE_HOST_CANNOT_DISCONNECT _ASQ(0x2B, 0x00) /* DTLPWRSO_C */
#define ASQ_COMMAND_SEQUENCE_ERROR _ASQ(0x2C, 0x00)                           /* DTLPWRSOMC */
#define ASQ_TOO_MANY_WINDOWS_SPECIFIED _ASQ(0x2C, 0x01)                       /* ______S___ */
#define ASQ_INVALID_COMBINATION_OF_WINDOWS_SPECIFIED _ASQ(0x2C, 0x02)         /* ______S___ */
#define ASQ_OVERWRITE_ERROR_ON_UPDATE_IN_PLACE                                   \
  _ASQ(0x2D, 0x00)                                                 /* _T________ \
                                                                    */
#define ASQ_COMMANDS_CLEARED_BY_ANOTHER_INITIATOR _ASQ(0x2F, 0x00) /* DTLPWRSOMC */
#define ASQ_INCOMPATIBLE_MEDIUM_INSTALLED _ASQ(0x30, 0x00)         /* DT__WR_OM_ */
#define ASQ_CANNOT_READ_MEDIUM_UNKNOWN_FORMAT                                     \
  _ASQ(0x30, 0x01)                                                  /* DT__WR_O__ \
                                                                     */
#define ASQ_CANNOT_READ_MEDIUM_INCOMPATIBLE_FORMAT _ASQ(0x30, 0x02) /* DT__WR_O__ */
#define ASQ_CLEANING_CARTRIDGE_INSTALLED _ASQ(0x30, 0x03)           /* DT________ */
#define ASQ_MEDIUM_FORMAT_CORRUPTED _ASQ(0x31, 0x00)                /* DT__W__O__ */
#define ASQ_FORMAT_COMMAND_FAILED _ASQ(0x31, 0x01)                  /* D_L____O__ */
#define ASQ_NO_DEFECT_SPARE_LOCATION_AVAILABLE                                              \
  _ASQ(0x32, 0x00)                                                            /* D___W__O__ \
                                                                               */
#define ASQ_DEFECT_LIST_UPDATE_FAILURE _ASQ(0x32, 0x01)                       /* D___W__O__ */
#define ASQ_TAPE_LENGTH_ERROR _ASQ(0x33, 0x00)                                /* _T________ */
#define ASQ_RIBBON_OR_INK_OR_TONER_FAILURE _ASQ(0x36, 0x00)                   /* __L_______ */
#define ASQ_ROUNDED_PARAMETER _ASQ(0x37, 0x00)                                /* DTL_WRSOMC */
#define ASQ_SAVING_PARAMETERS_NOT_SUPPORTED _ASQ(0x39, 0x00)                  /* DTL_WRSOMC */
#define ASQ_MEDIUM_NOT_PRESENT _ASQ(0x3A, 0x00)                               /* DTL_WRSOM_ */
#define ASQ_SEQUENTIAL_POSITIONING_ERROR _ASQ(0x3B, 0x00)                     /* _TL_______ */
#define ASQ_TAPE_POSITION_ERROR_AT_BEGINNING_OF_MEDIUM _ASQ(0x3B, 0x01)       /* _T________ */
#define ASQ_TAPE_POSITION_ERROR_AT_END_OF_MEDIUM _ASQ(0x3B, 0x02)             /* _T________ */
#define ASQ_TAPE_OR_ELECTRONIC_VERTICAL_FORMS_UNIT_NOT_READY _ASQ(0x3B, 0x03) /* __L_______ */
#define ASQ_SLEW_FAILURE _ASQ(0x3B, 0x04)                                     /* __L_______ */
#define ASQ_PAPER_JAM _ASQ(0x3B, 0x05)                                        /* __L_______ */
#define ASQ_FAILED_TO_SENSE_TOP_OF_FORM _ASQ(0x3B, 0x06)                      /* __L_______ */
#define ASQ_FAILED_TO_SENSE_BOTTOM_OF_FORM _ASQ(0x3B, 0x07)                   /* __L_______ */
#define ASQ_REPOSITION_ERROR _ASQ(0x3B, 0x08)                                 /* _T________ */
#define ASQ_READ_PAST_END_OF_MEDIUM _ASQ(0x3B, 0x09)                          /* ______S___ */
#define ASQ_READ_PAST_BEGINNING_OF_MEDIUM _ASQ(0x3B, 0x0A)                    /* ______S___ */
#define ASQ_POSITION_PAST_END_OF_MEDIUM _ASQ(0x3B, 0x0B)                      /* ______S___ */
#define ASQ_POSITION_PAST_BEGINNING_OF_MEDIUM                                       \
  _ASQ(0x3B, 0x0C)                                                    /* ______S___ \
                                                                       */
#define ASQ_MEDIUM_DESTINATION_ELEMENT_FULL _ASQ(0x3B, 0x0D)          /* ________M_ */
#define ASQ_MEDIUM_SOURCE_ELEMENT_EMPTY _ASQ(0x3B, 0x0E)              /* ________M_ */
#define ASQ_INVALID_BITS_IN_IDENTIFY_MESSAGE _ASQ(0x3D, 0x00)         /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_HAS_NOT_SELF_CONFIGURED_YET _ASQ(0x3E, 0x00) /* DTLPWRSOMC */
#define ASQ_TARGET_OPERATING_CONDITIONS_HAVE_CHANGED _ASQ(0x3F, 0x00) /* DTLPWRSOMC */
#define ASQ_MICROCODE_HAS_BEEN_CHANGED _ASQ(0x3F, 0x01)               /* DTLPWRSOMC */
#define ASQ_CHANGED_OPERATING_DEFINITION _ASQ(0x3F, 0x02)             /* DTLPWRSOMC */
#define ASQ_INQUIRY_DATA_HAS_CHANGED _ASQ(0x3F, 0x03)                 /* DTLPWRSOMC */
#define ASQ_RAM_FAILURE _ASQ(0x40, 0x00)                              /* D_________ */
#define ASQ_DIAGNOSTIC_FAILURE_ON_COMPONENT_00                                        \
  _ASQ(0x40, 0x00)                                                      /* DTLPWRSOMC \
                                                                         */
#define ASQ_DATA_PATH_FAILURE _ASQ(0x41, 0x00)                          /* D_________ */
#define ASQ_POWER_ON_OR_SELF_TEST_FAILURE _ASQ(0x42, 0x00)              /* D_________ */
#define ASQ_MESSAGE_ERROR _ASQ(0x43, 0x00)                              /* DTLPWRSOMC */
#define ASQ_INTERNAL_TARGET_FAILURE _ASQ(0x44, 0x00)                    /* DTLPWRSOMC */
#define ASQ_SELECT_OR_RESELECT_FAILURE _ASQ(0x45, 0x00)                 /* DTLPWRSOMC */
#define ASQ_UNSUCCESSFUL_SOFT_RESET _ASQ(0x46, 0x00)                    /* DTLPWRSOMC */
#define ASQ_SCSI_PARITY_ERROR _ASQ(0x47, 0x00)                          /* DTLPWRSOMC */
#define ASQ_INITIATOR_DETECTED_ERROR_MESSAGE_RECEIVED _ASQ(0x48, 0x00)  /* DTLPWRSOMC */
#define ASQ_INVALID_MESSAGE_ERROR _ASQ(0x49, 0x00)                      /* DTLPWRSOMC */
#define ASQ_COMMAND_PHASE_ERROR _ASQ(0x4A, 0x00)                        /* DTLPWRSOMC */
#define ASQ_DATA_PHASE_ERROR _ASQ(0x4B, 0x00)                           /* DTLPWRSOMC */
#define ASQ_LOGICAL_UNIT_FAILED_SELF_CONFIGURATION _ASQ(0x4C, 0x00)     /* DTLPWRSOMC */
#define ASQ_OVERLAPPED_COMMANDS_ATTEMPTED _ASQ(0x4E, 0x00)              /* DTLPWRSOMC */
#define ASQ_WRITE_APPEND_ERROR _ASQ(0x50, 0x00)                         /* _T________ */
#define ASQ_WRITE_APPEND_POSITION_ERROR _ASQ(0x50, 0x01)                /* _T________ */
#define ASQ_POSITION_ERROR_RELATED_TO_TIMING _ASQ(0x50, 0x02)           /* _T________ */
#define ASQ_ERASE_FAILURE _ASQ(0x51, 0x00)                              /* _T_____O__ */
#define ASQ_CARTRIDGE_FAULT _ASQ(0x52, 0x00)                            /* _T________ */
#define ASQ_MEDIA_LOAD_OR_EJECT_FAILED _ASQ(0x53, 0x00)                 /* DTL_WRSOM_ */
#define ASQ_UNLOAD_TAPE_FAILURE _ASQ(0x53, 0x01)                        /* _T________ */
#define ASQ_MEDIUM_REMOVAL_PREVENTED _ASQ(0x53, 0x02)                   /* DT__WR_OM_ */
#define ASQ_SCSI_TO_HOST_SYSTEM_INTERFACE_FAILURE _ASQ(0x54, 0x00)      /* ___P______ */
#define ASQ_SYSTEM_RESOURCE_FAILURE _ASQ(0x55, 0x00)                    /* ___P______ */
#define ASQ_UNABLE_TO_RECOVER_TABLE_OF_CONTENTS _ASQ(0x57, 0x00)        /* _______R__ */
#define ASQ_GENERATION_DOES_NOT_EXIST _ASQ(0x58, 0x00)                  /* __O_______ */
#define ASQ_UPDATED_BLOCK_READ _ASQ(0x59, 0x00)                         /* __O_______ */
#define ASQ_OPERATOR_REQUEST_OR_STATE_CHANGE_INPUT _ASQ(0x5A, 0x00)     /* DTLPWRSOM_ */
#define ASQ_OPERATOR_MEDIUM_REMOVAL_REQUEST _ASQ(0x5A, 0x01)            /* DT__WR_OM_ */
#define ASQ_OPERATOR_SELECTED_WRITE_PROTECT _ASQ(0x5A, 0x02)            /* DT__W__O__ */
#define ASQ_OPERATOR_SELECTED_WRITE_PERMIT _ASQ(0x5A, 0x03)             /* DT__W__O__ */
#define ASQ_LOG_EXCEPTION _ASQ(0x5B, 0x00)                              /* DTLPWRSOM_ */
#define ASQ_THRESHOLD_CONDITION_MET _ASQ(0x5B, 0x01)                    /* DTLPWRSOM_ */
#define ASQ_LOG_COUNTER_AT_MAXIMUM _ASQ(0x5B, 0x02)                     /* DTLPWRSOM_ */
#define ASQ_LOG_LIST_CODES_EXHAUSTED _ASQ(0x5B, 0x03)                   /* DTLPWRSOM_ */
#define ASQ_RPL_STATUS_CHANGE _ASQ(0x5C, 0x00)                          /* D___O_____ */
#define ASQ_SPINDLES_SYNCHRONIZED _ASQ(0x5C, 0x01)                      /* D___O_____ */
#define ASQ_SPINDLES_NOT_SYNCHRONIZED _ASQ(0x5C, 0x02)                  /* D___O_____ */
#define ASQ_LAMP_FAILURE _ASQ(0x60, 0x00)                               /* ______S___ */
#define ASQ_VIDEO_ACQUISITION_ERROR _ASQ(0x61, 0x00)                    /* ______S___ */
#define ASQ_UNABLE_TO_ACQUIRE_VIDEO _ASQ(0x61, 0x01)                    /* ______S___ */
#define ASQ_OUT_OF_FOCUS _ASQ(0x61, 0x02)                               /* ______S___ */
#define ASQ_SCAN_HEAD_POSITIONING_ERROR _ASQ(0x62, 0x00)                /* ______S___ */
#define ASQ_END_OF_USER_AREA_ENCOUNTERED_ON_THIS_TRACK _ASQ(0x63, 0x00) /* _____R____ */
#define ASQ_ILLEGAL_MODE_FOR_THIS_TRACK _ASQ(0x64, 0x00)                /* _____R____ */

/* clang-format off */
