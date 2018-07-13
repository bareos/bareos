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

#include <cstring>
#include <security/pam_appl.h>

#include "bareos.h"
#include "ua.h"

static const int debuglevel = 200;

static const std::string service_name("bareos");

struct PamData {
   std::string username_;
   BareosSocket *UA_sock_;

   PamData(BareosSocket *UA_sock, std::string username) {
      UA_sock_ = UA_sock;
      username_ = username;
   }
};

/*
 * PAM-Callback called by Bareos PAM-Handler
 *
 */
static bool PamConvSendMessage(BareosSocket *UA_sock, const char *msg, int msg_style)
{
   char buf = msg_style;
   if (!UA_sock->send((const char*)&buf, 1)) {
      Dmsg0(debuglevel, "PamConvSendMessage error\n");
      return false;
   }
   if (!UA_sock->send(msg, strlen(msg) +1)) {
      Dmsg0(debuglevel, "PamConvSendMessage error\n");
      return false;
   }
   return true;
}

static int PamConversionCallback(int num_msg, const struct pam_message **msgm,
                struct pam_response **response, void *appdata_ptr)
{
   if (!appdata_ptr) {
      Dmsg0(debuglevel, "pam_conv_callback pointer error\n");
      return PAM_BUF_ERR;
   }

   if ((num_msg <= 0) || (num_msg > PAM_MAX_NUM_MSG)) {
      Dmsg0(debuglevel, "pam_conv_callback wrong number of messages\n");
      return (PAM_CONV_ERR);
   }

   struct pam_response *resp =
         reinterpret_cast<pam_response *> (actuallycalloc(
               num_msg, sizeof(struct pam_response)));

   if (!resp) {
      Dmsg0(debuglevel, "pam_conv_callback memory error\n");
      return PAM_BUF_ERR;
   }

   PamData *pam_data = reinterpret_cast<PamData *>(appdata_ptr);

   bool error = false;
   int i = 0;
   for ( ; i < num_msg && !error; i++) {
      switch (msgm[i]->msg_style) {
         case PAM_PROMPT_ECHO_OFF:
         case PAM_PROMPT_ECHO_ON:
               if (!PamConvSendMessage(pam_data->UA_sock_,
                     msgm[i]->msg, msgm[i]->msg_style)) {
               error = true;
               break;
            }
            if (pam_data->UA_sock_->IsStop() || pam_data->UA_sock_->IsError()) {
               error = true;
               break;
            }
            if (pam_data->UA_sock_->recv()) {
               resp[i].resp = actuallystrdup(pam_data->UA_sock_->msg);
            }
            if (pam_data->UA_sock_->IsStop() || pam_data->UA_sock_->IsError()) {
               error = true;
               break;
            }
            break;
         case PAM_ERROR_MSG:
         case PAM_TEXT_INFO:
            if (!PamConvSendMessage(pam_data->UA_sock_,
                  msgm[i]->msg, PAM_PROMPT_ECHO_ON)) {
               error = true;
               break;
         }
         default:
            Dmsg3(debuglevel, "message[%d]: pam error type: %d error: \"%s\"\n",
                  1, msgm[i]->msg_style, msgm[i]->msg);
            error = true;
            break;
      } /* switch (msgm[i]->msg_style) { */
   } /* for( ; i < num_msg ..) */


   if (error) {
      for (int i = 0; i < num_msg; ++i) {
         if (resp[i].resp) {
            memset(resp[i].resp, 0, strlen(resp[i].resp));
            Actuallyfree(resp[i].resp);
         }
      }
      memset(resp, 0, num_msg * sizeof *resp);
      Actuallyfree(resp);
      *response = nullptr;
      return PAM_CONV_ERR;
   }

   *response = resp;
   return PAM_SUCCESS;
}

bool PamAuthenticateUseragent(BareosSocket *UA_sock, const std::string &username)
{
   std::unique_ptr<PamData> pam_callback_data(new PamData(UA_sock, username));
   std::unique_ptr<struct pam_conv> pam_conversation_container(new struct pam_conv);
   struct pam_handle *pamh; /* pam session handle */

   pam_conversation_container->conv = PamConversionCallback;
   pam_conversation_container->appdata_ptr = pam_callback_data.get();

   int err = pam_start(service_name.c_str(), username.c_str(),
                      pam_conversation_container.get(), &pamh);
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

   if (err == PAM_SUCCESS) {
      if (!PamConvSendMessage(UA_sock, "", PAM_SUCCESS)) {
         Dmsg1(debuglevel, "PAM end failed: %s\n", pam_strerror(pamh, err));
         return false;
      }
   }
   return err == PAM_SUCCESS;
}
