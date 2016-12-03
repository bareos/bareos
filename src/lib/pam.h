#ifndef __pam_auth__h__
#define __pam_auth__h__

#include "config.h"

#if defined(HAVE_PAM)

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#endif

#if defined(HAVE_LINUX_OS)
#define PAM_CONST const
#else
#define PAM_CONST
#endif

class pam_functor {
public:
   pam_functor(const char *user__, const char *psk__);
   static int conv_cb(int num_msg, PAM_CONST struct pam_message **msg,
                      struct pam_response **resp, pam_functor *);
private:
   int cb(int num_msg, const struct pam_message **msg,
          struct pam_response **resp) const;
   bool send(struct pam_response **resp, const char *data) const;

   const char *user;
   const char *psk;
};

inline
pam_functor::pam_functor(const char *user__, const char *psk__)
   : user(user__)
   , psk(psk__)
{
}

inline int
pam_functor::conv_cb(int num_msg, const struct pam_message **msg,
                     struct pam_response **resp, pam_functor *fnc) {
   return fnc->cb(num_msg, msg, resp);
}

#endif /* HAVE_PAM */

#endif
