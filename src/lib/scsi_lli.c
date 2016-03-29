/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/*
 * Low level SCSI interface.
 *
 * Marco van Wieringen, March 2012
 */

#include "bareos.h"

/* Forward referenced functions */

#ifdef HAVE_LOWLEVEL_SCSI_INTERFACE

#include "scsi_lli.h"

#if defined(HAVE_LINUX_OS)

#include <scsi/sg.h>
#include <scsi/scsi.h>

#ifndef SIOC_REQSENSE
#define SIOC_REQSENSE _IOR ('C',0x02,SCSI_PAGE_SENSE)
#endif

/*
 * Core interface function to lowlevel SCSI interface.
 */
static inline bool do_scsi_cmd_page(int fd, const char *device_name,
                                    void *cdb, unsigned int cdb_len,
                                    void *cmd_page, unsigned int cmd_page_len,
                                    int direction)
{
   int rc;
   sg_io_hdr_t io_hdr;
   SCSI_PAGE_SENSE sense;
   bool opened_device = false;
   bool retval = false;

   /*
    * See if we need to open the device_name or if we got an open filedescriptor.
    */
   if (fd == -1) {
      fd = open(device_name, O_RDWR | O_NONBLOCK | O_BINARY);
      if (fd < 0) {
         berrno be;

         Emsg2(M_ERROR, 0, _("Failed to open %s: ERR=%s\n"),
               device_name, be.bstrerror());
         return false;
      }
      opened_device = true;
   }

   memset(&sense, 0, sizeof(sense));
   memset(&io_hdr, 0, sizeof(io_hdr));
   io_hdr.interface_id = 'S';
   io_hdr.cmd_len = cdb_len;
   io_hdr.mx_sb_len = sizeof(sense);
   io_hdr.dxfer_direction = direction;
   io_hdr.dxfer_len = cmd_page_len;
   io_hdr.dxferp = (char *)cmd_page;
   io_hdr.cmdp = (unsigned char *)cdb;
   io_hdr.sbp = (unsigned char *)&sense;

   rc = ioctl(fd, SG_IO, &io_hdr);
   if (rc <  0) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Unable to perform SG_IO ioctl on fd %d: ERR=%s\n"),
            fd, be.bstrerror());
      goto bail_out;
   }

   if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
      Emsg3(M_ERROR, 0, _("Failed with info 0x%02x mask status 0x%02x msg status 0x%02x\n"),
            io_hdr.info, io_hdr.masked_status, io_hdr.msg_status);
      Emsg2(M_ERROR, 0, _("     host status 0x%02x driver status 0x%02x\n"),
            io_hdr.host_status, io_hdr.driver_status );
      goto bail_out;
   }

   retval = true;

bail_out:
   /*
    * See if we opened the device in this function, if so close it.
    */
   if (opened_device) {
      close(fd);
   }

   return retval;
}

/*
 * Receive a lowlevel SCSI cmd from a SCSI device using the Linux SG_IO ioctl interface.
 */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, SG_DXFER_FROM_DEV);
}

/*
 * Send a lowlevel SCSI cmd to a SCSI device using the Linux SG_IO ioctl interface.
 */
bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, SG_DXFER_TO_DEV);
}

/*
 * Check if the SCSI sense is EOD.
 */
bool check_scsi_at_eod(int fd)
{
   int rc;
   SCSI_PAGE_SENSE sense;

   memset(&sense, 0, sizeof(sense));

   rc = ioctl(fd, SIOC_REQSENSE, &sense);
   if (rc != 0) {
      return false;
   }

   return sense.senseKey == 0x08 && /* BLANK CHECK. */
          sense.addSenseCode == 0x00 && /* End of data (Combination of asc and ascq)*/
          sense.addSenseCodeQual == 0x05;
}

#elif defined(HAVE_SUN_OS)

#include <sys/scsi/impl/uscsi.h>

#ifndef LOBYTE
#define LOBYTE(_w)    ((_w) & 0xff)
#endif

/*
 * Core interface function to lowlevel SCSI interface.
 */
static inline bool do_scsi_cmd_page(int fd, const char *device_name,
                                    void *cdb, unsigned int cdb_len,
                                    void *cmd_page, unsigned int cmd_page_len,
                                    int flags)
{
   int rc;
   struct uscsi_cmd my_cmd;
   SCSI_PAGE_SENSE sense;
   bool opened_device = false;
   bool retval = false;

   /*
    * See if we need to open the device_name or if we got an open filedescriptor.
    */
   if (fd == -1) {
      fd = open(device_name, O_RDWR | O_NONBLOCK | O_BINARY);
      if (fd < 0) {
         berrno be;

         Emsg2(M_ERROR, 0, _("Failed to open %s: ERR=%s\n"),
               device_name, be.bstrerror());
         return false;
      }
      opened_device = true;
   }

   memset(&sense, 0, sizeof(sense));

   my_cmd.uscsi_flags = flags;
   my_cmd.uscsi_timeout = 15;   /* Allow 15 seconds for completion */
   my_cmd.uscsi_cdb = (char *)cdb;
   my_cmd.uscsi_cdblen = cdb_len;
   my_cmd.uscsi_bufaddr = (char *)cmd_page;
   my_cmd.uscsi_buflen = cmd_page_len;
   my_cmd.uscsi_rqlen = sizeof(sense);
   my_cmd.uscsi_rqbuf = (char *)&sense;

   rc = ioctl(fd, USCSICMD, &my_cmd);
   if (rc != 0) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Unable to perform USCSICMD ioctl on fd %d: ERR=%s\n"),
            fd, be.bstrerror());
      Emsg3(M_ERROR, 0, _("Sense Key: %0.2X ASC: %0.2X ASCQ: %0.2X\n"),
            LOBYTE(sense.senseKey), sense.addSenseCode, sense.addSenseCodeQual);
      goto bail_out;
   }

   retval = true;

bail_out:
   /*
    * See if we opened the device in this function, if so close it.
    */
   if (opened_device) {
      close(fd);
   }
   return retval;
}

/*
 * Receive a lowlevel SCSI cmd from a SCSI device using the Solaris USCSI ioctl interface.
 */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len,
                          (USCSI_READ | USCSI_SILENT | USCSI_RQENABLE));
}

/*
 * Send a lowlevel SCSI cmd to a SCSI device using the Solaris USCSI ioctl interface.
 */
bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len,
                          (USCSI_WRITE | USCSI_SILENT | USCSI_RQENABLE));
}

bool check_scsi_at_eod(int fd)
{
   return false;
}

#elif defined(HAVE_FREEBSD_OS)

#include <camlib.h>
#include <cam/scsi/scsi_message.h>

#ifndef SAM_STAT_CHECK_CONDITION
#define SAM_STAT_CHECK_CONDITION 0x2
#endif

#ifndef SAM_STAT_COMMAND_TERMINATED
#define SAM_STAT_COMMAND_TERMINATED 0x22
#endif

/*
 * Core interface function to lowlevel SCSI interface.
 */
static inline bool do_scsi_cmd_page(int fd, const char *device_name,
                                    void *cdb, unsigned int cdb_len,
                                    void *cmd_page, unsigned int cmd_page_len,
                                    int direction)
{
   int unitnum, len;
   union ccb *ccb;
   char errbuf[128];
   char cam_devicename[64];
   struct cam_device *cam_dev;
   SCSI_PAGE_SENSE sense;
   bool retval = false;

   /*
    * See what CAM device to use.
    */
   if (cam_get_device(device_name, cam_devicename, sizeof(cam_devicename), &unitnum) == -1) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Failed to find CAM device for %s: ERR=%s\n"),
            device_name, be.bstrerror());
      return false;
   }

   cam_dev = cam_open_spec_device(cam_devicename, unitnum, O_RDWR, NULL);
   if (!cam_dev) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Failed to open CAM device for %s: ERR=%s\n"),
            device_name, be.bstrerror());
      return false;
   }

   ccb = cam_getccb(cam_dev);
   if (!ccb) {
      Emsg1(M_ERROR, 0, _("Failed to allocate new ccb for %s\n"),
            device_name);
      goto bail_out;
   }

   /*
    * Clear out structure, except for header that was filled for us.
    */
   memset(&(&ccb->ccb_h)[1], 0, sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

   cam_fill_csio(&ccb->csio,
                 1, /* retries */
                 NULL, /* cbfcnp */
                 direction, /* flags */
                 MSG_SIMPLE_Q_TAG, /* tagaction */
                 (u_int8_t *)cmd_page, /* dataptr */
                 cmd_page_len, /* datalen */
                 sizeof(sense), /* senselength */
                 cdb_len, /* cdblength  */
                 15000 /* timeout (millisecs) */);
   memcpy(ccb->csio.cdb_io.cdb_bytes, cdb, cdb_len);

   if (cam_send_ccb(cam_dev, ccb) < 0) {
      Emsg2(M_ERROR, 0, _("Failed to send ccb to device %s: %s\n"),
            device_name, cam_error_string(cam_dev, ccb, errbuf, sizeof(errbuf),
                                          CAM_ESF_ALL, CAM_EPF_ALL));
      cam_freeccb(ccb);
      goto bail_out;
   }

   /*
    * Retrieve the SCSI sense data.
    */
   if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) ||
       ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR)) {
      if ((SAM_STAT_CHECK_CONDITION == ccb->csio.scsi_status) ||
          (SAM_STAT_COMMAND_TERMINATED == ccb->csio.scsi_status)) {
         len = sizeof(sense) - ccb->csio.sense_resid;
         if (len) {
            memcpy(&sense, &(ccb->csio.sense_data), len);
         }
      }
   }

   retval = true;

bail_out:
   /*
    * Close the CAM device.
    */
   cam_close_device(cam_dev);
   return retval;
}

/*
 * Receive a lowlevel SCSI cmd from a SCSI device using the FreeBSD SCSI CAM interface.
 */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, CAM_DIR_IN);
}

/*
 * Send a lowlevel SCSI cmd to a SCSI device using the FreeBSD SCSI CAM interface.
 */
bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, CAM_DIR_OUT);
}

bool check_scsi_at_eod(int fd)
{
   return false;
}

#elif defined(HAVE_NETBSD_OS) || defined(HAVE_OPENBSD_OS)

#if defined(HAVE_NETBSD_OS)
#include <dev/scsipi/scsipi_all.h>
#else
#include <scsi/uscsi_all.h>
#endif

#ifndef LOBYTE
#define LOBYTE(_w)    ((_w) & 0xff)
#endif

/*
 * Core interface function to lowlevel SCSI interface.
 */
static inline bool do_scsi_cmd_page(int fd, const char *device_name,
                                    void *cdb, unsigned int cdb_len,
                                    void *cmd_page, unsigned int cmd_page_len,
                                    int flags)
{
   int rc;
   scsireq_t req;
   SCSI_PAGE_SENSE *sense;
   bool opened_device = false;
   bool retval = false;

   /*
    * See if we need to open the device_name or if we got an open filedescriptor.
    */
   if (fd == -1) {
      fd = open(device_name, O_RDWR | O_NONBLOCK| O_BINARY);
      if (fd < 0) {
         berrno be;

         Emsg2(M_ERROR, 0, _("Failed to open %s: ERR=%s\n"),
               device_name, be.bstrerror());
         return false;
      }
      opened_device = true;
   }

   memset(&req, 0, sizeof(req));

   memcpy(req.cmd, cdb, cdb_len);
   req.cmdlen = cdb_len;
   req.databuf = cmd_page;
   req.datalen = cmd_page_len;
   req.timeout = 15000 /* timeout (millisecs) */;
   req.flags = flags;
   req.senselen = sizeof(SCSI_PAGE_SENSE);

   rc = ioctl(fd, SCIOCCOMMAND, &req);
   if (rc <  0) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Unable to perform SCIOCCOMMAND ioctl on fd %d: ERR=%s\n"),
            fd, be.bstrerror());
      goto bail_out;
   }

   switch (req.retsts) {
   case SCCMD_OK:
      retval = true;
      break;
   case SCCMD_SENSE:
      sense = req.sense;
      Emsg3(M_ERROR, 0, _("Sense Key: %0.2X ASC: %0.2X ASCQ: %0.2X\n"),
            LOBYTE(sense.senseKey), sense.addSenseCode, sense.addSenseCodeQual);
      break;
   case SCCMD_TIMEOUT:
      Emsg1(M_ERROR, 0, _("SCIOCCOMMAND ioctl on %s returned SCSI command timed out\n"),
            devicename);
      break;
   case SCCMD_BUSY:
      Emsg1(M_ERROR, 0, _("SCIOCCOMMAND ioctl on %s returned device is busy\n"),
            devicename);
      break;
   case SCCMD_SENSE:
      break;
   default:
      Emsg2(M_ERROR, 0, _("SCIOCCOMMAND ioctl on %s returned unknown status %d\n"),
            device_name, req.retsts);
      break;
   }

bail_out:
   /*
    * See if we opened the device in this function, if so close it.
    */
   if (opened_device) {
      close(fd);
   }
   return retval;
}

/*
 * Receive a lowlevel SCSI cmd from a SCSI device using the NetBSD/OpenBSD
 * SCIOCCOMMAND ioctl interface.
 */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, SCCMD_READ);
}

/*
 * Send a lowlevel SCSI cmd to a SCSI device using the NetBSD/OpenBSD
 * SCIOCCOMMAND ioctl interface.
 */
bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return do_scsi_cmd_page(fd, device_name, cdb, cdb_len, cmd_page, cmd_page_len, SCCMD_WRITE);
}

bool check_scsi_at_eod(int fd)
{
   return false;
}
#endif
#else
/*
 * Dummy lowlevel functions when no support for platform.
 */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return false;
}

bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len)
{
   return false;
}

bool check_scsi_at_eod(int fd)
{
   return false;
}
#endif /* HAVE_LOWLEVEL_SCSI_INTERFACE */
