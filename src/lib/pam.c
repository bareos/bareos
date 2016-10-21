#include "config.h"
#include "pam.h"

#if defined(HAVE_PAM)

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#endif

#include <string.h>
#include <stdlib.h>

int
pam_functor::cb(int num_msg, PAM_CONST struct pam_message **msg,
                struct pam_response **resp) const {
   if (!msg || !*msg || !resp)
      return PAM_BUF_ERR;
   switch ((*msg)->msg_style) {
   case PAM_PROMPT_ECHO_OFF:
      return send(resp, psk) ? PAM_SUCCESS : PAM_CONV_ERR;
   case PAM_PROMPT_ECHO_ON:
      return send(resp, user) ? PAM_SUCCESS : PAM_CONV_ERR;
   case PAM_ERROR_MSG:
   case PAM_TEXT_INFO:
   default:
      return PAM_SUCCESS;
   }
}

bool
pam_functor::send(struct pam_response **r, const char *data) const {
   struct pam_response *resp = (struct pam_response *)
      calloc(1, sizeof(struct pam_response));
   if (!resp)
      return false;
   resp->resp = strdup(data);
   resp->resp_retcode = resp->resp ? PAM_SUCCESS : PAM_BUF_ERR;
   *r = resp;
   return true;
}

#endif /* HAVE_PAM */
