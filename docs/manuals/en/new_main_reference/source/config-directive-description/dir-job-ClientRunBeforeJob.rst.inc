This is basically a shortcut for the :config:option:`dir/job/RunScript`\  resource, that run on the client before the backup job.



   .. warning::

      For compatibility reasons, with this shortcut, the command is executed
      directly when the client receive it. And if the command is in error, other
      remote runscripts will be discarded. To be sure that all commands will be
      sent and executed, you have to use :config:option:`dir/job/RunScript`\  syntax.
