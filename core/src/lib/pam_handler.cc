//
// Created by torsten on 17.04.18.
//
#include "pam_handler.h"

#include "bareos.h"
#include <cstring>
#include <security/pam_appl.h>

static const int debuglevel = 200;

static const std::string service_name("bareos");

class PamData {
public:
   std::string password_;
   std::string username_;

   PamData(std::string username, std::string password) {
      username_ = username;
      password_ = password;
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
   auto pam_data = reinterpret_cast<PamData *>(appdata_ptr);

   if ((resp = static_cast<pam_response *>(actuallycalloc(num_msg, sizeof(struct pam_response)))) == nullptr) {
      return PAM_BUF_ERR;
   }

   switch ((*msgm)->msg_style) {
      case PAM_PROMPT_ECHO_OFF:
      case PAM_PROMPT_ECHO_ON: {
         resp->resp = actuallystrdup(pam_data->password_.c_str());
         break;
      }
      case PAM_ERROR_MSG:
      case PAM_TEXT_INFO:break;
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
      if (resp[i].resp != NULL) {
         memset(resp[i].resp, 0, strlen(resp[i].resp));
         free(resp[i].resp);
      }
   }
   memset(resp, 0, num_msg * sizeof *resp);
   free(resp);
   *response = NULL;
   return PAM_CONV_ERR;
}

bool pam_authenticate_useragent(std::string username, std::string password) {
   PamData pam_data(username, password);
   const struct pam_conv pam_conversation = {conv, (void *) &pam_data};
   pam_handle_t *pamh = nullptr;

   /* START */
   int err = pam_start(service_name.c_str(), username.c_str(), &pam_conversation, &pamh);
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM start failed: %s\n", pam_strerror(pamh, err));
   }

   err = pam_set_item(pamh, PAM_RUSER, username.c_str());
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM set_item failed: %s\n", pam_strerror(pamh, err));
   }

   /* AUTHENTICATE */
   err = pam_authenticate(pamh, 0);
   if (err != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM authentication failed: %s\n", pam_strerror(pamh, err));
   }

   /* END */
   if (pam_end(pamh, err) != PAM_SUCCESS) {
      Dmsg1(debuglevel, "PAM end failed: %s\n", pam_strerror(pamh, err));
      return false;
   }

   return err == 0;
}
