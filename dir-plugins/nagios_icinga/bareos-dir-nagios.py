# This is a sample Bareos director Plugin
# It can be used as a template. What it does so far is evaluating the status of a backup job
# when the event bDirEventJobEnd is called and computes a nagios/Icinga Code and status message.
#
# The message / code is not yet transferred to Nagios / Icinga, this needs adaption to your environment.
#
# Create a link from this file to your Bareos plugins dir/bareos-dir.py (usually /usr/lib64/bareos/plugins/bareos-dir.py) 
# This is a known limitation (only one director plugin with fixed name possible for now)
#
# Add the following 2 lines to your director config / director resource:
#
# Plugin Directory = /usr/lib/bareos/plugins
# Plugin Names = python 
#
# To see what is happening (debug messages) start your Bareos director with -d 200
# (c) Bareos GmbH & Co. KG, Maik Aussendorf
# License: AGPLv3

from bareosdir import *
from bareos_dir_consts import *

def load_bareos_plugin(context):
  DebugMessage(context, 100, "load_bareos_plugin called\n");
  events = [];
  events.append(bDirEventType['bDirEventJobStart']);
  events.append(bDirEventType['bDirEventJobEnd']);
  events.append(bDirEventType['bDirEventJobInit']);
  events.append(bDirEventType['bDirEventJobRun']);
  RegisterEvents(context, events);
  return bRCs['bRC_OK'];

def handle_plugin_event(context, event):
  if event == bDirEventType['bDirEventJobStart']:
    DebugMessage(context, 100, "bDirEventJobStart event triggered\n");
    jobname = GetValue(context, brDirVariable['bDirVarJobName']);
    DebugMessage(context, 100, "Job " + jobname + " starting\n");

  elif event == bDirEventType['bDirEventJobEnd']:
    DebugMessage(context, 100, "bDirEventJobEnd event triggered\n");
    # See http://regress.bareos.org/doxygen/html/d2/d75/namespacebareos__dir__consts.html
    # for a list of available brDirVariable
    jobName = GetValue(context, brDirVariable['bDirVarJobName']);
    # JobStatus is one char T:Terminated Successfully, E: terminated with errors, W: warning, A: Canceled by user, f: fatal error
    jobStatus = chr(GetValue(context, brDirVariable['bDirVarJobStatus']));
    jobErrors = GetValue(context, brDirVariable['bDirVarJobErrors']);
    jobBytes = GetValue(context, brDirVariable['bDirVarJobBytes']);
    jobId = GetValue(context, brDirVariable['bDirVarJobId']);
    jobClient = GetValue(context, brDirVariable['bDirVarClient']);
    DebugMessage(context, 100, "Job %s stopped with status %s, errors: %s, Total bytes: %d\n" %(jobName,jobStatus,jobErrors,jobBytes));
    nagiosMessage = "";
    if (jobStatus == 'E' or jobStatus == 'f'):
        nagiosResult = 2; # critical
        nagiosMessage = "CRITICAL: Bareos job %s on %s with id %d failed with %s errors (%s), %d jobBytes" %(jobName,jobClient,jobId,jobErrors,jobStatus,jobBytes);
    elif (jobStatus == 'W'):
        nagiosResult = 1; # warning
        nagiosMessage = "WARNING: Bareos job %s on %s with id %d terminated with warnings, %s errors, %d jobBytes" %(jobName,jobClient,jobId,jobErrors,jobBytes);
    elif (jobStatus == 'A'):
        nagiosResult = 1; # warning
        nagiosMessage = "WARNING: Bareos job %s on %s with id %d canceled, %s errors, %d jobBytes" %(jobName,jobClient,jobId,jobErrors,jobBytes);
    elif (jobStatus == 'T'):
        nagiosResult = 0; # OK
        nagiosMessage = "OK: Bareos job %s on %s id %d terminated successfully, %s errors, %d jobBytes" %(jobName,jobClient,jobId,jobErrors,jobBytes);
    else:
        nagiosResult = 3; # unknown
        nagiosMessage = "UNKNOWN: Bareos job %s on %s with id %d ended with %s errors (state: %s), %d jobBytes" %(jobName,jobClient,jobId,jobErrors,jobStatus,jobBytes);
    DebugMessage(context, 100, "Nagios Code: %d, NagiosMessage: %s\n" %(nagiosResult,nagiosMessage));

#TODO: for this example we have computed nagiosResult and nagiosMessage. 
# Must be somehow assigned to a host and service and
# passed to Nagios / Icinga via command pipe or nsca. 


  elif event == bDirEventType['bDirEventJobInit']:
    DebugMessage(context, 100, "bDirEventJobInit event triggered\n");

  elif event == bDirEventType['bDirEventJobRun']:
    DebugMessage(context, 100, "bDirEventJobRun event triggered\n");

  return bRCs['bRC_OK'];

