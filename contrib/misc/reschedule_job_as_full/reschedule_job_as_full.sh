#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024 Bareos GmbH & Co. KG
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
#
# This helper script can be used to reschedule a failed job to level=Full.
# You can add this runscript resource in your job jobdefs like this
#  RunScript {
#    RunsWhen = After
#    RunsOnFailure = Yes
#    RunsOnSuccess = No
#    RunsOnClient = No
#    Command = "/usr/lib/bareos/scripts/reschedule_job_as_full.sh '%e' '%i' '%n' '%l' '%t'"
#  }
# The last argument of the command line can be a path for a debug log file.
JobExitStatus=$1
JobId=$2
JobName=$3
JobLevel=$4
JobType=$5

debug_log_file=${6:-/dev/null}
# To enable debug log files, use the following:
# debug_log_file="/tmp/reschedule_incremental_as_full_job_${JobId}.log"
{
  echo "JobExitStatus:${JobExitStatus}"
  echo "JobId:${JobId}"
  echo "JobName:${JobName}"
  echo "JobLevel:${JobLevel}"
  echo "JobType:${JobType}"
} >> "${debug_log_file}"

if [ "${JobExitStatus}" == "Fatal Error" ] && [ "${JobLevel}" == "Incremental" ] && [ "${JobType}" == "Backup" ]; then
  if (bconsole <<< "list joblog jobid=${JobId}" | tee -a "${debug_log_file}" | grep -q -F "A new full level backup of this job is required."); then
    echo "Required new full level run will be started in 1 minute."
    if ! bconsole <<< "run job=${JobName} level=Full when=\"$(date -d "now 1min" +"%Y-%m-%d %H:%M:%S")\" yes"; then
      echo "Error while rescheduling ${JobName}"
      exit 1
    fi
  else
    echo "No match for new full level required!" >> "${debug_log_file}"
  fi
fi

