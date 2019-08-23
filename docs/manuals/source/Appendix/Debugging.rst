Debugging
=========

:index:`\ <single: Crash>`\  :index:`\ <single: Debug; crash>`\ 

This chapter describes, how to debug Bareos, when the program crashes. If you are just interested about how to get more information about a running Bareos daemon, please read :ref:`section-debug-messages`.

If you are running on a Linux system, and you have a set of working configuration files, it is very unlikely that Bareos will crash. As with all software, however, it is inevitable that someday, it may crash.

This chapter explains what you should do if one of the three Bareos daemons (Director, File, Storage) crashes. When we speak of crashing, we mean that the daemon terminates abnormally because of an error. There are many cases where Bareos detects errors (such as PIPE errors) and will fail a job. These are not considered crashes. In addition, under certain conditions, Bareos will detect a fatal in the configuration, such as lack of permission to read/write the working directory. In that case,
Bareos will force itself to crash with a SEGFAULT. However, before crashing, Bareos will normally display a message indicating why. For more details, please read on.

Traceback
---------

:index:`\ <single: Traceback>`\ 

Each of the three Bareos daemons has a built-in exception handler which, in case of an error, will attempt to produce a traceback. If successful the traceback will be emailed to you.

For this to work, you need to ensure that a few things are setup correctly on your system:

#. You must have a version of Bareos with debug information and not stripped of debugging symbols. When using a packaged version of Bareos, this requires to install the Bareos debug packages (**bareos-debug** on RPM based systems, **bareos-dbg** on Debian based systems).

#. On Linux, :command:`gdb` (the GNU debugger) must be installed. On some systems such as Solaris, :command:`gdb` may be replaced by :command:`dbx`.

#. By default, btraceback uses :command:`bsmtp` to send the traceback via email. Therefore it expects a local mail transfer daemon running. It send the traceback to root@localhost via :strong:`localhost`.

#. Some Linux distributions, e.g. Ubuntu:index:`\ <single: Platform; Ubuntu; Debug>`\ , disable the possibility to examine the memory of other processes. While this is a good idea for hardening a system, our debug mechanismen will fail. To disable this feature, run (as root):

   .. code-block:: shell-session
      :caption: disable ptrace protection to enable debugging (required on Ubuntu Linux)

      root@host:~# test -e /proc/sys/kernel/yama/ptrace_scope && echo 0 > /proc/sys/kernel/yama/ptrace_scope

If all the above conditions are met, the daemon that crashes will produce a traceback report and email it. If the above conditions are not true, you can run the debugger by hand as described below.

Testing The Traceback
---------------------

:index:`\ <single: Traceback; Test>`\ 

To "manually" test the traceback feature, you simply start Bareos then obtain the PID of the main daemon thread (there are multiple threads). The output produced here will look different depending on what OS and what version of the kernel you are running.

.. code-block:: shell-session
   :caption: get the process ID of a running Bareos daemon

   root@host:~# ps fax | grep bareos-dir
    2103 ?        S      0:00 /usr/sbin/bareos-dir

which in this case is 2103. Then while Bareos is running, you call the program giving it the path to the Bareos executable and the PID. In this case, it is:

.. code-block:: shell-session
   :caption: get traceback of running Bareos director daemon

   root@host:~# btraceback /usr/sbin/bareos-dir 2103

It should produce an email showing you the current state of the daemon (in this case the Director), and then exit leaving Bareos running as if nothing happened. If this is not the case, you will need to correct the problem by modifying the :command:`btraceback` script.

Getting A Traceback On Other Systems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It should be possible to produce a similar backtrace on systems other than Linux, either using :command:`gdb` or some other debugger.
:index:`Solaris <single: Platform; Solaris; Debug>`\  with :command:`dbx` loaded works quite fine. On other systems, you will need to modify the :command:`btraceback` program to invoke the correct debugger, and possibly correct the :file:`btraceback.gdb` script to have appropriate commands for your debugger.
Please keep in mind that for any debugger to work, it will most likely need to run as root.

Manually Running Bareos Under The Debugger
------------------------------------------

If for some reason you cannot get the automatic traceback, or if you want to interactively examine the variable contents after a crash, you can run Bareos under the debugger. Assuming you want to run the Storage daemon under the debugger (the technique is the same for the other daemons, only the name changes), you would do the following:

#. The Director and the File daemon should be running but the Storage daemon should not.

#. Start the Storage daemon under the debugger:

   .. code-block:: shell-session
      :caption: run the Bareos Storage daemon in the debugger

      root@host:~# su - bareos -s /bin/bash
      bareos@host:~# gdb --args /usr/sbin/bareos-sd -f -s -d 200
      (gdb) run

   Bareos Parameter:

   -f
      foreground

   -s
      no signals

   -d nnn
      debug level

   See section :ref:`daemon command line options <section-daemon-command-line-options>` for a detailed list of options.

#. At this point, Bareos will be fully operational.

#. In another shell command window, start the Console program and do what is necessary to cause Bareos to die.

#. When Bareos crashes, the gdb shell window will become active and gdb will show you the error that occurred.

#. To get a general traceback of all threads, issue the following command:

   .. code-block:: shell-session
      :caption: Bareos Storage daemon in a debugger session

      (gdb) thread apply all bt

   After that you can issue any debugging command.
