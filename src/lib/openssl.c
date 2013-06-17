/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.

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
 * openssl.c OpenSSL support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 */

#include "bareos.h"
#include <assert.h>

#ifdef HAVE_OPENSSL

#include <openssl/err.h>
#include <openssl/rand.h>

/* Array of mutexes for use with OpenSSL static locking */
static pthread_mutex_t *mutexes;

/* OpenSSL dynamic locking structure */
struct CRYPTO_dynlock_value {
   pthread_mutex_t mutex;
};

/*
 * ***FIXME*** this is a sort of dummy to avoid having to
 *   change all the existing code to pass either a jcr or
 *   a NULL.  Passing a NULL causes the messages to be
 *   printed by the daemon -- not very good :-(
 */
void openssl_post_errors(int code, const char *errstring)
{
   openssl_post_errors(NULL, code, errstring);
}


/*
 * Post all per-thread openssl errors
 */
void openssl_post_errors(JCR *jcr, int code, const char *errstring)
{
   char buf[512];
   unsigned long sslerr;

   /* Pop errors off of the per-thread queue */
   while((sslerr = ERR_get_error()) != 0) {
      /* Acquire the human readable string */
      ERR_error_string_n(sslerr, buf, sizeof(buf));
      Dmsg3(50, "jcr=%p %s: ERR=%s\n", jcr, errstring, buf);
      Qmsg2(jcr, M_ERROR, 0, "%s: ERR=%s\n", errstring, buf);
   }
}

/*
 * Return an OpenSSL thread ID
 *  Returns: thread ID
 *
 */
static unsigned long get_openssl_thread_id(void)
{
#ifdef HAVE_WIN32
   return (unsigned long)getpid();
#else
   /*
    * Comparison without use of pthread_equal() is mandated by the OpenSSL API
    *
    * Note: this creates problems with the new Win32 pthreads
    *   emulation code, which defines pthread_t as a structure.
    */
   return ((unsigned long)pthread_self());
#endif
}

/*
 * Allocate a dynamic OpenSSL mutex
 */
static struct CRYPTO_dynlock_value *openssl_create_dynamic_mutex (const char *file, int line)
{
   struct CRYPTO_dynlock_value *dynlock;
   int status;

   dynlock = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));

   if ((status = pthread_mutex_init(&dynlock->mutex, NULL)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to init mutex: ERR=%s\n"), be.bstrerror(status));
   }

   return dynlock;
}

static void openssl_update_dynamic_mutex(int mode, struct CRYPTO_dynlock_value *dynlock, const char *file, int line)
{
   if (mode & CRYPTO_LOCK) {
      P(dynlock->mutex);
   } else {
      V(dynlock->mutex);
   }
}

static void openssl_destroy_dynamic_mutex(struct CRYPTO_dynlock_value *dynlock, const char *file, int line)
{
   int status;

   if ((status = pthread_mutex_destroy(&dynlock->mutex)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to destroy mutex: ERR=%s\n"), be.bstrerror(status));
   }

   free(dynlock);
}

/*
 * (Un)Lock a static OpenSSL mutex
 */
static void openssl_update_static_mutex (int mode, int i, const char *file, int line)
{
   if (mode & CRYPTO_LOCK) {
      P(mutexes[i]);
   } else {
      V(mutexes[i]);
   }
}

/*
 * Initialize OpenSSL thread support
 *  Returns: 0 on success
 *           errno on failure
 */
int openssl_init_threads (void)
{
   int i, numlocks;
   int status;


   /* Set thread ID callback */
   CRYPTO_set_id_callback(get_openssl_thread_id);

   /* Initialize static locking */
   numlocks = CRYPTO_num_locks();
   mutexes = (pthread_mutex_t *) malloc(numlocks * sizeof(pthread_mutex_t));
   for (i = 0; i < numlocks; i++) {
      if ((status = pthread_mutex_init(&mutexes[i], NULL)) != 0) {
         berrno be;
         Jmsg1(NULL, M_FATAL, 0, _("Unable to init mutex: ERR=%s\n"), be.bstrerror(status));
         return status;
      }
   }

   /* Set static locking callback */
   CRYPTO_set_locking_callback(openssl_update_static_mutex);

   /* Initialize dyanmic locking */
   CRYPTO_set_dynlock_create_callback(openssl_create_dynamic_mutex);
   CRYPTO_set_dynlock_lock_callback(openssl_update_dynamic_mutex);
   CRYPTO_set_dynlock_destroy_callback(openssl_destroy_dynamic_mutex);

   return 0;
}

/*
 * Clean up OpenSSL threading support
 */
void openssl_cleanup_threads(void)
{
   int i, numlocks;
   int status;

   /* Unset thread ID callback */
   CRYPTO_set_id_callback(NULL);

   /* Deallocate static lock mutexes */
   numlocks = CRYPTO_num_locks();
   for (i = 0; i < numlocks; i++) {
      if ((status = pthread_mutex_destroy(&mutexes[i])) != 0) {
         berrno be;

         switch (status) {
         case EPERM:
            /* No need to report errors when we get an EPERM */
            break;
         default:
            /* We don't halt execution, reporting the error should be sufficient */
            Jmsg2(NULL, M_ERROR, 0, _("Unable to destroy mutex: %d ERR=%s\n"),
                  status, be.bstrerror(status));
            break;
         }
      }
   }

   /* Unset static locking callback */
   CRYPTO_set_locking_callback(NULL);

   /* Free static lock array */
   free(mutexes);

   /* Unset dynamic locking callbacks */
   CRYPTO_set_dynlock_create_callback(NULL);
   CRYPTO_set_dynlock_lock_callback(NULL);
   CRYPTO_set_dynlock_destroy_callback(NULL);
}


/*
 * Seed OpenSSL PRNG
 *  Returns: 1 on success
 *           0 on failure
 */
int openssl_seed_prng (void)
{
   const char *names[]  = { "/dev/urandom", "/dev/random", NULL };
   int i;

   // ***FIXME***
   // Win32 Support
   // Read saved entropy?

   for (i = 0; names[i]; i++) {
      if (RAND_load_file(names[i], 1024) != -1) {
         /* Success */
         return 1;
      }
   }

   /* Fail */
   return 0;
}

/*
 * Save OpenSSL Entropy
 *  Returns: 1 on success
 *           0 on failure
 */
int openssl_save_prng (void)
{
   // ***FIXME***
   // Implement PRNG state save
   return 1;
}

#endif /* HAVE_OPENSSL */
