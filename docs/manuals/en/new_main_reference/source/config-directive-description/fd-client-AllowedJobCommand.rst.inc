This directive filters what type of jobs the filedaemon should allow. Until now we had the -b (backup only) and -r (restore only) flags which could be specified at the startup of the filedaemon.

Allowed Job Command can be defined globally for all directors by adding it to the global filedaemon resource or for a specific director when added to the director resource.

You specify all commands you want to be executed by the filedaemon. When you don’t specify the option it will be empty which means all commands are allowed.

The following example shows how to use this functionality: 

.. literalinclude:: /_static/config/FdClientAllowedJobCommand1.conf



All commands that are allowed are specified each on a new line with the Allowed Job Command keyword.

The following job commands are recognized:

backup
   allow backups to be made

restore
   allow restores to be done

verify
   allow verify jobs to be done

estimate
   allow estimate cmds to be executed

runscript
   allow runscripts to run

Only the important commands the filedaemon can perform are filtered, as some commands are part of the above protocols and by disallowing the action the other commands are not invoked at all.

If runscripts are not needed it would be recommended as a security measure to disable running those or only allow the commands that you really want to be used.

Runscripts are particularly a problem as they allow the |bareosFd| to run arbitrary commands. You may also look into the :config:option:`fd/client/AllowedScriptDir`\  keyword to limit the impact of the runscript command.

