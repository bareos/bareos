This directive limits the impact of the runscript command of the filedaemon.

It can be specified either for all directors by adding it to the global filedaemon resource or for a specific director when added to the director resource.

All directories in which the scripts or commands are located that you allow to be run by the runscript command of the filedaemon. Any program not in one of these paths (or subpaths) cannot be used. The implementation checks if the full path of the script starts with one of the specified paths.

The following example shows how to use this functionality:



   .. literalinclude:: /include/config/FdClientAllowedScriptDir1.conf
      :language: bareosconfig


