#include "bareos.h"
#include "pam.h"

#if defined(HAVE_PAM)

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#endif

#define DEFAULT_PAM_SERVICE_NAME "bareos"

static const int dbglvl = 50;

/*
 * See if the username has groupname as primary or secondary group.
 */
static inline bool user_is_in_group(const char *username, const char *groupname)
{
   struct passwd *pw_entry;
   struct group *gr_entry;

   pw_entry = getpwnam(username);
   if (!pw_entry) {
      return false;
   }

   gr_entry = getgrnam(groupname);
   if (!gr_entry) {
      return false;
   }

   /*
    * See if the primary group id matches.
    */
   if (pw_entry->pw_gid == gr_entry->gr_gid) {
      return true;
   }

   for (int i = 0; gr_entry->gr_mem[i]; i++) {
      if (bstrcasecmp(username, gr_entry->gr_mem[i])) {
         return true;
      }
   }

   return false;
}

static inline bool do_check_acl(const char *user,
                                alist *allowed_users,
                                alist *allowed_groups)
{
   /*
    * See if the user is a valid username in the list of allowed users.
    */
   if (allowed_users) {
      const char *allowed_user;

      foreach_alist(allowed_user, (alist *)allowed_users) {
         if (bstrcasecmp(allowed_user, user)) {
            return true;
         }
      }
   }

   /*
    * See if the user has either one of the allowed groups as primary or secondary group.
    */
   if (allowed_groups) {
      const char *allowed_group;

      foreach_alist(allowed_group, (alist *)allowed_groups) {
         if (user_is_in_group(user, allowed_group)) {
            return true;
         }
      }
   }

   return false;
}

bool
pam_authenticate_useragent(const char *user, const char *psk,
                           alist *allowed_users,
                           alist *allowed_groups,
                           bool check_acl,
                           const char *host)
{
   int auth_ok = 0;
   int rc = PAM_SUCCESS;

   pam_functor fnc(user, psk);
   pam_handle_t *ph = NULL;

   struct pam_conv conv = {
      (int (*)(int, const struct pam_message **,
               struct pam_response **, void *))&pam_functor::conv_cb,
      (void *)&fnc
   };

   if (check_acl && !allowed_users && !allowed_groups) {
      goto fail;
   }

   rc = pam_start(DEFAULT_PAM_SERVICE_NAME,
                  user, &conv, &ph);
   if (rc != PAM_SUCCESS) {
      Dmsg1(100, "Failed to pam_start(): ERR=%s\n", pam_strerror(ph, rc));
      goto fail;
   }

   rc = pam_set_item(ph, PAM_RHOST, host);
   if (rc != PAM_SUCCESS) {
      Dmsg1(100, "Failed to pam_set_item() PAM_RHOST: ERR=%s\n", pam_strerror(ph, rc));
      goto fail;
   }

   rc = pam_authenticate(ph, 0);
   if (rc != PAM_SUCCESS) {
      Dmsg1(100, "Failed to pam_authenticate(): ERR=%s\n", pam_strerror(ph, rc));
      goto fail;
   }

   rc = pam_acct_mgmt(ph, 0);
   switch (rc) {
   case PAM_SUCCESS:
      break;
   case PAM_USER_UNKNOWN:
      Dmsg1(100, "%s Unknown Account\n", user);
      goto fail;
   case PAM_ACCT_EXPIRED:
      Dmsg1(100, "%s Account expired\n", user);
      goto fail;
   default:
      Dmsg1(100, "Failed to pam_acct_mgmt(): ERR=%s\n", pam_strerror(ph, rc));
      goto fail;
   }

   if (check_acl && !do_check_acl(user, allowed_users, allowed_groups)) {
      goto fail;
   }

   auth_ok = 1;

fail:
   if (ph)
      pam_end(ph, rc);
   return auth_ok;
}

#else /* !HAVE_PAM */

bool
pam_authenticate_useragent(const char *user, const char *psk,
                           const alist *allowed_users,
                           const alist *allowed_groups,
                           bool check_acl,
                           const char *host)
{
   return false;
}

#endif
