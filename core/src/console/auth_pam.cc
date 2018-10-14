/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#include <security/pam_appl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "include/bareos.h"
#include "console_output.h"

enum class PamAuthState {
   INIT,
   RECEIVE_MSG_TYPE,
   RECEIVE_MSG,
   READ_INPUT,
   SEND_INPUT,
   AUTH_OK
};

static PamAuthState state = PamAuthState::INIT;
static bool SetEcho(FILE *std_in, bool on);


bool ConsolePamAuthenticate(FILE *std_in, BareosSocket *UA_sock)
{
   bool quit = false;
   bool error = false;

   int type;
   btimer_t *tid = nullptr;
   char *userinput = nullptr;

//   UA_sock->fsend("@@username:franku");

   while (!error && !quit) {
      switch(state) {
         case PamAuthState::INIT:
            if(tid) {
               StopBsockTimer(tid);
            }
            tid = StartBsockTimer(UA_sock, 10);
            if (!tid) {
               error = true;
            }
            state = PamAuthState::RECEIVE_MSG_TYPE;
            break;
         case PamAuthState::RECEIVE_MSG_TYPE:
            if (UA_sock->recv() == 1) {
               type = UA_sock->msg[0];
               switch (type) {
                  case PAM_PROMPT_ECHO_OFF:
                  case PAM_PROMPT_ECHO_ON:
                     SetEcho (std_in, type == PAM_PROMPT_ECHO_ON);
                     state = PamAuthState::RECEIVE_MSG;
                     break;
                  case PAM_SUCCESS:
                     state = PamAuthState::AUTH_OK;
                     quit = true;
                     break;
                  default:
                     Dmsg1(100, "Error, unknown pam type %d\n", type);
                     error = true;
                     break;
               } /* switch (type) */
            } else {
               error = true;
            }
            break;
         case PamAuthState::RECEIVE_MSG:
            if (UA_sock->recv() > 0) {
               ConsoleOutput(UA_sock->msg);
               state = PamAuthState::READ_INPUT;
            } else {
               error = true;
            }
            break;
         case PamAuthState::READ_INPUT: {
               userinput = readline("");
               if (userinput) {
                  state = PamAuthState::SEND_INPUT;
               } else {
                  error = true;
               }
            }
            break;
         case PamAuthState::SEND_INPUT:
            UA_sock->fsend(userinput);
            Actuallyfree(userinput);
            state = PamAuthState::INIT;
            break;
         default:
            break;
      }
      if (UA_sock->IsStop() || UA_sock->IsError()) {
         if(userinput) {
            Actuallyfree(userinput);
         }
         if(tid) {
            StopBsockTimer(tid);
         }
         error = true;
         break;
      }
   }; /* while (!quit) */

   SetEcho (std_in, true);
   ConsoleOutput("\n");

   return !error;
}

#include <termios.h>
static bool SetEcho(FILE *std_in, bool on)
{
    struct termios termios_old, termios_new;

    /* Turn echoing off and fail if we can’t. */
    if (tcgetattr (fileno (std_in), &termios_old) != 0)
        return false;
    termios_new = termios_old;
    if (on) {
      termios_new.c_lflag |=  ECHO;
    } else {
      termios_new.c_lflag &= ~ECHO;
    }
    if (tcsetattr (fileno (std_in), TCSAFLUSH, &termios_new) != 0)
        return false;
   return true;
}
