The RunScript directive behaves like a resource in that it requires opening and closing braces around a number of directives that make up the body of the runscript.

The specified Command (see below for details) is run as an external program prior or after the current Job. This is optional. By default, the program is executed on the Client side like in ``ClientRunXXXJob``.

**Console** options are special commands that are sent to the director instead of the OS. At this time, console command outputs are redirected to log with the jobid 0.

You can use following console command: ``delete``, ``disable``, ``enable``, ``estimate``, ``list``, ``llist``, ``memory``, ``prune``, ``purge``, ``reload``, ``status``, ``setdebug``, ``show``, ``time``, ``trace``, ``update``, ``version``, ``.client``, ``.jobs``, ``.pool``, ``.storage``. See :ref:`section-bconsole` for more information. You need to specify needed information on command line, nothing will be prompted. Example:



   .. literalinclude:: /include/config/DirJobRunScript1.conf
      :language: bareosconfig



You can specify more than one Command/Console option per RunScript.

| You can use following options may be specified in the body of the runscript:

# Tabular in LaTex format (original)

::

   \begin{tabular}{|p{0.2\textwidth}|p{0.2\textwidth}|p{0.4\textwidth}|}
      \hline
      Options         & Value  & Information   \\
      \hline
      \hline
      Runs On Success & :strong:`Yes` | No & run if JobStatus is successful\\
      \hline
      Runs On Failure & Yes | :strong:`No` & run if JobStatus isn't successful\\
      \hline
      Runs On Client  & :strong:`Yes` | No & run command on client\\
      \hline
      Runs When       & :strong:`\variable{Never}` | \variable{Before} | \variable{After} | \variable{Always} | \textsl{\variable{AfterVSS}} & When to run\\
      \hline
      Fail Job On Error & :strong:`Yes` | No & Fail job if script returns something different from 0\\
      \hline
      Command          &       &   External command\\
      \hline
      Console          &       &   Console command\\
      \hline
      \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================= ============================================================================================================================================================================ =====================================================
Options           Value                                                                                                                                                                        Information
================= ============================================================================================================================================================================ =====================================================
Runs On Success   **Yes** \| No                                                                                                                                                                run if JobStatus is successful
Runs On Failure   Yes \| **No**                                                                                                                                                                run if JobStatus isn’t successful
Runs On Client    **Yes** \| No                                                                                                                                                                run command on client
Runs When         **\variable{Never}** \| \variable{Before} \| \variable{After} \| \variable{Always} \| *\variable{AfterVSS}* When to run
Fail Job On Error **Yes** \| No                                                                                                                                                                Fail job if script returns something different from 0
Command                                                                                                                                                                                        External command
Console                                                                                                                                                                                        Console command
================= ============================================================================================================================================================================ =====================================================

Any output sent by the command to standard output will be included in the Bareos job report. The command string must be a valid program name or name of a shell script.



   .. warning::
      The command string is parsed then fed to the OS,
      which means that the path will be searched to execute your specified
      command, but there is no shell interpretation. As a consequence, if you
      invoke complicated commands or want any shell features such as redirection
      or piping, you must call a shell script and do it inside that script.
      Alternatively, it is possible to use :command:`sh -c '...'` in the command
      definition to force shell interpretation, see example below.

Before executing the specified command, Bareos performs character substitution of the following characters:



.. _character substitution:

  # Tabular in LaTex format (original)

::

   \begin{longtable}{ l l }
          \%\% & \% \\
          \%b & Job Bytes \\
          \%B & Job Bytes in human readable format \\
          \%c & Client's name \\
          \%d & Daemon's name (Such as host-dir or host-fd) \\
          \%D & Director's name (Also valid on file daemon) \\
          \%e & Job Exit Status \\
          \%f & Job FileSet (Only on director side) \\
          \%F & Job Files \\
          \%h & Client address \\
          \%i & Job Id \\
          \%j & Unique Job Id \\
          \%l & Job Level \\
          \%n & Job name \\
          \%p & Pool name (Only on director side) \\
          \%P & Daemon PID \\
          \%s & Since time \\
          \%t & Job type (Backup, ...) \\
          \%v & Read Volume name(s) (Only on director side) \\
          \%V & Write Volume name(s) (Only on director side) \\
          \%w & Storage name (Only on director side) \\
          \%x & Spooling enabled? ("yes" or "no") \\
      \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

== ============================================
%% %
%b Job Bytes
%B Job Bytes in human readable format
%c Client’s name
%d Daemon’s name (Such as host-dir or host-fd)
%D Director’s name (Also valid on file daemon)
%e Job Exit Status
%f Job FileSet (Only on director side)
%F Job Files
%h Client address
%i Job Id
%j Unique Job Id
%l Job Level
%n Job name
%p Pool name (Only on director side)
%P Daemon PID
%s Since time
%t Job type (Backup, ...)
%v Read Volume name(s) (Only on director side)
%V Write Volume name(s) (Only on director side)
%w Storage name (Only on director side)
%x Spooling enabled? ("yes" or "no")
== ============================================



Some character substitutions are not available in all situations. The Job Exit Status code %e edits the following values:

:index:`[TAG=Exit Status] <single: Exit Status>`

-  OK

-  Error

-  Fatal Error

-  Canceled

-  Differences

-  Unknown term code

Thus if you edit it on a command line, you will need to enclose it within some sort of quotes.

| You can use these following shortcuts:
| # Tabular in LaTex format (original)

::

   \begin{tabular}{|l|c|c|c|c|c}
      \hline
      Keyword & RunsOnSuccess & RunsOnFailure  & FailJobOnError & Runs On Client & RunsWhen  \\
      \hline
      \hline
      :config:option:`dir/job/RunBeforeJob`\          &        &       & Yes     & No     & Before \\
      \hline
      :config:option:`dir/job/RunAfterJob`\           &  Yes   &   No  &         & No     & After  \\
      \hline
      :config:option:`dir/job/RunAfterFailedJob`\    &  No    &  Yes  &         & No     & After  \\
      \hline
      :config:option:`dir/job/ClientRunBeforeJob`\   &        &       & Yes     & Yes    & Before \\
      \hline
      :config:option:`dir/job/ClientRunAfterJob`\    &  Yes   &   No  &         & Yes    & After  \\
      \hline
      \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

==================================================================== ============= ============= ============== ============== ========
Keyword                                                              RunsOnSuccess RunsOnFailure FailJobOnError Runs On Client RunsWhen
==================================================================== ============= ============= ============== ============== ========
:config:option:`dir/job/RunBeforeJob`\                                     Yes            No             Before
:config:option:`dir/job/RunAfterJob`\          Yes           No                           No             After
:config:option:`dir/job/RunAfterFailedJob`\   No            Yes                          No             After
:config:option:`dir/job/ClientRunBeforeJob`\                              Yes            Yes            Before
:config:option:`dir/job/ClientRunAfterJob`\   Yes           No                           Yes            After
==================================================================== ============= ============= ============== ============== ========

Examples: 

.. literalinclude:: /include/config/DirJobRunScript2.conf
   :language: bareosconfig



Special Windows Considerations :index:`[TAG=Windows->Run Script] <pair: Windows; Run Script>`

You can run scripts just after snapshots initializations with *AfterVSS* keyword.

In addition, for a Windows client, please take note that you must ensure a correct path to your script. The script or program can be a .com, .exe or a .bat file. If you just put the program name in then Bareos will search using the same rules that cmd.exe uses (current directory, Bareos bin directory, and PATH). It will even try the different extensions in the same order as cmd.exe. The command can be anything that cmd.exe or command.com will recognize as an executable file.

However, if you have slashes in the program name then Bareos figures you are fully specifying the name, so you must also explicitly add the three character extension.

The command is run in a Win32 environment, so Unix like commands will not work unless you have installed and properly configured Cygwin in addition to and separately from Bareos.

The System %Path% will be searched for the command. (under the environment variable dialog you have have both System Environment and User Environment, we believe that only the System environment will be available to bareos-fd, if it is running as a service.)

System environment variables can be referenced with %var% and used as either part of the command name or arguments.

| So if you have a script in the Bareos
| bin directory then the following lines should work fine:



   .. literalinclude:: /include/config/DirJobRunScript3.conf
      :language: bareosconfig



The outer set of quotes is removed when the configuration file is parsed. You need to escape the inner quotes so that they are there when the code that parses the command line for execution runs so it can tell what the program name is.

The special characters \configCharsToQuote will need to be quoted, if they are part of a filename or argument.

If someone is logged in, a blank "command" window running the commands will be present during the execution of the command.

Some Suggestions from Phil Stracchino for running on Win32 machines with the native Win32 File daemon:

#. You might want the ClientRunBeforeJob directive to specify a .bat file which runs the actual client-side commands, rather than trying to run (for example) regedit /e directly.

#. The batch file should explicitly ’exit 0’ on successful completion.

#. The path to the batch file should be specified in Unix form:

   ``Client Run Before Job = "c:/bareos/bin/systemstate.bat"``

   rather than DOS/Windows form:

   INCORRECT: ``Client Run Before Job = "c:\bareos\bin\systemstate.bat"``

For Win32, please note that there are certain limitations:

``Client Run Before Job = "C:/Program Files/Bareos/bin/pre-exec.bat"``

Lines like the above do not work because there are limitations of cmd.exe that is used to execute the command. Bareos prefixes the string you supply with :command:`cmd.exe /c`. To test that your command works you should type :command:`cmd /c "C:/Program Files/test.exe"` at a cmd prompt and see what happens. Once the command is correct insert a backslash (\) before each double quote ("), and then put quotes around the whole thing when putting it in the director’s .conf
file. You either need to have only one set of quotes or else use the short name and don’t put quotes around the command path.

Below is the output from cmd’s help as it relates to the command line passed to the /c option.

If /C or /K is specified, then the remainder of the command line after the switch is processed as a command line, where the following logic is used to process quote (") characters:

#. If all of the following conditions are met, then quote characters on the command line are preserved:

   -  no /S switch.

   -  exactly two quote characters.

   -  no special characters between the two quote characters, where special is one of: \configCharsToQuote

   -  there are one or more whitespace characters between the the two quote characters.

   -  the string between the two quote characters is the name of an executable file.

#. Otherwise, old behavior is to see if the first character is a quote character and if so, strip the leading character and remove the last quote character on the command line, preserving any text after the last quote character.

