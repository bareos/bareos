/*
 * Copyright (c) 1998,1999,2000
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
 *      This is the #include file for the Operating System
 *      (O/S) specific portion of the NDMJOBLIB library.
 *      By O/S we mean the programming environment including
 *      compilers, header files, as well the host O/S APIs.
 *      O/S specific is clear and concise, so that's how we refer
 *      to the hosting environment.
 *
 *      This file, ndmos.h, essentially #include's the right
 *      ndmos_xxx.h for the hosting environment. The companion
 *      source C files, ndmos_xxx*.c, are similarly selected by
 *      ndmos.c.
 *
 *      The strategy for separating the O/S specific and O/S
 *      generic portions of NDMJOBLIB has four key points:
 *
 *        1) Isolate O/S specific portions in separate
 *           files which can be developed, contributed,
 *           and maintained independently of the overall
 *           source base.
 *
 *        2) NEVER NEVER #ifdef based on O/S or programming
 *           environment in the O/S generic portions. These
 *           make collective maintenance and integration
 *           too difficult.
 *
 *        3) Use O/S specific #define macros (NDMOS_...)
 *           and C functions (ndmos_...) as wrappers around
 *           the portions that vary between environments
 *           and applications.
 *
 *        4) Use generic, objective-oriented #ifdef's to isolate
 *           and omit functionality which may not be wanted in
 *           all applications.
 *
 *      There are templates in ndmos_xxx.h and ndmos_xxx.c
 *      to get started on a new O/S specific portion.
 *      Send contributions to the current keeper of NDMJOB.
 *      Contact ndmp-tech@ndmp.org for details.
 *
 *      >>>  DO NOT MODIFY THIS FILE OR ANY GENERIC  <<<
 *      >>>  PORTION OF NDMJOBLIB FOR THE SAKE OF A  <<<
 *      >>>  HOSTING ENVIRONMENT OR APPLICATION.     <<<
 *
 *      If you discover additional isolation requirements,
 *      raise the issue on ndmp-tech@ndmp.org. Propose new
 *      #define NDMOS_ macros to address them. Then, submit
 *      the proposal with required changes to the current
 *      keeper of NDMJOB. Changes to the generic portion which
 *      use #ifdef's based on anything other than NDMOS_
 *      macros will be summarily rejected.
 *
 *      There are four sections of this file:
 *        1) Establish identities for various O/S platforms
 *        2) Try to auto-recognize the environment
 *        3) #include the right O/S specific ndmos_xxx.h
 *        4) Establish default #define-itions for macros
 *           left undefined
 */


#ifndef _NDMOS_H
#define _NDMOS_H

// Silence compiler for known warnings.
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wenum-conversion"
#elif __GNUC__
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wenum-compare"
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif
#elif __SUNPRO_C
#pragma error_messages(off, E_ENUM_TYPE_MISMATCH_OP, E_ENUM_TYPE_MISMATCH_ARG, \
                       E_STATEMENT_NOT_REACHED)
#endif

// Operating system idents
#define NDMOS_IDENT(A, B, C, D) (((A) << 24) + ((B) << 16) + ((C) << 8) + (D))

#define NDMOS_ID_FREEBSD NDMOS_IDENT('F', 'B', 's', 'd')
#define NDMOS_ID_SOLARIS NDMOS_IDENT('S', 'o', 'l', 'a')
#define NDMOS_ID_LINUX NDMOS_IDENT('L', 'n', 'u', 'x')
#define NDMOS_ID_IRIX NDMOS_IDENT('I', 'R', 'I', 'X')
#define NDMOS_ID_HPUX NDMOS_IDENT('H', 'P', 'U', 'X')
#define NDMOS_ID_ULTRIX NDMOS_IDENT('U', 'l', 't', 'r')
#define NDMOS_ID_TRU64 NDMOS_IDENT('T', 'R', 'U', 64)
#define NDMOS_ID_AIX NDMOS_IDENT('A', 'I', 'X', 0)

/*
 * Only explicitly include config.h when we are doing standalone
 * compiling of the lib when included from Bareos there is no need
 * to re-include config.h as that config file is not protected against
 * multiple including. So we just test to see if include/bareos.h is not
 * already included as that leaves a nice fingerprint.
 */
#ifndef BAREOS_INCLUDE_BAREOS_H_
#include "include/config.h"
#endif

/*
 * If NDMOS_ID isn't set, try to autorecognize the OS based
 * on standard CPP symbols.
 */
#ifndef NDMOS_ID
#ifdef HAVE_FREEBSD_OS
#define NDMOS_ID NDMOS_ID_FREEBSD
#endif

#ifdef HAVE_SUN_OS
#define NDMOS_ID NDMOS_ID_SOLARIS
#endif

#ifdef HAVE_LINUX_OS
#define NDMOS_ID NDMOS_ID_LINUX
#endif
#endif /* !NDMOS_ID */

// Do we got it?
#ifndef NDMOS_ID
@ @ @ @You need to use - DNDMOS_ID = NDMOS_ID_xxxx
#endif


// Based on NDMOS_ID, #include the right O/S specific header file
#if NDMOS_ID == NDMOS_ID_FREEBSD
#include "ndmos_freebsd.h"
#endif

#if NDMOS_ID == NDMOS_ID_SOLARIS
#include "ndmos_solaris.h"
#endif

#if NDMOS_ID == NDMOS_ID_LINUX
#include "ndmos_linux.h"
#endif


/*
 * From here down, pre-processor symbols are #define'd to defaults
 * if not already #define'd in the O/S specific header file.
 *
 * Application Program Interfaces (APIs). The macros give the
 * O/S specific header a chance to use casts or different functions.
 *
 * NDMOS_API_BCOPY              -- memory copy
 * NDMOS_API_BZERO              -- zero-fill memory
 * NDMOS_API_MALLOC             -- memory allocator, no initialization
 * NDMOS_API_FREE               -- memory deallocator
 * NDMOS_API_STRTOLL            -- convert strings to long long
 * NDMOS_API_STRDUP             -- malloc() and strcpy()
 * NDMOS_API_STREND             -- return pointer to end of string
 *
 * Important constants that sometimes vary between O/S's.
 *
 * NDMOS_CONST_ALIGN            -- alignment for memory allocation
 * NDMOS_CONST_EWOULDBLOCK      -- errno when async I/O would stall
 * NDMOS_CONST_TAPE_REC_MAX     -- maximum tape record length
 * NDMOS_CONST_TAPE_REC_MIN     -- minimum tape record length
 * NDMOS_CONST_PATH_MAX         -- maximum path name length
 * NDMOS_CONST_NDMJOBLIB_REVISION -- String for in-house change level
 *                                 Convention "0" if no changes
 *                                 from released source. If changes,
 *                                 the initials of local keeper with
 *                                 sequence number or date code.
 * NDMOS_CONST_VENDOR_NAME      -- String for server_info_reply.vendor_name
 * NDMOS_CONST_PRODUCT_NAME     -- String for server_info_reply.product_name
 * NDMOS_CONST_PRODUCT_REVISION -- String for product change level
 *                                 By "product" we mean the program
 *                                 enclosing this NDMJOBLIB library.
 * NDMOS_CONST_NDMOS_REVISION   -- String for ndmos_xxx change level
 *
 * Macros for certain small operations which can vary.
 *
 * NDMOS_MACRO_NEW              -- allocate a data structure, wrapper
 *                                 around NDMOS_API_MALLOC() with casts
 * NDMOS_MACRO_NEWN             -- allocate a vector of data structs
 * NDMOS_MACRO_ZEROFILL         -- zero-fill data structure, wrapper
 *                                 around NDMOS_API_BZERO()
 * NDMOS_MACRO_ZEROFILL_SIZE    -- zero-fill data structure, wrapper
 *                                 around NDMOS_API_BZERO()
 * NDMOS_MACRO_SRAND            -- wrapper around srand(3), for MD5
 * NDMOS_MACRO_RAND             -- wrapper around rand(3), for MD5
 * NDMOS_MACRO_OK_TAPE_REC_LEN  -- Uses NDMOS_CONST_TAPE_REC_MIN/MAX
 * NDMOS_MACRO_SET_SOCKADDR     -- Sets struct sockaddr_in with
 *                                 respect to NDMOS_OPTION_HAVE_SIN_LEN
 *
 * The NDMOS_MACRO_xxx_ADDITIONS macros allow additional O/S
 * specific members to key structures in ndmagents.h
 *
 * NDMOS_MACRO_CONTROL_AGENT_ADDITIONS  -- struct ndm_control_agent
 * NDMOS_MACRO_DATA_AGENT_ADDITIONS     -- struct ndm_data_agent
 * NDMOS_MACRO_ROBOT_AGENT_ADDITIONS    -- struct ndm_robot_agent
 * NDMOS_MACRO_SESSION_ADDITIONS        -- struct ndm_session
 * NDMOS_MACRO_TAPE_AGENT_ADDITIONS     -- struct ndm_tape_agent
 *
 * All NDMOS_OPTION_... parameters are either #define'd or #undef'ed.
 * They are solely interpretted in #ifdef's and #ifndef's, and the
 * value of the definition means nothing. These _OPTIONS_'s can be set
 * on the command line (-D) in the Makefile, or in the O/S specific
 * header file.
 *
 * NDMOS_OPTION_NO_NDMP2        -- omit NDMPv2 support
 * NDMOS_OPTION_NO_NDMP3        -- omit NDMPv3 support
 * NDMOS_OPTION_NO_NDMP4        -- omit NDMPv4 support
 *
 * NDMOS_OPTION_NO_CONTROL_AGENT -- omit CONTROL agent features
 * NDMOS_OPTION_NO_DATA_AGENT   -- omit DATA agent features
 * NDMOS_OPTION_NO_ROBOT_AGENT  -- omit ROBOT agent features
 * NDMOS_OPTION_NO_TAPE_AGENT   -- omit TAPE agent features
 * NDMOS_OPTION_NO_TEST_AGENTS  -- omit any agent test features
 *      For some purposes, an all-in-one is overkill, or perhaps
 *      impractical. Everything is carefully constructed so
 *      that certain agents can be omitted without disturbing
 *      the rest. The two most obvious uses are to either
 *      keep or omit just the CONTROL agent.
 *
 * NDMOS_OPTION_ALLOW_SCSI_AND_TAPE_BOTH_OPEN
 *      The NDMP specification says that both TAPE and SCSI can not
 *      be open at the same time. Then the workflow docs suggest
 *      a separate process for the ROBOT agent to get around the
 *      restriction. #define'ing this breaks the spec and allows
 *      both to be open.
 *
 * NDMOS_OPTION_HAVE_SIN_LEN
 *      In preparation for IPv6, some O/Ss use a sin_len field to
 *      indicate the length of the address. This _OPTION_ affects
 *      the standard NDMOS_MACRO_SET_SOCKADDR.
 *
 * NDMOS_OPTION_TAPE_SIMULATOR
 *      Early in bring-up on a new system, it's a little easier to
 *      get started by using the tape simulator. It represents
 *      an idealized MTIO-type tape subsystem, and uses a disk
 *      file to represent the tape. As the real O/S specific tape
 *      subsystem interface is implemented (ndmos_tape_...()), the
 *      tape simulator serves as a reference for correct implementation.
 *
 * NDMOS_OPTION_ROBOT_SIMULATOR
 *      Similarly, for testing multi-tape operations, it's easier to
 *      get started by using the robot simulator. It represents a simple
 *      robot with ten slots and two drives.  It operates on a directory,
 *      and uses rename() to load and unload tapes.  It operates in concert
 *      with the tape simulator.  The robot name is a directory, and it
 *      creates drives named 'drive0', 'drive1', etc. in that directory
 *
 * NDMOS_OPTION_GAP_SIMULATOR
 *      Implement the spinnaker GAP simulator in the tape simulator.
 *      When you want a clean tape stream in the tape simulator disable
 *      this option and the output should only contain the info from
 *      the data agent no fancy Begin Of Tape (BOT) simulation etc.
 *
 * NDMOS_OPTION_USE_SELECT_FOR_CHAN_POLL -- use common select() code
 * NDMOS_OPTION_USE_POLL_FOR_CHAN_POLL   -- use common poll() code
 *      These two _OPTION_'s choose common code to implement
 *      ndmos_chan_poll(). The select() and poll() functions are
 *      fairly common and consistent among various O/S's to detect
 *      ready I/O. Only one can be defined. If the common code
 *      doesn't work out, don't define either _OPTION_ and implement
 *      ndmos_chan_poll() in the O/S specific C source file.
 *
 * NDMOS_OPTION_USE_GETHOSTBYNAME -- use gethostbyname() code
 * NDMOS_OPTION_USE_GETADDRINFO   -- use getaddrinfo() code
 *      These two _OPTION_'s choose common code to implement
 *      the lookup of a host. getaddrinfo() is the new interface
 *      and gethostbyname() is deprecated by POSIX so should only
 *      be used when there is no support for getaddrinfo()
 */

#if HAVE_POLL
#define NDMOS_OPTION_USE_POLL_FOR_CHAN_POLL 1
#else
#define NDMOS_OPTION_USE_SELECT_FOR_CHAN_POLL 1
#endif

#define NDMOS_OPTION_USE_GETADDRINFO 1
#define NDMOS_OPTION_TAPE_SIMULATOR 1
#define NDMOS_OPTION_ROBOT_SIMULATOR 1
#define NDMOS_OPTION_GAP_SIMULATOR 1

// Constants
#ifndef NDMOS_CONST_ALIGN
#define NDMOS_CONST_ALIGN sizeof(uint64_t)
#endif /* !NDMOS_CONST_ALIGN */

#ifndef NDMOS_CONST_TAPE_REC_MIN
#define NDMOS_CONST_TAPE_REC_MIN 1
#endif /* !NDMOS_CONST_TAPE_REC_MIN */

#ifndef NDMOS_CONST_TAPE_REC_MAX
#define NDMOS_CONST_TAPE_REC_MAX (1024 * 1024)
#endif /* !NDMOS_CONST_TAPE_REC_MAX */

#ifndef NDMOS_CONST_PATH_MAX
#define NDMOS_CONST_PATH_MAX (1024)
#endif /* !NDMOS_CONST_PATH_MAX */

#ifndef NDMOS_CONST_EWOULDBLOCK
#ifdef EWOULDBLOCK
#define NDMOS_CONST_EWOULDBLOCK EWOULDBLOCK
#else /* EWOULDBLOCK */
#define NDMOS_CONST_EWOULDBLOCK EAGAIN
#endif /* EWOULDBLOCK */
#endif /* !NDMOS_CONST_EWOULDBLOCK */

#ifndef NDMOS_CONST_NDMJOBLIB_REVISION
#define NDMOS_CONST_NDMJOBLIB_REVISION "1.4a"
#endif /* !NDMOS_CONST_NDMJOBLIB_REVISION */

#ifndef NDMOS_CONST_VENDOR_NAME
#define NDMOS_CONST_VENDOR_NAME "PublicDomain"
#endif /* !NDMOS_CONST_VENDOR_NAME */

#ifndef NDMOS_CONST_PRODUCT_NAME
#define NDMOS_CONST_PRODUCT_NAME "NDMJOB"
#endif /* !NDMOS_CONST_PRODUCT_NAME */

#ifndef NDMOS_CONST_PRODUCT_REVISION
#define NDMOS_CONST_PRODUCT_REVISION "1.4a"
#endif /* !NDMOS_CONST_PRODUCT_REVISION */

#ifndef NDMOS_CONST_NDMOS_REVISION
#define NDMOS_CONST_NDMOS_REVISION "0"
#endif /* !NDMOS_CONST_NDMOS_REVISION */


// Application Program Interfaces (APIs)
#ifndef NDMOS_API_BZERO
#define NDMOS_API_BZERO(P, N) (void)bzero((void*)(P), (N))
#endif /* !NDMOS_API_BZERO */

#ifndef NDMOS_API_BCOPY
#define NDMOS_API_BCOPY(S, D, N) (void)bcopy((void*)(S), (void*)(D), (N))
#endif /* !NDMOS_API_BCOPY */

#ifndef NDMOS_API_MALLOC
#define NDMOS_API_MALLOC(N) malloc(N)
#endif /* !NDMOS_API_MALLOC */

#ifndef NDMOS_API_FREE
#define NDMOS_API_FREE(P) free((void*)(P))
#endif /* !NDMOS_API_FREE */

#ifndef NDMOS_API_STRTOLL
#define NDMOS_API_STRTOLL(P, PP, BASE) strtoll(P, PP, BASE)
#endif /* !NDMOS_API_STRTOLL */

#ifndef NDMOS_API_STRDUP
#define NDMOS_API_STRDUP(S) strdup(S)
#endif /* !NDMOS_API_STRDUP */

#ifndef NDMOS_API_STREND

#ifdef __cplusplus
    extern "C"
{
#endif

  extern char* ndml_strend(char* s); /* ndml_util.c */

#ifdef __cplusplus
}
#endif

#define NDMOS_API_STREND(S) ndml_strend(S)
#endif /* !NDMOS_API_STREND */


// Macros
#ifndef NDMOS_MACRO_NEW
#define NDMOS_MACRO_NEW(T) ((T*)NDMOS_API_MALLOC(sizeof(T)))
#endif /* !NDMOS_MACRO_NEW */

#ifndef NDMOS_MACRO_NEWN
#define NDMOS_MACRO_NEWN(T, N) ((T*)NDMOS_API_MALLOC(sizeof(T) * (N)))
#endif /* !NDMOS_MACRO_NEWN */

#ifndef NDMOS_MACRO_FREE
#define NDMOS_MACRO_FREE(T) free(T)
#endif

#ifndef NDMOS_MACRO_ZEROFILL
#define NDMOS_MACRO_ZEROFILL(P) NDMOS_API_BZERO(P, sizeof *(P))
#endif /* !NDMOS_MACRO_ZEROFILL */

#ifndef NDMOS_MACRO_ZEROFILL_SIZE
#define NDMOS_MACRO_ZEROFILL_SIZE(P, N) NDMOS_API_BZERO(P, N)
#endif /* !NDMOS_MACRO_ZEROFILL_SIZE */

#ifndef NDMOS_MACRO_SRAND
#define NDMOS_MACRO_SRAND() srand(time(0))
#endif /* !NDMOS_MACRO_SRAND */

#ifndef NDMOS_MACRO_RAND
#define NDMOS_MACRO_RAND() rand()
#endif /* !NDMOS_MACRO_RAND */

#ifndef NDMOS_MACRO_OK_TAPE_REC_LEN
#define NDMOS_MACRO_OK_TAPE_REC_LEN(LEN) \
  (NDMOS_CONST_TAPE_REC_MIN <= (LEN) && (LEN) <= NDMOS_CONST_TAPE_REC_MAX)
#endif /* !NDMOS_MACRO_OK_TAPE_REC_LEN */

#ifndef NDMOS_MACRO_SET_SOCKADDR
#ifdef NDMOS_OPTION_HAVE_SIN_LEN
#define NDMOS_MACRO_SET_SOCKADDR(SA, INADDR, PORT)               \
  (NDMOS_MACRO_ZEROFILL(SA),                                     \
   ((struct sockaddr_in*)(SA))->sin_len = sizeof *(SA),          \
   ((struct sockaddr_in*)(SA))->sin_family = AF_INET,            \
   ((struct sockaddr_in*)(SA))->sin_addr.s_addr = htonl(INADDR), \
   ((struct sockaddr_in*)(SA))->sin_port = htons(PORT))
#else /* NDMOS_OPTION_HAVE_SIN_LEN */
#define NDMOS_MACRO_SET_SOCKADDR(SA, INADDR, PORT)               \
  (NDMOS_MACRO_ZEROFILL(SA),                                     \
   ((struct sockaddr_in*)(SA))->sin_family = AF_INET,            \
   ((struct sockaddr_in*)(SA))->sin_addr.s_addr = htonl(INADDR), \
   ((struct sockaddr_in*)(SA))->sin_port = htons(PORT))
#endif /* NDMOS_OPTION_HAVE_SIN_LEN */
#endif /* !NDMOS_MACRO_SET_SOCKADDR */


// Composite effects

#ifdef NDMOS_OPTION_NO_DATA_AGENT
#ifdef NDMOS_OPTION_NO_TAPE_AGENT
#ifdef NDMOS_OPTION_NO_ROBOT_AGENT
#define NDMOS_EFFECT_NO_SERVER_AGENTS
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */

#ifdef NDMOS_OPTION_NO_NDMP3
#ifdef NDMOS_OPTION_NO_NDMP4
#define NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#endif /* !NDMOS_OPTION_NO_NDMP4 */

/* check for a conflict: robot sim requires tape sim */
#ifdef NDMOS_OPTION_ROBOT_SIMULATOR
#ifndef NDMOS_OPTION_TAPE_SIMULATOR
#error robot simulator requires the tape simulator
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */
#ifdef NDMOS_COMMON_SCSI_INTERFACE
#error robot simulator and ndmos scsi interface are incompatible
#endif
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */

// simulator fields

#ifdef NDMOS_OPTION_TAPE_SIMULATOR
#define NDMOS_MACRO_TAPE_AGENT_ADDITIONS \
  int32_t tape_fd;                       \
  char* drive_name;                      \
  int32_t weof_on_close;                 \
  int32_t sent_leom;
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */

#ifdef NDMOS_OPTION_ROBOT_SIMULATOR
#undef NDMOS_MACRO_ROBOT_AGENT_ADDITIONS
#define NDMOS_MACRO_ROBOT_AGENT_ADDITIONS char* sim_dir;
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */

#endif /* _NDMOS_H */
