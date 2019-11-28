Bareos FD Plugin API
====================

To write a Bareos plugin, you create a dynamic shared object program (or
dll on Win32) with a particular name and two exported entry points,
place it in the **Plugins Directory**, which is defined in the
**bareos-fd.conf** file in the **Client** resource, and when the FD
starts, it will load all the plugins that end with **-fd.so** (or
**-fd.dll** on Win32) found in that directory.

Normal vs Command vs Options Plugins
------------------------------------

In general, there are three ways that plugins are called. The first way,
is when a particular event is detected in Bareos, it will transfer
control to each plugin that is loaded in turn informing the plugin of
the event. This is very similar to how a **RunScript** works, and the
events are very similar. Once the plugin gets control, it can interact
with Bareos by getting and setting Bareos variables. In this way, it
behaves much like a RunScript. Currently very few Bareos variables are
defined, but they will be implemented as the need arises, and it is very
extensible.

We plan to have plugins register to receive events that they normally
would not receive, such as an event for each file examined for backup or
restore. This feature is not yet implemented.

The second type of plugin, which is more useful and fully implemented in
the current version is what we call a command plugin. As with all
plugins, it gets notified of important events as noted above (details
described below), but in addition, this kind of plugin can accept a
command line, which is a:

::

       Plugin = <command-string>

directive that is placed in the Include section of a FileSet and is very
similar to the “File =” directive. When this Plugin directive is
encountered by Bareos during backup, it passes the “command” part of the
Plugin directive only to the plugin that is explicitly named in the
first field of that command string. This allows that plugin to backup
any file or files on the system that it wants. It can even create
“virtual files” in the catalog that contain data to be restored but do
not necessarily correspond to actual files on the filesystem.

The important features of the command plugin entry points are:

-  It is triggered by a “Plugin =” directive in the FileSet

-  Only a single plugin is called that is named on the “Plugin =”
   directive.

-  The full command string after the “Plugin =” is passed to the plugin
   so that it can be told what to backup/restore.

The third type of plugin is the Options Plugin, this kind of plugin is
useful to implement some custom filter on data. For example, you can
implement a compression algorithm in a very simple way. Bareos will call
this plugin for each file that is selected in a FileSet (according to
Wild/Regex/Exclude/Include rules). As with all plugins, it gets notified
of important events as noted above (details described below), but in
addition, this kind of plugin can be placed in a Options group, which is
a:

::

     FileSet {
        Name = TestFS
        Include {
           Options {
              Compression = GZIP1
              Signature = MD5
              Wild = "*.txt"
              Plugin = <command-string>
           }
           File = /
        }
    }

Loading Plugins
---------------

Once the File daemon loads the plugins, it asks the OS for the two entry
points (loadPlugin and unloadPlugin) then calls the **loadPlugin** entry
point (see below).

Bareos passes information to the plugin through this call and it gets
back information that it needs to use the plugin. Later, Bareos will
call particular functions that are defined by the **loadPlugin**
interface.

When Bareos is finished with the plugin (when Bareos is going to exit),
it will call the **unloadPlugin** entry point.

The two entry points are:

::

    bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)

    and

    bRC unloadPlugin()

both these external entry points to the shared object are defined as C
entry points to avoid name mangling complications with C++. However, the
shared object can actually be written in any language (preferably C or
C++) providing that it follows C language calling conventions.

The definitions for **bRC** and the arguments are
**src/filed/fd-plugins.h** and so this header file needs to be included
in your plugin. It along with **src/lib/plugins.h** define basically the
whole plugin interface. Within this header file, it includes the
following files:

::

    #include <sys/types.h>
    #include "config.h"
    #include "bc_types.h"
    #include "lib/plugins.h"
    #include <sys/stat.h>

Aside from the **bc_types.h** and **confit.h** headers, the plugin
definition uses the minimum code from Bareos. The bc_types.h file is
required to ensure that the data type definitions in arguments
correspond to the Bareos core code.

The return codes are defined as:

::

    typedef enum {
      bRC_OK    = 0,                         /* OK */
      bRC_Stop  = 1,                         /* Stop calling other plugins */
      bRC_Error = 2,                         /* Some kind of error */
      bRC_More  = 3,                         /* More files to backup */
      bRC_Term  = 4,                         /* Unload me */
      bRC_Seen  = 5,                         /* Return code from checkFiles */
      bRC_Core  = 6,                         /* Let Bareos core handles this file */
      bRC_Skip  = 7,                         /* Skip the proposed file */
    } bRC;

At a future point in time, we hope to make the Bareos libbac.a into a
shared object so that the plugin can use much more of Bareos’s
infrastructure, but for this first cut, we have tried to minimize the
dependence on Bareos.

loadPlugin
----------

As previously mentioned, the **loadPlugin** entry point in the plugin is
called immediately after Bareos loads the plugin when the File daemon
itself is first starting. This entry point is only called once during
the execution of the File daemon. In calling the plugin, the first two
arguments are information from Bareos that is passed to the plugin, and
the last two arguments are information about the plugin that the plugin
must return to Bareos. The call is:

::

    bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)

and the arguments are:

lbinfo
    This is information about Bareos in general. Currently, the only
    value defined in the bInfo structure is the version, which is the
    Bareos plugin interface version, currently defined as 1. The
    **size** is set to the byte size of the structure. The exact
    definition of the bInfo structure as of this writing is:

    ::

        typedef struct s_bareosInfo {
           uint32_t size;
           uint32_t version;
        } bInfo;

lbfuncs
    The bFuncs structure defines the callback entry points within Bareos
    that the plugin can use register events, get Bareos values, set
    Bareos values, and send messages to the Job output or debug output.

    The exact definition as of this writing is:

    ::

        typedef struct s_bareosFuncs {
           uint32_t size;
           uint32_t version;
           bRC (*registerBareosEvents)(bpContext *ctx, ...);
           bRC (*getBareosValue)(bpContext *ctx, bVariable var, void *value);
           bRC (*setBareosValue)(bpContext *ctx, bVariable var, void *value);
           bRC (*JobMessage)(bpContext *ctx, const char *file, int line,
               int type, utime_t mtime, const char *fmt, ...);
           bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
               int level, const char *fmt, ...);
           void *(*bareosMalloc)(bpContext *ctx, const char *file, int line,
               size_t size);
           void (*bareosFree)(bpContext *ctx, const char *file, int line, void *mem);
        } bFuncs;

    We will discuss these entry points and how to use them a bit later
    when describing the plugin code.

pInfo
    When the loadPlugin entry point is called, the plugin must
    initialize an information structure about the plugin and return a
    pointer to this structure to Bareos.

    The exact definition as of this writing is:

    ::

        typedef struct s_pluginInfo {
           uint32_t size;
           uint32_t version;
           const char *plugin_magic;
           const char *plugin_license;
           const char *plugin_author;
           const char *plugin_date;
           const char *plugin_version;
           const char *plugin_description;
        } pInfo;

    Where:

    version
        is the current Bareos defined plugin interface version,
        currently set to 1. If the interface version differs from the
        current version of Bareos, the plugin will not be run (not yet
        implemented).
    plugin_magic
        is a pointer to the text string “\*FDPluginData\*”, a sort of
        sanity check. If this value is not specified, the plugin will
        not be run (not yet implemented).
    plugin_license
        is a pointer to a text string that describes the plugin license.
        Bareos will only accept compatible licenses (not yet
        implemented).
    plugin_author
        is a pointer to the text name of the author of the program. This
        string can be anything but is generally the author’s name.
    plugin_date
        is the pointer text string containing the date of the plugin.
        This string can be anything but is generally some human readable
        form of the date.
    plugin_version
        is a pointer to a text string containing the version of the
        plugin. The contents are determined by the plugin writer.
    plugin_description
        is a pointer to a string describing what the plugin does. The
        contents are determined by the plugin writer.

    The pInfo structure must be defined in static memory because Bareos
    does not copy it and may refer to the values at any time while the
    plugin is loaded. All values must be supplied or the plugin will not
    run (not yet implemented). All text strings must be either ASCII or
    UTF-8 strings that are terminated with a zero byte.

pFuncs
    When the loadPlugin entry point is called, the plugin must
    initialize an entry point structure about the plugin and return a
    pointer to this structure to Bareos. This structure contains pointer
    to each of the entry points that the plugin must provide for Bareos.
    When Bareos is actually running the plugin, it will call the defined
    entry points at particular times. All entry points must be defined.

    The pFuncs structure must be defined in static memory because Bareos
    does not copy it and may refer to the values at any time while the
    plugin is loaded.

    The exact definition as of this writing is:

    ::

        typedef struct s_pluginFuncs {
           uint32_t size;
           uint32_t version;
           bRC (*newPlugin)(bpContext *ctx);
           bRC (*freePlugin)(bpContext *ctx);
           bRC (*getPluginValue)(bpContext *ctx, pVariable var, void *value);
           bRC (*setPluginValue)(bpContext *ctx, pVariable var, void *value);
           bRC (*handlePluginEvent)(bpContext *ctx, bEvent *event, void *value);
           bRC (*startBackupFile)(bpContext *ctx, struct save_pkt *sp);
           bRC (*endBackupFile)(bpContext *ctx);
           bRC (*startRestoreFile)(bpContext *ctx, const char *cmd);
           bRC (*endRestoreFile)(bpContext *ctx);
           bRC (*pluginIO)(bpContext *ctx, struct io_pkt *io);
           bRC (*createFile)(bpContext *ctx, struct restore_pkt *rp);
           bRC (*setFileAttributes)(bpContext *ctx, struct restore_pkt *rp);
           bRC (*checkFile)(bpContext *ctx, char *fname);
        } pFuncs;

    The details of the entry points will be presented in separate
    sections below.

    Where:

    size
        is the byte size of the structure.
    version
        is the plugin interface version currently set to 3.

    Sample code for loadPlugin:

    ::

          bfuncs = lbfuncs;                  /* set Bareos funct pointers */
          binfo  = lbinfo;
          *pinfo  = &pluginInfo;             /* return pointer to our info */
          *pfuncs = &pluginFuncs;            /* return pointer to our functions */

           return bRC_OK;

    where pluginInfo and pluginFuncs are statically defined structures.
    See bpipe-fd.c for details.

Plugin Entry Points
-------------------

This section will describe each of the entry points (subroutines) within
the plugin that the plugin must provide for Bareos, when they are called
and their arguments. As noted above, pointers to these subroutines are
passed back to Bareos in the pFuncs structure when Bareos calls the
loadPlugin() externally defined entry point.

newPlugin(bpContext \*ctx)
~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the entry point that Bareos will call when a new “instance” of
the plugin is created. This typically happens at the beginning of a Job.
If 10 Jobs are running simultaneously, there will be at least 10
instances of the plugin.

The bpContext structure will be passed to the plugin, and during this
call, if the plugin needs to have its private working storage that is
associated with the particular instance of the plugin, it should create
it from the heap (malloc the memory) and store a pointer to its private
working storage in the **pContext** variable. Note: since Bareos is a
multi-threaded program, you must not keep any variable data in your
plugin unless it is truly meant to apply globally to the whole plugin.
In addition, you must be aware that except the first and last call to
the plugin (loadPlugin and unloadPlugin) all the other calls will be
made by threads that correspond to a Bareos job. The bpContext that will
be passed for each thread will remain the same throughout the Job thus
you can keep your private Job specific data in it (**bContext**).

::

    typedef struct s_bpContext {
      void *pContext;   /* Plugin private context */
      void *bContext;   /* Bareos private context */
    } bpContext;

This context pointer will be passed as the first argument to all the
entry points that Bareos calls within the plugin. Needless to say, the
plugin should not change the bContext variable, which is Bareos’s
private context pointer for this instance (Job) of this plugin.

freePlugin(bpContext \*ctx)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This entry point is called when the this instance of the plugin is no
longer needed (the Job is ending), and the plugin should release all
memory it may have allocated for this particular instance (Job) i.e. the
pContext. This is not the final termination of the plugin signaled by a
call to **unloadPlugin**. Any other instances (Job) will continue to
run, and the entry point **newPlugin** may be called again if other jobs
start.

getPluginValue(bpContext \*ctx, pVariable var, void \*value)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bareos will call this entry point to get a value from the plugin. This
entry point is currently not called.

setPluginValue(bpContext \*ctx, pVariable var, void \*value)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bareos will call this entry point to set a value in the plugin. This
entry point is currently not called.

handlePluginEvent(bpContext \*ctx, bEvent \*event, void \*value)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This entry point is called when Bareos encounters certain events
(discussed below). This is, in fact, the main way that most plugins get
control when a Job runs and how they know what is happening in the job.
It can be likened to the **RunScript** feature that calls external
programs and scripts, and is very similar to the Bareos Python
interface. When the plugin is called, Bareos passes it the pointer to an
event structure (bEvent), which currently has one item, the eventType:

::

    typedef struct s_bEvent {
       uint32_t eventType;
    } bEvent;

which defines what event has been triggered, and for each event, Bareos
will pass a pointer to a value associated with that event. If no value
is associated with a particular event, Bareos will pass a NULL pointer,
so the plugin must be careful to always check value pointer prior to
dereferencing it.

The current list of events are:

::

    typedef enum {
      bEventJobStart                        = 1,
      bEventJobEnd                          = 2,
      bEventStartBackupJob                  = 3,
      bEventEndBackupJob                    = 4,
      bEventStartRestoreJob                 = 5,
      bEventEndRestoreJob                   = 6,
      bEventStartVerifyJob                  = 7,
      bEventEndVerifyJob                    = 8,
      bEventBackupCommand                   = 9,
      bEventRestoreCommand                  = 10,
      bEventLevel                           = 11,
      bEventSince                           = 12,
      bEventCancelCommand                   = 13,  /* Executed by another thread */

      /* Just before bEventVssPrepareSnapshot */
      bEventVssBackupAddComponents          = 14,

      bEventVssRestoreLoadComponentMetadata = 15,
      bEventVssRestoreSetComponentsSelected = 16,
      bEventRestoreObject                   = 17,
      bEventEndFileSet                      = 18,
      bEventPluginCommand                   = 19,
      bEventVssBeforeCloseRestore           = 21,

      /* Add drives to VSS snapshot
       *  argument: char[27] drivelist
       * You need to add them without duplicates,
       * see fd_common.h add_drive() copy_drives() to get help
       */
      bEventVssPrepareSnapshot              = 22,
      bEventOptionPlugin                    = 23,
      bEventHandleBackupFile                = 24 /* Used with Options Plugin */

    } bEventType;

Most of the above are self-explanatory.

bEventJobStart
    is called whenever a Job starts. The value passed is a pointer to a
    string that contains: “Jobid=nnn Job=job-name”. Where nnn will be
    replaced by the JobId and job-name will be replaced by the Job name.
    The variable is temporary so if you need the values, you must copy
    them.
bEventJobEnd
    is called whenever a Job ends. No value is passed.
bEventStartBackupJob
    is called when a Backup Job begins. No value is passed.
bEventEndBackupJob
    is called when a Backup Job ends. No value is passed.
bEventStartRestoreJob
    is called when a Restore Job starts. No value is passed.
bEventEndRestoreJob
    is called when a Restore Job ends. No value is passed.
bEventStartVerifyJob
    is called when a Verify Job starts. No value is passed.
bEventEndVerifyJob
    is called when a Verify Job ends. No value is passed.
bEventBackupCommand
    is called prior to the bEventStartBackupJob and the plugin is passed
    the command string (everything after the equal sign in “Plugin =” as
    the value.

    Note, if you intend to backup a file, this is an important first
    point to write code that copies the command string passed into your
    pContext area so that you will know that a backup is being performed
    and you will know the full contents of the “Plugin =” command (
    i.e. what to backup and what virtual filename the user wants to call
    it.

bEventRestoreCommand
    is called prior to the bEventStartRestoreJob and the plugin is
    passed the command string (everything after the equal sign in
    “Plugin =” as the value.

    See the notes above concerning backup and the command string. This
    is the point at which Bareos passes you the original command string
    that was specified during the backup, so you will want to save it in
    your pContext area for later use when Bareos calls the plugin again.

bEventLevel
    is called when the level is set for a new Job. The value is a 32 bit
    integer stored in the void*, which represents the Job Level code.
bEventSince
    is called when the since time is set for a new Job. The value is a
    time_t time at which the last job was run.
bEventCancelCommand
    is called whenever the currently running Job is cancelled. Be warned
    that this event is sent by a different thread.
bEventVssBackupAddComponents
    bEventPluginCommand
    is called for each PluginCommand present in the current FileSet. The
    event will be sent only on plugin specifed in the command. The
    argument is the PluginCommand (not valid after the call).
bEventHandleBackupFile
    is called for each file of a FileSet when using a Options Plugin. If
    the plugin returns CF_OK, it will be used for the backup, if it
    returns CF_SKIP, the file will be skipped. Anything else will backup
    the file with Bareos core functions.

During each of the above calls, the plugin receives either no specific
value or only one value, which in some cases may not be sufficient.
However, knowing the context of the event, the plugin can call back to
the Bareos entry points it was passed during the **loadPlugin** call and
get to a number of Bareos variables. (at the current time few Bareos
variables are implemented, but it easily extended at a future time and
as needs require).

startBackupFile(bpContext \*ctx, struct save_pkt \*sp)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This entry point is called only if your plugin is a command plugin, and
it is called when Bareos encounters the “Plugin =” directive in the
Include section of the FileSet. Called when beginning the backup of a
file. Here Bareos provides you with a pointer to the **save_pkt**
structure and you must fill in this packet with the “attribute” data of
the file.

::

    struct save_pkt {
       int32_t pkt_size;                  /* size of this packet */
       char *fname;                       /* Full path and filename */
       char *link;                        /* Link name if any */
       struct stat statp;                 /* System stat() packet for file */
       int32_t type;                      /* FT_xx for this file */
       uint32_t flags;                    /* Bareos internal flags */
       bool portable;                     /* set if data format is portable */
       char *cmd;                         /* command */
       uint32_t delta_seq;                /* Delta sequence number */
       char *object_name;                 /* Object name to create */
       char *object;                      /* restore object data to save */
       int32_t object_len;                /* restore object length */
       int32_t index;                     /* restore object index */
       int32_t pkt_end;                   /* end packet sentinel */
    };

The second argument is a pointer to the **save_pkt** structure for the
file to be backed up. The plugin is responsible for filling in all the
fields of the **save_pkt**. If you are backing up a real file, then
generally, the statp structure can be filled in by doing a **stat**
system call on the file.

If you are backing up a database or something that is more complex, you
might want to create a virtual file. That is a file that does not
actually exist on the filesystem, but represents say an object that you
are backing up. In that case, you need to ensure that the **fname**
string that you pass back is unique so that it does not conflict with a
real file on the system, and you need to artifically create values in
the statp packet.

Example programs such as **bpipe-fd.c** show how to set these fields.
You must take care not to store pointers the stack in the pointer fields
such as fname and link, because when you return from your function, your
stack entries will be destroyed. The solution in that case is to
malloc() and return the pointer to it. In order to not have memory
leaks, you should store a pointer to all memory allocated in your
pContext structure so that in subsequent calls or at termination, you
can release it back to the system.

Once the backup has begun, Bareos will call your plugin at the
**pluginIO** entry point to “read” the data to be backed up. Please see
the **bpipe-fd.c** plugin for how to do I/O.

Example of filling in the save_pkt as used in bpipe-fd.c:

::

       struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
       time_t now = time(NULL);
       sp->fname = p_ctx->fname;
       sp->statp.st_mode = 0700 | S_IFREG;
       sp->statp.st_ctime = now;
       sp->statp.st_mtime = now;
       sp->statp.st_atime = now;
       sp->statp.st_size = -1;
       sp->statp.st_blksize = 4096;
       sp->statp.st_blocks = 1;
       p_ctx->backup = true;
       return bRC_OK;

Note: the filename to be created has already been created from the
command string previously sent to the plugin and is in the plugin
context (p_ctx->fname) and is a malloc()ed string. This example creates
a regular file (S_IFREG), with various fields being created.

In general, the sequence of commands issued from Bareos to the plugin to
do a backup while processing the “Plugin =” directive are:

1. generate a bEventBackupCommand event to the specified plugin and pass
   it the command string.

2. make a startPluginBackup call to the plugin, which fills in the data
   needed in save_pkt to save as the file attributes and to put on the
   Volume and in the catalog.

3. call Bareos’s internal save_file() subroutine to save the specified
   file. The plugin will then be called at pluginIO() to “open” the
   file, and then to read the file data. Note, if you are dealing with a
   virtual file, the “open” operation is something the plugin does
   internally and it doesn’t necessarily mean opening a file on the
   filesystem. For example in the case of the bpipe-fd.c program, it
   initiates a pipe to the requested program. Finally when the plugin
   signals to Bareos that all the data was read, Bareos will call the
   plugin with the “close” pluginIO() function.

endBackupFile(bpContext \*ctx)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Called at the end of backing up a file for a command plugin. If the
plugin’s work is done, it should return bRC_OK. If the plugin wishes to
create another file and back it up, then it must return bRC_More (not
yet implemented). This is probably a good time to release any malloc()ed
memory you used to pass back filenames.

startRestoreFile(bpContext \*ctx, const char \*cmd)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Called when the first record is read from the Volume that was previously
written by the command plugin.

createFile(bpContext \*ctx, struct restore_pkt \*rp)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Called for a command plugin to create a file during a Restore job before
restoring the data. This entry point is called before any I/O is done on
the file. After this call, Bareos will call pluginIO() to open the file
for write.

The data in the restore_pkt is passed to the plugin and is based on the
data that was originally given by the plugin during the backup and the
current user restore settings (e.g. where, RegexWhere, replace). This
allows the plugin to first create a file (if necessary) so that the data
can be transmitted to it. The next call to the plugin will be a pluginIO
command with a request to open the file write-only.

This call must return one of the following values:

::

     enum {
       CF_SKIP = 1,       /* skip file (not newer or something) */
       CF_ERROR,          /* error creating file */
       CF_EXTRACT,        /* file created, data to extract */
       CF_CREATED,        /* file created, no data to extract */
       CF_CORE            /* let bareos core handles the file creation */
    };

in the restore_pkt value **create_status**. For a normal file, unless
there is an error, you must return **CF_EXTRACT**.

::

    struct restore_pkt {
       int32_t pkt_size;                  /* size of this packet */
       int32_t stream;                    /* attribute stream id */
       int32_t data_stream;               /* id of data stream to follow */
       int32_t type;                      /* file type FT */
       int32_t file_index;                /* file index */
       int32_t LinkFI;                    /* file index to data if hard link */
       uid_t uid;                         /* userid */
       struct stat statp;                 /* decoded stat packet */
       const char *attrEx;                /* extended attributes if any */
       const char *ofname;                /* output filename */
       const char *olname;                /* output link name */
       const char *where;                 /* where */
       const char *RegexWhere;            /* regex where */
       int replace;                       /* replace flag */
       int create_status;                 /* status from createFile() */
       int32_t pkt_end;                   /* end packet sentinel */

    };

Typical code to create a regular file would be the following:

::

       struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
       time_t now = time(NULL);
       sp->fname = p_ctx->fname;   /* set the full path/filename I want to create */
       sp->type = FT_REG;
       sp->statp.st_mode = 0700 | S_IFREG;
       sp->statp.st_ctime = now;
       sp->statp.st_mtime = now;
       sp->statp.st_atime = now;
       sp->statp.st_size = -1;
       sp->statp.st_blksize = 4096;
       sp->statp.st_blocks = 1;
       return bRC_OK;

This will create a virtual file. If you are creating a file that
actually exists, you will most likely want to fill the statp packet
using the stat() system call.

Creating a directory is similar, but requires a few extra steps:

::

       struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
       time_t now = time(NULL);
       sp->fname = p_ctx->fname;   /* set the full path I want to create */
       sp->link = xxx; where xxx is p_ctx->fname with a trailing forward slash
       sp->type = FT_DIREND
       sp->statp.st_mode = 0700 | S_IFDIR;
       sp->statp.st_ctime = now;
       sp->statp.st_mtime = now;
       sp->statp.st_atime = now;
       sp->statp.st_size = -1;
       sp->statp.st_blksize = 4096;
       sp->statp.st_blocks = 1;
       return bRC_OK;

The link field must be set with the full cononical path name, which
always ends with a forward slash. If you do not terminate it with a
forward slash, you will surely have problems later.

As with the example that creates a file, if you are backing up a real
directory, you will want to do an stat() on the directory.

Note, if you want the directory permissions and times to be correctly
restored, you must create the directory **after** all the file
directories have been sent to Bareos. That allows the restore process to
restore all the files in a directory using default directory options,
then at the end, restore the directory permissions. If you do it the
other way around, each time you restore a file, the OS will modify the
time values for the directory entry.

setFileAttributes(bpContext \*ctx, struct restore_pkt \*rp)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is call not yet implemented. Called for a command plugin.

See the definition of **restore_pkt** in the above section.

endRestoreFile(bpContext \*ctx)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Called when a command plugin is done restoring a file.

pluginIO(bpContext \*ctx, struct io_pkt \*io)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Called to do the input (backup) or output (restore) of data from or to a
file for a command plugin. These routines simulate the Unix read(),
write(), open(), close(), and lseek() I/O calls, and the arguments are
passed in the packet and the return values are also placed in the
packet. In addition for Win32 systems the plugin must return two
additional values (described below).

::

     enum {
       IO_OPEN = 1,
       IO_READ = 2,
       IO_WRITE = 3,
       IO_CLOSE = 4,
       IO_SEEK = 5
    };

    struct io_pkt {
       int32_t pkt_size;                  /* Size of this packet */
       int32_t func;                      /* Function code */
       int32_t count;                     /* read/write count */
       mode_t mode;                       /* permissions for created files */
       int32_t flags;                     /* Open flags */
       char *buf;                         /* read/write buffer */
       const char *fname;                 /* open filename */
       int32_t status;                    /* return status */
       int32_t io_errno;                  /* errno code */
       int32_t lerror;                    /* Win32 error code */
       int32_t whence;                    /* lseek argument */
       boffset_t offset;                  /* lseek argument */
       bool win32;                        /* Win32 GetLastError returned */
       int32_t pkt_end;                   /* end packet sentinel */
    };

The particular Unix function being simulated is indicated by the
**func**, which will have one of the IO_OPEN, IO_READ, … codes listed
above. The status code that would be returned from a Unix call is
returned in **status** for IO_OPEN, IO_CLOSE, IO_READ, and IO_WRITE. The
return value for IO_SEEK is returned in **offset** which in general is a
64 bit value.

When there is an error on Unix systems, you must always set io_error,
and on a Win32 system, you must always set win32, and the returned value
from the OS call GetLastError() in lerror.

For all except IO_SEEK, **status** is the return result. In general it
is a positive integer unless there is an error in which case it is -1.

The following describes each call and what you get and what you should
return:

IO_OPEN
    You will be passed fname, mode, and flags. You must set on return:
    status, and if there is a Unix error io_errno must be set to the
    errno value, and if there is a Win32 error win32 and lerror.
IO_READ
    You will be passed: count, and buf (buffer of size count). You must
    set on return: status to the number of bytes read into the buffer
    (buf) or -1 on an error, and if there is a Unix error io_errno must
    be set to the errno value, and if there is a Win32 error, win32 and
    lerror must be set.
IO_WRITE
    You will be passed: count, and buf (buffer of size count). You must
    set on return: status to the number of bytes written from the buffer
    (buf) or -1 on an error, and if there is a Unix error io_errno must
    be set to the errno value, and if there is a Win32 error, win32 and
    lerror must be set.
IO_CLOSE
    Nothing will be passed to you. On return you must set status to 0 on
    success and -1 on failure. If there is a Unix error io_errno must be
    set to the errno value, and if there is a Win32 error, win32 and
    lerror must be set.
IO_LSEEK
    You will be passed: offset, and whence. offset is a 64 bit value and
    is the position to seek to relative to whence. whence is one of the
    following SEEK_SET, SEEK_CUR, or SEEK_END indicating to either to
    seek to an absolute possition, relative to the current position or
    relative to the end of the file. You must pass back in offset the
    absolute location to which you seeked. If there is an error, offset
    should be set to -1. If there is a Unix error io_errno must be set
    to the errno value, and if there is a Win32 error, win32 and lerror
    must be set.

    Note: Bareos will call IO_SEEK only when writing a sparse file.

bool checkFile(bpContext \*ctx, char \*fname)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If this entry point is set, Bareos will call it after backing up all
file data during an Accurate backup. It will be passed the full filename
for each file that Bareos is proposing to mark as deleted. Only files
previously backed up but not backed up in the current session will be
marked to be deleted. If you return **false**, the file will be be
marked deleted. If you return **true** the file will not be marked
deleted. This permits a plugin to ensure that previously saved virtual
files or files controlled by your plugin that have not change (not
backed up in the current job) are not marked to be deleted. This entry
point will only be called during Accurate Incrmental and Differential
backup jobs.

Bareos Plugin Entrypoints
-------------------------

When Bareos calls one of your plugin entrypoints, you can call back to
the entrypoints in Bareos that were supplied during the xxx plugin call
to get or set information within Bareos.

bRC registerBareosEvents(bpContext \*ctx, …)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This Bareos entrypoint will allow you to register to receive events that
are not autmatically passed to your plugin by default. This entrypoint
currently is unimplemented.

bRC getBareosValue(bpContext \*ctx, bVariable var, void \*value)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Calling this entrypoint, you can obtain specific values that are
available in Bareos. The following Variables can be referenced:

-  bVarJobId returns an int

-  bVarFDName returns a char \*

-  bVarLevel returns an int

-  bVarClient returns a char \*

-  bVarJobName returns a char \*

-  bVarJobStatus returns an int

-  bVarSinceTime returns an int (time_t)

-  bVarAccurate returns an int

bRC setBareosValue(bpContext \*ctx, bVariable var, void \*value)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Calling this entrypoint allows you to set particular values in Bareos.
The only variable that can currently be set is **bVarFileSeen** and the
value passed is a char \* that points to the full filename for a file
that you are indicating has been seen and hence is not deleted.

bRC JobMessage(bpContext \*ctx, const char \*file, int line, int type, utime_t mtime, const char \*fmt, …)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This call permits you to put a message in the Job Report.

bRC DebugMessage(bpContext \*ctx, const char \*file, int line, int level, const char \*fmt, …)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This call permits you to print a debug message.

void bareosMalloc(bpContext \*ctx, const char \*file, int line, size_t size)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This call permits you to obtain memory from Bareos’s memory allocator.

void bareosFree(bpContext \*ctx, const char \*file, int line, void \*mem)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This call permits you to free memory obtained from Bareos’s memory
allocator.
