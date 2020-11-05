#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

"""
Constants used by Bareos.
"""


class Constants:

    unit_separator = chr(0x1F)
    record_separator = chr(0x1E)
    record_separator_compat_regex = r"[ {0}]".format(chr(0x1E))
    group_separator = chr(0x1D)

    """
    translated enum from https://github.com/bareos/bareos/blob/master/core/src/lib/bsock.h
    """
    BNET_EOD = -1  # ,          /* End of data stream, new data may follow */
    BNET_EOD_POLL = -2  # ,          /* End of data and poll all in one */
    BNET_STATUS = -3  # ,          /* Send full status */
    BNET_TERMINATE = -4  # ,          /* Conversation terminated, doing close() */
    BNET_POLL = -5  # ,          /* Poll request, I'm hanging on a read */
    BNET_HEARTBEAT = -6  # ,          /* Heartbeat Response requested */
    BNET_HB_RESPONSE = -7  # ,          /* Only response permited to HB */
    BNET_xxxxxxPROMPT = -8  # ,          /* No longer used -- Prompt for subcommand */
    BNET_BTIME = -9  # ,          /* Send UTC btime */
    BNET_BREAK = -10  # ,         /* Stop current command -- ctl-c */
    BNET_START_SELECT = -11  # ,         /* Start of a selection list */
    BNET_END_SELECT = -12  # ,         /* End of a select list */
    BNET_INVALID_CMD = -13  # ,         /* Invalid command sent */
    BNET_CMD_FAILED = -14  # ,         /* Command failed */
    BNET_CMD_OK = -15  # ,         /* Command succeeded */
    BNET_CMD_BEGIN = -16  # ,         /* Start command execution */
    BNET_MSGS_PENDING = -17  # ,         /* Messages pending */
    BNET_MAIN_PROMPT = -18  # ,         /* Server ready and waiting */
    BNET_SELECT_INPUT = -19  # ,         /* Return selection input */
    BNET_WARNING_MSG = -20  # ,         /* Warning message */
    BNET_ERROR_MSG = -21  # ,         /* Error message -- command failed */
    BNET_INFO_MSG = -22  # ,         /* Info message -- status line */
    BNET_RUN_CMD = -23  # ,         /* Run command follows */
    BNET_YESNO = -24  # ,         /* Request yes no response */
    BNET_START_RTREE = -25  # ,         /* Start restore tree mode */
    BNET_END_RTREE = -26  # ,         /* End restore tree mode */
    BNET_SUB_PROMPT = -27  # ,         /* Indicate we are at a subprompt */
    BNET_TEXT_INPUT = -28  #          /* Get text input from user */

    description = {
        BNET_EOD: "End of data stream, new data may follow",
        BNET_EOD_POLL: "End of data and poll all in one",
        BNET_STATUS: "Send full status",
        BNET_TERMINATE: "Conversation terminated",
        BNET_POLL: "Poll request, I'm hanging on a read",
        BNET_HEARTBEAT: "Heartbeat Response requested",
        BNET_HB_RESPONSE: "Only response permited to HB",
        BNET_xxxxxxPROMPT: "No longer used -- Prompt for subcommand",
        BNET_BTIME: "Send UTC btime",
        BNET_BREAK: "Stop current command -- ctl-c",
        BNET_START_SELECT: "Start of a selection list",
        BNET_END_SELECT: "End of a select list",
        BNET_INVALID_CMD: "Invalid command sent",
        BNET_CMD_FAILED: "Command failed",
        BNET_CMD_OK: "Command succeeded",
        BNET_CMD_BEGIN: "Start command execution",
        BNET_MSGS_PENDING: "Messages pending",
        BNET_MAIN_PROMPT: "Server ready and waiting",
        BNET_SELECT_INPUT: "Return selection input",
        BNET_WARNING_MSG: "Warning message",
        BNET_ERROR_MSG: "Error message -- command failed",
        BNET_INFO_MSG: "Info message -- status line",
        BNET_RUN_CMD: "Run command follows",
        BNET_YESNO: "Request yes no response",
        BNET_START_RTREE: "Start restore tree mode",
        BNET_END_RTREE: "End restore tree mode",
        BNET_SUB_PROMPT: "Indicate we are at a subprompt",
        BNET_TEXT_INPUT: "Get text input from user",
    }

    @staticmethod
    def get_description(code):
        try:
            description = Constants.description[code]
        except KeyError:
            # self.logger.error( "unknown bsock code " + code )
            return
        else:
            return description
