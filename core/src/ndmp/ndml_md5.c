/*
 * Copyright (c) 2000
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
 * MD5 authentication support
 ****************************************************************
 * Both sides share a secret in the form of a clear-text
 * password. One side generates a challenge (64-bytes)
 * and conveys it to the other. The other side then
 * uses the challenge, the clear-text password, and
 * the NDMP rules for MD5, and generates a digest.
 * The digest is returned as proof that both sides
 * share the same secret clear-text password.
 *
 * The NDMP rules for MD5 are implemented in ndmmd5_digest().
 * It amounts to positioning the clear-text password and challenge
 * into a "message" buffer, then applying the MD5 algorithm.
 *
 * ndmmd5_generate_challenge() generates a challenge[]
 * using conventional random number routines.
 *
 * ndmmd5_ok_digest() takes a locally known challenge[]
 * and clear-text password, a remotely generated
 * digest[], and determines if everything is correct.
 *
 * Using MD5 prevents clear-text passwords from being conveyed
 * over the network. However, it compels both sides to maintain
 * clear-text passwords in a secure fashion, which is difficult
 * to say the least. Because the NDMP MD5 rules must be followed
 * to digest() the password, it's impractical to consider
 * an external authentication authority.
 *
 * Credits to Rajiv of NetApp for helping with MD5 stuff.
 */


#include "ndmlib.h"
#include "md5.h"


int ndmmd5_generate_challenge(char challenge[NDMP_MD5_CHALLENGE_LENGTH])
{
  int i;

  NDMOS_MACRO_SRAND();

  for (i = 0; i < NDMP_MD5_CHALLENGE_LENGTH; i++) {
    challenge[i] = NDMOS_MACRO_RAND() >> (i & 7);
  }

  return 0;
}


int ndmmd5_ok_digest(char challenge[NDMP_MD5_CHALLENGE_LENGTH],
                     char* clear_text_password,
                     char digest[NDMP_MD5_DIGEST_LENGTH])
{
  char my_digest[16];
  int i;

  ndmmd5_digest(challenge, clear_text_password, my_digest);

  for (i = 0; i < NDMP_MD5_DIGEST_LENGTH; i++)
    if (digest[i] != my_digest[i]) return 0; /* Invalid */

  return 1; /* OK */
}


int ndmmd5_digest(char challenge[NDMP_MD5_CHALLENGE_LENGTH],
                  char* clear_text_password,
                  char digest[NDMP_MD5_DIGEST_LENGTH])
{
  int pwlength = strlen(clear_text_password);
  struct MD5Context mdContext;
  unsigned char message[128];

  /*
   * The spec describes the construction of the 128 byte
   * "message" (probably MD5-speak). It is described as:
   *
   *    PASSWORD PADDING CHALLENGE PADDING PASSWORD
   *
   * Each PADDING is defined as zeros of length 64 minus pwlen.
   *
   * A pwlen of over 32 would result in not all fields
   * fitting. This begs a question of the order elements
   * are inserted into the message[]. You get a different
   * message[] if you insert the PASSWORD(s) before
   * the CHALLENGE than you get the other way around.
   *
   * A pwlen of over 64 would result in PADDING of negative
   * length, which could cause crash boom bang.
   *
   * The resolution of this vaguery implemented here is to
   * only use the first 32 bytes of the password. All
   * fields fit. Order dependencies are avoided.
   *
   * Final resolution is pending.
   */
  if (pwlength > 32) pwlength = 32;

  /*
   * Compose the 128-byte buffer according to NDMP rules
   */
  NDMOS_API_BZERO(message, sizeof message);
  NDMOS_API_BCOPY(clear_text_password, &message[0], pwlength);
  NDMOS_API_BCOPY(clear_text_password, &message[128 - pwlength], pwlength);
  NDMOS_API_BCOPY(challenge, &message[64 - pwlength], 64);

  /*
   * Grind it up, ala MD5
   */
  MD5Init(&mdContext);
  MD5Update(&mdContext, message, 128);
  MD5Final((unsigned char*)digest, &mdContext);

  /*
   * ding! done
   */
  return 0;
}
