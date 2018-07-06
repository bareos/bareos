/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "auth_pam.h"

#include "bareos.h"
#include <cstring>
#include <security/pam_appl.h>

static const int debuglevel = 200;

static const std::string service_name("bareos");

struct PamData {
   std::string username_;
   BareosSocket *bs_;

   PamData(BareosSocket *bs, std::string username) {
      bs_ = bs;
      username_ = username;
   }
};

/// PAM-Callback calls Bareos PAM-Handler
static int conv(int num_msg, const struct pam_message **msgm,
                struct pam_response **response, void *appdata_ptr) {
   if (!num_msg || !*msgm || !response) {
      return PAM_BUF_ERR;
   }

   if ((num_msg <= 0) || (num_msg > PAM_MAX_NUM_MSG)) {
      return (PAM_CONV_ERR);
   }

   struct pam_response *resp;
   resp = static_cast<pam_response *>(actuallycalloc(num_msg, sizeof(struct pam_response)));
   if (resp == nullptr) {
      return PAM_BUF_ERR;
   }

   auto pam_data = reinterpret_cast<PamData *>(appdata_ptr);
   ASSERT(pam_data);

   switch ((*msgm)->msg_style) {
      case PAM_PROMPT_ECHO_OFF:
      case PAM_PROMPT_ECHO_ON: {
         BareosSocket *bs = pam_data->bs_;
         bs->fsend((*msgm)->msg);
         if (bs->recv()) {
            resp->resp = actuallystrdup(bs->msg);
         }
         break;
      }
      case PAM_ERROR_MSG:
      case PAM_TEXT_INFO: {
         BareosSocket *bs = pam_data->bs_;
         bs->fsend((*msgm)->msg);
         break;
      }
      default: {
         const pam_message *m = *msgm;
         Dmsg3(debuglevel, "message[%d]: pam error type: %d error: \"%s\"\n",
               1, m->msg_style, m->msg);
         goto err;
      }
   }

   *response = resp;
   return PAM_SUCCESS;

   err:
   for (int i = 0; i < num_msg; ++i) {
      if (resp[i].resp) {
         memset(resp[i].resp, 0, strlen(resp[i].resp));
         free(resp[i].resp);
      }
   }
   memset(resp, 0, num_msg * sizeof *resp);
   free(resp);
   *response = nullptr;
   return PAM_CONV_ERR;
}

bool pam_authenticate_useragent(BareosSocket *bs, std::string username) {
   PamData pam_data(bs, username);
   const struct pam_conv pam_conversation = {conv, (void *) &pam_data};
   pam_handle_t *pamh = nullptr;

   int err = pam_start(service_name.c_str(), nullptr, &pam_conversation, &pamh);
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM start failed: %s\n", pam_strerror(pamh, err));
   }

   err = pam_set_item(pamh, PAM_RUSER, username.c_str());
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM set_item failed: %s\n", pam_strerror(pamh, err));
   }

   err = pam_authenticate(pamh, 0);
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM authentication failed: %s\n", pam_strerror(pamh, err));
   }

   if (pam_end(pamh, err) != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM end failed: %s\n", pam_strerror(pamh, err));
      return false;
   }

   return err == 0;
}
