from bareosfd import *
from bareos_fd_consts import *
from io import open
from os import O_WRONLY, O_CREAT

def load_bareos_plugin(context, plugindef):
  DebugMessage(context, 100, "load_bareos_plugin called with param: " + plugindef + "\n");
  events = [];
  events.append(bEventType['bEventJobEnd']);
  events.append(bEventType['bEventEndBackupJob']);
  events.append(bEventType['bEventEndFileSet']);
  events.append(bEventType['bEventHandleBackupFile']);
  RegisterEvents(context, events);
  fdname = GetValue(context, bVariable['bVarFDName']);
  DebugMessage(context, 100, "FDName = " + fdname + "\n");
  workingdir = GetValue(context, bVariable['bVarWorkingDir']);
  DebugMessage(context, 100, "WorkingDir = " + workingdir + "\n");

  return bRCs['bRC_OK'];

def parse_plugin_definition(context, plugindef):
  global file_to_backup;

  DebugMessage(context, 100, "parse_plugin_definition called with param: " + plugindef + "\n");
  file_to_backup = "Unknown";
  plugin_options = plugindef.split(":");
  for current_option in plugin_options:
    key,sep,val = current_option.partition("=");
    if val == '':
      continue;

    elif key == 'module_path':
      continue;

    elif key == 'module_name':
      continue;

    elif key == 'filename':
      file_to_backup = val;
      continue;

    else:
      DebugMessage(context, 100, "parse_plugin_definition unknown option " + key + "with value " + val + "\n");
      return bRCs['bRC_Error'];

  return bRCs['bRC_OK'];

def handle_plugin_event(context, event):
  if event == bEventType['bEventJobEnd']:
    DebugMessage(context, 100, "handle_plugin_event called with bEventJobEnd\n");

  elif event == bEventType['bEventEndBackupJob']:
    DebugMessage(context, 100, "handle_plugin_event called with bEventEndBackupJob\n");

  elif event == bEventType['bEventEndFileSet']:
    DebugMessage(context, 100, "handle_plugin_event called with bEventEndFileSet\n");

  else:
    DebugMessage(context, 100, "handle_plugin_event called with event" + str(event) + "\n");

  return bRCs['bRC_OK'];

def start_backup_file(context, savepkt):
  DebugMessage(context, 100, "start_backup called\n");
  if file_to_backup == 'Unknown':
    JobMessage(context, bJobMessageType['M_FATAL'], "No filename specified in plugin definition to backup\n");
    return bRCs['bRC_Error'];

  statp = StatPacket();
  savepkt.statp = statp;
  savepkt.fname = file_to_backup;
  savepkt.type = bFileType['FT_REG'];

  JobMessage(context, bJobMessageType['M_INFO'], "Starting backup of " + file_to_backup + "\n");

  return bRCs['bRC_OK'];

def end_backup_file(context):
  DebugMessage(context, 100, "end_backup_file() entry point in Python called\n")

  return bRCs['bRC_OK'];

def plugin_io(context, IOP):
  global file

  DebugMessage(context, 100, "plugin_io called with " + str(IOP) + "\n");

  FNAME = IOP.fname;
  if IOP.func == bIOPS['IO_OPEN']:
    try:
      if IOP.flags & (O_CREAT | O_WRONLY):
        file = open(FNAME, 'wb');
      else:
        file = open(FNAME, 'rb');
    except:
      IOP.status = -1;
      return bRCs['bRC_Error'];

    return bRCs['bRC_OK'];

  elif IOP.func == bIOPS['IO_CLOSE']:
    file.close();
    return bRCs['bRC_OK'];

  elif IOP.func == bIOPS['IO_SEEK']:
    return bRCs['bRC_OK'];

  elif IOP.func == bIOPS['IO_READ']:
    IOP.buf = bytearray(IOP.count);
    IOP.status = file.readinto(IOP.buf);
    IOP.io_errno = 0
    return bRCs['bRC_OK'];

  elif IOP.func == bIOPS['IO_WRITE']:
    IOP.status = file.write(IOP.buf);
    IOP.io_errno = 0
    return bRCs['bRC_OK'];

def start_restore_file(context, cmd):
  DebugMessage(context, 100, "start_restore_file() entry point in Python called with " + str(cmd) + "\n")

  return bRCs['bRC_OK'];

def end_restore_file(context):
  DebugMessage(context, 100, "end_restore_file() entry point in Python called\n")

  return bRCs['bRC_OK'];

def create_file(context, restorepkt):
  DebugMessage(context, 100, "create_file() entry point in Python called with " + str(restorepkt) + "\n")

  restorepkt.create_status = bCFs['CF_EXTRACT'];

  return bRCs['bRC_OK'];

def check_file(context, fname):
  DebugMessage(context, 100, "check_file() entry point in Python called with " + str(fname) + "\n")

  return bRCs['bRC_OK'];

def restore_object_data(context, restoreobject):
  DebugMessage(context, 100, "restore_object_data called with " + str(restoreobject) + "\n");

  return bRCs['bRC_OK'];

def handle_backup_file(context, savepkt):
  DebugMessage(context, 100, "handle_backup_file called with " + str(savepkt) + "\n");

  return bRCs['bRC_OK'];
