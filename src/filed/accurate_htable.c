/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, December 2013
 */
/**
 * @file
 * This file contains the HTABLE abstraction of the accurate payload storage.
 */

#include "bareos.h"
#include "filed.h"
#include "accurate.h"

static int dbglvl = 100;

B_ACCURATE_HTABLE::B_ACCURATE_HTABLE()
{
   m_filenr = 0;
   m_file_list = NULL;
   m_seen_bitmap = NULL;
}

B_ACCURATE_HTABLE::~B_ACCURATE_HTABLE()
{
}

bool B_ACCURATE_HTABLE::init(JCR *jcr, uint32_t nbfile)
{
   CurFile *elt = NULL;

   if (!m_file_list) {
      m_file_list = (htable *)malloc(sizeof(htable));
      m_file_list->init(elt, &elt->link, nbfile);
   }

   if (!m_seen_bitmap) {
      m_seen_bitmap = (char *)malloc(nbytes_for_bits(nbfile));
      clear_all_bits(nbfile, m_seen_bitmap);
   }

   return true;
}

bool B_ACCURATE_HTABLE::add_file(JCR *jcr,
                                 char *fname,
                                 int fname_length,
                                 char *lstat,
                                 int lstat_length,
                                 char *chksum,
                                 int chksum_length,
                                 int32_t delta_seq)
{
   CurFile *item;
   int total_length;
   bool retval = true;

   total_length = sizeof(CurFile) + fname_length + lstat_length + chksum_length + 3;
   item = (CurFile *)m_file_list->hash_malloc(total_length);

   item->fname = (char *)item + sizeof(CurFile);
   memcpy(item->fname, fname, fname_length);
   item->fname[fname_length] = '\0';

   item->payload.lstat = item->fname + fname_length + 1;
   memcpy(item->payload.lstat, lstat, lstat_length);
   item->payload.lstat[lstat_length] = '\0';

   item->payload.chksum = item->payload.lstat + lstat_length + 1;
   if (chksum_length) {
      memcpy(item->payload.chksum, chksum, chksum_length);
   }
   item->payload.chksum[chksum_length] = '\0';

   item->payload.delta_seq = delta_seq;
   item->payload.filenr = m_filenr++;
   m_file_list->insert(item->fname, item);

   if (chksum) {
      Dmsg4(dbglvl, "add fname=<%s> lstat=%s delta_seq=%i chksum=%s\n", fname, lstat, delta_seq, chksum);
   } else {
      Dmsg2(dbglvl, "add fname=<%s> lstat=%s\n", fname, lstat);
   }

   return retval;
}

bool B_ACCURATE_HTABLE::end_load(JCR *jcr)
{
   /*
    * Nothing to do.
    */
   return true;
}

accurate_payload *B_ACCURATE_HTABLE::lookup_payload(JCR *jcr, char *fname)
{
   CurFile *temp;

   temp = (CurFile *)m_file_list->lookup(fname);
   return (temp) ? &temp->payload : NULL;
}

bool B_ACCURATE_HTABLE::update_payload(JCR *jcr, char *fname, accurate_payload *payload)
{
   /*
    * Nothing to do.
    */
   return true;
}

bool B_ACCURATE_HTABLE::send_base_file_list(JCR *jcr)
{
   CurFile *elt;
   FF_PKT *ff_pkt;
   int32_t LinkFIc;
   struct stat statp;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->getJobLevel() != L_FULL) {
      return true;
   }

   if (m_file_list == NULL) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_BASE;

   foreach_htable(elt, m_file_list) {
      if (bit_is_set(elt->payload.filenr, m_seen_bitmap)) {
         Dmsg1(dbglvl, "base file fname=%s\n", elt->fname);
         decode_stat(elt->payload.lstat, &statp, sizeof(statp), &LinkFIc); /* decode catalog stat */
         ff_pkt->fname = elt->fname;
         ff_pkt->statp = statp;
         encode_and_send_attributes(jcr, ff_pkt, stream);
      }
   }

   term_find_files(ff_pkt);
   return true;
}

bool B_ACCURATE_HTABLE::send_deleted_list(JCR *jcr)
{
   CurFile *elt;
   FF_PKT *ff_pkt;
   int32_t LinkFIc;
   struct stat statp;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   foreach_htable(elt, m_file_list) {
      if (bit_is_set(elt->payload.filenr, m_seen_bitmap) ||
          plugin_check_file(jcr, elt->fname)) {
         continue;
      }
      Dmsg1(dbglvl, "deleted fname=%s\n", elt->fname);
      ff_pkt->fname = elt->fname;
      decode_stat(elt->payload.lstat, &statp, sizeof(statp), &LinkFIc); /* decode catalog stat */
      ff_pkt->statp.st_mtime = statp.st_mtime;
      ff_pkt->statp.st_ctime = statp.st_ctime;
      encode_and_send_attributes(jcr, ff_pkt, stream);
   }

   term_find_files(ff_pkt);
   return true;
}

void B_ACCURATE_HTABLE::destroy(JCR *jcr)
{
   if (m_file_list) {
      m_file_list->destroy();
      free(m_file_list);
      m_file_list = NULL;
   }

   if (m_seen_bitmap) {
      free(m_seen_bitmap);
      m_seen_bitmap = NULL;
   }

   m_filenr = 0;
}
